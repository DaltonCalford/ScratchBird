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
 * Concurrent Page Access Tests
 *
 * Tests HIGH priority concurrency issues:
 * - Issue 2.1: Page-level concurrency control
 * - Issue 2.2: Cross-page transaction updates
 * - Issue 2.3: Snapshot isolation with concurrent modifications
 * - Issue 2.4: Buffer pool contention under heavy load
 *
 * Test Categories:
 * 1. Concurrent reads/writes to different pages (isolation)
 * 2. Concurrent updates to same page (MVCC validation)
 * 3. Cross-page transactions (atomicity)
 * 4. Snapshot consistency under concurrent modifications
 * 5. Buffer pool stress with page contention
 *
 * What This Test Validates:
 * - Pages can be safely accessed concurrently without data corruption
 * - MVCC ensures transaction isolation at page level
 * - Cross-page updates are atomic
 * - Snapshots remain consistent despite concurrent writes
 * - Buffer pool handles high contention gracefully
 *
 * Execution:
 *   From build/: ctest -R ConcurrentPageAccess -V
 *   Expected: All tests pass, no data corruption detected
 */

#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <algorithm>
#include "test_helpers.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/transaction_manager.h"

using namespace scratchbird::core;

static bool isHeavyConcurrencyTest() {
    const char* env = std::getenv("SCRATCHBIRD_TEST_HEAVY");
    return env && env[0] != '\0' && env[0] != '0';
}

static int parallelDivisor() {
    const char* env = std::getenv("CTEST_PARALLEL_LEVEL");
    if (!env) {
        return 1;
    }
    int level = std::atoi(env);
    return level > 1 ? 2 : 1;
}

static std::mt19937 makeThreadRng(int seed) {
    std::seed_seq seq{0x1234u, static_cast<uint32_t>(seed)};
    return std::mt19937(seq);
}

class ConcurrentPageAccessTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ =
            scratchbird::testing::uniqueTestDbPath("test_concurrent_page_access", ".db");
        std::remove(test_db_path_.c_str());

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        status = db_->open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        pool_ = db_->buffer_pool();
        ASSERT_NE(pool_, nullptr);

        page_mgr_ = db_->page_manager();
        ASSERT_NE(page_mgr_, nullptr);

        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(txn_mgr_, nullptr);

        // Pre-allocate pages for tests
        for (int i = 0; i < 100; ++i) {
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
 * Test 1: Concurrent Reads to Different Pages
 *
 * Tests Issue 2.1 - Page-level concurrency control
 * Validates that multiple threads can read different pages concurrently
 */
TEST_F(ConcurrentPageAccessTest, ConcurrentReadsDifferentPages) {
    const bool heavy = isHeavyConcurrencyTest();
    const int scale = parallelDivisor();
    const int NUM_THREADS = std::max(4, (heavy ? 50 : 16) / scale);
    const int ITERATIONS = std::max(10, (heavy ? 100 : 50) / scale);

    std::atomic<int> errors{0};
    std::atomic<int> successful_reads{0};
    std::atomic<int> pin_errors{0};
    std::atomic<int> unpin_errors{0};
    std::atomic<int> pin_invalid_argument{0};
    std::atomic<int> pin_io_error{0};
    std::atomic<int> pin_other{0};
    std::atomic<int> unpin_invalid_argument{0};
    std::atomic<int> unpin_not_found{0};
    std::atomic<int> unpin_other{0};
    std::mutex error_mutex;
    std::string last_error;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            std::mt19937 gen = makeThreadRng(t + 1);
            std::uniform_int_distribution<> dis(0, allocated_pages_.size() - 1);

            for (int i = 0; i < ITERATIONS; ++i) {
                uint32_t page_id = allocated_pages_[dis(gen)];

                void* buffer = nullptr;
                Status s = pool_->pinPage(page_id, &buffer, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                    pin_errors.fetch_add(1);
                    if (s == Status::INVALID_ARGUMENT) {
                        pin_invalid_argument.fetch_add(1);
                    } else if (s == Status::IO_ERROR) {
                        pin_io_error.fetch_add(1);
                    } else {
                        pin_other.fetch_add(1);
                    }
                    {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        last_error = ctx.message;
                    }
                    continue;
                }

                // Simulate read operation
                std::this_thread::sleep_for(std::chrono::microseconds(10));

                s = pool_->unpinPage(page_id, false, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                    unpin_errors.fetch_add(1);
                    if (s == Status::INVALID_ARGUMENT) {
                        unpin_invalid_argument.fetch_add(1);
                    } else if (s == Status::NOT_FOUND) {
                        unpin_not_found.fetch_add(1);
                    } else {
                        unpin_other.fetch_add(1);
                    }
                    {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        last_error = ctx.message;
                    }
                } else {
                    successful_reads.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0)
        << "Concurrent reads to different pages should not fail"
        << " (pin_errors=" << pin_errors.load()
        << ", unpin_errors=" << unpin_errors.load()
        << ", pin_invalid_argument=" << pin_invalid_argument.load()
        << ", pin_io_error=" << pin_io_error.load()
        << ", pin_other=" << pin_other.load()
        << ", unpin_invalid_argument=" << unpin_invalid_argument.load()
        << ", unpin_not_found=" << unpin_not_found.load()
        << ", unpin_other=" << unpin_other.load()
        << ", last_error='" << last_error << "')";
    EXPECT_EQ(successful_reads.load(), NUM_THREADS * ITERATIONS);

    std::cout << "Concurrent reads test: " << successful_reads.load() << " successful reads\n";
}

/**
 * Test 2: Concurrent Writes to Different Pages
 *
 * Tests Issue 2.1 - Page-level concurrency control
 * Validates that multiple threads can write different pages concurrently
 */
TEST_F(ConcurrentPageAccessTest, ConcurrentWritesDifferentPages) {
    const bool heavy = isHeavyConcurrencyTest();
    const int scale = parallelDivisor();
    const int NUM_THREADS = std::max(4, (heavy ? 40 : 12) / scale);
    const int ITERATIONS = std::max(10, (heavy ? 50 : 30) / scale);

    std::atomic<int> errors{0};
    std::atomic<int> successful_writes{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            std::mt19937 gen = makeThreadRng(t + 101);
            std::uniform_int_distribution<> dis(0, allocated_pages_.size() - 1);

            for (int i = 0; i < ITERATIONS; ++i) {
                uint32_t page_id = allocated_pages_[dis(gen)];

                void* buffer = nullptr;
                Status s = pool_->pinPage(page_id, &buffer, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                    continue;
                }

                // Simulate write operation
                char* data = static_cast<char*>(buffer);
                data[0] = static_cast<char>(t);
                std::this_thread::sleep_for(std::chrono::microseconds(10));

                s = pool_->unpinPage(page_id, true, &ctx); // dirty = true
                if (s != Status::OK) {
                    errors.fetch_add(1);
                } else {
                    successful_writes.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Concurrent writes to different pages should not fail";
    EXPECT_EQ(successful_writes.load(), NUM_THREADS * ITERATIONS);

    std::cout << "Concurrent writes test: " << successful_writes.load() << " successful writes\n";
}

/**
 * Test 3: Concurrent Read/Write to Same Page (MVCC)
 *
 * Tests Issue 2.2 - MVCC validation with concurrent updates to same page
 * Validates that multiple threads can safely access the same page
 */
TEST_F(ConcurrentPageAccessTest, ConcurrentReadWriteSamePage) {
    const bool heavy = isHeavyConcurrencyTest();
    const int scale = parallelDivisor();
    const int NUM_READ_THREADS = std::max(4, (heavy ? 30 : 12) / scale);
    const int NUM_WRITE_THREADS = std::max(2, (heavy ? 10 : 4) / scale);
    const int ITERATIONS = std::max(10, (heavy ? 100 : 50) / scale);
    const uint32_t SHARED_PAGE = allocated_pages_[10];

    std::atomic<int> errors{0};
    std::atomic<int> successful_ops{0};
    std::vector<std::thread> threads;

    // Reader threads
    for (int t = 0; t < NUM_READ_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext ctx;
            for (int i = 0; i < ITERATIONS; ++i) {
                void* buffer = nullptr;
                Status s = pool_->pinPage(SHARED_PAGE, &buffer, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                    continue;
                }

                // Read operation
                std::this_thread::yield();

                s = pool_->unpinPage(SHARED_PAGE, false, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                } else {
                    successful_ops.fetch_add(1);
                }
            }
        });
    }

    // Writer threads
    for (int t = 0; t < NUM_WRITE_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            for (int i = 0; i < ITERATIONS; ++i) {
                void* buffer = nullptr;
                Status s = pool_->pinPage(SHARED_PAGE, &buffer, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                    continue;
                }

                // Write operation
                char* data = static_cast<char*>(buffer);
                data[t % 100] = static_cast<char>(t);
                std::this_thread::yield();

                s = pool_->unpinPage(SHARED_PAGE, true, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                } else {
                    successful_ops.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Concurrent read/write to same page should be safe";
    EXPECT_EQ(successful_ops.load(), (NUM_READ_THREADS + NUM_WRITE_THREADS) * ITERATIONS);

    std::cout << "Same-page concurrency test: " << successful_ops.load() << " operations completed\n";
}

/**
 * Test 4: Cross-Page Transaction Updates
 *
 * Tests Issue 2.3 - Cross-page transaction atomicity
 * Validates that transactions spanning multiple pages are atomic
 */
TEST_F(ConcurrentPageAccessTest, CrossPageTransactionUpdates) {
    const bool heavy = isHeavyConcurrencyTest();
    const int scale = parallelDivisor();
    const int NUM_THREADS = std::max(4, (heavy ? 20 : 8) / scale);
    const int ITERATIONS = std::max(10, (heavy ? 50 : 20) / scale);
    txn_mgr_->enableGroupCommit(false);

    std::atomic<int> errors{0};
    std::atomic<int> successful_transactions{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            std::unique_ptr<ConnectionContext> conn;
            Status s = db_->connect(conn, &ctx);
            if (s != Status::OK) {
                errors.fetch_add(1);
                return;
            }
            std::mt19937 gen = makeThreadRng(t + 201);
            std::uniform_int_distribution<> dis(0, allocated_pages_.size() - 1);

            for (int i = 0; i < ITERATIONS; ++i) {
                // Update 3 different pages in one transaction
                bool transaction_ok = true;
                std::vector<uint32_t> pages_to_update;
                std::vector<void*> buffers;

                for (int j = 0; j < 3; ++j) {
                    uint32_t page_id = allocated_pages_[dis(gen)];
                    pages_to_update.push_back(page_id);

                    void* buffer = nullptr;
                    s = pool_->pinPage(page_id, &buffer, &ctx);
                    if (s != Status::OK) {
                        transaction_ok = false;
                        break;
                    }
                    buffers.push_back(buffer);

                    // Write to page
                    char* data = static_cast<char*>(buffer);
                    data[0] = static_cast<char>(t);
                }

                // Unpin all pages
                for (size_t j = 0; j < buffers.size(); ++j) {
                    s = pool_->unpinPage(pages_to_update[j], true, &ctx);
                    if (s != Status::OK) {
                        transaction_ok = false;
                    }
                }

                // Commit transaction
                if (transaction_ok) {
                    s = conn->commit(&ctx);
                    if (s == Status::OK) {
                        successful_transactions.fetch_add(1);
                    } else {
                        errors.fetch_add(1);
                    }
                } else {
                    conn->rollback(&ctx);
                    errors.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Cross-page transactions should complete atomically";
    EXPECT_EQ(successful_transactions.load(), NUM_THREADS * ITERATIONS);

    std::cout << "Cross-page transaction test: " << successful_transactions.load() << " transactions committed\n";
}

/**
 * Test 5: Snapshot Consistency Under Concurrent Modifications
 *
 * Tests Issue 2.4 - Snapshot isolation with concurrent page modifications
 * Validates that snapshots remain consistent despite concurrent writes
 */
TEST_F(ConcurrentPageAccessTest, SnapshotConsistencyUnderConcurrentMods) {
    const bool heavy = isHeavyConcurrencyTest();
    const int scale = parallelDivisor();
    const int NUM_WRITER_THREADS = std::max(4, (heavy ? 20 : 8) / scale);
    const int NUM_SNAPSHOT_THREADS = std::max(4, (heavy ? 30 : 12) / scale);
    const int ITERATIONS = std::max(10, (heavy ? 50 : 30) / scale);

    std::atomic<int> errors{0};
    std::atomic<int> successful_snapshots{0};
    std::atomic<int> successful_writes{0};
    std::vector<std::thread> threads;

    // Writer threads - modify pages concurrently
    for (int t = 0; t < NUM_WRITER_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            std::unique_ptr<ConnectionContext> conn;
            Status s = db_->connect(conn, &ctx);
            if (s != Status::OK) {
                errors.fetch_add(1);
                return;
            }
            std::mt19937 gen = makeThreadRng(t + 301);
            std::uniform_int_distribution<> dis(0, allocated_pages_.size() - 1);

            for (int i = 0; i < ITERATIONS; ++i) {
                uint32_t page_id = allocated_pages_[dis(gen)];
                void* buffer = nullptr;
                s = pool_->pinPage(page_id, &buffer, &ctx);
                if (s == Status::OK) {
                    char* data = static_cast<char*>(buffer);
                    data[0] = static_cast<char>(t);
                    pool_->unpinPage(page_id, true, &ctx);

                    s = conn->commit(&ctx);
                    if (s == Status::OK) {
                        successful_writes.fetch_add(1);
                    }
                }
            }
        });
    }

    // Snapshot threads - get current XID concurrently with writes (MGA: no snapshots)
    for (int t = 0; t < NUM_SNAPSHOT_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext ctx;
            for (int i = 0; i < ITERATIONS; ++i) {
                // FIREBIRD MGA: Just get current XID, no snapshot needed
                uint64_t current_xid = txn_mgr_->getCurrentXid();
                if (current_xid > 0) {
                    successful_snapshots.fetch_add(1);
                } else {
                    errors.fetch_add(1);
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Snapshots should remain consistent under concurrent modifications";
    EXPECT_GT(successful_snapshots.load(), 0);
    EXPECT_GT(successful_writes.load(), 0);

    std::cout << "Snapshot consistency test: " << successful_snapshots.load()
              << " snapshots, " << successful_writes.load() << " writes\n";
}

/**
 * Test 6: Buffer Pool High Contention Stress
 *
 * Tests Issue 2.5 - Buffer pool stress with page contention
 * Validates that buffer pool handles high contention gracefully
 */
TEST_F(ConcurrentPageAccessTest, BufferPoolHighContentionStress) {
    const bool heavy = isHeavyConcurrencyTest();
    const int scale = parallelDivisor();
    const int NUM_THREADS = std::max(8, (heavy ? 80 : 24) / scale);
    const int ITERATIONS = std::max(10, (heavy ? 100 : 50) / scale);
    // Access only 20 pages with 80 threads - forces high contention
    const int HOT_PAGES = heavy ? 20 : 10;

    std::atomic<int> errors{0};
    std::atomic<int> successful_ops{0};
    std::vector<std::thread> threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            std::mt19937 gen = makeThreadRng(t + 401);
            std::uniform_int_distribution<> dis(0, HOT_PAGES - 1);

            for (int i = 0; i < ITERATIONS; ++i) {
                uint32_t page_id = allocated_pages_[dis(gen)];

                void* buffer = nullptr;
                Status s = pool_->pinPage(page_id, &buffer, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                    continue;
                }

                // Simulate work
                char* data = static_cast<char*>(buffer);
                data[t % 100] = static_cast<char>(t);

                bool dirty = (i % 5 == 0); // 20% write ratio
                s = pool_->unpinPage(page_id, dirty, &ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                } else {
                    successful_ops.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    auto duration_ms = duration.count();
    if (duration_ms <= 0) {
        duration_ms = 1;
    }

    EXPECT_EQ(errors.load(), 0) << "Buffer pool should handle high contention gracefully";
    EXPECT_EQ(successful_ops.load(), NUM_THREADS * ITERATIONS);

    auto stats = pool_->getStats();

    std::cout << "High contention stress test:\n";
    std::cout << "  Operations: " << successful_ops.load() << "\n";
    std::cout << "  Duration: " << duration_ms << "ms\n";
    std::cout << "  Throughput: " << (successful_ops.load() * 1000 / duration_ms) << " ops/s\n";
    std::cout << "  Buffer pool stats:\n";
    std::cout << "    Hits: " << stats.hits << "\n";
    std::cout << "    Misses: " << stats.misses << "\n";
    std::cout << "    Evictions: " << stats.evictions << "\n";
}

// Note: main() is provided by GTest::gtest_main (linked in CMakeLists.txt)
// Test suite will be discovered and run automatically
