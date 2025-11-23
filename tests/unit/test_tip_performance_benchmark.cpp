// ScratchBird TIP Performance Benchmark
// Measures TIP-based visibility performance vs old snapshot-based approach
// Validates that TIP lookups are O(1) fast

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/tid.h"
#include <chrono>
#include <memory>
#include <vector>
#include <iostream>

using namespace scratchbird::core;
using namespace std::chrono;

class TIPPerformanceBenchmark : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_ = std::make_unique<Database>("test_tip_perf.db", 100 * 1024 * 1024); // 100MB
        ASSERT_EQ(db_->open(), Status::OK);
        tm_ = db_->transaction_manager();
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
        }
    }

    std::unique_ptr<Database> db_;
    TransactionManager *tm_;
};

// =============================================================================
// TIP Lookup Performance Tests
// =============================================================================

TEST_F(TIPPerformanceBenchmark, TIPLookupSpeed)
{
    // Measure raw TIP lookup performance
    const int NUM_LOOKUPS = 100000;

    // Create transactions to populate TIP
    std::vector<uint64_t> xids;
    for (int i = 0; i < 1000; i++)
    {
        uint64_t xid = tm_->beginTransaction();
        xids.push_back(xid);
        if (i % 2 == 0)
        {
            tm_->commit(xid);
        }
        else
        {
            tm_->rollback(xid);
        }
    }

    // Benchmark TIP lookups
    uint64_t reader_xid = tm_->beginTransaction();
    auto start = high_resolution_clock::now();

    int visible_count = 0;
    for (int i = 0; i < NUM_LOOKUPS; i++)
    {
        uint64_t test_xid = xids[i % xids.size()];
        if (tm_->isVersionVisible(test_xid, reader_xid))
        {
            visible_count++;
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    tm_->commit(reader_xid);

    // Report results
    double avg_lookup_ns = (duration.count() * 1000.0) / NUM_LOOKUPS;

    std::cout << "\n=== TIP Lookup Performance ===" << std::endl;
    std::cout << "Total lookups: " << NUM_LOOKUPS << std::endl;
    std::cout << "Total time: " << duration.count() << " µs" << std::endl;
    std::cout << "Average per lookup: " << avg_lookup_ns << " ns" << std::endl;
    std::cout << "Lookups per second: " << (NUM_LOOKUPS * 1000000.0 / duration.count()) << std::endl;
    std::cout << "Visible transactions: " << visible_count << std::endl;

    // TIP lookups should be very fast (< 100ns per lookup)
    EXPECT_LT(avg_lookup_ns, 100.0) << "TIP lookup too slow!";
}

TEST_F(TIPPerformanceBenchmark, BTreeSearchWithTIPVisibility)
{
    // Measure B-tree search performance with TIP-based visibility

    // Create B-tree
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t root_page = 0;
    BTree::create(db_.get(), index_uuid, &root_page);
    auto btree = BTree::open(db_.get(), index_uuid, root_page);

    // Insert 10000 entries
    uint64_t writer_xid = tm_->beginTransaction();
    for (int i = 0; i < 10000; i++)
    {
        std::vector<uint8_t> key(4);
        key[0] = (i >> 24) & 0xFF;
        key[1] = (i >> 16) & 0xFF;
        key[2] = (i >> 8) & 0xFF;
        key[3] = i & 0xFF;

        TID tid = createTID(1, i, 1);
        btree->insert(key, tid, writer_xid);
    }
    tm_->commit(writer_xid);

    // Benchmark searches
    const int NUM_SEARCHES = 1000;
    uint64_t reader_xid = tm_->beginTransaction();

    auto start = high_resolution_clock::now();

    int total_results = 0;
    for (int i = 0; i < NUM_SEARCHES; i++)
    {
        int search_key = i % 10000;
        std::vector<uint8_t> key(4);
        key[0] = (search_key >> 24) & 0xFF;
        key[1] = (search_key >> 16) & 0xFF;
        key[2] = (search_key >> 8) & 0xFF;
        key[3] = search_key & 0xFF;

        std::vector<TID> results;
        btree->search(key, reader_xid, &results);
        total_results += results.size();
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    tm_->commit(reader_xid);

    // Report results
    double avg_search_us = duration.count() / (double)NUM_SEARCHES;

    std::cout << "\n=== B-Tree Search with TIP Performance ===" << std::endl;
    std::cout << "Total searches: " << NUM_SEARCHES << std::endl;
    std::cout << "Total time: " << duration.count() << " µs" << std::endl;
    std::cout << "Average per search: " << avg_search_us << " µs" << std::endl;
    std::cout << "Searches per second: " << (NUM_SEARCHES * 1000000.0 / duration.count()) << std::endl;
    std::cout << "Total results found: " << total_results << std::endl;

    // B-tree searches with TIP should be fast (< 100µs per search)
    EXPECT_LT(avg_search_us, 100.0) << "B-tree search with TIP too slow!";
    EXPECT_EQ(total_results, NUM_SEARCHES); // All should be found
}

TEST_F(TIPPerformanceBenchmark, ConcurrentTIPAccess)
{
    // Test TIP performance under concurrent access

    const int NUM_THREADS = 4;
    const int LOOKUPS_PER_THREAD = 10000;

    // Create test transactions
    std::vector<uint64_t> xids;
    for (int i = 0; i < 100; i++)
    {
        uint64_t xid = tm_->beginTransaction();
        xids.push_back(xid);
        tm_->commit(xid);
    }

    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    std::atomic<int> total_visible{0};

    for (int t = 0; t < NUM_THREADS; t++)
    {
        threads.emplace_back([&]()
        {
            uint64_t reader_xid = tm_->beginTransaction();
            int visible_count = 0;

            for (int i = 0; i < LOOKUPS_PER_THREAD; i++)
            {
                uint64_t test_xid = xids[i % xids.size()];
                if (tm_->isVersionVisible(test_xid, reader_xid))
                {
                    visible_count++;
                }
            }

            total_visible += visible_count;
            tm_->commit(reader_xid);
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    int total_lookups = NUM_THREADS * LOOKUPS_PER_THREAD;
    double avg_lookup_ns = (duration.count() * 1000.0) / total_lookups;

    std::cout << "\n=== Concurrent TIP Access Performance ===" << std::endl;
    std::cout << "Threads: " << NUM_THREADS << std::endl;
    std::cout << "Total lookups: " << total_lookups << std::endl;
    std::cout << "Total time: " << duration.count() << " µs" << std::endl;
    std::cout << "Average per lookup: " << avg_lookup_ns << " ns" << std::endl;
    std::cout << "Throughput: " << (total_lookups * 1000000.0 / duration.count()) << " lookups/sec" << std::endl;
    std::cout << "Total visible: " << total_visible.load() << std::endl;

    // Concurrent TIP access should still be fast
    EXPECT_LT(avg_lookup_ns, 200.0) << "Concurrent TIP lookup too slow!";
}

TEST_F(TIPPerformanceBenchmark, VisibilityCheckScalability)
{
    // Test that TIP performance remains O(1) as transaction count grows

    std::vector<std::pair<int, double>> results; // (num_transactions, avg_lookup_time)

    for (int num_txns : {100, 1000, 10000, 50000})
    {
        // Create many transactions
        std::vector<uint64_t> xids;
        for (int i = 0; i < num_txns; i++)
        {
            uint64_t xid = tm_->beginTransaction();
            xids.push_back(xid);
            if (i % 3 == 0)
            {
                tm_->commit(xid);
            }
            else
            {
                tm_->rollback(xid);
            }
        }

        // Measure lookup time
        const int NUM_LOOKUPS = 10000;
        uint64_t reader_xid = tm_->beginTransaction();
        auto start = high_resolution_clock::now();

        for (int i = 0; i < NUM_LOOKUPS; i++)
        {
            uint64_t test_xid = xids[i % xids.size()];
            tm_->isVersionVisible(test_xid, reader_xid);
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<nanoseconds>(end - start);

        tm_->commit(reader_xid);

        double avg_ns = duration.count() / (double)NUM_LOOKUPS;
        results.push_back({num_txns, avg_ns});
    }

    // Print scalability results
    std::cout << "\n=== TIP Scalability Test ===" << std::endl;
    std::cout << "Num Transactions | Avg Lookup Time (ns)" << std::endl;
    std::cout << "-----------------+----------------------" << std::endl;

    for (const auto &[num_txns, avg_ns] : results)
    {
        std::cout << std::setw(16) << num_txns << " | " << avg_ns << std::endl;
    }

    // TIP should be O(1) - lookup time shouldn't grow significantly
    double first_time = results.front().second;
    double last_time = results.back().second;
    double growth_factor = last_time / first_time;

    std::cout << "\nGrowth factor (500x more txns): " << growth_factor << "x" << std::endl;

    // TIP is O(1), so growth should be minimal (< 3x even with cache effects)
    EXPECT_LT(growth_factor, 3.0) << "TIP lookup time grew too much - not O(1)!";
}

// Run all benchmarks
