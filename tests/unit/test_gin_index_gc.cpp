// PHASE 2 TASK 2.4: Unit tests for GIN Index Garbage Collection
#include <gtest/gtest.h>
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <vector>

using namespace scratchbird::core;

class GinIndexGCTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database *db_;

    void SetUp() override
    {
        // Create temporary database for testing
        test_db_path_ = "/tmp/test_gin_index_gc_" + std::to_string(getpid()) + ".db";

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

    // Helper: Simple key extractor that treats entire value as single key
    static std::vector<std::vector<uint8_t>> singleKeyExtractor(const void *data, size_t len)
    {
        std::vector<std::vector<uint8_t>> result;
        std::vector<uint8_t> key(static_cast<const uint8_t *>(data),
                                  static_cast<const uint8_t *>(data) + len);
        result.push_back(key);
        return result;
    }

    // Helper: Array key extractor - splits comma-separated string into keys
    static std::vector<std::vector<uint8_t>> arrayKeyExtractor(const void *data, size_t len)
    {
        std::vector<std::vector<uint8_t>> result;
        std::string str(static_cast<const char *>(data), len);

        size_t pos = 0;
        while (pos < str.length())
        {
            size_t comma = str.find(',', pos);
            if (comma == std::string::npos)
            {
                comma = str.length();
            }

            std::string key_str = str.substr(pos, comma - pos);
            std::vector<uint8_t> key(key_str.begin(), key_str.end());
            result.push_back(key);

            pos = comma + 1;
        }

        return result;
    }
};

// Test 1: Empty dead_tids vector (no-op)
TEST_F(GinIndexGCTest, EmptyDeadTidsVector)
{
    ErrorContext ctx;

    // Create GIN Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = GinIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create GIN Index: " << ctx.message;

    auto gin_index = GinIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr) << "Failed to open GIN Index: " << ctx.message;

    // Call removeDeadEntries with empty vector
    std::vector<uint64_t> dead_tids;
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = gin_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0);
    EXPECT_EQ(pages_modified, 0);
}

// Test 2: Dead TID not in index (idempotency)
TEST_F(GinIndexGCTest, DeadTidNotInIndex)
{
    ErrorContext ctx;

    // Create GIN Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = GinIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert a few entries into pending list
    const char *value1 = "apple";
    const char *value2 = "banana";

    uint64_t tid1 = (1ULL << 32) | 1; // page 1, item 1
    uint64_t tid2 = (1ULL << 32) | 2; // page 1, item 2

    status = gin_index->insert(value1, strlen(value1), tid1, singleKeyExtractor, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = gin_index->insert(value2, strlen(value2), tid2, singleKeyExtractor, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Try to remove TIDs that don't exist in index
    std::vector<uint64_t> dead_tids = {
        (2ULL << 32) | 1,  // page 2, item 1 (doesn't exist)
        (2ULL << 32) | 2   // page 2, item 2 (doesn't exist)
    };

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = gin_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0); // Nothing removed
}

// Test 3: Single dead TID removal from pending list
TEST_F(GinIndexGCTest, SingleDeadTidRemovalFromPendingList)
{
    ErrorContext ctx;

    // Create GIN Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = GinIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert entries into pending list
    const char *key1 = "alpha";
    const char *key2 = "beta";
    const char *key3 = "gamma";

    uint64_t tid1 = (1ULL << 32) | 1;
    uint64_t tid2 = (1ULL << 32) | 2;
    uint64_t tid3 = (1ULL << 32) | 3;

    status = gin_index->insert(key1, strlen(key1), tid1, singleKeyExtractor, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = gin_index->insert(key2, strlen(key2), tid2, singleKeyExtractor, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = gin_index->insert(key3, strlen(key3), tid3, singleKeyExtractor, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Get statistics before removal
    auto stats_before = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats_before.pending_list_count, 3);

    // Remove tid2
    std::vector<uint64_t> dead_tids = {tid2};
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = gin_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 1);  // One entry removed from pending list
    EXPECT_GE(pages_modified, 1);   // At least one page modified (pending page + meta)

    // Get statistics after removal
    auto stats_after = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats_after.pending_list_count, 2);  // Should decrease by 1
}

// Test 4: Bulk dead TID removal from pending list
TEST_F(GinIndexGCTest, BulkDeadTidRemovalFromPendingList)
{
    ErrorContext ctx;

    // Create GIN Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = GinIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert many entries into pending list
    const int num_entries = 50;
    std::vector<uint64_t> all_tids;

    for (int i = 0; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        uint64_t tid = (1ULL << 32) | i;
        all_tids.push_back(tid);

        status = gin_index->insert(key.c_str(), key.length(), tid, singleKeyExtractor, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Get statistics before removal
    auto stats_before = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats_before.pending_list_count, num_entries);

    // Remove every other entry (25 entries)
    std::vector<uint64_t> dead_tids;
    for (int i = 0; i < num_entries; i += 2)
    {
        dead_tids.push_back(all_tids[i]);
    }

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = gin_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 25);  // Half the entries
    EXPECT_GE(pages_modified, 1);

    // Get statistics after removal
    auto stats_after = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats_after.pending_list_count, 25);  // Half remaining
}

// Test 5: Index type name
TEST_F(GinIndexGCTest, IndexTypeName)
{
    ErrorContext ctx;

    // Create GIN Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = GinIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Check index type name
    const char *type_name = gin_index->indexTypeName();
    EXPECT_STREQ(type_name, "GIN");
}

// Test 6: Duplicate dead TIDs (idempotency)
TEST_F(GinIndexGCTest, DuplicateDeadTids)
{
    ErrorContext ctx;

    // Create GIN Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = GinIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert entry
    const char *key1 = "test_key";
    uint64_t tid1 = (1ULL << 32) | 1;

    status = gin_index->insert(key1, strlen(key1), tid1, singleKeyExtractor, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Remove with duplicate TIDs in vector
    std::vector<uint64_t> dead_tids = {tid1, tid1, tid1}; // Same TID 3 times
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = gin_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 1);  // Still only 1 entry removed (idempotent)
    EXPECT_GE(pages_modified, 1);
}

// Test 7: Multi-key insert and removal
TEST_F(GinIndexGCTest, MultiKeyRemoval)
{
    ErrorContext ctx;

    // Create GIN Index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    uint32_t meta_page = 0;

    Status status = GinIndex::create(db_, index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_, index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert entries with multiple keys (array-like)
    // Each insert generates multiple pending list entries (one per key)
    const char *value1 = "apple,banana,cherry";  // 3 keys
    const char *value2 = "date,elderberry";      // 2 keys
    const char *value3 = "fig,grape";             // 2 keys

    uint64_t tid1 = (1ULL << 32) | 1;
    uint64_t tid2 = (1ULL << 32) | 2;
    uint64_t tid3 = (1ULL << 32) | 3;

    status = gin_index->insert(value1, strlen(value1), tid1, arrayKeyExtractor, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = gin_index->insert(value2, strlen(value2), tid2, arrayKeyExtractor, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = gin_index->insert(value3, strlen(value3), tid3, arrayKeyExtractor, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Get statistics - should have 3+2+2 = 7 pending entries
    auto stats_before = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats_before.pending_list_count, 7);

    // Remove tid2 (which had 2 keys: "date" and "elderberry")
    std::vector<uint64_t> dead_tids = {tid2};
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = gin_index->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 2);  // 2 pending entries removed (date, elderberry)

    // Get statistics after removal
    auto stats_after = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats_after.pending_list_count, 5);  // 7 - 2 = 5
}
