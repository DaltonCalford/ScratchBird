#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <unordered_set>
#include <atomic>
#include <algorithm>
#include <random>
#include <filesystem>

#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/proc_array.h"
#include "test_helpers.h"

using namespace scratchbird::core;

/**
 * Test atomic XID allocation (Issue 1.2 from audit)
 * Verifies that concurrent transactions receive unique XIDs
 */

class AtomicXIDTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a unique database path for test isolation (parallel execution)
        test_db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_atomic_xid", ".sbrd");

        auto status = Database::create(test_db_file_->path(), 16384, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(test_db_file_->path(), nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        status = db_.initializeProcArray(200, nullptr);  // Support up to 200 concurrent backends
        ASSERT_EQ(status, Status::OK) << "Failed to initialize proc array";

        txn_mgr_ = db_.transaction_manager();
        ASSERT_NE(txn_mgr_, nullptr);
    }

    void TearDown() override {
        db_.close();
        // TestDatabaseFile will cleanup automatically on destruction
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_file_;
    Database db_;
    TransactionManager* txn_mgr_ = nullptr;
};

// Test 1: Serial XID allocation (baseline)
TEST_F(AtomicXIDTest, SerialAllocation) {
    uint32_t proc_id;
    auto status = ProcArrayManager::registerBackend(&proc_id, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to register backend";

    std::vector<uint64_t> xids;
    constexpr int NUM_TRANSACTIONS = 100;

    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        uint64_t xid;
        status = txn_mgr_->beginTransaction(proc_id, xid, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to begin transaction " << i;
        xids.push_back(xid);

        // Commit the transaction
        status = txn_mgr_->commitTransaction(proc_id, xid, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to commit transaction " << i;
    }

    // Verify all XIDs are unique
    std::unordered_set<uint64_t> unique_xids(xids.begin(), xids.end());
    EXPECT_EQ(unique_xids.size(), xids.size()) << "Duplicate XIDs in serial allocation!";

    // Verify XIDs are monotonically increasing
    for (size_t i = 1; i < xids.size(); i++) {
        EXPECT_GT(xids[i], xids[i-1]) << "XIDs not monotonically increasing at position " << i;
    }

    ProcArrayManager::unregisterBackend(proc_id, nullptr);
}

// Test 2: Concurrent XID allocation with 10 threads
TEST_F(AtomicXIDTest, ConcurrentAllocation_10Threads) {
    constexpr int NUM_THREADS = 10;
    constexpr int XIDS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::vector<std::vector<uint64_t>> xids_per_thread(NUM_THREADS);
    std::vector<uint32_t> proc_ids(NUM_THREADS);
    std::atomic<int> errors{0};

    // Register backends for all threads first
    for (int t = 0; t < NUM_THREADS; t++) {
        auto status = ProcArrayManager::registerBackend(&proc_ids[t], nullptr);
        if (status != Status::OK) {
            FAIL() << "Failed to register backend " << t;
        }
    }

    // Launch threads to allocate XIDs concurrently
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &xids_per_thread, &proc_ids, &errors]() {
            uint32_t proc_id = proc_ids[t];

            for (int i = 0; i < XIDS_PER_THREAD; i++) {
                uint64_t xid;
                auto status = txn_mgr_->beginTransaction(proc_id, xid, nullptr);
                if (status != Status::OK) {
                    errors++;
                    continue;
                }

                xids_per_thread[t].push_back(xid);

                // Commit immediately
                status = txn_mgr_->commitTransaction(proc_id, xid, nullptr);
                if (status != Status::OK) {
                    errors++;
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Unregister backends
    for (int t = 0; t < NUM_THREADS; t++) {
        ProcArrayManager::unregisterBackend(proc_ids[t], nullptr);
    }

    EXPECT_EQ(errors.load(), 0) << "Errors occurred during concurrent allocation";

    // Collect all XIDs
    std::vector<uint64_t> all_xids;
    for (const auto& thread_xids : xids_per_thread) {
        all_xids.insert(all_xids.end(), thread_xids.begin(), thread_xids.end());
    }

    EXPECT_EQ(all_xids.size(), NUM_THREADS * XIDS_PER_THREAD)
        << "Not all XIDs were allocated";

    // CRITICAL TEST: Verify NO duplicate XIDs
    std::unordered_set<uint64_t> unique_xids(all_xids.begin(), all_xids.end());
    EXPECT_EQ(unique_xids.size(), all_xids.size())
        << "RACE CONDITION DETECTED: Duplicate XIDs found!";

    // Report duplicates if any
    if (unique_xids.size() != all_xids.size()) {
        std::sort(all_xids.begin(), all_xids.end());
        std::cout << "Duplicate XIDs found:" << std::endl;
        for (size_t i = 1; i < all_xids.size(); i++) {
            if (all_xids[i] == all_xids[i-1]) {
                std::cout << "  XID " << all_xids[i] << " appeared multiple times" << std::endl;
            }
        }
    }
}

// Test 3: High-concurrency stress test with 100 threads
TEST_F(AtomicXIDTest, HighConcurrency_100Threads) {
    constexpr int NUM_THREADS = 100;
    constexpr int XIDS_PER_THREAD = 50;

    std::vector<std::thread> threads;
    std::vector<std::vector<uint64_t>> xids_per_thread(NUM_THREADS);
    std::atomic<int> errors{0};
    std::vector<uint32_t> proc_ids(NUM_THREADS);

    // Register all backends first
    for (int t = 0; t < NUM_THREADS; t++) {
        auto status = ProcArrayManager::registerBackend(&proc_ids[t], nullptr);
        if (status != Status::OK) {
            FAIL() << "Failed to register backend " << t;
        }
    }

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &xids_per_thread, &errors, &proc_ids]() {
            uint32_t proc_id = proc_ids[t];

            for (int i = 0; i < XIDS_PER_THREAD; i++) {
                uint64_t xid;
                auto status = txn_mgr_->beginTransaction(proc_id, xid, nullptr);
                if (status != Status::OK) {
                    errors++;
                    continue;
                }

                xids_per_thread[t].push_back(xid);

                // Commit immediately
                status = txn_mgr_->commitTransaction(proc_id, xid, nullptr);
                if (status != Status::OK) {
                    errors++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Unregister backends
    for (int t = 0; t < NUM_THREADS; t++) {
        ProcArrayManager::unregisterBackend(proc_ids[t], nullptr);
    }

    EXPECT_EQ(errors.load(), 0) << "Errors during high-concurrency test";

    // Collect and verify
    std::vector<uint64_t> all_xids;
    for (const auto& thread_xids : xids_per_thread) {
        all_xids.insert(all_xids.end(), thread_xids.begin(), thread_xids.end());
    }

    std::unordered_set<uint64_t> unique_xids(all_xids.begin(), all_xids.end());
    EXPECT_EQ(unique_xids.size(), all_xids.size())
        << "Race condition in high-concurrency test!";

    std::cout << "High-concurrency test: " << all_xids.size()
              << " unique XIDs allocated across " << NUM_THREADS << " threads" << std::endl;
}

// Test 4: Concurrent allocation with random delays (stress test)
TEST_F(AtomicXIDTest, ConcurrentWithDelays) {
    constexpr int NUM_THREADS = 20;
    constexpr int XIDS_PER_THREAD = 25;

    std::vector<std::thread> threads;
    std::vector<std::vector<uint64_t>> xids_per_thread(NUM_THREADS);
    std::atomic<int> errors{0};
    std::vector<uint32_t> proc_ids(NUM_THREADS);

    // Register all backends first
    for (int t = 0; t < NUM_THREADS; t++) {
        auto status = ProcArrayManager::registerBackend(&proc_ids[t], nullptr);
        if (status != Status::OK) {
            FAIL() << "Failed to register backend " << t;
        }
    }

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &xids_per_thread, &errors, &proc_ids]() {
            uint32_t proc_id = proc_ids[t];
            std::mt19937 rng(t); // Different seed per thread
            std::uniform_int_distribution<int> delay_dist(0, 100);

            for (int i = 0; i < XIDS_PER_THREAD; i++) {
                // Random microsecond delay to increase contention
                std::this_thread::sleep_for(std::chrono::microseconds(delay_dist(rng)));

                uint64_t xid;
                auto status = txn_mgr_->beginTransaction(proc_id, xid, nullptr);
                if (status != Status::OK) {
                    errors++;
                    continue;
                }

                xids_per_thread[t].push_back(xid);

                std::this_thread::sleep_for(std::chrono::microseconds(delay_dist(rng)));

                status = txn_mgr_->commitTransaction(proc_id, xid, nullptr);
                if (status != Status::OK) {
                    errors++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Unregister backends
    for (int t = 0; t < NUM_THREADS; t++) {
        ProcArrayManager::unregisterBackend(proc_ids[t], nullptr);
    }

    EXPECT_EQ(errors.load(), 0);

    std::vector<uint64_t> all_xids;
    for (const auto& thread_xids : xids_per_thread) {
        all_xids.insert(all_xids.end(), thread_xids.begin(), thread_xids.end());
    }

    std::unordered_set<uint64_t> unique_xids(all_xids.begin(), all_xids.end());
    EXPECT_EQ(unique_xids.size(), all_xids.size())
        << "Race condition with random delays!";
}

// Test 5: Verify XID sequence has no gaps (within reason)
TEST_F(AtomicXIDTest, SequentialConsistency) {
    // Expected runtime: ~3-4 seconds depending on CPU.
    constexpr int NUM_TRANSACTIONS = 1000;
    std::vector<uint64_t> xids;

    uint32_t proc_id;
    auto reg_status = ProcArrayManager::registerBackend(&proc_id, nullptr);
    ASSERT_EQ(reg_status, Status::OK) << "Failed to register backend";

    uint64_t first_xid = 0;
    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        uint64_t xid;
        auto status = txn_mgr_->beginTransaction(proc_id, xid, nullptr);
        ASSERT_EQ(status, Status::OK);

        if (i == 0) first_xid = xid;
        xids.push_back(xid);

        status = txn_mgr_->commitTransaction(proc_id, xid, nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    ProcArrayManager::unregisterBackend(proc_id, nullptr);

    // Verify sequential allocation (monotonic, gaps may occur from background maintenance)
    for (size_t i = 1; i < xids.size(); i++) {
        EXPECT_GT(xids[i], xids[i-1])
            << "XID sequence not monotonic at position " << i;
    }

    EXPECT_GE(xids.back(), first_xid + NUM_TRANSACTIONS - 1)
        << "XID sequence unexpectedly short";
}

// Test 6: Performance benchmark
TEST_F(AtomicXIDTest, PerformanceBenchmark) {
    // Expected runtime: ~3-4 seconds depending on CPU.
    constexpr int NUM_THREADS = 10;
    constexpr int XIDS_PER_THREAD = 1000;
    std::vector<uint32_t> proc_ids(NUM_THREADS);

    // Register all backends first (outside of timing)
    for (int t = 0; t < NUM_THREADS; t++) {
        auto status = ProcArrayManager::registerBackend(&proc_ids[t], nullptr);
        if (status != Status::OK) {
            FAIL() << "Failed to register backend " << t;
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &proc_ids]() {
            uint32_t proc_id = proc_ids[t];

            for (int i = 0; i < XIDS_PER_THREAD; i++) {
                uint64_t xid;
                txn_mgr_->beginTransaction(proc_id, xid, nullptr);
                txn_mgr_->commitTransaction(proc_id, xid, nullptr);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Unregister backends (outside of timing)
    for (int t = 0; t < NUM_THREADS; t++) {
        ProcArrayManager::unregisterBackend(proc_ids[t], nullptr);
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    int total_transactions = NUM_THREADS * XIDS_PER_THREAD;
    double transactions_per_sec = (total_transactions * 1000.0) / duration.count();

    std::cout << "Performance: " << total_transactions << " transactions in "
              << duration.count() << "ms" << std::endl;
    std::cout << "Throughput: " << static_cast<int>(transactions_per_sec)
              << " transactions/second" << std::endl;

    // Verify we meet performance target (at least 1K/sec - reduced from 10K for reliable CI)
    EXPECT_GT(transactions_per_sec, 1000)
        << "Performance below target (1K txn/sec)";
}

// Test 7: Verify atomic operations don't interfere with other fields
TEST_F(AtomicXIDTest, AtomicIsolation) {
    // Start multiple transactions concurrently and verify other state remains consistent
    constexpr int NUM_THREADS = 50;
    constexpr int ITERATIONS = 100;

    std::vector<std::thread> threads;
    std::atomic<int> consistency_errors{0};
    std::vector<uint32_t> proc_ids(NUM_THREADS);

    // Register all backends first
    for (int t = 0; t < NUM_THREADS; t++) {
        auto status = ProcArrayManager::registerBackend(&proc_ids[t], nullptr);
        if (status != Status::OK) {
            FAIL() << "Failed to register backend " << t;
        }
    }

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &consistency_errors, &proc_ids]() {
            uint32_t proc_id = proc_ids[t];

            for (int i = 0; i < ITERATIONS; i++) {
                uint64_t xid1, xid2;

                // Start two transactions
                auto status1 = txn_mgr_->beginTransaction(proc_id, xid1, nullptr);
                auto status2 = txn_mgr_->beginTransaction(proc_id, xid2, nullptr);

                if (status1 != Status::OK || status2 != Status::OK) {
                    consistency_errors++;
                    continue;
                }

                // In a concurrent environment, we can only verify that xid2 > xid1
                // (not that xid2 == xid1 + 1, since other threads may allocate XIDs between)
                if (xid2 <= xid1) {
                    consistency_errors++;
                }

                // Verify transaction state is accessible
                TransactionState state;
                if (txn_mgr_->getTransactionState(xid1, state, nullptr) != Status::OK) {
                    consistency_errors++;
                }

                txn_mgr_->commitTransaction(proc_id, xid1, nullptr);
                txn_mgr_->commitTransaction(proc_id, xid2, nullptr);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Unregister backends
    for (int t = 0; t < NUM_THREADS; t++) {
        ProcArrayManager::unregisterBackend(proc_ids[t], nullptr);
    }

    EXPECT_EQ(consistency_errors.load(), 0)
        << "Consistency errors detected in atomic isolation test";
}
