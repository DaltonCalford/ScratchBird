/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// PHASE 4A TASK 4A.1.6: Unit tests for BRIN Index
#include <gtest/gtest.h>
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/uuidv7.h"
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include <cstring>

using namespace scratchbird::core;

class BrinIndexTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database *db_;

    void SetUp() override
    {
        // Create temporary database for testing
        test_db_path_ = "/tmp/test_brin_" + std::to_string(getpid()) + ".db";

        // Remove if exists
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

        // Create database
        db_ = new Database();
        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        status = db_->open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
            delete db_;
            db_ = nullptr;
        }

        // Clean up test database
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    // Helper: Encode uint64_t as bytes (big-endian for correct comparison)
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

    // Helper: Decode uint64_t from bytes
    uint64_t decodeUint64(const std::vector<uint8_t> &bytes)
    {
        uint64_t value = 0;
        for (size_t i = 0; i < 8 && i < bytes.size(); ++i)
        {
            value = (value << 8) | bytes[i];
        }
        return value;
    }

    uint64_t currentXid() const
    {
        auto *tm = db_ ? db_->transaction_manager() : nullptr;
        return tm ? tm->getCurrentXid() : 1;
    }
};

// Test 1: Create BRIN index
TEST_F(BrinIndexTest, CreateIndex)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, // DataType::UINT64
                                      128,  // range_size
                                      &root_page, &ctx);

    EXPECT_EQ(status, Status::OK) << "Failed to create BRIN index: " << ctx.message;
    EXPECT_GT(root_page, 0) << "Root page should be allocated";
}

// Test 2: Open existing BRIN index
TEST_F(BrinIndexTest, OpenIndex)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    EXPECT_NE(brin, nullptr) << "Failed to open BRIN index: " << ctx.message;
}

// Test 3: Insert single value and create first range
TEST_F(BrinIndexTest, InsertSingleValue)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert value 1000 at block 0
    std::vector<uint8_t> value = encodeUint64(1000);
    status = brin->insert(value, 0, &ctx);
    EXPECT_EQ(status, Status::OK) << "Failed to insert: " << ctx.message;

    // Scan for values >= 1000
    std::vector<uint8_t> min_val = encodeUint64(1000);
    std::vector<uint32_t> blocks;
    uint64_t current_xid = currentXid();
    status = brin->scan(&min_val, nullptr, current_xid, &blocks, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(blocks.size(), 0) << "Should return blocks containing value >= 1000";
}

// Test 4: Insert multiple values in same range
TEST_F(BrinIndexTest, InsertMultipleValuesSameRange)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values in blocks 0-127 (same range)
    // Values: 100, 500, 300, 200, 400
    // Expected min: 100, max: 500
    std::vector<uint64_t> values = {100, 500, 300, 200, 400};
    for (size_t i = 0; i < values.size(); ++i)
    {
        std::vector<uint8_t> val = encodeUint64(values[i]);
        status = brin->insert(val, static_cast<uint32_t>(i), &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to insert value " << values[i];
    }

    // Scan for values between 150 and 450
    // Should match because range [100, 500] overlaps [150, 450]
    std::vector<uint8_t> min_val = encodeUint64(150);
    std::vector<uint8_t> max_val = encodeUint64(450);
    std::vector<uint32_t> blocks;
    uint64_t current_xid = currentXid();
    status = brin->scan(&min_val, &max_val, current_xid, &blocks, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(blocks.size(), 0) << "Should find blocks in range [150, 450]";

    // Scan for values > 600
    // Should NOT match because range max is 500
    min_val = encodeUint64(600);
    blocks.clear();
    status = brin->scan(&min_val, nullptr, current_xid, &blocks, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(blocks.size(), 0) << "Should not find blocks with values > 600";
}

// Test 5: Insert values across multiple ranges
TEST_F(BrinIndexTest, InsertMultipleRanges)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values in different ranges
    // Range 0 (blocks 0-127): values 100-200
    // Range 1 (blocks 128-255): values 5000-6000
    // Range 2 (blocks 256-383): values 10000-11000

    // Range 0
    for (uint32_t block = 0; block < 128; block += 10)
    {
        std::vector<uint8_t> val = encodeUint64(100 + block);
        status = brin->insert(val, block, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Range 1
    for (uint32_t block = 128; block < 256; block += 10)
    {
        std::vector<uint8_t> val = encodeUint64(5000 + (block - 128));
        status = brin->insert(val, block, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Range 2
    for (uint32_t block = 256; block < 384; block += 10)
    {
        std::vector<uint8_t> val = encodeUint64(10000 + (block - 256));
        status = brin->insert(val, block, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Scan for values in range 1 only (5000-6000)
    std::vector<uint8_t> min_val = encodeUint64(5000);
    std::vector<uint8_t> max_val = encodeUint64(6000);
    std::vector<uint32_t> blocks;
    uint64_t current_xid = currentXid();
    status = brin->scan(&min_val, &max_val, current_xid, &blocks, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Should only return blocks from range 1 (128-255)
    bool has_range_0 = false;
    bool has_range_1 = false;
    bool has_range_2 = false;
    for (uint32_t block : blocks)
    {
        if (block < 128) has_range_0 = true;
        if (block >= 128 && block < 256) has_range_1 = true;
        if (block >= 256) has_range_2 = true;
    }

    EXPECT_FALSE(has_range_0) << "Should not include range 0 (100-200)";
    EXPECT_TRUE(has_range_1) << "Should include range 1 (5000-6000)";
    EXPECT_FALSE(has_range_2) << "Should not include range 2 (10000-11000)";
}

// Test 6: Scan with no matching ranges (effective pruning)
TEST_F(BrinIndexTest, ScanNoMatch)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values 100-200 in blocks 0-127
    for (uint32_t block = 0; block < 128; block += 10)
    {
        std::vector<uint8_t> val = encodeUint64(100 + block);
        status = brin->insert(val, block, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Scan for values 500-600 (no overlap with [100, 227])
    std::vector<uint8_t> min_val = encodeUint64(500);
    std::vector<uint8_t> max_val = encodeUint64(600);
    std::vector<uint32_t> blocks;
    uint64_t current_xid = currentXid();
    status = brin->scan(&min_val, &max_val, current_xid, &blocks, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(blocks.size(), 0) << "Should prune all ranges (no match)";
}

// Test 7: Scan with null min (scan from -infinity)
TEST_F(BrinIndexTest, ScanNullMin)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values
    std::vector<uint8_t> val = encodeUint64(1000);
    status = brin->insert(val, 0, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Scan with null min (all values <= 2000)
    std::vector<uint8_t> max_val = encodeUint64(2000);
    std::vector<uint32_t> blocks;
    uint64_t current_xid = currentXid();
    status = brin->scan(nullptr, &max_val, current_xid, &blocks, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(blocks.size(), 0) << "Should match (1000 <= 2000)";
}

// Test 8: Scan with null max (scan to +infinity)
TEST_F(BrinIndexTest, ScanNullMax)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values
    std::vector<uint8_t> val = encodeUint64(5000);
    status = brin->insert(val, 0, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Scan with null max (all values >= 1000)
    std::vector<uint8_t> min_val = encodeUint64(1000);
    std::vector<uint32_t> blocks;
    uint64_t current_xid = currentXid();
    status = brin->scan(&min_val, nullptr, current_xid, &blocks, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(blocks.size(), 0) << "Should match (5000 >= 1000)";
}

// Test 9: Remove dead entries (empty list - idempotency)
TEST_F(BrinIndexTest, RemoveDeadEntriesEmpty)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Call removeDeadEntries with empty list
    std::vector<TID> dead_tids;
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = brin->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0);
    EXPECT_EQ(pages_modified, 0);
}

// Test 10: Remove dead entries (partial range - should remove)
TEST_F(BrinIndexTest, RemoveDeadEntriesPartial)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values in blocks 0-127
    std::vector<uint8_t> val = encodeUint64(1000);
    status = brin->insert(val, 0, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Mark only blocks 0-63 as dead (partial range)
    std::vector<TID> dead_tids;
    for (uint64_t block = 0; block < 64; ++block)
    {
        dead_tids.emplace_back(makeGPID(PRIMARY_TABLESPACE_ID, block), 0);
    }

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;
    status = brin->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 1) << "Should remove range (any dead block is treated as unsafe)";
}

// Test 11: Remove dead entries (complete range - should remove)
TEST_F(BrinIndexTest, RemoveDeadEntriesComplete)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values in blocks 0-127
    std::vector<uint8_t> val = encodeUint64(1000);
    status = brin->insert(val, 0, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Mark ALL blocks 0-127 as dead (complete range)
    std::vector<TID> dead_tids;
    for (uint64_t block = 0; block < 128; ++block)
    {
        dead_tids.emplace_back(makeGPID(PRIMARY_TABLESPACE_ID, block), 0);
    }

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;
    status = brin->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(entries_removed, 0) << "Should remove range (all blocks dead)";
}

// Test 12: Remove dead entries (multiple ranges)
TEST_F(BrinIndexTest, RemoveDeadEntriesMultipleRanges)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values in 3 ranges
    std::vector<uint8_t> val1 = encodeUint64(1000);
    std::vector<uint8_t> val2 = encodeUint64(2000);
    std::vector<uint8_t> val3 = encodeUint64(3000);
    status = brin->insert(val1, 0, &ctx);      // Range 0 (0-127)
    ASSERT_EQ(status, Status::OK);
    status = brin->insert(val2, 128, &ctx);    // Range 1 (128-255)
    ASSERT_EQ(status, Status::OK);
    status = brin->insert(val3, 256, &ctx);    // Range 2 (256-383)
    ASSERT_EQ(status, Status::OK);

    // Mark range 0 and range 2 as completely dead
    std::vector<TID> dead_tids;
    for (uint64_t block = 0; block < 128; ++block)
    {
        dead_tids.emplace_back(makeGPID(PRIMARY_TABLESPACE_ID, block), 0); // Range 0
    }
    for (uint64_t block = 256; block < 384; ++block)
    {
        dead_tids.emplace_back(makeGPID(PRIMARY_TABLESPACE_ID, block), 0); // Range 2
    }

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;
    status = brin->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 2) << "Should remove 2 ranges (0 and 2)";

    // Verify range 1 is still scannable
    std::vector<uint8_t> min_val = encodeUint64(1500);
    std::vector<uint8_t> max_val = encodeUint64(2500);
    std::vector<uint32_t> blocks;
    uint64_t current_xid = currentXid();
    status = brin->scan(&min_val, &max_val, current_xid, &blocks, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(blocks.size(), 0) << "Range 1 should still be accessible";
}

// Test 13: Get statistics
TEST_F(BrinIndexTest, GetStats)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values to create ranges
    for (uint32_t block = 0; block < 256; block += 50)
    {
        std::vector<uint8_t> val = encodeUint64(1000 + block);
        status = brin->insert(val, block, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    BrinIndex::BrinStats stats;
    status = brin->getStats(&stats, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(stats.total_ranges, 0) << "Should have created ranges";
}

// Test 14: Custom range size
TEST_F(BrinIndexTest, CustomRangeSize)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    // Create BRIN with range_size = 64 instead of default 128
    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 64, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Insert values in blocks 0, 64, 128 (3 different ranges)
    std::vector<uint8_t> val1 = encodeUint64(100);
    std::vector<uint8_t> val2 = encodeUint64(200);
    std::vector<uint8_t> val3 = encodeUint64(300);
    status = brin->insert(val1, 0, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = brin->insert(val2, 64, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = brin->insert(val3, 128, &ctx);
    ASSERT_EQ(status, Status::OK);

    BrinIndex::BrinStats stats;
    status = brin->getStats(&stats, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_GE(stats.total_ranges, 3) << "Should have 3 ranges with range_size=64";
}

// Test 15: Time-series workload simulation
TEST_F(BrinIndexTest, TimeSeriesWorkload)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      0x01, 128, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(brin, nullptr);

    // Simulate time-series inserts (monotonically increasing timestamps)
    // 1000 inserts, timestamp increases by 1000 each time
    const uint32_t num_inserts = 1000;
    for (uint32_t i = 0; i < num_inserts; ++i)
    {
        uint64_t timestamp = 1000000 + (i * 1000);
        std::vector<uint8_t> val = encodeUint64(timestamp);
        status = brin->insert(val, i, &ctx);
        if (status != Status::OK)
        {
            FAIL() << "Failed to insert at block " << i << ": " << ctx.message;
        }
    }

    // Query for recent data (last 100 blocks worth)
    uint64_t recent_start = 1000000 + (900 * 1000);
    std::vector<uint8_t> min_val = encodeUint64(recent_start);
    std::vector<uint32_t> blocks;
    uint64_t current_xid = currentXid();
    status = brin->scan(&min_val, nullptr, current_xid, &blocks, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(blocks.size(), 0) << "Should find recent blocks";
    if (blocks.empty())
    {
        ADD_FAILURE() << "BRIN scan returned no blocks for min=" << recent_start;
    }

    std::vector<uint32_t> sorted_blocks = blocks;
    std::sort(sorted_blocks.begin(), sorted_blocks.end());
    const uint32_t min_block = sorted_blocks.front();
    const uint32_t max_block = sorted_blocks.back();
    SCOPED_TRACE(::testing::Message()
                 << "BRIN scan stats: count=" << sorted_blocks.size()
                 << " min_block=" << min_block
                 << " max_block=" << max_block);

    // Should prune old blocks (blocks < 900 should be pruned)
    bool found_old_block = false;
    for (uint32_t block : blocks)
    {
        if (block < 900)
        {
            found_old_block = true;
            break;
        }
    }
    if (found_old_block)
    {
        std::ostringstream sample;
        const size_t limit = std::min<size_t>(sorted_blocks.size(), 10);
        for (size_t i = 0; i < limit; ++i)
        {
            if (i > 0)
            {
                sample << ", ";
            }
            sample << sorted_blocks[i];
        }
        ADD_FAILURE() << "Found old block <900 in BRIN scan. "
                      << "min_block=" << min_block
                      << " max_block=" << max_block
                      << " sample=[" << sample.str() << "]";
    }
    EXPECT_FALSE(found_old_block) << "Should prune old blocks effectively";
}
