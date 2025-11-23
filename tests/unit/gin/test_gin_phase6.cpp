// Test for GIN Index Phase 6: Advanced Performance Features
// Tests SIMD operations, parallel execution, range queries, and optimized wildcard/fuzzy matching

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

void testSIMDSetOperations()
{
    std::cout << "\n=== Test 1: SIMD Set Operations ===\n";

    // Test intersection
    std::cout << "Testing SIMD intersection...\n";
    std::vector<std::vector<uint64_t>> test_lists_and = {
        {1, 3, 5, 7, 9, 11, 13, 15, 17, 19},
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
        {1, 5, 9, 13, 17, 21, 25, 29}};

    auto intersection_result = GinIndex::mergeTidListsSIMD(test_lists_and);
    std::cout << "  Intersection result size: " << intersection_result.size() << "\n";
    std::cout << "  Expected: {1, 5, 9}\n";
    std::cout << "  Got: {";
    for (size_t i = 0; i < intersection_result.size(); i++)
    {
        std::cout << intersection_result[i];
        if (i < intersection_result.size() - 1)
            std::cout << ", ";
    }
    std::cout << "}\n";

    assert(intersection_result.size() == 3);
    assert(intersection_result[0] == 1);
    assert(intersection_result[1] == 5);
    assert(intersection_result[2] == 9);

    // Test union
    std::cout << "\nTesting SIMD union...\n";
    std::vector<std::vector<uint64_t>> test_lists_or = {
        {1, 3, 5},
        {2, 4, 6},
        {7, 8, 9}};

    auto union_result = GinIndex::unionTidListsSIMD(test_lists_or);
    std::cout << "  Union result size: " << union_result.size() << "\n";
    std::cout << "  Expected: 9 unique elements\n";

    assert(union_result.size() == 9);
    // Verify sorted and unique
    for (size_t i = 1; i < union_result.size(); i++)
    {
        assert(union_result[i] > union_result[i - 1]);
    }

    std::cout << "✓ SIMD set operations working correctly\n";
}

void testParallelQueries()
{
    std::cout << "\n=== Test 2: Parallel Query Execution ===\n";

    const char *db_path = "./test_gin_phase6_db2";
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

    // Insert 200 documents with various tag combinations
    std::cout << "Inserting 200 test documents...\n";
    for (uint64_t i = 1; i <= 200; i++)
    {
        std::string tags;
        tags += "tag_all,";
        if (i % 2 == 0)
            tags += "even,";
        if (i % 3 == 0)
            tags += "div3,";
        if (i % 5 == 0)
            tags += "div5,";
        if (i % 7 == 0)
            tags += "div7,";
        if (i % 10 == 0)
            tags += "div10,";
        tags.pop_back(); // Remove trailing comma

        index->insert(tags.data(), tags.size(), (1ULL << 32) | i, extractTags, &ctx);
    }

    status = index->mergePendingList(&ctx);
    assert(status == Status::OK);

    // Test parallel AND query
    std::cout << "\nTesting parallel AND query...\n";
    std::vector<std::vector<uint8_t>> and_keys = {
        makeKey("tag_all"),
        makeKey("even"),
        makeKey("div3"),
        makeKey("div5")};

    auto start = std::chrono::high_resolution_clock::now();
    auto parallel_results = index->findAllParallel(and_keys, 4, &ctx);
    auto end = std::chrono::high_resolution_clock::now();
    auto parallel_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "  Parallel query returned " << parallel_results.size() << " results\n";
    std::cout << "  Time: " << parallel_duration.count() << " μs\n";

    // Verify correctness - should find numbers divisible by 2, 3, and 5 (i.e., divisible by 30)
    assert(parallel_results.size() == 6); // 30, 60, 90, 120, 150, 180

    // Test parallel OR query
    std::cout << "\nTesting parallel OR query...\n";
    std::vector<std::vector<uint8_t>> or_keys = {
        makeKey("div7"),
        makeKey("div10")};

    start = std::chrono::high_resolution_clock::now();
    auto parallel_or_results = index->findAnyParallel(or_keys, 2, &ctx);
    end = std::chrono::high_resolution_clock::now();
    auto parallel_or_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "  Parallel OR query returned " << parallel_or_results.size() << " results\n";
    std::cout << "  Time: " << parallel_or_duration.count() << " μs\n";

    // Should find all multiples of 7 or 10
    // Multiples of 7: 7, 14, 21, 28, 35, ... 196 (28 total)
    // Multiples of 10: 10, 20, 30, ... 200 (20 total)
    // Overlap: 70, 140 (2 total)
    // Union: 28 + 20 - 2 = 46
    assert(parallel_or_results.size() == 46);

    std::cout << "✓ Parallel queries working correctly\n";

    db.close();
}

void testRangeQueries()
{
    std::cout << "\n=== Test 3: Range Query Support ===\n";

    const char *db_path = "./test_gin_phase6_db3";
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

    // Insert documents with alphabetically ordered tags
    std::cout << "Inserting documents with alphabetically ordered tags...\n";
    std::vector<std::string> tags = {"apple", "banana", "cherry", "date", "elderberry",
                                     "fig", "grape", "honeydew", "kiwi", "lemon",
                                     "mango", "nectarine", "orange", "papaya", "quince"};

    for (size_t i = 0; i < tags.size(); i++)
    {
        index->insert(tags[i].data(), tags[i].size(), (1ULL << 32) | (i + 1), extractTags, &ctx);
    }

    status = index->mergePendingList(&ctx);
    assert(status == Status::OK);

    // Test range query: [cherry, grape)
    std::cout << "\nTesting range query [cherry, grape)...\n";
    GinIndex::RangeQuery range;
    range.lower_bound = makeKey("cherry");
    range.upper_bound = makeKey("grape");
    range.lower_inclusive = true;
    range.upper_inclusive = false;

    auto range_results = index->findInRange(range, &ctx);
    std::cout << "  Range query returned " << range_results.size() << " results\n";
    std::cout << "  Expected: cherry, date, elderberry, fig (4 results)\n";

    assert(range_results.size() == 4);

    // Test range query: (elderberry, mango]
    std::cout << "\nTesting range query (elderberry, mango]...\n";
    range.lower_bound = makeKey("elderberry");
    range.upper_bound = makeKey("mango");
    range.lower_inclusive = false;
    range.upper_inclusive = true;

    range_results = index->findInRange(range, &ctx);
    std::cout << "  Range query returned " << range_results.size() << " results\n";
    std::cout << "  Expected: fig, grape, honeydew, kiwi, lemon, mango (6 results)\n";

    assert(range_results.size() == 6);

    std::cout << "✓ Range queries working correctly\n";

    db.close();
}

void testOptimizedWildcardQueries()
{
    std::cout << "\n=== Test 4: Optimized Wildcard Queries ===\n";

    const char *db_path = "./test_gin_phase6_db4";
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

    // Insert documents with prefix patterns
    std::cout << "Inserting documents with prefix patterns...\n";
    std::vector<std::string> tags = {
        "app", "apple", "application", "apply",
        "car", "card", "care", "cargo",
        "test", "testing", "tester",
        "dog", "cat", "bird"};

    for (size_t i = 0; i < tags.size(); i++)
    {
        index->insert(tags[i].data(), tags[i].size(), (1ULL << 32) | (i + 1), extractTags, &ctx);
    }

    status = index->mergePendingList(&ctx);
    assert(status == Status::OK);

    // Test prefix wildcard: "app%"
    std::cout << "\nTesting wildcard query 'app%'...\n";
    std::string pattern1 = "app%";
    auto wildcard_results = index->findWithWildcardOptimized(pattern1.data(), pattern1.size(), &ctx);
    std::cout << "  Wildcard query returned " << wildcard_results.size() << " results\n";
    std::cout << "  Expected: app, apple, application, apply (4 results)\n";

    assert(wildcard_results.size() == 4);

    // Test prefix wildcard: "car%"
    std::cout << "\nTesting wildcard query 'car%'...\n";
    std::string pattern2 = "car%";
    wildcard_results = index->findWithWildcardOptimized(pattern2.data(), pattern2.size(), &ctx);
    std::cout << "  Wildcard query returned " << wildcard_results.size() << " results\n";
    std::cout << "  Expected: car, card, care, cargo (4 results)\n";

    assert(wildcard_results.size() == 4);

    // Test prefix wildcard: "test%"
    std::cout << "\nTesting wildcard query 'test%'...\n";
    std::string pattern3 = "test%";
    wildcard_results = index->findWithWildcardOptimized(pattern3.data(), pattern3.size(), &ctx);
    std::cout << "  Wildcard query returned " << wildcard_results.size() << " results\n";
    std::cout << "  Expected: test, testing, tester (3 results)\n";

    assert(wildcard_results.size() == 3);

    std::cout << "✓ Optimized wildcard queries working correctly\n";

    db.close();
}

void testFuzzyMatchingInfrastructure()
{
    std::cout << "\n=== Test 5: Fuzzy Matching Infrastructure ===\n";

    const char *db_path = "./test_gin_phase6_db5";
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

    // Insert some test data
    std::vector<std::string> tags = {"hello", "world", "test"};
    for (size_t i = 0; i < tags.size(); i++)
    {
        index->insert(tags[i].data(), tags[i].size(), (1ULL << 32) | (i + 1), extractTags, &ctx);
    }

    status = index->mergePendingList(&ctx);
    assert(status == Status::OK);

    // Test fuzzy matching (should return NOT_IMPLEMENTED)
    std::cout << "Testing fuzzy matching (expecting NOT_IMPLEMENTED)...\n";
    std::string query = "helo";
    auto fuzzy_results = index->findFuzzyOptimized(query.data(), query.size(), 1, &ctx);

    std::cout << "  Fuzzy query returned " << fuzzy_results.size() << " results\n";
    std::cout << "  Expected: 0 (NOT_IMPLEMENTED)\n";

    // Should return empty results since it's not yet implemented
    assert(fuzzy_results.empty());

    std::cout << "✓ Fuzzy matching infrastructure in place (optimization pending)\n";

    db.close();
}

void testEdgeCases()
{
    std::cout << "\n=== Test 6: Edge Cases ===\n";

    const char *db_path = "./test_gin_phase6_db6";
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

    std::cout << "Testing empty SIMD operations...\n";
    std::vector<std::vector<uint64_t>> empty_lists;
    auto empty_result = GinIndex::mergeTidListsSIMD(empty_lists);
    assert(empty_result.empty());

    std::cout << "Testing single-element SIMD operations...\n";
    std::vector<std::vector<uint64_t>> single_list = {{42}};
    auto single_result = GinIndex::mergeTidListsSIMD(single_list);
    assert(single_result.size() == 1);
    assert(single_result[0] == 42);

    std::cout << "Testing parallel queries with empty index...\n";
    std::vector<std::vector<uint8_t>> test_keys = {makeKey("nonexistent")};
    auto parallel_empty = index->findAllParallel(test_keys, 2, &ctx);
    assert(parallel_empty.empty());

    std::cout << "Testing range query on empty index...\n";
    GinIndex::RangeQuery empty_range;
    empty_range.lower_bound = makeKey("a");
    empty_range.upper_bound = makeKey("z");
    empty_range.lower_inclusive = true;
    empty_range.upper_inclusive = true;
    auto range_empty = index->findInRange(empty_range, &ctx);
    assert(range_empty.empty());

    std::cout << "✓ Edge cases handled correctly\n";

    db.close();
}


TEST(GinPhase6Test, Comprehensive) {

    std::cout << "============================================\n";
    std::cout << "GIN Index Phase 6 Test Suite\n";
    std::cout << "Advanced Performance Features\n";
    std::cout << "============================================\n";

    try
    {
        testSIMDSetOperations();
        testParallelQueries();
        testRangeQueries();
        testOptimizedWildcardQueries();
        testFuzzyMatchingInfrastructure();
        testEdgeCases();

        std::cout << "\n============================================\n";
        std::cout << "All Phase 6 tests passed! ✓\n";
        std::cout << "============================================\n";

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << "\n";
        return 1;
    }

}
