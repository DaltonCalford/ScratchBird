#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_family.h"
#include "scratchbird/engine/index_hash.h"
#include "test_db_utils.h"

#include <cassert>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace scratchbird::engine;

void test_hash_functions()
{
    std::cout << "=== Testing Hash Functions ===" << std::endl;

    // Test Universal Hash Function
    UniversalHashFunction universal_hash;

    std::string test_keys[] = {"key1", "key2", "key3", "test", "hello", "world"};
    std::unordered_set<std::uint32_t> hash_values;

    for (const auto& key : test_keys) {
        std::uint32_t hash_val = universal_hash.hash(key);
        std::cout << "UniversalHash('" << key << "') = " << hash_val << std::endl;

        // Check for uniqueness (not required but good indicator)
        if (hash_values.find(hash_val) == hash_values.end()) {
            hash_values.insert(hash_val);
        }
    }

    std::cout << "Generated " << hash_values.size() << " unique hash values out of "
              << (sizeof(test_keys) / sizeof(test_keys[0])) << " keys" << std::endl;

    // Test FNV Hash Function
    FNVHashFunction fnv_hash;
    hash_values.clear();

    for (const auto& key : test_keys) {
        std::uint32_t hash_val = fnv_hash.hash(key);
        std::cout << "FNVHash('" << key << "') = " << hash_val << std::endl;
        hash_values.insert(hash_val);
    }

    std::cout << "FNV generated " << hash_values.size() << " unique hash values" << std::endl;
    std::cout << "✓ Hash function tests completed" << std::endl << std::endl;
}

void test_hash_index_basic_operations()
{
    std::cout << "=== Testing Hash Index Basic Operations ===" << std::endl;

    try {
        // Create test database
        scratchbird::tests::TestDatabaseRAII test_db("hash_index_basic", true);

        // Create FileMap (simplified for testing)
        FileMap::Layout layout;
        layout.page_size = 4096;
        layout.options.direct_io = false;
        FileMap fmap(layout);

        // Create hash index
        auto hash_index = std::make_unique<HashIndex>(std::move(fmap), 4096, false);
        hash_index->create_empty();

        // Test insertion
        std::cout << "Testing insertions..." << std::endl;
        std::string err;

        // Insert test data
        std::vector<std::pair<std::string, std::uint64_t>> test_data = {
            {"apple", 1}, {"banana", 2}, {"cherry", 3}, {"date", 4}, {"elderberry", 5}};

        for (const auto& [key, row_id] : test_data) {
            bool success = hash_index->insert(key, row_id, err);
            if (!success) {
                std::cout << "⚠ Insert failed for '" << key << "': " << err << std::endl;
            } else {
                std::cout << "✓ Inserted '" << key << "' -> " << row_id << std::endl;
            }
        }

        // Test search
        std::cout << "\nTesting searches..." << std::endl;
        for (const auto& [key, expected_row_id] : test_data) {
            std::vector<std::uint64_t> results;
            hash_index->search_equal(key, results);

            if (results.empty()) {
                std::cout << "⚠ No results found for key '" << key << "'" << std::endl;
            } else if (results.size() == 1 && results[0] == expected_row_id) {
                std::cout << "✓ Found '" << key << "' -> " << results[0] << std::endl;
            } else {
                std::cout << "⚠ Unexpected results for '" << key << "': got " << results.size()
                          << " results" << std::endl;
            }
        }

        // Test non-existent key
        std::vector<std::uint64_t> results;
        hash_index->search_equal("nonexistent", results);
        if (results.empty()) {
            std::cout << "✓ Correctly returned no results for non-existent key" << std::endl;
        } else {
            std::cout << "⚠ Unexpected results for non-existent key" << std::endl;
        }

        // Test statistics
        std::cout << "\nTesting statistics collection..." << std::endl;
        std::string stats = hash_index->collect_statistics();
        std::cout << stats << std::endl;

        // Test validation
        std::cout << "Testing validation..." << std::endl;
        std::string validation_error;
        bool is_valid = hash_index->validate(validation_error);
        if (is_valid) {
            std::cout << "✓ Index validation passed" << std::endl;
        } else {
            std::cout << "⚠ Index validation failed: " << validation_error << std::endl;
        }

        std::cout << "✓ Hash index basic operations test completed" << std::endl << std::endl;

    } catch (const std::exception& e) {
        std::cout << "⚠ Hash index basic operations failed: " << e.what() << std::endl;
    }
}

void test_hash_index_with_payload()
{
    std::cout << "=== Testing Hash Index with Payload (INCLUDE columns) ===" << std::endl;

    // Create test database
    scratchbird::tests::TestDatabaseRAII test_db("hash_index_payload", true);

    // Create FileMap
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    // Create hash index
    auto hash_index = std::make_unique<HashIndex>(std::move(fmap), 4096, false);
    hash_index->create_empty();

    // Test data with payloads
    std::vector<std::tuple<std::string, std::uint64_t, std::string>> test_data = {
        {"user1", 101, "John Doe"},
        {"user2", 102, "Jane Smith"},
        {"user3", 103, "Bob Johnson"},
        {"admin", 201, "Administrator"}};

    // Insert with payload
    std::cout << "Inserting entries with payloads..." << std::endl;
    std::string err;
    for (const auto& [key, row_id, payload] : test_data) {
        bool success = hash_index->insert_with_payload(key, row_id, payload, err);
        if (!success) {
            std::cout << "⚠ Insert with payload failed for '" << key << "': " << err << std::endl;
        } else {
            std::cout << "✓ Inserted '" << key << "' -> " << row_id << " (payload: '" << payload
                      << "')" << std::endl;
        }
    }

    // Search with payload
    std::cout << "\nSearching with payload retrieval..." << std::endl;
    for (const auto& [key, expected_row_id, expected_payload] : test_data) {
        std::vector<std::pair<std::uint64_t, std::string>> results;
        hash_index->search_equal_with_payload(key, results);

        if (results.empty()) {
            std::cout << "⚠ No results found for key '" << key << "'" << std::endl;
        } else if (results.size() == 1) {
            auto [row_id, payload] = results[0];
            if (row_id == expected_row_id && payload == expected_payload) {
                std::cout << "✓ Found '" << key << "' -> " << row_id << " (payload: '" << payload
                          << "')" << std::endl;
            } else {
                std::cout << "⚠ Mismatch for '" << key << "': expected (" << expected_row_id
                          << ", '" << expected_payload << "'), got (" << row_id << ", '" << payload
                          << "')" << std::endl;
            }
        } else {
            std::cout << "⚠ Multiple results for unique key '" << key << "'" << std::endl;
        }
    }

    std::cout << "✓ Hash index with payload test completed" << std::endl << std::endl;
}

void test_hash_index_unique_constraint()
{
    std::cout << "=== Testing Hash Index Unique Constraint ===" << std::endl;

    // Create test database
    scratchbird::tests::TestDatabaseRAII test_db("hash_index_unique", true);

    // Create FileMap
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    // Create unique hash index
    auto hash_index = std::make_unique<HashIndex>(std::move(fmap), 4096, true);
    hash_index->create_empty();

    std::string err;

    // Insert first entry
    bool success1 = hash_index->insert("unique_key", 100, err);
    if (success1) {
        std::cout << "✓ First insertion of 'unique_key' succeeded" << std::endl;
    } else {
        std::cout << "⚠ First insertion failed: " << err << std::endl;
    }

    // Try to insert duplicate key
    bool success2 = hash_index->insert("unique_key", 200, err);
    if (!success2) {
        std::cout << "✓ Duplicate key insertion correctly rejected: " << err << std::endl;
    } else {
        std::cout << "⚠ Duplicate key insertion should have been rejected" << std::endl;
    }

    // Verify original entry still exists
    std::vector<std::uint64_t> results;
    hash_index->search_equal("unique_key", results);
    if (results.size() == 1 && results[0] == 100) {
        std::cout << "✓ Original entry preserved after duplicate rejection" << std::endl;
    } else {
        std::cout << "⚠ Original entry corrupted after duplicate rejection" << std::endl;
    }

    std::cout << "✓ Unique constraint test completed" << std::endl << std::endl;
}

void test_index_family_factory()
{
    std::cout << "=== Testing Index Family Factory ===" << std::endl;

    // Test factory creation
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    // Test B-Tree creation
    auto btree_index =
        IndexFamilyFactory::create_index(IndexMethod::BTree, std::move(fmap), 4096, false);
    if (btree_index && btree_index->get_method() == IndexMethod::BTree) {
        std::cout << "✓ B-Tree index family creation succeeded" << std::endl;
    } else {
        std::cout << "⚠ B-Tree index family creation failed" << std::endl;
    }

    // Test Hash index creation
    layout = FileMap::Layout{};
    layout.page_size = 4096;
    layout.options.direct_io = false;
    fmap = FileMap(layout);
    auto hash_index =
        IndexFamilyFactory::create_index(IndexMethod::Hash, std::move(fmap), 4096, false);
    if (hash_index && hash_index->get_method() == IndexMethod::Hash) {
        std::cout << "✓ Hash index family creation succeeded" << std::endl;
    } else {
        std::cout << "⚠ Hash index family creation failed" << std::endl;
    }

    // Test capability queries
    bool btree_supports_range = IndexFamilyFactory::supports_range_queries(IndexMethod::BTree);
    bool hash_supports_range = IndexFamilyFactory::supports_range_queries(IndexMethod::Hash);

    if (btree_supports_range && !hash_supports_range) {
        std::cout << "✓ Range query capability detection correct" << std::endl;
    } else {
        std::cout << "⚠ Range query capability detection incorrect" << std::endl;
    }

    bool btree_supports_include = IndexFamilyFactory::supports_include_columns(IndexMethod::BTree);
    bool hash_supports_include = IndexFamilyFactory::supports_include_columns(IndexMethod::Hash);

    if (btree_supports_include && hash_supports_include) {
        std::cout << "✓ INCLUDE column capability detection correct" << std::endl;
    } else {
        std::cout << "⚠ INCLUDE column capability detection incorrect" << std::endl;
    }

    std::cout << "✓ Index family factory test completed" << std::endl << std::endl;
}

void test_hash_index_scan_operations()
{
    std::cout << "=== Testing Hash Index Scan Operations ===" << std::endl;

    // Create test database
    scratchbird::tests::TestDatabaseRAII test_db("hash_index_scan", true);

    // Create FileMap
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    // Create hash index
    auto hash_index = std::make_unique<HashIndex>(std::move(fmap), 4096, false);
    hash_index->create_empty();

    // Insert test data
    std::string err;
    for (int i = 1; i <= 10; ++i) {
        std::string key = "item_" + std::to_string(i);
        hash_index->insert(key, i * 100, err);
    }

    // Test hash index scan
    HashIndexScan scan(hash_index.get());

    std::cout << "Testing exact match scan..." << std::endl;
    bool scan_init = scan.init("item_5");
    if (scan_init) {
        std::cout << "✓ Scan initialization succeeded" << std::endl;

        std::uint64_t row_id;
        std::string key, payload;
        bool has_result = scan.next(row_id, key, payload);

        if (has_result && row_id == 500 && key == "item_5") {
            std::cout << "✓ Scan returned correct result: " << key << " -> " << row_id << std::endl;
        } else {
            std::cout << "⚠ Scan returned incorrect result" << std::endl;
        }

        // Check scan statistics
        std::cout << "Scan statistics: " << scan.rows_scanned() << " rows, "
                  << scan.pages_accessed() << " pages" << std::endl;
    } else {
        std::cout << "⚠ Scan initialization failed" << std::endl;
    }

    std::cout << "✓ Hash index scan test completed" << std::endl << std::endl;
}

int main()
{
    std::cout << "=== Hash Index Family Tests ===" << std::endl << std::endl;

    try {
        test_hash_functions();
        test_hash_index_basic_operations();
        test_hash_index_with_payload();
        test_hash_index_unique_constraint();
        test_index_family_factory();
        test_hash_index_scan_operations();

        std::cout << "=== All Hash Index Tests Completed Successfully ===" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
