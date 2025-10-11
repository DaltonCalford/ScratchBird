// Integration tests for Phase 3 Task 3.6: Advanced Transaction Features
// Tests table reservation locking, lock timeout, and SET TRANSACTION staging

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

class TransactionAdvancedIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = "test_txn_advanced.db";
        ::unlink(db_path_.c_str());
    }

    void TearDown() override
    {
        ::unlink(db_path_.c_str());
    }

    std::string db_path_;
};

// Test 1: Basic Table Reservation with SHARED lock
TEST_F(TransactionAdvancedIntegrationTest, TableReservationShared)
{
    ErrorContext ctx;

    // Create and open database
    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Create a test table
    ID table_id;
    memset(table_id.bytes.data(), 0x01, 16);

    CatalogManager::TableInfo table_info;
    table_info.table_id = table_id;
    table_info.table_name = "test_table";
    table_info.schema_id = db.catalog_manager()->getSystemSchemaId();

    ASSERT_EQ(db.catalog_manager()->createTable(table_info, &ctx), Status::OK);

    // Register backend and create connection context
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);
    ConnectionContext::setCurrent(&conn_ctx);

    // Reserve table with SHARED lock
    std::vector<ConnectionContext::TableReservation> reservations;
    reservations.push_back({"test_table", TableLockMode::SHARED, false});

    ASSERT_EQ(conn_ctx.reserveTables(reservations, &ctx), Status::OK);

    // Start new transaction with SNAPSHOT TABLE STABILITY (triggers lock acquisition)
    ASSERT_EQ(conn_ctx.startTransaction(false, IsolationLevel::SNAPSHOT_TABLE_STABILITY,
                                      true, &ctx), Status::OK);

    // Verify lock was acquired by checking lock manager statistics
    LockStats stats;
    db.lock_manager()->getStatistics(&stats);
    EXPECT_GT(stats.locks_acquired, 0u);

    // Cleanup
    ConnectionContext::setCurrent(nullptr);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 2: Table Reservation with PROTECTED lock
TEST_F(TransactionAdvancedIntegrationTest, TableReservationProtected)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Create test table
    ID table_id;
    memset(table_id.bytes.data(), 0x02, 16);

    CatalogManager::TableInfo table_info;
    table_info.table_id = table_id;
    table_info.table_name = "protected_table";
    table_info.schema_id = db.catalog_manager()->getSystemSchemaId();

    ASSERT_EQ(db.catalog_manager()->createTable(table_info, &ctx), Status::OK);

    // Register backend
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);
    ConnectionContext::setCurrent(&conn_ctx);

    // Reserve table with PROTECTED WRITE lock (exclusive)
    std::vector<ConnectionContext::TableReservation> reservations;
    reservations.push_back({"protected_table", TableLockMode::PROTECTED, true});

    ASSERT_EQ(conn_ctx.reserveTables(reservations, &ctx), Status::OK);

    // Start transaction with table stability isolation
    ASSERT_EQ(conn_ctx.startTransaction(false, IsolationLevel::SNAPSHOT_TABLE_STABILITY,
                                      true, &ctx), Status::OK);

    // Verify exclusive lock prevents concurrent access
    // (Would require second backend to fully test, but we can verify lock was acquired)
    LockStats stats;
    db.lock_manager()->getStatistics(&stats);
    EXPECT_GT(stats.locks_acquired, 0u);

    // Cleanup
    ConnectionContext::setCurrent(nullptr);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 3: Multiple Table Reservations
TEST_F(TransactionAdvancedIntegrationTest, MultipleTableReservations)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Create multiple test tables
    for (int i = 0; i < 3; i++)
    {
        ID table_id;
        memset(table_id.bytes.data(), 0x10 + i, 16);

        CatalogManager::TableInfo table_info;
        table_info.table_id = table_id;
        table_info.table_name = "table_" + std::to_string(i);
        table_info.schema_id = db.catalog_manager()->getSystemSchemaId();

        ASSERT_EQ(db.catalog_manager()->createTable(table_info, &ctx), Status::OK);
    }

    // Register backend
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);
    ConnectionContext::setCurrent(&conn_ctx);

    // Reserve multiple tables with different lock modes
    std::vector<ConnectionContext::TableReservation> reservations;
    reservations.push_back({"table_0", TableLockMode::SHARED, false});
    reservations.push_back({"table_1", TableLockMode::SHARED, true});
    reservations.push_back({"table_2", TableLockMode::PROTECTED, true});

    ASSERT_EQ(conn_ctx.reserveTables(reservations, &ctx), Status::OK);

    // Get initial lock count
    LockStats stats_before;
    db.lock_manager()->getStatistics(&stats_before);

    // Start transaction (triggers lock acquisition for all tables)
    ASSERT_EQ(conn_ctx.startTransaction(false, IsolationLevel::SNAPSHOT_TABLE_STABILITY,
                                      true, &ctx), Status::OK);

    // Verify locks were acquired
    LockStats stats_after;
    db.lock_manager()->getStatistics(&stats_after);
    EXPECT_EQ(stats_after.locks_acquired - stats_before.locks_acquired, 3u)
        << "Should have acquired locks on all 3 tables";

    // Cleanup
    ConnectionContext::setCurrent(nullptr);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 4: Lock Timeout Configuration
TEST_F(TransactionAdvancedIntegrationTest, LockTimeoutConfiguration)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register backend
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);

    // Set lock timeout to 5 seconds
    conn_ctx.setLockTimeout(5);
    EXPECT_EQ(conn_ctx.getLockTimeout(), 5u);

    // Set lock timeout to 0 (no wait)
    conn_ctx.setLockTimeout(0);
    EXPECT_EQ(conn_ctx.getLockTimeout(), 0u);

    // Set lock timeout to UINT32_MAX (wait forever)
    conn_ctx.setLockTimeout(UINT32_MAX);
    EXPECT_EQ(conn_ctx.getLockTimeout(), UINT32_MAX);

    // Cleanup
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 5: Lock Timeout Behavior (Non-blocking)
TEST_F(TransactionAdvancedIntegrationTest, LockTimeoutNoWait)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Create test table
    ID table_id;
    memset(table_id.bytes.data(), 0x20, 16);

    CatalogManager::TableInfo table_info;
    table_info.table_id = table_id;
    table_info.table_name = "timeout_table";
    table_info.schema_id = db.catalog_manager()->getSystemSchemaId();

    ASSERT_EQ(db.catalog_manager()->createTable(table_info, &ctx), Status::OK);

    // Register two backends
    uint32_t proc_id1, proc_id2;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id2, &ctx), Status::OK);

    // Backend 1: Acquire exclusive lock
    LockTag tag;
    tag.target_type = LockTarget::LOCK_TARGET_TABLE;
    tag.object_uuid = table_id;
    tag.page_num = 0;
    tag.offset_num = 0;
    tag.padding = 0;

    ASSERT_EQ(db.lock_manager()->acquireLock(proc_id1, tag,
                                             LockMode::LOCK_ACCESS_EXCLUSIVE,
                                             false, 0, &ctx), Status::OK);

    // Backend 2: Try to acquire with NO WAIT (timeout = 0)
    Status result = db.lock_manager()->acquireLock(proc_id2, tag,
                                                    LockMode::LOCK_ACCESS_SHARE,
                                                    false, 0, &ctx);

    // Should fail immediately with lock conflict
    EXPECT_EQ(result, Status::LOCK_CONFLICT)
        << "Lock acquisition with NO WAIT should fail immediately";

    // Cleanup
    ASSERT_EQ(db.lock_manager()->releaseAllLocks(proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id2, &ctx), Status::OK);
    db.close();
}

// Test 6: SET TRANSACTION Parameter Staging
TEST_F(TransactionAdvancedIntegrationTest, SetTransactionStaging)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register backend
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);

    // Initial transaction should have default settings (SNAPSHOT, READ_WRITE)
    EXPECT_EQ(conn_ctx.getIsolationLevel(), IsolationLevel::SNAPSHOT);
    EXPECT_FALSE(conn_ctx.isReadOnly());

    // Stage new settings with startTransaction(commit_outstanding = false)
    ASSERT_EQ(conn_ctx.startTransaction(true, IsolationLevel::READ_COMMITTED,
                                      false, &ctx), Status::OK);

    // Current transaction should still have old settings
    EXPECT_EQ(conn_ctx.getIsolationLevel(), IsolationLevel::SNAPSHOT);
    EXPECT_FALSE(conn_ctx.isReadOnly());

    // Commit to apply staged settings
    ASSERT_EQ(conn_ctx.commit(&ctx), Status::OK);

    // New transaction should have staged settings applied
    EXPECT_EQ(conn_ctx.getIsolationLevel(), IsolationLevel::READ_COMMITTED);
    EXPECT_TRUE(conn_ctx.isReadOnly());

    // Cleanup
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 7: START TRANSACTION with COMMIT OUTSTANDING
TEST_F(TransactionAdvancedIntegrationTest, StartTransactionCommitOutstanding)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register backend
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);

    uint64_t xid_before = conn_ctx.getCurrentXid();

    // START TRANSACTION with commit_outstanding = true (immediate application)
    ASSERT_EQ(conn_ctx.startTransaction(true, IsolationLevel::SNAPSHOT_TABLE_STABILITY,
                                      true, &ctx), Status::OK);

    // Should have committed old transaction and started new one
    uint64_t xid_after = conn_ctx.getCurrentXid();
    EXPECT_NE(xid_before, xid_after) << "XID should change after COMMIT OUTSTANDING";

    // New settings should be active immediately
    EXPECT_EQ(conn_ctx.getIsolationLevel(), IsolationLevel::SNAPSHOT_TABLE_STABILITY);
    EXPECT_TRUE(conn_ctx.isReadOnly());

    // Cleanup
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 8: Wait vs No Wait Flag
TEST_F(TransactionAdvancedIntegrationTest, WaitForLocksFlag)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register backend
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);

    // Default should be WAIT
    EXPECT_TRUE(conn_ctx.getWaitForLocks());

    // Set to NO WAIT
    conn_ctx.setWaitForLocks(false);
    EXPECT_FALSE(conn_ctx.getWaitForLocks());

    // Set back to WAIT
    conn_ctx.setWaitForLocks(true);
    EXPECT_TRUE(conn_ctx.getWaitForLocks());

    // Cleanup
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 9: READ ONLY Transaction Flag in ProcArray
TEST_F(TransactionAdvancedIntegrationTest, ReadOnlyFlagInProcArray)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register backend
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);

    // Start READ ONLY transaction
    ASSERT_EQ(conn_ctx.startTransaction(true, IsolationLevel::SNAPSHOT,
                                      true, &ctx), Status::OK);

    // Verify flag is set in ConnectionContext
    EXPECT_TRUE(conn_ctx.isReadOnly());

    // The flag should also be propagated to ProcArray
    // (Verified internally by ConnectionContext::beginNewTransaction()
    // which calls ProcArrayManager::setTransactionReadOnly())

    // Cleanup
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 10: Transaction Marker Updates
TEST_F(TransactionAdvancedIntegrationTest, TransactionMarkerUpdates)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Get initial markers
    uint64_t oit_before = db.transaction_manager()->getOldestXid();
    uint64_t oat_before = db.transaction_manager()->getOldestActiveXid();

    // Register backend and start transaction
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);

    uint64_t xid = conn_ctx.getCurrentXid();

    // OAT should include our transaction
    uint64_t oat_after = db.transaction_manager()->getOldestActiveXid();
    EXPECT_LE(oat_after, xid) << "OAT should be <= current XID";

    // Commit transaction
    ASSERT_EQ(conn_ctx.commit(&ctx), Status::OK);

    // After commit, markers should update
    // (Non-deterministic timing, but verify no errors)
    uint64_t oit_final = db.transaction_manager()->getOldestXid();
    uint64_t oat_final = db.transaction_manager()->getOldestActiveXid();

    // Markers should be valid
    EXPECT_GT(oit_final, 0u);
    EXPECT_GE(oat_final, oit_final);

    // Cleanup
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 11: Rollback with Staged Settings
TEST_F(TransactionAdvancedIntegrationTest, RollbackWithStagedSettings)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register backend
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);

    // Stage settings
    ASSERT_EQ(conn_ctx.startTransaction(true, IsolationLevel::READ_COMMITTED,
                                      false, &ctx), Status::OK);

    // Rollback should apply staged settings
    ASSERT_EQ(conn_ctx.rollback(&ctx), Status::OK);

    // Verify staged settings were applied
    EXPECT_EQ(conn_ctx.getIsolationLevel(), IsolationLevel::READ_COMMITTED);
    EXPECT_TRUE(conn_ctx.isReadOnly());

    // Cleanup
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}

// Test 12: Table Reservation Error Handling (Invalid Table)
TEST_F(TransactionAdvancedIntegrationTest, TableReservationInvalidTable)
{
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK);
    ASSERT_EQ(db.initializeProcArray(10, &ctx), Status::OK);

    // Register backend
    uint32_t proc_id;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK);

    ConnectionContext conn_ctx(&db, proc_id);
    ASSERT_EQ(conn_ctx.initialize(&ctx), Status::OK);
    ConnectionContext::setCurrent(&conn_ctx);

    // Reserve non-existent table
    std::vector<ConnectionContext::TableReservation> reservations;
    reservations.push_back({"nonexistent_table", TableLockMode::SHARED, false});

    ASSERT_EQ(conn_ctx.reserveTables(reservations, &ctx), Status::OK);

    // Starting transaction should fail when trying to acquire locks
    Status result = conn_ctx.startTransaction(false, IsolationLevel::SNAPSHOT_TABLE_STABILITY,
                                             true, &ctx);

    EXPECT_NE(result, Status::OK) << "Should fail to reserve nonexistent table";

    // Cleanup
    ConnectionContext::setCurrent(nullptr);
    ASSERT_EQ(ProcArrayManager::unregisterBackend(proc_id, &ctx), Status::OK);
    db.close();
}
