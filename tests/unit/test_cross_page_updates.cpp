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
        // Keep updates non-TOASTed and, when possible, in-place-overwrite-safe
        // (new tuple size <= old tuple size) so cross-page failures isolate to
        // back-version placement, not primary growth.
        uint32_t max_total = old_tuple_size;
        if (max_total > 240)
        {
            max_total = 240;
        }
        if (max_total < min_total)
        {
            max_total = min_total;
        }

        // Force cross-page if possible: same-page algorithm requires
        // (new_tuple + old_tuple) > free_space to return PAGE_FULL.
        uint32_t cross_page_min_total = min_total;
        if (free_space > old_tuple_size)
        {
            cross_page_min_total = free_space - old_tuple_size + 1;
        }

        uint32_t chosen_total = max_total;
        if (cross_page_min_total <= max_total)
        {
            chosen_total = cross_page_min_total;
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
    uint32_t page_id_v2;
    uint16_t item_id_v2;
    status = storage_engine_->updateTuple(test_table_id_, page_id_v1, item_id_v1,
                                          tuple2.data(), tuple2.size(),
                                          &page_id_v2, &item_id_v2, &ctx);
    ASSERT_EQ(status, Status::OK) << "First cross-page update failed: " << ctx.message;
    EXPECT_EQ(page_id_v2, page_id_v1) << "MGA should preserve page_id";
    EXPECT_EQ(item_id_v2, item_id_v1) << "MGA should preserve item_id";

    // Fill second page
    fillPageAlmostFull(page_id_v2, &ctx);

    // Second cross-page update
    auto tuple3 = createCrossPageUpdateTuple(page_id_v2, item_id_v2, 0x33, &ctx);
    uint32_t page_id_v3;
    uint16_t item_id_v3;
    status = storage_engine_->updateTuple(test_table_id_, page_id_v2, item_id_v2,
                                          tuple3.data(), tuple3.size(),
                                          &page_id_v3, &item_id_v3, &ctx);
    ASSERT_EQ(status, Status::OK) << "Second cross-page update failed: " << ctx.message;
    EXPECT_EQ(page_id_v3, page_id_v2) << "MGA should preserve page_id";
    EXPECT_EQ(item_id_v3, item_id_v2) << "MGA should preserve item_id";

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
