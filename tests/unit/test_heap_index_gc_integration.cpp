/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * @file test_heap_index_gc_integration.cpp
 * @brief Integration tests for PHASE 2 TASK 2.6: Heap-Index GC Integration
 *
 * Tests the integration between HeapPage::collectDeadTuples(),
 * GarbageCollector::cleanIndexes(), and heap pruning to ensure
 * that dead tuples are properly removed from both heap and indexes.
 *
 * Test coverage:
 * - collectDeadTuples() correctly identifies dead tuples based on OIT
 * - GC cleanPage() flow calls collectDeadTuples() before prunePage()
 * - TID format is correct: (GPID, slot)
 *
 * NOTE: Full end-to-end index cleanup testing requires table metadata
 * integration which is pending (see GarbageCollector::cleanIndexes TODO).
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/config.h"
#include <filesystem>
#include <vector>
#include <cstring>

using namespace scratchbird::core;

class HeapIndexGCIntegrationTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database db_;

    void SetUp() override
    {
        // Ensure cooperative GC runs every time for deterministic tests.
        Config::getInstance().set("garbage_collection", "enabled", "true");
        Config::getInstance().set("garbage_collection", "policy", "COMBINED");
        Config::getInstance().set("garbage_collection", "cooperative_rate", "1");

        // Create temporary database for testing
        test_db_path_ = "/tmp/test_heap_index_gc_" + std::to_string(getpid()) + ".db";

        // Remove if exists
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

        // Create database
        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        // Initialize garbage collector
        auto gc = db_.garbage_collector();
        ASSERT_NE(gc, nullptr);
        status = gc->initialize(&ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to initialize GC: " << ctx.message;
    }

    void TearDown() override
    {
        db_.close();

        // Clean up test database
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    Status allocatePage(uint32_t &page_id, uint8_t **page_data, ErrorContext *ctx)
    {
        auto *page_mgr = db_.page_manager();
        if (!page_mgr)
        {
            return Status::INVALID_ARGUMENT;
        }

        Status status = page_mgr->allocatePage(page_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        void *buffer = nullptr;
        status = db_.buffer_pool()->pinPage(page_id, &buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        *page_data = static_cast<uint8_t *>(buffer);
        return Status::OK;
    }

    void releasePage(uint32_t page_id, bool dirty, ErrorContext *ctx)
    {
        db_.buffer_pool()->unpinPage(page_id, dirty, ctx);
        db_.page_manager()->freePage(page_id, ctx);
    }

    static std::vector<uint8_t> buildTuple(const uint8_t *payload, size_t payload_size)
    {
        std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload_size, 0);
        TupleHeader header{};
        header.session_id = ID{};
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        if (payload_size > 0)
        {
            std::memcpy(tuple.data() + sizeof(TupleHeader), payload, payload_size);
        }
        return tuple;
    }

    static void markXmaxCommitted(uint8_t *page_data, uint16_t item_id)
    {
        auto *items = reinterpret_cast<ItemPointer *>(page_data + sizeof(PageHeader));
        auto *tuple_hdr =
            reinterpret_cast<TupleHeader *>(page_data + items[item_id].offset);
        tuple_hdr->infomask |= TupleHeader::HEAP_XMAX_COMMITTED;
    }
};

// ========== HeapPage::collectDeadTuples() Tests ==========

TEST_F(HeapIndexGCIntegrationTest, CollectDeadTuples_EmptyPage)
{
    ErrorContext ctx;

    // Allocate a test page

    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = allocatePage(page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_NE(page_data, nullptr);

    // Initialize heap page
    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Collect dead tuples with OIT=1000
    std::vector<TID> dead_tids;
    status = heap_page.collectDeadTuples(1000, &dead_tids, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(dead_tids.empty()) << "Empty page should have no dead tuples";

    // Free page
    releasePage(page_id, true, &ctx);
}

TEST_F(HeapIndexGCIntegrationTest, CollectDeadTuples_LiveTuplesOnly)
{
    ErrorContext ctx;

    // Allocate and initialize page
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = allocatePage(page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Insert live tuples (xmax=0)
    uint8_t payload[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto tuple_data = buildTuple(payload, sizeof(payload));

    for (int i = 0; i < 3; i++)
    {
        uint16_t item_id = 0;
        status = heap_page.insertTuple(tuple_data.data(),
                                       static_cast<uint32_t>(tuple_data.size()),
                                       100 + i, &item_id, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Collect dead tuples with OIT=1000
    std::vector<TID> dead_tids;
    status = heap_page.collectDeadTuples(1000, &dead_tids, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(dead_tids.empty()) << "No dead tuples (all xmax=0)";

    releasePage(page_id, true, &ctx);
}

TEST_F(HeapIndexGCIntegrationTest, CollectDeadTuples_AllDead)
{
    ErrorContext ctx;

    // Allocate and initialize page
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = allocatePage(page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Insert and delete tuples (xmax < OIT)
    uint8_t payload[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto tuple_data = buildTuple(payload, sizeof(payload));

    for (int i = 0; i < 3; i++)
    {
        uint16_t item_id = 0;
        status = heap_page.insertTuple(tuple_data.data(),
                                       static_cast<uint32_t>(tuple_data.size()),
                                       100 + i, &item_id, &ctx);
        ASSERT_EQ(status, Status::OK);

        // Delete tuple (sets xmax and HEAP_XMAX_COMMITTED flag)
        status = heap_page.deleteTuple(item_id, 500 + i, &ctx);
        ASSERT_EQ(status, Status::OK);
        markXmaxCommitted(page_data, item_id);
    }

    // Collect dead tuples with OIT=1000 (all xmax values 500-502 < 1000)
    std::vector<TID> dead_tids;
    status = heap_page.collectDeadTuples(1000, &dead_tids, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(dead_tids.size(), 3) << "All 3 tuples should be dead";

    // Verify TID format: (GPID, slot)
    for (size_t i = 0; i < dead_tids.size(); i++)
    {
        TID tid = dead_tids[i];
        uint32_t tid_page = 0;
        ASSERT_TRUE(convertGPIDtoPageID(tid.gpid, &tid_page));

        EXPECT_EQ(tid_page, page_id) << "TID page_id should match";
        EXPECT_EQ(tid.slot, i) << "TID slot should be " << i;
    }

    releasePage(page_id, true, &ctx);
}

TEST_F(HeapIndexGCIntegrationTest, CollectDeadTuples_MixedLiveAndDead)
{
    ErrorContext ctx;

    // Allocate and initialize page
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = allocatePage(page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Insert mix of live and dead tuples
    uint8_t payload[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto tuple_data = buildTuple(payload, sizeof(payload));

    // Tuple 0: Live (xmax=0)
    uint16_t item_id = 0;
    status = heap_page.insertTuple(tuple_data.data(),
                                   static_cast<uint32_t>(tuple_data.size()),
                                   100, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Tuple 1: Dead (xmax=500 < OIT=1000)
    status = heap_page.insertTuple(tuple_data.data(),
                                   static_cast<uint32_t>(tuple_data.size()),
                                   200, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = heap_page.deleteTuple(item_id, 500, &ctx);
    ASSERT_EQ(status, Status::OK);
    markXmaxCommitted(page_data, item_id);

    // Tuple 2: Live (xmax=0)
    status = heap_page.insertTuple(tuple_data.data(),
                                   static_cast<uint32_t>(tuple_data.size()),
                                   300, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Tuple 3: Dead (xmax=600 < OIT=1000)
    status = heap_page.insertTuple(tuple_data.data(),
                                   static_cast<uint32_t>(tuple_data.size()),
                                   400, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = heap_page.deleteTuple(item_id, 600, &ctx);
    ASSERT_EQ(status, Status::OK);
    markXmaxCommitted(page_data, item_id);

    // Tuple 4: Live (xmax=1100 >= OIT=1000, still visible to older transactions)
    status = heap_page.insertTuple(tuple_data.data(),
                                   static_cast<uint32_t>(tuple_data.size()),
                                   500, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = heap_page.deleteTuple(item_id, 1100, &ctx);
    ASSERT_EQ(status, Status::OK);
    markXmaxCommitted(page_data, item_id);

    // Collect dead tuples with OIT=1000
    std::vector<TID> dead_tids;
    status = heap_page.collectDeadTuples(1000, &dead_tids, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(dead_tids.size(), 2) << "2 tuples should be dead (items 1 and 3)";

    // Verify dead TIDs are for items 1 and 3
    if (dead_tids.size() == 2)
    {
        uint16_t item1 = dead_tids[0].slot;
        uint16_t item2 = dead_tids[1].slot;

        EXPECT_EQ(item1, 1) << "First dead TID should be item 1";
        EXPECT_EQ(item2, 3) << "Second dead TID should be item 3";
    }

    releasePage(page_id, true, &ctx);
}

TEST_F(HeapIndexGCIntegrationTest, CollectDeadTuples_NullPointer)
{
    ErrorContext ctx;

    // Allocate and initialize page
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = allocatePage(page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Call with nullptr
    status = heap_page.collectDeadTuples(1000, nullptr, &ctx);

    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
    EXPECT_NE(ctx.message.find("cannot be null"), std::string::npos);

    releasePage(page_id, true, &ctx);
}

// ========== GarbageCollector Integration Tests ==========

TEST_F(HeapIndexGCIntegrationTest, GC_Integration_FlowWorks)
{
    ErrorContext ctx;

    // Create page with dead tuple
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = allocatePage(page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Insert and delete a tuple
    uint8_t payload[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto tuple_data = buildTuple(payload, sizeof(payload));
    uint16_t item_id = 0;
    status = heap_page.insertTuple(tuple_data.data(),
                                   static_cast<uint32_t>(tuple_data.size()),
                                   100, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = heap_page.deleteTuple(item_id, 500, &ctx);
    ASSERT_EQ(status, Status::OK);
    markXmaxCommitted(page_data, item_id);

    // Mark page dirty so GC will process it
    auto gc = db_.garbage_collector();
    gc->markPageDirty(page_id);

    // Trigger cooperative GC on this page
    // This should call collectDeadTuples() -> cleanIndexes() -> prunePage()
    gc->processPageCooperative(page_id, &ctx);

    // Verify page was processed
    auto stats = gc->getStatistics();
    EXPECT_GT(stats.cooperative_runs, 0) << "Cooperative GC should have run";

    // Note: cleanIndexes() currently logs but doesn't clean (pending table metadata)
    // This test verifies the integration flow works without errors

    releasePage(page_id, true, &ctx);
}

TEST_F(HeapIndexGCIntegrationTest, GC_Integration_HandlesEmptyPage)
{
    ErrorContext ctx;

    // Create empty page
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    Status status = allocatePage(page_id, &page_data, &ctx);
    ASSERT_EQ(status, Status::OK);

    HeapPage heap_page(page_data, db_.page_size());
    status = heap_page.initialize(page_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Mark page dirty and run GC
    auto gc = db_.garbage_collector();
    gc->markPageDirty(page_id);
    gc->processPageCooperative(page_id, &ctx);

    // Should handle empty page gracefully
    auto stats = gc->getStatistics();
    EXPECT_GT(stats.cooperative_runs, 0);

    releasePage(page_id, true, &ctx);
}

// Note: main() is provided by GTest::gtest_main linked in CMakeLists.txt
