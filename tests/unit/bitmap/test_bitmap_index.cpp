// ScratchBird Bitmap Index - Comprehensive Test Suite
// Tests all aspects of Roaring Bitmap implementation for low-cardinality columns

#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>

using namespace scratchbird::core;

// Test database setup
static const char *TEST_DB_PATH = "test_bitmap_index.db";
static const uint32_t PAGE_SIZE = 8192;
static const uint32_t NUM_PAGES = 1000;

// Helper to create test database
Database *createTestDatabase()
{
    std::remove(TEST_DB_PATH);

    ErrorContext ctx;
    UuidV7Bytes db_uuid;
    for (int i = 0; i < 16; i++)
    {
        db_uuid[i] = static_cast<uint8_t>(i);
    }

    auto status = Database::create(TEST_DB_PATH, PAGE_SIZE, db_uuid, &ctx);
    if (!status.isOk())
    {
        std::cerr << "Failed to create test database: " << ctx.message << std::endl;
        return nullptr;
    }

    auto db = Database::open(TEST_DB_PATH, &ctx);
    if (!db)
    {
        std::cerr << "Failed to open test database: " << ctx.message << std::endl;
        return nullptr;
    }

    return db.release();
}

// Test 1: Index creation and opening
void testIndexCreation()
{
    std::cout << "Test 1: Index creation and opening..." << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    ErrorContext ctx;
    UuidV7Bytes index_uuid;
    for (int i = 0; i < 16; i++)
    {
        index_uuid[i] = static_cast<uint8_t>(100 + i);
    }

    // Create index
    uint32_t meta_page = 0;
    auto status = BitmapIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status.isOk());
    assert(meta_page > 0);

    // Open index
    auto index = BitmapIndex::open(db, index_uuid, meta_page, &ctx);
    assert(index != nullptr);
    assert(std::memcmp(index->getUuid(), index_uuid, 16) == 0);

    delete db;
    std::cout << "✓ Index creation and opening works" << std::endl;
}

// Test 2: Basic insertion and single-value queries
void testBasicInsertionAndFind()
{
    std::cout << "\nTest 2: Basic insertion and single-value queries..." << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    ErrorContext ctx;
    UuidV7Bytes index_uuid;
    for (int i = 0; i < 16; i++)
    {
        index_uuid[i] = static_cast<uint8_t>(200 + i);
    }

    uint32_t meta_page = 0;
    auto status = BitmapIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status.isOk());

    auto index = BitmapIndex::open(db, index_uuid, meta_page, &ctx);
    assert(index != nullptr);

    // Insert tuples with gender values
    const char *male = "M";
    const char *female = "F";

    // Insert 100 males and 100 females
    for (uint64_t i = 0; i < 100; i++)
    {
        status = index->insert(male, 1, i, &ctx);
        assert(status.isOk());
    }

    for (uint64_t i = 100; i < 200; i++)
    {
        status = index->insert(female, 1, i, &ctx);
        assert(status.isOk());
    }

    // Find males
    auto males = index->find(male, 1, &ctx);
    assert(males.size() == 100);
    assert(std::is_sorted(males.begin(), males.end()));
    assert(males[0] == 0);
    assert(males[99] == 99);

    // Find females
    auto females = index->find(female, 1, &ctx);
    assert(females.size() == 100);
    assert(std::is_sorted(females.begin(), females.end()));
    assert(females[0] == 100);
    assert(females[99] == 199);

    delete db;
    std::cout << "✓ Basic insertion and find works correctly" << std::endl;
}

// Test 3: Logical AND operations (intersection)
void testLogicalAnd()
{
    std::cout << "\nTest 3: Logical AND operations..." << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    ErrorContext ctx;
    UuidV7Bytes gender_uuid, status_uuid;
    for (int i = 0; i < 16; i++)
    {
        gender_uuid[i] = static_cast<uint8_t>(300 + i);
        status_uuid[i] = static_cast<uint8_t>(400 + i);
    }

    // Create two indexes: one for gender, one for status
    uint32_t gender_meta = 0, status_meta = 0;
    auto s1 = BitmapIndex::create(db, gender_uuid, &gender_meta, &ctx);
    auto s2 = BitmapIndex::create(db, status_uuid, &status_meta, &ctx);
    assert(s1.isOk() && s2.isOk());

    auto gender_index = BitmapIndex::open(db, gender_uuid, gender_meta, &ctx);
    auto status_index = BitmapIndex::open(db, status_uuid, status_meta, &ctx);
    assert(gender_index && status_index);

    // Insert data:
    // Tuples 0-49:   Male + Active
    // Tuples 50-99:  Male + Inactive
    // Tuples 100-149: Female + Active
    // Tuples 150-199: Female + Inactive

    const char *male = "M";
    const char *female = "F";
    const char *active = "A";
    const char *inactive = "I";

    for (uint64_t i = 0; i < 50; i++)
    {
        gender_index->insert(male, 1, i, &ctx);
        status_index->insert(active, 1, i, &ctx);
    }

    for (uint64_t i = 50; i < 100; i++)
    {
        gender_index->insert(male, 1, i, &ctx);
        status_index->insert(inactive, 1, i, &ctx);
    }

    for (uint64_t i = 100; i < 150; i++)
    {
        gender_index->insert(female, 1, i, &ctx);
        status_index->insert(active, 1, i, &ctx);
    }

    for (uint64_t i = 150; i < 200; i++)
    {
        gender_index->insert(female, 1, i, &ctx);
        status_index->insert(inactive, 1, i, &ctx);
    }

    // Query: Gender='M' AND Status='A' (should return tuples 0-49)
    auto males = gender_index->find(male, 1, &ctx);
    auto actives = status_index->find(active, 1, &ctx);

    // Manual intersection
    std::vector<uint64_t> expected_intersection;
    std::set_intersection(males.begin(), males.end(),
                          actives.begin(), actives.end(),
                          std::back_inserter(expected_intersection));

    assert(expected_intersection.size() == 50);
    assert(expected_intersection[0] == 0);
    assert(expected_intersection[49] == 49);

    delete db;
    std::cout << "✓ Logical AND operations work correctly" << std::endl;
}

// Test 4: Logical OR operations (union)
void testLogicalOr()
{
    std::cout << "\nTest 4: Logical OR operations..." << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    ErrorContext ctx;
    UuidV7Bytes country_uuid;
    for (int i = 0; i < 16; i++)
    {
        country_uuid[i] = static_cast<uint8_t>(500 + i);
    }

    uint32_t country_meta = 0;
    auto status = BitmapIndex::create(db, country_uuid, &country_meta, &ctx);
    assert(status.isOk());

    auto country_index = BitmapIndex::open(db, country_uuid, country_meta, &ctx);
    assert(country_index != nullptr);

    // Insert data:
    // Tuples 0-99: USA
    // Tuples 100-199: Canada
    // Tuples 200-299: Mexico

    const char *usa = "USA";
    const char *canada = "CAN";
    const char *mexico = "MEX";

    for (uint64_t i = 0; i < 100; i++)
    {
        country_index->insert(usa, 3, i, &ctx);
    }

    for (uint64_t i = 100; i < 200; i++)
    {
        country_index->insert(canada, 3, i, &ctx);
    }

    for (uint64_t i = 200; i < 300; i++)
    {
        country_index->insert(mexico, 3, i, &ctx);
    }

    // Query: Country='USA' OR Country='Canada' (should return 200 tuples)
    std::vector<const void *> values = {usa, canada};
    std::vector<size_t> value_lens = {3, 3};

    auto results = country_index->findOr(values, value_lens, &ctx);
    assert(results.size() == 200);
    assert(std::is_sorted(results.begin(), results.end()));
    assert(results[0] == 0);
    assert(results[199] == 199);

    delete db;
    std::cout << "✓ Logical OR operations work correctly" << std::endl;
}

// Test 5: Container type conversion (ARRAY -> BITSET at 4096 threshold)
void testContainerConversion()
{
    std::cout << "\nTest 5: Container type conversion..." << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    ErrorContext ctx;
    UuidV7Bytes index_uuid;
    for (int i = 0; i < 16; i++)
    {
        index_uuid[i] = static_cast<uint8_t>(600 + i);
    }

    uint32_t meta_page = 0;
    auto status = BitmapIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status.isOk());

    auto index = BitmapIndex::open(db, index_uuid, meta_page, &ctx);
    assert(index != nullptr);

    // Insert a large number of tuples with the same value
    // This should trigger ARRAY -> BITSET conversion
    const char *value = "COMMON";

    // Insert 5000 tuples (exceeds 4096 threshold)
    for (uint64_t i = 0; i < 5000; i++)
    {
        status = index->insert(value, 6, i, &ctx);
        assert(status.isOk());
    }

    // Query should return all 5000 tuples
    auto results = index->find(value, 6, &ctx);
    assert(results.size() == 5000);
    assert(std::is_sorted(results.begin(), results.end()));

    // Verify all tuple IDs are present
    for (uint64_t i = 0; i < 5000; i++)
    {
        assert(results[i] == i);
    }

    delete db;
    std::cout << "✓ Container type conversion works correctly" << std::endl;
}

// Test 6: Multiple distinct values (low-cardinality scenario)
void testMultipleDistinctValues()
{
    std::cout << "\nTest 6: Multiple distinct values..." << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    ErrorContext ctx;
    UuidV7Bytes index_uuid;
    for (int i = 0; i < 16; i++)
    {
        index_uuid[i] = static_cast<uint8_t>(700 + i);
    }

    uint32_t meta_page = 0;
    auto status = BitmapIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status.isOk());

    auto index = BitmapIndex::open(db, index_uuid, meta_page, &ctx);
    assert(index != nullptr);

    // Insert 10 distinct priority levels (0-9), 100 tuples each
    for (int priority = 0; priority < 10; priority++)
    {
        char value[2] = {static_cast<char>('0' + priority), '\0'};
        for (uint64_t i = priority * 100; i < (priority + 1) * 100; i++)
        {
            status = index->insert(value, 1, i, &ctx);
            assert(status.isOk());
        }
    }

    // Query each priority level
    for (int priority = 0; priority < 10; priority++)
    {
        char value[2] = {static_cast<char>('0' + priority), '\0'};
        auto results = index->find(value, 1, &ctx);
        assert(results.size() == 100);
        assert(results[0] == static_cast<uint64_t>(priority * 100));
        assert(results[99] == static_cast<uint64_t>(priority * 100 + 99));
    }

    // Get statistics
    auto stats = index->getStatistics(&ctx);
    assert(stats.num_distinct_values == 10);
    assert(stats.total_tuples == 1000);

    delete db;
    std::cout << "✓ Multiple distinct values work correctly" << std::endl;
}

// Test 7: Edge case - Empty index
void testEmptyIndex()
{
    std::cout << "\nTest 7: Edge case - Empty index..." << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    ErrorContext ctx;
    UuidV7Bytes index_uuid;
    for (int i = 0; i < 16; i++)
    {
        index_uuid[i] = static_cast<uint8_t>(800 + i);
    }

    uint32_t meta_page = 0;
    auto status = BitmapIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status.isOk());

    auto index = BitmapIndex::open(db, index_uuid, meta_page, &ctx);
    assert(index != nullptr);

    // Query empty index
    const char *value = "X";
    auto results = index->find(value, 1, &ctx);
    assert(results.empty());

    // Get statistics
    auto stats = index->getStatistics(&ctx);
    assert(stats.num_distinct_values == 0);
    assert(stats.total_tuples == 0);

    delete db;
    std::cout << "✓ Empty index edge case handled correctly" << std::endl;
}

// Test 8: Edge case - Duplicate insertions
void testDuplicateInsertions()
{
    std::cout << "\nTest 8: Edge case - Duplicate insertions..." << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    ErrorContext ctx;
    UuidV7Bytes index_uuid;
    for (int i = 0; i < 16; i++)
    {
        index_uuid[i] = static_cast<uint8_t>(900 + i);
    }

    uint32_t meta_page = 0;
    auto status = BitmapIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status.isOk());

    auto index = BitmapIndex::open(db, index_uuid, meta_page, &ctx);
    assert(index != nullptr);

    const char *value = "A";

    // Insert same tuple ID multiple times
    for (int i = 0; i < 5; i++)
    {
        status = index->insert(value, 1, 42, &ctx);
        assert(status.isOk());
    }

    // Query should return tuple 42 only once
    auto results = index->find(value, 1, &ctx);
    assert(results.size() == 1);
    assert(results[0] == 42);

    delete db;
    std::cout << "✓ Duplicate insertions handled correctly" << std::endl;
}

// Test 9: Complex multi-value AND query
void testComplexAndQuery()
{
    std::cout << "\nTest 9: Complex multi-value AND query..." << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    ErrorContext ctx;
    UuidV7Bytes gender_uuid, status_uuid, country_uuid;
    for (int i = 0; i < 16; i++)
    {
        gender_uuid[i] = static_cast<uint8_t>(1000 + i);
        status_uuid[i] = static_cast<uint8_t>(1100 + i);
        country_uuid[i] = static_cast<uint8_t>(1200 + i);
    }

    uint32_t gender_meta = 0, status_meta = 0, country_meta = 0;
    BitmapIndex::create(db, gender_uuid, &gender_meta, &ctx);
    BitmapIndex::create(db, status_uuid, &status_meta, &ctx);
    BitmapIndex::create(db, country_uuid, &country_meta, &ctx);

    auto gender_idx = BitmapIndex::open(db, gender_uuid, gender_meta, &ctx);
    auto status_idx = BitmapIndex::open(db, status_uuid, status_meta, &ctx);
    auto country_idx = BitmapIndex::open(db, country_uuid, country_meta, &ctx);

    const char *male = "M", *female = "F";
    const char *active = "A", *inactive = "I";
    const char *usa = "USA", *canada = "CAN";

    // Insert test data
    // Tuples 0-24: Male, Active, USA (target)
    // Tuples 25-49: Male, Active, Canada
    // Tuples 50-74: Male, Inactive, USA
    // Tuples 75-99: Female, Active, USA

    for (uint64_t i = 0; i < 25; i++)
    {
        gender_idx->insert(male, 1, i, &ctx);
        status_idx->insert(active, 1, i, &ctx);
        country_idx->insert(usa, 3, i, &ctx);
    }

    for (uint64_t i = 25; i < 50; i++)
    {
        gender_idx->insert(male, 1, i, &ctx);
        status_idx->insert(active, 1, i, &ctx);
        country_idx->insert(canada, 3, i, &ctx);
    }

    for (uint64_t i = 50; i < 75; i++)
    {
        gender_idx->insert(male, 1, i, &ctx);
        status_idx->insert(inactive, 1, i, &ctx);
        country_idx->insert(usa, 3, i, &ctx);
    }

    for (uint64_t i = 75; i < 100; i++)
    {
        gender_idx->insert(female, 1, i, &ctx);
        status_idx->insert(active, 1, i, &ctx);
        country_idx->insert(usa, 3, i, &ctx);
    }

    // Query: Gender='M' AND Status='A' AND Country='USA'
    auto males = gender_idx->find(male, 1, &ctx);
    auto actives = status_idx->find(active, 1, &ctx);
    auto usa_residents = country_idx->find(usa, 3, &ctx);

    // Three-way intersection
    std::vector<uint64_t> temp, result;
    std::set_intersection(males.begin(), males.end(),
                          actives.begin(), actives.end(),
                          std::back_inserter(temp));
    std::set_intersection(temp.begin(), temp.end(),
                          usa_residents.begin(), usa_residents.end(),
                          std::back_inserter(result));

    assert(result.size() == 25);
    assert(result[0] == 0);
    assert(result[24] == 24);

    delete db;
    std::cout << "✓ Complex multi-value AND query works correctly" << std::endl;
}

// Test 10: Performance benchmark
void testPerformanceBenchmark()
{
    std::cout << "\nTest 10: Performance benchmark..." << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    ErrorContext ctx;
    UuidV7Bytes index_uuid;
    for (int i = 0; i < 16; i++)
    {
        index_uuid[i] = static_cast<uint8_t>(1300 + i);
    }

    uint32_t meta_page = 0;
    auto status = BitmapIndex::create(db, index_uuid, &meta_page, &ctx);
    assert(status.isOk());

    auto index = BitmapIndex::open(db, index_uuid, meta_page, &ctx);
    assert(index != nullptr);

    // Insert 10,000 tuples across 5 categories
    const char *categories[] = {"CAT_A", "CAT_B", "CAT_C", "CAT_D", "CAT_E"};

    auto start = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < 10000; i++)
    {
        const char *category = categories[i % 5];
        status = index->insert(category, 5, i, &ctx);
        assert(status.isOk());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "  Inserted 10,000 tuples in " << duration.count() << "ms" << std::endl;

    // Query performance
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 5; i++)
    {
        auto results = index->find(categories[i], 5, &ctx);
        assert(results.size() == 2000);
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "  Queried 5 categories (10,000 results) in " << duration.count() << "ms" << std::endl;

    // OR query performance
    std::vector<const void *> values = {categories[0], categories[1], categories[2]};
    std::vector<size_t> value_lens = {5, 5, 5};

    start = std::chrono::high_resolution_clock::now();
    auto or_results = index->findOr(values, value_lens, &ctx);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    assert(or_results.size() == 6000);
    std::cout << "  OR query (6,000 results) completed in " << duration.count() << "ms" << std::endl;

    delete db;
    std::cout << "✓ Performance benchmark completed" << std::endl;
}

int main()
{
    std::cout << "=== ScratchBird Bitmap Index Test Suite ===" << std::endl;
    std::cout << "Testing Roaring Bitmap implementation for low-cardinality columns\n" << std::endl;

    try
    {
        testIndexCreation();
        testBasicInsertionAndFind();
        testLogicalAnd();
        testLogicalOr();
        testContainerConversion();
        testMultipleDistinctValues();
        testEmptyIndex();
        testDuplicateInsertions();
        testComplexAndQuery();
        testPerformanceBenchmark();

        std::cout << "\n=== All 10 tests passed! ===" << std::endl;
        std::cout << "Bitmap index implementation is working correctly." << std::endl;

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n!!! Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
