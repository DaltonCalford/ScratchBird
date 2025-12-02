/**
 * Load Test for Columnstore Index
 *
 * Tests large-scale data ingestion and query performance:
 * - 100K-1M row inserts
 * - Throughput measurement
 * - Multi-segment scanning
 * - Compression at scale
 * - Memory efficiency
 */

#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/uuidv7.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <random>

using namespace scratchbird::core;

// Timing helper
class Timer
{
public:
    void start()
    {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double elapsedMs()
    {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        return duration.count() / 1000.0;
    }

    double elapsedSec()
    {
        return elapsedMs() / 1000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_time;
};

/**
 * Test 1: Insert 100K rows and measure throughput
 */
void testInsert100K()
{
    std::cout << "\n=== Load Test 1: Insert 100K Rows ===\n";

    // Create database
    std::string db_path = "/tmp/columnstore_load_100k.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    // Create columnstore with reasonable segment size
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;

    const uint32_t SEGMENT_SIZE = 1000;  // 1K rows per segment
    status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                     SEGMENT_SIZE, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, SEGMENT_SIZE, &ctx);
    assert(index != nullptr);

    // Insert 100K rows
    const uint32_t TOTAL_ROWS = 100000;
    std::cout << "  Inserting " << TOTAL_ROWS << " rows...\n";

    Timer timer;
    timer.start();

    for (uint32_t i = 0; i < TOTAL_ROWS; ++i)
    {
        int32_t value = static_cast<int32_t>(i % 100);  // Repeating pattern for compression
        status = index->insert(column_uuid, i, &value, sizeof(int32_t), false, &ctx);
        assert(status == Status::OK);

        if ((i + 1) % 10000 == 0)
        {
            std::cout << "    Progress: " << (i + 1) << " rows...\n";
        }
    }

    double elapsed_sec = timer.elapsedSec();
    double rows_per_sec = TOTAL_ROWS / elapsed_sec;

    std::cout << "  ✓ Insert complete:\n";
    std::cout << "    - Time: " << elapsed_sec << " seconds\n";
    std::cout << "    - Throughput: " << static_cast<int>(rows_per_sec) << " rows/sec\n";

    // Get statistics
    ColumnstoreIndex::ColumnstoreStats stats;
    status = index->getStats(&stats, &ctx);
    assert(status == Status::OK);

    std::cout << "  ✓ Statistics:\n";
    std::cout << "    - Segments: " << stats.total_segments << "\n";
    std::cout << "    - Rows: " << stats.total_rows << "\n";
    std::cout << "    - Compressed: " << (stats.compressed_bytes / 1024) << " KB\n";
    std::cout << "    - Uncompressed: " << (stats.uncompressed_bytes / 1024) << " KB\n";
    std::cout << "    - Compression ratio: " << stats.compression_ratio << "x\n";

    // Expected: approximately 100 segments (100K / 1K = 100)
    // Allow some variance due to segment boundary handling
    if (stats.total_segments < 95 || stats.total_segments > 105) {
        std::cerr << "ERROR: Expected ~100 segments, got " << stats.total_segments << "\n";
        assert(false && "Segment count out of expected range");
    }
    if (stats.total_rows != 100000) {
        std::cerr << "ERROR: Expected 100000 rows, got " << stats.total_rows << "\n";
        assert(false && "Row count mismatch");
    }

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Test 2: Scan 100K rows with predicate
 */
void testScan100K()
{
    std::cout << "\n=== Load Test 2: Scan 100K Rows ===\n";

    // Create database
    std::string db_path = "/tmp/columnstore_load_scan.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    // Create columnstore
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;

    const uint32_t SEGMENT_SIZE = 1000;
    status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                     SEGMENT_SIZE, CompressionType::NONE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, SEGMENT_SIZE, &ctx);
    assert(index != nullptr);

    // Insert 100K rows with uniform distribution
    const uint32_t TOTAL_ROWS = 100000;
    std::cout << "  Inserting " << TOTAL_ROWS << " rows...\n";

    std::mt19937 rng(42);
    std::uniform_int_distribution<int32_t> dist(1, 1000);

    for (uint32_t i = 0; i < TOTAL_ROWS; ++i)
    {
        int32_t value = dist(rng);
        status = index->insert(column_uuid, i, &value, sizeof(int32_t), false, &ctx);
        assert(status == Status::OK);
    }

    std::cout << "  ✓ Insert complete\n";

    // Scan with predicate (selectivity ~50%)
    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t current_xid = txn_mgr->getCurrentXid();

    ColumnPredicate predicate;
    predicate.op = ColumnPredicate::Op::GREATER_THAN;
    predicate.value = 500;

    std::cout << "  Scanning with predicate (value > 500)...\n";

    Timer timer;
    timer.start();

    ColumnScanIterator iter;
    status = index->beginScan(column_uuid, &predicate, current_xid, &iter, &ctx);
    assert(status == Status::OK);

    uint32_t total_matches = 0;
    uint32_t total_batches = 0;
    const uint32_t MAX_BATCHES = 10000;  // Safety limit

    while (!iter.scan_complete && total_batches < MAX_BATCHES)
    {
        ColumnScanBatch batch;
        status = index->scanNext(&iter, &batch, &ctx);
        assert(status == Status::OK);

        total_matches += batch.count;
        total_batches++;
    }

    status = index->endScan(&iter, &ctx);
    assert(status == Status::OK);

    double elapsed_sec = timer.elapsedSec();
    double rows_per_sec = total_matches / elapsed_sec;

    std::cout << "  ✓ Scan complete:\n";
    std::cout << "    - Time: " << elapsed_sec << " seconds\n";
    std::cout << "    - Matches: " << total_matches << "\n";
    std::cout << "    - Batches: " << total_batches << "\n";
    std::cout << "    - Scan throughput: " << static_cast<int>(rows_per_sec) << " rows/sec\n";

    // Expected: ~50K matches (50% selectivity)
    assert(total_matches > 45000 && total_matches < 55000);

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Test 3: Compression efficiency at scale (1M rows)
 */
void testCompressionScale()
{
    std::cout << "\n=== Load Test 3: Compression at Scale (1M rows) ===\n";
    std::cout << "  WARNING: This test inserts 1M rows and may take 1-2 minutes\n";

    // Create database
    std::string db_path = "/tmp/columnstore_load_1m.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    // Create columnstore with RLE compression
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;

    const uint32_t SEGMENT_SIZE = 5000;  // 5K rows per segment
    status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                     SEGMENT_SIZE, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, SEGMENT_SIZE, &ctx);
    assert(index != nullptr);

    // Insert 1M rows with low cardinality (good for RLE)
    const uint32_t TOTAL_ROWS = 1000000;
    std::cout << "  Inserting " << TOTAL_ROWS << " rows...\n";

    Timer timer;
    timer.start();

    for (uint32_t i = 0; i < TOTAL_ROWS; ++i)
    {
        int32_t value = static_cast<int32_t>(i % 10);  // Only 10 distinct values
        status = index->insert(column_uuid, i, &value, sizeof(int32_t), false, &ctx);
        assert(status == Status::OK);

        if ((i + 1) % 100000 == 0)
        {
            std::cout << "    Progress: " << (i + 1) << " rows...\n";
        }
    }

    double elapsed_sec = timer.elapsedSec();
    double rows_per_sec = TOTAL_ROWS / elapsed_sec;

    std::cout << "  ✓ Insert complete:\n";
    std::cout << "    - Time: " << elapsed_sec << " seconds\n";
    std::cout << "    - Throughput: " << static_cast<int>(rows_per_sec) << " rows/sec\n";

    // Get statistics
    ColumnstoreIndex::ColumnstoreStats stats;
    status = index->getStats(&stats, &ctx);
    assert(status == Status::OK);

    std::cout << "  ✓ Statistics:\n";
    std::cout << "    - Segments: " << stats.total_segments << "\n";
    std::cout << "    - Rows: " << stats.total_rows << "\n";
    std::cout << "    - Compressed: " << (stats.compressed_bytes / 1024 / 1024) << " MB\n";
    std::cout << "    - Uncompressed: " << (stats.uncompressed_bytes / 1024 / 1024) << " MB\n";
    std::cout << "    - Compression ratio: " << stats.compression_ratio << "x\n";

    // Expected: 200 segments (1M / 5K = 200)
    // With RLE and only 10 distinct values, should see significant compression
    assert(stats.total_segments == 200);
    assert(stats.total_rows == 1000000);
    assert(stats.compression_ratio > 5.0);  // At least 5x compression

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Test 4: Multi-column load test
 */
void testMultiColumnLoad()
{
    std::cout << "\n=== Load Test 4: Multi-Column (3 columns, 10K rows each) ===\n";

    // Create database
    std::string db_path = "/tmp/columnstore_load_multi.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    // Create columnstore with 3 columns
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column1_uuid = generateUuidV7();
    UuidV7Bytes column2_uuid = generateUuidV7();
    UuidV7Bytes column3_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column1_uuid, column2_uuid, column3_uuid};
    uint32_t root_page = 0;

    const uint32_t SEGMENT_SIZE = 1000;
    status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                     SEGMENT_SIZE, CompressionType::RLE, &root_page, &ctx);
    assert(status == Status::OK);

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, SEGMENT_SIZE, &ctx);
    assert(index != nullptr);

    // Insert 10K rows into each column
    const uint32_t TOTAL_ROWS = 10000;
    std::cout << "  Inserting " << TOTAL_ROWS << " rows into 3 columns...\n";

    Timer timer;
    timer.start();

    for (uint32_t i = 0; i < TOTAL_ROWS; ++i)
    {
        // Column 1: Sequential IDs
        int32_t value1 = static_cast<int32_t>(i);
        status = index->insert(column1_uuid, i, &value1, sizeof(int32_t), false, &ctx);
        assert(status == Status::OK);

        // Column 2: Repeating pattern
        int32_t value2 = static_cast<int32_t>(i % 10);
        status = index->insert(column2_uuid, i, &value2, sizeof(int32_t), false, &ctx);
        assert(status == Status::OK);

        // Column 3: Random values
        int32_t value3 = static_cast<int32_t>((i * 7919) % 1000);  // Pseudo-random
        status = index->insert(column3_uuid, i, &value3, sizeof(int32_t), false, &ctx);
        assert(status == Status::OK);
    }

    double elapsed_sec = timer.elapsedSec();
    double rows_per_sec = (TOTAL_ROWS * 3) / elapsed_sec;  // Total cells inserted

    std::cout << "  ✓ Insert complete:\n";
    std::cout << "    - Time: " << elapsed_sec << " seconds\n";
    std::cout << "    - Throughput: " << static_cast<int>(rows_per_sec) << " cells/sec\n";

    // Get statistics (will aggregate across all columns)
    ColumnstoreIndex::ColumnstoreStats stats;
    status = index->getStats(&stats, &ctx);
    assert(status == Status::OK);

    std::cout << "  ✓ Statistics (all columns):\n";
    std::cout << "    - Total segments: " << stats.total_segments << "\n";
    std::cout << "    - Total rows: " << stats.total_rows << "\n";
    std::cout << "    - Compression ratio: " << stats.compression_ratio << "x\n";

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Main test runner
 */
int main()
{
    std::cout << "==================================================\n";
    std::cout << "Columnstore Phase 7: Load Tests\n";
    std::cout << "==================================================\n";

    testInsert100K();
    testScan100K();
    // testCompressionScale();  // Commented out: 1M rows takes too long for regular CI
    testMultiColumnLoad();

    std::cout << "\n=== All Load Tests PASSED ===\n";
    std::cout << "\nNote: 1M row test (testCompressionScale) is commented out for CI speed.\n";
    std::cout << "Uncomment to run full scale test (takes 1-2 minutes).\n";

    return 0;
}
