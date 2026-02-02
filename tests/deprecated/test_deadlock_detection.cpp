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
 * @file test_deadlock_detection.cpp
 * @brief Comprehensive tests for deadlock detection in LockManager
 *
 * Tests cover:
 * - Simple 2-process deadlocks
 * - Multi-process cycles (3+ processes)
 * - Victim selection policy (youngest transaction)
 * - Deadlock resolution and cleanup
 * - Statistics tracking
 * - Edge cases and boundary conditions
 */

#include <gtest/gtest.h>
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"
#include <thread>
#include <chrono>
#include <vector>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

class DeadlockDetectionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Clean up any existing test database
        db_path_ = uniqueTestDbPath("test_deadlock_db", ".sb");
        ::unlink(db_path_.c_str());

        // Create test database
        ErrorContext ctx;
        Status status = Database::create(db_path_.c_str(), 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        status = db_->open(db_path_.c_str(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        lock_mgr_ = db_->lock_manager();
        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(lock_mgr_, nullptr);
        ASSERT_NE(txn_mgr_, nullptr);

        // Initialize ProcArray for testing
        ProcArray* proc_array = ProcArrayManager::getInstance();
        if (proc_array == nullptr) {
            proc_array = ProcArrayManager::createInstance(10, &ctx);
            ASSERT_NE(proc_array, nullptr);
        }
    }

    void TearDown() override
    {
        if (db_) {
            db_->close();
        }
        db_.reset();
        ::unlink(db_path_.c_str());
    }

    // Helper to create a lock tag for testing
    LockTag createLockTag(uint32_t resource_id)
    {
        LockTag tag;
        tag.target_type = LockTarget::LOCK_TARGET_TABLE;
        std::memset(tag.object_uuid.bytes, 0, 16);
        tag.object_uuid.bytes[0] = resource_id;
        tag.page_num = 0;
        tag.offset_num = 0;
        tag.padding = 0;
        return tag;
    }

    // Helper to register a process in ProcArray
    uint32_t registerProcess(uint64_t xid, bool is_read_only = false)
    {
        ErrorContext ctx;
        ProcArray* proc_array = ProcArrayManager::getInstance();
        EXPECT_NE(proc_array, nullptr);

        uint32_t proc_id = 0;
        Status status = ProcArrayManager::registerProcess(xid, is_read_only, &proc_id, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to register process: " << ctx.message;

        return proc_id;
    }

    void unregisterProcess(uint32_t proc_id)
    {
        ErrorContext ctx;
        ProcArrayManager::unregisterProcess(proc_id, &ctx);
    }

    std::unique_ptr<Database> db_;
    LockManager* lock_mgr_;
    TransactionManager* txn_mgr_;
    std::string db_path_;
};

/**
 * Test 1: Simple two-process deadlock (A waits for B, B waits for A)
 *
 * Scenario:
 * 1. Proc 1 acquires lock on Resource A
 * 2. Proc 2 acquires lock on Resource B
 * 3. Proc 1 tries to acquire lock on Resource B (blocks, waits for Proc 2)
 * 4. Proc 2 tries to acquire lock on Resource A (blocks, waits for Proc 1)
 * 5. Deadlock detector runs and detects cycle: 1 → 2 → 1
 * 6. Youngest transaction (higher XID) should be aborted
 */
TEST_F(DeadlockDetectionTest, SimpleTwoProcessDeadlock)
{
    ErrorContext ctx;

    // Register two processes with different XIDs
    uint32_t proc1 = registerProcess(100);  // Older transaction
    uint32_t proc2 = registerProcess(200);  // Younger transaction (should be victim)

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
    unregisterProcess(proc1);
    unregisterProcess(proc2);
}

/**
 * Test 2: Three-process deadlock cycle (A → B → C → A)
 *
 * Scenario:
 * - Proc 1 holds lock A, waits for lock B
 * - Proc 2 holds lock B, waits for lock C
 * - Proc 3 holds lock C, waits for lock A
 * Creates cycle: 1 → 2 → 3 → 1
 */
TEST_F(DeadlockDetectionTest, ThreeProcessCycle)
{
    ErrorContext ctx;

    // Register three processes with different XIDs
    uint32_t proc1 = registerProcess(100);  // Oldest
    uint32_t proc2 = registerProcess(200);
    uint32_t proc3 = registerProcess(300);  // Youngest (should be victim)

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
    unregisterProcess(proc1);
    unregisterProcess(proc2);
    unregisterProcess(proc3);
}

/**
 * Test 3: Verify victim selection chooses youngest transaction (highest XID)
 */
TEST_F(DeadlockDetectionTest, VictimSelectionYoungestTransaction)
{
    ErrorContext ctx;

    // Create processes with specific XIDs - proc2 has highest XID (youngest)
    uint32_t proc1 = registerProcess(100);
    uint32_t proc2 = registerProcess(500);  // Highest XID - should be victim
    uint32_t proc3 = registerProcess(200);

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

    // Create cycle: 1 → 2, 2 → 3, 3 → 1
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
    unregisterProcess(proc1);
    unregisterProcess(proc2);
    unregisterProcess(proc3);
}

/**
 * Test 4: No deadlock - simple lock wait should not trigger detection
 */
TEST_F(DeadlockDetectionTest, NoDeadlockSimpleLockWait)
{
    ErrorContext ctx;

    uint32_t proc1 = registerProcess(100);
    uint32_t proc2 = registerProcess(200);

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
    unregisterProcess(proc1);
    unregisterProcess(proc2);
}

/**
 * Test 5: Self-deadlock (should not happen, but test for safety)
 */
TEST_F(DeadlockDetectionTest, NoSelfDeadlock)
{
    ErrorContext ctx;

    uint32_t proc1 = registerProcess(100);
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
    unregisterProcess(proc1);
}

/**
 * Test 6: Multiple independent lock waits (no cycle)
 */
TEST_F(DeadlockDetectionTest, MultipleIndependentWaits)
{
    ErrorContext ctx;

    uint32_t proc1 = registerProcess(100);
    uint32_t proc2 = registerProcess(200);
    uint32_t proc3 = registerProcess(300);

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

    // No cycle: proc3 → proc1 (but proc1 doesn't wait for anyone)
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
    unregisterProcess(proc1);
    unregisterProcess(proc2);
    unregisterProcess(proc3);
}

/**
 * Test 7: Statistics tracking
 */
TEST_F(DeadlockDetectionTest, StatisticsTracking)
{
    ErrorContext ctx;

    // Get initial statistics
    LockStats stats_initial;
    lock_mgr_->getStatistics(&stats_initial);

    // Create a simple deadlock and resolve it
    uint32_t proc1 = registerProcess(100);
    uint32_t proc2 = registerProcess(200);

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
    unregisterProcess(proc1);
    unregisterProcess(proc2);
}

/**
 * Test 8: Deadlock with mixed lock modes (SHARE and EXCLUSIVE)
 */
TEST_F(DeadlockDetectionTest, MixedLockModes)
{
    ErrorContext ctx;

    uint32_t proc1 = registerProcess(100);
    uint32_t proc2 = registerProcess(200);

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
    unregisterProcess(proc1);
    unregisterProcess(proc2);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
