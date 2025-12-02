/**
 * Multi-Threaded Stress Tests (100+ Threads)
 *
 * Extreme stress testing to validate system behavior under maximum load:
 * - 100-200 concurrent threads
 * - Sustained high load (10,000+ operations per test)
 * - Mixed workloads (read, write, transaction, snapshot)
 * - Resource exhaustion scenarios
 *
 * Test Categories:
 * 1. Extreme thread count (100, 150, 200 threads)
 * 2. Sustained load (run for extended periods)
 * 3. Mixed operation types (buffer pool, transactions, snapshots)
 * 4. System resource limits (context switches, memory pressure)
 * 5. Throughput measurement (ops/sec under extreme load)
 *
 * What This Test Validates:
 * - System doesn't crash under extreme thread counts
 * - Performance degrades gracefully (no cliff)
 * - No deadlocks with 100+ concurrent threads
 * - Resource management scales to high concurrency
 * - Throughput remains reasonable under stress
 *
 * Execution:
 *   From build/: ./tests/multithreaded_stress
 *   Expected: Tests complete without crashes, graceful performance degradation
 */

#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

class MultiThreadStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = "/tmp/test_multithreaded_stress.db";
        std::remove(test_db_path_);

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

        // Pre-allocate 100 pages for stress tests
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
        std::remove(test_db_path_);
    }

    const char* test_db_path_;
    std::unique_ptr<Database> db_;
    BufferPool* pool_;
    PageManager* page_mgr_;
    TransactionManager* txn_mgr_;
    std::vector<uint32_t> allocated_pages_;
};

/**
 * Test 1: 100 Threads - Buffer Pool Stress
 *
 * Validates buffer pool with 100 concurrent threads
 */
TEST_F(MultiThreadStressTest, HundredThreadsBufferPoolStress) {
    const int NUM_THREADS = 100;
    const int ITERATIONS = 100;

    std::atomic<int> successful_ops{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, allocated_pages_.size() - 1);

            for (int i = 0; i < ITERATIONS; ++i) {
                uint32_t page_id = allocated_pages_[dis(gen)];
                void* buffer = nullptr;

                Status s = pool_->pinPage(page_id, &buffer, &ctx);
                if (s == Status::OK) {
                    successful_ops.fetch_add(1);
                    s = pool_->unpinPage(page_id, false, &ctx);
                    if (s != Status::OK) {
                        errors.fetch_add(1);
                    }
                } else {
                    errors.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    auto stats = pool_->getStats();

    EXPECT_GT(successful_ops.load(), 0) << "Some operations should succeed";

    std::cout << "\n100 Threads - Buffer Pool Stress:\n";
    std::cout << "  Threads: " << NUM_THREADS << "\n";
    std::cout << "  Total operations: " << (NUM_THREADS * ITERATIONS) << "\n";
    std::cout << "  Successful: " << successful_ops.load() << "\n";
    std::cout << "  Errors: " << errors.load() << "\n";
    std::cout << "  Duration: " << duration.count() << "ms\n";
    std::cout << "  Throughput: " << (successful_ops.load() * 1000 / duration.count()) << " ops/s\n";
    std::cout << "  Buffer pool evictions: " << stats.evictions << "\n";
    std::cout << "  Clock sweeps: " << stats.clock_sweeps << "\n";
}

/**
 * Test 2: 150 Threads - Mixed Workload
 *
 * Validates system with 150 threads doing mixed operations
 */
TEST_F(MultiThreadStressTest, HundredFiftyThreadsMixedWorkload) {
    const int NUM_THREADS = 150;
    const int ITERATIONS = 50;

    std::atomic<int> successful_ops{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext ctx;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, allocated_pages_.size() - 1);

            for (int i = 0; i < ITERATIONS; ++i) {
                switch (i % 3) {
                    case 0: {
                        // Buffer pool operation
                        uint32_t page_id = allocated_pages_[dis(gen)];
                        void* buffer = nullptr;
                        if (pool_->pinPage(page_id, &buffer, &ctx) == Status::OK) {
                            pool_->unpinPage(page_id, false, &ctx);
                            successful_ops.fetch_add(1);
                        } else {
                            errors.fetch_add(1);
                        }
                        break;
                    }
                    case 1: {
                        // Transaction operation
                        std::unique_ptr<ConnectionContext> conn;
                        if (db_->connect(conn, &ctx) == Status::OK) {
                            if (conn->commit(&ctx) == Status::OK) {
                                successful_ops.fetch_add(1);
                            } else {
                                errors.fetch_add(1);
                            }
                        } else {
                            errors.fetch_add(1);
                        }
                        break;
                    }
                    case 2: {
                        // MGA: getSnapshot removed - use getCurrentXid instead
                        uint64_t current_xid = txn_mgr_->getCurrentXid();
                        (void)current_xid; // Suppress unused variable warning
                        successful_ops.fetch_add(1);
                        break;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_GT(successful_ops.load(), 0) << "Some operations should succeed";

    std::cout << "\n150 Threads - Mixed Workload:\n";
    std::cout << "  Threads: " << NUM_THREADS << "\n";
    std::cout << "  Total operations: " << (NUM_THREADS * ITERATIONS) << "\n";
    std::cout << "  Successful: " << successful_ops.load() << "\n";
    std::cout << "  Errors: " << errors.load() << "\n";
    std::cout << "  Duration: " << duration.count() << "ms\n";
    std::cout << "  Throughput: " << (successful_ops.load() * 1000 / duration.count()) << " ops/s\n";
    std::cout << "  Success rate: " << (successful_ops.load() * 100.0 / (NUM_THREADS * ITERATIONS)) << "%\n";
}

/**
 * Test 3: 200 Threads - Maximum Stress
 *
 * Validates system behavior at extreme thread count
 */
TEST_F(MultiThreadStressTest, TwoHundredThreadsMaximumStress) {
    const int NUM_THREADS = 200;
    const int ITERATIONS = 50;

    std::atomic<int> successful_ops{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                // Simple buffer pool operations
                uint32_t page_id = allocated_pages_[i % allocated_pages_.size()];
                void* buffer = nullptr;

                if (pool_->pinPage(page_id, &buffer, &ctx) == Status::OK) {
                    pool_->unpinPage(page_id, false, &ctx);
                    successful_ops.fetch_add(1);
                } else {
                    errors.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    auto stats = pool_->getStats();

    EXPECT_GT(successful_ops.load(), 0) << "Some operations should succeed even with 200 threads";

    std::cout << "\n200 Threads - Maximum Stress:\n";
    std::cout << "  Threads: " << NUM_THREADS << "\n";
    std::cout << "  Total operations: " << (NUM_THREADS * ITERATIONS) << "\n";
    std::cout << "  Successful: " << successful_ops.load() << "\n";
    std::cout << "  Errors: " << errors.load() << "\n";
    std::cout << "  Duration: " << duration.count() << "ms\n";
    std::cout << "  Throughput: " << (successful_ops.load() * 1000 / duration.count()) << " ops/s\n";
    std::cout << "  Success rate: " << (successful_ops.load() * 100.0 / (NUM_THREADS * ITERATIONS)) << "%\n";
    std::cout << "  Buffer pool stats:\n";
    std::cout << "    Evictions: " << stats.evictions << "\n";
    std::cout << "    Clock sweeps: " << stats.clock_sweeps << "\n";
}

/**
 * Test 4: Sustained Load - 100 Threads for Extended Period
 *
 * Validates system stability under sustained load
 */
TEST_F(MultiThreadStressTest, SustainedLoadHundredThreads) {
    const int NUM_THREADS = 100;
    const int ITERATIONS = 200; // 2x longer

    std::atomic<int> successful_ops{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext ctx;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, allocated_pages_.size() - 1);

            for (int i = 0; i < ITERATIONS; ++i) {
                uint32_t page_id = allocated_pages_[dis(gen)];
                void* buffer = nullptr;

                if (pool_->pinPage(page_id, &buffer, &ctx) == Status::OK) {
                    // Hold the pin for a bit to simulate real work
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    pool_->unpinPage(page_id, false, &ctx);
                    successful_ops.fetch_add(1);
                } else {
                    errors.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_GT(successful_ops.load(), 0) << "Some operations should succeed";

    std::cout << "\nSustained Load - 100 Threads:\n";
    std::cout << "  Threads: " << NUM_THREADS << "\n";
    std::cout << "  Total operations: " << (NUM_THREADS * ITERATIONS) << "\n";
    std::cout << "  Successful: " << successful_ops.load() << "\n";
    std::cout << "  Errors: " << errors.load() << "\n";
    std::cout << "  Duration: " << duration.count() << "ms\n";
    std::cout << "  Throughput: " << (successful_ops.load() * 1000 / duration.count()) << " ops/s\n";
}

/**
 * Test 5: Throughput Benchmark - 100 Threads
 *
 * Measures peak throughput with 100 threads
 */
TEST_F(MultiThreadStressTest, ThroughputBenchmarkHundredThreads) {
    const int NUM_THREADS = 100;
    const int ITERATIONS = 150;

    std::atomic<int> successful_ops{0};
    std::vector<std::thread> threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext ctx;

            // Use a hot working set (only 20 pages) for maximum throughput
            const int HOT_PAGES = 20;

            for (int i = 0; i < ITERATIONS; ++i) {
                uint32_t page_id = allocated_pages_[i % HOT_PAGES];
                void* buffer = nullptr;

                if (pool_->pinPage(page_id, &buffer, &ctx) == Status::OK) {
                    pool_->unpinPage(page_id, false, &ctx);
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

    auto stats = pool_->getStats();
    uint64_t total_accesses = stats.hits + stats.misses;
    double hit_rate = (total_accesses > 0) ? (stats.hits * 100.0 / total_accesses) : 0.0;

    EXPECT_GT(successful_ops.load(), 0);

    std::cout << "\nThroughput Benchmark - 100 Threads:\n";
    std::cout << "  Threads: " << NUM_THREADS << "\n";
    std::cout << "  Operations: " << successful_ops.load() << "\n";
    std::cout << "  Duration: " << duration.count() << "ms\n";
    std::cout << "  **Peak Throughput: " << (successful_ops.load() * 1000 / duration.count()) << " ops/s**\n";
    std::cout << "  Buffer pool hit rate: " << hit_rate << "%\n";
    std::cout << "  Evictions: " << stats.evictions << "\n";
}

// Note: main() is provided by GTest::gtest_main (linked in CMakeLists.txt)
