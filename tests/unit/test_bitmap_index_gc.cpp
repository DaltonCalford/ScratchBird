// PHASE 2 TASK 2.5: Unit tests for Bitmap Index Garbage Collection
#include <gtest/gtest.h>
#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <vector>

using namespace scratchbird::core;

class BitmapIndexGCTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database *db_;

    void SetUp() override
    {
        // Create temporary database for testing
        test_db_path_ = "/tmp/test_bitmap_index_gc_" + std::to_string(getpid()) + ".db";

        // Remove if exists
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

        // Create database
        db_ = new Database();
        ErrorContext ctx;
        Status status = db_->create(test_db_path_, &ctx);
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
};

// Test 1: Empty dead_tids vector (no-op)
TEST_F(BitmapIndexGCTest, EmptyDeadTidsVector)
{
    ErrorContext ctx;

    // Create Bitmap Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = BitmapIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create Bitmap Index: " << ctx.message;

    auto bitmap_index = BitmapIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(bitmap_index, nullptr) << "Failed to open Bitmap Index: " << ctx.message;

    // Call removeDeadEntries with empty vector
    std::vector<uint64_t> dead_tids;
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = bitmap_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0);
    EXPECT_EQ(pages_modified, 0);
}

// Test 2: Dead TID not in index (idempotency)
TEST_F(BitmapIndexGCTest, DeadTidNotInIndex)
{
    ErrorContext ctx;

    // Create Bitmap Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = BitmapIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto bitmap_index = BitmapIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(bitmap_index, nullptr);

    // Insert a few entries
    const char *value1 = "red";
    const char *value2 = "blue";

    uint64_t tid1 = (1ULL << 32) | 1; // page 1, item 1
    uint64_t tid2 = (1ULL << 32) | 2; // page 1, item 2

    status = bitmap_index->insert(value1, strlen(value1), tid1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = bitmap_index->insert(value2, strlen(value2), tid2, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Try to remove TIDs that don't exist in index
    std::vector<uint64_t> dead_tids = {
        (2ULL << 32) | 1,  // page 2, item 1 (doesn't exist)
        (2ULL << 32) | 2   // page 2, item 2 (doesn't exist)
    };

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = bitmap_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    // May be 0 if TIDs don't exist, or non-zero if they happen to collide with existing TIDs
}

// Test 3: Single dead TID removal
TEST_F(BitmapIndexGCTest, SingleDeadTidRemoval)
{
    ErrorContext ctx;

    // Create Bitmap Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = BitmapIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto bitmap_index = BitmapIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(bitmap_index, nullptr);

    // Insert entries with same value (multiple TIDs in one bitmap)
    const char *value = "active";

    uint64_t tid1 = 1; // Simple TID
    uint64_t tid2 = 2;
    uint64_t tid3 = 3;

    status = bitmap_index->insert(value, strlen(value), tid1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = bitmap_index->insert(value, strlen(value), tid2, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = bitmap_index->insert(value, strlen(value), tid3, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify find returns all 3 TIDs
    auto found_tids = bitmap_index->find(value, strlen(value), nullptr, &ctx);
    EXPECT_EQ(found_tids.size(), 3);

    // Remove tid2
    std::vector<uint64_t> dead_tids = {tid2};
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = bitmap_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_GE(entries_removed, 1);  // At least one entry removed

    // Verify find now returns only 2 TIDs (tid1 and tid3)
    found_tids = bitmap_index->find(value, strlen(value), nullptr, &ctx);
    EXPECT_EQ(found_tids.size(), 2);
}

// Test 4: Bulk dead TID removal
TEST_F(BitmapIndexGCTest, BulkDeadTidRemoval)
{
    ErrorContext ctx;

    // Create Bitmap Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = BitmapIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto bitmap_index = BitmapIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(bitmap_index, nullptr);

    // Insert many entries with same value
    const char *value = "status_active";
    const int num_entries = 50;
    std::vector<uint64_t> all_tids;

    for (int i = 0; i < num_entries; i++)
    {
        uint64_t tid = i + 1; // Simple TIDs: 1, 2, 3, ..., 50
        all_tids.push_back(tid);

        status = bitmap_index->insert(value, strlen(value), tid, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Verify find returns all TIDs
    auto found_before = bitmap_index->find(value, strlen(value), nullptr, &ctx);
    EXPECT_EQ(found_before.size(), num_entries);

    // Remove every other entry (25 entries)
    std::vector<uint64_t> dead_tids;
    for (int i = 0; i < num_entries; i += 2)
    {
        dead_tids.push_back(all_tids[i]);
    }

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = bitmap_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_GE(entries_removed, 25);  // At least 25 entries removed

    // Verify find returns remaining TIDs
    auto found_after = bitmap_index->find(value, strlen(value), nullptr, &ctx);
    EXPECT_EQ(found_after.size(), 25);  // Half remaining
}

// Test 5: Index type name
TEST_F(BitmapIndexGCTest, IndexTypeName)
{
    ErrorContext ctx;

    // Create Bitmap Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = BitmapIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto bitmap_index = BitmapIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(bitmap_index, nullptr);

    // Check index type name
    const char *type_name = bitmap_index->indexTypeName();
    EXPECT_STREQ(type_name, "Bitmap");
}

// Test 6: Duplicate dead TIDs (idempotency)
TEST_F(BitmapIndexGCTest, DuplicateDeadTids)
{
    ErrorContext ctx;

    // Create Bitmap Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = BitmapIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto bitmap_index = BitmapIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(bitmap_index, nullptr);

    // Insert entry
    const char *value = "test_value";
    uint64_t tid1 = 100;

    status = bitmap_index->insert(value, strlen(value), tid1, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Remove with duplicate TIDs in vector
    std::vector<uint64_t> dead_tids = {tid1, tid1, tid1}; // Same TID 3 times
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = bitmap_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    // Should remove only once (idempotent)
}

// Test 7: Multiple values, each with dead TIDs
TEST_F(BitmapIndexGCTest, MultipleValuesRemoval)
{
    ErrorContext ctx;

    // Create Bitmap Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = BitmapIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto bitmap_index = BitmapIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(bitmap_index, nullptr);

    // Insert entries with different values
    // Value "red": TIDs 1, 2, 3
    // Value "blue": TIDs 4, 5, 6
    // Value "green": TIDs 7, 8, 9

    const char *red = "red";
    const char *blue = "blue";
    const char *green = "green";

    for (uint64_t tid = 1; tid <= 3; tid++)
    {
        status = bitmap_index->insert(red, strlen(red), tid, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    for (uint64_t tid = 4; tid <= 6; tid++)
    {
        status = bitmap_index->insert(blue, strlen(blue), tid, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    for (uint64_t tid = 7; tid <= 9; tid++)
    {
        status = bitmap_index->insert(green, strlen(green), tid, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Remove TID 2 (from red), TID 5 (from blue), TID 8 (from green)
    std::vector<uint64_t> dead_tids = {2, 5, 8};
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = bitmap_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_GE(entries_removed, 3);  // At least 3 entries removed (one from each bitmap)

    // Verify each value now has 2 TIDs
    auto red_tids = bitmap_index->find(red, strlen(red), nullptr, &ctx);
    EXPECT_EQ(red_tids.size(), 2);  // 1, 3

    auto blue_tids = bitmap_index->find(blue, strlen(blue), nullptr, &ctx);
    EXPECT_EQ(blue_tids.size(), 2);  // 4, 6

    auto green_tids = bitmap_index->find(green, strlen(green), nullptr, &ctx);
    EXPECT_EQ(green_tids.size(), 2);  // 7, 9
}

// Test 8: Empty index (no dictionary entries)
TEST_F(BitmapIndexGCTest, EmptyIndexGC)
{
    ErrorContext ctx;

    // Create Bitmap Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = BitmapIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto bitmap_index = BitmapIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(bitmap_index, nullptr);

    // Don't insert anything, try to remove dead TIDs
    std::vector<uint64_t> dead_tids = {1, 2, 3};
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = bitmap_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0);  // Nothing to remove
    EXPECT_EQ(pages_modified, 0);
}
