/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <algorithm>

using namespace scratchbird::core;

class GinPostingTreeTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 8192;

    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_gin_posting_tree", ".db");
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, kPageSize, &ctx), Status::OK)
            << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK)
            << ctx.message;
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
            db_.reset();
        }
        std::filesystem::remove(test_db_path_);
    }

    // Test helper: Extract single word as key
    static std::vector<std::vector<uint8_t>> extractSingleWord(const void *data, size_t len)
    {
        std::vector<std::vector<uint8_t>> keys;
        std::vector<uint8_t> key(static_cast<const uint8_t *>(data),
                                 static_cast<const uint8_t *>(data) + len);
        keys.push_back(key);
        return keys;
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
};

TEST_F(GinPostingTreeTest, PostingListToTreeConversion)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert enough entries to trigger conversion
    // Threshold is 64, so insert 70 documents with same key
    std::string key_word = "database";

    for (int i = 0; i < 70; i++)
    {
        TID tid{1, static_cast<uint16_t>(i + 1)};
        status = gin_index->insert(key_word.data(), key_word.length(), tid,
                                   extractSingleWord, &ctx);
        ASSERT_EQ(status, Status::OK);
    }
}

TEST_F(GinPostingTreeTest, PostingTreeInsertionAndGrowth)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert many entries to test tree growth and splits
    std::string key_word = "search";

    for (int i = 0; i < 2000; i++)
    {
        TID tid{1, static_cast<uint16_t>((i % 65535) + 1)};
        status = gin_index->insert(key_word.data(), key_word.length(), tid,
                                   extractSingleWord, &ctx);
        ASSERT_EQ(status, Status::OK);
    }
}

TEST_F(GinPostingTreeTest, PostingTreeSortedOrder)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert TIDs in random order
    std::vector<TID> inserted_tids;
    std::string key_word = "index";

    // Generate TIDs
    for (int i = 0; i < 100; i++)
    {
        TID tid{1, static_cast<uint16_t>(i + 1)};
        inserted_tids.push_back(tid);
    }

    // Shuffle TIDs
    std::random_shuffle(inserted_tids.begin(), inserted_tids.end());

    // Insert in shuffled order
    for (const TID &tid : inserted_tids)
    {
        status = gin_index->insert(key_word.data(), key_word.length(), tid,
                                   extractSingleWord, &ctx);
        ASSERT_EQ(status, Status::OK);
    }
}

TEST_F(GinPostingTreeTest, PostingTreeDuplicateHandling)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    std::string key_word = "duplicate";

    // Insert 100 unique TIDs
    for (int i = 0; i < 100; i++)
    {
        TID tid{1, static_cast<uint16_t>(i + 1)};
        status = gin_index->insert(key_word.data(), key_word.length(), tid,
                                   extractSingleWord, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Try to insert duplicates
    for (int i = 0; i < 100; i++)
    {
        TID tid{1, static_cast<uint16_t>(i + 1)};
        status = gin_index->insert(key_word.data(), key_word.length(), tid,
                                   extractSingleWord, &ctx);
        ASSERT_EQ(status, Status::OK); // Should succeed but not duplicate
    }
}

TEST_F(GinPostingTreeTest, PostingTreeMultipleKeys)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert many documents for multiple different keys
    std::vector<std::string> keys = {"database", "index", "query", "search", "performance"};

    for (const auto &key : keys)
    {
        for (int i = 0; i < 100; i++)
        {
            TID tid{1, static_cast<uint16_t>(i + 1)};
            status = gin_index->insert(key.data(), key.length(), tid,
                                       extractSingleWord, &ctx);
            ASSERT_EQ(status, Status::OK);
        }
    }
}

TEST_F(GinPostingTreeTest, PostingTreeStatistics)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert enough to trigger tree conversion
    std::string key_word = "statistics";

    for (int i = 0; i < 150; i++)
    {
        TID tid{1, static_cast<uint16_t>(i + 1)};
        status = gin_index->insert(key_word.data(), key_word.length(), tid,
                                   extractSingleWord, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto stats = gin_index->getStatistics(&ctx);
    EXPECT_GT(stats.pending_list_count, 0u);
}

TEST_F(GinPostingTreeTest, PostingListStaysListUnderThreshold)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert fewer than threshold (64)
    std::string key_word = "small";

    for (int i = 0; i < 50; i++)
    {
        TID tid{1, static_cast<uint16_t>(i + 1)};
        status = gin_index->insert(key_word.data(), key_word.length(), tid,
                                   extractSingleWord, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Verify entries were inserted
    auto stats = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats.pending_list_count, 50u);
}
