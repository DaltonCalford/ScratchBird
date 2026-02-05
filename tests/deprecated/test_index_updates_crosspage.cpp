/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developers-public-license-version-1-0/
 */
#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/connection_context.h"
#include "test_helpers.h"
#include <filesystem>
#include <cstring>

using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

/**
 * Test suite for index entry updates during cross-page tuple relocations
 *
 * These tests verify that when a tuple update causes a cross-page relocation,
 * all associated indexes (BTree and Hash) are properly updated to point to
 * the new tuple location.
 */
class IndexUpdatesCrossPageTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ErrorContext ctx;

        // Create database
        ASSERT_EQ(Database::create(db_file_.path(), 8192, &ctx), Status::OK)
            << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_.path(), &ctx), Status::OK)
            << "Failed to open database: " << ctx.message;

        // Initialize catalog
        ASSERT_EQ(db_->catalog_manager()->initialize(&ctx), Status::OK)
            << "Failed to initialize catalog: " << ctx.message;

        // Create test schema
        ASSERT_EQ(db_->catalog_manager()->createSchema("test_schema", "test_user", schema_id_, &ctx),
                  Status::OK)
            << "Failed to create schema: " << ctx.message;

        // Create test table with columns
        std::vector<CatalogManager::ColumnInfo> columns;

        CatalogManager::ColumnInfo id_col;
        id_col.column_id = generateUuidV7();
        id_col.column_name = "id";
        id_col.ordinal = 0;
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.nullable = false;
        id_col.table_id = ID{}; // Will be set by createTable
        columns.push_back(id_col);

        CatalogManager::ColumnInfo data_col;
        data_col.column_id = generateUuidV7();
        data_col.column_name = "data";
        data_col.ordinal = 1;
        data_col.data_type = static_cast<uint16_t>(DataType::VARCHAR);
        data_col.type_precision = 1000; // Large column to trigger cross-page updates
        data_col.nullable = true;
        data_col.table_id = ID{}; // Will be set by createTable
        columns.push_back(data_col);

        ASSERT_EQ(db_->catalog_manager()->createTable(schema_id_, "test_table", columns, table_id_, 0, &ctx),
                  Status::OK)
            << "Failed to create table: " << ctx.message;

        // Save column IDs for later use
        std::vector<CatalogManager::ColumnInfo> saved_columns;
        ASSERT_EQ(db_->catalog_manager()->getColumns(table_id_, saved_columns, &ctx), Status::OK);
        ASSERT_EQ(saved_columns.size(), 2u);
        id_column_id_ = saved_columns[0].column_id;
        data_column_id_ = saved_columns[1].column_id;

        // Set up connection context for transaction support
        conn_ctx_.setDatabase(db_.get());
        conn_ctx_.setConnectionId(1);
        conn_ctx_.setProcId(1);
        conn_ctx_.setTransactionId(100);
        ConnectionContext::setCurrent(&conn_ctx_);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);

        if (db_)
        {
            db_->close();
        }
        db_.reset();
    }

    // Helper to create a simple tuple with id and data fields
    std::vector<uint8_t> makeTuple(uint32_t id, const std::string &data)
    {
        std::vector<uint8_t> tuple;

        // Simple layout: just concatenate the id and data
        // In a real implementation, this would follow proper tuple layout
        tuple.resize(sizeof(uint32_t) + data.size());
        std::memcpy(tuple.data(), &id, sizeof(uint32_t));
        std::memcpy(tuple.data() + sizeof(uint32_t), data.data(), data.size());

        return tuple;
    }

    // Helper to extract id from tuple
    uint32_t extractId(const std::vector<uint8_t> &tuple)
    {
        if (tuple.size() < sizeof(uint32_t))
        {
            return 0;
        }
        return *reinterpret_cast<const uint32_t *>(tuple.data());
    }

    TestDatabaseFile db_file_{"test_index_updates_crosspage"};
    std::unique_ptr<Database> db_;
    ConnectionContext conn_ctx_;
    ID schema_id_;
    ID table_id_;
    ID id_column_id_;
    ID data_column_id_;
};

// Test 1: BTree index is updated during cross-page tuple relocation
TEST_F(IndexUpdatesCrossPageTest, BTreeIndexUpdateOnCrossPageRelocation)
{
    ErrorContext ctx;

    // Create a BTree index on the id column
    ID index_id;
    Status status = db_->catalog_manager()->createIndex(
        table_id_, "idx_id", {"id"}, index_id, false,
        CatalogManager::IndexType::BTREE, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    // Get index info
    CatalogManager::IndexInfo index_info;
    status = db_->catalog_manager()->getIndex(index_id, index_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Open the BTree index
    auto btree = BTree::open(db_.get(), index_info.index_id, index_info.root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert a tuple
    uint32_t initial_page_id, initial_item_id;
    std::vector<uint8_t> tuple1 = makeTuple(100, "initial data");
    status = db_->storage_engine()->insertTuple(table_id_, tuple1.data(), tuple1.size(),
                                                &initial_page_id, &initial_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Manually add entry to index (simulating what would happen in a real INSERT)
    uint64_t initial_tid = (static_cast<uint64_t>(initial_page_id) << 32) |
                           (static_cast<uint64_t>(initial_item_id) << 16);
    std::vector<uint8_t> key1(sizeof(uint32_t));
    std::memcpy(key1.data(), &tuple1[0], sizeof(uint32_t));
    status = btree->insert(key1, initial_tid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify index entry exists
    std::vector<uint64_t> tuple_ids;
    status = btree->search(key1, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(tuple_ids.size(), 1u);
    ASSERT_EQ(tuple_ids[0], initial_tid);

    // Update the tuple with large data to force cross-page relocation
    uint32_t new_page_id, new_item_id;
    std::string large_data(6000, 'X'); // Large enough to not fit on same page
    std::vector<uint8_t> tuple2 = makeTuple(100, large_data); // Same ID
    status = db_->storage_engine()->updateTuple(table_id_, initial_page_id, initial_item_id,
                                                tuple2.data(), tuple2.size(),
                                                &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify that cross-page relocation occurred
    ASSERT_NE(new_page_id, initial_page_id) << "Expected cross-page relocation";

    // Verify index now points to new location
    uint64_t new_tid = (static_cast<uint64_t>(new_page_id) << 32) |
                       (static_cast<uint64_t>(new_item_id) << 16);

    tuple_ids.clear();
    status = btree->search(key1, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(tuple_ids.size(), 1u);
    ASSERT_EQ(tuple_ids[0], new_tid) << "Index should point to new tuple location";

    // Verify old location is not in index
    ASSERT_NE(tuple_ids[0], initial_tid) << "Index should not contain old TID";
}

// Test 2: Hash index is updated during cross-page tuple relocation
TEST_F(IndexUpdatesCrossPageTest, HashIndexUpdateOnCrossPageRelocation)
{
    ErrorContext ctx;

    // Create a Hash index on the id column
    ID index_id;
    Status status = db_->catalog_manager()->createIndex(
        table_id_, "idx_id_hash", {"id"}, index_id, false,
        CatalogManager::IndexType::HASH, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    // Get index info
    CatalogManager::IndexInfo index_info;
    status = db_->catalog_manager()->getIndex(index_id, index_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Open the Hash index
    auto hash_index = HashIndex::open(db_.get(), index_info.index_id, index_info.root_page, &ctx);
    ASSERT_NE(hash_index, nullptr);

    // Insert a tuple
    uint32_t initial_page_id, initial_item_id;
    std::vector<uint8_t> tuple1 = makeTuple(200, "initial data");
    status = db_->storage_engine()->insertTuple(table_id_, tuple1.data(), tuple1.size(),
                                                &initial_page_id, &initial_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Manually add entry to hash index
    uint64_t initial_tid = (static_cast<uint64_t>(initial_page_id) << 32) |
                           (static_cast<uint64_t>(initial_item_id) << 16);
    uint32_t id_value = 200;
    status = hash_index->insert(&id_value, sizeof(uint32_t), initial_tid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify index entry exists
    std::vector<uint64_t> tuple_ids = hash_index->find(&id_value, sizeof(uint32_t), &ctx);
    ASSERT_EQ(tuple_ids.size(), 1u);
    ASSERT_EQ(tuple_ids[0], initial_tid);

    // Update the tuple with large data to force cross-page relocation
    uint32_t new_page_id, new_item_id;
    std::string large_data(6000, 'Y');
    std::vector<uint8_t> tuple2 = makeTuple(200, large_data); // Same ID
    status = db_->storage_engine()->updateTuple(table_id_, initial_page_id, initial_item_id,
                                                tuple2.data(), tuple2.size(),
                                                &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify that cross-page relocation occurred
    ASSERT_NE(new_page_id, initial_page_id) << "Expected cross-page relocation";

    // Verify index now points to new location
    uint64_t new_tid = (static_cast<uint64_t>(new_page_id) << 32) |
                       (static_cast<uint64_t>(new_item_id) << 16);

    tuple_ids = hash_index->find(&id_value, sizeof(uint32_t), &ctx);
    ASSERT_EQ(tuple_ids.size(), 1u);
    ASSERT_EQ(tuple_ids[0], new_tid) << "Hash index should point to new tuple location";
}

// Test 3: Multiple indexes are all updated during cross-page relocation
TEST_F(IndexUpdatesCrossPageTest, MultipleIndexesUpdatedOnCrossPageRelocation)
{
    ErrorContext ctx;

    // Create both BTree and Hash indexes
    ID btree_index_id, hash_index_id;
    Status status = db_->catalog_manager()->createIndex(
        table_id_, "idx_btree", {"id"}, btree_index_id, false,
        CatalogManager::IndexType::BTREE, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = db_->catalog_manager()->createIndex(
        table_id_, "idx_hash", {"id"}, hash_index_id, false,
        CatalogManager::IndexType::HASH, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Get index info
    CatalogManager::IndexInfo btree_info, hash_info;
    status = db_->catalog_manager()->getIndex(btree_index_id, btree_info, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = db_->catalog_manager()->getIndex(hash_index_id, hash_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Open indexes
    auto btree = BTree::open(db_.get(), btree_info.index_id, btree_info.root_page, &ctx);
    ASSERT_NE(btree, nullptr);
    auto hash_index = HashIndex::open(db_.get(), hash_info.index_id, hash_info.root_page, &ctx);
    ASSERT_NE(hash_index, nullptr);

    // Insert tuple
    uint32_t initial_page_id, initial_item_id;
    std::vector<uint8_t> tuple1 = makeTuple(300, "data");
    status = db_->storage_engine()->insertTuple(table_id_, tuple1.data(), tuple1.size(),
                                                &initial_page_id, &initial_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    uint64_t initial_tid = (static_cast<uint64_t>(initial_page_id) << 32) |
                           (static_cast<uint64_t>(initial_item_id) << 16);

    // Add to both indexes
    std::vector<uint8_t> key(sizeof(uint32_t));
    std::memcpy(key.data(), tuple1.data(), sizeof(uint32_t));
    status = btree->insert(key, initial_tid, &ctx);
    ASSERT_EQ(status, Status::OK);

    uint32_t id_value = 300;
    status = hash_index->insert(&id_value, sizeof(uint32_t), initial_tid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Update to force cross-page relocation
    uint32_t new_page_id, new_item_id;
    std::string large_data(6000, 'Z');
    std::vector<uint8_t> tuple2 = makeTuple(300, large_data);
    status = db_->storage_engine()->updateTuple(table_id_, initial_page_id, initial_item_id,
                                                tuple2.data(), tuple2.size(),
                                                &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_NE(new_page_id, initial_page_id);

    uint64_t new_tid = (static_cast<uint64_t>(new_page_id) << 32) |
                       (static_cast<uint64_t>(new_item_id) << 16);

    // Verify both indexes are updated
    std::vector<uint64_t> btree_tids;
    status = btree->search(key, &btree_tids, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(btree_tids.size(), 1u);
    ASSERT_EQ(btree_tids[0], new_tid) << "BTree index should point to new location";

    std::vector<uint64_t> hash_tids = hash_index->find(&id_value, sizeof(uint32_t), &ctx);
    ASSERT_EQ(hash_tids.size(), 1u);
    ASSERT_EQ(hash_tids[0], new_tid) << "Hash index should point to new location";
}

// Test 4: Index updates work correctly when table has no indexes
TEST_F(IndexUpdatesCrossPageTest, CrossPageRelocationWithNoIndexes)
{
    ErrorContext ctx;

    // No indexes created - just verify update works

    // Insert tuple
    uint32_t initial_page_id, initial_item_id;
    std::vector<uint8_t> tuple1 = makeTuple(400, "data");
    Status status = db_->storage_engine()->insertTuple(table_id_, tuple1.data(), tuple1.size(),
                                                       &initial_page_id, &initial_item_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Update to force cross-page relocation
    uint32_t new_page_id, new_item_id;
    std::string large_data(6000, 'W');
    std::vector<uint8_t> tuple2 = makeTuple(400, large_data);
    status = db_->storage_engine()->updateTuple(table_id_, initial_page_id, initial_item_id,
                                                tuple2.data(), tuple2.size(),
                                                &new_page_id, &new_item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_NE(new_page_id, initial_page_id) << "Expected cross-page relocation";

    // Success - no indexes to update, but operation should still work
}
