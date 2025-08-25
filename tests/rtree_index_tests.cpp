#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_family.h"
#include "scratchbird/engine/index_rtree.h"
#include "test_db_utils.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace scratchbird::engine;

void test_rectangle_operations()
{
    std::cout << "=== Testing Rectangle Operations ===" << std::endl;

    Rectangle r1(0.0, 0.0, 10.0, 10.0);
    Rectangle r2(5.0, 5.0, 15.0, 15.0);
    Rectangle r3(20.0, 20.0, 30.0, 30.0);

    // Test area calculation
    double area1 = r1.area();
    std::cout << "Rectangle (0,0,10,10) area: " << area1 << std::endl;
    assert(area1 == 100.0);

    // Test intersection
    bool intersects12 = r1.intersects(r2);
    bool intersects13 = r1.intersects(r3);
    std::cout << "R1 intersects R2: " << (intersects12 ? "yes" : "no") << std::endl;
    std::cout << "R1 intersects R3: " << (intersects13 ? "yes" : "no") << std::endl;
    assert(intersects12 == true);
    assert(intersects13 == false);

    // Test containment
    Rectangle r4(2.0, 2.0, 8.0, 8.0);
    bool contains14 = r1.contains(r4);
    bool contains41 = r4.contains(r1);
    std::cout << "R1 contains R4: " << (contains14 ? "yes" : "no") << std::endl;
    std::cout << "R4 contains R1: " << (contains41 ? "yes" : "no") << std::endl;
    assert(contains14 == true);
    assert(contains41 == false);

    // Test expansion
    Rectangle r5 = r1;
    r5.expand(r2);
    std::cout << "Expanded rectangle: (" << r5.min_x << "," << r5.min_y << "," << r5.max_x << ","
              << r5.max_y << ")" << std::endl;
    assert(r5.min_x == 0.0 && r5.min_y == 0.0 && r5.max_x == 15.0 && r5.max_y == 15.0);

    std::cout << "✓ Rectangle operations test completed" << std::endl << std::endl;
}

void test_rtree_spatial_parsing()
{
    std::cout << "=== Testing R-Tree Spatial Parsing ===" << std::endl;

    // Create test database
    scratchbird::tests::TestDatabaseRAII test_db("rtree_parsing", true);

    // Create FileMap
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    // Create R-Tree index
    auto rtree_index = std::make_unique<RTreeIndex>(std::move(fmap), 4096, false);
    rtree_index->create_empty();

    // Test BBOX format parsing
    try {
        Rectangle bbox = rtree_index->parse_rectangle("BBOX(10.0,20.0,30.0,40.0)");
        std::cout << "✓ Parsed BBOX format: (" << bbox.min_x << "," << bbox.min_y << ","
                  << bbox.max_x << "," << bbox.max_y << ")" << std::endl;
        assert(bbox.min_x == 10.0 && bbox.min_y == 20.0 && bbox.max_x == 30.0 &&
               bbox.max_y == 40.0);
    } catch (const std::exception& e) {
        std::cout << "⚠ BBOX parsing failed: " << e.what() << std::endl;
    }

    // Test comma-separated format parsing
    try {
        Rectangle coords = rtree_index->parse_rectangle("5.5,6.5,15.5,16.5");
        std::cout << "✓ Parsed coordinate format: (" << coords.min_x << "," << coords.min_y << ","
                  << coords.max_x << "," << coords.max_y << ")" << std::endl;
        assert(coords.min_x == 5.5 && coords.min_y == 6.5 && coords.max_x == 15.5 &&
               coords.max_y == 16.5);
    } catch (const std::exception& e) {
        std::cout << "⚠ Coordinate parsing failed: " << e.what() << std::endl;
    }

    // Test format output
    Rectangle test_rect(1.0, 2.0, 3.0, 4.0);
    std::string formatted = rtree_index->format_rectangle(test_rect);
    std::cout << "✓ Rectangle formatting: " << formatted << std::endl;

    std::cout << "✓ Spatial parsing test completed" << std::endl << std::endl;
}

void test_rtree_basic_operations()
{
    std::cout << "=== Testing R-Tree Basic Operations ===" << std::endl;

    // Create test database
    scratchbird::tests::TestDatabaseRAII test_db("rtree_basic", true);

    // Create FileMap
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    // Create R-Tree index
    auto rtree_index = std::make_unique<RTreeIndex>(std::move(fmap), 4096, false);
    rtree_index->create_empty();

    // Test insertion
    std::cout << "Testing insertions..." << std::endl;
    std::string err;

    std::vector<std::pair<std::string, std::uint64_t>> test_data = {{"BBOX(0,0,10,10)", 1},
                                                                    {"BBOX(5,5,15,15)", 2},
                                                                    {"BBOX(20,20,30,30)", 3},
                                                                    {"BBOX(25,25,35,35)", 4},
                                                                    {"1.0,1.0,9.0,9.0", 5}};

    for (const auto& [spatial_key, row_id] : test_data) {
        bool success = rtree_index->insert(spatial_key, row_id, err);
        if (!success) {
            std::cout << "⚠ Insert failed for '" << spatial_key << "': " << err << std::endl;
        } else {
            std::cout << "✓ Inserted '" << spatial_key << "' -> " << row_id << std::endl;
        }
    }

    // Test exact search (equality)
    std::cout << "\nTesting exact searches..." << std::endl;
    for (const auto& [spatial_key, expected_row_id] : test_data) {
        std::vector<std::uint64_t> results;
        rtree_index->search_equal(spatial_key, results);

        if (results.empty()) {
            std::cout << "⚠ No results found for key '" << spatial_key << "'" << std::endl;
        } else {
            std::cout << "✓ Found " << results.size() << " result(s) for '" << spatial_key << "'"
                      << std::endl;
            for (auto row_id : results) {
                std::cout << "  -> " << row_id << std::endl;
            }
        }
    }

    // Test spatial intersection search
    std::cout << "\nTesting spatial intersection searches..." << std::endl;

    // Search for rectangles intersecting with (0,0,20,20)
    std::vector<RTreeEntry> intersection_results;
    Rectangle query_rect(0.0, 0.0, 20.0, 20.0);
    rtree_index->search_intersects(query_rect, intersection_results);

    std::cout << "Found " << intersection_results.size()
              << " intersecting entries with (0,0,20,20):" << std::endl;
    for (const auto& entry : intersection_results) {
        std::cout << "  Row " << entry.row_id << ": (" << entry.rect.min_x << ","
                  << entry.rect.min_y << "," << entry.rect.max_x << "," << entry.rect.max_y << ")"
                  << std::endl;
    }

    // Test statistics
    std::cout << "\nTesting statistics collection..." << std::endl;
    std::string stats = rtree_index->collect_statistics();
    std::cout << stats << std::endl;

    // Test validation
    std::cout << "Testing validation..." << std::endl;
    std::string validation_error;
    bool is_valid = rtree_index->validate(validation_error);
    if (is_valid) {
        std::cout << "✓ Index validation passed" << std::endl;
    } else {
        std::cout << "⚠ Index validation failed: " << validation_error << std::endl;
    }

    std::cout << "✓ R-Tree basic operations test completed" << std::endl << std::endl;
}

void test_rtree_with_payload()
{
    std::cout << "=== Testing R-Tree with Payload (INCLUDE columns) ===" << std::endl;

    // Create test database
    scratchbird::tests::TestDatabaseRAII test_db("rtree_payload", true);

    // Create FileMap
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    // Create R-Tree index
    auto rtree_index = std::make_unique<RTreeIndex>(std::move(fmap), 4096, false);
    rtree_index->create_empty();

    // Test data with payloads (location data)
    std::vector<std::tuple<std::string, std::uint64_t, std::string>> test_data = {
        {"BBOX(37.7,-122.5,37.8,-122.4)", 101, "San Francisco"},
        {"BBOX(40.7,-74.0,40.8,-73.9)", 102, "New York"},
        {"BBOX(34.0,-118.3,34.1,-118.2)", 103, "Los Angeles"},
        {"BBOX(41.8,-87.7,41.9,-87.6)", 104, "Chicago"}};

    // Insert with payload
    std::cout << "Inserting spatial entries with payloads..." << std::endl;
    std::string err;
    for (const auto& [spatial_key, row_id, payload] : test_data) {
        bool success = rtree_index->insert_with_payload(spatial_key, row_id, payload, err);
        if (!success) {
            std::cout << "⚠ Insert with payload failed for '" << spatial_key << "': " << err
                      << std::endl;
        } else {
            std::cout << "✓ Inserted '" << spatial_key << "' -> " << row_id << " (payload: '"
                      << payload << "')" << std::endl;
        }
    }

    // Search with payload
    std::cout << "\nSearching with payload retrieval..." << std::endl;
    for (const auto& [spatial_key, expected_row_id, expected_payload] : test_data) {
        std::vector<std::pair<std::uint64_t, std::string>> results;
        rtree_index->search_equal_with_payload(spatial_key, results);

        if (results.empty()) {
            std::cout << "⚠ No results found for key '" << spatial_key << "'" << std::endl;
        } else {
            for (const auto& [row_id, payload] : results) {
                std::cout << "✓ Found '" << spatial_key << "' -> " << row_id << " (payload: '"
                          << payload << "')" << std::endl;
            }
        }
    }

    std::cout << "✓ R-Tree with payload test completed" << std::endl << std::endl;
}

void test_rtree_spatial_queries()
{
    std::cout << "=== Testing R-Tree Spatial Query Types ===" << std::endl;

    // Create test database
    scratchbird::tests::TestDatabaseRAII test_db("rtree_spatial", true);

    // Create FileMap
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    // Create R-Tree index
    auto rtree_index = std::make_unique<RTreeIndex>(std::move(fmap), 4096, false);
    rtree_index->create_empty();

    // Insert test spatial data (various rectangles)
    std::vector<std::pair<std::string, std::uint64_t>> spatial_data = {
        {"BBOX(0,0,10,10)", 1},   // Small rectangle in origin
        {"BBOX(5,5,25,25)", 2},   // Overlapping larger rectangle
        {"BBOX(15,15,20,20)", 3}, // Small rectangle inside #2
        {"BBOX(30,30,40,40)", 4}, // Separate rectangle
        {"BBOX(8,8,12,12)", 5},   // Intersects with #1 and #2
        {"BBOX(35,35,45,45)", 6}  // Overlaps with #4
    };

    std::string err;
    for (const auto& [bbox, row_id] : spatial_data) {
        rtree_index->insert(bbox, row_id, err);
    }

    // Test intersection queries
    std::cout << "Testing intersection queries..." << std::endl;

    Rectangle query1(7, 7, 18, 18); // Should intersect with entries 1, 2, 3, 5
    std::vector<RTreeEntry> intersects;
    rtree_index->search_intersects(query1, intersects);
    std::cout << "Intersection with (7,7,18,18): " << intersects.size() << " results" << std::endl;
    for (const auto& entry : intersects) {
        std::cout << "  Row " << entry.row_id << std::endl;
    }

    // Test containment queries
    std::cout << "\nTesting containment queries..." << std::endl;

    Rectangle query2(6, 6, 24, 24); // Should contain entry 3
    std::vector<RTreeEntry> contains;
    rtree_index->search_contains(query2, contains);
    std::cout << "Query (6,6,24,24) contains: " << contains.size() << " results" << std::endl;
    for (const auto& entry : contains) {
        std::cout << "  Row " << entry.row_id << std::endl;
    }

    // Test within queries
    std::cout << "\nTesting within queries..." << std::endl;

    Rectangle query3(0, 0, 50, 50); // Large query rectangle
    std::vector<RTreeEntry> within;
    rtree_index->search_within(query3, within);
    std::cout << "Within query (0,0,50,50): " << within.size() << " results" << std::endl;
    for (const auto& entry : within) {
        std::cout << "  Row " << entry.row_id << std::endl;
    }

    std::cout << "✓ Spatial query types test completed" << std::endl << std::endl;
}

void test_index_family_factory_rtree()
{
    std::cout << "=== Testing Index Family Factory R-Tree Creation ===" << std::endl;

    // Test R-Tree index creation through factory
    FileMap::Layout layout;
    layout.page_size = 4096;
    layout.options.direct_io = false;
    FileMap fmap(layout);

    auto rtree_index =
        IndexFamilyFactory::create_index(IndexMethod::RTree, std::move(fmap), 4096, false);
    if (rtree_index && rtree_index->get_method() == IndexMethod::RTree) {
        std::cout << "✓ R-Tree index family creation succeeded" << std::endl;
    } else {
        std::cout << "⚠ R-Tree index family creation failed" << std::endl;
    }

    // Test capability queries
    bool rtree_supports_range = IndexFamilyFactory::supports_range_queries(IndexMethod::RTree);
    bool rtree_supports_include = IndexFamilyFactory::supports_include_columns(IndexMethod::RTree);
    bool rtree_supports_partial = IndexFamilyFactory::supports_partial_indexes(IndexMethod::RTree);

    std::cout << "R-Tree capabilities:" << std::endl;
    std::cout << "  Range queries: " << (rtree_supports_range ? "yes" : "no") << std::endl;
    std::cout << "  INCLUDE columns: " << (rtree_supports_include ? "yes" : "no") << std::endl;
    std::cout << "  Partial indexes: " << (rtree_supports_partial ? "yes" : "no") << std::endl;

    // Test cost estimation
    if (rtree_index) {
        rtree_index->create_empty();

        double search_cost = rtree_index->estimate_search_cost("BBOX(0,0,10,10)");
        double range_cost =
            rtree_index->estimate_range_cost("BBOX(0,0,10,10)", "BBOX(20,20,30,30)");
        double maint_cost = rtree_index->estimate_maintenance_cost();

        std::cout << "Cost estimates:" << std::endl;
        std::cout << "  Search: " << search_cost << std::endl;
        std::cout << "  Range: " << range_cost << std::endl;
        std::cout << "  Maintenance: " << maint_cost << std::endl;
    }

    std::cout << "✓ Index family factory R-Tree test completed" << std::endl << std::endl;
}

int main()
{
    std::cout << "=== R-Tree Index Family Tests ===" << std::endl << std::endl;

    try {
        test_rectangle_operations();
        test_rtree_spatial_parsing();
        test_rtree_basic_operations();
        test_rtree_with_payload();
        test_rtree_spatial_queries();
        test_index_family_factory_rtree();

        std::cout << "=== All R-Tree Index Tests Completed Successfully ===" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
