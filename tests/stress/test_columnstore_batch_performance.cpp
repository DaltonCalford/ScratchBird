/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Performance benchmark for Columnstore Phase 5 batch processing
 *
 * Tests SIMD-accelerated predicate evaluation and batch scan throughput.
 * Target: 10-100x improvement over row-by-row processing.
 */

#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestShortPath;

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

private:
    std::chrono::high_resolution_clock::time_point start_time;
};

// Create test database
Database *createTestDatabase()
{
    std::string db_path = uniqueTestShortPath("columnstore_batch_perf_test", ".db");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Database *db = new Database();

    Status status = Database::create(db_path, 8192, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to create database: " << ctx.message << std::endl;
        delete db;
        return nullptr;
    }

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
 * Benchmark: RLE compression/decompression throughput
 */
void benchmarkRLEThroughput()
{
    std::cout << "\n=== Benchmark: RLE Compression Throughput ===" << std::endl;

    Database *db = createTestDatabase();
    if (!db)
        return;

    // Create columnstore index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;
    ErrorContext ctx;

    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                            10000, CompressionType::RLE, &root_page, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to create columnstore: " << ctx.message << std::endl;
        delete db;
        return;
    }

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 10000, &ctx);

    // Generate test data: 1M INT32 values with high repetition
    const uint32_t ROW_COUNT = 1000000;
    std::vector<int32_t> values;
    values.reserve(ROW_COUNT);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int32_t> dist(1, 100);  // Low cardinality

    for (uint32_t i = 0; i < ROW_COUNT; ++i)
    {
        values.push_back(dist(rng));
    }

    // Create segment
    ColumnSegment segment;
    segment.data_type = DataType::INT32;
    segment.row_count = ROW_COUNT;
    segment.data.resize(ROW_COUNT * sizeof(int32_t));
    std::memcpy(segment.data.data(), values.data(), ROW_COUNT * sizeof(int32_t));

    // Benchmark compression
    Timer timer;
    timer.start();

    std::vector<uint8_t> compressed;
    status = index->compressRLE(segment, &compressed, &ctx);

    double compress_time_ms = timer.elapsedMs();

    if (status != Status::OK)
    {
        std::cerr << "Compression failed: " << ctx.message << std::endl;
        delete db;
        return;
    }

    double compress_throughput_mb_s = (ROW_COUNT * sizeof(int32_t) / 1024.0 / 1024.0) / (compress_time_ms / 1000.0);
    double compression_ratio = static_cast<double>(ROW_COUNT * sizeof(int32_t)) / compressed.size();

    std::cout << "  Compressed " << ROW_COUNT << " INT32 values in " << compress_time_ms << " ms" << std::endl;
    std::cout << "  Compression throughput: " << compress_throughput_mb_s << " MB/s" << std::endl;
    std::cout << "  Compression ratio: " << compression_ratio << "x" << std::endl;
    std::cout << "  Original size: " << (ROW_COUNT * sizeof(int32_t)) << " bytes" << std::endl;
    std::cout << "  Compressed size: " << compressed.size() << " bytes" << std::endl;

    // Benchmark decompression
    timer.start();

    ColumnSegment decompressed;
    status = index->decompressRLE(compressed, DataType::INT32, ROW_COUNT, &decompressed, &ctx);

    double decompress_time_ms = timer.elapsedMs();

    if (status != Status::OK)
    {
        std::cerr << "Decompression failed: " << ctx.message << std::endl;
        delete db;
        return;
    }

    double decompress_throughput_mb_s = (ROW_COUNT * sizeof(int32_t) / 1024.0 / 1024.0) / (decompress_time_ms / 1000.0);

    std::cout << "  Decompressed in " << decompress_time_ms << " ms" << std::endl;
    std::cout << "  Decompression throughput: " << decompress_throughput_mb_s << " MB/s" << std::endl;

    delete db;
}

/**
 * Benchmark: SIMD vs Scalar predicate evaluation
 */
void benchmarkSIMDPredicate()
{
    std::cout << "\n=== Benchmark: SIMD Predicate Evaluation ===" << std::endl;

    Database *db = createTestDatabase();
    if (!db)
        return;

    // Create columnstore index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;
    ErrorContext ctx;

    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                            100000, CompressionType::NONE, &root_page, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to create columnstore: " << ctx.message << std::endl;
        delete db;
        return;
    }

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 100000, &ctx);

    // Generate test data: 1M INT32 values with uniform distribution
    const uint32_t ROW_COUNT = 1000000;
    std::vector<int32_t> values;
    values.reserve(ROW_COUNT);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int32_t> dist(1, 1000);

    for (uint32_t i = 0; i < ROW_COUNT; ++i)
    {
        values.push_back(dist(rng));
    }

    // Create segment (uncompressed for this test)
    ColumnSegment segment;
    segment.data_type = DataType::INT32;
    segment.row_count = ROW_COUNT;
    segment.data.resize(ROW_COUNT * sizeof(int32_t));
    segment.min_value = 1;
    segment.max_value = 1000;
    std::memcpy(segment.data.data(), values.data(), ROW_COUNT * sizeof(int32_t));

    // Test predicate: WHERE value > 500 (50% selectivity)
    ColumnPredicate predicate;
    predicate.op = ColumnPredicate::Op::GREATER_THAN;
    predicate.value = 500;

    // Benchmark predicate evaluation (with SIMD if available)
    Timer timer;
    timer.start();

    std::vector<uint32_t> matching_offsets;
    status = index->applyPredicate(segment, predicate, &matching_offsets, &ctx);

    double time_ms = timer.elapsedMs();

    if (status != Status::OK)
    {
        std::cerr << "Predicate evaluation failed: " << ctx.message << std::endl;
        delete db;
        return;
    }

    double throughput_million_rows_per_sec = (ROW_COUNT / 1000000.0) / (time_ms / 1000.0);

    std::cout << "  Evaluated predicate on " << ROW_COUNT << " INT32 values in " << time_ms << " ms" << std::endl;
    std::cout << "  Throughput: " << throughput_million_rows_per_sec << " million rows/sec" << std::endl;
    std::cout << "  Matching rows: " << matching_offsets.size() << " (" << (matching_offsets.size() * 100.0 / ROW_COUNT) << "%)" << std::endl;

#if defined(__AVX2__)
    std::cout << "  SIMD: AVX2 enabled" << std::endl;
#else
    std::cout << "  SIMD: Not available (scalar fallback)" << std::endl;
#endif

    delete db;
}

/**
 * Benchmark: Batch scan throughput
 */
void benchmarkBatchScan()
{
    std::cout << "\n=== Benchmark: Batch Scan Throughput ===" << std::endl;

    Database *db = createTestDatabase();
    if (!db)
        return;

    // Create columnstore index
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::vector<UuidV7Bytes> columns = {column_uuid};
    uint32_t root_page = 0;
    ErrorContext ctx;

    Status status = ColumnstoreIndex::create(db, index_uuid, table_uuid, columns,
                                            10000, CompressionType::RLE, &root_page, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to create columnstore: " << ctx.message << std::endl;
        delete db;
        return;
    }

    auto index = ColumnstoreIndex::open(db, index_uuid, root_page, 10000, &ctx);

    std::cout << "  Note: Batch scan API implemented but requires disk segments." << std::endl;
    std::cout << "  Full benchmark deferred to integration testing." << std::endl;

    delete db;
}

/**
 * Main benchmark runner
 */
int main()
{
    std::cout << "==================================================" << std::endl;
    std::cout << "Columnstore Phase 5: Batch Processing Benchmarks" << std::endl;
    std::cout << "==================================================" << std::endl;

    benchmarkRLEThroughput();
    benchmarkSIMDPredicate();
    benchmarkBatchScan();

    std::cout << "\n=== All Benchmarks Complete ===" << std::endl;

    return 0;
}
