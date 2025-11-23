// Test for GIN Index Phase 5: Query Optimization & Advanced Operators
// Tests selectivity-based reordering, PostgreSQL operators, and advanced query features

#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
#include <iostream>
#include "gtest/gtest.h"
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>

using namespace scratchbird::core;

// Helper: Create a key from a string
std::vector<uint8_t> makeKey(const std::string &s)
{
    return std::vector<uint8_t>(s.begin(), s.end());
}

// Helper: Simple key extractor for tags
std::vector<std::vector<uint8_t>> extractTags(const void *data, size_t len)
{
    std::string tags_str(static_cast<const char *>(data), len);
    std::vector<std::vector<uint8_t>> keys;

    size_t start = 0;
    while (start < tags_str.size())
    {
        size_t comma = tags_str.find(',', start);
        if (comma == std::string::npos)
        {
            comma = tags_str.size();
        }

        std::string tag = tags_str.substr(start, comma - start);
        if (!tag.empty())
        {
            keys.push_back(makeKey(tag));
        }

        start = comma + 1;
    }

    return keys;
}

void testCardinalityEstimation()
{
    std::cout << "\n=== Test 1: Cardinality Estimation ===" << std::endl;

    const char *db_path = "./test_gin_phase5_db1";
    std::remove(db_path);

    ErrorContext ctx;
    Database db;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    status = db.open(db_path, &ctx);
    assert(status == Status::OK);

    UuidV7Bytes uuid = generateUuidV7();
    uint32_t meta_page = 0;
    status = GinIndex::create(&db, uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto index = GinIndex::open(&db, uuid, meta_page, &ctx);
    assert(index != nullptr);

    // Insert documents with varying key frequencies
    // "common" appears in 10 docs
    // "rare" appears in 2 docs
    // "unique" appears in 1 doc

    for (uint64_t i = 1; i <= 10; i++)
    {
        std::string tags = "common,tag" + std::to_string(i);
        index->insert(tags.data(), tags.size(), (1ULL << 32) | i, extractTags, &ctx);
    }

    for (uint64_t i = 11; i <= 12; i++)
    {
        std::string tags = "rare,tag" + std::to_string(i);
        index->insert(tags.data(), tags.size(), (1ULL << 32) | i, extractTags, &ctx);
    }

    std::string unique_tags = "unique,tag13";
    index->insert(unique_tags.data(), unique_tags.size(), (1ULL << 32) | 13, extractTags, &ctx);

    // Merge pending list
    status = index->mergePendingList(&ctx);
    assert(status == Status::OK);

    // Test cardinality estimates
    uint32_t common_card = index->estimateKeyCardinality(makeKey("common"), &ctx);
    uint32_t rare_card = index->estimateKeyCardinality(makeKey("rare"), &ctx);
    uint32_t unique_card = index->estimateKeyCardinality(makeKey("unique"), &ctx);
    uint32_t missing_card = index->estimateKeyCardinality(makeKey("missing"), &ctx);

    std::cout << "Cardinality estimates:" << std::endl;
    std::cout << "  common: " << common_card << " (expected ~10)" << std::endl;
    std::cout << "  rare: " << rare_card << " (expected ~2)" << std::endl;
    std::cout << "  unique: " << unique_card << " (expected ~1)" << std::endl;
    std::cout << "  missing: " << missing_card << " (expected 0)" << std::endl;

    assert(common_card == 10);
    assert(rare_card == 2);
    assert(unique_card == 1);
    assert(missing_card == 0);

    std::cout << "✓ Cardinality estimation working correctly" << std::endl;

    db.close();
}

void testOptimizedQueries()
{
    std::cout << "\n=== Test 2: Selectivity-Based Query Optimization ===" << std::endl;

    const char *db_path = "./test_gin_phase5_db2";
    std::remove(db_path);

    ErrorContext ctx;
    Database db;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    status = db.open(db_path, &ctx);
    assert(status == Status::OK);

    UuidV7Bytes uuid = generateUuidV7();
    uint32_t meta_page = 0;
    status = GinIndex::create(&db, uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto index = GinIndex::open(&db, uuid, meta_page, &ctx);
    assert(index != nullptr);

    // Insert 100 documents with varying key selectivity
    for (uint64_t i = 1; i <= 100; i++)
    {
        std::string tags;
        tags += "common,";              // Appears in all 100
        if (i % 10 == 0) tags += "rare,"; // Appears in 10
        if (i % 50 == 0) tags += "very_rare"; // Appears in 2
        else tags.pop_back(); // Remove trailing comma

        index->insert(tags.data(), tags.size(), (1ULL << 32) | i, extractTags, &ctx);
    }

    status = index->mergePendingList(&ctx);
    assert(status == Status::OK);

    // Test optimized AND query
    GinIndex::QueryOptions options;
    options.optimize_key_order = true;
    options.parallel_execution = false;
    options.max_edit_distance = 0;
    options.case_sensitive = true;
    options.max_threads = 1;

    std::vector<std::vector<uint8_t>> keys = {
        makeKey("common"),
        makeKey("rare"),
        makeKey("very_rare")};

    auto start = std::chrono::high_resolution_clock::now();
    auto results_opt = index->findAllOptimized(keys, options, &ctx);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_opt = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Optimized query returned " << results_opt.size() << " results" << std::endl;
    std::cout << "Time: " << duration_opt.count() << " μs" << std::endl;

    // Verify correctness - should return docs 50 and 100
    assert(results_opt.size() == 2);
    assert(std::find(results_opt.begin(), results_opt.end(), (1ULL << 32) | 50) != results_opt.end());
    assert(std::find(results_opt.begin(), results_opt.end(), (1ULL << 32) | 100) != results_opt.end());

    // Test without optimization
    options.optimize_key_order = false;
    start = std::chrono::high_resolution_clock::now();
    auto results_std = index->findAllOptimized(keys, options, &ctx);
    end = std::chrono::high_resolution_clock::now();
    auto duration_std = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Standard query returned " << results_std.size() << " results" << std::endl;
    std::cout << "Time: " << duration_std.count() << " μs" << std::endl;

    // Results should be identical
    assert(results_std.size() == results_opt.size());
    assert(results_std == results_opt);

    std::cout << "✓ Optimized queries working correctly" << std::endl;

    db.close();
}

void testPostgreSQLOperators()
{
    std::cout << "\n=== Test 3: PostgreSQL GIN Operators ===" << std::endl;

    const char *db_path = "./test_gin_phase5_db3";
    std::remove(db_path);

    ErrorContext ctx;
    Database db;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    status = db.open(db_path, &ctx);
    assert(status == Status::OK);

    UuidV7Bytes uuid = generateUuidV7();
    uint32_t meta_page = 0;
    status = GinIndex::create(&db, uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto index = GinIndex::open(&db, uuid, meta_page, &ctx);
    assert(index != nullptr);

    // Insert test documents with various tag combinations
    // Doc 1: {red, blue}
    // Doc 2: {red, green}
    // Doc 3: {blue, green}
    // Doc 4: {red, blue, green}
    // Doc 5: {yellow}

    index->insert("red,blue", 8, (1ULL << 32) | 1, extractTags, &ctx);
    index->insert("red,green", 9, (1ULL << 32) | 2, extractTags, &ctx);
    index->insert("blue,green", 10, (1ULL << 32) | 3, extractTags, &ctx);
    index->insert("red,blue,green", 14, (1ULL << 32) | 4, extractTags, &ctx);
    index->insert("yellow", 6, (1ULL << 32) | 5, extractTags, &ctx);

    status = index->mergePendingList(&ctx);
    assert(status == Status::OK);

    // Test CONTAINS (@>) - documents containing all specified tags
    std::cout << "\nTest CONTAINS (@>)" << std::endl;
    std::vector<std::vector<uint8_t>> empty_left;
    std::vector<std::vector<uint8_t>> red_blue = {makeKey("red"), makeKey("blue")};

    auto results = index->executeOperator(GinIndex::GinOperator::CONTAINS, empty_left, red_blue, &ctx);
    std::cout << "  Documents containing {red, blue}: " << results.size() << std::endl;
    assert(results.size() == 2); // Docs 1 and 4
    assert(std::find(results.begin(), results.end(), (1ULL << 32) | 1) != results.end());
    assert(std::find(results.begin(), results.end(), (1ULL << 32) | 4) != results.end());

    // Test OVERLAP (&&) - documents with at least one common tag
    std::cout << "\nTest OVERLAP (&&)" << std::endl;
    std::vector<std::vector<uint8_t>> red_yellow = {makeKey("red"), makeKey("yellow")};

    results = index->executeOperator(GinIndex::GinOperator::OVERLAP, red_yellow, empty_left, &ctx);
    std::cout << "  Documents with {red} OR {yellow}: " << results.size() << std::endl;
    assert(results.size() == 4); // Docs 1, 2, 4, 5

    // Test EXISTS (?) - any document with the specified tag
    std::cout << "\nTest EXISTS (?)" << std::endl;
    std::vector<std::vector<uint8_t>> green_only = {makeKey("green")};

    results = index->executeOperator(GinIndex::GinOperator::EXISTS, green_only, empty_left, &ctx);
    std::cout << "  Documents with {green}: " << results.size() << std::endl;
    assert(results.size() == 3); // Docs 2, 3, 4

    // Test EXISTS_ALL (?&) - documents containing ALL tags
    std::cout << "\nTest EXISTS_ALL (?&)" << std::endl;
    std::vector<std::vector<uint8_t>> all_three = {
        makeKey("red"),
        makeKey("blue"),
        makeKey("green")};

    results = index->executeOperator(GinIndex::GinOperator::EXISTS_ALL, all_three, empty_left, &ctx);
    std::cout << "  Documents with {red, blue, green}: " << results.size() << std::endl;
    assert(results.size() == 1); // Only doc 4

    // Test EXISTS_ANY (?|) - documents with at least one tag
    std::cout << "\nTest EXISTS_ANY (?|)" << std::endl;
    std::vector<std::vector<uint8_t>> blue_yellow = {makeKey("blue"), makeKey("yellow")};

    results = index->executeOperator(GinIndex::GinOperator::EXISTS_ANY, blue_yellow, empty_left, &ctx);
    std::cout << "  Documents with {blue} OR {yellow}: " << results.size() << std::endl;
    assert(results.size() == 4); // Docs 1, 3, 4, 5

    std::cout << "✓ All PostgreSQL GIN operators working correctly" << std::endl;

    db.close();
}

void testEdgeCases()
{
    std::cout << "\n=== Test 4: Edge Cases ===" << std::endl;

    const char *db_path = "./test_gin_phase5_db4";
    std::remove(db_path);

    ErrorContext ctx;
    Database db;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    status = db.open(db_path, &ctx);
    assert(status == Status::OK);

    UuidV7Bytes uuid = generateUuidV7();
    uint32_t meta_page = 0;
    status = GinIndex::create(&db, uuid, &meta_page, &ctx);
    assert(status == Status::OK);

    auto index = GinIndex::open(&db, uuid, meta_page, &ctx);
    assert(index != nullptr);

    // Test empty key sets
    std::cout << "Testing empty key sets..." << std::endl;
    GinIndex::QueryOptions options;
    options.optimize_key_order = true;
    options.parallel_execution = false;
    options.max_edit_distance = 0;
    options.case_sensitive = true;
    options.max_threads = 1;

    std::vector<std::vector<uint8_t>> empty_keys;
    auto results = index->findAllOptimized(empty_keys, options, &ctx);
    assert(results.empty());

    results = index->findAnyOptimized(empty_keys, options, &ctx);
    assert(results.empty());

    // Test operators with empty index
    std::cout << "Testing operators on empty index..." << std::endl;
    std::vector<std::vector<uint8_t>> test_keys = {makeKey("test")};
    std::vector<std::vector<uint8_t>> empty;

    results = index->executeOperator(GinIndex::GinOperator::CONTAINS, empty, test_keys, &ctx);
    assert(results.empty());

    results = index->executeOperator(GinIndex::GinOperator::EXISTS, test_keys, empty, &ctx);
    assert(results.empty());

    // Insert a document and test
    index->insert("tag1,tag2", 9, (1ULL << 32) | 1, extractTags, &ctx);
    status = index->mergePendingList(&ctx);
    assert(status == Status::OK);

    // Test single key optimization
    std::cout << "Testing single key optimization..." << std::endl;
    std::vector<std::vector<uint8_t>> single_key = {makeKey("tag1")};
    results = index->findAllOptimized(single_key, options, &ctx);
    assert(results.size() == 1);
    assert(results[0] == ((1ULL << 32) | 1));

    std::cout << "✓ Edge cases handled correctly" << std::endl;

    db.close();
}


TEST(GinPhase5Test, Comprehensive) {

    std::cout << "============================================" << std::endl;
    std::cout << "GIN Index Phase 5 Test Suite" << std::endl;
    std::cout << "Query Optimization & Advanced Operators" << std::endl;
    std::cout << "============================================" << std::endl;

    try
    {
        testCardinalityEstimation();
        testOptimizedQueries();
        testPostgreSQLOperators();
        testEdgeCases();

        std::cout << "\n============================================" << std::endl;
        std::cout << "All Phase 5 tests passed! ✓" << std::endl;
        std::cout << "============================================" << std::endl;

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

}
