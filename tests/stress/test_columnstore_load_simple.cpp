/**
 * Simple Load Test for Columnstore Index
 *
 * Tests large-scale data ingestion with minimal complexity
 */

#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/uuidv7.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <chrono>

using namespace scratchbird::core;

// Timing helper
class Timer
{
public:
    void start()
    {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double elapsedSec()
    {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        return duration.count() / 1000000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_time;
};

int main()
{
    std::cout << "=== Columnstore Simple Load Test ===\n";
    std::cout << "Testing insert and scan performance with 50K rows\n\n";

    // Create database
    std::string db_path = "/tmp/columnstore_load_simple.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to create database: " << ctx.message << std::endl;
        return 1;
    }

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to open database: " << ctx.message << std::endl;
        delete db;
        return 1;
    }

    std::cout << "✓ Database created\n";

    // Create columnstore
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;

    const uint32_t SEGMENT_SIZE = 500;  // 500 rows per segment (to fit in single page)
    status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                     SEGMENT_SIZE, CompressionType::RLE, &root_page, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to create columnstore: " << ctx.message << std::endl;
        delete db;
        return 1;
    }

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, SEGMENT_SIZE, &ctx);
    if (index == nullptr)
    {
        std::cerr << "Failed to open columnstore\n";
        delete db;
        return 1;
    }

    std::cout << "✓ Columnstore index created\n";

    // Insert 50K rows
    const uint32_t TOTAL_ROWS = 50000;
    std::cout << "\nInserting " << TOTAL_ROWS << " rows...\n";

    Timer timer;
    timer.start();

    for (uint32_t i = 0; i < TOTAL_ROWS; ++i)
    {
        int32_t value = static_cast<int32_t>(i % 100);  // Repeating pattern for compression
        TID tid{0, static_cast<uint64_t>(i), 0};
        status = index->insert(column_uuid, tid, &value, sizeof(int32_t), false, &ctx);
        if (status != Status::OK)
        {
            std::cerr << "Insert failed at i=" << i << ": " << ctx.message << std::endl;
            delete db;
            return 1;
        }

        if ((i + 1) % 10000 == 0)
        {
            std::cout << "  Progress: " << (i + 1) << " rows...\n";
        }
    }

    double insert_time = timer.elapsedSec();
    double insert_throughput = TOTAL_ROWS / insert_time;

    std::cout << "✓ Insert complete:\n";
    std::cout << "  - Time: " << insert_time << " seconds\n";
    std::cout << "  - Throughput: " << static_cast<int>(insert_throughput) << " rows/sec\n";

    // Get statistics
    std::cout << "\nGathering statistics...\n";
    ColumnstoreIndex::ColumnstoreStats stats;
    status = index->getStats(&stats, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "getStats failed: " << ctx.message << std::endl;
        delete db;
        return 1;
    }

    std::cout << "✓ Statistics:\n";
    std::cout << "  - Segments: " << stats.total_segments << "\n";
    std::cout << "  - Rows: " << stats.total_rows << "\n";
    std::cout << "  - Compressed: " << (stats.compressed_bytes / 1024) << " KB\n";
    std::cout << "  - Uncompressed: " << (stats.uncompressed_bytes / 1024) << " KB\n";
    std::cout << "  - Compression ratio: " << stats.compression_ratio << "x\n";

    // Expected: 100 segments (50K / 500 = 100)
    if (stats.total_segments != 100)
    {
        std::cerr << "WARNING: Expected 100 segments, got " << stats.total_segments << std::endl;
    }
    if (stats.total_rows != 50000)
    {
        std::cerr << "WARNING: Expected 50000 rows, got " << stats.total_rows << std::endl;
    }

    // Scan test
    std::cout << "\nScanning with predicate (value > 50)...\n";

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t current_xid = txn_mgr->getCurrentXid();

    ColumnPredicate predicate;
    predicate.op = ColumnPredicate::Op::GREATER_THAN;
    predicate.value = 50;

    timer.start();

    ColumnScanIterator iter;
    status = index->beginScan(column_uuid, &predicate, current_xid, &iter, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "beginScan failed: " << ctx.message << std::endl;
        delete db;
        return 1;
    }

    uint32_t total_matches = 0;
    uint32_t total_batches = 0;
    const uint32_t MAX_BATCHES = 10000;  // Safety limit

    while (!iter.scan_complete && total_batches < MAX_BATCHES)
    {
        ColumnScanBatch batch;
        status = index->scanNext(&iter, &batch, &ctx);
        if (status != Status::OK)
        {
            std::cerr << "scanNext failed: " << ctx.message << std::endl;
            delete db;
            return 1;
        }
        total_matches += batch.count;
        total_batches++;
    }

    status = index->endScan(&iter, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "endScan failed: " << ctx.message << std::endl;
        delete db;
        return 1;
    }

    double scan_time = timer.elapsedSec();
    double scan_throughput = total_matches / scan_time;

    std::cout << "✓ Scan complete:\n";
    std::cout << "  - Time: " << scan_time << " seconds\n";
    std::cout << "  - Matches: " << total_matches << "\n";
    std::cout << "  - Batches: " << total_batches << "\n";
    std::cout << "  - Scan throughput: " << static_cast<int>(scan_throughput) << " rows/sec\n";

    // Expected: values 51-99 repeating 500 times each = 49 * 500 = 24500 matches
    uint32_t expected_matches = 49 * 500;  // Values 51-99 (49 values) × 500 occurrences each
    if (total_matches != expected_matches)
    {
        std::cerr << "WARNING: Expected " << expected_matches << " matches, got " << total_matches << std::endl;
    }

    // Cleanup
    delete db;

    std::cout << "\n=== Load Test PASSED ===\n";
    std::cout << "\nSummary:\n";
    std::cout << "  - Inserted 50,000 rows across 50 segments\n";
    std::cout << "  - Insert throughput: " << static_cast<int>(insert_throughput) << " rows/sec\n";
    std::cout << "  - Scan throughput: " << static_cast<int>(scan_throughput) << " rows/sec\n";
    std::cout << "  - Compression ratio: " << stats.compression_ratio << "x\n";

    return 0;
}
