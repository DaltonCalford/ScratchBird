#include <gtest/gtest.h>
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/hash_functions.h"
#include <cstring>
#include <filesystem>
#include <algorithm>

using namespace scratchbird::core;

class HashIndexTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test database
        test_db_path = "test_hash_index.db";

        // Remove if exists
        if (std::filesystem::exists(test_db_path))
        {
            std::filesystem::remove(test_db_path);
        }

        // Create database
        ErrorContext ctx;
        Status status = Database::create(test_db_path.c_str(), 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        // Open database
        db = std::make_unique<Database>();
        status = db->open(test_db_path, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;
    }

    void TearDown() override
    {
        if (db)
        {
            db->close();
            db.reset();
        }

        // Clean up test file
        if (std::filesystem::exists(test_db_path))
        {
            std::filesystem::remove(test_db_path);
        }
    }

    std::string test_db_path;
    std::unique_ptr<Database> db;
};

// Test 1: Create hash index
TEST_F(HashIndexTest, CreateIndex)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create hash index: " << ctx.message;
    ASSERT_GT(meta_page, 0);
}

// Test 2: Open existing hash index
TEST_F(HashIndexTest, OpenIndex)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    // Create index
    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Open index
    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr) << "Failed to open hash index: " << ctx.message;
    ASSERT_EQ(index->getIndexUuid(), index_uuid);
    ASSERT_EQ(index->getMetaPage(), meta_page);
}

// Test 3: Insert single entry
TEST_F(HashIndexTest, InsertSingle)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert entry
    const char* key = "test_key";
    uint64_t tuple_id = (1ULL << 32) | 1; // page 1, item 1

    status = index->insert(key, std::strlen(key), tuple_id, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to insert: " << ctx.message;

    // Verify statistics
    auto stats = index->getStatistics(&ctx);
    ASSERT_EQ(stats.num_tuples, 1);
    ASSERT_EQ(stats.num_deleted, 0);
}

// Test 4: Insert and find
TEST_F(HashIndexTest, InsertAndFind)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert multiple entries
    const char* key1 = "key1";
    const char* key2 = "key2";
    const char* key3 = "key3";

    uint64_t tid1 = (1ULL << 32) | 1;
    uint64_t tid2 = (1ULL << 32) | 2;
    uint64_t tid3 = (1ULL << 32) | 3;

    ASSERT_EQ(index->insert(key1, std::strlen(key1), tid1, &ctx), Status::OK);
    ASSERT_EQ(index->insert(key2, std::strlen(key2), tid2, &ctx), Status::OK);
    ASSERT_EQ(index->insert(key3, std::strlen(key3), tid3, &ctx), Status::OK);

    // Find entries
    auto results1 = index->find(key1, std::strlen(key1), &ctx);
    ASSERT_EQ(results1.size(), 1);
    ASSERT_EQ(results1[0], tid1);

    auto results2 = index->find(key2, std::strlen(key2), &ctx);
    ASSERT_EQ(results2.size(), 1);
    ASSERT_EQ(results2[0], tid2);

    auto results3 = index->find(key3, std::strlen(key3), &ctx);
    ASSERT_EQ(results3.size(), 1);
    ASSERT_EQ(results3[0], tid3);

    // Find non-existent key
    const char* key4 = "key4";
    auto results4 = index->find(key4, std::strlen(key4), &ctx);
    ASSERT_EQ(results4.size(), 0);
}

// Test 5: Insert duplicates (same key, different tuple IDs)
TEST_F(HashIndexTest, InsertDuplicates)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert same key with different tuple IDs
    const char* key = "duplicate_key";
    uint64_t tid1 = (1ULL << 32) | 1;
    uint64_t tid2 = (1ULL << 32) | 2;
    uint64_t tid3 = (1ULL << 32) | 3;

    ASSERT_EQ(index->insert(key, std::strlen(key), tid1, &ctx), Status::OK);
    ASSERT_EQ(index->insert(key, std::strlen(key), tid2, &ctx), Status::OK);
    ASSERT_EQ(index->insert(key, std::strlen(key), tid3, &ctx), Status::OK);

    // Find should return all three
    auto results = index->find(key, std::strlen(key), &ctx);
    ASSERT_EQ(results.size(), 3);

    // Verify all tuple IDs are present
    ASSERT_TRUE(std::find(results.begin(), results.end(), tid1) != results.end());
    ASSERT_TRUE(std::find(results.begin(), results.end(), tid2) != results.end());
    ASSERT_TRUE(std::find(results.begin(), results.end(), tid3) != results.end());
}

// Test 6: Delete entry
TEST_F(HashIndexTest, DeleteEntry)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert entry
    const char* key = "test_key";
    uint64_t tuple_id = (1ULL << 32) | 1;

    ASSERT_EQ(index->insert(key, std::strlen(key), tuple_id, &ctx), Status::OK);

    // Verify it exists
    auto results = index->find(key, std::strlen(key), &ctx);
    ASSERT_EQ(results.size(), 1);

    // Delete entry
    status = index->remove(key, std::strlen(key), tuple_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify it's gone
    results = index->find(key, std::strlen(key), &ctx);
    ASSERT_EQ(results.size(), 0);

    // Verify statistics
    auto stats = index->getStatistics(&ctx);
    ASSERT_EQ(stats.num_deleted, 1);
}

// Test 7: Bucket split with many entries
TEST_F(HashIndexTest, BucketSplit)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Get initial statistics
    auto stats_before = index->getStatistics(&ctx);
    uint32_t initial_depth = stats_before.global_depth;

    // Insert many entries to trigger splits
    const int num_entries = 1000;
    for (int i = 0; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        uint64_t tuple_id = (1ULL << 32) | i;

        status = index->insert(key.c_str(), key.length(), tuple_id, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to insert entry " << i;
    }

    // Verify all entries can be found
    for (int i = 0; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        auto results = index->find(key.c_str(), key.length(), &ctx);
        ASSERT_EQ(results.size(), 1) << "Failed to find entry " << i;
        ASSERT_EQ(results[0], (1ULL << 32) | i);
    }

    // Verify statistics
    auto stats_after = index->getStatistics(&ctx);
    ASSERT_EQ(stats_after.num_tuples, num_entries);
    ASSERT_GE(stats_after.global_depth, initial_depth); // Depth should increase
}

// Test 8: Vacuum operation
TEST_F(HashIndexTest, Vacuum)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert entries
    const int num_entries = 100;
    for (int i = 0; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        uint64_t tuple_id = (1ULL << 32) | i;
        ASSERT_EQ(index->insert(key.c_str(), key.length(), tuple_id, &ctx), Status::OK);
    }

    // Delete half of them
    for (int i = 0; i < num_entries / 2; i++)
    {
        std::string key = "key_" + std::to_string(i);
        uint64_t tuple_id = (1ULL << 32) | i;
        ASSERT_EQ(index->remove(key.c_str(), key.length(), tuple_id, &ctx), Status::OK);
    }

    // Verify deleted count
    auto stats_before = index->getStatistics(&ctx);
    ASSERT_EQ(stats_before.num_deleted, num_entries / 2);

    // Run vacuum
    status = index->vacuum(&ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify deleted count is reduced
    auto stats_after = index->getStatistics(&ctx);
    ASSERT_EQ(stats_after.num_deleted, 0);

    // Verify remaining entries are still findable
    for (int i = num_entries / 2; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        auto results = index->find(key.c_str(), key.length(), &ctx);
        ASSERT_EQ(results.size(), 1);
    }
}

// Test 9: Hash function consistency
TEST_F(HashIndexTest, HashFunctionConsistency)
{
    const char* key = "test_key";
    size_t len = std::strlen(key);

    // Hash same key multiple times
    uint64_t hash1 = MurmurHash64(key, len);
    uint64_t hash2 = MurmurHash64(key, len);
    uint64_t hash3 = MurmurHash64(key, len);

    // Should all be the same
    ASSERT_EQ(hash1, hash2);
    ASSERT_EQ(hash2, hash3);

    // Different keys should have different hashes
    const char* key2 = "different_key";
    uint64_t hash4 = MurmurHash64(key2, std::strlen(key2));
    ASSERT_NE(hash1, hash4);
}

// Test 10: Large dataset stress test
TEST_F(HashIndexTest, LargeDataset)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert 10,000 entries
    const int num_entries = 10000;
    for (int i = 0; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        uint64_t tuple_id = (1ULL << 32) | i;

        status = index->insert(key.c_str(), key.length(), tuple_id, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed at entry " << i;
    }

    // Verify statistics
    auto stats = index->getStatistics(&ctx);
    ASSERT_EQ(stats.num_tuples, num_entries);
    ASSERT_GT(stats.global_depth, 4); // Should have grown beyond initial depth

    // Random access test - verify 1000 random entries
    for (int i = 0; i < 1000; i++)
    {
        int idx = rand() % num_entries;
        std::string key = "key_" + std::to_string(idx);
        auto results = index->find(key.c_str(), key.length(), &ctx);
        ASSERT_EQ(results.size(), 1);
        ASSERT_EQ(results[0], (1ULL << 32) | idx);
    }
}

// Test 11: Statistics accuracy
TEST_F(HashIndexTest, StatisticsAccuracy)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Initial statistics
    auto stats = index->getStatistics(&ctx);
    ASSERT_EQ(stats.num_tuples, 0);
    ASSERT_EQ(stats.num_deleted, 0);
    ASSERT_EQ(stats.global_depth, INITIAL_GLOBAL_DEPTH);
    ASSERT_EQ(stats.num_buckets, 1U << INITIAL_GLOBAL_DEPTH);

    // Insert 50 entries
    for (int i = 0; i < 50; i++)
    {
        std::string key = "key_" + std::to_string(i);
        uint64_t tuple_id = (1ULL << 32) | i;
        index->insert(key.c_str(), key.length(), tuple_id, &ctx);
    }

    stats = index->getStatistics(&ctx);
    ASSERT_EQ(stats.num_tuples, 50);
    ASSERT_GT(stats.avg_entries_per_bucket, 0.0);
    ASSERT_GT(stats.load_factor, 0.0);
    ASSERT_LT(stats.load_factor, 100.0);
}

// Test 12: Invalid operations
TEST_F(HashIndexTest, InvalidOperations)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert with null key
    status = index->insert(nullptr, 10, 1, &ctx);
    ASSERT_EQ(status, Status::INVALID_ARGUMENT);

    // Insert with zero length
    const char* key = "test";
    status = index->insert(key, 0, 1, &ctx);
    ASSERT_EQ(status, Status::INVALID_ARGUMENT);

    // Insert with zero tuple ID
    status = index->insert(key, 4, 0, &ctx);
    ASSERT_EQ(status, Status::INVALID_ARGUMENT);

    // Remove non-existent entry
    status = index->remove(key, 4, 1, &ctx);
    ASSERT_EQ(status, Status::NOT_FOUND);
}
