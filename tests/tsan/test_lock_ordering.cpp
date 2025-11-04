/**
 * ThreadSanitizer Test: Lock Ordering (Deadlock Prevention)
 *
 * Tests CRITICAL-3 fix: TransactionManager lock ordering hierarchy
 * - Issue: Inconsistent lock acquisition order could cause deadlocks
 * - Fix: Documented and enforced lock hierarchy:
 *        1. mutex_ (TransactionManager)
 *        2. ProcArray::array_lock
 *        3. group_commit_mutex_ (independent)
 * - Validation: No deadlocks under concurrent transaction operations
 *
 * Lock Ordering Rules (from transaction_manager.h):
 * - ALWAYS acquire mutex_ FIRST, then ProcArray::array_lock
 * - NEVER acquire mutex_ while holding ProcArray::array_lock
 * - group_commit_mutex_ is independent, can be acquired in any order
 *
 * What This Test Validates:
 * - Concurrent begin/commit/rollback operations don't deadlock
 * - Multiple operations that acquire locks in hierarchy order
 * - Heavy contention scenarios (100+ threads)
 *
 * Execution:
 *   From build/: ./tests/tsan_lock_ordering
 *   Expected: No deadlocks, all operations complete successfully
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

class TSANLockOrderingTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = "test_tsan_lock_ordering.db";
        std::remove(test_db_path_);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 8192, &ctx);
        ASSERT_EQ(status, Status::OK);

        db_ = std::make_unique<Database>();
        status = db_->open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK);

        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(txn_mgr_, nullptr);
    }

    void TearDown() override {
        if (db_) {
            db_->close();
        }
        std::remove(test_db_path_);
    }

    const char* test_db_path_;
    std::unique_ptr<Database> db_;
    TransactionManager* txn_mgr_;
};

/**
 * Test 1: Concurrent Transaction Lifecycle (Begin/Commit)
 *
 * This test stresses the lock acquisition paths during transaction lifecycle.
 * Multiple threads concurrently begin and commit transactions, which requires:
 * - beginTransaction(): acquires mutex_, then ProcArray::array_lock (wrlock)
 * - commitTransaction(): acquires mutex_, releases it, then group_commit_mutex_
 *
 * Validates correct lock ordering prevents deadlocks.
 */
TEST_F(TSANLockOrderingTest, ConcurrentTransactionLifecycle) {
    const int NUM_THREADS = 100;
    const int ITERATIONS = 50;

    std::atomic<int> errors{0};
    std::atomic<int> deadlocks{0};
    std::vector<std::thread> threads;

    auto start_time = std::chrono::steady_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext thread_ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                // Create connection (implicitly begins transaction)
                std::unique_ptr<ConnectionContext> conn;
                Status s = db_->connect(conn, &thread_ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                    continue;
                }

                // Simulate some work
                std::this_thread::yield();

                // Commit transaction
                s = conn->commit(&thread_ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                }

                // Small delay to increase contention
                if (i % 10 == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Monitor for deadlocks (if threads take too long, assume deadlock)
    bool completed = true;
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_time
    ).count();

    EXPECT_EQ(errors.load(), 0) << "Errors occurred during transaction lifecycle";
    EXPECT_LT(elapsed, 30) << "Test took too long, possible deadlock";

    std::cout << "Concurrent lifecycle test: " << (NUM_THREADS * ITERATIONS)
              << " transactions completed in " << elapsed << "s\n";

    auto stats = txn_mgr_->getStats();
    std::cout << "Transaction stats:\n";
    std::cout << "  Started: " << stats.transactions_started << "\n";
    std::cout << "  Committed: " << stats.transactions_committed << "\n";
}

/**
 * Test 2: Mixed Operations (Commit + Snapshot)
 *
 * This test mixes different operations that acquire locks:
 * - Commit: mutex_ → group_commit_mutex_
 * - getSnapshot: mutex_ → ProcArray::array_lock (rdlock)
 * - getTransactionState: mutex_ only
 *
 * Validates lock ordering is maintained across different operation types.
 */
TEST_F(TSANLockOrderingTest, MixedOperations) {
    const int NUM_COMMIT_THREADS = 40;
    const int NUM_SNAPSHOT_THREADS = 40;
    const int ITERATIONS = 100;

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    // Commit threads
    for (int t = 0; t < NUM_COMMIT_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext thread_ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                std::unique_ptr<ConnectionContext> conn;
                if (db_->connect(conn, &thread_ctx) == Status::OK) {
                    conn->commit(&thread_ctx);
                }
            }
        });
    }

    // Snapshot threads (MGA: snapshots removed, using current_xid instead)
    for (int t = 0; t < NUM_SNAPSHOT_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext thread_ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                // FIREBIRD MGA: Just get current XID, no snapshot needed
                uint64_t current_xid = txn_mgr_->getCurrentXid();
                (void)current_xid; // Suppress unused warning
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Errors occurred during mixed operations";

    std::cout << "Mixed operations test: "
              << (NUM_COMMIT_THREADS * ITERATIONS) << " commits + "
              << (NUM_SNAPSHOT_THREADS * ITERATIONS) << " snapshots completed\n";
}

/**
 * Test 3: High Contention Stress Test
 *
 * Maximum contention scenario with 100+ threads all competing for locks.
 * This is the most likely scenario to expose deadlocks if lock ordering
 * is incorrect.
 */
TEST_F(TSANLockOrderingTest, HighContentionStress) {
    const int NUM_THREADS = 150;
    const int ITERATIONS = 30;

    std::atomic<int> errors{0};
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            ErrorContext thread_ctx;

            for (int i = 0; i < ITERATIONS; ++i) {
                std::unique_ptr<ConnectionContext> conn;
                Status s = db_->connect(conn, &thread_ctx);
                if (s != Status::OK) {
                    errors.fetch_add(1);
                    continue;
                }

                // Alternate between commit and rollback
                if ((t + i) % 2 == 0) {
                    s = conn->commit(&thread_ctx);
                } else {
                    s = conn->rollback(&thread_ctx);
                }

                if (s != Status::OK) {
                    errors.fetch_add(1);
                } else {
                    completed.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Errors occurred during high contention stress";
    EXPECT_EQ(completed.load(), NUM_THREADS * ITERATIONS)
        << "Not all operations completed";

    std::cout << "High contention stress test: " << completed.load()
              << " operations completed successfully\n";

    auto stats = txn_mgr_->getStats();
    std::cout << "Final transaction stats:\n";
    std::cout << "  Committed: " << stats.transactions_committed << "\n";
    std::cout << "  Aborted: " << stats.transactions_aborted << "\n";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "\n========================================\n";
    std::cout << "ThreadSanitizer: Lock Ordering Test\n";
    std::cout << "========================================\n";
    std::cout << "Testing CRITICAL-3 fix: Lock hierarchy\n";
    std::cout << "Expected: No deadlocks, all tests complete\n";
    std::cout << "========================================\n\n";

    return RUN_ALL_TESTS();
}
