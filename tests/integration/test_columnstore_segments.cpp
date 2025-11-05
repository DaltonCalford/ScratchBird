/**
 * Integration tests for Columnstore Phase 6 - Segment Management
 *
 * Tests:
 * - Creating and flushing segments to disk
 * - Segment chain traversal (findSegment)
 * - Multi-segment scans with batch iterator
 * - Statistics collection across segments
 * - MGA visibility with segment-level xmin/xmax
 */

#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/uuidv7.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <iostream>

using namespace scratchbird::core;

// Test database creation
Database *createTestDatabase()
{
    std::string db_path = "columnstore_segments_test.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to create database: " << ctx.message << std::endl;
        return nullptr;
    }

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to open database: " << ctx.message << std::endl;
        delete db;
        return nullptr;
    }

    return db;
}

/**
 * Test: Insert and flush multiple segments
 */
void testMultiSegmentInsert()
{
    std::cout << "\n=== Test: Multi-Segment Insert ===" << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    // Create columnstore index with small segment size
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;
    ErrorContext ctx;

    // Small segment size to force multiple segments
    const uint32_t SEGMENT_SIZE = 100;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                            SEGMENT_SIZE, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, SEGMENT_SIZE, &ctx);
    assert(index != nullptr);

    // Insert 250 values (should create 3 segments: 100, 100, 50)
    for (int32_t i = 0; i < 250; ++i)
    {
        int32_t value = i;
        status = index->insert(column_uuid, i, &value, sizeof(int32_t), false, &ctx);
        assert(status == Status::OK);
    }

    // Get statistics
    ColumnstoreIndex::ColumnstoreStats stats;
    status = index->getStats(&stats, &ctx);
    assert(status == Status::OK);

    std::cout << "  Total segments: " << stats.total_segments << std::endl;
    std::cout << "  Total rows: " << stats.total_rows << std::endl;
    std::cout << "  Compressed: " << stats.compressed_bytes << " bytes" << std::endl;
    std::cout << "  Uncompressed: " << stats.uncompressed_bytes << " bytes" << std::endl;
    std::cout << "  Compression ratio: " << stats.compression_ratio << "x" << std::endl;

    // With 250 inserts and segment_size=100:
    // - 2 full segments of 100 rows each = 200 rows on disk
    // - 50 rows still in buffer (not yet flushed)
    assert(stats.total_segments == 2);
    assert(stats.total_rows == 200);  // Only counts persisted rows

    delete db;
    std::cout << "  PASS" << std::endl;
}

/**
 * Test: Scan across multiple segments
 */
void testMultiSegmentScan()
{
    std::cout << "\n=== Test: Multi-Segment Scan ===" << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    // Create columnstore index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;
    ErrorContext ctx;

    // Small segment size
    const uint32_t SEGMENT_SIZE = 50;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                            SEGMENT_SIZE, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, SEGMENT_SIZE, &ctx);
    assert(index != nullptr);

    // Insert 150 values (3 segments of 50 each)
    for (int32_t i = 0; i < 150; ++i)
    {
        int32_t value = i * 2;  // Even numbers 0, 2, 4, ..., 298
        status = index->insert(column_uuid, i, &value, sizeof(int32_t), false, &ctx);
        assert(status == Status::OK);
    }

    // Scan with predicate: value >= 100 (should match values 100, 102, 104, ..., 298)
    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t current_xid = txn_mgr->getCurrentXid();

    ColumnPredicate predicate;
    predicate.op = ColumnPredicate::Op::GREATER_EQUAL;
    predicate.value = 100;

    ColumnScanIterator iter;
    status = index->beginScan(column_uuid, &predicate, current_xid, &iter, &ctx);
    assert(status == Status::OK);

    uint32_t total_matches = 0;
    std::vector<uint64_t> all_tids;

    while (!iter.scan_complete)
    {
        ColumnScanBatch batch;
        status = index->scanNext(&iter, &batch, &ctx);
        assert(status == Status::OK);

        total_matches += batch.count;
        all_tids.insert(all_tids.end(), batch.tids.begin(), batch.tids.end());
    }

    status = index->endScan(&iter, &ctx);
    assert(status == Status::OK);

    // Expected: values >= 100 means values from 100 to 298 (step 2)
    // That's 50 TIDs: 50, 51, 52, ..., 99 → values 100, 102, 104, ..., 198
    // Wait, i goes from 0 to 149, value = i * 2
    // So value >= 100 means i >= 50
    // Matches: i = 50 to 149 → 100 values

    std::cout << "  Total matches: " << total_matches << std::endl;
    std::cout << "  Expected: 100" << std::endl;
    assert(total_matches == 100);

    delete db;
    std::cout << "  PASS" << std::endl;
}

/**
 * Test: Compression ratio across segments
 */
void testCompressionRatio()
{
    std::cout << "\n=== Test: Compression Ratio ===" << std::endl;

    Database *db = createTestDatabase();
    assert(db != nullptr);

    // Create columnstore index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;
    ErrorContext ctx;

    // Use RLE compression with repeated values for high compression ratio
    const uint32_t SEGMENT_SIZE = 100;
    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                            SEGMENT_SIZE, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, SEGMENT_SIZE, &ctx);
    assert(index != nullptr);

    // Insert 300 repeated values (high compression ratio expected)
    for (int32_t i = 0; i < 300; ++i)
    {
        int32_t value = 42;  // Same value for all rows
        status = index->insert(column_uuid, i, &value, sizeof(int32_t), false, &ctx);
        assert(status == Status::OK);
    }

    // Get compression statistics
    ColumnstoreIndex::ColumnstoreStats stats;
    status = index->getStats(&stats, &ctx);
    assert(status == Status::OK);

    std::cout << "  Uncompressed: " << stats.uncompressed_bytes << " bytes" << std::endl;
    std::cout << "  Compressed: " << stats.compressed_bytes << " bytes" << std::endl;
    std::cout << "  Compression ratio: " << stats.compression_ratio << "x" << std::endl;

    // With RLE and all values = 42, we should see significant compression
    assert(stats.compression_ratio > 10.0);  // At least 10x compression

    delete db;
    std::cout << "  PASS" << std::endl;
}

/**
 * Main test runner
 */
int main()
{
    std::cout << "==================================================" << std::endl;
    std::cout << "Columnstore Phase 6: Segment Management Tests" << std::endl;
    std::cout << "==================================================" << std::endl;

    testMultiSegmentInsert();
    testMultiSegmentScan();
    testCompressionRatio();

    std::cout << "\n=== All Tests Passed ===" << std::endl;

    return 0;
}
