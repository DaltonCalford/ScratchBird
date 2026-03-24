/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developers-public-license-version-1-0/
 */
/**
 * @file test_cross_page_updates.cpp
 * @brief Comprehensive tests for cross-page tuple updates (CRIT-002)
 *
 * Tests cover:
 * - Cross-page version chains when tuple grows beyond page capacity
 * - HOT (Heap-Only Tuple) updates vs cross-page updates
 * - Version chain traversal across pages
 * - Lock acquisition on new tuple locations
 * - Garbage collection with cross-page version chains
 * - MVCC visibility with cross-page updates
 */

#include <gtest/gtest.h>
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/page_manager.h"
#include "test_helpers.h"
#include <vector>
#include <cstring>

using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

class CrossPageUpdateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ErrorContext ctx;

        // Create test database with small page size to trigger cross-page updates easily
        ASSERT_EQ(Database::create(db_file_.path(), 8192, &ctx), Status::OK)
            << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_.path(), &ctx), Status::OK)
            << "Failed to open database: " << ctx.message;

        // Database::open() already initializes the catalog, no need to call initialize() again
        // Just get the default schema for table creation
        scratchbird::core::CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(db_->catalog_manager()->getSchema("PUBLIC", schema_info, &ctx), Status::OK)
            << "Failed to get PUBLIC schema: " << ctx.message;
        schema_id_ = schema_info.schema_id;

        // Create a test table with columns
        std::vector<CatalogManager::ColumnInfo> columns;
        CatalogManager::ColumnInfo col;
        col.column_id = generateUuidV7();
        col.column_name = "data";
        col.ordinal = 0;
        col.data_type = static_cast<uint16_t>(DataType::BLOB);
        col.nullable = true;
        columns.push_back(col);

        ASSERT_EQ(db_->catalog_manager()->createTable(schema_id_, "test_table", columns, 
                                                       test_table_id_, 0, &ctx),
                  Status::OK)
            << "Failed to create table: " << ctx.message;

        storage_engine_ = db_->storage_engine();
        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(storage_engine_, nullptr);
        ASSERT_NE(txn_mgr_, nullptr);
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
        }
        db_.reset();
    }

    // Helper to create tuple data with canonical tuple format:
    // [TupleHeader][payload bytes...]
    std::vector<uint8_t> createTupleData(uint32_t payload_size, uint8_t fill_byte = 0xAA)
    {
        std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload_size, 0);
        auto *hdr = reinterpret_cast<TupleHeader *>(tuple.data());
        hdr->xmin = 0;
        hdr->xmax = 0;
        hdr->back_version_gpid = INVALID_GPID;
        hdr->back_version_slot = 0;
        hdr->ctid_gpid = INVALID_GPID;
        hdr->ctid_slot = 0;
        hdr->infomask = 0;
        hdr->null_bitmap_offset = 0;
        hdr->padding = 0;
        hdr->session_id = ID{};
        std::memset(tuple.data() + sizeof(TupleHeader), fill_byte, payload_size);
        return tuple;
    }

    // Helper to fill a page almost completely
    void fillPageAlmostFull(uint32_t page_id, ErrorContext *ctx)
    {
        // Pin the page
        void *page_buffer;
        Status status = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);
        ASSERT_EQ(status, Status::OK);

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        HeapPage heap_page(page_data, db_->page_size());

        // Fill with non-TOAST tuples until we have very little free space.
        // Keep payload below TOAST threshold so updates exercise heap-page space logic.
        const uint32_t target_free_space = 260;
        const uint32_t tuple_payload_size = 64;
        auto tuple_data = createTupleData(tuple_payload_size);

        while (heap_page.getFreeSpace() > target_free_space + tuple_data.size() + sizeof(ItemPointer))
        {
            uint16_t item_id;
            uint64_t xmin = txn_mgr_ ? txn_mgr_->getCurrentXid() : 100;
            status = heap_page.insertTuple(tuple_data.data(),
                                           static_cast<uint32_t>(tuple_data.size()),
                                           xmin, &item_id, ctx);
            if (status != Status::OK)
            {
                break; // Page is full enough
            }
        }

        db_->buffer_pool()->unpinPage(page_id, true, ctx);
    }

    uint32_t allocateOwnedHeapPage(const ID &table_id, ErrorContext *ctx)
    {
        uint32_t page_id = 0;
        void *page_buffer = nullptr;
        EXPECT_EQ(db_->buffer_pool()->allocatePage(&page_id, &page_buffer, ctx), Status::OK)
            << ctx->message;

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        std::memset(page_data, 0, db_->page_size());

        HeapPage heap_page(page_data, db_->page_size(), nullptr, db_.get(), table_id);
        EXPECT_EQ(heap_page.initialize(page_id, ctx), Status::OK) << ctx->message;
        heap_page.applyOwningTableContract(false);

        EXPECT_EQ(db_->buffer_pool()->unpinPage(page_id, true, ctx), Status::OK) << ctx->message;
        return page_id;
    }

    // Choose an update tuple size that forces cross-page back-versioning while
    // still fitting as the primary tuple in overwriteTuple().
    std::vector<uint8_t> createCrossPageUpdateTuple(uint32_t page_id, uint16_t item_id,
                                                    uint8_t fill_byte, ErrorContext *ctx)
    {
        void *page_buffer = nullptr;
        Status status = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return createTupleData(120, fill_byte);
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        HeapPage heap_page(page_data, db_->page_size());
        const uint8_t *old_tuple = nullptr;
        uint32_t old_tuple_size = 0;
        status = heap_page.getTuple(item_id, &old_tuple, &old_tuple_size, ctx);
        uint32_t free_space = heap_page.getFreeSpace();
        db_->buffer_pool()->unpinPage(page_id, false, ctx);
        if (status != Status::OK)
        {
            return createTupleData(120, fill_byte);
        }

        const uint32_t min_total = sizeof(TupleHeader) + 8;
        uint32_t chosen_total = min_total;

        // Prefer a tuple that still fits the overwrite path but is large enough
        // that same-page update cannot keep both the new head and the back version.
        if (free_space > 8)
        {
            uint32_t candidate_total = free_space - 8;
            if (candidate_total > 240)
            {
                candidate_total = 240;
            }
            if (candidate_total > old_tuple_size && candidate_total >= min_total)
            {
                chosen_total = candidate_total;
            }
        }

        // Fallback for tight pages: choose the smallest tuple that pushes the
        // same-page allocator over budget while staying non-TOASTed.
        if (chosen_total == min_total)
        {
            uint32_t max_total = (free_space > 8) ? (free_space - 8) : free_space;
            if (max_total > 240)
            {
                max_total = 240;
            }
            if (max_total < min_total)
            {
                max_total = min_total;
            }

            uint32_t cross_page_min_total = min_total;
            if (free_space > old_tuple_size)
            {
                cross_page_min_total = free_space - old_tuple_size + 1;
            }

            chosen_total = max_total;
            if (cross_page_min_total <= max_total)
            {
                chosen_total = cross_page_min_total;
            }
        }

        uint32_t payload = chosen_total - sizeof(TupleHeader);
        return createTupleData(payload, fill_byte);
    }

    TestDatabaseFile db_file_{"test_cross_page_updates"};
    std::unique_ptr<Database> db_;
    StorageEngine *storage_engine_;
    TransactionManager *txn_mgr_;
    ID schema_id_;
    ID test_table_id_;
};

/**
 * Test 1: Basic cross-page update when tuple grows beyond page capacity
 */
TEST_F(CrossPageUpdateTest, BasicCrossPageUpdate)
{
    ErrorContext ctx;

    // Insert initial small tuple
    auto small_tuple = createTupleData(120, 0xAA);
    uint32_t page_id;
    uint16_t item_id;

    Status status = storage_engine_->insertTuple(test_table_id_, small_tuple.data(),
                                                 small_tuple.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to insert initial tuple: " << ctx.message;

    // Fill the page almost completely so next update won't fit
    fillPageAlmostFull(page_id, &ctx);

    // Now update with a large tuple that won't fit on same page
    auto large_tuple = createCrossPageUpdateTuple(page_id, item_id, 0xBB, &ctx);
    uint32_t new_page_id;
    uint16_t new_item_id;

    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                          large_tuple.data(), large_tuple.size(),
                                          &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << "Cross-page update failed: " << ctx.message;

    // MGA semantics: primary TID remains stable across updates.
    EXPECT_EQ(new_page_id, page_id) << "MGA update should preserve page_id";
    EXPECT_EQ(new_item_id, item_id) << "MGA update should preserve item_id";

    // Verify we can read the new tuple
    Tuple tuple_out;
    status = storage_engine_->getTuple(new_page_id, new_item_id, &tuple_out, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to read new tuple: " << ctx.message;

    // Verify tuple data (skip header)
    const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_out.data);
    const uint8_t *data_ptr = tuple_out.data + sizeof(TupleHeader);
    uint32_t data_size = tuple_out.data_size - sizeof(TupleHeader);

    EXPECT_EQ(data_size, large_tuple.size() - sizeof(TupleHeader));
    EXPECT_EQ(std::memcmp(data_ptr,
                          large_tuple.data() + sizeof(TupleHeader),
                          large_tuple.size() - sizeof(TupleHeader)),
              0)
        << "Tuple data mismatch";

    // Cross-page update in MGA is represented by a back-version pointer.
    EXPECT_TRUE(hdr->hasBackVersion()) << "Updated tuple should have back version pointer";
    const TID back_tid = hdr->getBackVersionTID();
    EXPECT_NE(static_cast<uint32_t>(getPageNumber(back_tid.gpid)), page_id)
        << "Back version should be placed on a different page";
}

/**
 * Test 1b: Back-version placement prefers the closest same-extent candidate.
 */
TEST_F(CrossPageUpdateTest, CrossPageBackVersionPrefersSameExtentCandidate)
{
    ErrorContext ctx;
    constexpr uint32_t kBackVersionExtentPages = 32;
    constexpr uint32_t kBackVersionLocalityBucketPages = 128;

    auto small_tuple = createTupleData(120, 0x7A);
    uint32_t page_id = 0;
    uint32_t candidate_page_id = 0;
    do
    {
        page_id = allocateOwnedHeapPage(test_table_id_, &ctx);
        candidate_page_id = allocateOwnedHeapPage(test_table_id_, &ctx);
    } while ((page_id / 32u) != (candidate_page_id / 32u) || (page_id % 32u) == 31u);

    void *page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK)
        << ctx.message;
    auto *page_data = static_cast<uint8_t *>(page_buffer);
    HeapPage primary_heap_page(page_data, db_->page_size(), nullptr, db_.get(), test_table_id_);
    uint16_t item_id = 0;
    Status status = primary_heap_page.insertTuple(small_tuple.data(),
                                                  static_cast<uint32_t>(small_tuple.size()),
                                                  txn_mgr_ ? txn_mgr_->getCurrentXid() : 100,
                                                  &item_id,
                                                  &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to place primary tuple: " << ctx.message;
    ASSERT_EQ(db_->buffer_pool()->unpinPage(page_id, true, &ctx), Status::OK) << ctx.message;

    fillPageAlmostFull(page_id, &ctx);

    ASSERT_EQ(page_id / 32, candidate_page_id / 32)
        << "Test setup expected a same-extent spill page";

    std::vector<GPID> table_pages;
    ASSERT_EQ(db_->catalog_manager()->enumerateTablePages(test_table_id_, table_pages, &ctx),
              Status::OK)
        << "Failed to enumerate table pages: " << ctx.message;

    auto same_extent = [&](uint32_t lhs, uint32_t rhs) {
        return (lhs / kBackVersionExtentPages) == (rhs / kBackVersionExtentPages);
    };
    auto same_bucket = [&](uint32_t lhs, uint32_t rhs) {
        return (lhs / kBackVersionLocalityBucketPages) == (rhs / kBackVersionLocalityBucketPages);
    };
    auto distance = [&](uint32_t lhs, uint32_t rhs) {
        return (lhs > rhs) ? (lhs - rhs) : (rhs - lhs);
    };
    auto tier = [&](uint32_t primary_page_id, uint32_t possible_page_id) {
        if (same_extent(primary_page_id, possible_page_id) &&
            same_bucket(primary_page_id, possible_page_id))
        {
            return 0u;
        }
        if (same_bucket(primary_page_id, possible_page_id))
        {
            return 1u;
        }
        return 2u;
    };

    uint32_t expected_back_page_id = 0;
    bool expected_back_page_valid = false;
    for (const auto &page_gpid : table_pages)
    {
        const uint32_t possible_page_id = static_cast<uint32_t>(getPageNumber(page_gpid));
        if (possible_page_id == page_id)
        {
            continue;
        }

        void *candidate_buffer = nullptr;
        ASSERT_EQ(db_->buffer_pool()->pinPage(possible_page_id, &candidate_buffer, &ctx), Status::OK)
            << "Failed to pin candidate page " << possible_page_id << ": " << ctx.message;

        auto *candidate_data = static_cast<uint8_t *>(candidate_buffer);
        HeapPage candidate_heap_page(candidate_data, db_->page_size());
        const bool has_space = candidate_heap_page.hasFreeSpace(
            static_cast<uint32_t>(small_tuple.size()));
        ASSERT_EQ(db_->buffer_pool()->unpinPage(possible_page_id, false, &ctx), Status::OK)
            << ctx.message;
        if (!has_space)
        {
            continue;
        }

        if (!expected_back_page_valid)
        {
            expected_back_page_id = possible_page_id;
            expected_back_page_valid = true;
            continue;
        }

        const uint32_t possible_tier = tier(page_id, possible_page_id);
        const uint32_t expected_tier = tier(page_id, expected_back_page_id);
        if (possible_tier < expected_tier ||
            (possible_tier == expected_tier &&
             (distance(page_id, possible_page_id) <
                  distance(page_id, expected_back_page_id) ||
              (distance(page_id, possible_page_id) ==
                   distance(page_id, expected_back_page_id) &&
               possible_page_id < expected_back_page_id))))
        {
            expected_back_page_id = possible_page_id;
        }
    }

    ASSERT_TRUE(expected_back_page_valid)
        << "Test setup expected at least one non-primary page with room for the back version";
    ASSERT_TRUE(same_extent(page_id, expected_back_page_id))
        << "Expected back-version page should stay within the primary extent";

    auto updated_tuple = createCrossPageUpdateTuple(page_id, item_id, 0x5D, &ctx);
    uint32_t updated_page_id = 0;
    uint16_t updated_item_id = 0;
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                          updated_tuple.data(), updated_tuple.size(),
                                          &updated_page_id, &updated_item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << "Cross-page update failed: " << ctx.message;
    ASSERT_EQ(updated_page_id, page_id);
    ASSERT_EQ(updated_item_id, item_id);

    Tuple tuple_out;
    status = storage_engine_->getTuple(updated_page_id, updated_item_id, &tuple_out, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to read updated tuple: " << ctx.message;

    const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_out.data);
    ASSERT_TRUE(hdr->hasBackVersion());
    const TID back_tid = hdr->getBackVersionTID();
    const uint32_t back_page_id = static_cast<uint32_t>(getPageNumber(back_tid.gpid));

    EXPECT_EQ(back_page_id, expected_back_page_id)
        << "Back-version placement should choose the best same-table page by "
           "same-extent locality, distance, and page-id tie-break";
}

/**
 * Test 2: Verify version chain links across pages
 */
TEST_F(CrossPageUpdateTest, VersionChainLinksAcrossPages)
{
    ErrorContext ctx;

    // Insert initial tuple
    auto tuple1 = createTupleData(120, 0x11);
    uint32_t page_id;
    uint16_t item_id;
    Status status = storage_engine_->insertTuple(test_table_id_, tuple1.data(),
                                                 tuple1.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Fill page
    fillPageAlmostFull(page_id, &ctx);

    // Update to trigger cross-page update
    auto tuple2 = createCrossPageUpdateTuple(page_id, item_id, 0x22, &ctx);
    uint32_t new_page_id;
    uint16_t new_item_id;
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                          tuple2.data(), tuple2.size(),
                                          &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify current tuple points to back version on a different page.
    void *primary_page_buffer;
    status = db_->buffer_pool()->pinPage(page_id, &primary_page_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto *primary_page_data = static_cast<uint8_t *>(primary_page_buffer);
    HeapPage primary_heap_page(primary_page_data, db_->page_size());

    const uint8_t *primary_tuple_data;
    uint32_t primary_tuple_size;
    status = primary_heap_page.getTuple(item_id, &primary_tuple_data, &primary_tuple_size, &ctx);
    ASSERT_EQ(status, Status::OK);

    const auto *primary_hdr = reinterpret_cast<const TupleHeader *>(primary_tuple_data);
    EXPECT_EQ(new_page_id, page_id);
    EXPECT_EQ(new_item_id, item_id);
    EXPECT_TRUE(primary_hdr->hasBackVersion()) << "Current tuple should point to back version";

    TID back_tid = primary_hdr->getBackVersionTID();
    uint32_t back_page_id = static_cast<uint32_t>(getPageNumber(back_tid.gpid));
    EXPECT_NE(back_page_id, page_id) << "Back version should be cross-page";

    db_->buffer_pool()->unpinPage(page_id, false, &ctx);

    // Verify back version stores old payload.
    void *back_page_buffer;
    status = db_->buffer_pool()->pinPage(back_page_id, &back_page_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage back_page(static_cast<uint8_t *>(back_page_buffer), db_->page_size());
    const uint8_t *back_tuple_data;
    uint32_t back_tuple_size;
    status = back_page.getTuple(back_tid.slot, &back_tuple_data, &back_tuple_size, &ctx);
    ASSERT_EQ(status, Status::OK);

    const auto *back_hdr = reinterpret_cast<const TupleHeader *>(back_tuple_data);
    const uint8_t *back_data_ptr = back_tuple_data + sizeof(TupleHeader);
    uint32_t back_data_size = back_tuple_size - sizeof(TupleHeader);
    EXPECT_EQ(back_data_size, tuple1.size() - sizeof(TupleHeader));
    EXPECT_EQ(std::memcmp(back_data_ptr,
                          tuple1.data() + sizeof(TupleHeader),
                          tuple1.size() - sizeof(TupleHeader)),
              0);
    EXPECT_NE(back_hdr->xmax, 0UL) << "Back version should carry xmax";

    db_->buffer_pool()->unpinPage(back_page_id, false, &ctx);
}

/**
 * Test 3: Multiple cross-page updates create proper chain
 */
TEST_F(CrossPageUpdateTest, MultipleUpdatesCreateChain)
{
    ErrorContext ctx;

    // Insert initial tuple
    auto tuple1 = createTupleData(100, 0x11);
    uint32_t page_id_v1;
    uint16_t item_id_v1;
    Status status = storage_engine_->insertTuple(test_table_id_, tuple1.data(),
                                                 tuple1.size(), &page_id_v1, &item_id_v1, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Fill first page
    fillPageAlmostFull(page_id_v1, &ctx);

    // First cross-page update
    auto tuple2 = createCrossPageUpdateTuple(page_id_v1, item_id_v1, 0x22, &ctx);
    uint32_t page_id_second;
    uint16_t item_id_second;
    status = storage_engine_->updateTuple(test_table_id_, page_id_v1, item_id_v1,
                                          tuple2.data(), tuple2.size(),
                                          &page_id_second, &item_id_second, &ctx);
    ASSERT_EQ(status, Status::OK) << "First cross-page update failed: " << ctx.message;
    EXPECT_EQ(page_id_second, page_id_v1) << "MGA should preserve page_id";
    EXPECT_EQ(item_id_second, item_id_v1) << "MGA should preserve item_id";

    // Fill second page
    fillPageAlmostFull(page_id_second, &ctx);

    // Second cross-page update
    auto tuple3 = createCrossPageUpdateTuple(page_id_second, item_id_second, 0x33, &ctx);
    uint32_t page_id_v3;
    uint16_t item_id_v3;
    status = storage_engine_->updateTuple(test_table_id_, page_id_second, item_id_second,
                                          tuple3.data(), tuple3.size(),
                                          &page_id_v3, &item_id_v3, &ctx);
    ASSERT_EQ(status, Status::OK) << "Second cross-page update failed: " << ctx.message;
    EXPECT_EQ(page_id_v3, page_id_second) << "MGA should preserve page_id";
    EXPECT_EQ(item_id_v3, item_id_second) << "MGA should preserve item_id";

    // Verify latest tuple payload is visible at stable TID.
    Tuple latest_tuple;
    status = storage_engine_->getTuple(page_id_v1, item_id_v1, &latest_tuple, &ctx);
    ASSERT_EQ(status, Status::OK);
    const uint8_t *latest_data = latest_tuple.data + sizeof(TupleHeader);
    uint32_t latest_size = latest_tuple.data_size - sizeof(TupleHeader);
    EXPECT_EQ(latest_size, tuple3.size() - sizeof(TupleHeader));
    EXPECT_EQ(std::memcmp(latest_data,
                          tuple3.data() + sizeof(TupleHeader),
                          tuple3.size() - sizeof(TupleHeader)),
              0);

    // Latest tuple should keep a back-version pointer after multi-update chain.
    const auto *latest_hdr = reinterpret_cast<const TupleHeader *>(latest_tuple.data);
    EXPECT_TRUE(latest_hdr->hasBackVersion());
}

/**
 * Test 4: HOT update (same page) vs cross-page update
 */
TEST_F(CrossPageUpdateTest, HOTUpdateVsCrossPage)
{
    ErrorContext ctx;

    // Insert initial tuple on an empty page
    auto tuple1 = createTupleData(120, 0xAA);
    uint32_t page_id;
    uint16_t item_id;
    Status status = storage_engine_->insertTuple(test_table_id_, tuple1.data(),
                                                 tuple1.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Update with slightly larger tuple that still fits on same page (HOT update)
    auto tuple2 = createTupleData(140, 0xBB);
    uint32_t new_page_id;
    uint16_t new_item_id;
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                          tuple2.data(), tuple2.size(),
                                          &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // MGA semantics: stable TID on update.
    EXPECT_EQ(new_page_id, page_id) << "HOT update should keep tuple on same page";
    EXPECT_EQ(new_item_id, item_id) << "MGA update should keep tuple item_id stable";

    // Now fill the page and do another update (cross-page)
    fillPageAlmostFull(page_id, &ctx);

    auto tuple3 = createCrossPageUpdateTuple(new_page_id, new_item_id, 0xCC, &ctx);
    uint32_t final_page_id;
    uint16_t final_item_id;
    status = storage_engine_->updateTuple(test_table_id_, new_page_id, new_item_id,
                                          tuple3.data(), tuple3.size(),
                                          &final_page_id, &final_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Cross-page in MGA keeps primary TID stable and links to back version on another page.
    EXPECT_EQ(final_page_id, page_id) << "MGA cross-page should preserve page_id";
    EXPECT_EQ(final_item_id, item_id) << "MGA cross-page should preserve item_id";

    Tuple current_tuple;
    status = storage_engine_->getTuple(final_page_id, final_item_id, &current_tuple, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto *current_hdr = reinterpret_cast<const TupleHeader *>(current_tuple.data);
    ASSERT_TRUE(current_hdr->hasBackVersion());
    TID back_tid = current_hdr->getBackVersionTID();
    EXPECT_NE(static_cast<uint32_t>(getPageNumber(back_tid.gpid)), page_id)
        << "Back version should be allocated on a different page";
}

TEST_F(CrossPageUpdateTest, SameAndCrossPageHeadsShareMutationContract)
{
    ErrorContext ctx;

    auto tuple1 = createTupleData(120, 0x61);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    Status status = storage_engine_->insertTuple(test_table_id_, tuple1.data(),
                                                 tuple1.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    auto same_page_tuple = createTupleData(140, 0x62);
    uint32_t same_page_id = 0;
    uint16_t same_item_id = 0;
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                          same_page_tuple.data(), same_page_tuple.size(),
                                          &same_page_id, &same_item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_EQ(same_page_id, page_id);
    ASSERT_EQ(same_item_id, item_id);

    void *same_page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(page_id, &same_page_buffer, &ctx), Status::OK)
        << ctx.message;
    HeapPage same_page(static_cast<uint8_t *>(same_page_buffer), db_->page_size());
    const uint8_t *same_head_tuple = nullptr;
    uint32_t same_head_size = 0;
    ASSERT_EQ(same_page.getTuple(item_id, &same_head_tuple, &same_head_size, &ctx), Status::OK)
        << ctx.message;
    const auto *same_head_hdr = reinterpret_cast<const TupleHeader *>(same_head_tuple);
    ASSERT_TRUE(same_head_hdr->hasBackVersion());
    EXPECT_EQ((same_head_hdr->infomask & TupleHeader::HEAP_CHAIN), 0u);
    TID same_back_tid = same_head_hdr->getBackVersionTID();
    ASSERT_EQ(static_cast<uint32_t>(getPageNumber(same_back_tid.gpid)), page_id);
    const uint8_t *same_back_tuple = nullptr;
    uint32_t same_back_size = 0;
    ASSERT_EQ(same_page.getTuple(same_back_tid.slot, &same_back_tuple, &same_back_size, &ctx),
              Status::OK) << ctx.message;
    const auto *same_back_hdr = reinterpret_cast<const TupleHeader *>(same_back_tuple);
    EXPECT_NE((same_back_hdr->infomask & TupleHeader::HEAP_CHAIN), 0u);
    EXPECT_NE((same_back_hdr->infomask & TupleHeader::HEAP_UPDATED), 0u);
    db_->buffer_pool()->unpinPage(page_id, false, &ctx);

    fillPageAlmostFull(page_id, &ctx);
    auto cross_page_tuple = createCrossPageUpdateTuple(page_id, item_id, 0x63, &ctx);
    uint32_t final_page_id = 0;
    uint16_t final_item_id = 0;
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                          cross_page_tuple.data(), cross_page_tuple.size(),
                                          &final_page_id, &final_item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_EQ(final_page_id, page_id);
    ASSERT_EQ(final_item_id, item_id);

    void *cross_head_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(page_id, &cross_head_buffer, &ctx), Status::OK)
        << ctx.message;
    HeapPage cross_head_page(static_cast<uint8_t *>(cross_head_buffer), db_->page_size());
    const uint8_t *cross_head_tuple = nullptr;
    uint32_t cross_head_size = 0;
    ASSERT_EQ(cross_head_page.getTuple(item_id, &cross_head_tuple, &cross_head_size, &ctx),
              Status::OK) << ctx.message;
    const auto *cross_head_hdr = reinterpret_cast<const TupleHeader *>(cross_head_tuple);
    ASSERT_TRUE(cross_head_hdr->hasBackVersion());
    EXPECT_EQ((cross_head_hdr->infomask & TupleHeader::HEAP_CHAIN), 0u);
    TID cross_back_tid = cross_head_hdr->getBackVersionTID();
    EXPECT_NE(static_cast<uint32_t>(getPageNumber(cross_back_tid.gpid)), page_id);
    db_->buffer_pool()->unpinPage(page_id, false, &ctx);

    void *cross_back_buffer = nullptr;
    uint32_t cross_back_page_id = static_cast<uint32_t>(getPageNumber(cross_back_tid.gpid));
    ASSERT_EQ(db_->buffer_pool()->pinPage(cross_back_page_id, &cross_back_buffer, &ctx),
              Status::OK) << ctx.message;
    HeapPage cross_back_page(static_cast<uint8_t *>(cross_back_buffer), db_->page_size());
    const uint8_t *cross_back_tuple = nullptr;
    uint32_t cross_back_size = 0;
    ASSERT_EQ(cross_back_page.getTuple(cross_back_tid.slot, &cross_back_tuple, &cross_back_size,
                                       &ctx),
              Status::OK) << ctx.message;
    const auto *cross_back_hdr = reinterpret_cast<const TupleHeader *>(cross_back_tuple);
    EXPECT_NE((cross_back_hdr->infomask & TupleHeader::HEAP_CHAIN), 0u);
    EXPECT_NE((cross_back_hdr->infomask & TupleHeader::HEAP_UPDATED), 0u);
    EXPECT_NE((cross_back_hdr->infomask & TupleHeader::HEAP_MOVED), 0u);
    db_->buffer_pool()->unpinPage(cross_back_page_id, false, &ctx);
}

TEST_F(CrossPageUpdateTest, SameAndCrossPageMutationLifecycleLeavesChainAuditClean)
{
    ErrorContext ctx;

    auto tuple1 = createTupleData(120, 0x71);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    Status status = storage_engine_->insertTuple(test_table_id_, tuple1.data(),
                                                 tuple1.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    auto same_page_tuple = createTupleData(140, 0x72);
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                          same_page_tuple.data(), same_page_tuple.size(),
                                          nullptr, nullptr, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    fillPageAlmostFull(page_id, &ctx);
    auto cross_page_tuple = createCrossPageUpdateTuple(page_id, item_id, 0x73, &ctx);
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                          cross_page_tuple.data(), cross_page_tuple.size(),
                                          nullptr, nullptr, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    void *primary_page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(page_id, &primary_page_buffer, &ctx), Status::OK)
        << ctx.message;
    HeapPage primary_page(static_cast<uint8_t *>(primary_page_buffer),
                          db_->page_size(),
                          nullptr,
                          db_.get(),
                          test_table_id_);
    HeapPage::VersionChainAuditResult primary_audit{};
    ASSERT_EQ(primary_page.auditVersionChainMetadata(HeapPage::VersionChainAuditMode::READ_ONLY,
                                                     &primary_audit,
                                                     &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(primary_audit.anomaly_count, 0u) << primary_audit.summary;

    const uint8_t *head_tuple = nullptr;
    uint32_t head_size = 0;
    ASSERT_EQ(primary_page.getTuple(item_id, &head_tuple, &head_size, &ctx), Status::OK)
        << ctx.message;
    const auto *head_hdr = reinterpret_cast<const TupleHeader *>(head_tuple);
    ASSERT_TRUE(head_hdr->hasBackVersion());
    TID cross_back_tid = head_hdr->getBackVersionTID();
    db_->buffer_pool()->unpinPage(page_id, false, &ctx);

    const uint32_t cross_back_page_id =
        static_cast<uint32_t>(getPageNumber(cross_back_tid.gpid));
    ASSERT_NE(cross_back_page_id, page_id);
}

/**
 * Test 5: Verify MVCC visibility with cross-page updates
 */
TEST_F(CrossPageUpdateTest, MVCCVisibilityAcrossPages)
{
    ErrorContext ctx;

    // Insert initial tuple
    auto tuple1 = createTupleData(120, 0xAA);
    uint32_t page_id;
    uint16_t item_id;
    Status status = storage_engine_->insertTuple(test_table_id_, tuple1.data(),
                                                 tuple1.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Get current XID for visibility checks
    uint64_t xid_before_update = txn_mgr_ ? txn_mgr_->getCurrentXid() : 100;

    // Fill page
    fillPageAlmostFull(page_id, &ctx);

    // Do cross-page update
    auto tuple2 = createCrossPageUpdateTuple(page_id, item_id, 0xBB, &ctx);
    uint32_t new_page_id;
    uint16_t new_item_id;
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                          tuple2.data(), tuple2.size(),
                                          &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    uint64_t xid_after_update = txn_mgr_ ? txn_mgr_->getCurrentXid() : 100;

    EXPECT_EQ(new_page_id, page_id);
    EXPECT_EQ(new_item_id, item_id);

    // Current tuple should be visible at stable TID and point to a back version.
    void *current_page_buffer;
    status = db_->buffer_pool()->pinPage(page_id, &current_page_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage current_page(static_cast<uint8_t *>(current_page_buffer), db_->page_size());
    const uint8_t *current_tuple_data;
    uint32_t current_tuple_size;
    status = current_page.getTuple(item_id, &current_tuple_data, &current_tuple_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto *current_hdr = reinterpret_cast<const TupleHeader *>(current_tuple_data);
    ASSERT_TRUE(current_hdr->hasBackVersion());
    EXPECT_EQ(current_hdr->xmax, 0UL) << "Current tuple should not be deleted";
    uint64_t current_xmin = current_hdr->xmin;

    TID back_tid = current_hdr->getBackVersionTID();
    uint32_t back_page_id = static_cast<uint32_t>(getPageNumber(back_tid.gpid));

    db_->buffer_pool()->unpinPage(page_id, false, &ctx);

    // Back version should carry old visibility metadata.
    void *back_page_buffer;
    status = db_->buffer_pool()->pinPage(back_page_id, &back_page_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage back_page(static_cast<uint8_t *>(back_page_buffer), db_->page_size());
    const uint8_t *back_tuple_data;
    uint32_t back_tuple_size;
    status = back_page.getTuple(back_tid.slot, &back_tuple_data, &back_tuple_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto *back_hdr = reinterpret_cast<const TupleHeader *>(back_tuple_data);

    EXPECT_NE(back_hdr->xmax, 0UL) << "Back version should have xmax set";
    EXPECT_EQ(current_xmin, back_hdr->xmax)
        << "Current tuple xmin should match back-version xmax";
    EXPECT_GE(xid_after_update, xid_before_update);
    db_->buffer_pool()->unpinPage(back_page_id, false, &ctx);
}

/**
 * Test 6: Test with TOAST-sized data (very large tuples)
 */
TEST_F(CrossPageUpdateTest, LargeTupleUpdate)
{
    ErrorContext ctx;

    // Insert small initial tuple
    auto small_tuple = createTupleData(120, 0xAA);
    uint32_t page_id;
    uint16_t item_id;
    Status status = storage_engine_->insertTuple(test_table_id_, small_tuple.data(),
                                                 small_tuple.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Fill page
    fillPageAlmostFull(page_id, &ctx);

    // Update with very large tuple (multiple KB)
    auto huge_tuple = createTupleData(6000, 0xBB); // 6KB tuple
    uint32_t new_page_id;
    uint16_t new_item_id;
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                          huge_tuple.data(), huge_tuple.size(),
                                          &new_page_id, &new_item_id, &ctx);

    // Should succeed (either on new page or TOASTed)
    ASSERT_EQ(status, Status::OK) << "Large tuple update should succeed: " << ctx.message;

    // Verify we can read it back
    Tuple tuple_out;
    status = storage_engine_->getTuple(new_page_id, new_item_id, &tuple_out, &ctx);
    ASSERT_EQ(status, Status::OK) << "Should be able to read large tuple: " << ctx.message;
}

/**
 * Test 7: Cross-page update error handling
 */
TEST_F(CrossPageUpdateTest, ErrorHandling)
{
    ErrorContext ctx;

    // Try to update non-existent tuple
    auto tuple_data = createTupleData(512);
    uint32_t new_page_id;
    uint16_t new_item_id;

    Status status = storage_engine_->updateTuple(test_table_id_, 999, 999,
                                                 tuple_data.data(), tuple_data.size(),
                                                 &new_page_id, &new_item_id, &ctx);

    // Should fail gracefully
    EXPECT_NE(status, Status::OK) << "Update of non-existent tuple should fail";
}
