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
 * GIN Index DML Integration Tests
 *
 * Tests TASK-DML-1: GIN Index DML Integration (November 20, 2025)
 * - INSERT operations maintain GIN index
 * - UPDATE operations update GIN index (remove old keys, insert new keys)
 * - DELETE operations remove keys from GIN index
 *
 * Test Categories:
 * 1. Basic DML Tests:
 *    - INSERT with simple values
 *    - INSERT with array values (multiple keys extracted)
 *    - DELETE removes all keys for a tuple
 *    - UPDATE detects indexed column changes
 * 2. Key Extraction Tests:
 *    - Default extractor (single key from value)
 *    - Array extractor (multiple keys from array)
 *    - Custom extractors (TSVECTOR, JSONB)
 * 3. Transaction Visibility Tests:
 *    - GIN index respects transaction visibility
 *    - Concurrent inserts/deletes maintain consistency
 * 4. Index Consistency Tests:
 *    - Verify index contains correct keys after DML
 *    - Verify posting lists contain correct TIDs
 *    - Verify no orphaned entries after DELETE
 *
 * Implementation Status:
 * - [x] INSERT integration (storage_engine.cpp:75-88)
 * - [x] DELETE integration (storage_engine.cpp:145-160)
 * - [x] UPDATE integration (uses INSERT/DELETE paths)
 * - [ ] TASK-CRITICAL-1: GIN remove() uses physical deletion (needs MGA fix)
 * - [ ] Specialized key extractors (array, JSONB, TSVECTOR)
 *
 * Prerequisites:
 * - Build issues in rtree_index.cpp and spgist_index.cpp must be resolved
 * - TASK-CRITICAL-1 (GIN MGA compliance) should be completed for full correctness
 *
 * Execution (when enabled):
 *   From build/: ctest -R GinDML -V
 *   Expected: All tests pass, index maintained correctly after DML
 */

#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include <vector>
#include "scratchbird/core/database.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/gin_extractors.h"
#include "scratchbird/core/connection_context.h"

using namespace scratchbird::core;
using namespace scratchbird::sblr;

// NOTE: Tests are currently disabled pending resolution of pre-existing build issues
// in rtree_index.cpp and spgist_index.cpp. Once those are resolved, these tests
// can be enabled and run.
#if 0

class GinDMLTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = "/tmp/test_gin_dml.db";
        std::remove(test_db_path_);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        status = db_->open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        storage_ = db_->storage_engine();
        ASSERT_NE(storage_, nullptr);

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(txn_mgr_, nullptr);
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
        }
        std::remove(test_db_path_);
    }

    // Helper: Create a test table with a GIN-indexed column
    ID createTableWithGINIndex(const std::string& table_name, const std::string& index_name)
    {
        ErrorContext ctx;

        // Create table
        CatalogManager::TableInfo table_info;
        table_info.table_name = table_name;
        table_info.schema_id = catalog_->getPublicSchemaId(&ctx);

        Status status = catalog_->createTable(table_info, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create table: " << ctx.message;

        // Add column for GIN index (e.g., TEXT or ARRAY type)
        CatalogManager::ColumnInfo col_info;
        col_info.column_name = "data";
        col_info.data_type = static_cast<uint16_t>(DataType::TEXT);

        status = catalog_->addColumn(table_info.table_id, col_info, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to add column: " << ctx.message;

        // Create GIN index
        CatalogManager::IndexInfo index_info;
        index_info.index_name = index_name;
        index_info.table_id = table_info.table_id;
        index_info.index_type = static_cast<uint16_t>(CatalogManager::IndexType::GIN);
        index_info.column_ids = {col_info.column_id};

        status = catalog_->createIndex(index_info, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create GIN index: " << ctx.message;

        return table_info.table_id;
    }

    const char *test_db_path_;
    std::unique_ptr<Database> db_;
    StorageEngine *storage_;
    CatalogManager *catalog_;
    TransactionManager *txn_mgr_;
};

// Test 1: INSERT maintains GIN index
TEST_F(GinDMLTest, InsertMaintainsIndex)
{
    ErrorContext ctx;

    // Create table with GIN index
    ID table_id = createTableWithGINIndex("test_insert", "idx_gin_insert");

    // Start transaction
    uint64_t xid = txn_mgr_->beginTransaction(IsolationLevel::READ_COMMITTED, false);

    // Insert tuple with indexed value
    std::string test_value = "hello world";
    std::vector<uint8_t> tuple_data(test_value.begin(), test_value.end());

    uint32_t page_id;
    uint16_t item_id;
    Status status = storage_->insertTuple(table_id, tuple_data.data(), tuple_data.size(),
                                         &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to insert tuple: " << ctx.message;

    // Commit transaction
    txn_mgr_->commitTransaction(xid);

    // TODO: Verify that GIN index contains the extracted keys
    // This requires GIN find() API to check for key presence
}

// Test 2: DELETE removes keys from GIN index
TEST_F(GinDMLTest, DeleteRemovesKeys)
{
    ErrorContext ctx;

    // Create table with GIN index
    ID table_id = createTableWithGINIndex("test_delete", "idx_gin_delete");

    // Insert a tuple
    uint64_t xid = txn_mgr_->beginTransaction(IsolationLevel::READ_COMMITTED, false);

    std::string test_value = "test data";
    std::vector<uint8_t> tuple_data(test_value.begin(), test_value.end());

    uint32_t page_id;
    uint16_t item_id;
    Status status = storage_->insertTuple(table_id, tuple_data.data(), tuple_data.size(),
                                         &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    txn_mgr_->commitTransaction(xid);

    // Delete the tuple
    xid = txn_mgr_->beginTransaction(IsolationLevel::READ_COMMITTED, false);

    status = storage_->deleteTuple(table_id, page_id, item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to delete tuple: " << ctx.message;

    txn_mgr_->commitTransaction(xid);

    // TODO: Verify that GIN index no longer contains the keys
    // (or marks them as deleted if TASK-CRITICAL-1 is complete)
}

// Test 3: UPDATE updates GIN index
TEST_F(GinDMLTest, UpdateUpdatesIndex)
{
    ErrorContext ctx;

    // Create table with GIN index
    ID table_id = createTableWithGINIndex("test_update", "idx_gin_update");

    // Insert a tuple
    uint64_t xid = txn_mgr_->beginTransaction(IsolationLevel::READ_COMMITTED, false);

    std::string old_value = "old data";
    std::vector<uint8_t> old_tuple(old_value.begin(), old_value.end());

    uint32_t page_id;
    uint16_t item_id;
    Status status = storage_->insertTuple(table_id, old_tuple.data(), old_tuple.size(),
                                         &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    txn_mgr_->commitTransaction(xid);

    // Update the tuple
    xid = txn_mgr_->beginTransaction(IsolationLevel::READ_COMMITTED, false);

    std::string new_value = "new data";
    std::vector<uint8_t> new_tuple(new_value.begin(), new_value.end());

    uint32_t new_page_id;
    uint16_t new_item_id;
    status = storage_->updateTuple(table_id, page_id, item_id,
                                  new_tuple.data(), new_tuple.size(),
                                  &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to update tuple: " << ctx.message;

    txn_mgr_->commitTransaction(xid);

    // TODO: Verify that GIN index:
    // - Removed old keys (or marked them deleted)
    // - Added new keys
}

// Test 4: Key extractor selection
TEST_F(GinDMLTest, KeyExtractorSelection)
{
    // Test that default extractor treats value as single key
    std::string test_value = "single key";
    std::vector<uint8_t> value_bytes(test_value.begin(), test_value.end());

    auto keys = GinExtractorRegistry::defaultExtractor(value_bytes.data(), value_bytes.size());

    ASSERT_EQ(keys.size(), 1) << "Default extractor should return single key";
    ASSERT_EQ(keys[0], value_bytes) << "Key should match input value";

    // TODO: Test array extractor with actual array data
    // TODO: Test TSVECTOR extractor (when implemented)
    // TODO: Test JSONB extractor (when implemented)
}

#endif // Tests disabled

// Main function
