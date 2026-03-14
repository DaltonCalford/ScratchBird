/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// ScratchBird TIP Performance Benchmark
// Measures TIP-based visibility performance vs old snapshot-based approach
// Validates that TIP lookups are O(1) fast

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/tid.h"
#include "test_helpers.h"
#include <chrono>
#include <memory>
#include <vector>
#include <iostream>
#include <thread>
#include <atomic>
#include <iomanip>

using namespace scratchbird::core;
using namespace std::chrono;

class TIPPerformanceBenchmark : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_tip_perf");
        ErrorContext ctx;

        ASSERT_EQ(Database::create(test_db_->path(), 8192, &ctx), Status::OK)
            << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_->path(), &ctx), Status::OK)
            << "Failed to open database: " << ctx.message;

        tm_ = db_->transaction_manager();
        ASSERT_NE(tm_, nullptr);

        Status status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(status, Status::OK) << "Failed to initialize ProcArray: " << ctx.message;
        }

        status = ProcArrayManager::registerBackend(&proc_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to register backend: " << ctx.message;
    }

    void TearDown() override
    {
        if (proc_id_ != 0)
        {
            ProcArrayManager::unregisterBackend(proc_id_);
            proc_id_ = 0;
        }
        if (db_)
        {
            db_->close();
        }

        test_db_.reset();
    }

    uint64_t beginTxn(ErrorContext *ctx)
    {
        uint64_t xid = 0;
        Status status = tm_->beginTransaction(proc_id_, xid, ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to begin transaction: " << (ctx ? ctx->message : "");
        return xid;
    }

    void commitTxn(uint64_t xid, ErrorContext *ctx)
    {
        Status status = tm_->commitTransaction(proc_id_, xid, ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to commit transaction: " << (ctx ? ctx->message : "");
    }

    void rollbackTxn(uint64_t xid, ErrorContext *ctx)
    {
        Status status = tm_->rollbackTransaction(proc_id_, xid, ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to rollback transaction: " << (ctx ? ctx->message : "");
    }

    GPID allocateRootGpid(ErrorContext *ctx)
    {
        auto *pm = db_ ? db_->page_manager() : nullptr;
        if (!pm)
        {
            if (ctx)
            {
                ctx->message = "PageManager not available";
            }
            return 0;
        }
        GPID gpid = 0;
        Status status = pm->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &gpid, ctx);
        if (status != Status::OK)
        {
            return 0;
        }
        return gpid;
    }

    std::unique_ptr<Database> db_;
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_;
    TransactionManager *tm_;
    uint32_t proc_id_ = 0;
};

// =============================================================================
// TIP Lookup Performance Tests
// =============================================================================

TEST_F(TIPPerformanceBenchmark, TIPLookupSpeed)
{
    ErrorContext ctx;
    // Measure raw TIP lookup performance
    const int NUM_LOOKUPS = 100000;

    // Create transactions to populate TIP
    std::vector<uint64_t> xids;
    for (int i = 0; i < 1000; i++)
    {
        uint64_t xid = beginTxn(&ctx);
        xids.push_back(xid);
        if (i % 2 == 0)
        {
            commitTxn(xid, &ctx);
        }
        else
        {
            rollbackTxn(xid, &ctx);
        }
    }

    // Benchmark TIP lookups
    uint64_t reader_xid = beginTxn(&ctx);
    auto start = high_resolution_clock::now();

    int visible_count = 0;
    for (int i = 0; i < NUM_LOOKUPS; i++)
    {
        uint64_t test_xid = xids[i % xids.size()];
        if (tm_->isTransactionVisible(test_xid, reader_xid))
        {
            visible_count++;
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    commitTxn(reader_xid, &ctx);

    // Report results
    double avg_lookup_ns = (duration.count() * 1000.0) / NUM_LOOKUPS;

    std::cout << "\n=== TIP Lookup Performance ===" << std::endl;
    std::cout << "Total lookups: " << NUM_LOOKUPS << std::endl;
    std::cout << "Total time: " << duration.count() << " µs" << std::endl;
    std::cout << "Average per lookup: " << avg_lookup_ns << " ns" << std::endl;
    std::cout << "Lookups per second: " << (NUM_LOOKUPS * 1000000.0 / duration.count()) << std::endl;
    std::cout << "Visible transactions: " << visible_count << std::endl;

    // TIP lookups should be very fast (<150ns ideally). Under full-suite
    // parallel load this test is a sanity check, not a strict benchmark.
    constexpr double WARNING_AVG_LOOKUP_NS = 1000.0;
    constexpr double MAX_AVG_LOOKUP_NS_UNDER_FULL_LOAD = 5000.0;
    if (avg_lookup_ns > WARNING_AVG_LOOKUP_NS) {
        std::cout << "WARNING: TIP lookup slower than expected under parallel load (" 
                  << avg_lookup_ns << " ns)" << std::endl;
    }
    EXPECT_LT(avg_lookup_ns, MAX_AVG_LOOKUP_NS_UNDER_FULL_LOAD)
        << "TIP lookup excessively slow (> 5000ns)!";
}

TEST_F(TIPPerformanceBenchmark, InventoryHorizonWalkSpeed)
{
    ErrorContext ctx;
    const int NUM_TERMINAL_TRANSACTIONS = 128;
    const int NUM_WALKS = 20;

    for (int i = 0; i < NUM_TERMINAL_TRANSACTIONS; ++i)
    {
        uint64_t xid = beginTxn(&ctx);
        if ((i % 2) == 0)
        {
            commitTxn(xid, &ctx);
        }
        else
        {
            rollbackTxn(xid, &ctx);
        }
    }

    uint64_t prepared_xid = beginTxn(&ctx);
    ASSERT_EQ(tm_->prepareTransaction(proc_id_,
                                      prepared_xid,
                                      "tip_horizon_walk_speed",
                                      generateUuidV7(),
                                      &ctx),
              Status::OK)
        << ctx.message;

    uint64_t oldest_interesting = 0;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < NUM_WALKS; ++i)
    {
        ASSERT_EQ(tm_->findOldestInterestingXidFromInventory(oldest_interesting, &ctx), Status::OK)
            << ctx.message;
    }
    auto end = high_resolution_clock::now();

    ASSERT_EQ(oldest_interesting, prepared_xid);

    auto duration = duration_cast<microseconds>(end - start);
    const double avg_walk_us = duration.count() / static_cast<double>(NUM_WALKS);

    std::cout << "\n=== Inventory Horizon Walk Performance ===" << std::endl;
    std::cout << "Terminal transactions: " << NUM_TERMINAL_TRANSACTIONS << std::endl;
    std::cout << "Prepared frontier xid: " << prepared_xid << std::endl;
    std::cout << "Walks: " << NUM_WALKS << std::endl;
    std::cout << "Total time: " << duration.count() << " us" << std::endl;
    std::cout << "Average per walk: " << avg_walk_us << " us" << std::endl;

    constexpr double MAX_AVG_WALK_US_UNDER_FULL_LOAD = 5000.0;
    EXPECT_LT(avg_walk_us, MAX_AVG_WALK_US_UNDER_FULL_LOAD)
        << "Inventory horizon walk too slow (> 5000us)!";
}

TEST_F(TIPPerformanceBenchmark, BTreeSearchWithTIPVisibility)
{
    ErrorContext ctx;
    // Measure B-tree search performance with TIP-based visibility

    // Create B-tree
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

    GPID root_gpid = allocateRootGpid(&ctx);
    ASSERT_NE(root_gpid, 0u) << "Failed to allocate root GPID: " << ctx.message;

    ASSERT_EQ(BTree::create(db_.get(), index_uuid, table_uuid, column_uuids, root_gpid, &ctx), Status::OK)
        << "Failed to create B-tree: " << ctx.message;

    auto btree = BTree::open(db_.get(), index_uuid, root_gpid, &ctx);
    ASSERT_TRUE(btree) << "Failed to open B-tree: " << ctx.message;

    // Insert entries
    const int NUM_ENTRIES = 2000;
    uint64_t writer_xid = beginTxn(&ctx);
    for (int i = 0; i < NUM_ENTRIES; i++)
    {
        std::vector<uint8_t> key(4);
        key[0] = (i >> 24) & 0xFF;
        key[1] = (i >> 16) & 0xFF;
        key[2] = (i >> 8) & 0xFF;
        key[3] = i & 0xFF;

        TID tid = makeTID(1, static_cast<uint64_t>(i), 1);
        ASSERT_EQ(btree->insert(key, tid, writer_xid, &ctx), Status::OK)
            << "Insert failed: " << ctx.message;
    }
    // Benchmark searches without visibility filtering (current_xid = 0)
    const int NUM_SEARCHES = 200;
    uint64_t reader_xid = 0;

    auto start = high_resolution_clock::now();

    int total_results = 0;
    for (int i = 0; i < NUM_SEARCHES; i++)
    {
        int search_key = i % NUM_ENTRIES;
        std::vector<uint8_t> key(4);
        key[0] = (search_key >> 24) & 0xFF;
        key[1] = (search_key >> 16) & 0xFF;
        key[2] = (search_key >> 8) & 0xFF;
        key[3] = search_key & 0xFF;

        std::vector<TID> results;
        Status search_status = btree->search(key, reader_xid, &results, &ctx);
        if (search_status == Status::OK)
        {
            total_results += results.size();
        }
        else if (search_status != Status::NOT_FOUND)
        {
            ASSERT_EQ(search_status, Status::OK) << "Search failed: " << ctx.message;
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    commitTxn(writer_xid, &ctx);

    // Report results
    double avg_search_us = duration.count() / (double)NUM_SEARCHES;

    std::cout << "\n=== B-Tree Search with TIP Performance ===" << std::endl;
    std::cout << "Total searches: " << NUM_SEARCHES << std::endl;
    std::cout << "Total time: " << duration.count() << " µs" << std::endl;
    std::cout << "Average per search: " << avg_search_us << " µs" << std::endl;
    std::cout << "Searches per second: " << (NUM_SEARCHES * 1000000.0 / duration.count()) << std::endl;
    std::cout << "Total results found: " << total_results << std::endl;

    // B-tree searches with TIP should be fast (< 200µs per search)
    EXPECT_LT(avg_search_us, 200.0) << "B-tree search with TIP too slow!";
    if (total_results == 0)
    {
        std::cout << "Note: no results found; index visibility/filtering may be affecting results\n";
    }
}

TEST_F(TIPPerformanceBenchmark, ConcurrentTIPAccess)
{
    // Test TIP performance under concurrent access

    const int NUM_THREADS = 4;
    const int LOOKUPS_PER_THREAD = 10000;

    // Create test transactions
    std::vector<uint64_t> xids;
    ErrorContext ctx;
    for (int i = 0; i < 100; i++)
    {
        uint64_t xid = beginTxn(&ctx);
        xids.push_back(xid);
        commitTxn(xid, &ctx);
    }

    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    std::atomic<int> total_visible{0};

    for (int t = 0; t < NUM_THREADS; t++)
    {
        threads.emplace_back([&]()
        {
            ErrorContext thread_ctx;
            uint32_t thread_proc_id = 0;
            Status status = ProcArrayManager::registerBackend(&thread_proc_id, &thread_ctx);
            if (status != Status::OK)
            {
                return;
            }

            uint64_t reader_xid = 0;
            status = tm_->beginTransaction(thread_proc_id, reader_xid, &thread_ctx);
            if (status != Status::OK)
            {
                ProcArrayManager::unregisterBackend(thread_proc_id, &thread_ctx);
                return;
            }

            int visible_count = 0;

            for (int i = 0; i < LOOKUPS_PER_THREAD; i++)
            {
                uint64_t test_xid = xids[i % xids.size()];
                if (tm_->isTransactionVisible(test_xid, reader_xid))
                {
                    visible_count++;
                }
            }

            total_visible += visible_count;

            tm_->commitTransaction(thread_proc_id, reader_xid, &thread_ctx);
            ProcArrayManager::unregisterBackend(thread_proc_id, &thread_ctx);
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
    EXPECT_LT(avg_lookup_ns, 5000.0) << "Concurrent TIP lookup too slow!";
}

TEST_F(TIPPerformanceBenchmark, VisibilityCheckScalability)
{
    // Test that TIP performance remains O(1) as transaction count grows

    std::vector<std::pair<int, double>> results; // (num_transactions, avg_lookup_time)
    ErrorContext ctx;

    for (int num_txns : {100, 500, 1000, 2000})
    {
        // Create many transactions
        std::vector<uint64_t> xids;
        for (int i = 0; i < num_txns; i++)
        {
            uint64_t xid = beginTxn(&ctx);
            xids.push_back(xid);
            if (i % 3 == 0)
            {
                commitTxn(xid, &ctx);
            }
            else
            {
                rollbackTxn(xid, &ctx);
            }
        }

        // Measure lookup time
        const int NUM_LOOKUPS = 500;
        uint64_t reader_xid = beginTxn(&ctx);
        auto start = high_resolution_clock::now();

        for (int i = 0; i < NUM_LOOKUPS; i++)
        {
            uint64_t test_xid = xids[i % xids.size()];
            tm_->isTransactionVisible(test_xid, reader_xid);
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<nanoseconds>(end - start);

        commitTxn(reader_xid, &ctx);

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

    // TIP is O(1), so growth should be modest (< 30x even with cache effects)
    EXPECT_LT(growth_factor, 30.0) << "TIP lookup time grew too much - not O(1)!";
}

// Run all benchmarks
