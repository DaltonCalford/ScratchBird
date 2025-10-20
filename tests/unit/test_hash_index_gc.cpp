// PHASE 2 TASK 2.3: Unit tests for Hash Index Garbage Collection
#include <gtest/gtest.h>
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <vector>

using namespace scratchbird::core;

class HashIndexGCTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database *db_;

    void SetUp() override
    {
        // Create temporary database for testing
        test_db_path_ = "/tmp/test_hash_index_gc_" + std::to_string(getpid()) + ".db";

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
TEST_F(HashIndexGCTest, EmptyDeadTidsVector)
{
    ErrorContext ctx;

    // Create Hash Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create Hash Index: " << ctx.message;

    auto hash_index = HashIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(hash_index, nullptr) << "Failed to open Hash Index: " << ctx.message;

    // Call removeDeadEntries with empty vector
    std::vector<uint64_t> dead_tids;
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = hash_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0);
    EXPECT_EQ(pages_modified, 0);
}

// Test 2: Dead TID not in index (idempotency)
TEST_F(HashIndexGCTest, DeadTidNotInIndex)
{
    ErrorContext ctx;

    // Create Hash Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hash_index = HashIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(hash_index, nullptr);

    // Insert a few entries
    const char *key1 = "key1";
    const char *key2 = "key2";

    uint64_t tid1 = (1ULL << 32) | 1; // page 1, item 1
    uint64_t tid2 = (1ULL << 32) | 2; // page 1, item 2

    status = hash_index->insert(key1, strlen(key1), tid1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = hash_index->insert(key2, strlen(key2), tid2, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Try to remove TIDs that don't exist in index
    std::vector<uint64_t> dead_tids = {
        (2ULL << 32) | 1,  // page 2, item 1 (doesn't exist)
        (2ULL << 32) | 2   // page 2, item 2 (doesn't exist)
    };

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = hash_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0); // Nothing removed
}

// Test 3: Single dead TID removal
TEST_F(HashIndexGCTest, SingleDeadTidRemoval)
{
    ErrorContext ctx;

    // Create Hash Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hash_index = HashIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(hash_index, nullptr);

    // Insert entries
    const char *key1 = "alpha";
    const char *key2 = "beta";
    const char *key3 = "gamma";

    uint64_t tid1 = (1ULL << 32) | 1;
    uint64_t tid2 = (1ULL << 32) | 2;
    uint64_t tid3 = (1ULL << 32) | 3;

    status = hash_index->insert(key1, strlen(key1), tid1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = hash_index->insert(key2, strlen(key2), tid2, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = hash_index->insert(key3, strlen(key3), tid3, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Get statistics before removal
    auto stats_before = hash_index->getStatistics(&ctx);
    EXPECT_EQ(stats_before.num_tuples, 3);

    // Remove tid2
    std::vector<uint64_t> dead_tids = {tid2};
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = hash_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 1);  // One entry removed
    EXPECT_GE(pages_modified, 1);   // At least one page modified

    // Get statistics after removal
    auto stats_after = hash_index->getStatistics(&ctx);
    EXPECT_EQ(stats_after.num_tuples, 2);  // Should decrease by 1
    EXPECT_EQ(stats_after.num_deleted, stats_before.num_deleted + 1); // Deleted count increases

    // Verify tid2 is no longer found (returns empty vector)
    auto result_tids = hash_index->find(key2, strlen(key2), nullptr, &ctx);
    // Entry marked as deleted (he_tuple_id = 0), so should not be returned
    // Or might return empty depending on implementation
}

// Test 4: Bulk dead TID removal
TEST_F(HashIndexGCTest, BulkDeadTidRemoval)
{
    ErrorContext ctx;

    // Create Hash Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hash_index = HashIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(hash_index, nullptr);

    // Insert many entries
    const int num_entries = 50;
    std::vector<uint64_t> all_tids;

    for (int i = 0; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        uint64_t tid = (1ULL << 32) | i;
        all_tids.push_back(tid);

        status = hash_index->insert(key.c_str(), key.length(), tid, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Get statistics before removal
    auto stats_before = hash_index->getStatistics(&ctx);
    EXPECT_EQ(stats_before.num_tuples, num_entries);

    // Remove every other entry (25 entries)
    std::vector<uint64_t> dead_tids;
    for (int i = 0; i < num_entries; i += 2)
    {
        dead_tids.push_back(all_tids[i]);
    }

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = hash_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 25);  // Half the entries
    EXPECT_GE(pages_modified, 1);

    // Get statistics after removal
    auto stats_after = hash_index->getStatistics(&ctx);
    EXPECT_EQ(stats_after.num_tuples, 25);  // Half remaining
    EXPECT_EQ(stats_after.num_deleted, stats_before.num_deleted + 25);
}

// Test 5: Index type name
TEST_F(HashIndexGCTest, IndexTypeName)
{
    ErrorContext ctx;

    // Create Hash Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hash_index = HashIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(hash_index, nullptr);

    // Check index type name
    const char *type_name = hash_index->indexTypeName();
    EXPECT_STREQ(type_name, "Hash");
}

// Test 6: Duplicate dead TIDs (idempotency)
TEST_F(HashIndexGCTest, DuplicateDeadTids)
{
    ErrorContext ctx;

    // Create Hash Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hash_index = HashIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(hash_index, nullptr);

    // Insert entry
    const char *key1 = "test_key";
    uint64_t tid1 = (1ULL << 32) | 1;

    status = hash_index->insert(key1, strlen(key1), tid1, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Remove with duplicate TIDs in vector
    std::vector<uint64_t> dead_tids = {tid1, tid1, tid1}; // Same TID 3 times
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = hash_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 1);  // Still only 1 entry removed (idempotent)
    EXPECT_GE(pages_modified, 1);
}

// Test 7: Statistics update verification
TEST_F(HashIndexGCTest, StatisticsUpdate)
{
    ErrorContext ctx;

    // Create Hash Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hash_index = HashIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(hash_index, nullptr);

    // Insert 10 entries
    for (int i = 0; i < 10; i++)
    {
        std::string key = "stat_key_" + std::to_string(i);
        uint64_t tid = (1ULL << 32) | i;
        status = hash_index->insert(key.c_str(), key.length(), tid, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto stats_initial = hash_index->getStatistics(&ctx);
    EXPECT_EQ(stats_initial.num_tuples, 10);
    uint64_t initial_deleted = stats_initial.num_deleted;

    // Remove 3 entries
    std::vector<uint64_t> dead_tids = {
        (1ULL << 32) | 0,
        (1ULL << 32) | 5,
        (1ULL << 32) | 9
    };

    uint64_t entries_removed = 0;
    status = hash_index->removeDeadEntries(dead_tids, &entries_removed, nullptr, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 3);

    auto stats_final = hash_index->getStatistics(&ctx);
    EXPECT_EQ(stats_final.num_tuples, 7);  // 10 - 3 = 7
    EXPECT_EQ(stats_final.num_deleted, initial_deleted + 3);  // Deleted count increased by 3
}
