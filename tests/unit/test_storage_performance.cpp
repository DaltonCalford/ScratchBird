#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <cstdio>

using namespace scratchbird::core;
using namespace std::chrono;

class StoragePerformanceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        cleanup_test_files();
    }

    void TearDown() override
    {
        cleanup_test_files();
    }

    void cleanup_test_files()
    {
        std::filesystem::remove("test_perf.db");
    }

    struct BenchmarkResult
    {
        std::string operation;
        int count;
        double duration_seconds;
        double rate_per_second;
        size_t bytes_processed;
        double throughput_mb_per_second;
    };

    void print_benchmark_header()
    {
        std::cout << "\n╔═══════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║              Storage Engine Performance Benchmarks                 ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════════════╝\n";
    }

    void print_result(const BenchmarkResult &result)
    {
        std::cout << "\n" << result.operation << ":\n";
        std::cout << "  Count: " << result.count << "\n";
        std::cout << "  Time: " << std::fixed << std::setprecision(3) << result.duration_seconds
                  << " seconds\n";
        std::cout << "  Rate: " << std::fixed << std::setprecision(0) << result.rate_per_second
                  << " ops/sec\n";
        if (result.bytes_processed > 0)
        {
            std::cout << "  Throughput: " << std::fixed << std::setprecision(2)
                      << result.throughput_mb_per_second << " MB/sec\n";
        }
    }
};

// Benchmark: Sequential Insert Performance
TEST_F(StoragePerformanceTest, SequentialInsertBenchmark)
{
    print_benchmark_header();

    const std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536, 131072};
    const std::vector<size_t> tuple_sizes = {100, 500, 1000, 2000};
    const int num_tuples = 5000;

    for (uint32_t page_size : page_sizes)
    {
        std::cout << "\n=== Page Size: " << page_size << " bytes ===\n";

        for (size_t tuple_size : tuple_sizes)
        {
            cleanup_test_files();
            ASSERT_EQ(Database::create("test_perf.db", page_size), Status::OK);

            Database db;
            ASSERT_EQ(db.open("test_perf.db"), Status::OK);

            StorageEngine engine(&db);

            std::vector<uint8_t> tuple_data(tuple_size, 0xAA);

            auto start = high_resolution_clock::now();

            for (int i = 0; i < num_tuples; i++)
            {
                // Vary data to avoid compression benefits
                tuple_data[0] = i & 0xFF;
                tuple_data[1] = (i >> 8) & 0xFF;

                uint32_t page_id;
                uint16_t item_id;
                Status status = engine.insertTuple(1, tuple_data.data(),
                                                    tuple_data.size() + sizeof(TupleHeader),
                                                    &page_id, &item_id, nullptr);
                ASSERT_EQ(status, Status::OK);
            }

            db.sync(nullptr); // Ensure all data is written

            auto end = high_resolution_clock::now();
            duration<double> elapsed = end - start;

            BenchmarkResult result;
            result.operation = "Insert " + std::to_string(tuple_size) + "B tuples";
            result.count = num_tuples;
            result.duration_seconds = elapsed.count();
            result.rate_per_second = num_tuples / elapsed.count();
            result.bytes_processed = num_tuples * (tuple_size + sizeof(TupleHeader));
            result.throughput_mb_per_second =
                result.bytes_processed / (1024.0 * 1024.0) / elapsed.count();

            print_result(result);

            db.close();
        }
    }
}

// Benchmark: Sequential Scan Performance
TEST_F(StoragePerformanceTest, SequentialScanBenchmark)
{
    std::cout << "\n\n=== Sequential Scan Benchmark ===\n";

    // Setup: Create database with various tuple counts
    const std::vector<int> tuple_counts = {1000, 5000, 10000, 25000};
    const size_t tuple_size = 500;

    for (int count : tuple_counts)
    {
        cleanup_test_files();
        ASSERT_EQ(Database::create("test_perf.db", 32768), Status::OK);

        Database db;
        ASSERT_EQ(db.open("test_perf.db"), Status::OK);

        StorageEngine engine(&db);

        // Insert tuples
        std::vector<uint8_t> tuple_data(tuple_size, 0xBB);
        for (int i = 0; i < count; i++)
        {
            tuple_data[0] = i & 0xFF;

            uint32_t page_id;
            uint16_t item_id;
            Status status =
                engine.insertTuple(1, tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                    &page_id, &item_id, nullptr);
            ASSERT_EQ(status, Status::OK);
        }

        db.sync(nullptr);

        // Benchmark scan
        const int num_scans = 10;
        auto start = high_resolution_clock::now();

        int total_tuples_scanned = 0;
        for (int scan = 0; scan < num_scans; scan++)
        {
            auto iterator = engine.create_scan(1, nullptr);
            Tuple tuple;
            while (!iterator->is_done())
            {
                if (iterator->next(&tuple, nullptr) == Status::OK)
                {
                    total_tuples_scanned++;
                }
            }
        }

        auto end = high_resolution_clock::now();
        duration<double> elapsed = end - start;

        BenchmarkResult result;
        result.operation = "Scan " + std::to_string(count) + " tuples";
        result.count = total_tuples_scanned;
        result.duration_seconds = elapsed.count();
        result.rate_per_second = total_tuples_scanned / elapsed.count();
        result.bytes_processed = total_tuples_scanned * (tuple_size + sizeof(TupleHeader));
        result.throughput_mb_per_second =
            result.bytes_processed / (1024.0 * 1024.0) / elapsed.count();

        print_result(result);

        db.close();
    }
}

// Benchmark: Random Access Performance
TEST_F(StoragePerformanceTest, RandomAccessBenchmark)
{
    std::cout << "\n\n=== Random Access Benchmark ===\n";

    ASSERT_EQ(Database::create("test_perf.db", 16384), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_perf.db"), Status::OK);

    StorageEngine engine(&db);

    // Insert tuples and keep track of their IDs
    const int num_tuples = 10000;
    const size_t tuple_size = 200;
    std::vector<std::pair<uint32_t, uint16_t>> tuple_ids;
    std::vector<uint8_t> tuple_data(tuple_size, 0xCC);

    for (int i = 0; i < num_tuples; i++)
    {
        tuple_data[0] = i & 0xFF;
        tuple_data[1] = (i >> 8) & 0xFF;

        uint32_t page_id;
        uint16_t item_id;
        Status status =
            engine.insertTuple(1, tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        ASSERT_EQ(status, Status::OK);
        tuple_ids.push_back({page_id, item_id});
    }

    db.sync(nullptr);

    // Random access benchmark
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_tuples - 1);

    const int num_accesses = 10000;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < num_accesses; i++)
    {
        int idx = dis(gen);
        Tuple tuple;
        Status status =
            engine.getTuple(tuple_ids[idx].first, tuple_ids[idx].second, &tuple, nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    auto end = high_resolution_clock::now();
    duration<double> elapsed = end - start;

    BenchmarkResult result;
    result.operation = "Random Get";
    result.count = num_accesses;
    result.duration_seconds = elapsed.count();
    result.rate_per_second = num_accesses / elapsed.count();
    result.bytes_processed = 0; // Not counting for random access
    result.throughput_mb_per_second = 0;

    print_result(result);

    db.close();
}

// Benchmark: Mixed Workload
TEST_F(StoragePerformanceTest, MixedWorkloadBenchmark)
{
    std::cout << "\n\n=== Mixed Workload Benchmark ===\n";
    std::cout << "(80% reads, 15% inserts, 5% deletes)\n";

    ASSERT_EQ(Database::create("test_perf.db", 16384), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_perf.db"), Status::OK);

    StorageEngine engine(&db);

    // Initial data
    const int initial_tuples = 5000;
    const size_t tuple_size = 300;
    std::vector<std::pair<uint32_t, uint16_t>> tuple_ids;
    std::vector<uint8_t> tuple_data(tuple_size, 0xDD);

    for (int i = 0; i < initial_tuples; i++)
    {
        uint32_t page_id;
        uint16_t item_id;
        Status status =
            engine.insertTuple(1, tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        ASSERT_EQ(status, Status::OK);
        tuple_ids.push_back({page_id, item_id});
    }

    // Mixed operations
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> op_dis(1, 100);
    std::uniform_int_distribution<> idx_dis(0, tuple_ids.size() - 1);

    const int num_operations = 20000;
    int read_count = 0, insert_count = 0, delete_count = 0;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < num_operations; i++)
    {
        int op = op_dis(gen);

        if (op <= 80)
        { // 80% reads
            if (!tuple_ids.empty())
            {
                int idx = idx_dis(gen) % tuple_ids.size();
                Tuple tuple;
                engine.getTuple(tuple_ids[idx].first, tuple_ids[idx].second, &tuple, nullptr);
                read_count++;
            }
        }
        else if (op <= 95)
        { // 15% inserts
            uint32_t page_id;
            uint16_t item_id;
            Status status =
                engine.insertTuple(1, tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                    &page_id, &item_id, nullptr);
            if (status == Status::OK)
            {
                tuple_ids.push_back({page_id, item_id});
                insert_count++;
            }
        }
        else
        { // 5% deletes
            if (!tuple_ids.empty())
            {
                int idx = idx_dis(gen) % tuple_ids.size();
                Status status =
                    engine.deleteTuple(tuple_ids[idx].first, tuple_ids[idx].second, nullptr);
                if (status == Status::OK)
                {
                    tuple_ids.erase(tuple_ids.begin() + idx);
                    delete_count++;
                }
            }
        }
    }

    auto end = high_resolution_clock::now();
    duration<double> elapsed = end - start;

    std::cout << "\nResults:\n";
    std::cout << "  Total operations: " << num_operations << "\n";
    std::cout << "  Reads: " << read_count << " (" << (read_count * 100.0 / num_operations)
              << "%)\n";
    std::cout << "  Inserts: " << insert_count << " (" << (insert_count * 100.0 / num_operations)
              << "%)\n";
    std::cout << "  Deletes: " << delete_count << " (" << (delete_count * 100.0 / num_operations)
              << "%)\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(3) << elapsed.count()
              << " seconds\n";
    std::cout << "  Operations/sec: " << std::fixed << std::setprecision(0)
              << num_operations / elapsed.count() << "\n";

    db.close();
}

// Benchmark: Page Fill Efficiency
TEST_F(StoragePerformanceTest, PageFillEfficiencyBenchmark)
{
    std::cout << "\n\n=== Page Fill Efficiency Benchmark ===\n";

    const std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536, 131072};
    const std::vector<size_t> tuple_sizes = {50, 100, 200, 500, 1000};

    for (uint32_t page_size : page_sizes)
    {
        std::cout << "\nPage Size: " << page_size << " bytes\n";

        for (size_t tuple_size : tuple_sizes)
        {
            cleanup_test_files();
            ASSERT_EQ(Database::create("test_perf.db", page_size), Status::OK);

            Database db;
            ASSERT_EQ(db.open("test_perf.db"), Status::OK);

            StorageEngine engine(&db);

            // Calculate theoretical capacity
            size_t overhead_per_page = sizeof(PageHeader) + sizeof(HeapPageSpecial);
            size_t overhead_per_tuple = sizeof(ItemPointer) + sizeof(TupleHeader);
            size_t usable_space = page_size - overhead_per_page;
            size_t total_tuple_size = tuple_size + overhead_per_tuple;
            int theoretical_tuples_per_page = usable_space / total_tuple_size;

            // Fill one page
            std::vector<uint8_t> tuple_data(tuple_size, 0xEE);
            uint32_t first_page_id = 0;
            int tuples_in_page = 0;

            for (int i = 0; i < theoretical_tuples_per_page * 2; i++)
            {
                uint32_t page_id;
                uint16_t item_id;
                Status status = engine.insertTuple(1, tuple_data.data(),
                                                    tuple_data.size() + sizeof(TupleHeader),
                                                    &page_id, &item_id, nullptr);

                if (status != Status::OK)
                    break;

                if (first_page_id == 0)
                    first_page_id = page_id;

                if (page_id == first_page_id)
                {
                    tuples_in_page++;
                }
                else
                {
                    break; // Moved to next page
                }
            }

            double efficiency = (tuples_in_page * 100.0) / theoretical_tuples_per_page;

            std::cout << "  Tuple size " << tuple_size << "B: " << tuples_in_page << " tuples/page "
                      << "(theoretical: " << theoretical_tuples_per_page << ", "
                      << "efficiency: " << std::fixed << std::setprecision(1) << efficiency
                      << "%)\n";

            db.close();
        }
    }
}

// Benchmark: Transaction Overhead
TEST_F(StoragePerformanceTest, TransactionOverheadBenchmark)
{
    std::cout << "\n\n=== Transaction Overhead Benchmark ===\n";

    ASSERT_EQ(Database::create("test_perf.db", 16384), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_perf.db"), Status::OK);

    StorageEngine engine(&db);

    const int num_operations = 10000;
    std::vector<uint8_t> tuple_data(200, 0xFF);

    // Benchmark 1: One transaction per operation
    auto start1 = high_resolution_clock::now();

    for (int i = 0; i < num_operations; i++)
    {
        // Transaction management is now handled by TransactionManager

        uint32_t page_id;
        uint16_t item_id;
        engine.insertTuple(1, tuple_data.data(), tuple_data.size() + sizeof(TupleHeader), &page_id,
                            &item_id, nullptr);
    }

    auto end1 = high_resolution_clock::now();
    duration<double> elapsed1 = end1 - start1;

    // Benchmark 2: Batch operations in fewer transactions
    const int batch_size = 100;
    cleanup_test_files();
    ASSERT_EQ(Database::create("test_perf.db", 16384), Status::OK);
    Database db2;
    ASSERT_EQ(db2.open("test_perf.db"), Status::OK);
    StorageEngine engine2(&db2);

    auto start2 = high_resolution_clock::now();

    for (int i = 0; i < num_operations; i++)
    {
        if (i % batch_size == 0)
        {
            // Transaction management is now handled by TransactionManager
        }

        uint32_t page_id;
        uint16_t item_id;
        engine2.insertTuple(1, tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                             &page_id, &item_id, nullptr);
    }

    auto end2 = high_resolution_clock::now();
    duration<double> elapsed2 = end2 - start2;

    std::cout << "\nResults:\n";
    std::cout << "  One transaction per operation:\n";
    std::cout << "    Time: " << std::fixed << std::setprecision(3) << elapsed1.count()
              << " seconds\n";
    std::cout << "    Rate: " << std::fixed << std::setprecision(0)
              << num_operations / elapsed1.count() << " ops/sec\n";

    std::cout << "\n  Batched transactions (" << batch_size << " ops/txn):\n";
    std::cout << "    Time: " << std::fixed << std::setprecision(3) << elapsed2.count()
              << " seconds\n";
    std::cout << "    Rate: " << std::fixed << std::setprecision(0)
              << num_operations / elapsed2.count() << " ops/sec\n";

    std::cout << "\n  Speedup: " << std::fixed << std::setprecision(2)
              << elapsed1.count() / elapsed2.count() << "x\n";

    db.close();
    db2.close();
}