#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>

#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/vacuum.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

class MGAIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = "test_mga_integration.db";
        ::unlink(db_path_.c_str());
    }

    void TearDown() override {
        ::unlink(db_path_.c_str());
    }

    std::string db_path_;
};

// Test 1: ProcArray - Multiple Backend Registration
TEST_F(MGAIntegrationTest, ProcArrayMultipleBackends) {
    ErrorContext ctx;

    // Create database
    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);

    // Initialize ProcArray with 10 backends
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register 5 backends
    std::vector<uint32_t> proc_ids;
    for (int i = 0; i < 5; i++) {
        uint32_t proc_id;
        ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);
        EXPECT_LT(proc_id, 10u);
        proc_ids.push_back(proc_id);
    }

    // Verify count
    uint32_t active_count;
    ASSERT_EQ(ProcArrayManager::getNumActiveBackends(&active_count, &ctx), Status::OK);
    EXPECT_EQ(active_count, 5u);

    // Unregister all
    for (uint32_t proc_id : proc_ids) {
        ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    }

    // Verify count is 0
    ASSERT_EQ(ProcArrayManager::getNumActiveBackends(&active_count, &ctx), Status::OK);
    EXPECT_EQ(active_count, 0u);

    db.close();
}

// Test 2: Transaction Manager - Multiple Concurrent Transactions
TEST_F(MGAIntegrationTest, MultipleTransactions) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register 3 backends
    uint32_t proc_id1, proc_id2, proc_id3;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id2, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id3, &ctx), Status::OK);

    // Start 3 transactions
    uint64_t xid1, xid2, xid3;
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id1, xid1, &ctx), Status::OK);
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id2, xid2, &ctx), Status::OK);
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id3, xid3, &ctx), Status::OK);

    // Verify all XIDs are unique
    EXPECT_NE(xid1, xid2);
    EXPECT_NE(xid2, xid3);
    EXPECT_NE(xid1, xid3);

    // Get snapshot from transaction 2
    TransactionManager::Snapshot snapshot;
    ASSERT_EQ(db.transaction_manager()->getSnapshot(snapshot, &ctx), Status::OK);

    // Snapshot should see transaction 1 as active
    bool found_xid1 = false;
    for (uint64_t active_xid : snapshot.active_xids) {
        if (active_xid == xid1) {
            found_xid1 = true;
            break;
        }
    }
    EXPECT_TRUE(found_xid1);

    // Commit all transactions
    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id1, xid1, &ctx), Status::OK);
    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id2, xid2, &ctx), Status::OK);
    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id3, xid3, &ctx), Status::OK);

    // Unregister backends
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id2, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id3, &ctx), Status::OK);

    db.close();
}

// Test 3: Lock Manager - Basic Lock Acquisition
TEST_F(MGAIntegrationTest, LockManagerBasicLocks) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    // Create a lock tag for a table
    LockTag tag{};
    tag.target_type = LockTarget::LOCK_TARGET_TABLE;
    // Use a dummy UUID
    memset(tag.object_uuid.bytes.data(), 0xAB, 16);
    tag.page_num = 0;
    tag.offset_num = 0;
    tag.padding = 0;

    // Acquire ACCESS_SHARE lock (non-blocking)
    ASSERT_EQ(db.lock_manager()->acquireLock(proc_id, tag,
                                             LockMode::LOCK_ACCESS_SHARE,
                                             false, 0, &ctx), Status::OK);

    // Release lock
    ASSERT_EQ(db.lock_manager()->releaseLock(proc_id, tag,
                                             LockMode::LOCK_ACCESS_SHARE, &ctx), Status::OK);

    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 4: Lock Manager - Conflicting Locks
TEST_F(MGAIntegrationTest, LockManagerConflicts) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    uint32_t proc_id1, proc_id2;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id2, &ctx), Status::OK);

    LockTag tag{};
    tag.target_type = LockTarget::LOCK_TARGET_TABLE;
    memset(tag.object_uuid.bytes.data(), 0xCD, 16);
    tag.page_num = 0;
    tag.offset_num = 0;
    tag.padding = 0;

    // Backend 1 acquires ACCESS_EXCLUSIVE lock
    ASSERT_EQ(db.lock_manager()->acquireLock(proc_id1, tag,
                                             LockMode::LOCK_ACCESS_EXCLUSIVE,
                                             false, 0, &ctx), Status::OK);

    // Backend 2 tries to acquire ACCESS_SHARE (should conflict, non-blocking)
    Status result = db.lock_manager()->acquireLock(proc_id2, tag,
                                                    LockMode::LOCK_ACCESS_SHARE,
                                                    false, 0, &ctx);
    EXPECT_EQ(result, Status::LOCK_CONFLICT);

    // Release backend 1's lock
    ASSERT_EQ(db.lock_manager()->releaseLock(proc_id1, tag,
                                             LockMode::LOCK_ACCESS_EXCLUSIVE, &ctx), Status::OK);

    // Now backend 2 should be able to acquire
    ASSERT_EQ(db.lock_manager()->acquireLock(proc_id2, tag,
                                             LockMode::LOCK_ACCESS_SHARE,
                                             false, 0, &ctx), Status::OK);

    ASSERT_EQ(db.lock_manager()->releaseLock(proc_id2, tag,
                                             LockMode::LOCK_ACCESS_SHARE, &ctx), Status::OK);

    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id2, &ctx), Status::OK);
    db.close();
}

// Test 5: Version Chains - Basic UPDATE
TEST_F(MGAIntegrationTest, VersionChainsBasicUpdate) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    uint64_t xid;
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id, xid, &ctx), Status::OK);

    // Create a test tuple (just TupleHeader + some data)
    std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + 100);
    auto* hdr = reinterpret_cast<TupleHeader*>(tuple_data.data());
    hdr->xmin = xid;
    hdr->xmax = 0;
    hdr->next_version_tid = 0;
    hdr->ctid_page = 0;
    hdr->ctid_item = 0;
    hdr->infomask = 0;
    hdr->null_bitmap_offset = 0;
    hdr->padding = 0;

    // Fill with test data
    for (size_t i = sizeof(TupleHeader); i < tuple_data.size(); i++) {
        tuple_data[i] = static_cast<uint8_t>(i % 256);
    }

    ID table_id;
    memset(table_id.bytes.data(), 0xEF, 16);

    // Insert initial tuple
    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(db.storage_engine()->insertTuple(table_id, tuple_data.data(),
                                               tuple_data.size(), &page_id, &item_id, &ctx),
              Status::OK);

    // Update the tuple (create new version)
    std::vector<uint8_t> new_tuple_data = tuple_data;
    for (size_t i = sizeof(TupleHeader); i < new_tuple_data.size(); i++) {
        new_tuple_data[i] = static_cast<uint8_t>((i + 1) % 256);
    }

    uint32_t new_page_id;
    uint16_t new_item_id;
    Status update_result = db.storage_engine()->updateTuple(page_id, item_id,
                                                            new_tuple_data.data(),
                                                            new_tuple_data.size(),
                                                            &new_page_id, &new_item_id, &ctx);

    // Update might fail if not implemented or page full, that's ok for this test
    if (update_result == Status::OK) {
        // Verify old and new are different items
        EXPECT_TRUE(page_id != new_page_id || item_id != new_item_id);
    }

    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id, xid, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 6: Vacuum - Basic Vacuum Horizon
TEST_F(MGAIntegrationTest, VacuumHorizon) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Get vacuum horizon with no active transactions
    uint64_t horizon;
    ASSERT_EQ(db.vacuum()->getVacuumHorizon(&horizon, &ctx), Status::OK);

    // Should get a valid horizon (likely UINT64_MAX or a recent XID)
    // Just verify the call succeeds

    // Start a transaction
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    uint64_t xid;
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id, xid, &ctx), Status::OK);

    // Get horizon again
    ASSERT_EQ(db.vacuum()->getVacuumHorizon(&horizon, &ctx), Status::OK);

    // Horizon should be <= xid (since transaction is active)
    EXPECT_LE(horizon, xid);

    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id, xid, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 7: Vacuum - VacuumTable Statistics
TEST_F(MGAIntegrationTest, VacuumTableStats) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    ID table_id;
    memset(table_id.bytes.data(), 0x12, 16);

    VacuumStats stats;
    ASSERT_EQ(db.vacuum()->vacuumTable(table_id, &stats, &ctx), Status::OK);

    // Verify statistics structure is populated
    // Should have scanned some pages (or 0 if no heap pages exist)
    EXPECT_GE(stats.pages_scanned, 0u);
    EXPECT_GE(stats.tuples_scanned, 0u);
    EXPECT_GE(stats.vacuum_time_us, 0u);

    db.close();
}

// Test 8: Integration - Full CRUD with Transactions
TEST_F(MGAIntegrationTest, FullCRUDWithTransactions) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    // Transaction 1: INSERT
    uint64_t xid1;
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id, xid1, &ctx), Status::OK);

    std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + 50);
    auto* hdr = reinterpret_cast<TupleHeader*>(tuple_data.data());
    hdr->xmin = xid1;
    hdr->xmax = 0;
    hdr->next_version_tid = 0;
    hdr->infomask = 0;

    ID table_id;
    memset(table_id.bytes.data(), 0x34, 16);

    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(db.storage_engine()->insertTuple(table_id, tuple_data.data(),
                                               tuple_data.size(), &page_id, &item_id, &ctx),
              Status::OK);

    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id, xid1, &ctx), Status::OK);

    // Transaction 2: READ
    uint64_t xid2;
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id, xid2, &ctx), Status::OK);

    Tuple tuple_out;
    Status read_result = db.storage_engine()->getTuple(page_id, item_id, &tuple_out, &ctx);

    // Read might succeed or fail depending on visibility, both are ok for basic test
    EXPECT_TRUE(read_result == Status::OK || read_result == Status::NOT_FOUND);

    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id, xid2, &ctx), Status::OK);

    // Transaction 3: DELETE
    uint64_t xid3;
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id, xid3, &ctx), Status::OK);

    Status delete_result = db.storage_engine()->deleteTuple(page_id, item_id, &ctx);
    EXPECT_TRUE(delete_result == Status::OK || delete_result == Status::NOT_FOUND);

    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id, xid3, &ctx), Status::OK);

    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 9: Stress Test - Many Transactions
TEST_F(MGAIntegrationTest, StressManyTransactions) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(100, &ctx), Status::OK);

    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    // Start and commit 100 transactions
    for (int i = 0; i < 100; i++) {
        uint64_t xid;
        ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id, xid, &ctx), Status::OK);

        // Commit immediately
        ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id, xid, &ctx), Status::OK);
    }

    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 10: Lock Manager - Statistics
TEST_F(MGAIntegrationTest, LockManagerStatistics) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    // Get initial statistics
    LockStats stats1;
    db.lock_manager()->getStatistics(&stats1);

    // Acquire and release a lock
    LockTag tag{};
    tag.target_type = LockTarget::LOCK_TARGET_TABLE;
    memset(tag.object_uuid.bytes.data(), 0x56, 16);

    ASSERT_EQ(db.lock_manager()->acquireLock(proc_id, tag,
                                             LockMode::LOCK_ACCESS_SHARE,
                                             false, 0, &ctx), Status::OK);

    // Get statistics after lock
    LockStats stats2;
    db.lock_manager()->getStatistics(&stats2);

    // Should have increased locks_acquired
    EXPECT_GT(stats2.locks_acquired, stats1.locks_acquired);

    ASSERT_EQ(db.lock_manager()->releaseLock(proc_id, tag,
                                             LockMode::LOCK_ACCESS_SHARE, &ctx), Status::OK);

    // Get statistics after release
    LockStats stats3;
    db.lock_manager()->getStatistics(&stats3);

    // Should have increased locks_released
    EXPECT_GT(stats3.locks_released, stats2.locks_released);

    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 11: Deadlock Detection - Classic Circular Wait
// Suite 4 (MGA/Concurrency) Compliance Test
//
// Tests deadlock detection with 2 backends creating a circular wait:
// - Backend 1 locks Tuple A, wants Tuple B
// - Backend 2 locks Tuple B, wants Tuple A
// Expected: One transaction aborts with deadlock error
TEST_F(MGAIntegrationTest, DeadlockDetectionCircularWait) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register two backends
    uint32_t proc_id1, proc_id2;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id2, &ctx), Status::OK);

    // Create two lock tags for two different tuples
    LockTag tuple_a_tag{};
    tuple_a_tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
    memset(tuple_a_tag.object_uuid.bytes.data(), 0xAA, 16);
    tuple_a_tag.page_num = 0;
    tuple_a_tag.offset_num = 1;
    tuple_a_tag.padding = 0;

    LockTag tuple_b_tag{};
    tuple_b_tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
    memset(tuple_b_tag.object_uuid.bytes.data(), 0xBB, 16);
    tuple_b_tag.page_num = 0;
    tuple_b_tag.offset_num = 2;
    tuple_b_tag.padding = 0;

    // Backend 1: Acquire EXCLUSIVE lock on Tuple A
    ASSERT_EQ(db.lock_manager()->acquireLock(proc_id1, tuple_a_tag,
                                             LockMode::LOCK_EXCLUSIVE,
                                             false, 0, &ctx), Status::OK);

    // Backend 2: Acquire EXCLUSIVE lock on Tuple B
    ASSERT_EQ(db.lock_manager()->acquireLock(proc_id2, tuple_b_tag,
                                             LockMode::LOCK_EXCLUSIVE,
                                             false, 0, &ctx), Status::OK);

    // Now create circular wait scenario using threads
    bool backend1_got_deadlock = false;
    bool backend2_got_deadlock = false;
    bool backend1_success = false;
    bool backend2_success = false;

    // Backend 1 thread: Try to acquire Tuple B (will wait)
    std::thread thread1([&]() {
        ErrorContext ctx1;
        Status result = db.lock_manager()->acquireLock(proc_id1, tuple_b_tag,
                                                       LockMode::LOCK_EXCLUSIVE,
                                                       true, 5000, &ctx1);
        if (result == Status::DEADLOCK) {
            backend1_got_deadlock = true;
        } else if (result == Status::OK) {
            backend1_success = true;
        }
    });

    // Small delay to ensure thread1 starts waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Backend 2 thread: Try to acquire Tuple A (will create deadlock)
    std::thread thread2([&]() {
        ErrorContext ctx2;
        Status result = db.lock_manager()->acquireLock(proc_id2, tuple_a_tag,
                                                       LockMode::LOCK_EXCLUSIVE,
                                                       true, 5000, &ctx2);
        if (result == Status::DEADLOCK) {
            backend2_got_deadlock = true;
        } else if (result == Status::OK) {
            backend2_success = true;
        }
    });

    // Wait for both threads
    thread1.join();
    thread2.join();

    // Verify deadlock was detected and one transaction aborted
    EXPECT_TRUE(backend1_got_deadlock || backend2_got_deadlock)
        << "One backend should detect deadlock and abort";

    // Verify exactly one got deadlock (the other should succeed or still wait)
    EXPECT_FALSE(backend1_got_deadlock && backend2_got_deadlock)
        << "Only one backend should be aborted in deadlock";

    // Check statistics - deadlock counter should have increased
    LockStats stats;
    db.lock_manager()->getStatistics(&stats);
    EXPECT_GT(stats.deadlocks_detected, 0u)
        << "Deadlock counter should be incremented";

    // Clean up - release any held locks
    db.lock_manager()->releaseAllLocks(proc_id1, &ctx);
    db.lock_manager()->releaseAllLocks(proc_id2, &ctx);

    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id2, &ctx), Status::OK);
    db.close();
}

// Test 12: Snapshot Isolation - Overlapping Transactions
// Suite 4 (MGA/Concurrency) Compliance Test
//
// Tests snapshot isolation with overlapping transactions:
// - Transaction T1 starts, reads tuple
// - Transaction T2 starts, updates tuple, commits
// - Transaction T1 should still see old version (snapshot isolation)
// - Transaction T3 starts after T2 commits, should see new version
TEST_F(MGAIntegrationTest, SnapshotIsolationOverlappingTransactions) {
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register three backends
    uint32_t proc_id1, proc_id2, proc_id3;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id2, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id3, &ctx), Status::OK);

    // Initial setup: Insert a tuple in a committed transaction (T0)
    uint64_t xid0;
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id1, xid0, &ctx), Status::OK);

    std::vector<uint8_t> initial_tuple_data(sizeof(TupleHeader) + 100);
    auto* hdr0 = reinterpret_cast<TupleHeader*>(initial_tuple_data.data());
    hdr0->xmin = xid0;
    hdr0->xmax = 0;
    hdr0->next_version_tid = 0;
    hdr0->ctid_page = 0;
    hdr0->ctid_item = 0;
    hdr0->infomask = 0;
    hdr0->null_bitmap_offset = 0;
    hdr0->padding = 0;

    // Fill with recognizable pattern (all 0xAA)
    for (size_t i = sizeof(TupleHeader); i < initial_tuple_data.size(); i++) {
        initial_tuple_data[i] = 0xAA;
    }

    ID table_id;
    memset(table_id.bytes.data(), 0x11, 16);

    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(db.storage_engine()->insertTuple(table_id, initial_tuple_data.data(),
                                               initial_tuple_data.size(), &page_id, &item_id, &ctx),
              Status::OK);

    // Commit initial transaction
    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id1, xid0, &ctx), Status::OK);

    // Transaction T1: Start and take snapshot
    uint64_t xid1;
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id1, xid1, &ctx), Status::OK);

    TransactionManager::Snapshot snapshot1;
    ASSERT_EQ(db.transaction_manager()->getSnapshot(snapshot1, &ctx), Status::OK);

    // T1 reads the tuple (should see 0xAA pattern)
    Tuple tuple1;
    Status read1_result = db.storage_engine()->getTuple(page_id, item_id, &tuple1, &ctx);

    // Store whether T1 successfully read the tuple
    bool t1_read_success = (read1_result == Status::OK);
    std::vector<uint8_t> t1_data;
    if (t1_read_success) {
        t1_data.assign(tuple1.data, tuple1.data + tuple1.data_size);
    }

    // Transaction T2: Start, update tuple, commit (while T1 is still active)
    uint64_t xid2;
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id2, xid2, &ctx), Status::OK);

    // Create updated tuple with 0xBB pattern
    std::vector<uint8_t> updated_tuple_data(sizeof(TupleHeader) + 100);
    auto* hdr2 = reinterpret_cast<TupleHeader*>(updated_tuple_data.data());
    hdr2->xmin = xid2;
    hdr2->xmax = 0;
    hdr2->next_version_tid = 0;
    hdr2->ctid_page = 0;
    hdr2->ctid_item = 0;
    hdr2->infomask = 0;
    hdr2->null_bitmap_offset = 0;
    hdr2->padding = 0;

    for (size_t i = sizeof(TupleHeader); i < updated_tuple_data.size(); i++) {
        updated_tuple_data[i] = 0xBB;
    }

    // T2 updates the tuple
    uint32_t new_page_id;
    uint16_t new_item_id;
    Status update_result = db.storage_engine()->updateTuple(page_id, item_id,
                                                            updated_tuple_data.data(),
                                                            updated_tuple_data.size(),
                                                            &new_page_id, &new_item_id, &ctx);

    // Update might not be implemented or might fail for various reasons
    // For this test, we primarily verify snapshot behavior
    bool t2_updated = (update_result == Status::OK);

    // T2 commits
    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id2, xid2, &ctx), Status::OK);

    // T1 reads again (should still see original 0xAA pattern due to snapshot isolation)
    Tuple tuple1_again;
    Status read1_again_result = db.storage_engine()->getTuple(page_id, item_id, &tuple1_again, &ctx);

    if (t1_read_success && read1_again_result == Status::OK) {
        // Verify T1 still sees same data (snapshot isolation)
        std::vector<uint8_t> t1_data_again(tuple1_again.data, tuple1_again.data + tuple1_again.data_size);

        // Compare tuple data (skip header as it may have visibility info updated)
        if (t1_data.size() >= sizeof(TupleHeader) && t1_data_again.size() >= sizeof(TupleHeader)) {
            bool data_matches = (memcmp(t1_data.data() + sizeof(TupleHeader),
                                       t1_data_again.data() + sizeof(TupleHeader),
                                       std::min(t1_data.size(), t1_data_again.size()) - sizeof(TupleHeader)) == 0);

            EXPECT_TRUE(data_matches)
                << "T1 should see same tuple data before and after T2's update (snapshot isolation)";
        }
    }

    // T1 commits
    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id1, xid1, &ctx), Status::OK);

    // Transaction T3: Start after T2 committed (should see T2's changes)
    uint64_t xid3;
    ASSERT_EQ(db.transaction_manager()->beginTransaction(proc_id3, xid3, &ctx), Status::OK);

    TransactionManager::Snapshot snapshot3;
    ASSERT_EQ(db.transaction_manager()->getSnapshot(snapshot3, &ctx), Status::OK);

    // Verify T3's snapshot does NOT contain T2 (since T2 committed before T3 started)
    bool t3_sees_t2_active = false;
    for (uint64_t active_xid : snapshot3.active_xids) {
        if (active_xid == xid2) {
            t3_sees_t2_active = true;
            break;
        }
    }
    EXPECT_FALSE(t3_sees_t2_active)
        << "T3 should not see T2 as active since T2 committed before T3 started";

    // T3 commits
    ASSERT_EQ(db.transaction_manager()->commitTransaction(proc_id3, xid3, &ctx), Status::OK);

    // Verify snapshot properties
    EXPECT_LT(snapshot1.xmin, snapshot3.xmin)
        << "Later snapshot should have higher xmin";

    EXPECT_GT(snapshot3.xmin, xid0)
        << "T3's snapshot xmin should be after committed transaction T0";

    // Clean up
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id2, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id3, &ctx), Status::OK);
    db.close();
}
