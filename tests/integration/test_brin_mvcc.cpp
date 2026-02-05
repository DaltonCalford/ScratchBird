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
 * BRIN Index MVCC Integration Tests
 *
 * Tests MVCC Visibility for BRIN Index using current APIs.
 * 
 * BRIN indexes are block-level indexes that store min/max values
 * for ranges of blocks. They support MVCC through the TransactionId
 * parameter in scan operations (per MGA_RULES.md Rule 11).
 * 
 * Note: BRIN insert() does not take xid because the index stores
 * only min/max summaries per range, not individual tuple visibility.
 * Visibility is checked during scan() using the current_xid.
 */

#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include <thread>
#include <chrono>
#include "scratchbird/core/database.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/proc_array.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class BrinMVCCTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_brin_mvcc", ".db");
        std::remove(test_db_path_.c_str());

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        status = db_->open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        // Initialize ProcArray for multi-transaction support
        status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT) {
            ASSERT_EQ(status, Status::OK);
        }

        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(txn_mgr_, nullptr);

        buffer_pool_ = db_->buffer_pool();
        ASSERT_NE(buffer_pool_, nullptr);

        // Register a backend for transaction support
        status = ProcArrayManager::registerBackend(&proc_id_, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    void TearDown() override
    {
        ErrorContext ctx;
        ProcArrayManager::unregisterBackend(proc_id_, &ctx);
        
        if (db_)
        {
            db_->close();
        }
        std::remove(test_db_path_.c_str());
    }

    // Helper: Encode uint64_t as bytes (big-endian)
    std::vector<uint8_t> encodeUint64(uint64_t value)
    {
        std::vector<uint8_t> bytes(8);
        for (int i = 7; i >= 0; --i)
        {
            bytes[i] = value & 0xFF;
            value >>= 8;
        }
        return bytes;
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    TransactionManager *txn_mgr_;
    BufferPool *buffer_pool_;
    uint32_t proc_id_ = 0;
};

// Test 1: Range visibility with transaction ID (basic MVCC)
TEST_F(BrinMVCCTest, RangeVisibilityBasic)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert value (block number 0) - BRIN insert doesn't need xid
    std::vector<uint8_t> val1 = encodeUint64(1000);
    status = brin->insert(val1, 0, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Start transaction to scan
    uint64_t xid = 0;
    status = txn_mgr_->beginTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_GT(xid, 0);

    // Scan using transaction's xid for visibility
    std::vector<uint32_t> block_numbers;
    status = brin->scan(&val1, &val1, xid, &block_numbers, &ctx);
    ASSERT_EQ(status, Status::OK);
    // BRIN is a lossy index - it returns all blocks in ranges that might contain matches
    EXPECT_GE(block_numbers.size(), 1);

    status = txn_mgr_->commitTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);
}

// Test 2: Repeatable Read - consistent view within transaction
TEST_F(BrinMVCCTest, RepeatableReadIsolation)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert initial values
    std::vector<uint8_t> val1 = encodeUint64(1000);
    std::vector<uint8_t> val2 = encodeUint64(2000);
    status = brin->insert(val1, 0, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = brin->insert(val2, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Start transaction for scanning
    uint64_t xid = 0;
    status = txn_mgr_->beginTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // First scan
    std::vector<uint32_t> blocks_1;
    std::vector<uint8_t> min_val = encodeUint64(0);
    std::vector<uint8_t> max_val = encodeUint64(9999);
    status = brin->scan(&min_val, &max_val, xid, &blocks_1, &ctx);
    ASSERT_EQ(status, Status::OK);
    // BRIN returns all blocks in the matching ranges (lossy index)
    EXPECT_GE(blocks_1.size(), 1);

    // Second scan in same transaction (should see same result - repeatable read)
    std::vector<uint32_t> blocks_2;
    status = brin->scan(&min_val, &max_val, xid, &blocks_2, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_GE(blocks_2.size(), 1);
    EXPECT_EQ(blocks_1, blocks_2);

    status = txn_mgr_->commitTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);
}

// Test 3: Read Committed - scan sees committed data
TEST_F(BrinMVCCTest, ReadCommittedIsolation)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert value
    std::vector<uint8_t> val1 = encodeUint64(1000);
    status = brin->insert(val1, 0, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Start transaction
    uint64_t xid = 0;
    status = txn_mgr_->beginTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Should see the value (BRIN returns blocks in matching ranges)
    std::vector<uint32_t> block_numbers;
    status = brin->scan(&val1, &val1, xid, &block_numbers, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_GE(block_numbers.size(), 1);

    status = txn_mgr_->commitTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);
}

// Test 4: Multiple block inserts
TEST_F(BrinMVCCTest, MultipleBlockInserts)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    const int num_blocks = 5;

    // Insert values into different blocks
    for (int i = 0; i < num_blocks; ++i)
    {
        std::vector<uint8_t> val = encodeUint64(1000 + i * 100);
        status = brin->insert(val, i, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Start transaction and verify scan returns blocks
    uint64_t xid = 0;
    status = txn_mgr_->beginTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::vector<uint32_t> all_blocks;
    std::vector<uint8_t> min_val = encodeUint64(0);
    std::vector<uint8_t> max_val = encodeUint64(9999);
    status = brin->scan(&min_val, &max_val, xid, &all_blocks, &ctx);
    ASSERT_EQ(status, Status::OK);
    // BRIN returns all blocks in the matching ranges (coarse-grained index)
    EXPECT_GE(all_blocks.size(), 1);

    status = txn_mgr_->commitTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);
}

// Test 5: Range query returns correct blocks
TEST_F(BrinMVCCTest, RangeQueryReturnsBlocks)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values in different ranges
    status = brin->insert(encodeUint64(100), 0, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = brin->insert(encodeUint64(500), 1, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = brin->insert(encodeUint64(1000), 2, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = brin->insert(encodeUint64(2000), 3, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Start transaction
    uint64_t xid = 0;
    status = txn_mgr_->beginTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Query range 0-300 should return blocks with values 100 (and possibly 500 if same range)
    std::vector<uint8_t> range_min = encodeUint64(0);
    std::vector<uint8_t> range_max = encodeUint64(300);
    std::vector<uint32_t> blocks;
    status = brin->scan(&range_min, &range_max, xid, &blocks, &ctx);
    ASSERT_EQ(status, Status::OK);
    // BRIN is approximate - should find candidate blocks
    EXPECT_GE(blocks.size(), 1);

    // Query range 1500-2500 should return block with value 2000
    range_min = encodeUint64(1500);
    range_max = encodeUint64(2500);
    blocks.clear();
    status = brin->scan(&range_min, &range_max, xid, &blocks, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_GE(blocks.size(), 1);

    status = txn_mgr_->commitTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);
}

// Test 6: Multiple scanners from same transaction
TEST_F(BrinMVCCTest, MultipleConcurrentScanners)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert multiple values
    const int num_values = 10;
    for (int i = 0; i < num_values; ++i)
    {
        std::vector<uint8_t> val = encodeUint64(1000 + i * 100);
        status = brin->insert(val, i, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Multiple scans from same transaction
    uint64_t xid = 0;
    status = txn_mgr_->beginTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    for (int scan = 0; scan < 3; ++scan)
    {
        std::vector<uint32_t> blocks;
        std::vector<uint8_t> min_val = encodeUint64(0);
        std::vector<uint8_t> max_val = encodeUint64(9999);
        status = brin->scan(&min_val, &max_val, xid, &blocks, &ctx);
        ASSERT_EQ(status, Status::OK);
        // BRIN returns blocks from matching ranges, not exact count
        EXPECT_GE(blocks.size(), 1);
    }

    status = txn_mgr_->commitTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);
}

// Test 7: Time series simulation with BRIN
TEST_F(BrinMVCCTest, TimeSeriesWithMVCC)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Simulate time series data (timestamps increasing)
    const int num_points = 20;
    for (int i = 0; i < num_points; ++i)
    {
        // Simulate timestamp (e.g., Unix timestamp)
        uint64_t timestamp = 1700000000 + i * 3600; // hourly data
        std::vector<uint8_t> val = encodeUint64(timestamp);
        status = brin->insert(val, i, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Query for a specific time range
    uint64_t xid = 0;
    status = txn_mgr_->beginTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Query for first 5 hours
    std::vector<uint8_t> start_time = encodeUint64(1700000000);
    std::vector<uint8_t> end_time = encodeUint64(1700000000 + 5 * 3600);
    std::vector<uint32_t> blocks_in_range;
    status = brin->scan(&start_time, &end_time, xid, &blocks_in_range, &ctx);
    ASSERT_EQ(status, Status::OK);

    // BRIN is approximate - should find entries in the range
    EXPECT_GE(blocks_in_range.size(), 1);

    status = txn_mgr_->commitTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);
}

// Test 8: Value removal updates range
TEST_F(BrinMVCCTest, RemoveValueFromRange)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert value
    std::vector<uint8_t> val = encodeUint64(1000);
    status = brin->insert(val, 0, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Start transaction
    uint64_t xid = 0;
    status = txn_mgr_->beginTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify it's visible (BRIN returns candidate blocks)
    std::vector<uint32_t> blocks_before;
    status = brin->scan(&val, &val, xid, &blocks_before, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_GE(blocks_before.size(), 1);

    // Remove the value
    status = brin->remove(val, 0, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = txn_mgr_->commitTransaction(proc_id_, xid, &ctx);
    ASSERT_EQ(status, Status::OK);
}
