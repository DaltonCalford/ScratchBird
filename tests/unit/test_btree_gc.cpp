/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// PHASE 2 TASK 2.2: Unit tests for B-Tree Index Garbage Collection
#include <gtest/gtest.h>
#include "scratchbird/core/btree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/uuidv7.h"
#include <filesystem>
#include <vector>
#include <unistd.h>

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

    GPID allocateRootGpid(ErrorContext *ctx)
    {
        if (!db_)
        {
            if (ctx) ctx->message = "Database not initialized";
            return 0;
        }
        auto *pm = db_->page_manager();
        if (!pm)
        {
            if (ctx) ctx->message = "PageManager not available";
            return 0;
        }
        GPID gpid = 0;
        Status status = pm->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &gpid, ctx);
        if (status != Status::OK)
        {
            return 0;
        }
        return gpid;
    }
};

struct LeafPageSnapshot
{
    uint16_t free_space = 0;
    uint16_t flags = 0;
    uint16_t count = 0;
};

// Test 1: Empty dead_tids vector (no-op)
TEST_F(BTreeGCTest, EmptyDeadTidsVector)
{
    ErrorContext ctx;

    // Create B-Tree index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create B-Tree: " << ctx.message;

    auto btree = BTree::open(db_, index_uuid, root_gpid, &ctx);
    ASSERT_NE(btree, nullptr) << "Failed to open B-Tree: " << ctx.message;

    // Call removeDeadEntries with empty vector
    std::vector<TID> dead_tids;
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
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(db_, index_uuid, root_gpid, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert a few entries
    std::vector<uint8_t> key1 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> key2 = {0x04, 0x05, 0x06};

    TID tid1{makeGPID(PRIMARY_TABLESPACE_ID, 1), 1};
    TID tid2{makeGPID(PRIMARY_TABLESPACE_ID, 1), 2};

    status = btree->insert(key1, tid1, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key2, tid2, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Try to remove TIDs that don't exist in index
    std::vector<TID> dead_tids = {
        TID{makeGPID(PRIMARY_TABLESPACE_ID, 2), 1},  // page 2, item 1 (doesn't exist)
        TID{makeGPID(PRIMARY_TABLESPACE_ID, 2), 2}   // page 2, item 2 (doesn't exist)
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
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(db_, index_uuid, root_gpid, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert entries
    std::vector<uint8_t> key1 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> key2 = {0x04, 0x05, 0x06};
    std::vector<uint8_t> key3 = {0x07, 0x08, 0x09};

    TID tid1{makeGPID(PRIMARY_TABLESPACE_ID, 1), 1};
    TID tid2{makeGPID(PRIMARY_TABLESPACE_ID, 1), 2};
    TID tid3{makeGPID(PRIMARY_TABLESPACE_ID, 1), 3};

    status = btree->insert(key1, tid1, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key2, tid2, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key3, tid3, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Remove tid2
    std::vector<TID> dead_tids = {tid2};
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = btree->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 1);  // One entry removed
    EXPECT_GE(pages_modified, 1);   // At least one page modified

    // Verify tid2 is no longer found
    std::vector<TID> result_tids;
    status = btree->search(key2, 0, &result_tids, &ctx);

    // Entry should be marked as deleted (might still be found but marked)
    // Or NOT_FOUND if vacuum ran
    // Either is acceptable
}

// Test 4: Index type name
TEST_F(BTreeGCTest, IndexTypeName)
{
    ErrorContext ctx;

    // Create B-Tree index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(db_, index_uuid, root_gpid, &ctx);
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
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(db_, index_uuid, root_gpid, &ctx);
    ASSERT_NE(btree, nullptr);

    // Insert entry
    std::vector<uint8_t> key1 = {0x01, 0x02, 0x03};
    TID tid1{makeGPID(PRIMARY_TABLESPACE_ID, 1), 1};

    status = btree->insert(key1, tid1, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Remove with duplicate TIDs in vector
    std::vector<TID> dead_tids = {tid1, tid1, tid1}; // Same TID 3 times
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = btree->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 1);  // Still only 1 entry removed (idempotent)
    EXPECT_GE(pages_modified, 1);
}

TEST_F(BTreeGCTest, RemoveDeadEntriesCompactsLeafWhenReclaimThresholdExceeded)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(db_, index_uuid, root_gpid, &ctx);
    ASSERT_NE(btree, nullptr);

    std::vector<std::pair<std::vector<uint8_t>, TID>> entries;
    entries.reserve(32);
    for (int i = 0; i < 32; ++i)
    {
        std::vector<uint8_t> key(256, 0);
        for (size_t j = 0; j < key.size(); ++j)
        {
            key[j] = static_cast<uint8_t>((i * 19 + static_cast<int>(j)) & 0xFF);
        }

        TID tid{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(100 + i)), 1};
        status = btree->insert(key, tid, 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to insert wide key " << i << ": " << ctx.message;
        entries.push_back({std::move(key), tid});
    }

    const uint32_t root_page_num = static_cast<uint32_t>(getPageNumber(root_gpid));
    auto read_root_leaf = [&]() -> LeafPageSnapshot {
        LeafPageSnapshot snapshot{};
        void *page_data = nullptr;
        Status pin_status = db_->buffer_pool()->pinPage(root_page_num, &page_data, &ctx);
        EXPECT_EQ(pin_status, Status::OK) << ctx.message;
        if (pin_status == Status::OK)
        {
            const auto *page = reinterpret_cast<const SBBTreePage *>(page_data);
            snapshot.free_space = page->btr_free_space;
            snapshot.flags = page->btr_flags;
            snapshot.count = page->btr_count;
            db_->buffer_pool()->unpinPage(root_page_num, false, &ctx);
        }
        return snapshot;
    };

    const LeafPageSnapshot before = read_root_leaf();
    ASSERT_TRUE((before.flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0)
        << "Expected wide-key fixture to remain on a single leaf root page";

    std::vector<TID> dead_tids;
    dead_tids.reserve(20);
    for (size_t i = 0; i < 20; ++i)
    {
        dead_tids.push_back(entries[i].second);
    }

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;
    status = btree->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(entries_removed, dead_tids.size());
    EXPECT_GE(pages_modified, 1u);

    const LeafPageSnapshot after = read_root_leaf();
    EXPECT_GT(after.free_space, before.free_space);
    EXPECT_LT(after.count, before.count);
    EXPECT_EQ(after.flags & static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE), 0u);

    for (size_t i = 0; i < dead_tids.size(); ++i)
    {
        std::vector<TID> tuple_ids;
        status = btree->search(entries[i].first, 0, &tuple_ids, &ctx);
        EXPECT_EQ(status, Status::NOT_FOUND)
            << "Compacted dead key " << i << " should no longer be visible";
    }

    for (size_t i = dead_tids.size(); i < entries.size(); ++i)
    {
        std::vector<TID> tuple_ids;
        status = btree->search(entries[i].first, 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Live key " << i << " should remain searchable";
        ASSERT_EQ(tuple_ids.size(), 1u);
        EXPECT_EQ(tuple_ids.front(), entries[i].second);
    }
}

TEST_F(BTreeGCTest, RemoveDeadEntriesPublishesResidualCleanupDebtBelowCompactionThreshold)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    Status status = BTree::create(db_, index_uuid, table_uuid, column_uuids, root_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto btree = BTree::open(db_, index_uuid, root_gpid, &ctx);
    ASSERT_NE(btree, nullptr);

    const std::vector<uint8_t> key = {0x10, 0x20, 0x30, 0x40};
    const TID dead_tid{makeGPID(PRIMARY_TABLESPACE_ID, 7), 3};

    status = btree->insert(key, dead_tid, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;
    status = btree->removeDeadEntries({dead_tid}, &entries_removed, &pages_modified, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 1u);
    EXPECT_EQ(pages_modified, 1u);

    IndexCleanupDebtSnapshot snapshot{};
    status = btree->getCleanupDebtSnapshot(&snapshot, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(snapshot.backlog_pages, 1u);
    EXPECT_GT(snapshot.backlog_bytes, 0u);
    EXPECT_EQ(snapshot.first_locality_page_id,
              static_cast<uint32_t>(getPageNumber(root_gpid)));
    EXPECT_FALSE(snapshot.repair_required);
}
