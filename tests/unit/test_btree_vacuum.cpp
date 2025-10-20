#include <gtest/gtest.h>
#include "scratchbird/core/btree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/uuidv7.h"
#include <filesystem>
#include <cstring>

namespace scratchbird::core::test
{

class BTreeVacuumTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test database directory
        test_db_path_ = "/tmp/test_btree_vacuum_db";
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove_all(test_db_path_);
        }
        std::filesystem::create_directories(test_db_path_);

        // Create database
        ErrorContext ctx;
        auto status = Database::create(test_db_path_, 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;
    }

    void TearDown() override
    {
        db_.close();
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove_all(test_db_path_);
        }
    }

    std::string test_db_path_;
    Database db_;
};

TEST_F(BTreeVacuumTest, VacuumEmptyTree)
{
    ErrorContext ctx;

    // Create a B-tree index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    auto status = BTree::create(&db_, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create B-tree: " << ctx.message;

    // Open the B-tree
    auto btree = BTree::open(&db_, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr) << "Failed to open B-tree: " << ctx.message;

    // Vacuum empty tree
    BTree::VacuumStats stats;
    status = btree->vacuum(&stats, &ctx);
    ASSERT_EQ(status, Status::OK) << "Vacuum failed: " << ctx.message;

    EXPECT_EQ(stats.pages_visited, 1);  // Only root page
    EXPECT_EQ(stats.pages_vacuumed, 0); // No garbage
    EXPECT_EQ(stats.nodes_removed, 0);
    EXPECT_EQ(stats.bytes_reclaimed, 0);
    EXPECT_EQ(stats.pages_merged, 0);
}

TEST_F(BTreeVacuumTest, VacuumWithDeletedNodes)
{
    ErrorContext ctx;

    // Create a B-tree index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    auto status = BTree::create(&db_, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create B-tree: " << ctx.message;

    // Open the B-tree
    auto btree = BTree::open(&db_, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr) << "Failed to open B-tree: " << ctx.message;

    // Insert some test data
    const int num_entries = 20;
    std::vector<std::pair<std::vector<uint8_t>, uint64_t>> test_data;

    for (int i = 0; i < num_entries; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        uint64_t tuple_id = i + 1000;

        status = btree->insert(key, tuple_id, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to insert key " << i << ": " << ctx.message;

        test_data.push_back({key, tuple_id});
    }

    // Delete every other entry
    int deleted_count = 0;
    for (int i = 0; i < num_entries; i += 2)
    {
        status = btree->remove(test_data[i].first, test_data[i].second, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to remove key " << i << ": " << ctx.message;
        deleted_count++;
    }

    // Vacuum the tree
    BTree::VacuumStats stats;
    status = btree->vacuum(&stats, &ctx);
    ASSERT_EQ(status, Status::OK) << "Vacuum failed: " << ctx.message;

    // Verify vacuum statistics
    EXPECT_GT(stats.pages_visited, 0) << "Should have visited at least one page";
    EXPECT_GT(stats.pages_vacuumed, 0) << "Should have vacuumed at least one page";
    EXPECT_EQ(stats.nodes_removed, deleted_count) << "Should have removed all deleted nodes";
    EXPECT_GT(stats.bytes_reclaimed, 0) << "Should have reclaimed some bytes";

    // Verify remaining entries are still searchable
    for (int i = 1; i < num_entries; i += 2)
    {
        std::vector<uint64_t> tuple_ids;
        status = btree->search(test_data[i].first, nullptr, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to search key " << i << ": " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1);
        EXPECT_EQ(tuple_ids[0], test_data[i].second);
    }

    // Verify deleted entries are not found
    for (int i = 0; i < num_entries; i += 2)
    {
        std::vector<uint64_t> tuple_ids;
        status = btree->search(test_data[i].first, nullptr, &tuple_ids, &ctx);
        EXPECT_EQ(status, Status::NOT_FOUND) << "Deleted key " << i << " should not be found";
    }
}

TEST_F(BTreeVacuumTest, CompactPageReclainsSpace)
{
    ErrorContext ctx;

    // Create a B-tree index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    auto status = BTree::create(&db_, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db_, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert 50 entries
    for (int i = 0; i < 50; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        status = btree->insert(key, i + 1000, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Delete 40 entries (80% deletion)
    for (int i = 0; i < 40; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        status = btree->remove(key, i + 1000, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Vacuum should reclaim significant space
    BTree::VacuumStats stats;
    status = btree->vacuum(&stats, &ctx);
    ASSERT_EQ(status, Status::OK);

    EXPECT_EQ(stats.nodes_removed, 40);
    EXPECT_GT(stats.bytes_reclaimed, 0);

    // Verify remaining 10 entries are still accessible
    for (int i = 40; i < 50; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        std::vector<uint64_t> tuple_ids;
        status = btree->search(key, nullptr, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(tuple_ids.size(), 1);
        EXPECT_EQ(tuple_ids[0], i + 1000);
    }
}

TEST_F(BTreeVacuumTest, VacuumMultipleTimes)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    auto status = BTree::create(&db_, index_uuid, table_uuid, column_uuids, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db_, index_uuid, root_page, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert 30 entries
    for (int i = 0; i < 30; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        status = btree->insert(key, i + 1000, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Delete 15 entries
    for (int i = 0; i < 15; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        status = btree->remove(key, i + 1000, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // First vacuum
    BTree::VacuumStats stats1;
    status = btree->vacuum(&stats1, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(stats1.nodes_removed, 15);

    // Second vacuum on already cleaned tree
    BTree::VacuumStats stats2;
    status = btree->vacuum(&stats2, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(stats2.nodes_removed, 0) << "Second vacuum should find no garbage";
    EXPECT_EQ(stats2.bytes_reclaimed, 0) << "Second vacuum should reclaim no space";
}

} // namespace scratchbird::core::test
