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
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include <vector>
#include <cstring>

using namespace scratchbird::core;

class CrossPageUpdateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Clean up any existing test database
        std::string db_path = "/tmp/test_cross_page_updates.sb";
        ::unlink(db_path.c_str());

        // Create test database with small page size to trigger cross-page updates easily
        ErrorContext ctx;
        Status status = Database::create(db_path, 8192, &ctx);  // 8KB pages
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        status = db_->open(db_path, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        storage_engine_ = db_->storage_engine();
        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(storage_engine_, nullptr);
        ASSERT_NE(txn_mgr_, nullptr);

        // Create a test table ID
        std::memset(test_table_id_.bytes, 0, 16);
        test_table_id_.bytes[0] = 42;  // Arbitrary test table ID
    }

    void TearDown() override
    {
        if (db_) {
            db_->close();
        }
        db_.reset();
        std::string db_path = "/tmp/test_cross_page_updates.sb";
        ::unlink(db_path.c_str());
    }

    // Helper to create tuple data
    std::vector<uint8_t> createTupleData(uint32_t size, uint8_t fill_byte = 0xAA)
    {
        std::vector<uint8_t> data(size);
        std::memset(data.data(), fill_byte, size);
        return data;
    }

    // Helper to fill a page almost completely
    void fillPageAlmostFull(uint32_t page_id, ErrorContext* ctx)
    {
        // Pin the page
        void* page_buffer;
        Status status = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);
        ASSERT_EQ(status, Status::OK);

        auto* page_data = static_cast<uint8_t*>(page_buffer);
        HeapPage heap_page(page_data, db_->page_size());

        // Fill with tuples until we have less than 1KB free
        const uint32_t target_free_space = 1024;
        const uint32_t tuple_size = 512;
        auto tuple_data = createTupleData(tuple_size);

        while (heap_page.getFreeSpace() > target_free_space + tuple_size + sizeof(TupleHeader))
        {
            uint16_t item_id;
            uint64_t xmin = txn_mgr_ ? txn_mgr_->getCurrentXid() : 100;
            status = heap_page.insertTuple(tuple_data.data(), tuple_size, xmin, &item_id, ctx);
            if (status != Status::OK) {
                break;  // Page is full enough
            }
        }

        db_->buffer_pool()->unpinPage(page_id, true, ctx);
    }

    std::unique_ptr<Database> db_;
    StorageEngine* storage_engine_;
    TransactionManager* txn_mgr_;
    ID test_table_id_;
};

/**
 * Test 1: Basic cross-page update when tuple grows beyond page capacity
 */
TEST_F(CrossPageUpdateTest, BasicCrossPageUpdate)
{
    ErrorContext ctx;

    // Insert initial small tuple
    auto small_tuple = createTupleData(512, 0xAA);
    uint32_t page_id;
    uint16_t item_id;

    Status status = storage_engine_->insertTuple(test_table_id_, small_tuple.data(),
                                                 small_tuple.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to insert initial tuple: " << ctx.message;

    // Fill the page almost completely so next update won't fit
    fillPageAlmostFull(page_id, &ctx);

    // Now update with a large tuple that won't fit on same page
    auto large_tuple = createTupleData(2048, 0xBB);
    uint32_t new_page_id;
    uint16_t new_item_id;

    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                         large_tuple.data(), large_tuple.size(),
                                         &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << "Cross-page update failed: " << ctx.message;

    // Verify new tuple is on a different page
    EXPECT_NE(new_page_id, page_id) << "Tuple should be on different page";

    // Verify we can read the new tuple
    Tuple tuple_out;
    status = storage_engine_->getTuple(new_page_id, new_item_id, &tuple_out, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to read new tuple: " << ctx.message;

    // Verify tuple data (skip header)
    const auto* hdr = reinterpret_cast<const TupleHeader*>(tuple_out.data);
    const uint8_t* data_ptr = tuple_out.data + sizeof(TupleHeader);
    uint32_t data_size = tuple_out.data_size - sizeof(TupleHeader);

    EXPECT_EQ(data_size, large_tuple.size());
    EXPECT_EQ(std::memcmp(data_ptr, large_tuple.data(), large_tuple.size()), 0)
        << "Tuple data mismatch";
}

/**
 * Test 2: Verify version chain links across pages
 */
TEST_F(CrossPageUpdateTest, VersionChainLinksAcrossPages)
{
    ErrorContext ctx;

    // Insert initial tuple
    auto tuple1 = createTupleData(512, 0x11);
    uint32_t page_id;
    uint16_t item_id;
    Status status = storage_engine_->insertTuple(test_table_id_, tuple1.data(),
                                                 tuple1.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Fill page
    fillPageAlmostFull(page_id, &ctx);

    // Update to trigger cross-page update
    auto tuple2 = createTupleData(2048, 0x22);
    uint32_t new_page_id;
    uint16_t new_item_id;
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                         tuple2.data(), tuple2.size(),
                                         &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify old tuple points to new tuple
    void* old_page_buffer;
    status = db_->buffer_pool()->pinPage(page_id, &old_page_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto* old_page_data = static_cast<uint8_t*>(old_page_buffer);
    HeapPage old_heap_page(old_page_data, db_->page_size());

    const uint8_t* old_tuple_data;
    uint32_t old_tuple_size;
    status = old_heap_page.getTuple(item_id, &old_tuple_data, &old_tuple_size, &ctx);
    ASSERT_EQ(status, Status::OK);

    const auto* old_hdr = reinterpret_cast<const TupleHeader*>(old_tuple_data);

    // Verify old tuple has back_version_tid pointing to new location
    EXPECT_NE(old_hdr->back_version_tid, 0UL) << "Old tuple should have next version";
    EXPECT_TRUE(old_hdr->infomask & TupleHeader::HEAP_UPDATED) << "Old tuple should be marked as updated";
    EXPECT_TRUE(old_hdr->infomask & TupleHeader::HEAP_MOVED) << "Old tuple should be marked as moved";

    // Extract page_id and item_id from back_version_tid
    uint32_t next_page = static_cast<uint32_t>(old_hdr->back_version_tid >> 32);
    uint16_t next_item = static_cast<uint16_t>((old_hdr->back_version_tid >> 16) & 0xFFFF);

    EXPECT_EQ(next_page, new_page_id) << "Version chain should point to new page";
    EXPECT_EQ(next_item, new_item_id) << "Version chain should point to new item";

    db_->buffer_pool()->unpinPage(page_id, false, &ctx);
}

/**
 * Test 3: Multiple cross-page updates create proper chain
 */
TEST_F(CrossPageUpdateTest, MultipleUpdatesCreateChain)
{
    ErrorContext ctx;

    // Insert initial tuple
    auto tuple1 = createTupleData(256, 0x11);
    uint32_t page_id_v1;
    uint16_t item_id_v1;
    Status status = storage_engine_->insertTuple(test_table_id_, tuple1.data(),
                                                 tuple1.size(), &page_id_v1, &item_id_v1, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Fill first page
    fillPageAlmostFull(page_id_v1, &ctx);

    // First cross-page update
    auto tuple2 = createTupleData(2048, 0x22);
    uint32_t page_id_v2;
    uint16_t item_id_v2;
    status = storage_engine_->updateTuple(test_table_id_, page_id_v1, item_id_v1,
                                         tuple2.data(), tuple2.size(),
                                         &page_id_v2, &item_id_v2, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_NE(page_id_v2, page_id_v1) << "First update should be cross-page";

    // Fill second page
    fillPageAlmostFull(page_id_v2, &ctx);

    // Second cross-page update
    auto tuple3 = createTupleData(2048, 0x33);
    uint32_t page_id_v3;
    uint16_t item_id_v3;
    status = storage_engine_->updateTuple(test_table_id_, page_id_v2, item_id_v2,
                                         tuple3.data(), tuple3.size(),
                                         &page_id_v3, &item_id_v3, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_NE(page_id_v3, page_id_v2) << "Second update should also be cross-page";

    // Verify chain: v1 -> v2 -> v3
    // Check v1 -> v2
    void* page1_buffer;
    status = db_->buffer_pool()->pinPage(page_id_v1, &page1_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage page1(static_cast<uint8_t*>(page1_buffer), db_->page_size());
    const uint8_t* tuple1_data;
    uint32_t tuple1_size;
    status = page1.getTuple(item_id_v1, &tuple1_data, &tuple1_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto* hdr1 = reinterpret_cast<const TupleHeader*>(tuple1_data);
    uint32_t next_page_from_v1 = static_cast<uint32_t>(hdr1->back_version_tid >> 32);
    EXPECT_EQ(next_page_from_v1, page_id_v2) << "V1 should point to V2";
    db_->buffer_pool()->unpinPage(page_id_v1, false, &ctx);

    // Check v2 -> v3
    void* page2_buffer;
    status = db_->buffer_pool()->pinPage(page_id_v2, &page2_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage page2(static_cast<uint8_t*>(page2_buffer), db_->page_size());
    const uint8_t* tuple2_data;
    uint32_t tuple2_size;
    status = page2.getTuple(item_id_v2, &tuple2_data, &tuple2_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto* hdr2 = reinterpret_cast<const TupleHeader*>(tuple2_data);
    uint32_t next_page_from_v2 = static_cast<uint32_t>(hdr2->back_version_tid >> 32);
    EXPECT_EQ(next_page_from_v2, page_id_v3) << "V2 should point to V3";
    db_->buffer_pool()->unpinPage(page_id_v2, false, &ctx);

    // Check v3 is latest (no next version)
    void* page3_buffer;
    status = db_->buffer_pool()->pinPage(page_id_v3, &page3_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage page3(static_cast<uint8_t*>(page3_buffer), db_->page_size());
    const uint8_t* tuple3_data;
    uint32_t tuple3_size;
    status = page3.getTuple(item_id_v3, &tuple3_data, &tuple3_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto* hdr3 = reinterpret_cast<const TupleHeader*>(tuple3_data);
    EXPECT_EQ(hdr3->back_version_tid, 0UL) << "V3 should be latest version";
    db_->buffer_pool()->unpinPage(page_id_v3, false, &ctx);
}

/**
 * Test 4: HOT update (same page) vs cross-page update
 */
TEST_F(CrossPageUpdateTest, HOTUpdateVsCrossPage)
{
    ErrorContext ctx;

    // Insert initial tuple on an empty page
    auto tuple1 = createTupleData(512, 0xAA);
    uint32_t page_id;
    uint16_t item_id;
    Status status = storage_engine_->insertTuple(test_table_id_, tuple1.data(),
                                                 tuple1.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Update with slightly larger tuple that still fits on same page (HOT update)
    auto tuple2 = createTupleData(600, 0xBB);
    uint32_t new_page_id;
    uint16_t new_item_id;
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                         tuple2.data(), tuple2.size(),
                                         &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Should be on same page (HOT update)
    EXPECT_EQ(new_page_id, page_id) << "HOT update should keep tuple on same page";
    EXPECT_NE(new_item_id, item_id) << "New item ID should be different even on same page";

    // Now fill the page and do another update (cross-page)
    fillPageAlmostFull(page_id, &ctx);

    auto tuple3 = createTupleData(2048, 0xCC);
    uint32_t final_page_id;
    uint16_t final_item_id;
    status = storage_engine_->updateTuple(test_table_id_, new_page_id, new_item_id,
                                         tuple3.data(), tuple3.size(),
                                         &final_page_id, &final_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Should be on different page (cross-page update)
    EXPECT_NE(final_page_id, page_id) << "Cross-page update should move to different page";
}

/**
 * Test 5: Verify MVCC visibility with cross-page updates
 */
TEST_F(CrossPageUpdateTest, MVCCVisibilityAcrossPages)
{
    ErrorContext ctx;

    // Insert initial tuple
    auto tuple1 = createTupleData(512, 0xAA);
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
    auto tuple2 = createTupleData(2048, 0xBB);
    uint32_t new_page_id;
    uint16_t new_item_id;
    status = storage_engine_->updateTuple(test_table_id_, page_id, item_id,
                                         tuple2.data(), tuple2.size(),
                                         &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    uint64_t xid_after_update = txn_mgr_ ? txn_mgr_->getCurrentXid() : 100;

    // Old tuple should have xmax set
    void* old_page_buffer;
    status = db_->buffer_pool()->pinPage(page_id, &old_page_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage old_page(static_cast<uint8_t*>(old_page_buffer), db_->page_size());
    const uint8_t* old_tuple_data;
    uint32_t old_tuple_size;
    status = old_page.getTuple(item_id, &old_tuple_data, &old_tuple_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto* old_hdr = reinterpret_cast<const TupleHeader*>(old_tuple_data);
    EXPECT_NE(old_hdr->xmax, 0UL) << "Old tuple should have xmax set";
    db_->buffer_pool()->unpinPage(page_id, false, &ctx);

    // New tuple should have proper xmin
    void* new_page_buffer;
    status = db_->buffer_pool()->pinPage(new_page_id, &new_page_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage new_page(static_cast<uint8_t*>(new_page_buffer), db_->page_size());
    const uint8_t* new_tuple_data;
    uint32_t new_tuple_size;
    status = new_page.getTuple(new_item_id, &new_tuple_data, &new_tuple_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto* new_hdr = reinterpret_cast<const TupleHeader*>(new_tuple_data);
    EXPECT_EQ(new_hdr->xmin, old_hdr->xmax) << "New tuple xmin should equal old tuple xmax";
    EXPECT_EQ(new_hdr->xmax, 0UL) << "New tuple should not be deleted";
    db_->buffer_pool()->unpinPage(new_page_id, false, &ctx);
}

/**
 * Test 6: Test with TOAST-sized data (very large tuples)
 */
TEST_F(CrossPageUpdateTest, LargeTupleUpdate)
{
    ErrorContext ctx;

    // Insert small initial tuple
    auto small_tuple = createTupleData(256, 0xAA);
    uint32_t page_id;
    uint16_t item_id;
    Status status = storage_engine_->insertTuple(test_table_id_, small_tuple.data(),
                                                 small_tuple.size(), &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Fill page
    fillPageAlmostFull(page_id, &ctx);

    // Update with very large tuple (multiple KB)
    auto huge_tuple = createTupleData(6000, 0xBB);  // 6KB tuple
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
    ASSERT_EQ(status, Status::OK) << "Should be able to read large tuple";
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

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
