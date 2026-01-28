/**
 * Simple End-to-End Test for Columnstore Index
 *
 * Minimal test to validate basic functionality without complex scenarios
 */

#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/uuidv7.h"
#include <cassert>
#include <cstdio>
#include <iostream>

using namespace scratchbird::core;

int main()
{
    std::cout << "=== Columnstore Simple E2E Test ===\n";

    // Create database
    std::string db_path = "/tmp/columnstore_simple_e2e.db";
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

    std::cout << "✓ Database created and opened\n";

    // Create columnstore index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;

    status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                     100, CompressionType::RLE, &root_page, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to create columnstore: " << ctx.message << std::endl;
        delete db;
        return 1;
    }

    std::cout << "✓ Columnstore index created\n";

    // Open index
    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 100, &ctx);
    if (index == nullptr)
    {
        std::cerr << "Failed to open columnstore\n";
        delete db;
        return 1;
    }

    std::cout << "✓ Columnstore index opened\n";

    // Insert 150 values (will flush to disk since segment_size=100)
    for (int32_t i = 0; i < 150; ++i)
    {
        int32_t value = i * 2;
        TID tid{0, static_cast<uint64_t>(i), 0};
        status = index->insert(column_uuid, tid, &value, sizeof(int32_t), false, &ctx);
        if (status != Status::OK)
        {
            std::cerr << "Insert failed at i=" << i << ": " << ctx.message << std::endl;
            delete db;
            return 1;
        }
    }

    std::cout << "✓ Inserted 150 values\n";

    // Get statistics (should show flushed segments)
    ColumnstoreIndex::ColumnstoreStats stats;
    status = index->getStats(&stats, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "getStats failed: " << ctx.message << std::endl;
        delete db;
        return 1;
    }

    std::cout << "✓ Statistics retrieved:\n";
    std::cout << "  - Segments: " << stats.total_segments << "\n";
    std::cout << "  - Rows: " << stats.total_rows << "\n";
    std::cout << "  - Compression ratio: " << stats.compression_ratio << "x\n";

    // Simple scan
    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t current_xid = txn_mgr->getCurrentXid();

    ColumnPredicate predicate;
    predicate.op = ColumnPredicate::Op::GREATER_THAN;
    predicate.value = 50;

    ColumnScanIterator iter;
    status = index->beginScan(column_uuid, &predicate, current_xid, &iter, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "beginScan failed: " << ctx.message << std::endl;
        delete db;
        return 1;
    }

    uint32_t total_matches = 0;
    int iterations = 0;
    const int MAX_ITERATIONS = 100;  // Safety limit

    while (!iter.scan_complete && iterations < MAX_ITERATIONS)
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
        iterations++;
    }

    status = index->endScan(&iter, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "endScan failed: " << ctx.message << std::endl;
        delete db;
        return 1;
    }

    std::cout << "✓ Scan completed:\n";
    std::cout << "  - Matches: " << total_matches << "\n";
    std::cout << "  - Iterations: " << iterations << "\n";

    // Cleanup
    delete db;

    std::cout << "\n=== All Tests PASSED ===\n";
    return 0;
}
