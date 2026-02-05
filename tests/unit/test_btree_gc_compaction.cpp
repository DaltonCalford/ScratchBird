/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>
#include "scratchbird/core/btree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/tid.h"
#include "test_helpers.h"
#include <filesystem>
#include <cstring>

namespace scratchbird::core::test
{

// ScratchBird MGA GC compaction tests (not PostgreSQL VACUUM).
class BTreeGcCompactionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test database path
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_btree_vacuum", ".db");
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

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
            std::filesystem::remove(test_db_path_);
        }
    }

    std::string test_db_path_;
    Database db_;

    GPID allocateRootGpid(ErrorContext *ctx)
    {
        auto *pm = db_.page_manager();
        if (!pm)
        {
            if (ctx) ctx->message = "PageManager not available";
            return 0;
        }
        GPID gpid = 0;
        auto status = pm->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &gpid, ctx);
        if (status != Status::OK)
        {
            return 0;
        }
        return gpid;
    }
};

TEST_F(BTreeGcCompactionTest, GcCompactionEmptyTree)
{
    ErrorContext ctx;

    // Create a B-tree index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    auto status = BTree::create(&db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create B-tree: " << ctx.message;

    // Open the B-tree
    auto btree = BTree::open(&db_, index_uuid, root_gpid, &ctx);
    ASSERT_NE(btree, nullptr) << "Failed to open B-tree: " << ctx.message;

    // GC compact empty tree
    BTree::GcCompactionStats stats;
    status = btree->gcCompact(&stats, &ctx);
    ASSERT_EQ(status, Status::OK) << "GC compaction failed: " << ctx.message;

    EXPECT_EQ(stats.pages_visited, 1);  // Only root page
    EXPECT_EQ(stats.pages_compacted, 0); // No garbage
    EXPECT_EQ(stats.nodes_removed, 0);
    EXPECT_EQ(stats.bytes_reclaimed, 0);
    EXPECT_EQ(stats.pages_merged, 0);
}

TEST_F(BTreeGcCompactionTest, GcCompactionWithDeletedNodes)
{
    ErrorContext ctx;

    // Create a B-tree index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    auto status = BTree::create(&db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create B-tree: " << ctx.message;

    // Open the B-tree
    auto btree = BTree::open(&db_, index_uuid, root_gpid, &ctx);
    ASSERT_NE(btree, nullptr) << "Failed to open B-tree: " << ctx.message;

    // Insert some test data
    const int num_entries = 20;
    std::vector<std::pair<std::vector<uint8_t>, TID>> test_data;

    for (int i = 0; i < num_entries; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        TID tid{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i + 1)), 1};

        status = btree->insert(key, tid, 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to insert key " << i << ": " << ctx.message;

        test_data.push_back({key, tid});
    }

    // Delete every other entry
    int deleted_count = 0;
    for (int i = 0; i < num_entries; i += 2)
    {
        status = btree->remove(test_data[i].first, test_data[i].second, 2, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to remove key " << i << ": " << ctx.message;
        deleted_count++;
    }

    // GC compact the tree
    BTree::GcCompactionStats stats;
    status = btree->gcCompact(&stats, &ctx);
    ASSERT_EQ(status, Status::OK) << "GC compaction failed: " << ctx.message;

    // Verify GC statistics
    EXPECT_GT(stats.pages_visited, 0) << "Should have visited at least one page";
    EXPECT_GT(stats.pages_compacted, 0) << "Should have compacted at least one page";
    EXPECT_EQ(stats.nodes_removed, deleted_count) << "Should have removed all deleted nodes";
    EXPECT_GT(stats.bytes_reclaimed, 0) << "Should have reclaimed some bytes";

    // Verify remaining entries are still searchable
    for (int i = 1; i < num_entries; i += 2)
    {
        std::vector<TID> tuple_ids;
        status = btree->search(test_data[i].first, 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to search key " << i << ": " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1);
        EXPECT_EQ(tuple_ids[0], test_data[i].second);
    }

    // Verify deleted entries are not found
    for (int i = 0; i < num_entries; i += 2)
    {
        std::vector<TID> tuple_ids;
        status = btree->search(test_data[i].first, 0, &tuple_ids, &ctx);
        EXPECT_EQ(status, Status::NOT_FOUND) << "Deleted key " << i << " should not be found";
    }
}

TEST_F(BTreeGcCompactionTest, CompactionReclaimsSpace)
{
    ErrorContext ctx;

    // Create a B-tree index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    auto status = BTree::create(&db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db_, index_uuid, root_gpid, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert 50 entries
    for (int i = 0; i < 50; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        TID tid{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i + 1)), 1};
        status = btree->insert(key, tid, 1, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Delete 40 entries (80% deletion)
    for (int i = 0; i < 40; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        TID tid{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i + 1)), 1};
        status = btree->remove(key, tid, 2, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // GC compaction should reclaim significant space
    BTree::GcCompactionStats stats;
    status = btree->gcCompact(&stats, &ctx);
    ASSERT_EQ(status, Status::OK);

    EXPECT_EQ(stats.nodes_removed, 40);
    EXPECT_GT(stats.bytes_reclaimed, 0);

    // Verify remaining 10 entries are still accessible
    for (int i = 40; i < 50; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        std::vector<TID> tuple_ids;
        status = btree->search(key, 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(tuple_ids.size(), 1);
        EXPECT_EQ(tuple_ids[0], TID(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i + 1)), 1));
    }
}

TEST_F(BTreeGcCompactionTest, GcCompactionMultipleTimes)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    auto status = BTree::create(&db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(&db_, index_uuid, root_gpid, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert 30 entries
    for (int i = 0; i < 30; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        TID tid{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i + 1)), 1};
        status = btree->insert(key, tid, 1, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Delete 15 entries
    for (int i = 0; i < 15; ++i)
    {
        std::vector<uint8_t> key(sizeof(int));
        std::memcpy(key.data(), &i, sizeof(int));
        TID tid{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i + 1)), 1};
        status = btree->remove(key, tid, 2, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // First GC compaction
    BTree::GcCompactionStats stats1;
    status = btree->gcCompact(&stats1, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(stats1.nodes_removed, 15);

    // Second GC compaction on already cleaned tree
    BTree::GcCompactionStats stats2;
    status = btree->gcCompact(&stats2, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(stats2.nodes_removed, 0) << "Second GC compaction should find no garbage";
    EXPECT_EQ(stats2.bytes_reclaimed, 0) << "Second GC compaction should reclaim no space";
}

} // namespace scratchbird::core::test
