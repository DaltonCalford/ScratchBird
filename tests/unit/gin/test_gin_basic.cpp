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

using namespace scratchbird::core;

class GinIndexTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 8192;

    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_gin_basic", ".db");
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

    // Test helper: Simple key extractor for string arrays
    // Splits string by spaces and returns each word as a key
    static std::vector<std::vector<uint8_t>> extractWordsFromString(const void *data, size_t len)
    {
        std::vector<std::vector<uint8_t>> keys;
        std::string str(static_cast<const char *>(data), len);

        size_t start = 0;
        size_t end = 0;

        while ((end = str.find(' ', start)) != std::string::npos)
        {
            if (end > start)
            {
                std::string word = str.substr(start, end - start);
                std::vector<uint8_t> key(word.begin(), word.end());
                keys.push_back(key);
            }
            start = end + 1;
        }

        // Last word
        if (start < str.length())
        {
            std::string word = str.substr(start);
            std::vector<uint8_t> key(word.begin(), word.end());
            keys.push_back(key);
        }

        return keys;
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
};

TEST_F(GinIndexTest, CreateGinIndex)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);

    ASSERT_EQ(status, Status::OK);
    EXPECT_NE(meta_page, 0u);
}

TEST_F(GinIndexTest, OpenGinIndex)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Open the index
    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);
}

TEST_F(GinIndexTest, InsertPendingList)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert a document (string with multiple words)
    std::string doc1 = "the quick brown fox";
    TID tid1{1, 1}; // gpid 1, slot 1

    status = gin_index->insert(doc1.data(), doc1.length(), tid1,
                               extractWordsFromString, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Insert another document
    std::string doc2 = "the lazy brown dog";
    TID tid2{1, 2}; // gpid 1, slot 2

    status = gin_index->insert(doc2.data(), doc2.length(), tid2,
                               extractWordsFromString, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Check statistics
    auto stats = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats.pending_list_count, 8u); // 4 words from doc1 + 4 from doc2
}

TEST_F(GinIndexTest, PendingListMultiplePages)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Insert many documents to fill multiple pending pages
    // Each page holds 113 entries, so 200 documents with 1 word each = 200 entries
    for (int i = 0; i < 200; i++)
    {
        std::string doc = "word" + std::to_string(i);
        TID tid{1, static_cast<uint16_t>(i + 1)};

        status = gin_index->insert(doc.data(), doc.length(), tid,
                                   extractWordsFromString, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Check statistics
    auto stats = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats.pending_list_count, 200u);
}

TEST_F(GinIndexTest, Statistics)
{
    ErrorContext ctx;

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db_.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto gin_index = GinIndex::open(db_.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(gin_index, nullptr);

    // Get initial statistics
    auto stats1 = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats1.pending_list_count, 0u);
    EXPECT_EQ(stats1.num_keys, 0u);
    EXPECT_EQ(stats1.num_tuples, 0u);

    // Insert documents
    std::string doc = "database indexing performance";
    TID tid{1, 1};

    status = gin_index->insert(doc.data(), doc.length(), tid,
                               extractWordsFromString, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Check updated statistics
    auto stats2 = gin_index->getStatistics(&ctx);
    EXPECT_EQ(stats2.pending_list_count, 3u);
}
