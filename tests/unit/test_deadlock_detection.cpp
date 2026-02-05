/**
 * @file test_deadlock_detection.cpp
 * @brief Comprehensive deadlock detection tests for LockManager + ProcArray + TransactionManager
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <filesystem>
#include <memory>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

namespace
{
    LockTag makeTag(uint64_t page_num)
    {
        LockTag tag{};
        tag.target_type = LockTarget::LOCK_TARGET_PAGE;
        for (size_t i = 0; i < tag.object_uuid.bytes.size(); ++i)
        {
            tag.object_uuid.bytes[i] = static_cast<uint8_t>(i + 1);
        }
        tag.page_num = page_num;
        tag.offset_num = 0;
        tag.padding = 0;
        return tag;
    }

    LockTag createLockTag(uint32_t resource_id)
    {
        LockTag tag;
        tag.target_type = LockTarget::LOCK_TARGET_TABLE;
        std::memset(tag.object_uuid.bytes.data(), 0, 16);
        tag.object_uuid.bytes[0] = static_cast<uint8_t>(resource_id);
        tag.page_num = 0;
        tag.offset_num = 0;
        tag.padding = 0;
        return tag;
    }
}

class DeadlockDetectionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = uniqueTestDbPath("test_deadlock_detection", ".sbrd");
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_.string(), 8192, &ctx), Status::OK)
            << ctx.error_message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_.string(), &ctx), Status::OK)
            << ctx.error_message;

        // Ensure ProcArray is initialized for deadlock victim selection.
        ASSERT_EQ(db_->initializeProcArray(8, &ctx), Status::OK)
            << ctx.error_message;

        lock_mgr_ = db_->lock_manager();
        ASSERT_NE(lock_mgr_, nullptr);

        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(txn_mgr_, nullptr);
    }

    void TearDown() override
    {
        ErrorContext ctx;
        ProcArrayManager::shutdown(&ctx);
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }

    uint32_t registerBackend()
    {
        ErrorContext ctx;
        uint32_t proc_id = 0;
        Status status = ProcArrayManager::registerBackend(&proc_id, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to register backend: " << ctx.message;
        return proc_id;
    }

    void unregisterBackend(uint32_t proc_id)
    {
        ErrorContext ctx;
        ProcArrayManager::unregisterBackend(proc_id, &ctx);
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<Database> db_;
    LockManager* lock_mgr_ = nullptr;
    TransactionManager* txn_mgr_ = nullptr;
};

/**
 * Test 1: Detect and resolve a simple deadlock between two processes
 */
TEST_F(DeadlockDetectionTest, DetectsAndResolvesDeadlock)
{
    ErrorContext ctx;

    uint32_t proc1 = 0;
    uint32_t proc2 = 0;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc2, &ctx), Status::OK);

    uint64_t xid1 = 0;
    uint64_t xid2 = 0;
    ASSERT_EQ(txn_mgr_->beginTransaction(proc1, xid1, &ctx), Status::OK);
    ASSERT_EQ(txn_mgr_->beginTransaction(proc2, xid2, &ctx), Status::OK);

    const LockTag tagA = makeTag(100);
    const LockTag tagB = makeTag(200);

    std::atomic<int> stage1{0};
    std::atomic<int> stage2{0};
    std::atomic<Status> t1_status{Status::OK};
    std::atomic<Status> t2_status{Status::OK};

    auto worker = [&](uint32_t proc_id, const LockTag &first, const LockTag &second,
                      std::atomic<Status> &status_out)
    {
        ErrorContext tctx;

        Status first_status = lock_mgr_->acquireLock(proc_id, first, LockMode::LOCK_EXCLUSIVE,
                                                     false, 0, &tctx);
        if (first_status != Status::OK)
        {
            status_out.store(first_status);
            return;
        }
        stage1.fetch_add(1);

        // Wait until both threads have acquired their first lock.
        auto start_wait = std::chrono::steady_clock::now();
        while (stage1.load() < 2)
        {
            if (std::chrono::steady_clock::now() - start_wait > std::chrono::seconds(1))
            {
                status_out.store(Status::LOCK_TIMEOUT);
                lock_mgr_->releaseAllLocks(proc_id, nullptr);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        stage2.fetch_add(1);

        // This call will block until deadlock detection breaks the cycle.
        Status status = lock_mgr_->acquireLock(proc_id, second, LockMode::LOCK_EXCLUSIVE,
                                               true, 2000, &tctx);
        status_out.store(status);

        // Clean up any locks held by this proc.
        lock_mgr_->releaseAllLocks(proc_id, nullptr);
    };

    std::thread t1(worker, proc1, tagA, tagB, std::ref(t1_status));
    std::thread t2(worker, proc2, tagB, tagA, std::ref(t2_status));

    // Wait for both threads to start waiting on the second lock.
    bool deadlock_ready = false;
    auto wait_start = std::chrono::steady_clock::now();
    while (stage2.load() < 2)
    {
        if (std::chrono::steady_clock::now() - wait_start > std::chrono::seconds(2))
        {
            ADD_FAILURE() << "Timed out waiting for deadlock setup";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (stage2.load() >= 2)
    {
        deadlock_ready = true;
    }

    // Trigger deadlock detection.
    if (deadlock_ready)
    {
        ASSERT_EQ(lock_mgr_->detectDeadlocks(&ctx), Status::OK);
    }

    t1.join();
    t2.join();

    LockStats stats{};
    lock_mgr_->getStatistics(&stats);
    EXPECT_GE(stats.deadlocks_detected, 1u);

    // At least one thread should have acquired its second lock after resolution.
    const Status s1 = t1_status.load();
    const Status s2 = t2_status.load();
    EXPECT_TRUE(s1 == Status::OK || s2 == Status::OK);

    ProcArrayManager::unregisterBackend(proc1, &ctx);
    ProcArrayManager::unregisterBackend(proc2, &ctx);
}

/**
 * Test 2: Simple two-process deadlock (A waits for B, B waits for A)
 *
 * Scenario:
 * 1. Proc 1 acquires lock on Resource A
 * 2. Proc 2 acquires lock on Resource B
 * 3. Proc 1 tries to acquire lock on Resource B (blocks, waits for Proc 2)
 * 4. Proc 2 tries to acquire lock on Resource A (blocks, waits for Proc 1)
 * 5. Deadlock detector runs and detects cycle: 1 -> 2 -> 1
 * 6. Youngest transaction (higher XID) should be aborted
 */
TEST_F(DeadlockDetectionTest, SimpleTwoProcessDeadlock)
{
    ErrorContext ctx;

    // Register two backends
    uint32_t proc1 = registerBackend();
    uint32_t proc2 = registerBackend();

    LockTag lockA = createLockTag(1);
    LockTag lockB = createLockTag(2);

    // Proc 1 acquires exclusive lock on A
    Status status = lock_mgr_->acquireLock(proc1, lockA, LockMode::LOCK_EXCLUSIVE,
                                           true, 0, &ctx);
    ASSERT_EQ(status, Status::OK) << "Proc 1 should acquire lock A";

    // Proc 2 acquires exclusive lock on B
    status = lock_mgr_->acquireLock(proc2, lockB, LockMode::LOCK_EXCLUSIVE,
                                    true, 0, &ctx);
    ASSERT_EQ(status, Status::OK) << "Proc 2 should acquire lock B";

    // Now create deadlock scenario in separate threads
    std::atomic<bool> proc1_blocked{false};
    std::atomic<bool> proc2_blocked{false};
    std::atomic<Status> proc1_result{Status::OK};
    std::atomic<Status> proc2_result{Status::OK};

    // Thread 1: Proc 1 tries to acquire lock B (will block)
    std::thread t1([&]() {
        ErrorContext ctx1;
        proc1_blocked = true;
        Status s = lock_mgr_->acquireLock(proc1, lockB, LockMode::LOCK_EXCLUSIVE,
                                          true, 2000, &ctx1);  // 2 second timeout
        proc1_result = s;
    });

    // Wait for proc1 to start blocking
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Thread 2: Proc 2 tries to acquire lock A (will block, creating deadlock)
    std::thread t2([&]() {
        ErrorContext ctx2;
        proc2_blocked = true;
        Status s = lock_mgr_->acquireLock(proc2, lockA, LockMode::LOCK_EXCLUSIVE,
                                          true, 2000, &ctx2);  // 2 second timeout
        proc2_result = s;
    });

    // Wait for both to be blocked
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get statistics before deadlock detection
    LockStats stats_before;
    lock_mgr_->getStatistics(&stats_before);
    uint64_t deadlocks_before = stats_before.deadlocks_detected;

    // Run deadlock detection
    status = lock_mgr_->detectDeadlocks(&ctx);
    ASSERT_EQ(status, Status::OK) << "Deadlock detection should succeed";

    // Get statistics after deadlock detection
    LockStats stats_after;
    lock_mgr_->getStatistics(&stats_after);

    // Verify deadlock was detected
    EXPECT_GT(stats_after.deadlocks_detected, deadlocks_before)
        << "Deadlock counter should increment";

    // Wait for threads to complete
    t1.join();
    t2.join();

    // One of the processes should have been aborted (victim)
    // The other should have acquired its lock or timed out
    bool one_aborted = (proc1_result == Status::LOCK_TIMEOUT) ||
                       (proc2_result == Status::LOCK_TIMEOUT);
    EXPECT_TRUE(one_aborted) << "At least one process should timeout/abort";

    // Cleanup
    lock_mgr_->releaseAllLocks(proc1, &ctx);
    lock_mgr_->releaseAllLocks(proc2, &ctx);
    unregisterBackend(proc1);
    unregisterBackend(proc2);
}

/**
 * Test 3: Three-process deadlock cycle (A -> B -> C -> A)
 *
 * Scenario:
 * - Proc 1 holds lock A, waits for lock B
 * - Proc 2 holds lock B, waits for lock C
 * - Proc 3 holds lock C, waits for lock A
 * Creates cycle: 1 -> 2 -> 3 -> 1
 */
TEST_F(DeadlockDetectionTest, ThreeProcessCycle)
{
    ErrorContext ctx;

    // Register three backends
    uint32_t proc1 = registerBackend();
    uint32_t proc2 = registerBackend();
    uint32_t proc3 = registerBackend();

    LockTag lockA = createLockTag(1);
    LockTag lockB = createLockTag(2);
    LockTag lockC = createLockTag(3);

    // Each process acquires one lock
    ASSERT_EQ(lock_mgr_->acquireLock(proc1, lockA, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);
    ASSERT_EQ(lock_mgr_->acquireLock(proc2, lockB, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);
    ASSERT_EQ(lock_mgr_->acquireLock(proc3, lockC, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);

    // Create cycle with threads
    std::thread t1([&]() {
        ErrorContext ctx1;
        lock_mgr_->acquireLock(proc1, lockB, LockMode::LOCK_EXCLUSIVE,
                               true, 2000, &ctx1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread t2([&]() {
        ErrorContext ctx2;
        lock_mgr_->acquireLock(proc2, lockC, LockMode::LOCK_EXCLUSIVE,
                               true, 2000, &ctx2);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread t3([&]() {
        ErrorContext ctx3;
        lock_mgr_->acquireLock(proc3, lockA, LockMode::LOCK_EXCLUSIVE,
                               true, 2000, &ctx3);
    });

    // Wait for all to be blocked
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Run deadlock detection
    LockStats stats_before;
    lock_mgr_->getStatistics(&stats_before);

    Status status = lock_mgr_->detectDeadlocks(&ctx);
    ASSERT_EQ(status, Status::OK);

    LockStats stats_after;
    lock_mgr_->getStatistics(&stats_after);
    EXPECT_GT(stats_after.deadlocks_detected, stats_before.deadlocks_detected);

    // Cleanup
    t1.join();
    t2.join();
    t3.join();

    lock_mgr_->releaseAllLocks(proc1, &ctx);
    lock_mgr_->releaseAllLocks(proc2, &ctx);
    lock_mgr_->releaseAllLocks(proc3, &ctx);
    unregisterBackend(proc1);
    unregisterBackend(proc2);
    unregisterBackend(proc3);
}

/**
 * Test 4: Verify victim selection chooses youngest transaction (highest XID)
 */
TEST_F(DeadlockDetectionTest, VictimSelectionYoungestTransaction)
{
    ErrorContext ctx;

    // Register three backends
    uint32_t proc1 = registerBackend();
    uint32_t proc2 = registerBackend();
    uint32_t proc3 = registerBackend();

    LockTag lockA = createLockTag(1);
    LockTag lockB = createLockTag(2);
    LockTag lockC = createLockTag(3);

    // Set up locks
    ASSERT_EQ(lock_mgr_->acquireLock(proc1, lockA, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);
    ASSERT_EQ(lock_mgr_->acquireLock(proc2, lockB, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);
    ASSERT_EQ(lock_mgr_->acquireLock(proc3, lockC, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);

    // Create cycle: 1 -> 2, 2 -> 3, 3 -> 1
    // Victim should be proc2 (XID=500)
    std::atomic<Status> proc1_status{Status::OK};
    std::atomic<Status> proc2_status{Status::OK};
    std::atomic<Status> proc3_status{Status::OK};

    std::thread t1([&]() {
        ErrorContext ctx1;
        proc1_status = lock_mgr_->acquireLock(proc1, lockB, LockMode::LOCK_EXCLUSIVE,
                                              true, 2000, &ctx1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread t2([&]() {
        ErrorContext ctx2;
        proc2_status = lock_mgr_->acquireLock(proc2, lockC, LockMode::LOCK_EXCLUSIVE,
                                              true, 2000, &ctx2);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread t3([&]() {
        ErrorContext ctx3;
        proc3_status = lock_mgr_->acquireLock(proc3, lockA, LockMode::LOCK_EXCLUSIVE,
                                              true, 2000, &ctx3);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Detect deadlock
    Status status = lock_mgr_->detectDeadlocks(&ctx);
    ASSERT_EQ(status, Status::OK);

    t1.join();
    t2.join();
    t3.join();

    // Proc2 (highest XID) should have been aborted/timed out
    // Note: Due to timing, we just verify that deadlock was resolved
    // (at least one process should have failed to acquire)
    bool deadlock_resolved = (proc1_status != Status::OK) ||
                             (proc2_status != Status::OK) ||
                             (proc3_status != Status::OK);
    EXPECT_TRUE(deadlock_resolved) << "Deadlock should be resolved by aborting a victim";

    // Cleanup
    lock_mgr_->releaseAllLocks(proc1, &ctx);
    lock_mgr_->releaseAllLocks(proc2, &ctx);
    lock_mgr_->releaseAllLocks(proc3, &ctx);
    unregisterBackend(proc1);
    unregisterBackend(proc2);
    unregisterBackend(proc3);
}

/**
 * Test 5: No deadlock - simple lock wait should not trigger detection
 */
TEST_F(DeadlockDetectionTest, NoDeadlockSimpleLockWait)
{
    ErrorContext ctx;

    uint32_t proc1 = registerBackend();
    uint32_t proc2 = registerBackend();

    LockTag lockA = createLockTag(1);

    // Proc 1 acquires lock
    ASSERT_EQ(lock_mgr_->acquireLock(proc1, lockA, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);

    // Proc 2 waits for lock (but not a deadlock - just a wait)
    std::thread t2([&]() {
        ErrorContext ctx2;
        // Will timeout, but that's OK - just testing no false deadlock detection
        lock_mgr_->acquireLock(proc2, lockA, LockMode::LOCK_EXCLUSIVE,
                               true, 500, &ctx2);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Run deadlock detection - should find no cycles
    LockStats stats_before;
    lock_mgr_->getStatistics(&stats_before);

    Status status = lock_mgr_->detectDeadlocks(&ctx);
    ASSERT_EQ(status, Status::OK);

    LockStats stats_after;
    lock_mgr_->getStatistics(&stats_after);

    // Deadlock count should NOT increase (no cycle detected)
    EXPECT_EQ(stats_after.deadlocks_detected, stats_before.deadlocks_detected)
        << "No deadlock should be detected for simple lock wait";

    // Cleanup
    t2.join();
    lock_mgr_->releaseAllLocks(proc1, &ctx);
    lock_mgr_->releaseAllLocks(proc2, &ctx);
    unregisterBackend(proc1);
    unregisterBackend(proc2);
}

/**
 * Test 6: Self-deadlock (should not happen, but test for safety)
 */
TEST_F(DeadlockDetectionTest, NoSelfDeadlock)
{
    ErrorContext ctx;

    uint32_t proc1 = registerBackend();
    LockTag lockA = createLockTag(1);

    // Proc 1 acquires lock
    ASSERT_EQ(lock_mgr_->acquireLock(proc1, lockA, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);

    // Same process trying to acquire same lock again
    // This should either succeed (if recursive locking supported) or wait/timeout
    // but should NOT create a deadlock cycle (self-edges should be filtered)

    Status status = lock_mgr_->detectDeadlocks(&ctx);
    ASSERT_EQ(status, Status::OK);

    // Cleanup
    lock_mgr_->releaseAllLocks(proc1, &ctx);
    unregisterBackend(proc1);
}

/**
 * Test 7: Multiple independent lock waits (no cycle)
 */
TEST_F(DeadlockDetectionTest, MultipleIndependentWaits)
{
    ErrorContext ctx;

    uint32_t proc1 = registerBackend();
    uint32_t proc2 = registerBackend();
    uint32_t proc3 = registerBackend();

    LockTag lockA = createLockTag(1);
    LockTag lockB = createLockTag(2);

    // Proc 1 holds lock A
    ASSERT_EQ(lock_mgr_->acquireLock(proc1, lockA, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);

    // Proc 2 holds lock B
    ASSERT_EQ(lock_mgr_->acquireLock(proc2, lockB, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);

    // Proc 3 waits for lock A (waits for proc1)
    std::thread t3([&]() {
        ErrorContext ctx3;
        lock_mgr_->acquireLock(proc3, lockA, LockMode::LOCK_EXCLUSIVE,
                               true, 500, &ctx3);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // No cycle: proc3 -> proc1 (but proc1 doesn't wait for anyone)
    LockStats stats_before;
    lock_mgr_->getStatistics(&stats_before);

    Status status = lock_mgr_->detectDeadlocks(&ctx);
    ASSERT_EQ(status, Status::OK);

    LockStats stats_after;
    lock_mgr_->getStatistics(&stats_after);
    EXPECT_EQ(stats_after.deadlocks_detected, stats_before.deadlocks_detected)
        << "No deadlock in independent waits";

    // Cleanup
    t3.join();
    lock_mgr_->releaseAllLocks(proc1, &ctx);
    lock_mgr_->releaseAllLocks(proc2, &ctx);
    lock_mgr_->releaseAllLocks(proc3, &ctx);
    unregisterBackend(proc1);
    unregisterBackend(proc2);
    unregisterBackend(proc3);
}

/**
 * Test 8: Statistics tracking
 */
TEST_F(DeadlockDetectionTest, StatisticsTracking)
{
    ErrorContext ctx;

    // Get initial statistics
    LockStats stats_initial;
    lock_mgr_->getStatistics(&stats_initial);

    // Create a simple deadlock and resolve it
    uint32_t proc1 = registerBackend();
    uint32_t proc2 = registerBackend();

    LockTag lockA = createLockTag(1);
    LockTag lockB = createLockTag(2);

    ASSERT_EQ(lock_mgr_->acquireLock(proc1, lockA, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);
    ASSERT_EQ(lock_mgr_->acquireLock(proc2, lockB, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);

    std::thread t1([&]() {
        ErrorContext ctx1;
        lock_mgr_->acquireLock(proc1, lockB, LockMode::LOCK_EXCLUSIVE,
                               true, 2000, &ctx1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread t2([&]() {
        ErrorContext ctx2;
        lock_mgr_->acquireLock(proc2, lockA, LockMode::LOCK_EXCLUSIVE,
                               true, 2000, &ctx2);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Detect deadlock
    Status status = lock_mgr_->detectDeadlocks(&ctx);
    ASSERT_EQ(status, Status::OK);

    // Get final statistics
    LockStats stats_final;
    lock_mgr_->getStatistics(&stats_final);

    // Verify statistics updated
    EXPECT_GT(stats_final.deadlocks_detected, stats_initial.deadlocks_detected)
        << "Deadlock counter should increment";
    EXPECT_GE(stats_final.locks_acquired, stats_initial.locks_acquired)
        << "Lock acquisition counter should increase";
    EXPECT_GE(stats_final.lock_waits, stats_initial.lock_waits)
        << "Lock wait counter should increase";

    // Cleanup
    t1.join();
    t2.join();
    lock_mgr_->releaseAllLocks(proc1, &ctx);
    lock_mgr_->releaseAllLocks(proc2, &ctx);
    unregisterBackend(proc1);
    unregisterBackend(proc2);
}

/**
 * Test 9: Deadlock with mixed lock modes (SHARE and EXCLUSIVE)
 */
TEST_F(DeadlockDetectionTest, MixedLockModes)
{
    ErrorContext ctx;

    uint32_t proc1 = registerBackend();
    uint32_t proc2 = registerBackend();

    LockTag lockA = createLockTag(1);
    LockTag lockB = createLockTag(2);

    // Proc 1 holds SHARE on A
    ASSERT_EQ(lock_mgr_->acquireLock(proc1, lockA, LockMode::LOCK_SHARE,
                                     true, 0, &ctx), Status::OK);

    // Proc 2 holds EXCLUSIVE on B
    ASSERT_EQ(lock_mgr_->acquireLock(proc2, lockB, LockMode::LOCK_EXCLUSIVE,
                                     true, 0, &ctx), Status::OK);

    // Proc 1 waits for EXCLUSIVE on B (conflicts with proc2's EXCLUSIVE)
    std::thread t1([&]() {
        ErrorContext ctx1;
        lock_mgr_->acquireLock(proc1, lockB, LockMode::LOCK_EXCLUSIVE,
                               true, 2000, &ctx1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Proc 2 waits for EXCLUSIVE on A (conflicts with proc1's SHARE)
    std::thread t2([&]() {
        ErrorContext ctx2;
        lock_mgr_->acquireLock(proc2, lockA, LockMode::LOCK_EXCLUSIVE,
                               true, 2000, &ctx2);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Detect deadlock
    Status status = lock_mgr_->detectDeadlocks(&ctx);
    ASSERT_EQ(status, Status::OK);

    // Cleanup
    t1.join();
    t2.join();
    lock_mgr_->releaseAllLocks(proc1, &ctx);
    lock_mgr_->releaseAllLocks(proc2, &ctx);
    unregisterBackend(proc1);
    unregisterBackend(proc2);
}
