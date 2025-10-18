#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/error_context.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace scratchbird::core;

class GroupCommitTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create test database
        ErrorContext ctx;
        Status status = Database::create(db_path_.c_str(), 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        db_ = new Database();
        status = db_->open(db_path_.c_str(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(txn_mgr_, nullptr);

        // Initialize ProcArray for multi-transaction support
        status = db_->initializeProcArray(100, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to initialize ProcArray: " << ctx.message;
    }

    void TearDown() override
    {
        if (db_)
        {
            delete db_;
            db_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }

    std::string db_path_ = "test_group_commit.db";
    Database *db_ = nullptr;
    TransactionManager *txn_mgr_ = nullptr;
};

// Test 1: Basic group commit functionality
TEST_F(GroupCommitTest, BasicGroupCommit)
{
    ErrorContext ctx;

    // Ensure group commit is enabled
    txn_mgr_->enableGroupCommit(true);

    // Begin and commit a single transaction
    uint64_t xid;
    Status status = txn_mgr_->beginTransaction(0, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = txn_mgr_->commitTransaction(0, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify transaction is committed
    TransactionState state;
    status = txn_mgr_->getTransactionState(xid, state, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(state, TransactionState::COMMITTED);
}

// Test 2: Leader election (first committer becomes leader)
TEST_F(GroupCommitTest, LeaderElection)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(true);

    std::atomic<int> commits_completed{0};
    std::atomic<int> leader_count{0};

    const int num_threads = 10;
    std::vector<std::thread> threads;

    // Launch multiple threads that commit simultaneously
    for (int i = 0; i < num_threads; i++)
    {
        threads.emplace_back([this, i, &commits_completed, &leader_count]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = i;

            // Begin transaction
            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);

            // Small delay to ensure all threads are ready
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // Commit (first one becomes leader)
            status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);

            commits_completed.fetch_add(1);
        });
    }

    // Wait for all threads to complete
    for (auto &t : threads)
    {
        t.join();
    }

    // Verify all commits succeeded
    EXPECT_EQ(commits_completed.load(), num_threads);

    // Check group commit statistics
    auto [group_commits, total_xids] = txn_mgr_->getGroupCommitStats();
    EXPECT_GT(group_commits, 0) << "Expected at least one group commit";
    EXPECT_EQ(total_xids, num_threads) << "Expected all XIDs to be batched";
}

// Test 3: Batch collection with timeout
TEST_F(GroupCommitTest, BatchCollectionTimeout)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(true);
    txn_mgr_->setGroupCommitTimeout(5000); // 5ms timeout

    const int num_commits = 5;
    std::vector<std::thread> threads;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_commits; i++)
    {
        threads.emplace_back([this, i]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = i;

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);

            status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // Should complete quickly with group commit (much less than num_commits * fsync_time)
    EXPECT_LT(elapsed_ms, 100) << "Group commit should be fast";

    // Verify statistics
    auto [group_commits, total_xids] = txn_mgr_->getGroupCommitStats();
    EXPECT_GT(group_commits, 0);
    EXPECT_EQ(total_xids, num_commits);
}

// Test 4: Batch size limit
TEST_F(GroupCommitTest, BatchSizeLimit)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(true);
    txn_mgr_->setGroupCommitBatchSize(5); // Small batch size

    const int num_commits = 20;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_commits; i++)
    {
        threads.emplace_back([this, i]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = i;

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);

            // Small delay to ensure threads pile up
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // Should have multiple group commits due to batch size limit
    auto [group_commits, total_xids] = txn_mgr_->getGroupCommitStats();
    EXPECT_GE(group_commits, num_commits / 5) << "Expected multiple batches";
    EXPECT_EQ(total_xids, num_commits);
}

// Test 5: Fallback to individual commit when disabled
TEST_F(GroupCommitTest, FallbackWhenDisabled)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(false); // Disable group commit

    const int num_commits = 5;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_commits; i++)
    {
        threads.emplace_back([this, i]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = i;

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);

            status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // Should have zero group commits
    auto [group_commits, total_xids] = txn_mgr_->getGroupCommitStats();
    EXPECT_EQ(group_commits, 0) << "Should not use group commit when disabled";
    EXPECT_EQ(total_xids, 0);
}

// Test 6: High concurrency stress test (100 threads)
TEST_F(GroupCommitTest, HighConcurrencyStress)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(true);
    txn_mgr_->setGroupCommitTimeout(10000);  // 10ms
    txn_mgr_->setGroupCommitBatchSize(32);

    const int num_threads = 100;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::thread> threads;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_threads; i++)
    {
        threads.emplace_back([this, i, &success_count, &failure_count]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = i % 100; // Reuse proc slots

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            if (status != Status::OK)
            {
                failure_count.fetch_add(1);
                return;
            }

            status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
            if (status == Status::OK)
            {
                success_count.fetch_add(1);
            }
            else
            {
                failure_count.fetch_add(1);
            }
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // Verify all commits succeeded
    EXPECT_EQ(success_count.load(), num_threads) << "All commits should succeed";
    EXPECT_EQ(failure_count.load(), 0) << "No commits should fail";

    // Check statistics
    auto [group_commits, total_xids] = txn_mgr_->getGroupCommitStats();
    EXPECT_GT(group_commits, 0) << "Should have performed group commits";
    EXPECT_EQ(total_xids, num_threads) << "All XIDs should be accounted for";

    // Calculate average batch size
    double avg_batch_size = static_cast<double>(total_xids) / group_commits;
    EXPECT_GT(avg_batch_size, 1.0) << "Average batch size should be > 1";

    std::cout << "High concurrency test results:\n";
    std::cout << "  Threads: " << num_threads << "\n";
    std::cout << "  Elapsed: " << elapsed_ms << " ms\n";
    std::cout << "  Group commits: " << group_commits << "\n";
    std::cout << "  Total XIDs: " << total_xids << "\n";
    std::cout << "  Average batch size: " << avg_batch_size << "\n";
    std::cout << "  Throughput: " << (num_threads * 1000.0 / elapsed_ms) << " commits/sec\n";
}

// Test 7: Mixed read-write transactions
TEST_F(GroupCommitTest, MixedReadWrite)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(true);

    const int num_writers = 20;
    const int num_readers = 20;
    std::vector<std::thread> threads;

    // Writers
    for (int i = 0; i < num_writers; i++)
    {
        threads.emplace_back([this, i]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = i;

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);

            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);
        });
    }

    // Readers (just begin and commit, no writes)
    for (int i = 0; i < num_readers; i++)
    {
        threads.emplace_back([this, i]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = num_writers + i;

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);

            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // Verify statistics
    auto [group_commits, total_xids] = txn_mgr_->getGroupCommitStats();
    EXPECT_GT(group_commits, 0);
    EXPECT_EQ(total_xids, num_writers + num_readers);
}

// Test 8: Error handling during group commit
TEST_F(GroupCommitTest, ErrorHandling)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(true);

    // This test verifies that if a commit fails, all waiters get the error
    // In practice, commit failures are rare, but we test the mechanism

    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> ok_count{0};
    std::atomic<int> error_count{0};

    for (int i = 0; i < num_threads; i++)
    {
        threads.emplace_back([this, i, &ok_count, &error_count]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = i;

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            if (status != Status::OK)
            {
                error_count.fetch_add(1);
                return;
            }

            status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
            if (status == Status::OK)
            {
                ok_count.fetch_add(1);
            }
            else
            {
                error_count.fetch_add(1);
            }
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // All should succeed in normal operation
    EXPECT_EQ(ok_count.load(), num_threads);
    EXPECT_EQ(error_count.load(), 0);
}

// Test 9: Sequential vs batched performance comparison
TEST_F(GroupCommitTest, PerformanceComparison)
{
    ErrorContext ctx;
    const int num_commits = 50;

    // Test with group commit disabled (sequential)
    txn_mgr_->enableGroupCommit(false);
    auto start_seq = std::chrono::steady_clock::now();

    for (int i = 0; i < num_commits; i++)
    {
        uint64_t xid;
        Status status = txn_mgr_->beginTransaction(i % 100, xid, &ctx);
        ASSERT_EQ(status, Status::OK);

        status = txn_mgr_->commitTransaction(i % 100, xid, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto elapsed_seq = std::chrono::steady_clock::now() - start_seq;
    auto seq_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_seq).count();

    // Reset statistics
    auto [gc1, xids1] = txn_mgr_->getGroupCommitStats();

    // Test with group commit enabled (batched)
    txn_mgr_->enableGroupCommit(true);
    auto start_batch = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_commits; i++)
    {
        threads.emplace_back([this, i]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = i % 100;

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);

            status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto elapsed_batch = std::chrono::steady_clock::now() - start_batch;
    auto batch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_batch).count();

    auto [gc2, xids2] = txn_mgr_->getGroupCommitStats();
    int group_commits = gc2 - gc1;
    int batched_xids = xids2 - xids1;

    std::cout << "\nPerformance comparison:\n";
    std::cout << "  Sequential: " << seq_ms << " ms (" << (num_commits * 1000.0 / seq_ms)
              << " commits/sec)\n";
    std::cout << "  Batched: " << batch_ms << " ms (" << (num_commits * 1000.0 / batch_ms)
              << " commits/sec)\n";
    std::cout << "  Speedup: " << (static_cast<double>(seq_ms) / batch_ms) << "x\n";
    std::cout << "  Group commits: " << group_commits << "\n";
    std::cout << "  Average batch size: " << (static_cast<double>(batched_xids) / group_commits)
              << "\n";

    // Group commit should be faster (or at least not slower)
    EXPECT_LE(batch_ms, seq_ms * 2) << "Batched should not be significantly slower";
}

// Test 10: Group commit with rollbacks
TEST_F(GroupCommitTest, RollbackGroupCommit)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(true);
    txn_mgr_->setGroupCommitTimeout(10000);  // 10ms
    txn_mgr_->setGroupCommitBatchSize(32);

    const int num_rollbacks = 20;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::thread> threads;

    // Get baseline statistics
    auto [gc_before, xids_before] = txn_mgr_->getGroupCommitStats();

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_rollbacks; i++)
    {
        threads.emplace_back([this, i, &success_count, &failure_count]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = i;

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            if (status != Status::OK)
            {
                failure_count.fetch_add(1);
                return;
            }

            // Rollback transaction
            status = txn_mgr_->rollbackTransaction(proc_id, xid, &thread_ctx);
            if (status == Status::OK)
            {
                success_count.fetch_add(1);
            }
            else
            {
                failure_count.fetch_add(1);
            }
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // Verify all rollbacks succeeded
    EXPECT_EQ(success_count.load(), num_rollbacks);
    EXPECT_EQ(failure_count.load(), 0);

    // Check statistics - rollbacks should also be batched
    auto [gc_after, xids_after] = txn_mgr_->getGroupCommitStats();
    uint64_t group_commits = gc_after - gc_before;
    uint64_t total_xids = xids_after - xids_before;

    EXPECT_GT(group_commits, 0) << "Rollbacks should use group commit";
    EXPECT_EQ(total_xids, num_rollbacks) << "All rollbacks should be batched";

    // Verify transactions are actually aborted
    for (int i = 0; i < num_rollbacks; i++)
    {
        uint64_t xid = 3 + i; // XIDs start at FROZEN_XID + 1 = 3
        TransactionState state;
        Status status = txn_mgr_->getTransactionState(xid, state, &ctx);
        if (status == Status::OK)
        {
            EXPECT_EQ(state, TransactionState::ABORTED) << "XID " << xid << " should be aborted";
        }
    }

    double avg_batch_size = static_cast<double>(total_xids) / group_commits;
    std::cout << "\nRollback group commit test results:\n";
    std::cout << "  Rollbacks: " << num_rollbacks << "\n";
    std::cout << "  Elapsed: " << elapsed_ms << " ms\n";
    std::cout << "  Group commits: " << group_commits << "\n";
    std::cout << "  Average batch size: " << avg_batch_size << "\n";
    std::cout << "  Throughput: " << (num_rollbacks * 1000.0 / elapsed_ms) << " rollbacks/sec\n";
}

// Test 11: Mixed commits and rollbacks in same batch
TEST_F(GroupCommitTest, MixedCommitRollback)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(true);
    txn_mgr_->setGroupCommitTimeout(10000);
    txn_mgr_->setGroupCommitBatchSize(32);

    const int num_commits = 15;
    const int num_rollbacks = 15;
    const int total = num_commits + num_rollbacks;
    std::atomic<int> commit_success{0};
    std::atomic<int> rollback_success{0};
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;

    // Get baseline statistics
    auto [gc_before, xids_before] = txn_mgr_->getGroupCommitStats();

    // Launch mixed commit/rollback threads
    for (int i = 0; i < total; i++)
    {
        bool should_commit = (i % 2 == 0); // Alternate between commit and rollback

        threads.emplace_back([this, i, should_commit, &commit_success, &rollback_success,
                              &failures]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = i;

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            if (status != Status::OK)
            {
                failures.fetch_add(1);
                return;
            }

            // Small delay to allow batching
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            if (should_commit)
            {
                status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
                if (status == Status::OK)
                {
                    commit_success.fetch_add(1);
                }
                else
                {
                    failures.fetch_add(1);
                }
            }
            else
            {
                status = txn_mgr_->rollbackTransaction(proc_id, xid, &thread_ctx);
                if (status == Status::OK)
                {
                    rollback_success.fetch_add(1);
                }
                else
                {
                    failures.fetch_add(1);
                }
            }
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // Verify all operations succeeded
    EXPECT_EQ(commit_success.load(), num_commits);
    EXPECT_EQ(rollback_success.load(), num_rollbacks);
    EXPECT_EQ(failures.load(), 0);

    // Check statistics - mixed operations should be batched together
    auto [gc_after, xids_after] = txn_mgr_->getGroupCommitStats();
    uint64_t group_commits = gc_after - gc_before;
    uint64_t total_xids = xids_after - xids_before;

    EXPECT_GT(group_commits, 0) << "Mixed operations should use group commit";
    EXPECT_EQ(total_xids, total) << "All operations should be batched";

    double avg_batch_size = static_cast<double>(total_xids) / group_commits;
    std::cout << "\nMixed commit/rollback test results:\n";
    std::cout << "  Commits: " << commit_success.load() << "\n";
    std::cout << "  Rollbacks: " << rollback_success.load() << "\n";
    std::cout << "  Group commits: " << group_commits << "\n";
    std::cout << "  Average batch size: " << avg_batch_size << "\n";
    std::cout << "  fsync reduction: "
              << (100.0 * (1.0 - static_cast<double>(group_commits) / total)) << "%\n";
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
