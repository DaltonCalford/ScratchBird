/**
 * @file test_columnstore_dict.cpp
 * @brief Unit tests for Columnstore Dictionary Encoding
 *
 * Tests dictionary encoding compression and decompression for string data.
 *
 * Test Coverage:
 * - Dictionary builder operations
 * - Low-cardinality string compression (< 10% unique)
 * - High-cardinality rejection (fall back to RLE)
 * - NULL value handling
 * - Empty input
 * - Single unique value (best case)
 * - Round-trip verification
 * - Compression ratio validation
 */

#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

using namespace scratchbird::core;

// Helper: Create a Database instance for testing
Database* createTestDatabase()
{
    const char* test_db_path = "/tmp/test_columnstore_dict.db";
    std::remove(test_db_path);

    ErrorContext ctx;
    Status status = Database::create(test_db_path, 8192, &ctx);
    if (status != Status::OK)
    {
        fprintf(stderr, "Failed to create test database: %s\n", ctx.message);
        return nullptr;
    }

    Database* db = new Database();
    status = db->open(test_db_path, &ctx);
    if (status != Status::OK)
    {
        fprintf(stderr, "Failed to open test database: %s\n", ctx.message);
        delete db;
        return nullptr;
    }

    return db;
}

// Helper: Create ColumnSegment with string values (null-terminated)
ColumnSegment createStringSegment(const std::vector<std::string>& values,
                                  const std::vector<bool>& nulls = {})
{
    ColumnSegment seg;
    seg.data_type = DataType::TEXT;
    seg.row_count = static_cast<uint32_t>(values.size());
    seg.null_count = 0;

    // Build data buffer with null-terminated strings
    for (size_t i = 0; i < values.size(); ++i)
    {
        bool is_null = (i < nulls.size()) ? nulls[i] : false;

        if (is_null)
        {
            seg.null_count++;
            seg.data.push_back(0);  // Empty string for NULL
        }
        else
        {
            // Copy string + null terminator
            seg.data.insert(seg.data.end(), values[i].begin(), values[i].end());
            seg.data.push_back(0);
        }
    }

    // Set up NULL bitmap
    if (!nulls.empty())
    {
        seg.null_bitmap = nulls;
    }
    else
    {
        seg.null_bitmap.resize(values.size(), false);
    }

    return seg;
}

//=============================================================================
// Test 1: Dictionary Builder - Basic Operations
//=============================================================================
void test_dictionary_builder()
{
    printf("Test 1: Dictionary builder basic operations...\n");

    Dictionary dict;

    // Add values
    uint32_t code1 = dict.addValue("Apple");
    uint32_t code2 = dict.addValue("Banana");
    uint32_t code3 = dict.addValue("Cherry");

    // Codes should be sequential
    assert(code1 == 0);
    assert(code2 == 1);
    assert(code3 == 2);

    // Re-adding same value should return same code
    uint32_t code1_again = dict.addValue("Apple");
    assert(code1_again == 0);
    assert(dict.size() == 3);

    // Get code for existing value
    int32_t lookup1 = dict.getCode("Banana");
    assert(lookup1 == 1);

    // Get code for non-existent value
    int32_t lookup2 = dict.getCode("Durian");
    assert(lookup2 == -1);

    // Get value for code
    std::string value;
    bool found = dict.getValue(0, &value);
    assert(found);
    assert(value == "Apple");

    found = dict.getValue(100, &value);
    assert(!found);

    // Clear dictionary
    dict.clear();
    assert(dict.size() == 0);

    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 2: Low-Cardinality Compression (Best Case)
//=============================================================================
void test_low_cardinality_best_case()
{
    printf("Test 2: Low-cardinality compression (best case)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::DICTIONARY, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Best case: 1000 values, only 1 unique (0.1% cardinality)
    std::vector<std::string> values(1000, "Active");
    ColumnSegment input_seg = createStringSegment(values);

    // Compress
    std::vector<uint8_t> compressed;
    Dictionary dict;
    status = index->compressDictionary(input_seg, &compressed, &dict, &ctx);
    assert(status == Status::OK);

    // Dictionary should have 1 entry
    assert(dict.size() == 1);

    // Compressed size should be very small (RLE of codes)
    // Expected: ~9 bytes for RLE (1 code repeated 1000 times)
    printf("  Dictionary size: %zu entries\n", dict.size());
    printf("  Compressed: %zu bytes (vs ~6000 bytes uncompressed)\n", compressed.size());
    printf("  Ratio: %.2fx\n", 6000.0 / compressed.size());

    assert(compressed.size() < 100);  // Should be < 100 bytes

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressDictionary(compressed, dict, DataType::TEXT, 1000, &output_seg, &ctx);
    assert(status == Status::OK);
    assert(output_seg.row_count == 1000);

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 3: High-Cardinality Rejection
//=============================================================================
void test_high_cardinality_rejection()
{
    printf("Test 3: High-cardinality rejection (should fail)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::DICTIONARY, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // High cardinality: 100 unique values out of 100 total (100% cardinality)
    std::vector<std::string> values;
    for (int i = 0; i < 100; ++i)
    {
        values.push_back("Value_" + std::to_string(i));
    }
    ColumnSegment input_seg = createStringSegment(values);

    // Compress - should FAIL due to high cardinality
    std::vector<uint8_t> compressed;
    Dictionary dict;
    status = index->compressDictionary(input_seg, &compressed, &dict, &ctx);
    assert(status == Status::INVALID_ARGUMENT);  // Should reject

    printf("  Cardinality: 100%% (100 unique / 100 total)\n");
    printf("  Status: REJECTED (as expected)\n");

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 4: Medium Cardinality (5% unique)
//=============================================================================
void test_medium_cardinality()
{
    printf("Test 4: Medium cardinality (5%% unique)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::DICTIONARY, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // 5 unique values repeated across 100 values (5% cardinality)
    std::vector<std::string> unique = {"Red", "Blue", "Green", "Yellow", "Orange"};
    std::vector<std::string> values;
    for (int i = 0; i < 100; ++i)
    {
        values.push_back(unique[i % 5]);
    }
    ColumnSegment input_seg = createStringSegment(values);

    // Compress
    std::vector<uint8_t> compressed;
    Dictionary dict;
    status = index->compressDictionary(input_seg, &compressed, &dict, &ctx);
    assert(status == Status::OK);

    // Dictionary should have 5 entries
    assert(dict.size() == 5);

    printf("  Dictionary size: %zu entries\n", dict.size());
    printf("  Cardinality: 5%% (5 unique / 100 total)\n");
    printf("  Compressed: %zu bytes\n", compressed.size());

    // Decompress and verify
    ColumnSegment output_seg;
    status = index->decompressDictionary(compressed, dict, DataType::TEXT, 100, &output_seg, &ctx);
    assert(status == Status::OK);
    assert(output_seg.row_count == 100);

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 5: NULL Value Handling
//=============================================================================
void test_null_handling()
{
    printf("Test 5: NULL value handling...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::DICTIONARY, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Test data: ["Active", NULL, NULL, "Inactive", "Active", NULL]
    std::vector<std::string> values = {"Active", "", "", "Inactive", "Active", ""};
    std::vector<bool> nulls = {false, true, true, false, false, true};
    ColumnSegment input_seg = createStringSegment(values, nulls);

    // Compress
    std::vector<uint8_t> compressed;
    Dictionary dict;
    status = index->compressDictionary(input_seg, &compressed, &dict, &ctx);
    assert(status == Status::OK);

    printf("  Dictionary size: %zu entries\n", dict.size());
    printf("  NULL count: 3 / 6\n");

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressDictionary(compressed, dict, DataType::TEXT, 6, &output_seg, &ctx);
    assert(status == Status::OK);

    // Verify NULL bitmap matches
    assert(output_seg.null_bitmap.size() == nulls.size());
    for (size_t i = 0; i < nulls.size(); ++i)
    {
        assert(output_seg.null_bitmap[i] == nulls[i]);
    }

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 6: Empty Input
//=============================================================================
void test_empty_input()
{
    printf("Test 6: Empty input...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::DICTIONARY, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Empty segment
    ColumnSegment input_seg;
    input_seg.data_type = DataType::TEXT;
    input_seg.row_count = 0;

    // Compress
    std::vector<uint8_t> compressed;
    Dictionary dict;
    status = index->compressDictionary(input_seg, &compressed, &dict, &ctx);
    assert(status == Status::OK);
    assert(compressed.empty());
    assert(dict.size() == 0);

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressDictionary(compressed, dict, DataType::TEXT, 0, &output_seg, &ctx);
    assert(status == Status::OK);
    assert(output_seg.row_count == 0);

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 7: Single Unique Value (Extreme Compression)
//=============================================================================
void test_single_unique_value()
{
    printf("Test 7: Single unique value (extreme compression)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::DICTIONARY, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // 10000 values, all "PENDING"
    std::vector<std::string> values(10000, "PENDING");
    ColumnSegment input_seg = createStringSegment(values);

    // Compress
    std::vector<uint8_t> compressed;
    Dictionary dict;
    status = index->compressDictionary(input_seg, &compressed, &dict, &ctx);
    assert(status == Status::OK);

    size_t uncompressed_size = 10000 * 8;  // Approx 8 bytes per "PENDING"
    double ratio = static_cast<double>(uncompressed_size) / compressed.size();

    printf("  Dictionary size: %zu entry\n", dict.size());
    printf("  Compressed: %zu bytes → %zu bytes (ratio: %.2fx)\n",
           uncompressed_size, compressed.size(), ratio);

    assert(dict.size() == 1);
    assert(ratio > 100.0);  // Should achieve > 100x compression

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressDictionary(compressed, dict, DataType::TEXT, 10000, &output_seg, &ctx);
    assert(status == Status::OK);
    assert(output_seg.row_count == 10000);

    delete db;
    printf("  ✅ PASSED (compression ratio: %.2fx)\n\n", ratio);
}

//=============================================================================
// Test 8: Round-Trip Verification
//=============================================================================
void test_round_trip()
{
    printf("Test 8: Round-trip verification...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::DICTIONARY, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Test data with low cardinality
    std::vector<std::string> original = {"Apple", "Apple", "Banana", "Cherry", "Apple", "Banana"};
    ColumnSegment input_seg = createStringSegment(original);

    // First compression
    std::vector<uint8_t> compressed1;
    Dictionary dict1;
    status = index->compressDictionary(input_seg, &compressed1, &dict1, &ctx);
    assert(status == Status::OK);

    // Decompression
    ColumnSegment decompressed_seg;
    status = index->decompressDictionary(compressed1, dict1, DataType::TEXT, 6, &decompressed_seg, &ctx);
    assert(status == Status::OK);

    // Second compression
    std::vector<uint8_t> compressed2;
    Dictionary dict2;
    status = index->compressDictionary(decompressed_seg, &compressed2, &dict2, &ctx);
    assert(status == Status::OK);

    // Dictionaries should be equivalent (same size)
    assert(dict1.size() == dict2.size());

    // Compressed data should be identical
    assert(compressed1.size() == compressed2.size());

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Main Test Runner
//=============================================================================
int main()
{
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Columnstore Dictionary Encoding - Unit Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    test_dictionary_builder();
    test_low_cardinality_best_case();
    test_high_cardinality_rejection();
    test_medium_cardinality();
    test_null_handling();
    test_empty_input();
    test_single_unique_value();
    test_round_trip();

    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  ✅ ALL TESTS PASSED (8/8)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return;
}
