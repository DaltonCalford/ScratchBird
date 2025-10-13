#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <set>

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

// Helper: Convert string to key
std::vector<uint8_t> stringToKey(const std::string &str)
{
    return std::vector<uint8_t>(str.begin(), str.end());
}

void test_find_all_basic()
{
    std::cout << "Test: findAll() basic AND operation\n";

    const char *db_path = "/tmp/test_gin_phase4_db1";
    std::remove(db_path);

    ErrorContext ctx;
    Database db;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    status = db.open(db_path, &ctx);
    assert(status == Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    status = GinIndex::create(&db, index_uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto gin_index = GinIndex::open(&db, index_uuid, meta_page, &ctx);
    assert(gin_index != nullptr);

    std::cout << "  Inserting test documents...\n";
    // Document 1: has "apple" and "banana"
    status = gin_index->insert("apple", 5, (1ULL << 32) | 1, extractSingleWord, &ctx);
    assert(status == Status::OK);
    status = gin_index->insert("banana", 6, (1ULL << 32) | 1, extractSingleWord, &ctx);
    assert(status == Status::OK);

    // Document 2: has "apple" and "cherry"
    status = gin_index->insert("apple", 5, (1ULL << 32) | 2, extractSingleWord, &ctx);
    assert(status == Status::OK);
    status = gin_index->insert("cherry", 6, (1ULL << 32) | 2, extractSingleWord, &ctx);
    assert(status == Status::OK);

    // Document 3: has "apple", "banana", and "cherry"
    status = gin_index->insert("apple", 5, (1ULL << 32) | 3, extractSingleWord, &ctx);
    assert(status == Status::OK);
    status = gin_index->insert("banana", 6, (1ULL << 32) | 3, extractSingleWord, &ctx);
    assert(status == Status::OK);
    status = gin_index->insert("cherry", 6, (1ULL << 32) | 3, extractSingleWord, &ctx);
    assert(status == Status::OK);

    // Document 4: has only "banana"
    status = gin_index->insert("banana", 6, (1ULL << 32) | 4, extractSingleWord, &ctx);
    assert(status == Status::OK);

    // Trigger merge
    status = gin_index->mergePendingList(&ctx);
    assert(status == Status::OK);

    std::cout << "  Testing AND queries...\n";

    // Query: apple AND banana -> should return docs 1, 3
    std::vector<std::vector<uint8_t>> keys_ab = {stringToKey("apple"), stringToKey("banana")};
    auto results_ab = gin_index->findAll(keys_ab, &ctx);
    std::cout << "    apple AND banana: " << results_ab.size() << " results\n";
    assert(results_ab.size() == 2);
    std::set<uint64_t> expected_ab = {(1ULL << 32) | 1, (1ULL << 32) | 3};
    assert(std::set<uint64_t>(results_ab.begin(), results_ab.end()) == expected_ab);

    // Query: apple AND cherry -> should return docs 2, 3
    std::vector<std::vector<uint8_t>> keys_ac = {stringToKey("apple"), stringToKey("cherry")};
    auto results_ac = gin_index->findAll(keys_ac, &ctx);
    std::cout << "    apple AND cherry: " << results_ac.size() << " results\n";
    assert(results_ac.size() == 2);

    // Query: apple AND banana AND cherry -> should return doc 3 only
    std::vector<std::vector<uint8_t>> keys_abc = {
        stringToKey("apple"), stringToKey("banana"), stringToKey("cherry")};
    auto results_abc = gin_index->findAll(keys_abc, &ctx);
    std::cout << "    apple AND banana AND cherry: " << results_abc.size() << " results\n";
    assert(results_abc.size() == 1);
    assert(results_abc[0] == ((1ULL << 32) | 3));

    // Query: banana AND nonexistent -> should return 0
    std::vector<std::vector<uint8_t>> keys_bn = {stringToKey("banana"), stringToKey("nonexistent")};
    auto results_bn = gin_index->findAll(keys_bn, &ctx);
    std::cout << "    banana AND nonexistent: " << results_bn.size() << " results\n";
    assert(results_bn.size() == 0);

    std::cout << "  ✓ All findAll() tests passed\n";

    db.close();
}

void test_find_any_basic()
{
    std::cout << "Test: findAny() basic OR operation\n";

    const char *db_path = "/tmp/test_gin_phase4_db2";
    std::remove(db_path);

    ErrorContext ctx;
    Database db;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    status = db.open(db_path, &ctx);
    assert(status == Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    status = GinIndex::create(&db, index_uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto gin_index = GinIndex::open(&db, index_uuid, meta_page, &ctx);
    assert(gin_index != nullptr);

    std::cout << "  Inserting test documents...\n";
    // Document 1: has "red"
    status = gin_index->insert("red", 3, (1ULL << 32) | 1, extractSingleWord, &ctx);
    assert(status == Status::OK);

    // Document 2: has "green"
    status = gin_index->insert("green", 5, (1ULL << 32) | 2, extractSingleWord, &ctx);
    assert(status == Status::OK);

    // Document 3: has "blue"
    status = gin_index->insert("blue", 4, (1ULL << 32) | 3, extractSingleWord, &ctx);
    assert(status == Status::OK);

    // Document 4: has "red" and "blue"
    status = gin_index->insert("red", 3, (1ULL << 32) | 4, extractSingleWord, &ctx);
    assert(status == Status::OK);
    status = gin_index->insert("blue", 4, (1ULL << 32) | 4, extractSingleWord, &ctx);
    assert(status == Status::OK);

    // Trigger merge
    status = gin_index->mergePendingList(&ctx);
    assert(status == Status::OK);

    std::cout << "  Testing OR queries...\n";

    // Query: red OR green -> should return docs 1, 2, 4
    std::vector<std::vector<uint8_t>> keys_rg = {stringToKey("red"), stringToKey("green")};
    auto results_rg = gin_index->findAny(keys_rg, &ctx);
    std::cout << "    red OR green: " << results_rg.size() << " results\n";
    assert(results_rg.size() == 3);
    std::set<uint64_t> expected_rg = {(1ULL << 32) | 1, (1ULL << 32) | 2, (1ULL << 32) | 4};
    assert(std::set<uint64_t>(results_rg.begin(), results_rg.end()) == expected_rg);

    // Query: red OR blue -> should return docs 1, 3, 4
    std::vector<std::vector<uint8_t>> keys_rb = {stringToKey("red"), stringToKey("blue")};
    auto results_rb = gin_index->findAny(keys_rb, &ctx);
    std::cout << "    red OR blue: " << results_rb.size() << " results\n";
    assert(results_rb.size() == 3);

    // Query: red OR green OR blue -> should return all docs 1, 2, 3, 4
    std::vector<std::vector<uint8_t>> keys_rgb = {
        stringToKey("red"), stringToKey("green"), stringToKey("blue")};
    auto results_rgb = gin_index->findAny(keys_rgb, &ctx);
    std::cout << "    red OR green OR blue: " << results_rgb.size() << " results\n";
    assert(results_rgb.size() == 4);

    // Query: nonexistent1 OR nonexistent2 -> should return 0
    std::vector<std::vector<uint8_t>> keys_nn = {stringToKey("nonexistent1"), stringToKey("nonexistent2")};
    auto results_nn = gin_index->findAny(keys_nn, &ctx);
    std::cout << "    nonexistent1 OR nonexistent2: " << results_nn.size() << " results\n";
    assert(results_nn.size() == 0);

    std::cout << "  ✓ All findAny() tests passed\n";

    db.close();
}

void test_complex_queries()
{
    std::cout << "Test: Complex multi-key queries\n";

    const char *db_path = "/tmp/test_gin_phase4_db3";
    std::remove(db_path);

    ErrorContext ctx;
    Database db;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    status = db.open(db_path, &ctx);
    assert(status == Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    status = GinIndex::create(&db, index_uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto gin_index = GinIndex::open(&db, index_uuid, meta_page, &ctx);
    assert(gin_index != nullptr);

    std::cout << "  Inserting 100 documents with various keys...\n";

    // Create diverse dataset
    std::vector<std::string> word_pool = {"alpha", "beta", "gamma", "delta", "epsilon"};

    for (int i = 1; i <= 100; i++)
    {
        // Each document gets 1-3 words
        int num_words = (i % 3) + 1;
        for (int j = 0; j < num_words; j++)
        {
            std::string word = word_pool[(i + j) % word_pool.size()];
            status = gin_index->insert(word.data(), word.length(),
                                       (1ULL << 32) | i, extractSingleWord, &ctx);
            assert(status == Status::OK);
        }
    }

    // Trigger merge
    status = gin_index->mergePendingList(&ctx);
    assert(status == Status::OK);

    std::cout << "  Testing complex queries...\n";

    // AND query with 2 keys
    std::vector<std::vector<uint8_t>> keys_and2 = {stringToKey("alpha"), stringToKey("beta")};
    auto results_and2 = gin_index->findAll(keys_and2, &ctx);
    std::cout << "    alpha AND beta: " << results_and2.size() << " results\n";
    assert(results_and2.size() > 0);

    // OR query with 3 keys
    std::vector<std::vector<uint8_t>> keys_or3 = {
        stringToKey("gamma"), stringToKey("delta"), stringToKey("epsilon")};
    auto results_or3 = gin_index->findAny(keys_or3, &ctx);
    std::cout << "    gamma OR delta OR epsilon: " << results_or3.size() << " results\n";
    assert(results_or3.size() > 0);

    // AND query with all keys (should have very few results)
    std::vector<std::vector<uint8_t>> keys_and_all = {
        stringToKey("alpha"), stringToKey("beta"), stringToKey("gamma"),
        stringToKey("delta"), stringToKey("epsilon")};
    auto results_and_all = gin_index->findAll(keys_and_all, &ctx);
    std::cout << "    all 5 keys AND: " << results_and_all.size() << " results\n";

    std::cout << "  ✓ Complex query tests passed\n";

    db.close();
}

void test_large_scale_multi_key()
{
    std::cout << "Test: Large-scale multi-key operations\n";

    const char *db_path = "/tmp/test_gin_phase4_db4";
    std::remove(db_path);

    ErrorContext ctx;
    Database db;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    status = db.open(db_path, &ctx);
    assert(status == Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    status = GinIndex::create(&db, index_uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto gin_index = GinIndex::open(&db, index_uuid, meta_page, &ctx);
    assert(gin_index != nullptr);

    std::cout << "  Inserting 1000 documents...\n";

    // Insert many documents with overlapping keys
    std::vector<std::string> keys = {"key1", "key2", "key3", "key4", "key5"};

    for (int i = 1; i <= 1000; i++)
    {
        // Each document gets 2-3 keys based on its number
        for (size_t j = 0; j < keys.size(); j++)
        {
            if (i % (j + 2) == 0)
            {
                status = gin_index->insert(keys[j].data(), keys[j].length(),
                                           (1ULL << 32) | i, extractSingleWord, &ctx);
                assert(status == Status::OK);
            }
        }
    }

    std::cout << "  ✓ Inserted 1000 documents\n";

    // Test AND query
    std::vector<std::vector<uint8_t>> keys_and = {stringToKey("key1"), stringToKey("key2")};
    auto results_and = gin_index->findAll(keys_and, &ctx);
    std::cout << "    key1 AND key2: " << results_and.size() << " results\n";
    assert(results_and.size() > 0);

    // Test OR query
    std::vector<std::vector<uint8_t>> keys_or = {stringToKey("key4"), stringToKey("key5")};
    auto results_or = gin_index->findAny(keys_or, &ctx);
    std::cout << "    key4 OR key5: " << results_or.size() << " results\n";
    assert(results_or.size() > 0);

    std::cout << "  ✓ Large-scale multi-key tests passed\n";

    db.close();
}

void test_edge_cases()
{
    std::cout << "Test: Edge cases\n";

    const char *db_path = "/tmp/test_gin_phase4_db5";
    std::remove(db_path);

    ErrorContext ctx;
    Database db;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    status = db.open(db_path, &ctx);
    assert(status == Status::OK);

    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;
    status = GinIndex::create(&db, index_uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto gin_index = GinIndex::open(&db, index_uuid, meta_page, &ctx);
    assert(gin_index != nullptr);

    std::cout << "  Testing edge cases...\n";

    // Empty key list
    std::vector<std::vector<uint8_t>> empty_keys;
    auto results_empty = gin_index->findAll(empty_keys, &ctx);
    assert(results_empty.size() == 0);
    std::cout << "    ✓ Empty key list handled\n";

    // Single key AND (should work like find())
    std::vector<std::vector<uint8_t>> single_key = {stringToKey("test")};
    auto results_single = gin_index->findAll(single_key, &ctx);
    assert(results_single.size() == 0); // No data inserted
    std::cout << "    ✓ Single key query handled\n";

    // Query on empty index
    std::vector<std::vector<uint8_t>> keys_any = {stringToKey("a"), stringToKey("b")};
    auto results_any = gin_index->findAny(keys_any, &ctx);
    assert(results_any.size() == 0);
    std::cout << "    ✓ Empty index query handled\n";

    std::cout << "  ✓ All edge case tests passed\n";

    db.close();
}

int main()
{
    std::cout << "=== GIN Index Phase 4 Tests (Multi-Key Queries) ===\n\n";

    try
    {
        test_find_all_basic();
        std::cout << "\n";

        test_find_any_basic();
        std::cout << "\n";

        test_complex_queries();
        std::cout << "\n";

        test_large_scale_multi_key();
        std::cout << "\n";

        test_edge_cases();
        std::cout << "\n";

        std::cout << "=== All GIN Phase 4 tests passed! ===\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
