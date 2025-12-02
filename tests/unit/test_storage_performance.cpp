/**
 * Storage Performance Tests using HeapPage directly
 *
 * These tests benchmark heap page operations without requiring
 * CatalogManager integration. Tests use HeapPage directly to
 * measure storage layer performance.
 */

#include <gtest/gtest.h>
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <sstream>
#include <cstring>

using namespace scratchbird::core;
using namespace std::chrono;

class StoragePerformanceTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}

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
        std::cout << "║              HeapPage Performance Benchmarks                       ║\n";
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

// Benchmark: Sequential Insert Performance with different page sizes
TEST_F(StoragePerformanceTest, SequentialInsertBenchmark)
{
    print_benchmark_header();

    const std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536, 131072};
    const std::vector<size_t> tuple_sizes = {100, 500, 1000, 2000};
    const int NUM_PAGES = 50;

    for (uint32_t page_size : page_sizes)
    {
        std::cout << "\n=== Page Size: " << page_size << " bytes ===\n";

        for (size_t tuple_size : tuple_sizes)
        {
            // Create page buffers
            std::vector<std::vector<uint8_t>> page_buffers(NUM_PAGES, std::vector<uint8_t>(page_size, 0));
            std::vector<HeapPage> heap_pages;

            for (int p = 0; p < NUM_PAGES; p++)
            {
                heap_pages.emplace_back(page_buffers[p].data(), page_size);
                ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
            }

            std::vector<uint8_t> tuple_data(tuple_size, 0xAA);
            int total_inserted = 0;
            int current_page = 0;

            auto start = high_resolution_clock::now();

            // Insert tuples
            while (current_page < NUM_PAGES)
            {
                // Vary data to avoid any caching effects
                tuple_data[0] = total_inserted & 0xFF;
                tuple_data[1] = (total_inserted >> 8) & 0xFF;

                uint16_t item_id;
                Status status = heap_pages[current_page].insertTuple(
                    tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

                if (status == Status::OK)
                {
                    total_inserted++;
                }
                else
                {
                    current_page++;
                }
            }

            auto end = high_resolution_clock::now();
            duration<double> elapsed = end - start;

            BenchmarkResult result;
            result.operation = "Insert " + std::to_string(tuple_size) + "B tuples";
            result.count = total_inserted;
            result.duration_seconds = elapsed.count();
            result.rate_per_second = total_inserted / elapsed.count();
            result.bytes_processed = total_inserted * tuple_size;
            result.throughput_mb_per_second =
                result.bytes_processed / (1024.0 * 1024.0) / elapsed.count();

            print_result(result);
        }
    }
}

// Benchmark: Sequential Scan Performance
TEST_F(StoragePerformanceTest, SequentialScanBenchmark)
{
    std::cout << "\n\n=== Sequential Scan Benchmark ===\n";

    const std::vector<int> target_counts = {1000, 5000, 10000};
    const size_t TUPLE_SIZE = 200;
    const uint32_t PAGE_SIZE = 32768;
    const int MAX_PAGES = 200;

    for (int target_count : target_counts)
    {
        // Create page buffers
        std::vector<std::vector<uint8_t>> page_buffers(MAX_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
        std::vector<HeapPage> heap_pages;

        for (int p = 0; p < MAX_PAGES; p++)
        {
            heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
            ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
        }

        // Insert tuples
        std::vector<uint8_t> tuple_data(TUPLE_SIZE, 0xBB);
        int total_inserted = 0;
        int current_page = 0;

        while (total_inserted < target_count && current_page < MAX_PAGES)
        {
            tuple_data[0] = total_inserted & 0xFF;

            uint16_t item_id;
            Status status = heap_pages[current_page].insertTuple(
                tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

            if (status == Status::OK)
            {
                total_inserted++;
            }
            else
            {
                current_page++;
            }
        }

        // Benchmark scan
        const int NUM_SCANS = 10;
        auto start = high_resolution_clock::now();

        int total_scanned = 0;
        for (int scan = 0; scan < NUM_SCANS; scan++)
        {
            for (int p = 0; p <= current_page && p < MAX_PAGES; p++)
            {
                for (uint16_t slot = 0; slot < heap_pages[p].getItemCount(); slot++)
                {
                    const uint8_t *data;
                    uint32_t size;
                    Status status = heap_pages[p].getTuple(slot, &data, &size, nullptr);
                    if (status == Status::OK && data != nullptr)
                    {
                        total_scanned++;
                    }
                }
            }
        }

        auto end = high_resolution_clock::now();
        duration<double> elapsed = end - start;

        BenchmarkResult result;
        result.operation = "Scan " + std::to_string(total_inserted) + " tuples";
        result.count = total_scanned;
        result.duration_seconds = elapsed.count();
        result.rate_per_second = total_scanned / elapsed.count();
        result.bytes_processed = total_scanned * TUPLE_SIZE;
        result.throughput_mb_per_second =
            result.bytes_processed / (1024.0 * 1024.0) / elapsed.count();

        print_result(result);
    }
}

// Benchmark: Random Access Performance
TEST_F(StoragePerformanceTest, RandomAccessBenchmark)
{
    std::cout << "\n\n=== Random Access Benchmark ===\n";

    const uint32_t PAGE_SIZE = 16384;
    const int NUM_PAGES = 100;
    const size_t TUPLE_SIZE = 200;

    // Create page buffers
    std::vector<std::vector<uint8_t>> page_buffers(NUM_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
    std::vector<HeapPage> heap_pages;

    for (int p = 0; p < NUM_PAGES; p++)
    {
        heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }

    // Insert tuples and keep track of their IDs
    struct TupleLoc { int page; uint16_t slot; };
    std::vector<TupleLoc> tuple_locs;
    std::vector<uint8_t> tuple_data(TUPLE_SIZE, 0xCC);
    int current_page = 0;

    while (current_page < NUM_PAGES)
    {
        tuple_data[0] = tuple_locs.size() & 0xFF;
        tuple_data[1] = (tuple_locs.size() >> 8) & 0xFF;

        uint16_t item_id;
        Status status = heap_pages[current_page].insertTuple(
            tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

        if (status == Status::OK)
        {
            tuple_locs.push_back({current_page, item_id});
        }
        else
        {
            current_page++;
        }
    }

    ASSERT_GT(tuple_locs.size(), 1000) << "Should have inserted many tuples";

    // Random access benchmark
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, tuple_locs.size() - 1);

    const int NUM_ACCESSES = 10000;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < NUM_ACCESSES; i++)
    {
        int idx = dis(gen);
        const uint8_t *data;
        uint32_t size;
        Status status = heap_pages[tuple_locs[idx].page].getTuple(
            tuple_locs[idx].slot, &data, &size, nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    auto end = high_resolution_clock::now();
    duration<double> elapsed = end - start;

    BenchmarkResult result;
    result.operation = "Random Get";
    result.count = NUM_ACCESSES;
    result.duration_seconds = elapsed.count();
    result.rate_per_second = NUM_ACCESSES / elapsed.count();
    result.bytes_processed = 0;
    result.throughput_mb_per_second = 0;

    print_result(result);
}

// Benchmark: Mixed Workload
TEST_F(StoragePerformanceTest, MixedWorkloadBenchmark)
{
    std::cout << "\n\n=== Mixed Workload Benchmark ===\n";
    std::cout << "(70% reads, 30% inserts)\n";

    const uint32_t PAGE_SIZE = 16384;
    const int MAX_PAGES = 100;
    const size_t TUPLE_SIZE = 300;

    // Create page buffers
    std::vector<std::vector<uint8_t>> page_buffers(MAX_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
    std::vector<HeapPage> heap_pages;

    for (int p = 0; p < MAX_PAGES; p++)
    {
        heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }

    // Initial data
    struct TupleLoc { int page; uint16_t slot; };
    std::vector<TupleLoc> tuple_locs;
    std::vector<uint8_t> tuple_data(TUPLE_SIZE, 0xDD);
    int current_page = 0;

    // Insert initial tuples (fill first 50 pages)
    while (current_page < 50)
    {
        uint16_t item_id;
        Status status = heap_pages[current_page].insertTuple(
            tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

        if (status == Status::OK)
        {
            tuple_locs.push_back({current_page, item_id});
        }
        else
        {
            current_page++;
        }
    }

    ASSERT_GT(tuple_locs.size(), 100) << "Should have initial tuples";

    // Mixed operations
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> op_dis(1, 100);

    const int NUM_OPERATIONS = 20000;
    int read_count = 0, insert_count = 0;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < NUM_OPERATIONS && current_page < MAX_PAGES; i++)
    {
        int op = op_dis(gen);

        if (op <= 70)
        {
            // Read
            if (!tuple_locs.empty())
            {
                std::uniform_int_distribution<> idx_dis(0, tuple_locs.size() - 1);
                int idx = idx_dis(gen);
                const uint8_t *data;
                uint32_t size;
                heap_pages[tuple_locs[idx].page].getTuple(tuple_locs[idx].slot, &data, &size, nullptr);
                read_count++;
            }
        }
        else
        {
            // Insert
            uint16_t item_id;
            Status status = heap_pages[current_page].insertTuple(
                tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

            if (status == Status::OK)
            {
                tuple_locs.push_back({current_page, item_id});
                insert_count++;
            }
            else
            {
                current_page++;
                if (current_page < MAX_PAGES)
                {
                    status = heap_pages[current_page].insertTuple(
                        tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);
                    if (status == Status::OK)
                    {
                        tuple_locs.push_back({current_page, item_id});
                        insert_count++;
                    }
                }
            }
        }
    }

    auto end = high_resolution_clock::now();
    duration<double> elapsed = end - start;

    int total_ops = read_count + insert_count;
    std::cout << "\nResults:\n";
    std::cout << "  Total operations: " << total_ops << "\n";
    std::cout << "  Reads: " << read_count << " (" << (read_count * 100.0 / total_ops) << "%)\n";
    std::cout << "  Inserts: " << insert_count << " (" << (insert_count * 100.0 / total_ops) << "%)\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(3) << elapsed.count() << " seconds\n";
    std::cout << "  Operations/sec: " << std::fixed << std::setprecision(0)
              << total_ops / elapsed.count() << "\n";
}

// Benchmark: Page Fill Efficiency
TEST_F(StoragePerformanceTest, PageFillEfficiencyBenchmark)
{
    std::cout << "\n\n=== Page Fill Efficiency Benchmark ===\n";

    const std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536};
    const std::vector<size_t> tuple_sizes = {50, 100, 200, 500, 1000};

    for (uint32_t page_size : page_sizes)
    {
        std::cout << "\nPage Size: " << page_size << " bytes\n";

        for (size_t tuple_size : tuple_sizes)
        {
            // Create single page
            std::vector<uint8_t> page_buffer(page_size, 0);
            HeapPage heap_page(page_buffer.data(), page_size);
            ASSERT_EQ(heap_page.initialize(0, nullptr), Status::OK);

            // Calculate theoretical capacity (rough estimate)
            // Header overhead + item pointers + tuple headers
            size_t overhead_per_page = 64;  // Approximate page header
            size_t overhead_per_tuple = 8 + sizeof(TupleHeader);  // ItemPointer + TupleHeader
            size_t usable_space = page_size - overhead_per_page;
            size_t total_tuple_size = tuple_size + overhead_per_tuple;
            int theoretical_tuples = usable_space / total_tuple_size;

            // Fill page
            std::vector<uint8_t> tuple_data(tuple_size, 0xEE);
            int tuples_inserted = 0;

            for (int i = 0; i < theoretical_tuples * 2; i++)
            {
                uint16_t item_id;
                Status status = heap_page.insertTuple(
                    tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

                if (status != Status::OK) break;
                tuples_inserted++;
            }

            double efficiency = theoretical_tuples > 0
                ? (tuples_inserted * 100.0) / theoretical_tuples
                : 0;

            std::cout << "  Tuple size " << tuple_size << "B: " << tuples_inserted << " tuples/page "
                      << "(theoretical: " << theoretical_tuples << ", "
                      << "efficiency: " << std::fixed << std::setprecision(1) << efficiency
                      << "%)\n";
        }
    }
}

// Benchmark: Transaction Overhead Simulation
TEST_F(StoragePerformanceTest, TransactionOverheadBenchmark)
{
    std::cout << "\n\n=== Transaction Overhead Benchmark ===\n";

    const uint32_t PAGE_SIZE = 16384;
    const int NUM_PAGES = 50;
    const size_t TUPLE_SIZE = 200;
    const int NUM_OPERATIONS = 10000;

    // Create page buffers
    std::vector<std::vector<uint8_t>> page_buffers(NUM_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
    std::vector<HeapPage> heap_pages;

    for (int p = 0; p < NUM_PAGES; p++)
    {
        heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }

    std::vector<uint8_t> tuple_data(TUPLE_SIZE, 0xFF);
    int current_page = 0;

    // Benchmark 1: Simulated one transaction per operation
    auto start1 = high_resolution_clock::now();
    int inserted1 = 0;

    for (int i = 0; i < NUM_OPERATIONS && current_page < NUM_PAGES; i++)
    {
        // Simulate transaction overhead with some computation
        volatile int xid = i + 1;  // Simulate XID assignment
        (void)xid;

        uint16_t item_id;
        Status status = heap_pages[current_page].insertTuple(
            tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

        if (status == Status::OK)
        {
            inserted1++;
        }
        else
        {
            current_page++;
        }
    }

    auto end1 = high_resolution_clock::now();
    duration<double> elapsed1 = end1 - start1;

    // Reset for second benchmark
    for (int p = 0; p < NUM_PAGES; p++)
    {
        memset(page_buffers[p].data(), 0, PAGE_SIZE);
        heap_pages[p] = HeapPage(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }
    current_page = 0;

    // Benchmark 2: Simulated batched transactions
    const int BATCH_SIZE = 100;
    auto start2 = high_resolution_clock::now();
    int inserted2 = 0;

    for (int batch = 0; batch < NUM_OPERATIONS / BATCH_SIZE && current_page < NUM_PAGES; batch++)
    {
        // Simulate batch transaction overhead once per batch
        volatile int xid = batch + 1;
        (void)xid;

        for (int i = 0; i < BATCH_SIZE && current_page < NUM_PAGES; i++)
        {
            uint16_t item_id;
            Status status = heap_pages[current_page].insertTuple(
                tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

            if (status == Status::OK)
            {
                inserted2++;
            }
            else
            {
                current_page++;
            }
        }
    }

    auto end2 = high_resolution_clock::now();
    duration<double> elapsed2 = end2 - start2;

    std::cout << "\nResults:\n";
    std::cout << "  One operation per 'transaction':\n";
    std::cout << "    Inserted: " << inserted1 << " tuples\n";
    std::cout << "    Time: " << std::fixed << std::setprecision(3) << elapsed1.count() << " seconds\n";
    std::cout << "    Rate: " << std::fixed << std::setprecision(0)
              << inserted1 / elapsed1.count() << " ops/sec\n";

    std::cout << "\n  Batched operations (" << BATCH_SIZE << " ops/batch):\n";
    std::cout << "    Inserted: " << inserted2 << " tuples\n";
    std::cout << "    Time: " << std::fixed << std::setprecision(3) << elapsed2.count() << " seconds\n";
    std::cout << "    Rate: " << std::fixed << std::setprecision(0)
              << inserted2 / elapsed2.count() << " ops/sec\n";

    if (elapsed2.count() > 0)
    {
        std::cout << "\n  Speedup: " << std::fixed << std::setprecision(2)
                  << elapsed1.count() / elapsed2.count() << "x\n";
    }
}
