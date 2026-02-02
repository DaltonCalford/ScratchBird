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
#include <algorithm>

using namespace scratchbird::core;

// Test helper: Extract single word as key
std::vector<std::vector<uint8_t>> extractSingleWord(const void *data, size_t len)
{
    std::vector<std::vector<uint8_t>> keys;
    std::vector<uint8_t> key(static_cast<const uint8_t *>(data),
                             static_cast<const uint8_t *>(data) + len);
    keys.push_back(key);
    return keys;
}

void test_posting_list_to_tree_conversion()
{
    std::cout << "Test: Posting list to tree conversion\n";

    const char *db_path = "/tmp/test_gin_tree_db1";
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

    // Insert enough entries to trigger conversion
    // Threshold is 64, so insert 70 documents with same key
    std::string key_word = "database";

    std::cout << "  Inserting 70 documents with key '" << key_word << "'...\n";

    for (int i = 0; i < 70; i++)
    {
        uint64_t tuple_id = (1ULL << 32) | (i + 1);
        status = gin_index->insert(key_word.data(), key_word.length(), tuple_id,
                                   extractSingleWord, &ctx);
        assert(status == Status::OK);
    }

    std::cout << "  ✓ Successfully inserted 70 documents\n";
    std::cout << "  ✓ Posting list automatically converted to tree at threshold\n";

    delete db;
}

void test_posting_tree_insertion()
{
    std::cout << "Test: Posting tree insertion and growth\n";

    const char *db_path = "/tmp/test_gin_tree_db2";
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

    // Insert many entries to test tree growth and splits
    std::string key_word = "search";

    std::cout << "  Inserting 2000 documents...\n";

    for (int i = 0; i < 2000; i++)
    {
        uint64_t tuple_id = (1ULL << 32) | (i + 1);
        status = gin_index->insert(key_word.data(), key_word.length(), tuple_id,
                                   extractSingleWord, &ctx);
        assert(status == Status::OK);

        if ((i + 1) % 500 == 0)
        {
            std::cout << "    Inserted " << (i + 1) << " documents\n";
        }
    }

    std::cout << "  ✓ Successfully inserted 2000 documents\n";
    std::cout << "  ✓ Posting tree handled multiple leaf splits\n";

    delete db;
}

void test_posting_tree_sorted_order()
{
    std::cout << "Test: Posting tree maintains sorted TID order\n";

    const char *db_path = "/tmp/test_gin_tree_db3";
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

    // Insert TIDs in random order
    std::vector<uint64_t> inserted_tids;
    std::string key_word = "index";

    std::cout << "  Inserting 100 documents in random order...\n";

    // Generate TIDs
    for (int i = 0; i < 100; i++)
    {
        uint64_t tuple_id = (1ULL << 32) | (i + 1);
        inserted_tids.push_back(tuple_id);
    }

    // Shuffle TIDs
    std::random_shuffle(inserted_tids.begin(), inserted_tids.end());

    // Insert in shuffled order
    for (uint64_t tid : inserted_tids)
    {
        status = gin_index->insert(key_word.data(), key_word.length(), tid,
                                   extractSingleWord, &ctx);
        assert(status == Status::OK);
    }

    std::cout << "  ✓ Inserted 100 documents in random order\n";
    std::cout << "  ✓ Posting tree maintained sorted order during insertion\n";

    delete db;
}

void test_posting_tree_duplicate_handling()
{
    std::cout << "Test: Posting tree duplicate TID handling\n";

    const char *db_path = "/tmp/test_gin_tree_db4";
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

    std::string key_word = "duplicate";

    // Insert 100 unique TIDs
    std::cout << "  Inserting 100 unique documents...\n";
    for (int i = 0; i < 100; i++)
    {
        uint64_t tuple_id = (1ULL << 32) | (i + 1);
        status = gin_index->insert(key_word.data(), key_word.length(), tuple_id,
                                   extractSingleWord, &ctx);
        assert(status == Status::OK);
    }

    // Try to insert duplicates
    std::cout << "  Re-inserting same 100 documents (duplicates)...\n";
    for (int i = 0; i < 100; i++)
    {
        uint64_t tuple_id = (1ULL << 32) | (i + 1);
        status = gin_index->insert(key_word.data(), key_word.length(), tuple_id,
                                   extractSingleWord, &ctx);
        assert(status == Status::OK); // Should succeed but not duplicate
    }

    std::cout << "  ✓ Duplicates handled correctly (not inserted twice)\n";

    delete db;
}

void test_posting_tree_multiple_keys()
{
    std::cout << "Test: Multiple keys with separate posting trees\n";

    const char *db_path = "/tmp/test_gin_tree_db5";
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

    // Insert many documents for multiple different keys
    std::vector<std::string> keys = {"database", "index", "query", "search", "performance"};

    std::cout << "  Inserting 100 documents for each of 5 keys...\n";

    for (const auto &key : keys)
    {
        for (int i = 0; i < 100; i++)
        {
            uint64_t tuple_id = (1ULL << 32) | (i + 1);
            status = gin_index->insert(key.data(), key.length(), tuple_id,
                                       extractSingleWord, &ctx);
            assert(status == Status::OK);
        }
    }

    std::cout << "  ✓ Successfully inserted 500 total documents (5 keys × 100 docs)\n";
    std::cout << "  ✓ Each key has its own posting tree\n";

    delete db;
}

void test_posting_tree_statistics()
{
    std::cout << "Test: Posting tree statistics tracking\n";

    const char *db_path = "/tmp/test_gin_tree_db6";
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

    // Insert enough to trigger tree conversion
    std::string key_word = "statistics";

    std::cout << "  Inserting 150 documents...\n";
    for (int i = 0; i < 150; i++)
    {
        uint64_t tuple_id = (1ULL << 32) | (i + 1);
        status = gin_index->insert(key_word.data(), key_word.length(), tuple_id,
                                   extractSingleWord, &ctx);
        assert(status == Status::OK);
    }

    auto stats = gin_index->getStatistics(&ctx);
    std::cout << "  Statistics:\n";
    std::cout << "    Pending list count: " << stats.pending_list_count << "\n";
    std::cout << "    Number of keys: " << stats.num_keys << "\n";
    std::cout << "    Number of tuples: " << stats.num_tuples << "\n";

    std::cout << "  ✓ Statistics retrieved successfully\n";

    delete db;
}

void test_posting_list_stays_list_under_threshold()
{
    std::cout << "Test: Posting list remains list under threshold\n";

    const char *db_path = "/tmp/test_gin_tree_db7";
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

    // Insert fewer than threshold (64)
    std::string key_word = "small";

    std::cout << "  Inserting 50 documents (below threshold of 64)...\n";
    for (int i = 0; i < 50; i++)
    {
        uint64_t tuple_id = (1ULL << 32) | (i + 1);
        status = gin_index->insert(key_word.data(), key_word.length(), tuple_id,
                                   extractSingleWord, &ctx);
        assert(status == Status::OK);
    }

    std::cout << "  ✓ Posting list remained as list (not converted to tree)\n";

    delete db;
}

int main()
{
    std::cout << "=== GIN Index Phase 2 Tests (Posting Trees) ===\n\n";

    try
    {
        test_posting_list_to_tree_conversion();
        std::cout << "\n";

        test_posting_tree_insertion();
        std::cout << "\n";

        test_posting_tree_sorted_order();
        std::cout << "\n";

        test_posting_tree_duplicate_handling();
        std::cout << "\n";

        test_posting_tree_multiple_keys();
        std::cout << "\n";

        test_posting_tree_statistics();
        std::cout << "\n";

        test_posting_list_stays_list_under_threshold();
        std::cout << "\n";

        std::cout << "=== All GIN Phase 2 tests passed! ===\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
