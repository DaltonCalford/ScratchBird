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
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/hash_functions.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/transaction_manager.h"
#include "test_helpers.h"
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <cstdlib>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

namespace
{
    struct EntryLocation
    {
        bool found = false;
        uint32_t page_id = 0;
        bool reachable_from_directory = false;
    };

    bool scanIndexForEntry(Database *db, GPID meta_gpid, uint64_t hash, const TID &tid, ErrorContext *ctx)
    {
        if (!db)
        {
            return false;
        }

        BufferPool *buffer_pool = db->buffer_pool();
        if (!buffer_pool)
        {
            return false;
        }

        void *meta_buffer = nullptr;
        Status status = buffer_pool->pinPageGlobal(meta_gpid, &meta_buffer, ctx);
        if (status != Status::OK)
        {
            return false;
        }

        auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_buffer);
        uint32_t global_depth = meta->hip_global_depth;
        uint64_t directory_page = meta->hip_directory_page;
        uint16_t tablespace_id = getTablespaceID(meta_gpid);

        buffer_pool->unpinPageGlobal(meta_gpid, false, ctx);

        uint32_t num_buckets = 1U << global_depth;
        size_t pointers_per_page =
            (db->page_size() - sizeof(SBHashDirectoryPage)) / sizeof(uint64_t);
        if (pointers_per_page == 0)
        {
            return false;
        }

        std::set<uint64_t> visited;
        uint64_t current_dir_page = directory_page;
        uint32_t processed = 0;

        while (current_dir_page != 0 && processed < num_buckets)
        {
            void *dir_buffer = nullptr;
            status = buffer_pool->pinPageGlobal(makeGPID(tablespace_id, current_dir_page), &dir_buffer, ctx);
            if (status != Status::OK)
            {
                return false;
            }

            auto *dir_page = reinterpret_cast<SBHashDirectoryPage *>(dir_buffer);
            uint32_t entries_this_page =
                static_cast<uint32_t>(std::min<size_t>(pointers_per_page, num_buckets - processed));

            for (uint32_t i = 0; i < entries_this_page; i++)
            {
                uint64_t bucket_page = dir_page->hdp_bucket_pointers[i];
                if (visited.insert(bucket_page).second)
                {
                    uint64_t current_bucket = bucket_page;
                    while (current_bucket != 0)
                    {
                        void *bucket_buffer = nullptr;
                        status = buffer_pool->pinPageGlobal(makeGPID(tablespace_id, current_bucket),
                                                            &bucket_buffer, ctx);
                        if (status != Status::OK)
                        {
                            buffer_pool->unpinPageGlobal(makeGPID(tablespace_id, current_dir_page), false, ctx);
                            return false;
                        }

                        auto *bucket = reinterpret_cast<SBHashBucketPage *>(bucket_buffer);
                        uint16_t entry_count = bucket->hbp_entry_count;
                        uint64_t overflow_page = bucket->hbp_overflow_page;

                        for (uint16_t j = 0; j < entry_count; j++)
                        {
                            const HashEntry &entry = bucket->hbp_entries[j];
                            if (entry.he_key_hash == hash && entry.getTID() == tid)
                            {
                                buffer_pool->unpinPageGlobal(makeGPID(tablespace_id, current_bucket), false, ctx);
                                buffer_pool->unpinPageGlobal(makeGPID(tablespace_id, current_dir_page), false, ctx);
                                return true;
                            }
                        }

                        buffer_pool->unpinPageGlobal(makeGPID(tablespace_id, current_bucket), false, ctx);
                        current_bucket = overflow_page;
                    }
                }
            }

            uint64_t next_dir_page = dir_page->hdp_next_page;
            buffer_pool->unpinPageGlobal(makeGPID(tablespace_id, current_dir_page), false, ctx);
            current_dir_page = next_dir_page;
            processed += entries_this_page;
        }

        return false;
    }

    EntryLocation scanAllAllocatedPagesForEntry(Database *db,
                                                const std::string &db_path,
                                                uint64_t hash,
                                                const TID &tid,
                                                ErrorContext *ctx)
    {
        EntryLocation location;
        if (!db)
        {
            return location;
        }

        BufferPool *buffer_pool = db->buffer_pool();
        PageManager *page_manager = db->page_manager();
        if (!buffer_pool || !page_manager)
        {
            return location;
        }

        std::error_code size_error;
        uint64_t file_size = static_cast<uint64_t>(std::filesystem::file_size(db_path, size_error));
        if (size_error || db->page_size() == 0)
        {
            return location;
        }

        uint64_t total_pages = file_size / db->page_size();
        for (uint64_t page_id = 0; page_id < total_pages; ++page_id)
        {
            GPID gpid = makeGPID(PRIMARY_TABLESPACE_ID, page_id);
            if (!page_manager->isAllocatedGlobal(gpid))
            {
                continue;
            }

            void *page_buffer = nullptr;
            Status status = buffer_pool->pinPageGlobal(gpid, &page_buffer, ctx);
            if (status != Status::OK)
            {
                continue;
            }

            auto *header = reinterpret_cast<PageHeader *>(page_buffer);
            if (header->page_type == static_cast<uint16_t>(PageType::PAGE_TYPE_HASH_BUCKET))
            {
                auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_buffer);
                for (uint16_t i = 0; i < bucket->hbp_entry_count; ++i)
                {
                    const HashEntry &entry = bucket->hbp_entries[i];
                    if (entry.he_key_hash == hash && entry.getTID() == tid)
                    {
                        location.found = true;
                        location.page_id = static_cast<uint32_t>(page_id);
                        buffer_pool->unpinPageGlobal(gpid, false, ctx);
                        return location;
                    }
                }
            }

            buffer_pool->unpinPageGlobal(gpid, false, ctx);
        }

        return location;
    }
} // namespace

class HashIndexTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test database
        test_db_path = uniqueTestDbPath("test_hash_index");

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

    GPID allocateMetaGpid(ErrorContext *ctx)
    {
        auto *pm = db ? db->page_manager() : nullptr;
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

    uint32_t metaPageId(GPID gpid) const
    {
        uint32_t page_id = 0;
        convertGPIDtoPageID(gpid, &page_id);
        return page_id;
    }

    std::string test_db_path;
    std::unique_ptr<Database> db;
};

// Test 1: Create hash index
TEST_F(HashIndexTest, CreateIndex)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    GPID meta_gpid = allocateMetaGpid(&ctx);

    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create hash index: " << ctx.message;
    ASSERT_GT(metaPageId(meta_gpid), 0u);
}

// Test 2: Open existing hash index
TEST_F(HashIndexTest, OpenIndex)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    GPID meta_gpid = allocateMetaGpid(&ctx);

    // Create index
    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Open index
    auto index = HashIndex::open(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_NE(index, nullptr) << "Failed to open hash index: " << ctx.message;
    ASSERT_EQ(index->getIndexUuid(), index_uuid);
    ASSERT_EQ(index->getMetaPage(), metaPageId(meta_gpid));
}

// Test 3: Insert single entry
TEST_F(HashIndexTest, InsertSingle)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    GPID meta_gpid = allocateMetaGpid(&ctx);

    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert entry
    const char* key = "test_key";
    TID tuple_id{makeGPID(PRIMARY_TABLESPACE_ID, 1), 1};

    status = index->insert(key, std::strlen(key), tuple_id, 1, &ctx);
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
    GPID meta_gpid = allocateMetaGpid(&ctx);

    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert multiple entries
    const char* key1 = "key1";
    const char* key2 = "key2";
    const char* key3 = "key3";

    TID tid1{makeGPID(PRIMARY_TABLESPACE_ID, 1), 1};
    TID tid2{makeGPID(PRIMARY_TABLESPACE_ID, 1), 2};
    TID tid3{makeGPID(PRIMARY_TABLESPACE_ID, 1), 3};

    ASSERT_EQ(index->insert(key1, std::strlen(key1), tid1, 1, &ctx), Status::OK);
    ASSERT_EQ(index->insert(key2, std::strlen(key2), tid2, 1, &ctx), Status::OK);
    ASSERT_EQ(index->insert(key3, std::strlen(key3), tid3, 1, &ctx), Status::OK);

    // Find entries
    std::vector<TID> results1;
    ASSERT_EQ(index->find(key1, std::strlen(key1), 0, &results1, &ctx), Status::OK);
    ASSERT_EQ(results1.size(), 1);
    ASSERT_EQ(results1[0], tid1);

    std::vector<TID> results2;
    ASSERT_EQ(index->find(key2, std::strlen(key2), 0, &results2, &ctx), Status::OK);
    ASSERT_EQ(results2.size(), 1);
    ASSERT_EQ(results2[0], tid2);

    std::vector<TID> results3;
    ASSERT_EQ(index->find(key3, std::strlen(key3), 0, &results3, &ctx), Status::OK);
    ASSERT_EQ(results3.size(), 1);
    ASSERT_EQ(results3[0], tid3);

    // Find non-existent key
    const char* key4 = "key4";
    std::vector<TID> results4;
    ASSERT_EQ(index->find(key4, std::strlen(key4), 0, &results4, &ctx), Status::OK);
    ASSERT_EQ(results4.size(), 0);
}

// Test 5: Insert duplicates (same key, different tuple IDs)
TEST_F(HashIndexTest, InsertDuplicates)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    GPID meta_gpid = allocateMetaGpid(&ctx);

    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert same key with different tuple IDs
    const char* key = "duplicate_key";
    TID tid1{makeGPID(PRIMARY_TABLESPACE_ID, 1), 1};
    TID tid2{makeGPID(PRIMARY_TABLESPACE_ID, 1), 2};
    TID tid3{makeGPID(PRIMARY_TABLESPACE_ID, 1), 3};

    ASSERT_EQ(index->insert(key, std::strlen(key), tid1, 1, &ctx), Status::OK);
    ASSERT_EQ(index->insert(key, std::strlen(key), tid2, 1, &ctx), Status::OK);
    ASSERT_EQ(index->insert(key, std::strlen(key), tid3, 1, &ctx), Status::OK);

    // Find should return all three
    std::vector<TID> results;
    ASSERT_EQ(index->find(key, std::strlen(key), 0, &results, &ctx), Status::OK);
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
    GPID meta_gpid = allocateMetaGpid(&ctx);

    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert entry
    const char* key = "test_key";
    TID tuple_id{makeGPID(PRIMARY_TABLESPACE_ID, 1), 1};

    ASSERT_EQ(index->insert(key, std::strlen(key), tuple_id, 1, &ctx), Status::OK);

    // Verify it exists
    std::vector<TID> results;
    uint64_t current_xid = db->transaction_manager()->getCurrentXid();
    ASSERT_EQ(index->find(key, std::strlen(key), current_xid, &results, &ctx), Status::OK);
    ASSERT_EQ(results.size(), 1);

    // Delete entry
    status = index->remove(key, std::strlen(key), tuple_id, 2, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify it's gone
    results.clear();
    ASSERT_EQ(index->find(key, std::strlen(key), current_xid, &results, &ctx), Status::OK);
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
    GPID meta_gpid = allocateMetaGpid(&ctx);

    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_NE(index, nullptr);

    // Get initial statistics
    auto stats_before = index->getStatistics(&ctx);
    uint32_t initial_depth = stats_before.global_depth;

    // Insert many entries to trigger splits
    const int num_entries = 1000;
    for (int i = 0; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        TID tuple_id{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i)), 1};

        status = index->insert(key.c_str(), key.length(), tuple_id, 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to insert entry " << i;
    }

    // Verify all entries can be found
    for (int i = 0; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        std::vector<TID> results;
    ASSERT_EQ(index->find(key.c_str(), key.length(), 0, &results, &ctx), Status::OK);
        ASSERT_EQ(results.size(), 1) << "Failed to find entry " << i;
        ASSERT_EQ(results[0], TID(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i)), 1));
    }

    // Verify statistics
    auto stats_after = index->getStatistics(&ctx);
    ASSERT_EQ(stats_after.num_tuples, num_entries);
    ASSERT_GE(stats_after.global_depth, initial_depth); // Depth should increase
}

// Test 8: GC compaction operation
TEST_F(HashIndexTest, GcCompaction)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    GPID meta_gpid = allocateMetaGpid(&ctx);

    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert entries
    const int num_entries = 100;
    for (int i = 0; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        TID tuple_id{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i)), 1};
        ASSERT_EQ(index->insert(key.c_str(), key.length(), tuple_id, 1, &ctx), Status::OK);
    }

    // Delete half of them
    for (int i = 0; i < num_entries / 2; i++)
    {
        std::string key = "key_" + std::to_string(i);
        TID tuple_id{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i)), 1};
        ASSERT_EQ(index->remove(key.c_str(), key.length(), tuple_id, 2, &ctx), Status::OK);
    }

    // Verify deleted count
    auto stats_before = index->getStatistics(&ctx);
    ASSERT_EQ(stats_before.num_deleted, num_entries / 2);

    // Run GC compaction
    status = index->gcCompact(&ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify deleted count is reduced
    auto stats_after = index->getStatistics(&ctx);
    ASSERT_EQ(stats_after.num_deleted, 0);

    // Verify remaining entries are still findable
    for (int i = num_entries / 2; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        std::vector<TID> results;
    ASSERT_EQ(index->find(key.c_str(), key.length(), 0, &results, &ctx), Status::OK);
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
    Logger::getInstance().setLogLevel(LogLevel::DEBUG);
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    GPID meta_gpid = allocateMetaGpid(&ctx);

    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert 10,000 entries
    const int num_entries = 10000;
    for (int i = 0; i < num_entries; i++)
    {
        std::string key = "key_" + std::to_string(i);
        TID tuple_id{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i)), 1};

        status = index->insert(key.c_str(), key.length(), tuple_id, 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed at entry " << i;
    }

    // Verify statistics
    auto stats = index->getStatistics(&ctx);
    ASSERT_EQ(stats.num_tuples, num_entries);
    ASSERT_GT(stats.global_depth, 4); // Should have grown beyond initial depth

    // Random access test - verify 1000 random entries
    std::srand(1);
    for (int i = 0; i < 1000; i++)
    {
        int idx = rand() % num_entries;
        std::string key = "key_" + std::to_string(idx);
        std::vector<TID> results;
        ASSERT_EQ(index->find(key.c_str(), key.length(), 0, &results, &ctx), Status::OK);
        if (results.size() != 1)
        {
            uint64_t hash = MurmurHash64(key.c_str(), key.length());
            bool found_elsewhere = scanIndexForEntry(db.get(), meta_gpid, hash,
                                                     TID(makeGPID(PRIMARY_TABLESPACE_ID,
                                                                  static_cast<uint64_t>(idx)),
                                                         1),
                                                     &ctx);
            EntryLocation any_location =
                scanAllAllocatedPagesForEntry(db.get(),
                                              test_db_path,
                                              hash,
                                              TID(makeGPID(PRIMARY_TABLESPACE_ID,
                                                           static_cast<uint64_t>(idx)),
                                                  1),
                                              &ctx);
            FAIL() << "Missing idx=" << idx
                   << " global_depth=" << stats.global_depth
                   << " found_elsewhere=" << (found_elsewhere ? "true" : "false")
                   << " found_anywhere=" << (any_location.found ? "true" : "false")
                   << " page_id=" << any_location.page_id;
        }
        ASSERT_EQ(results[0], TID(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(idx)), 1));
    }
}

// Test 11: Statistics accuracy
TEST_F(HashIndexTest, StatisticsAccuracy)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    GPID meta_gpid = allocateMetaGpid(&ctx);

    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_gpid, &ctx);
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
        TID tuple_id{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i)), 1};
        index->insert(key.c_str(), key.length(), tuple_id, 1, &ctx);
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
    GPID meta_gpid = allocateMetaGpid(&ctx);

    Status status = HashIndex::create(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert with null key
    status = index->insert(nullptr, 10, TID(makeGPID(PRIMARY_TABLESPACE_ID, 1), 1), 1, &ctx);
    ASSERT_EQ(status, Status::INVALID_ARGUMENT);

    // Insert with zero length
    const char* key = "test";
    status = index->insert(key, 0, TID(makeGPID(PRIMARY_TABLESPACE_ID, 1), 1), 1, &ctx);
    ASSERT_EQ(status, Status::INVALID_ARGUMENT);

    // Insert with zero tuple ID
    status = index->insert(key, 4, TID(makeGPID(PRIMARY_TABLESPACE_ID, 0), 0), 1, &ctx);
    ASSERT_EQ(status, Status::INVALID_ARGUMENT);

    // Remove non-existent entry
    status = index->remove(key, 4, TID(makeGPID(PRIMARY_TABLESPACE_ID, 1), 1), 2, &ctx);
    ASSERT_EQ(status, Status::NOT_FOUND);
}
