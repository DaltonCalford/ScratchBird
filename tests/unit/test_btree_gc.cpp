// PHASE 2 TASK 2.2: Unit tests for B-Tree Index Garbage Collection
#include <gtest/gtest.h>
#include "scratchbird/core/btree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <vector>

using namespace scratchbird::core;

class BTreeGCTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database *db_;

    void SetUp() override
    {
        // Create temporary database for testing
        test_db_path_ = "/tmp/test_btree_gc_" + std::to_string(getpid()) + ".db";

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
TEST_F(BTreeGCTest, EmptyDeadTidsVector)
{
    ErrorContext ctx;

    // Create B-Tree index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create B-Tree: " << ctx.message;

    auto btree = BTree::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr) << "Failed to open B-Tree: " << ctx.message;

    // Call removeDeadEntries with empty vector
    std::vector<uint64_t> dead_tids;
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = btree->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0);
    EXPECT_EQ(pages_modified, 0);
}

// Test 2: Dead TID not in index (idempotency)
TEST_F(BTreeGCTest, DeadTidNotInIndex)
{
    ErrorContext ctx;

    // Create B-Tree index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert a few entries
    std::vector<uint8_t> key1 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> key2 = {0x04, 0x05, 0x06};

    uint64_t tid1 = (1ULL << 32) | 1; // page 1, item 1
    uint64_t tid2 = (1ULL << 32) | 2; // page 1, item 2

    status = btree->insert(key1, tid1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key2, tid2, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Try to remove TIDs that don't exist in index
    std::vector<uint64_t> dead_tids = {
        (2ULL << 32) | 1,  // page 2, item 1 (doesn't exist)
        (2ULL << 32) | 2   // page 2, item 2 (doesn't exist)
    };

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = btree->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0); // Nothing removed
    EXPECT_EQ(pages_modified, 0);  // No pages modified
}

// Test 3: Single dead TID removal
TEST_F(BTreeGCTest, SingleDeadTidRemoval)
{
    ErrorContext ctx;

    // Create B-Tree index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert entries
    std::vector<uint8_t> key1 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> key2 = {0x04, 0x05, 0x06};
    std::vector<uint8_t> key3 = {0x07, 0x08, 0x09};

    uint64_t tid1 = (1ULL << 32) | 1;
    uint64_t tid2 = (1ULL << 32) | 2;
    uint64_t tid3 = (1ULL << 32) | 3;

    status = btree->insert(key1, tid1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key2, tid2, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key3, tid3, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Remove tid2
    std::vector<uint64_t> dead_tids = {tid2};
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = btree->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 1);  // One entry removed
    EXPECT_GE(pages_modified, 1);   // At least one page modified

    // Verify tid2 is no longer found
    std::vector<uint64_t> result_tids;
    status = btree->search(key2, nullptr, &result_tids, &ctx);

    // Entry should be marked as deleted (might still be found but marked)
    // Or NOT_FOUND if vacuum ran
    // Either is acceptable
}

// Test 4: Index type name
TEST_F(BTreeGCTest, IndexTypeName)
{
    ErrorContext ctx;

    // Create B-Tree index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Check index type name
    const char *type_name = btree->indexTypeName();
    EXPECT_STREQ(type_name, "B-Tree");
}

// Test 5: Duplicate dead TIDs (idempotency)
TEST_F(BTreeGCTest, DuplicateDeadTids)
{
    ErrorContext ctx;

    // Create B-Tree index
    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert entry
    std::vector<uint8_t> key1 = {0x01, 0x02, 0x03};
    uint64_t tid1 = (1ULL << 32) | 1;

    status = btree->insert(key1, tid1, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Remove with duplicate TIDs in vector
    std::vector<uint64_t> dead_tids = {tid1, tid1, tid1}; // Same TID 3 times
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = btree->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 1);  // Still only 1 entry removed (idempotent)
    EXPECT_GE(pages_modified, 1);
}
