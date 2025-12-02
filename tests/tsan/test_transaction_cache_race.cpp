/**
 * ThreadSanitizer Test: TransactionManager Cache Race
 *
 * Tests CRITICAL-2 fix: TransactionManager const correctness
 * - Issue: getTransactionState() was const but modified transaction_cache_ (mutable)
 * - Fix: Removed const qualifier to properly reflect cache modification
 * - Validation: TSAN should detect NO data races on cache access
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

class TSANTransactionCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = "/tmp/test_tsan_transaction_cache.db";
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
 * Test: Concurrent Transaction State Queries
 *
 * Validates that multiple threads can safely query transaction state
 * which modifies the internal LRU cache.
 */
TEST_F(TSANTransactionCacheTest, ConcurrentCacheQueries) {
    const int NUM_THREADS = 50;
    const int ITERATIONS = 500;

    // Create some transactions
    std::vector<std::unique_ptr<ConnectionContext>> connections;
    std::vector<uint64_t> xids;

    ErrorContext ctx;
    for (int i = 0; i < 20; ++i) {
        std::unique_ptr<ConnectionContext> conn;
        Status s = db_->connect(conn, &ctx);
        ASSERT_EQ(s, Status::OK);
        xids.push_back(conn->getCurrentXid());
        connections.push_back(std::move(conn));
    }

    // Concurrently query transaction states
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            ErrorContext thread_ctx;
            for (int i = 0; i < ITERATIONS; ++i) {
                uint64_t xid = xids[i % xids.size()];
                TransactionState state;
                if (txn_mgr_->getTransactionState(xid, state, &thread_ctx) != Status::OK) {
                    errors.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);

    // Cleanup
    for (auto& conn : connections) {
        conn->commit(&ctx);
    }

    std::cout << "Concurrent cache queries: " << (NUM_THREADS * ITERATIONS) << " completed\n";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "\n========================================\n";
    std::cout << "ThreadSanitizer: Transaction Cache Test\n";
    std::cout << "========================================\n";
    std::cout << "Testing CRITICAL-2 fix: Cache synchronization\n";
    std::cout << "Expected: No data races\n";
    std::cout << "========================================\n\n";

    return RUN_ALL_TESTS();
}
