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
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

using namespace scratchbird::core;

// Test helper: Simple key extractor for string arrays
// Splits string by spaces and returns each word as a key
std::vector<std::vector<uint8_t>> extractWordsFromString(const void *data, size_t len)
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

void test_create_gin_index()
{
    std::cout << "Test: Create GIN index\n";

    // Create database
    const char *db_path = "/tmp/test_gin_db";
    std::remove(db_path);

    ErrorContext ctx;
    Database *db = Database::create(db_path, 8192, &ctx);
    assert(db != nullptr);

    // Create GIN index
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db, index_uuid, &meta_page, &ctx);

    assert(status == Status::OK);
    assert(meta_page != 0);

    std::cout << "✓ GIN index created successfully (meta page: " << meta_page << ")\n";

    delete db;
}

void test_open_gin_index()
{
    std::cout << "Test: Open GIN index\n";

    // Create database and index
    const char *db_path = "/tmp/test_gin_db2";
    std::remove(db_path);

    ErrorContext ctx;
    Database *db = Database::create(db_path, 8192, &ctx);
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    // Open the index
    auto gin_index = GinIndex::open(db, index_uuid, meta_page, &ctx);
    assert(gin_index != nullptr);

    std::cout << "✓ GIN index opened successfully\n";

    delete db;
}

void test_gin_insert_pending_list()
{
    std::cout << "Test: Insert into GIN pending list\n";

    // Create database and index
    const char *db_path = "/tmp/test_gin_db3";
    std::remove(db_path);

    ErrorContext ctx;
    Database *db = Database::create(db_path, 8192, &ctx);
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto gin_index = GinIndex::open(db, index_uuid, meta_page, &ctx);
    assert(gin_index != nullptr);

    // Insert a document (string with multiple words)
    std::string doc1 = "the quick brown fox";
    uint64_t tuple_id1 = (1ULL << 32) | 1; // page 1, item 1

    status = gin_index->insert(doc1.data(), doc1.length(), tuple_id1,
                               extractWordsFromString, &ctx);
    assert(status == Status::OK);

    std::cout << "✓ Inserted document 1: \"" << doc1 << "\"\n";

    // Insert another document
    std::string doc2 = "the lazy brown dog";
    uint64_t tuple_id2 = (1ULL << 32) | 2; // page 1, item 2

    status = gin_index->insert(doc2.data(), doc2.length(), tuple_id2,
                               extractWordsFromString, &ctx);
    assert(status == Status::OK);

    std::cout << "✓ Inserted document 2: \"" << doc2 << "\"\n";

    // Check statistics
    auto stats = gin_index->getStatistics(&ctx);
    std::cout << "  Pending list count: " << stats.pending_list_count << "\n";
    assert(stats.pending_list_count == 8); // 4 words from doc1 + 4 from doc2

    std::cout << "✓ GIN insert into pending list works correctly\n";

    delete db;
}

void test_gin_pending_list_multiple_pages()
{
    std::cout << "Test: GIN pending list spanning multiple pages\n";

    const char *db_path = "/tmp/test_gin_db4";
    std::remove(db_path);

    ErrorContext ctx;
    Database *db = Database::create(db_path, 8192, &ctx);
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto gin_index = GinIndex::open(db, index_uuid, meta_page, &ctx);
    assert(gin_index != nullptr);

    // Insert many documents to fill multiple pending pages
    // Each page holds 113 entries, so 200 documents with 1 word each = 200 entries
    std::cout << "  Inserting 200 documents...\n";

    for (int i = 0; i < 200; i++)
    {
        std::string doc = "word" + std::to_string(i);
        uint64_t tuple_id = (1ULL << 32) | (i + 1);

        status = gin_index->insert(doc.data(), doc.length(), tuple_id,
                                   extractWordsFromString, &ctx);
        assert(status == Status::OK);
    }

    // Check statistics
    auto stats = gin_index->getStatistics(&ctx);
    std::cout << "  Pending list count: " << stats.pending_list_count << "\n";
    assert(stats.pending_list_count == 200);

    std::cout << "✓ GIN pending list correctly spans multiple pages\n";

    delete db;
}

void test_gin_statistics()
{
    std::cout << "Test: GIN statistics\n";

    const char *db_path = "/tmp/test_gin_db5";
    std::remove(db_path);

    ErrorContext ctx;
    Database *db = Database::create(db_path, 8192, &ctx);
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    Status status = GinIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto gin_index = GinIndex::open(db, index_uuid, meta_page, &ctx);
    assert(gin_index != nullptr);

    // Get initial statistics
    auto stats1 = gin_index->getStatistics(&ctx);
    assert(stats1.pending_list_count == 0);
    assert(stats1.num_keys == 0);
    assert(stats1.num_tuples == 0);

    std::cout << "  Initial stats - pending: " << stats1.pending_list_count
              << ", keys: " << stats1.num_keys << ", tuples: " << stats1.num_tuples << "\n";

    // Insert documents
    std::string doc = "database indexing performance";
    uint64_t tuple_id = (1ULL << 32) | 1;

    status = gin_index->insert(doc.data(), doc.length(), tuple_id,
                               extractWordsFromString, &ctx);
    assert(status == Status::OK);

    // Check updated statistics
    auto stats2 = gin_index->getStatistics(&ctx);
    assert(stats2.pending_list_count == 3);

    std::cout << "  After insert - pending: " << stats2.pending_list_count
              << ", keys: " << stats2.num_keys << ", tuples: " << stats2.num_tuples << "\n";

    std::cout << "✓ GIN statistics working correctly\n";

    delete db;
}

int main()
{
    std::cout << "=== GIN Index Phase 1 Tests ===\n\n";

    try
    {
        test_create_gin_index();
        std::cout << "\n";

        test_open_gin_index();
        std::cout << "\n";

        test_gin_insert_pending_list();
        std::cout << "\n";

        test_gin_pending_list_multiple_pages();
        std::cout << "\n";

        test_gin_statistics();
        std::cout << "\n";

        std::cout << "=== All GIN Phase 1 tests passed! ===\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
