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
 * Helgrind Race Condition Tests
 *
 * Tests using Valgrind's Helgrind tool to detect:
 * - Lock ordering violations
 * - Data races
 * - Potential deadlocks
 * - Improper synchronization
 *
 * Helgrind vs ThreadSanitizer:
 * - Helgrind: More thorough lock order checking, slower
 * - TSAN: Faster, better for atomics, less lock order validation
 * - Use both tools for comprehensive validation
 *
 * Test Categories:
 * 1. Lock ordering validation (mutex → array_lock ordering)
 * 2. Atomic operations validation (verify proper synchronization)
 * 3. Read-write lock validation (shared vs exclusive access)
 * 4. Cache access patterns (LRU cache eviction under contention)
 *
 * Execution:
 *   From build/: valgrind --tool=helgrind ./tests/helgrind_races
 *   Expected: No errors, proper lock ordering detected
 *
 * Note: Helgrind tests run slower than TSAN (~10-100x slower)
 */

#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>
#include "test_helpers.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

class HelgrindRaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = uniqueTestDbPath("test_helgrind_races");
        std::remove(test_db_path_.c_str());

        ErrorContext ctx;
        Status status = Database::create(test_db_path_.c_str(), 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        status = db_->open(test_db_path_.c_str(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        pool_ = db_->buffer_pool();
        ASSERT_NE(pool_, nullptr);

        page_mgr_ = db_->page_manager();
        ASSERT_NE(page_mgr_, nullptr);

        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(txn_mgr_, nullptr);

        // Pre-allocate pages
        for (int i = 0; i < 50; ++i) {
            uint32_t page_id = 0;
            status = page_mgr_->allocatePage(page_id, &ctx);
            ASSERT_EQ(status, Status::OK);
            allocated_pages_.push_back(page_id);
        }
    }

    void TearDown() override {
        if (db_) {
            db_->close();
        }
        std::remove(test_db_path_.c_str());
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    BufferPool* pool_;
    PageManager* page_mgr_;
    TransactionManager* txn_mgr_;
    std::vector<uint32_t> allocated_pages_;
};

/**
 * Test 1: Lock Ordering - Transaction Manager
 *
 * Validates that mutex_ is always acquired before ProcArray::array_lock
 * This is the documented lock hierarchy from CRITICAL-3 fix
 */
TEST_F(HelgrindRaceTest, TransactionManagerLockOrdering) {
    const int NUM_THREADS = 10;
    const int ITERATIONS = 20;

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    // Mix of operations that acquire locks in documented order
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                if (i % 3 == 0) {
                    // beginTransaction: acquires mutex_ first
                    std::unique_ptr<ConnectionContext> conn;
                    Status s = db_->connect(conn, &ctx);
                    if (s == Status::OK) {
                        conn->commit(&ctx);
                    }
                } else if (i % 3 == 1) {
                    // MGA: getSnapshot removed - use getCurrentXid instead
                    uint64_t current_xid = txn_mgr_->getCurrentXid();
                    (void)current_xid; // Suppress unused variable warning
                } else {
                    // getTransactionState: acquires mutex_ for cache access
                    TransactionState state;
                    uint64_t xid = 1 + (t * ITERATIONS + i);
                    txn_mgr_->getTransactionState(xid, state, &ctx);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Helgrind should detect no lock ordering violations
    EXPECT_EQ(errors.load(), 0);

    std::cout << "Lock ordering test completed: " << (NUM_THREADS * ITERATIONS) << " operations\n";
}

/**
 * Test 2: Atomic Operations - Buffer Pool Frame Metadata
 *
 * Validates atomic pin_count and usage_count operations
 * Helgrind should see proper synchronization via atomics
 */
TEST_F(HelgrindRaceTest, AtomicBufferPoolOperations) {
    const int NUM_THREADS = 10;
    const int ITERATIONS = 50;

    std::atomic<int> successful_ops{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                uint32_t page_id = allocated_pages_[(t + i) % allocated_pages_.size()];
                void* buffer = nullptr;

                Status s = pool_->pinPage(page_id, &buffer, &ctx);
                if (s == Status::OK) {
                    successful_ops.fetch_add(1);
                    pool_->unpinPage(page_id, false, &ctx);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(successful_ops.load(), 0);

    std::cout << "Atomic operations test: " << successful_ops.load() << " operations\n";
}

/**
 * Test 3: Read-Write Lock - ProcArray Array Lock
 *
 * Validates proper use of pthread_rwlock in ProcArray
 * Multiple readers can hold lock simultaneously, but writers are exclusive
 */
TEST_F(HelgrindRaceTest, ReadWriteLockValidation) {
    const int NUM_READER_THREADS = 15;
    const int NUM_WRITER_THREADS = 5;
    const int ITERATIONS = 20;

    std::atomic<int> reader_ops{0};
    std::atomic<int> writer_ops{0};
    std::vector<std::thread> threads;

    // Reader threads - take snapshots (read lock on ProcArray)
    for (int t = 0; t < NUM_READER_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                // MGA: getSnapshot removed - use getCurrentXid instead
                uint64_t current_xid = txn_mgr_->getCurrentXid();
                (void)current_xid; // Suppress unused variable warning
                reader_ops.fetch_add(1);
                std::this_thread::yield();
            }
        });
    }

    // Writer threads - commit transactions (write lock on ProcArray)
    for (int t = 0; t < NUM_WRITER_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                std::unique_ptr<ConnectionContext> conn;
                Status s = db_->connect(conn, &ctx);
                if (s == Status::OK) {
                    s = conn->commit(&ctx);
                    if (s == Status::OK) {
                        writer_ops.fetch_add(1);
                    }
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(reader_ops.load(), 0);
    EXPECT_GT(writer_ops.load(), 0);

    std::cout << "Read-write lock test: " << reader_ops.load() << " reads, "
              << writer_ops.load() << " writes\n";
}

/**
 * Test 4: Cache Access Patterns - Transaction Cache
 *
 * Validates proper synchronization of LRU cache access
 * Tests CRITICAL-2 fix (removed const from cache-modifying methods)
 */
TEST_F(HelgrindRaceTest, CacheAccessSynchronization) {
    const int NUM_THREADS = 10;
    const int ITERATIONS = 30;

    std::atomic<int> cache_hits{0};
    std::vector<std::thread> threads;

    // Create some transactions to cache
    std::vector<std::unique_ptr<ConnectionContext>> initial_conns;
    std::vector<uint64_t> cached_xids;

    ErrorContext ctx;
    for (int i = 0; i < 10; ++i) {
        std::unique_ptr<ConnectionContext> conn;
        Status s = db_->connect(conn, &ctx);
        if (s == Status::OK) {
            cached_xids.push_back(conn->getCurrentXid());
            initial_conns.push_back(std::move(conn));
        }
    }

    // Concurrent cache queries
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext thread_ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                if (!cached_xids.empty()) {
                    uint64_t xid = cached_xids[i % cached_xids.size()];
                    TransactionState state;

                    Status s = txn_mgr_->getTransactionState(xid, state, &thread_ctx);
                    if (s == Status::OK) {
                        cache_hits.fetch_add(1);
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(cache_hits.load(), 0);

    std::cout << "Cache access test: " << cache_hits.load() << " successful queries\n";
}

/**
 * Test 5: Mixed Workload - All Operations
 *
 * Comprehensive test mixing all operation types
 * Validates no races across entire system
 */
TEST_F(HelgrindRaceTest, MixedWorkloadNoRaces) {
    const int NUM_THREADS = 12;
    const int ITERATIONS = 25;

    std::atomic<int> total_ops{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                // Mix of different operations
                switch (i % 4) {
                    case 0: {
                        // Transaction commit
                        std::unique_ptr<ConnectionContext> conn;
                        if (db_->connect(conn, &ctx) == Status::OK) {
                            conn->commit(&ctx);
                            total_ops.fetch_add(1);
                        }
                        break;
                    }
                    case 1: {
                        // MGA: getSnapshot removed - use getCurrentXid instead
                        uint64_t current_xid = txn_mgr_->getCurrentXid();
                        (void)current_xid; // Suppress unused variable warning
                        total_ops.fetch_add(1);
                        break;
                    }
                    case 2: {
                        // Buffer pool access
                        uint32_t page_id = allocated_pages_[i % allocated_pages_.size()];
                        void* buffer = nullptr;
                        if (pool_->pinPage(page_id, &buffer, &ctx) == Status::OK) {
                            pool_->unpinPage(page_id, false, &ctx);
                            total_ops.fetch_add(1);
                        }
                        break;
                    }
                    case 3: {
                        // Transaction state query
                        TransactionState state;
                        uint64_t xid = 1 + (t * ITERATIONS + i);
                        txn_mgr_->getTransactionState(xid, state, &ctx);
                        total_ops.fetch_add(1);
                        break;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(total_ops.load(), 0);

    std::cout << "Mixed workload test: " << total_ops.load() << " total operations\n";
}

// Note: main() is provided by GTest::gtest_main (linked in CMakeLists.txt)
// Run with: valgrind --tool=helgrind ./tests/helgrind_races
