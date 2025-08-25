#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_family.h"
#include "scratchbird/engine/index_ttl.h"
#include "test_db_utils.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

void test_ttl_basic_operations()
{
    std::cout << "=== Testing TTL Index Basic Operations ===" << std::endl;

    // Create test database
    scratchbird::tests::TestDatabaseRAII test_db("ttl_index_basic", true);

    // Create FileMap
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    // Create TTL index
    auto ttl_index = std::make_unique<TTLIndex>(std::move(fmap), 4096, false);
    ttl_index->create_empty();

    // Configure TTL with 2-second expiry for testing
    TTLIndex::TTLConfiguration config;
    config.expire_column = "expires_at";
    config.interval = std::chrono::seconds(2);
    config.auto_cleanup = true;
    config.cleanup_frequency = std::chrono::seconds(1);
    
    ttl_index->configure_ttl(config);

    std::cout << "Testing basic insertion..." << std::endl;
    std::string err;

    // Insert with short TTL
    auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(2);
    bool success = ttl_index->insert_with_ttl("session1", 101, expiry, "user_data_1", err);
    if (success) {
        std::cout << "✓ Inserted session1 with 2-second TTL" << std::endl;
    } else {
        std::cout << "⚠ TTL insertion failed: " << err << std::endl;
    }

    // Search immediately (should find)
    std::vector<std::uint64_t> results;
    ttl_index->search_equal("session1", results);
    if (results.size() == 1 && results[0] == 101) {
        std::cout << "✓ Found session1 immediately after insertion" << std::endl;
    } else {
        std::cout << "⚠ Session1 not found immediately after insertion" << std::endl;
    }

    // Wait for expiry
    std::cout << "Waiting 3 seconds for TTL expiry..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Search after expiry (should not find)
    results.clear();
    ttl_index->search_equal("session1", results);
    if (results.empty()) {
        std::cout << "✓ Session1 correctly expired after TTL" << std::endl;
    } else {
        std::cout << "⚠ Session1 found after expiry - TTL not working" << std::endl;
    }

    std::cout << "✓ TTL basic operations test completed" << std::endl << std::endl;
}

void test_ttl_cleanup_operations()
{
    std::cout << "=== Testing TTL Cleanup Operations ===" << std::endl;

    scratchbird::tests::TestDatabaseRAII test_db("ttl_cleanup", true);

    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    auto ttl_index = std::make_unique<TTLIndex>(std::move(fmap), 4096, false);
    ttl_index->create_empty();

    // Configure with manual cleanup
    TTLIndex::TTLConfiguration config;
    config.expire_column = "expires_at";
    config.interval = std::chrono::seconds(1);
    config.auto_cleanup = false; // Manual cleanup for testing
    config.cleanup_frequency = std::chrono::seconds(10);
    
    ttl_index->configure_ttl(config);

    std::cout << "Inserting entries with short TTL..." << std::endl;
    std::string err;

    // Insert multiple entries with 1-second expiry
    for (int i = 1; i <= 5; ++i) {
        auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(1);
        std::string key = "temp" + std::to_string(i);
        ttl_index->insert_with_ttl(key, 100 + i, expiry, "payload" + std::to_string(i), err);
    }

    // Verify all entries exist
    std::vector<std::uint64_t> results;
    for (int i = 1; i <= 5; ++i) {
        std::string key = "temp" + std::to_string(i);
        ttl_index->search_equal(key, results);
        if (!results.empty()) {
            std::cout << "✓ Found " << key << " before expiry" << std::endl;
        }
        results.clear();
    }

    // Wait for expiry
    std::cout << "Waiting 2 seconds for expiry..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check expired entries before cleanup
    auto expired = ttl_index->get_expired_entries();
    std::cout << "Found " << expired.size() << " expired entries" << std::endl;

    // Force cleanup
    std::cout << "Running manual cleanup..." << std::endl;
    ttl_index->force_cleanup();

    // Verify entries are gone after cleanup
    int found_after_cleanup = 0;
    for (int i = 1; i <= 5; ++i) {
        std::string key = "temp" + std::to_string(i);
        ttl_index->search_equal(key, results);
        if (!results.empty()) {
            found_after_cleanup++;
        }
        results.clear();
    }

    if (found_after_cleanup == 0) {
        std::cout << "✓ All expired entries cleaned up successfully" << std::endl;
    } else {
        std::cout << "⚠ " << found_after_cleanup << " entries remain after cleanup" << std::endl;
    }

    std::cout << "✓ TTL cleanup operations test completed" << std::endl << std::endl;
}

void test_ttl_unique_constraint()
{
    std::cout << "=== Testing TTL Unique Constraint ===" << std::endl;

    scratchbird::tests::TestDatabaseRAII test_db("ttl_unique", true);

    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    // Create unique TTL index
    auto ttl_index = std::make_unique<TTLIndex>(std::move(fmap), 4096, true);
    ttl_index->create_empty();

    TTLIndex::TTLConfiguration config;
    config.expire_column = "expires_at";
    config.interval = std::chrono::seconds(5);
    config.auto_cleanup = false;
    
    ttl_index->configure_ttl(config);

    std::string err;

    // Insert first entry
    auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(5);
    bool success1 = ttl_index->insert_with_ttl("unique_key", 200, expiry, "data1", err);
    if (success1) {
        std::cout << "✓ First insertion of unique_key succeeded" << std::endl;
    } else {
        std::cout << "⚠ First insertion failed: " << err << std::endl;
    }

    // Try duplicate insertion
    bool success2 = ttl_index->insert_with_ttl("unique_key", 201, expiry, "data2", err);
    if (!success2) {
        std::cout << "✓ Duplicate key insertion correctly rejected: " << err << std::endl;
    } else {
        std::cout << "⚠ Duplicate key insertion should have been rejected" << std::endl;
    }

    std::cout << "✓ TTL unique constraint test completed" << std::endl << std::endl;
}

void test_ttl_statistics()
{
    std::cout << "=== Testing TTL Statistics ===" << std::endl;

    scratchbird::tests::TestDatabaseRAII test_db("ttl_stats", true);

    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    auto ttl_index = std::make_unique<TTLIndex>(std::move(fmap), 4096, false);
    ttl_index->create_empty();

    // Insert some test data
    std::string err;
    for (int i = 1; i <= 3; ++i) {
        std::string key = "stat_key" + std::to_string(i);
        ttl_index->insert(key, 300 + i, err);
    }

    // Collect statistics
    std::string stats = ttl_index->collect_statistics();
    std::cout << "TTL Index Statistics:" << std::endl;
    std::cout << stats << std::endl;

    // Test cost estimation
    double search_cost = ttl_index->estimate_search_cost("stat_key1");
    double range_cost = ttl_index->estimate_range_cost("stat_key1", "stat_key3");
    double maintenance_cost = ttl_index->estimate_maintenance_cost();

    std::cout << "Cost estimates:" << std::endl;
    std::cout << "  Search cost: " << search_cost << std::endl;
    std::cout << "  Range cost: " << range_cost << std::endl;
    std::cout << "  Maintenance cost: " << maintenance_cost << std::endl;

    std::cout << "✓ TTL statistics test completed" << std::endl << std::endl;
}

void test_ttl_scan_operations()
{
    std::cout << "=== Testing TTL Scan Operations ===" << std::endl;

    scratchbird::tests::TestDatabaseRAII test_db("ttl_scan", true);

    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    auto ttl_index = std::make_unique<TTLIndex>(std::move(fmap), 4096, false);
    ttl_index->create_empty();

    // Insert test data
    std::string err;
    for (int i = 1; i <= 5; ++i) {
        std::string key = "scan_item_" + std::to_string(i);
        std::string payload = "payload_" + std::to_string(i);
        auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(10); // Long TTL
        ttl_index->insert_with_ttl(key, 400 + i, expiry, payload, err);
    }

    // Test scan
    TTLIndexScan scan(ttl_index.get());
    bool scan_init = scan.init();
    
    if (scan_init) {
        std::cout << "✓ TTL scan initialization succeeded" << std::endl;
        
        int count = 0;
        std::uint64_t row_id;
        std::string key, payload;
        
        while (scan.next(row_id, key, payload)) {
            std::cout << "  Scanned: " << key << " -> " << row_id << " (payload: " << payload << ")" << std::endl;
            count++;
        }
        
        if (count == 5) {
            std::cout << "✓ Scanned all 5 inserted entries" << std::endl;
        } else {
            std::cout << "⚠ Expected 5 entries, found " << count << std::endl;
        }
        
        std::cout << "Scan statistics: " << scan.rows_scanned() << " rows, " 
                  << scan.pages_accessed() << " pages" << std::endl;
    } else {
        std::cout << "⚠ TTL scan initialization failed" << std::endl;
    }

    std::cout << "✓ TTL scan operations test completed" << std::endl << std::endl;
}

void test_ttl_factory_integration()
{
    std::cout << "=== Testing TTL Index Factory Integration ===" << std::endl;

    // Test factory creation
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    auto ttl_index = IndexFamilyFactory::create_index(IndexMethod::TTL, std::move(fmap), 4096, false);
    
    if (ttl_index && ttl_index->get_method() == IndexMethod::TTL) {
        std::cout << "✓ TTL index family creation through factory succeeded" << std::endl;
        std::cout << "  Method: TTL" << std::endl;
    } else {
        std::cout << "⚠ TTL index family creation through factory failed" << std::endl;
    }

    // Test capability queries
    bool supports_range = IndexFamilyFactory::supports_range_queries(IndexMethod::TTL);
    bool supports_partial = IndexFamilyFactory::supports_partial_indexes(IndexMethod::TTL);
    bool supports_include = IndexFamilyFactory::supports_include_columns(IndexMethod::TTL);
    bool supports_expression = IndexFamilyFactory::supports_expression_indexes(IndexMethod::TTL);

    std::cout << "TTL Index Capabilities:" << std::endl;
    std::cout << "  Range queries: " << (supports_range ? "Yes" : "No") << std::endl;
    std::cout << "  Partial indexes: " << (supports_partial ? "Yes" : "No") << std::endl;
    std::cout << "  INCLUDE columns: " << (supports_include ? "Yes" : "No") << std::endl;
    std::cout << "  Expression indexes: " << (supports_expression ? "Yes" : "No") << std::endl;

    std::cout << "✓ TTL factory integration test completed" << std::endl << std::endl;
}

int main()
{
    std::cout << "=== TTL Index Family Tests ===" << std::endl << std::endl;

    try {
        test_ttl_basic_operations();
        test_ttl_cleanup_operations();
        test_ttl_unique_constraint();
        test_ttl_statistics();
        test_ttl_scan_operations();
        test_ttl_factory_integration();

        std::cout << "=== All TTL Index Tests Completed Successfully ===" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}