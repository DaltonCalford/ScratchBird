/**
 * Plan 01 Task D.3: BRIN GC Tests
 *
 * Tests for removeDeadEntries() per plan_01_task_d_index_gc_checklist.md
 *
 * NOTE: The specification requires rescanning heap blocks to recompute min/max,
 * but BRIN doesn't have heap access in current architecture. This implementation
 * uses a conservative approach: mark affected ranges as DELETED.
 *
 * Test coverage:
 * 1. Unit test: Mark ranges containing dead blocks as deleted
 * 2. Restart test: Deleted ranges persist across restart
 * 3. Negative test: Empty dead_tids is no-op
 * 4. Unaffected ranges test: Ranges outside dead blocks not deleted
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/types.h"
#include <filesystem>
#include <memory>

using namespace scratchbird::core;

namespace {

class BrinGCTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary database
        test_db_path_ = "/tmp/test_brin_gc.db";
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

        // Create and open database
        db_ = std::make_unique<Database>();
        Status status = db_->create(test_db_path_, 16384, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to create database";

        status = db_->open(test_db_path_, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to open database";
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
            db_.reset();
        }

        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    std::unique_ptr<Database> db_;
    std::string test_db_path_;
};

/**
 * Test 1: Unit test - Ranges containing dead blocks are marked deleted
 *
 * This test verifies that BRIN ranges covering blocks with dead tuples
 * are conservatively marked as DELETED.
 */
TEST_F(BrinGCTest, RangesWithDeadBlocksMarkedDeleted)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    // Create BRIN index with range_size=128 blocks
    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid,
                                     {column_uuid},
                                     static_cast<uint8_t>(DataType::INT32),
                                     128, &root_page, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to create BRIN index";
    ASSERT_GT(root_page, 0);

    // Open index
    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, nullptr);
    ASSERT_NE(brin, nullptr) << "Failed to open BRIN index";

    // Insert values at different block numbers to create ranges
    // Range 0: blocks 0-127
    // Range 1: blocks 128-255
    // Range 2: blocks 256-383

    // Insert in range 0 (block 10)
    std::vector<uint8_t> value1 = {0, 0, 0, 100};  // Integer 100
    status = brin->insert(value1, 10, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Insert in range 1 (block 150)
    std::vector<uint8_t> value2 = {0, 0, 0, 200};  // Integer 200
    status = brin->insert(value2, 150, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Insert in range 2 (block 300)
    std::vector<uint8_t> value3 = {0, 0, 1, 44};  // Integer 300
    status = brin->insert(value3, 300, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Flush to ensure persistence
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);

    // Create dead TIDs in blocks 10 and 150 (ranges 0 and 1)
    std::vector<TID> dead_tids = {
        TID(0, 10, 0),   // Block 10 in range 0
        TID(0, 150, 0)   // Block 150 in range 1
    };

    // Run GC
    uint64_t ranges_removed = 0;
    uint64_t pages_modified = 0;
    status = brin->removeDeadEntries(dead_tids, &ranges_removed, &pages_modified, nullptr);
    ASSERT_EQ(status, Status::OK) << "GC failed";

    // Verify that 2 ranges were marked deleted (ranges 0 and 1)
    EXPECT_EQ(ranges_removed, 2) << "Should have marked 2 ranges as deleted";
    EXPECT_GT(pages_modified, 0) << "Should have modified at least one page";

    // Flush to ensure persistence
    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK);
}

/**
 * Test 2: Restart test - Deleted ranges persist across restart
 *
 * This test verifies that range deletions persist after database restart.
 */
TEST_F(BrinGCTest, DeletedRangesPersistAcrossRestart)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    uint32_t root_page = 0;

    // Phase 1: Create index, insert, and run GC
    {
        Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid,
                                         {column_uuid},
                                         static_cast<uint8_t>(DataType::INT32),
                                         128, &root_page, nullptr);
        ASSERT_EQ(status, Status::OK);

        auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, nullptr);
        ASSERT_NE(brin, nullptr);

        // Insert values
        std::vector<uint8_t> value1 = {0, 0, 0, 50};
        status = brin->insert(value1, 20, nullptr);
        ASSERT_EQ(status, Status::OK);

        std::vector<uint8_t> value2 = {0, 0, 0, 75};
        status = brin->insert(value2, 40, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Mark block 20 as dead
        std::vector<TID> dead_tids = {TID(0, 20, 0)};

        uint64_t ranges_removed = 0;
        status = brin->removeDeadEntries(dead_tids, &ranges_removed, nullptr, nullptr);
        ASSERT_EQ(status, Status::OK);
        EXPECT_EQ(ranges_removed, 1) << "Should have marked 1 range as deleted";

        // Flush to disk
        status = db_->buffer_pool()->flushAll(nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    // Phase 2: Close and reopen database
    db_->close();
    Status status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reopen database";

    // Phase 3: Reopen index and verify GC persisted
    {
        auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, nullptr);
        ASSERT_NE(brin, nullptr) << "Failed to reopen BRIN index after restart";

        // Running GC again with same dead TID should find it already deleted
        std::vector<TID> dead_tids = {TID(0, 20, 0)};

        uint64_t ranges_removed = 0;
        status = brin->removeDeadEntries(dead_tids, &ranges_removed, nullptr, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Should be 0 because range was already marked deleted in Phase 1
        EXPECT_EQ(ranges_removed, 0) << "Range should already be deleted from Phase 1";
    }
}

/**
 * Test 3: Negative test - Empty dead_tids is no-op
 *
 * This test verifies that passing empty dead_tids returns OK
 * without modifying anything.
 */
TEST_F(BrinGCTest, EmptyDeadTidsIsNoOp)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    // Create BRIN index
    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid,
                                     {column_uuid},
                                     static_cast<uint8_t>(DataType::INT32),
                                     128, &root_page, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, nullptr);
    ASSERT_NE(brin, nullptr);

    // Insert one value
    std::vector<uint8_t> value = {0, 0, 0, 42};
    status = brin->insert(value, 10, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Run GC with empty dead_tids
    std::vector<TID> empty_dead_tids;
    uint64_t ranges_removed = 0;
    uint64_t pages_modified = 0;

    status = brin->removeDeadEntries(empty_dead_tids, &ranges_removed, &pages_modified, nullptr);
    ASSERT_EQ(status, Status::OK) << "Empty dead_tids should return OK";

    // Verify counters are zero
    EXPECT_EQ(ranges_removed, 0) << "No ranges should be removed";
    EXPECT_EQ(pages_modified, 0) << "No pages should be modified";
}

/**
 * Test 4: Unaffected ranges test - Ranges outside dead blocks not deleted
 *
 * This test verifies that only ranges containing dead blocks are marked,
 * and other ranges remain untouched.
 */
TEST_F(BrinGCTest, UnaffectedRangesNotDeleted)
{
    // Generate unique UUIDs
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    // Create BRIN index
    uint32_t root_page = 0;
    Status status = BrinIndex::create(db_.get(), index_uuid, table_uuid,
                                     {column_uuid},
                                     static_cast<uint8_t>(DataType::INT32),
                                     128, &root_page, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto brin = BrinIndex::open(db_.get(), index_uuid, root_page, nullptr);
    ASSERT_NE(brin, nullptr);

    // Insert values in 3 different ranges
    // Range 0: blocks 0-127
    // Range 1: blocks 128-255
    // Range 2: blocks 256-383

    std::vector<uint8_t> value1 = {0, 0, 0, 10};
    status = brin->insert(value1, 50, nullptr);    // Range 0
    ASSERT_EQ(status, Status::OK);

    std::vector<uint8_t> value2 = {0, 0, 0, 20};
    status = brin->insert(value2, 180, nullptr);   // Range 1
    ASSERT_EQ(status, Status::OK);

    std::vector<uint8_t> value3 = {0, 0, 0, 30};
    status = brin->insert(value3, 300, nullptr);   // Range 2
    ASSERT_EQ(status, Status::OK);

    // Mark only block 180 (range 1) as dead
    std::vector<TID> dead_tids = {TID(0, 180, 0)};

    uint64_t ranges_removed = 0;
    status = brin->removeDeadEntries(dead_tids, &ranges_removed, nullptr, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Only range 1 should be marked deleted
    EXPECT_EQ(ranges_removed, 1) << "Only 1 range should be marked deleted";

    // Running GC again on ranges 0 and 2 should mark those
    std::vector<TID> more_dead_tids = {
        TID(0, 50, 0),   // Range 0
        TID(0, 300, 0)   // Range 2
    };

    ranges_removed = 0;
    status = brin->removeDeadEntries(more_dead_tids, &ranges_removed, nullptr, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Should mark 2 more ranges
    EXPECT_EQ(ranges_removed, 2) << "2 additional ranges should be marked deleted";
}

} // anonymous namespace
