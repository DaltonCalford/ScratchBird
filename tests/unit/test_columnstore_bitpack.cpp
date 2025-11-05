/**
 * @file test_columnstore_bitpack.cpp
 * @brief Unit tests for Columnstore Bit-Packing Compression
 *
 * Tests bit-packing compression and decompression for integer data.
 *
 * Test Coverage:
 * - Small range (0-7, 3 bits per value)
 * - Large range (0-1000000, 20 bits per value)
 * - Negative values (-100 to 100)
 * - NULL value handling
 * - Best case: All values 0-1 (1 bit per value)
 * - Worst case: Full int32_t range (32 bits per value)
 * - Same value (0 bits per value)
 * - Large dataset (1M values)
 * - Round-trip verification
 */

#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdlib>

using namespace scratchbird::core;

// Helper: Create a Database instance for testing
Database* createTestDatabase()
{
    const char* test_db_path = "/tmp/test_columnstore_bitpack.db";
    std::remove(test_db_path);

    ErrorContext ctx;
    Status status = Database::create(test_db_path, 8192, &ctx);
    if (status != Status::OK)
    {
        fprintf(stderr, "Failed to create test database: %s\n", ctx.message.c_str());
        return nullptr;
    }

    Database* db = new Database();
    status = db->open(test_db_path, &ctx);
    if (status != Status::OK)
    {
        fprintf(stderr, "Failed to open test database: %s\n", ctx.message.c_str());
        delete db;
        return nullptr;
    }

    return db;
}

// Helper: Create a ColumnstoreIndex instance
ColumnstoreIndex* createTestIndex(Database* db)
{
    // Create index metadata
    SBColumnstoreIndex index_info;
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::memcpy(&index_info.idx_uuid, &index_uuid, sizeof(ID));
    std::memcpy(&index_info.idx_table_uuid, &table_uuid, sizeof(ID));
    index_info.idx_column_uuids.push_back(ID());
    std::memcpy(&index_info.idx_column_uuids[0], &column_uuid, sizeof(ID));
    index_info.idx_root_page = 0;
    index_info.idx_segment_size = 1024;
    index_info.idx_compression_type = static_cast<uint8_t>(CompressionType::BITPACK);
    index_info.idx_total_segments = 0;
    index_info.idx_total_rows = 0;
    index_info.idx_creation_xid = 1;

    return new ColumnstoreIndex(db, index_info);
}

// Helper: Create INT32 segment
ColumnSegment createInt32Segment(const std::vector<int32_t>& values,
                                  const std::vector<bool>& nulls = {})
{
    ColumnSegment seg;
    seg.data_type = DataType::INT32;
    seg.row_count = static_cast<uint32_t>(values.size());
    seg.null_bitmap = nulls;

    // Allocate data
    seg.data.resize(values.size() * sizeof(int32_t));
    std::memcpy(seg.data.data(), values.data(), values.size() * sizeof(int32_t));

    return seg;
}

// Helper: Verify INT32 segment values
bool verifyInt32Segment(const ColumnSegment& seg, const std::vector<int32_t>& expected)
{
    if (seg.row_count != expected.size())
        return false;

    const int32_t* values = reinterpret_cast<const int32_t*>(seg.data.data());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (values[i] != expected[i])
            return false;
    }

    return true;
}

// ============================================================================
// Test Cases
// ============================================================================

void test_small_range()
{
    printf("Test 1: Small range (0-7, 3 bits per value)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with values 0-7 (requires 3 bits per value)
    std::vector<int32_t> values = {0, 1, 2, 3, 4, 5, 6, 7, 3, 2, 1, 0};
    ColumnSegment input_seg = createInt32Segment(values);

    // Compress
    std::vector<uint8_t> compressed;
    ErrorContext ctx;
    Status status = index->compressBitpack(input_seg, &compressed, &ctx);

    assert(status == Status::OK);

    // Verify compression
    // Header: 16 bytes (min=0, bits=3)
    // Data: 12 values * 3 bits = 36 bits = 5 bytes
    printf("  Compressed %zu bytes → %zu bytes\n",
           values.size() * sizeof(int32_t), compressed.size());
    assert(compressed.size() == 16 + 5);  // Header + data

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressBitpack(compressed, DataType::INT32, input_seg.row_count,
                                      &output_seg, &ctx);

    assert(status == Status::OK);
    assert(verifyInt32Segment(output_seg, values));

    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_large_range()
{
    printf("Test 2: Large range (0-1000000, 20 bits per value)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with values 0-1000000 (requires 20 bits per value)
    std::vector<int32_t> values;
    for (int32_t i = 0; i < 100; ++i)
    {
        values.push_back(i * 10000);  // 0, 10000, 20000, ..., 990000
    }
    ColumnSegment input_seg = createInt32Segment(values);

    // Compress
    std::vector<uint8_t> compressed;
    ErrorContext ctx;
    Status status = index->compressBitpack(input_seg, &compressed, &ctx);

    assert(status == Status::OK);

    // Verify compression
    // Header: 16 bytes
    // Data: 100 values * 20 bits = 2000 bits = 250 bytes
    printf("  Compressed %zu bytes → %zu bytes\n",
           values.size() * sizeof(int32_t), compressed.size());
    assert(compressed.size() == 16 + 250);

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressBitpack(compressed, DataType::INT32, input_seg.row_count,
                                      &output_seg, &ctx);

    assert(status == Status::OK);
    assert(verifyInt32Segment(output_seg, values));

    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_negative_values()
{
    printf("Test 3: Negative values (-100 to 100)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with negative values
    std::vector<int32_t> values;
    for (int32_t i = -100; i <= 100; i += 10)
    {
        values.push_back(i);  // -100, -90, -80, ..., 90, 100
    }
    ColumnSegment input_seg = createInt32Segment(values);

    // Compress
    std::vector<uint8_t> compressed;
    ErrorContext ctx;
    Status status = index->compressBitpack(input_seg, &compressed, &ctx);

    assert(status == Status::OK);

    // Verify compression
    // Range: -100 to 100 = 200 values → requires 8 bits (ceil(log2(200+1)))
    printf("  Compressed %zu bytes → %zu bytes\n",
           values.size() * sizeof(int32_t), compressed.size());

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressBitpack(compressed, DataType::INT32, input_seg.row_count,
                                      &output_seg, &ctx);

    assert(status == Status::OK);
    assert(verifyInt32Segment(output_seg, values));

    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_null_handling()
{
    printf("Test 4: NULL value handling...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with NULLs
    std::vector<int32_t> values = {5, 10, 15, 20, 25, 30};
    std::vector<bool> nulls = {false, true, false, true, false, false};
    ColumnSegment input_seg = createInt32Segment(values, nulls);

    // Compress
    std::vector<uint8_t> compressed;
    ErrorContext ctx;
    Status status = index->compressBitpack(input_seg, &compressed, &ctx);

    assert(status == Status::OK);

    // Decompress
    ColumnSegment output_seg;
    output_seg.null_bitmap = nulls;  // Restore NULL bitmap
    status = index->decompressBitpack(compressed, DataType::INT32, input_seg.row_count,
                                      &output_seg, &ctx);

    assert(status == Status::OK);

    // Verify non-NULL values
    const int32_t* output_values = reinterpret_cast<const int32_t*>(output_seg.data.data());
    assert(output_values[0] == 5);   // Not NULL
    assert(output_values[2] == 15);  // Not NULL
    assert(output_values[4] == 25);  // Not NULL
    assert(output_values[5] == 30);  // Not NULL

    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_best_case_binary()
{
    printf("Test 5: Best case - Binary values (0-1, 1 bit per value)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with only 0 and 1 (requires 1 bit per value)
    std::vector<int32_t> values;
    for (int32_t i = 0; i < 10000; ++i)
    {
        values.push_back(i % 2);  // Alternating 0, 1, 0, 1, ...
    }
    ColumnSegment input_seg = createInt32Segment(values);

    // Compress
    std::vector<uint8_t> compressed;
    ErrorContext ctx;
    Status status = index->compressBitpack(input_seg, &compressed, &ctx);

    assert(status == Status::OK);

    // Verify compression ratio
    size_t uncompressed_size = values.size() * sizeof(int32_t);
    size_t compressed_size = compressed.size();
    double ratio = static_cast<double>(uncompressed_size) / compressed_size;

    printf("  Compressed %zu bytes → %zu bytes (ratio: %.2fx)\n",
           uncompressed_size, compressed_size, ratio);

    // Header: 16 bytes
    // Data: 10000 values * 1 bit = 10000 bits = 1250 bytes
    assert(compressed.size() == 16 + 1250);
    assert(ratio > 30.0);  // Should be ~32x compression

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressBitpack(compressed, DataType::INT32, input_seg.row_count,
                                      &output_seg, &ctx);

    assert(status == Status::OK);
    assert(verifyInt32Segment(output_seg, values));

    printf("  ✅ PASSED (compression ratio: %.2fx)\n\n", ratio);

    delete index;
    delete db;
}

void test_worst_case_full_range()
{
    printf("Test 6: Worst case - Full int32_t range (32 bits per value)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with full int32_t range
    std::vector<int32_t> values = {
        INT32_MIN, INT32_MIN / 2, 0, INT32_MAX / 2, INT32_MAX
    };
    ColumnSegment input_seg = createInt32Segment(values);

    // Compress
    std::vector<uint8_t> compressed;
    ErrorContext ctx;
    Status status = index->compressBitpack(input_seg, &compressed, &ctx);

    assert(status == Status::OK);

    // Verify no compression gain
    size_t uncompressed_size = values.size() * sizeof(int32_t);
    size_t compressed_size = compressed.size();
    double overhead = static_cast<double>(compressed_size) / uncompressed_size;

    printf("  Compressed %zu bytes → %zu bytes (overhead: %.2fx)\n",
           uncompressed_size, compressed_size, overhead);

    // Header: 16 bytes
    // Data: 5 values * 32 bits = 160 bits = 20 bytes
    // Total: 36 bytes vs 20 bytes uncompressed = 1.8x overhead (acceptable)
    assert(compressed.size() == 16 + 20);

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressBitpack(compressed, DataType::INT32, input_seg.row_count,
                                      &output_seg, &ctx);

    assert(status == Status::OK);
    assert(verifyInt32Segment(output_seg, values));

    printf("  ✅ PASSED (overhead: %.2fx)\n\n", overhead);

    delete index;
    delete db;
}

void test_same_value()
{
    printf("Test 7: Same value (0 bits per value)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with all same value
    std::vector<int32_t> values(10000, 42);  // 10000 times the value 42
    ColumnSegment input_seg = createInt32Segment(values);

    // Compress
    std::vector<uint8_t> compressed;
    ErrorContext ctx;
    Status status = index->compressBitpack(input_seg, &compressed, &ctx);

    assert(status == Status::OK);

    // Verify extreme compression
    size_t uncompressed_size = values.size() * sizeof(int32_t);
    size_t compressed_size = compressed.size();
    double ratio = static_cast<double>(uncompressed_size) / compressed_size;

    printf("  Compressed %zu bytes → %zu bytes (ratio: %.2fx)\n",
           uncompressed_size, compressed_size, ratio);

    // Header only (16 bytes) - no data needed
    assert(compressed.size() == 16);
    assert(ratio > 2000.0);  // Should be ~2500x compression

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressBitpack(compressed, DataType::INT32, input_seg.row_count,
                                      &output_seg, &ctx);

    assert(status == Status::OK);
    assert(verifyInt32Segment(output_seg, values));

    printf("  ✅ PASSED (compression ratio: %.2fx)\n\n", ratio);

    delete index;
    delete db;
}

void test_large_dataset()
{
    printf("Test 8: Large dataset (1M values, range 0-1000)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with 1M values in range 0-1000
    std::vector<int32_t> values;
    for (int32_t i = 0; i < 1000000; ++i)
    {
        values.push_back(i % 1000);  // Cycling 0-999
    }
    ColumnSegment input_seg = createInt32Segment(values);

    // Compress
    std::vector<uint8_t> compressed;
    ErrorContext ctx;
    Status status = index->compressBitpack(input_seg, &compressed, &ctx);

    assert(status == Status::OK);

    // Verify compression
    size_t uncompressed_size = values.size() * sizeof(int32_t);
    size_t compressed_size = compressed.size();
    double ratio = static_cast<double>(uncompressed_size) / compressed_size;

    printf("  Compressed %zu bytes → %zu bytes (ratio: %.2fx)\n",
           uncompressed_size, compressed_size, ratio);

    // Range 0-999 requires 10 bits per value
    // Header: 16 bytes
    // Data: 1M values * 10 bits = 10M bits = 1.25M bytes
    // Expected: 16 + 1250000 = 1250016 bytes
    // Compression ratio: 4MB / 1.25MB = ~3.2x
    assert(ratio > 3.0);

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressBitpack(compressed, DataType::INT32, input_seg.row_count,
                                      &output_seg, &ctx);

    assert(status == Status::OK);
    assert(verifyInt32Segment(output_seg, values));

    printf("  ✅ PASSED (compression ratio: %.2fx)\n\n", ratio);

    delete index;
    delete db;
}

void test_round_trip()
{
    printf("Test 9: Round-trip verification...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create random values
    std::vector<int32_t> values;
    for (int32_t i = 0; i < 1000; ++i)
    {
        values.push_back(rand() % 256);  // Random 0-255
    }
    ColumnSegment input_seg = createInt32Segment(values);

    // Compress
    std::vector<uint8_t> compressed1;
    ErrorContext ctx;
    Status status = index->compressBitpack(input_seg, &compressed1, &ctx);
    assert(status == Status::OK);

    // Decompress
    ColumnSegment output_seg;
    status = index->decompressBitpack(compressed1, DataType::INT32, input_seg.row_count,
                                      &output_seg, &ctx);
    assert(status == Status::OK);

    // Compress again
    std::vector<uint8_t> compressed2;
    status = index->compressBitpack(output_seg, &compressed2, &ctx);
    assert(status == Status::OK);

    // Verify identical
    assert(compressed1.size() == compressed2.size());
    assert(std::memcmp(compressed1.data(), compressed2.data(), compressed1.size()) == 0);

    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Columnstore Bit-Packing Compression - Unit Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    test_small_range();
    test_large_range();
    test_negative_values();
    test_null_handling();
    test_best_case_binary();
    test_worst_case_full_range();
    test_same_value();
    test_large_dataset();
    test_round_trip();

    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  ✅ ALL TESTS PASSED (9/9)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
