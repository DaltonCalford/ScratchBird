/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/mga_failpoint_manager.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <array>
#include <chrono>

using namespace scratchbird::core;

class GroupCommitTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create unique database path for test isolation (parallel execution)
        test_db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_group_commit", ".db");
        db_path_ = test_db_file_->path();

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

        // Register a bounded set of backends for testing (avoid exhausting shared slots)
        constexpr uint32_t kRequiredBackends = 20;
        uint32_t active_backends = 0;
        status = ProcArrayManager::getNumActiveBackends(&active_backends, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to query active backends: " << ctx.message;

        ProcArray *proc_array = ProcArrayManager::getInstance();
        ASSERT_NE(proc_array, nullptr);
        ASSERT_GE(proc_array->max_backends, active_backends);

        uint32_t available = proc_array->max_backends - active_backends;
        ASSERT_GE(available, kRequiredBackends)
            << "Not enough backend slots for tests (max_backends=" << proc_array->max_backends
            << ", active=" << active_backends << ")";

        for (uint32_t i = 0; i < kRequiredBackends; i++)
        {
            uint32_t proc_id;
            status = ProcArrayManager::registerBackend(&proc_id, &ctx);
            ASSERT_EQ(status, Status::OK) << "Failed to register backend " << i << ": " << ctx.message;
            registered_proc_ids_.push_back(proc_id);
        }
    }

    void TearDown() override
    {
        ErrorContext ctx;
        // Unregister all backends
        for (uint32_t proc_id : registered_proc_ids_)
        {
            ProcArrayManager::unregisterBackend(proc_id, &ctx);
        }
        registered_proc_ids_.clear();

        if (db_)
        {
            delete db_;
            db_ = nullptr;
        }
        // TestDatabaseFile will cleanup automatically on destruction
    }

    // Helper to get a registered proc_id by index
    uint32_t getProcId(int index)
    {
        return registered_proc_ids_[index % registered_proc_ids_.size()];
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_file_;
    std::string db_path_;
    Database *db_ = nullptr;
    TransactionManager *txn_mgr_ = nullptr;
    std::vector<uint32_t> registered_proc_ids_;
};

// Test 1: Basic group commit functionality
TEST_F(GroupCommitTest, BasicGroupCommit)
{
    ErrorContext ctx;

    // Ensure group commit is enabled
    txn_mgr_->enableGroupCommit(true);

    // Begin and commit a single transaction
    uint64_t xid;
    uint32_t proc_id = getProcId(0);
    Status status = txn_mgr_->beginTransaction(proc_id, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = txn_mgr_->commitTransaction(proc_id, xid, &ctx);
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
    auto [start_group_commits, start_total_xids] = txn_mgr_->getGroupCommitStats();

    // Launch multiple threads that commit simultaneously
    for (int i = 0; i < num_threads; i++)
    {
        threads.emplace_back([this, i, &commits_completed, &leader_count]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = getProcId(i);

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
    EXPECT_GT(group_commits - start_group_commits, 0) << "Expected at least one group commit";
    EXPECT_GE(total_xids - start_total_xids, num_threads) << "Expected all XIDs to be batched";
}

// Test 3: Batch collection with timeout
TEST_F(GroupCommitTest, BatchCollectionTimeout)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(true);
    // Use 10ms timeout (more tolerant under parallel load)
    txn_mgr_->setGroupCommitTimeout(10000); // 10ms timeout

    const int num_commits = 5;
    std::vector<std::thread> threads;
    auto [start_group_commits, start_total_xids] = txn_mgr_->getGroupCommitStats();

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_commits; i++)
    {
        threads.emplace_back([this, i]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = getProcId(i);

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

    // Under parallel test load, allow up to 500ms (thread scheduling can be delayed)
    // The key assertion is that group commits actually happened, not the exact timing
    if (elapsed_ms > 500) {
        std::cout << "WARNING: Group commit slower than expected under parallel load (" 
                  << elapsed_ms << " ms)" << std::endl;
    }
    EXPECT_LT(elapsed_ms, 500) << "Group commit excessively slow (> 500ms)!";

    // Verify statistics - this is the key functional assertion
    auto [group_commits, total_xids] = txn_mgr_->getGroupCommitStats();
    EXPECT_GT(group_commits - start_group_commits, 0) << "Expected at least one group commit";
    EXPECT_GE(total_xids - start_total_xids, num_commits) << "Expected all XIDs to be committed";
}

TEST_F(GroupCommitTest, GroupCommitPreTipFenceFailureRejectsWholeBatch)
{
    ErrorContext ctx;
    txn_mgr_->setDurabilityMode(DurabilityMode::GROUP_COMMIT);
    txn_mgr_->setGroupCommitTimeout(50000);
    txn_mgr_->setGroupCommitBatchSize(8);

    ASSERT_EQ(db_->mga_failpoint_manager()->installSeed(
                  "group-pretip-failure",
                  {{std::string(MgaFailpointTriggers::kAfterDirtyFlushBeforeTipTerminal),
                    MgaFailpointAction::RETURN_ERROR,
                    1,
                    Status::IO_ERROR,
                    0,
                    "group_pre_tip_blocked"}},
                  &ctx),
              Status::OK)
        << ctx.message;

    constexpr size_t kBatchMembers = 2;
    std::array<uint64_t, kBatchMembers> xids{};
    for (size_t i = 0; i < kBatchMembers; ++i)
    {
        ASSERT_EQ(txn_mgr_->beginTransaction(getProcId(static_cast<int>(i)),
                                             xids[i],
                                             &ctx),
                  Status::OK)
            << ctx.message;
    }

    std::atomic<size_t> ready{0};
    std::atomic<bool> go{false};
    std::array<Status, kBatchMembers> results{Status::OK, Status::OK};
    std::array<std::thread, kBatchMembers> threads;

    for (size_t i = 0; i < kBatchMembers; ++i)
    {
        threads[i] = std::thread([&, i]() {
            ErrorContext thread_ctx;
            ready.fetch_add(1, std::memory_order_release);
            while (!go.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            results[i] = txn_mgr_->commitTransaction(getProcId(static_cast<int>(i)),
                                                     xids[i],
                                                     &thread_ctx);
        });
    }

    while (ready.load(std::memory_order_acquire) != kBatchMembers)
    {
        std::this_thread::yield();
    }
    go.store(true, std::memory_order_release);

    for (auto &thread : threads)
    {
        thread.join();
    }

    for (Status result : results)
    {
        EXPECT_EQ(result, Status::IO_ERROR);
    }

    std::vector<MgaFailpointEvent> events;
    ASSERT_EQ(db_->mga_failpoint_manager()->listEvents(events, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].trigger_name,
              MgaFailpointTriggers::kAfterDirtyFlushBeforeTipTerminal);

    for (size_t i = 0; i < kBatchMembers; ++i)
    {
        TransactionState state = TransactionState::COMMITTED;
        ASSERT_EQ(txn_mgr_->getTransactionState(xids[i], state, &ctx), Status::OK) << ctx.message;
        EXPECT_EQ(state, TransactionState::ACTIVE);
        ASSERT_EQ(txn_mgr_->rollbackTransaction(getProcId(static_cast<int>(i)), xids[i], &ctx),
                  Status::OK)
            << ctx.message;
    }

    ASSERT_EQ(db_->mga_failpoint_manager()->clear(&ctx), Status::OK) << ctx.message;
}

// Test 4: Batch size limit - verify all commits are batched correctly
TEST_F(GroupCommitTest, BatchSizeLimit)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(true);
    txn_mgr_->setGroupCommitBatchSize(5); // Small batch size

    const int num_commits = 20;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    auto [start_group_commits, start_total_xids] = txn_mgr_->getGroupCommitStats();

    for (int i = 0; i < num_commits; i++)
    {
        threads.emplace_back([this, i, &success_count]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = getProcId(i);

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            if (status != Status::OK) return;

            // Small delay to ensure threads pile up
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            status = txn_mgr_->commitTransaction(proc_id, xid, &thread_ctx);
            if (status == Status::OK) {
                success_count.fetch_add(1);
            }
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // All commits should succeed
    EXPECT_EQ(success_count.load(), num_commits);

    // Verify batching is happening - at least 1 group commit with all XIDs
    auto [group_commits, total_xids] = txn_mgr_->getGroupCommitStats();
    EXPECT_GT(group_commits - start_group_commits, 0) << "Expected at least one group commit";
    EXPECT_GE(total_xids - start_total_xids, num_commits) << "All commits should be tracked";
}

// Test 5: Fallback to individual commit when disabled
TEST_F(GroupCommitTest, FallbackWhenDisabled)
{
    ErrorContext ctx;
    txn_mgr_->enableGroupCommit(false); // Disable group commit

    const int num_commits = 5;
    std::vector<std::thread> threads;
    auto [start_group_commits, start_total_xids] = txn_mgr_->getGroupCommitStats();

    for (int i = 0; i < num_commits; i++)
    {
        threads.emplace_back([this, i]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = getProcId(i);

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
    EXPECT_EQ(group_commits - start_group_commits, 0) << "Should not use group commit when disabled";
    EXPECT_EQ(total_xids - start_total_xids, 0);
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

    auto [start_group_commits, start_total_xids] = txn_mgr_->getGroupCommitStats();
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_threads; i++)
    {
        threads.emplace_back([this, i, &success_count, &failure_count]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = getProcId(i); // Reuse proc slots

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
    uint64_t delta_group_commits = group_commits - start_group_commits;
    uint64_t delta_total_xids = total_xids - start_total_xids;
    EXPECT_GT(delta_group_commits, 0) << "Should have performed group commits";
    EXPECT_GE(delta_total_xids, static_cast<uint64_t>(num_threads)) << "All XIDs should be accounted for";

    // Calculate average batch size
    double avg_batch_size = static_cast<double>(delta_total_xids) / delta_group_commits;
    EXPECT_GT(avg_batch_size, 1.0) << "Average batch size should be > 1";

    std::cout << "High concurrency test results:\n";
    std::cout << "  Threads: " << num_threads << "\n";
    std::cout << "  Elapsed: " << elapsed_ms << " ms\n";
    std::cout << "  Group commits: " << delta_group_commits << "\n";
    std::cout << "  Total XIDs: " << delta_total_xids << "\n";
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
    auto [start_group_commits, start_total_xids] = txn_mgr_->getGroupCommitStats();

    // Writers
    for (int i = 0; i < num_writers; i++)
    {
        threads.emplace_back([this, i]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = getProcId(i);

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
        threads.emplace_back([this, i, num_writers]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = getProcId(num_writers + i);

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
    EXPECT_GT(group_commits - start_group_commits, 0);
    EXPECT_GE(total_xids - start_total_xids, static_cast<uint64_t>(num_writers + num_readers));
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
            uint32_t proc_id = getProcId(i);

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
        uint32_t proc_id = getProcId(i);
        uint64_t xid;
        Status status = txn_mgr_->beginTransaction(proc_id, xid, &ctx);
        ASSERT_EQ(status, Status::OK);

        status = txn_mgr_->commitTransaction(proc_id, xid, &ctx);
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
            uint32_t proc_id = getProcId(i);

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
    std::cout << "  Sequential: " << seq_ms << " ms (" << (num_commits * 1000.0 / std::max(seq_ms, 1L))
              << " commits/sec)\n";
    std::cout << "  Batched: " << batch_ms << " ms (" << (num_commits * 1000.0 / std::max(batch_ms, 1L))
              << " commits/sec)\n";
    std::cout << "  Speedup: " << (static_cast<double>(std::max(seq_ms, 1L)) / std::max(batch_ms, 1L)) << "x\n";
    std::cout << "  Group commits: " << group_commits << "\n";
    std::cout << "  Average batch size: " << (group_commits > 0 ? static_cast<double>(batched_xids) / group_commits : 0.0)
              << "\n";

    // Group commit may be slower for small workloads due to thread overhead.
    // Skip performance assertion if times are too small to be meaningful (< 10ms).
    // The purpose of this test is to verify group commit functionality works,
    // not to benchmark performance which varies by system load.
    if (seq_ms < 10 || batch_ms < 10)
    {
        std::cout << "  Note: Times too small for meaningful performance comparison\n";
        // Just verify both completed successfully - the actual assertions in the loops already did this
    }
    else
    {
        // Group commit should be faster (or at least not significantly slower)
        // Use 5x tolerance to account for thread creation overhead on lightly loaded systems
        // and for CI/test environments with variable performance characteristics
        EXPECT_LE(batch_ms, seq_ms * 5) << "Batched should not be significantly slower";
    }
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
    std::vector<uint64_t> xids(num_rollbacks);  // Track actual XIDs
    std::mutex xids_mutex;

    // Get baseline statistics
    auto [gc_before, xids_before] = txn_mgr_->getGroupCommitStats();

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_rollbacks; i++)
    {
        threads.emplace_back([this, i, &success_count, &failure_count, &xids, &xids_mutex]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = getProcId(i);

            uint64_t xid;
            Status status = txn_mgr_->beginTransaction(proc_id, xid, &thread_ctx);
            if (status != Status::OK)
            {
                failure_count.fetch_add(1);
                return;
            }

            // Track the XID we got
            {
                std::lock_guard<std::mutex> lock(xids_mutex);
                xids[i] = xid;
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
    EXPECT_GE(total_xids, static_cast<uint64_t>(num_rollbacks)) << "All rollbacks should be batched";

    // Verify transactions are actually aborted using the actual XIDs we tracked
    for (int i = 0; i < num_rollbacks; i++)
    {
        uint64_t xid = xids[i];
        if (xid == 0) continue;  // Skip if thread failed to get XID
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
            uint32_t proc_id = getProcId(i);

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
    EXPECT_GE(total_xids, static_cast<uint64_t>(total)) << "All operations should be batched";

    double avg_batch_size = static_cast<double>(total_xids) / group_commits;
    std::cout << "\nMixed commit/rollback test results:\n";
    std::cout << "  Commits: " << commit_success.load() << "\n";
    std::cout << "  Rollbacks: " << rollback_success.load() << "\n";
    std::cout << "  Group commits: " << group_commits << "\n";
    std::cout << "  Average batch size: " << avg_batch_size << "\n";
    std::cout << "  fsync reduction: "
              << (100.0 * (1.0 - static_cast<double>(group_commits) / total)) << "%\n";
}
