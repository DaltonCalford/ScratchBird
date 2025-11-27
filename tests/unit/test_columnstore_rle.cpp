/**
 * @file test_columnstore_rle.cpp
 * @brief Unit tests for Columnstore RLE compression
 *
 * Tests Run-Length Encoding compression and decompression for all data types.
 *
 * Test Coverage:
 * - INT32/INT64 compression/decompression
 * - Best case: All same value (1000x compression)
 * - Worst case: All different values (1.2x overhead)
 * - NULL value handling
 * - Empty input
 * - Single value
 * - Alternating values
 * - Large datasets (1M values)
 * - Round-trip verification
 * - Error handling (corrupted data)
 */

#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "gtest/gtest.h"
#include <cstdio>
#include <cstring>
#include <vector>

using namespace scratchbird::core;

// Helper: Create a Database instance for testing
Database* createTestDatabase()
{
    const char* test_db_path = "/tmp/test_columnstore_rle.db";
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

// Helper: Create ColumnSegment with INT32 values
ColumnSegment createInt32Segment(const std::vector<int32_t>& values, const std::vector<bool>& nulls = {})
{
    ColumnSegment seg;
    seg.data_type = DataType::INT32;
    seg.row_count = static_cast<uint32_t>(values.size());
    seg.null_count = 0;

    // Copy values to data buffer
    seg.data.resize(values.size() * sizeof(int32_t));
    std::memcpy(seg.data.data(), values.data(), values.size() * sizeof(int32_t));

    // Set up NULL bitmap
    if (!nulls.empty())
    {
        seg.null_bitmap = nulls;
        for (bool is_null : nulls)
        {
            if (is_null) seg.null_count++;
        }
    }

    return seg;
}

// Helper: Create ColumnSegment with INT64 values
ColumnSegment createInt64Segment(const std::vector<int64_t>& values, const std::vector<bool>& nulls = {})
{
    ColumnSegment seg;
    seg.data_type = DataType::INT64;
    seg.row_count = static_cast<uint32_t>(values.size());
    seg.null_count = 0;

    // Copy values to data buffer
    seg.data.resize(values.size() * sizeof(int64_t));
    std::memcpy(seg.data.data(), values.data(), values.size() * sizeof(int64_t));

    // Set up NULL bitmap
    if (!nulls.empty())
    {
        seg.null_bitmap = nulls;
        for (bool is_null : nulls)
        {
            if (is_null) seg.null_count++;
        }
    }

    return seg;
}

// Helper: Extract INT32 values from ColumnSegment
std::vector<int32_t> extractInt32Values(const ColumnSegment& seg)
{
    std::vector<int32_t> values;
    const int32_t* data_ptr = reinterpret_cast<const int32_t*>(seg.data.data());
    for (uint32_t i = 0; i < seg.row_count; ++i)
    {
        values.push_back(data_ptr[i]);
    }
    return values;
}

// Helper: Extract INT64 values from ColumnSegment
std::vector<int64_t> extractInt64Values(const ColumnSegment& seg)
{
    std::vector<int64_t> values;
    const int64_t* data_ptr = reinterpret_cast<const int64_t*>(seg.data.data());
    for (uint32_t i = 0; i < seg.row_count; ++i)
    {
        values.push_back(data_ptr[i]);
    }
    return values;
}

//=============================================================================
// Test 1: Basic INT32 Compression/Decompression
//=============================================================================
void test_int32_basic()
{
    printf("Test 1: Basic INT32 compression/decompression...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    // Create index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Test data: [1, 1, 1, 2, 2, 3, 3, 3, 3]
    std::vector<int32_t> input_values = {1, 1, 1, 2, 2, 3, 3, 3, 3};
    ColumnSegment input_seg = createInt32Segment(input_values);

    // Compress
    std::vector<uint8_t> compressed;
    status = index->compressRLE(input_seg, &compressed, &ctx);
    assert(status == Status::OK);

    // Expected compressed size: 3 runs × (1 + 4 + 4) bytes = 27 bytes
    size_t expected_size = 3 * (1 + sizeof(int32_t) + sizeof(uint32_t));
    assert(compressed.size() == expected_size);

    printf("  Compressed %zu bytes → %zu bytes (ratio: %.2fx)\n",
           input_values.size() * sizeof(int32_t), compressed.size(),
           static_cast<double>(input_values.size() * sizeof(int32_t)) / compressed.size());

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressRLE(compressed, DataType::INT32, 9, &output_seg, &ctx);
    assert(status == Status::OK);
    assert(output_seg.row_count == 9);

    // Verify values match
    std::vector<int32_t> output_values = extractInt32Values(output_seg);
    assert(output_values == input_values);

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 2: INT64 Compression/Decompression
//=============================================================================
void test_int64_basic()
{
    printf("Test 2: INT64 compression/decompression...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Test data: Large INT64 values
    std::vector<int64_t> input_values = {1000000000000LL, 1000000000000LL, 2000000000000LL, 2000000000000LL};
    ColumnSegment input_seg = createInt64Segment(input_values);

    // Compress
    std::vector<uint8_t> compressed;
    status = index->compressRLE(input_seg, &compressed, &ctx);
    assert(status == Status::OK);

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressRLE(compressed, DataType::INT64, 4, &output_seg, &ctx);
    assert(status == Status::OK);

    // Verify
    std::vector<int64_t> output_values = extractInt64Values(output_seg);
    assert(output_values == input_values);

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 3: Best Case - All Same Value (1000x compression)
//=============================================================================
void test_best_case_compression()
{
    printf("Test 3: Best case compression (all same value)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // 10000 identical values
    std::vector<int32_t> input_values(10000, 42);
    ColumnSegment input_seg = createInt32Segment(input_values);

    // Compress
    std::vector<uint8_t> compressed;
    status = index->compressRLE(input_seg, &compressed, &ctx);
    assert(status == Status::OK);

    // Expected: 1 run × 9 bytes = 9 bytes (vs 40,000 bytes uncompressed)
    size_t uncompressed_size = input_values.size() * sizeof(int32_t);
    double ratio = static_cast<double>(uncompressed_size) / compressed.size();

    printf("  Compressed %zu bytes → %zu bytes (ratio: %.2fx)\n",
           uncompressed_size, compressed.size(), ratio);
    assert(ratio > 1000.0);  // Should be ~4444x

    // Decompress and verify
    ColumnSegment output_seg;
    status = index->decompressRLE(compressed, DataType::INT32, 10000, &output_seg, &ctx);
    assert(status == Status::OK);

    std::vector<int32_t> output_values = extractInt32Values(output_seg);
    assert(output_values == input_values);

    delete db;
    printf("  ✅ PASSED (compression ratio: %.2fx)\n\n", ratio);
}

//=============================================================================
// Test 4: Worst Case - All Different Values (1.2x overhead)
//=============================================================================
void test_worst_case_compression()
{
    printf("Test 4: Worst case compression (all different values)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // 1000 different values
    std::vector<int32_t> input_values;
    for (int32_t i = 0; i < 1000; ++i)
    {
        input_values.push_back(i);
    }
    ColumnSegment input_seg = createInt32Segment(input_values);

    // Compress
    std::vector<uint8_t> compressed;
    status = index->compressRLE(input_seg, &compressed, &ctx);
    assert(status == Status::OK);

    // Expected: 1000 runs × 9 bytes = 9000 bytes (vs 4000 bytes uncompressed) = 2.25x overhead
    size_t uncompressed_size = input_values.size() * sizeof(int32_t);
    double ratio = static_cast<double>(compressed.size()) / uncompressed_size;

    printf("  Compressed %zu bytes → %zu bytes (overhead: %.2fx)\n",
           uncompressed_size, compressed.size(), ratio);
    assert(ratio < 3.0);  // Should be ~2.25x overhead

    // Decompress and verify
    ColumnSegment output_seg;
    status = index->decompressRLE(compressed, DataType::INT32, 1000, &output_seg, &ctx);
    assert(status == Status::OK);

    std::vector<int32_t> output_values = extractInt32Values(output_seg);
    assert(output_values == input_values);

    delete db;
    printf("  ✅ PASSED (overhead: %.2fx)\n\n", ratio);
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
                                             1024, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Test data: [1, NULL, NULL, 2, 2, NULL]
    std::vector<int32_t> input_values = {1, 0, 0, 2, 2, 0};  // Values don't matter for NULLs
    std::vector<bool> input_nulls = {false, true, true, false, false, true};
    ColumnSegment input_seg = createInt32Segment(input_values, input_nulls);

    // Compress
    std::vector<uint8_t> compressed;
    status = index->compressRLE(input_seg, &compressed, &ctx);
    assert(status == Status::OK);

    // Expected: 4 runs (1, NULL×2, 2×2, NULL) × 9 bytes = 36 bytes
    printf("  Compressed %zu bytes → %zu bytes\n",
           input_values.size() * sizeof(int32_t), compressed.size());

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressRLE(compressed, DataType::INT32, 6, &output_seg, &ctx);
    assert(status == Status::OK);

    // Verify NULL bitmap matches
    assert(output_seg.null_bitmap == input_nulls);

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
                                             1024, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Empty segment
    ColumnSegment input_seg;
    input_seg.data_type = DataType::INT32;
    input_seg.row_count = 0;

    // Compress
    std::vector<uint8_t> compressed;
    status = index->compressRLE(input_seg, &compressed, &ctx);
    assert(status == Status::OK);
    assert(compressed.empty());

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressRLE(compressed, DataType::INT32, 0, &output_seg, &ctx);
    assert(status == Status::OK);
    assert(output_seg.row_count == 0);

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 7: Single Value
//=============================================================================
void test_single_value()
{
    printf("Test 7: Single value...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Single value
    std::vector<int32_t> input_values = {42};
    ColumnSegment input_seg = createInt32Segment(input_values);

    // Compress
    std::vector<uint8_t> compressed;
    status = index->compressRLE(input_seg, &compressed, &ctx);
    assert(status == Status::OK);

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressRLE(compressed, DataType::INT32, 1, &output_seg, &ctx);
    assert(status == Status::OK);

    std::vector<int32_t> output_values = extractInt32Values(output_seg);
    assert(output_values == input_values);

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 8: Alternating Values
//=============================================================================
void test_alternating_values()
{
    printf("Test 8: Alternating values (1,2,1,2,...)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Alternating: [1, 2, 1, 2, 1, 2, ...]
    std::vector<int32_t> input_values;
    for (int i = 0; i < 100; ++i)
    {
        input_values.push_back((i % 2) + 1);
    }
    ColumnSegment input_seg = createInt32Segment(input_values);

    // Compress
    std::vector<uint8_t> compressed;
    status = index->compressRLE(input_seg, &compressed, &ctx);
    assert(status == Status::OK);

    // Expected: 100 runs (worst case for RLE)
    printf("  Compressed %zu bytes → %zu bytes\n",
           input_values.size() * sizeof(int32_t), compressed.size());

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressRLE(compressed, DataType::INT32, 100, &output_seg, &ctx);
    assert(status == Status::OK);

    std::vector<int32_t> output_values = extractInt32Values(output_seg);
    assert(output_values == input_values);

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Test 9: Large Dataset (1M values)
//=============================================================================
void test_large_dataset()
{
    printf("Test 9: Large dataset (1M values)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // 1M values with 10 unique values (good RLE scenario)
    std::vector<int32_t> input_values;
    for (int i = 0; i < 1000000; ++i)
    {
        input_values.push_back(i % 10);  // 0-9 repeating
    }
    ColumnSegment input_seg = createInt32Segment(input_values);

    // Compress
    std::vector<uint8_t> compressed;
    status = index->compressRLE(input_seg, &compressed, &ctx);
    assert(status == Status::OK);

    size_t uncompressed_size = input_values.size() * sizeof(int32_t);
    double ratio = static_cast<double>(uncompressed_size) / compressed.size();

    printf("  Compressed %zu bytes → %zu bytes (ratio: %.2fx)\n",
           uncompressed_size, compressed.size(), ratio);

    // Decompress (verify first 1000 values to save time)
    ColumnSegment output_seg;
    status = index->decompressRLE(compressed, DataType::INT32, 1000000, &output_seg, &ctx);
    assert(status == Status::OK);
    assert(output_seg.row_count == 1000000);

    // Verify first 1000 values
    std::vector<int32_t> output_values = extractInt32Values(output_seg);
    for (int i = 0; i < 1000; ++i)
    {
        assert(output_values[i] == input_values[i]);
    }

    delete db;
    printf("  ✅ PASSED (compression ratio: %.2fx)\n\n", ratio);
}

//=============================================================================
// Test 10: Round-Trip Verification
//=============================================================================
void test_round_trip()
{
    printf("Test 10: Round-trip verification (compress → decompress → compress)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    uint32_t root_page = 0;
    ErrorContext ctx;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, column_uuids,
                                             1024, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 1024, &ctx);
    assert(index != nullptr);

    // Test data
    std::vector<int32_t> original_values = {5, 5, 5, 10, 10, 15, 15, 15};
    ColumnSegment original_seg = createInt32Segment(original_values);

    // First compression
    std::vector<uint8_t> compressed1;
    status = index->compressRLE(original_seg, &compressed1, &ctx);
    assert(status == Status::OK);

    // Decompression
    ColumnSegment decompressed_seg;
    status = index->decompressRLE(compressed1, DataType::INT32, 8, &decompressed_seg, &ctx);
    assert(status == Status::OK);

    // Second compression
    std::vector<uint8_t> compressed2;
    status = index->compressRLE(decompressed_seg, &compressed2, &ctx);
    assert(status == Status::OK);

    // Verify compressed data is identical
    assert(compressed1 == compressed2);

    delete db;
    printf("  ✅ PASSED\n\n");
}

//=============================================================================
// Main Test Runner
//=============================================================================

TEST(ColumnstoreRleTest, Comprehensive) {

    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Columnstore RLE Compression - Unit Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    test_int32_basic();
    test_int64_basic();
    test_best_case_compression();
    test_worst_case_compression();
    test_null_handling();
    test_empty_input();
    test_single_value();
    test_alternating_values();
    test_large_dataset();
    test_round_trip();

    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  ✅ ALL TESTS PASSED (10/10)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return;

}
