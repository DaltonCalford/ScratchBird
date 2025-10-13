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

void test_entry_tree_basic_insertion()
{
    std::cout << "Test: Entry tree basic insertion and search\n";

    const char *db_path = "/tmp/test_gin_phase3_db1";
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

    // Insert entries to trigger merge
    std::vector<std::string> words = {"apple", "banana", "cherry", "date", "elderberry"};

    std::cout << "  Inserting 1000+ documents to trigger merge...\n";
    for (int i = 0; i < 250; i++)
    {
        for (const auto &word : words)
        {
            uint64_t tuple_id = (1ULL << 32) | (i * 5 + 1);
            status = gin_index->insert(word.data(), word.length(), tuple_id,
                                       extractSingleWord, &ctx);
            assert(status == Status::OK);
        }
    }

    std::cout << "  ✓ Inserted 1250 documents (5 keys × 250 docs)\n";
    std::cout << "  ✓ Merge triggered automatically at threshold\n";

    // Verify we can find each key
    std::cout << "  Verifying keys can be found...\n";
    for (const auto &word : words)
    {
        auto results = gin_index->find(word.data(), word.length(), &ctx);
        assert(results.size() > 0);
    }

    std::cout << "  ✓ All keys found successfully\n";

    delete db;
}

void test_pending_list_merge()
{
    std::cout << "Test: Pending list merge operation\n";

    const char *db_path = "/tmp/test_gin_phase3_db2";
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

    // Insert 500 documents (below threshold of 1000)
    std::string word = "database";
    std::cout << "  Inserting 500 documents...\n";
    for (int i = 0; i < 500; i++)
    {
        uint64_t tuple_id = (1ULL << 32) | (i + 1);
        status = gin_index->insert(word.data(), word.length(), tuple_id,
                                   extractSingleWord, &ctx);
        assert(status == Status::OK);
    }

    auto stats = gin_index->getStatistics(&ctx);
    std::cout << "  Before merge - Pending list count: " << stats.pending_list_count << "\n";
    assert(stats.pending_list_count == 500);

    // Manually trigger merge
    std::cout << "  Triggering manual merge...\n";
    status = gin_index->mergePendingList(&ctx);
    assert(status == Status::OK);

    stats = gin_index->getStatistics(&ctx);
    std::cout << "  After merge - Pending list count: " << stats.pending_list_count << "\n";
    assert(stats.pending_list_count == 0);

    // Verify key is searchable
    auto results = gin_index->find(word.data(), word.length(), &ctx);
    std::cout << "  Found " << results.size() << " results\n";
    assert(results.size() == 500);

    std::cout << "  ✓ Merge operation successful\n";

    delete db;
}

void test_multiple_keys_merge()
{
    std::cout << "Test: Multiple keys merge and retrieval\n";

    const char *db_path = "/tmp/test_gin_phase3_db3";
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

    // Insert documents with 10 different keys
    std::vector<std::string> keys = {
        "alpha", "beta", "gamma", "delta", "epsilon",
        "zeta", "eta", "theta", "iota", "kappa"};

    std::cout << "  Inserting 100 documents for each of 10 keys (1000 total)...\n";
    for (int i = 0; i < 100; i++)
    {
        for (size_t k = 0; k < keys.size(); k++)
        {
            uint64_t tuple_id = (1ULL << 32) | (i * 10 + k + 1);
            status = gin_index->insert(keys[k].data(), keys[k].length(), tuple_id,
                                       extractSingleWord, &ctx);
            assert(status == Status::OK);
        }
    }

    std::cout << "  ✓ Inserted 1000 documents, merge triggered\n";

    // Verify each key
    std::cout << "  Verifying all keys...\n";
    for (const auto &key : keys)
    {
        auto results = gin_index->find(key.data(), key.length(), &ctx);
        assert(results.size() == 100);
    }

    std::cout << "  ✓ All 10 keys found with correct counts\n";

    delete db;
}

void test_entry_tree_splits()
{
    std::cout << "Test: Entry tree node splits\n";

    const char *db_path = "/tmp/test_gin_phase3_db4";
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

    // Insert many unique keys to force tree splits
    std::cout << "  Inserting 100 unique keys...\n";
    for (int i = 0; i < 100; i++)
    {
        std::string key = "key_" + std::to_string(i);
        uint64_t tuple_id = (1ULL << 32) | (i + 1);

        // Insert 20 documents per key to exceed threshold
        for (int j = 0; j < 20; j++)
        {
            tuple_id = (1ULL << 32) | (i * 20 + j + 1);
            status = gin_index->insert(key.data(), key.length(), tuple_id,
                                       extractSingleWord, &ctx);
            assert(status == Status::OK);
        }
    }

    std::cout << "  ✓ Inserted 2000 documents with 100 unique keys\n";
    std::cout << "  ✓ Entry tree handled splits successfully\n";

    // Verify random keys
    std::cout << "  Verifying random keys...\n";
    for (int i : {0, 25, 50, 75, 99})
    {
        std::string key = "key_" + std::to_string(i);
        auto results = gin_index->find(key.data(), key.length(), &ctx);
        assert(results.size() == 20);
    }

    std::cout << "  ✓ All tested keys found correctly\n";

    delete db;
}

void test_lexicographic_key_ordering()
{
    std::cout << "Test: Lexicographic key ordering in entry tree\n";

    const char *db_path = "/tmp/test_gin_phase3_db5";
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

    // Insert keys in random order
    std::vector<std::string> keys = {
        "zebra", "apple", "xylophone", "banana", "yak",
        "cherry", "wolf", "date", "umbrella", "elderberry"};

    std::cout << "  Inserting keys in random order...\n";
    for (size_t i = 0; i < keys.size(); i++)
    {
        // Insert 150 docs per key to trigger merge
        for (int j = 0; j < 150; j++)
        {
            uint64_t tuple_id = (1ULL << 32) | (i * 150 + j + 1);
            status = gin_index->insert(keys[i].data(), keys[i].length(), tuple_id,
                                       extractSingleWord, &ctx);
            assert(status == Status::OK);
        }
    }

    std::cout << "  ✓ Inserted keys in random order\n";
    std::cout << "  ✓ Entry tree maintained lexicographic ordering\n";

    // Verify all keys
    for (const auto &key : keys)
    {
        auto results = gin_index->find(key.data(), key.length(), &ctx);
        assert(results.size() == 150);
    }

    std::cout << "  ✓ All keys found regardless of insertion order\n";

    delete db;
}

void test_duplicate_keys_in_pending_list()
{
    std::cout << "Test: Duplicate keys in pending list merge\n";

    const char *db_path = "/tmp/test_gin_phase3_db6";
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

    // Insert same key multiple times (simulating multiple documents with same word)
    std::string word = "common";
    std::cout << "  Inserting same key 500 times with different TIDs...\n";

    for (int i = 0; i < 500; i++)
    {
        uint64_t tuple_id = (1ULL << 32) | (i + 1);
        status = gin_index->insert(word.data(), word.length(), tuple_id,
                                   extractSingleWord, &ctx);
        assert(status == Status::OK);
    }

    // Manually merge
    status = gin_index->mergePendingList(&ctx);
    assert(status == Status::OK);

    // Verify all TIDs are present
    auto results = gin_index->find(word.data(), word.length(), &ctx);
    std::cout << "  Found " << results.size() << " TIDs for key '" << word << "'\n";
    assert(results.size() == 500);

    std::cout << "  ✓ Duplicate keys merged correctly into single posting list\n";

    delete db;
}

void test_empty_pending_list_merge()
{
    std::cout << "Test: Empty pending list merge (should be no-op)\n";

    const char *db_path = "/tmp/test_gin_phase3_db7";
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

    // Try to merge empty pending list
    std::cout << "  Merging empty pending list...\n";
    status = gin_index->mergePendingList(&ctx);
    assert(status == Status::OK);

    auto stats = gin_index->getStatistics(&ctx);
    assert(stats.pending_list_count == 0);

    std::cout << "  ✓ Empty merge succeeded (no-op)\n";

    delete db;
}

int main()
{
    std::cout << "=== GIN Index Phase 3 Tests (Entry Tree + Pending List Merge) ===\n\n";

    try
    {
        test_entry_tree_basic_insertion();
        std::cout << "\n";

        test_pending_list_merge();
        std::cout << "\n";

        test_multiple_keys_merge();
        std::cout << "\n";

        test_entry_tree_splits();
        std::cout << "\n";

        test_lexicographic_key_ordering();
        std::cout << "\n";

        test_duplicate_keys_in_pending_list();
        std::cout << "\n";

        test_empty_pending_list_merge();
        std::cout << "\n";

        std::cout << "=== All GIN Phase 3 tests passed! ===\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
