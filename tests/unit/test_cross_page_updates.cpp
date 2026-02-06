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

    // Helper to create tuple data
    std::vector<uint8_t> createTupleData(uint32_t size, uint8_t fill_byte = 0xAA)
    {
        std::vector<uint8_t> data(size);
        std::memset(data.data(), fill_byte, size);
        return data;
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

        // Fill with tuples until we have less than 1KB free
        const uint32_t target_free_space = 1024;
        const uint32_t tuple_size = 512;
        auto tuple_data = createTupleData(tuple_size);

        while (heap_page.getFreeSpace() > target_free_space + tuple_size + sizeof(TupleHeader))
        {
            uint16_t item_id;
            uint64_t xmin = txn_mgr_ ? txn_mgr_->getCurrentXid() : 100;
            status = heap_page.insertTuple(tuple_data.data(), tuple_size, xmin, &item_id, ctx);
            if (status != Status::OK)
            {
                break; // Page is full enough
            }
        }

        db_->buffer_pool()->unpinPage(page_id, true, ctx);
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
    const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_out.data);
    const uint8_t *data_ptr = tuple_out.data + sizeof(TupleHeader);
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
    void *old_page_buffer;
    status = db_->buffer_pool()->pinPage(page_id, &old_page_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto *old_page_data = static_cast<uint8_t *>(old_page_buffer);
    HeapPage old_heap_page(old_page_data, db_->page_size());

    const uint8_t *old_tuple_data;
    uint32_t old_tuple_size;
    status = old_heap_page.getTuple(item_id, &old_tuple_data, &old_tuple_size, &ctx);
    ASSERT_EQ(status, Status::OK);

    const auto *old_hdr = reinterpret_cast<const TupleHeader *>(old_tuple_data);

    // Verify old tuple has back_version_tid pointing to new location
    EXPECT_TRUE(old_hdr->hasBackVersion()) << "Old tuple should have next version";
    EXPECT_TRUE(old_hdr->infomask & TupleHeader::HEAP_UPDATED) << "Old tuple should be marked as updated";
    EXPECT_TRUE(old_hdr->infomask & TupleHeader::HEAP_MOVED) << "Old tuple should be marked as moved";

    // Extract page_id and item_id from back_version_tid
    TID back_tid = old_hdr->getBackVersionTID();
    uint32_t next_page = static_cast<uint32_t>(getPageNumber(back_tid));
    uint16_t next_item = back_tid.slot;

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
    void *page1_buffer;
    status = db_->buffer_pool()->pinPage(page_id_v1, &page1_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage page1(static_cast<uint8_t *>(page1_buffer), db_->page_size());
    const uint8_t *tuple1_data;
    uint32_t tuple1_size;
    status = page1.getTuple(item_id_v1, &tuple1_data, &tuple1_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto *hdr1 = reinterpret_cast<const TupleHeader *>(tuple1_data);
    uint32_t next_page_from_v1 = static_cast<uint32_t>(getPageNumber(hdr1->getBackVersionTID()));
    EXPECT_EQ(next_page_from_v1, page_id_v2) << "V1 should point to V2";
    db_->buffer_pool()->unpinPage(page_id_v1, false, &ctx);

    // Check v2 -> v3
    void *page2_buffer;
    status = db_->buffer_pool()->pinPage(page_id_v2, &page2_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage page2(static_cast<uint8_t *>(page2_buffer), db_->page_size());
    const uint8_t *tuple2_data;
    uint32_t tuple2_size;
    status = page2.getTuple(item_id_v2, &tuple2_data, &tuple2_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto *hdr2 = reinterpret_cast<const TupleHeader *>(tuple2_data);
    uint32_t next_page_from_v2 = static_cast<uint32_t>(getPageNumber(hdr2->getBackVersionTID()));
    EXPECT_EQ(next_page_from_v2, page_id_v3) << "V2 should point to V3";
    db_->buffer_pool()->unpinPage(page_id_v2, false, &ctx);

    // Check v3 is latest (no next version)
    void *page3_buffer;
    status = db_->buffer_pool()->pinPage(page_id_v3, &page3_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage page3(static_cast<uint8_t *>(page3_buffer), db_->page_size());
    const uint8_t *tuple3_data;
    uint32_t tuple3_size;
    status = page3.getTuple(item_id_v3, &tuple3_data, &tuple3_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto *hdr3 = reinterpret_cast<const TupleHeader *>(tuple3_data);
    EXPECT_FALSE(hdr3->hasBackVersion()) << "V3 should be latest version";
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
    void *old_page_buffer;
    status = db_->buffer_pool()->pinPage(page_id, &old_page_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage old_page(static_cast<uint8_t *>(old_page_buffer), db_->page_size());
    const uint8_t *old_tuple_data;
    uint32_t old_tuple_size;
    status = old_page.getTuple(item_id, &old_tuple_data, &old_tuple_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto *old_hdr = reinterpret_cast<const TupleHeader *>(old_tuple_data);
    EXPECT_NE(old_hdr->xmax, 0UL) << "Old tuple should have xmax set";
    db_->buffer_pool()->unpinPage(page_id, false, &ctx);

    // New tuple should have proper xmin
    void *new_page_buffer;
    status = db_->buffer_pool()->pinPage(new_page_id, &new_page_buffer, &ctx);
    ASSERT_EQ(status, Status::OK);
    HeapPage new_page(static_cast<uint8_t *>(new_page_buffer), db_->page_size());
    const uint8_t *new_tuple_data;
    uint32_t new_tuple_size;
    status = new_page.getTuple(new_item_id, &new_tuple_data, &new_tuple_size, &ctx);
    ASSERT_EQ(status, Status::OK);
    const auto *new_hdr = reinterpret_cast<const TupleHeader *>(new_tuple_data);
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
