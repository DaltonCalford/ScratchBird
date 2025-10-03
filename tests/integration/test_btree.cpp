#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <random>
#include <algorithm>

using namespace scratchbird::core;

class BTreeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Clean up any existing test database
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    void TearDown() override
    {
        // Clean up test database
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    // Helper to create a key from an integer
    std::vector<uint8_t> makeKey(uint32_t value)
    {
        std::vector<uint8_t> key(sizeof(uint32_t));
        *reinterpret_cast<uint32_t*>(key.data()) = value;
        return key;
    }

    // Helper to get key value from bytes
    uint32_t getKeyValue(const std::vector<uint8_t>& key)
    {
        return *reinterpret_cast<const uint32_t*>(key.data());
    }

    const std::string test_db_path_ = "test_btree.db";
};

// Test 1: Create B-tree index
TEST_F(BTreeTest, CreateIndex)
{
    ErrorContext ctx;

    // Create database
    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    // Create B-tree index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
    uint32_t root_page = 0;

    status = BTree::create(&db, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_GT(root_page, 0u);

    db.close();
}

// Test 2: Open existing B-tree index
TEST_F(BTreeTest, OpenIndex)
{
    ErrorContext ctx;

    // Create database and index
    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
    uint32_t root_page = 0;

    status = BTree::create(&db, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Open the index
    auto btree = BTree::open(&db, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    db.close();
}

// Test 3: Basic insert and search
TEST_F(BTreeTest, BasicInsertAndSearch)
{
    ErrorContext ctx;

    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
    uint32_t root_page = 0;

    status = BTree::create(&db, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert some entries
    std::vector<uint8_t> key1 = makeKey(100);
    std::vector<uint8_t> key2 = makeKey(200);
    std::vector<uint8_t> key3 = makeKey(150);

    status = btree->insert(key1, 1001, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = btree->insert(key2, 2001, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = btree->insert(key3, 1501, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    // Search for entries
    std::vector<uint64_t> tuple_ids;

    status = btree->search(key1, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(tuple_ids.size(), 1u);
    ASSERT_EQ(tuple_ids[0], 1001u);

    tuple_ids.clear();
    status = btree->search(key2, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(tuple_ids.size(), 1u);
    ASSERT_EQ(tuple_ids[0], 2001u);

    tuple_ids.clear();
    status = btree->search(key3, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(tuple_ids.size(), 1u);
    ASSERT_EQ(tuple_ids[0], 1501u);

    db.close();
}

// Test 4: Insert many entries to trigger splits
TEST_F(BTreeTest, InsertManyEntriesTriggerSplits)
{
    ErrorContext ctx;

    Status status = Database::create(test_db_path_, 8192, &ctx); // Smaller page for easier splits
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
    uint32_t root_page = 0;

    status = BTree::create(&db, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert 1000 entries in sequential order
    for (uint32_t i = 0; i < 1000; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        status = btree->insert(key, i * 10, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed at insert " << i << ": " << ctx.message;
    }

    // Verify all entries can be found
    for (uint32_t i = 0; i < 1000; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint64_t> tuple_ids;
        status = btree->search(key, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed search at " << i;
        ASSERT_EQ(tuple_ids.size(), 1u);
        ASSERT_EQ(tuple_ids[0], i * 10);
    }

    db.close();
}

// Test 5: Insert in random order
TEST_F(BTreeTest, InsertRandomOrder)
{
    ErrorContext ctx;

    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
    uint32_t root_page = 0;

    status = BTree::create(&db, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Create random order
    std::vector<uint32_t> keys;
    for (uint32_t i = 0; i < 500; i++)
    {
        keys.push_back(i);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(keys.begin(), keys.end(), gen);

    // Insert in random order
    for (uint32_t key_val : keys)
    {
        std::vector<uint8_t> key = makeKey(key_val);
        status = btree->insert(key, key_val * 100, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed at key " << key_val;
    }

    // Verify all entries
    for (uint32_t i = 0; i < 500; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint64_t> tuple_ids;
        status = btree->search(key, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(tuple_ids.size(), 1u);
        ASSERT_EQ(tuple_ids[0], i * 100);
    }

    db.close();
}

// Test 6: Duplicate key handling
TEST_F(BTreeTest, DuplicateKeys)
{
    ErrorContext ctx;

    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
    uint32_t root_page = 0;

    status = BTree::create(&db, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert multiple tuples with same key
    std::vector<uint8_t> key = makeKey(42);

    status = btree->insert(key, 100, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key, 200, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key, 300, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Search should return all tuple IDs
    std::vector<uint64_t> tuple_ids;
    status = btree->search(key, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(tuple_ids.size(), 3u);

    // Verify all IDs are present (order may vary)
    std::sort(tuple_ids.begin(), tuple_ids.end());
    ASSERT_EQ(tuple_ids[0], 100u);
    ASSERT_EQ(tuple_ids[1], 200u);
    ASSERT_EQ(tuple_ids[2], 300u);

    db.close();
}

// Test 7: Remove operation (soft delete)
TEST_F(BTreeTest, RemoveOperation)
{
    ErrorContext ctx;

    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
    uint32_t root_page = 0;

    status = BTree::create(&db, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert entries
    std::vector<uint8_t> key1 = makeKey(10);
    std::vector<uint8_t> key2 = makeKey(20);
    std::vector<uint8_t> key3 = makeKey(30);

    status = btree->insert(key1, 100, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key2, 200, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key3, 300, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Remove one entry
    status = btree->remove(key2, 200, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    // Verify removed entry is not found
    std::vector<uint64_t> tuple_ids;
    status = btree->search(key2, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::NOT_FOUND);

    // Other entries should still exist
    status = btree->search(key1, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(tuple_ids.size(), 1u);

    db.close();
}

// Test 8: Search for non-existent key
TEST_F(BTreeTest, SearchNonExistent)
{
    ErrorContext ctx;

    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
    uint32_t root_page = 0;

    status = BTree::create(&db, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert some entries
    for (uint32_t i = 0; i < 100; i += 10)
    {
        std::vector<uint8_t> key = makeKey(i);
        status = btree->insert(key, i, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Search for key that doesn't exist
    std::vector<uint8_t> missing_key = makeKey(5);
    std::vector<uint64_t> tuple_ids;
    status = btree->search(missing_key, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::NOT_FOUND);

    db.close();
}

// Test 9: Large dataset test (10K entries)
TEST_F(BTreeTest, LargeDataset)
{
    ErrorContext ctx;

    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
    uint32_t root_page = 0;

    status = BTree::create(&db, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert 10,000 entries
    const uint32_t num_entries = 10000;
    for (uint32_t i = 0; i < num_entries; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        status = btree->insert(key, i * 7, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed at " << i << ": " << ctx.message;
    }

    // Random sample verification (1000 entries)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, num_entries - 1);

    for (int i = 0; i < 1000; i++)
    {
        uint32_t key_val = dist(gen);
        std::vector<uint8_t> key = makeKey(key_val);
        std::vector<uint64_t> tuple_ids;
        status = btree->search(key, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(tuple_ids.size(), 1u);
        ASSERT_EQ(tuple_ids[0], key_val * 7);
    }

    db.close();
}

// Test 10: Persistence - verify data survives close/reopen
TEST_F(BTreeTest, Persistence)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
    uint32_t root_page = 0;

    // Create and populate index
    {
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK);

        Database db;
        status = db.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK);

        status = BTree::create(&db, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
        ASSERT_EQ(status, Status::OK);

        auto btree = BTree::open(&db, index_uuid, root_page, &ctx);
        ASSERT_NE(btree, nullptr);

        // Insert data
        for (uint32_t i = 0; i < 100; i++)
        {
            std::vector<uint8_t> key = makeKey(i);
            status = btree->insert(key, i * 3, &ctx);
            ASSERT_EQ(status, Status::OK);
        }

        db.close();
    }

    // Reopen and verify
    {
        Database db;
        Status status = db.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK);

        auto btree = BTree::open(&db, index_uuid, root_page, &ctx);
        ASSERT_NE(btree, nullptr);

        // Verify all data
        for (uint32_t i = 0; i < 100; i++)
        {
            std::vector<uint8_t> key = makeKey(i);
            std::vector<uint64_t> tuple_ids;
            status = btree->search(key, &tuple_ids, &ctx);
            ASSERT_EQ(status, Status::OK);
            ASSERT_EQ(tuple_ids.size(), 1u);
            ASSERT_EQ(tuple_ids[0], i * 3);
        }

        db.close();
    }
}
