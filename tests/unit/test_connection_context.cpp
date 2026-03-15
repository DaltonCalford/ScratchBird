/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/types.h"
#include "test_helpers.h"
#include <algorithm>
#include <filesystem>
#include <thread>

using namespace scratchbird::core;

class ConnectionContextTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test database (use relative path in current directory)
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_connection_context", ".sbrd");

        // Clean up any existing test database
        std::filesystem::remove(test_db_path_);

        // Create new database
        ErrorContext err_ctx;
        Status s = Database::create(test_db_path_, 16384, &err_ctx);
        ASSERT_EQ(s, Status::OK) << "Failed to create test database: " << err_ctx.message;

        // Open database
        s = db_.open(test_db_path_, &err_ctx);
        ASSERT_EQ(s, Status::OK) << "Failed to open test database: " << err_ctx.message;
    }

    void TearDown() override
    {
        // Close database
        db_.close();

        // Clean up test database
        std::filesystem::remove(test_db_path_);
    }

    std::string test_db_path_;
    Database db_;
};

// Test basic connection creation
TEST_F(ConnectionContextTest, CreateConnection)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK) << "Failed to create connection: " << err_ctx.message;
    ASSERT_NE(conn, nullptr);

    // Verify connection has a valid XID
    uint64_t xid = conn->getCurrentXid();
    EXPECT_NE(xid, 0u) << "Connection should have active transaction";

    // Verify connection has a valid proc_id
    uint32_t proc_id = conn->getProcId();
    EXPECT_GE(proc_id, 0u);
}

// Test transaction commit with auto-start
TEST_F(ConnectionContextTest, CommitAutoStart)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint64_t xid1 = conn->getCurrentXid();
    EXPECT_NE(xid1, 0u);

    // Commit - should start new transaction
    s = conn->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK) << "Commit failed: " << err_ctx.message;

    uint64_t xid2 = conn->getCurrentXid();
    EXPECT_NE(xid2, 0u) << "Should have new transaction after commit";
    EXPECT_NE(xid1, xid2) << "New transaction should have different XID";
}

// Test transaction rollback with auto-start
TEST_F(ConnectionContextTest, RollbackAutoStart)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint64_t xid1 = conn->getCurrentXid();
    EXPECT_NE(xid1, 0u);

    // Rollback - should start new transaction
    s = conn->rollback(&err_ctx);
    ASSERT_EQ(s, Status::OK) << "Rollback failed: " << err_ctx.message;

    uint64_t xid2 = conn->getCurrentXid();
    EXPECT_NE(xid2, 0u) << "Should have new transaction after rollback";
    EXPECT_NE(xid1, xid2) << "New transaction should have different XID";
}

// Test default isolation level
TEST_F(ConnectionContextTest, DefaultIsolationLevel)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Default should be SNAPSHOT
    EXPECT_EQ(conn->getIsolationLevel(), IsolationLevel::SNAPSHOT);
    EXPECT_FALSE(conn->isReadOnly());
}

// Test START TRANSACTION with COMMIT OUTSTANDING
TEST_F(ConnectionContextTest, StartTransactionCommitOutstanding)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint64_t xid1 = conn->getCurrentXid();

    // START TRANSACTION with COMMIT OUTSTANDING
    s = conn->startTransaction(true, IsolationLevel::READ_COMMITTED, true, &err_ctx);
    ASSERT_EQ(s, Status::OK) << "START TRANSACTION failed: " << err_ctx.message;

    // Should have new transaction
    uint64_t xid2 = conn->getCurrentXid();
    EXPECT_NE(xid1, xid2) << "Should have new transaction after START TRANSACTION COMMIT OUTSTANDING";

    // Settings should be applied immediately
    EXPECT_EQ(conn->getIsolationLevel(), IsolationLevel::READ_COMMITTED);
    EXPECT_TRUE(conn->isReadOnly());
}

// Test START TRANSACTION without COMMIT OUTSTANDING (staged settings)
TEST_F(ConnectionContextTest, StartTransactionStagedSettings)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint64_t xid1 = conn->getCurrentXid();
    IsolationLevel original_isolation = conn->getIsolationLevel();

    // START TRANSACTION without COMMIT OUTSTANDING - settings should be staged
    s = conn->startTransaction(true, IsolationLevel::READ_COMMITTED, false, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // XID should NOT change yet
    EXPECT_EQ(conn->getCurrentXid(), xid1) << "XID should not change when staging settings";

    // Settings should NOT be applied yet
    EXPECT_EQ(conn->getIsolationLevel(), original_isolation) << "Isolation level should not change yet";
    EXPECT_FALSE(conn->isReadOnly()) << "Read-only should not change yet";

    // Commit - staged settings should be applied
    s = conn->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Now settings should be applied
    EXPECT_EQ(conn->getIsolationLevel(), IsolationLevel::READ_COMMITTED);
    EXPECT_TRUE(conn->isReadOnly());
}

// Test thread-local storage
TEST_F(ConnectionContextTest, ThreadLocalStorage)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Set as current connection
    ConnectionContext::setCurrent(conn.get());

    // Verify we can retrieve it
    EXPECT_EQ(ConnectionContext::getCurrent(), conn.get());

    // Verify helper methods work
    EXPECT_EQ(ConnectionContext::getCurrentProcId(), static_cast<int32_t>(conn->getProcId()));
    EXPECT_EQ(ConnectionContext::getCurrentTransactionId(), conn->getCurrentXid());

    // Clear current connection
    ConnectionContext::setCurrent(nullptr);
    EXPECT_EQ(ConnectionContext::getCurrent(), nullptr);
}

// Test thread-local storage with no connection
TEST_F(ConnectionContextTest, ThreadLocalStorageNoConnection)
{
    // No connection set - should return safe defaults
    EXPECT_EQ(ConnectionContext::getCurrent(), nullptr);
    EXPECT_EQ(ConnectionContext::getCurrentProcId(), -1);
    EXPECT_EQ(ConnectionContext::getCurrentTransactionId(), 0u);
}

// Test multiple connections
TEST_F(ConnectionContextTest, MultipleConnections)
{
    ErrorContext err_ctx;

    // Create first connection
    std::unique_ptr<ConnectionContext> conn1;
    Status s = db_.connect(conn1, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Create second connection
    std::unique_ptr<ConnectionContext> conn2;
    s = db_.connect(conn2, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Connections should have different proc_ids
    EXPECT_NE(conn1->getProcId(), conn2->getProcId());

    // Connections should have different XIDs (assuming both start transactions)
    EXPECT_NE(conn1->getCurrentXid(), conn2->getCurrentXid());
}

// Test connection cleanup on destruction
TEST_F(ConnectionContextTest, ConnectionCleanup)
{
    ErrorContext err_ctx;

    {
        std::unique_ptr<ConnectionContext> conn;
        Status s = db_.connect(conn, &err_ctx);
        ASSERT_EQ(s, Status::OK);

        // Set as current
        ConnectionContext::setCurrent(conn.get());

        // Connection goes out of scope
    }

    // Current connection should be cleared
    EXPECT_EQ(ConnectionContext::getCurrent(), nullptr);
}

// Test transaction settings
TEST_F(ConnectionContextTest, TransactionSettings)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Test wait for locks setting
    EXPECT_TRUE(conn->getWaitForLocks()); // Default is true
    conn->setWaitForLocks(false);
    EXPECT_FALSE(conn->getWaitForLocks());

    // Test lock timeout setting
    EXPECT_EQ(conn->getLockTimeout(), 60u); // Default is 60 seconds
    conn->setLockTimeout(120);
    EXPECT_EQ(conn->getLockTimeout(), 120u);
}

// Test default isolation behavior and statement XID defaults
TEST_F(ConnectionContextTest, SnapshotCreation)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Default isolation is SNAPSHOT
    EXPECT_EQ(conn->getIsolationLevel(), IsolationLevel::SNAPSHOT);

    // Statement XID should default to current XID when no statement XID is active
    EXPECT_EQ(conn->getStatementXID(), conn->getCurrentXid());
}

// Test table reservation
TEST_F(ConnectionContextTest, TableReservation)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    auto *catalog = db_.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    CatalogManager::SchemaInfo schema_info;
    s = catalog->getSchema("public", schema_info, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    CatalogManager::ColumnInfo col;
    col.column_name = "id";
    col.data_type = static_cast<uint16_t>(DataType::INT32);
    std::vector<CatalogManager::ColumnInfo> columns{col};

    ID table_id1;
    s = catalog->createTable(schema_info.schema_id, "test_table1", columns, table_id1, 0,
                             &err_ctx);
    ASSERT_EQ(s, Status::OK);

    ID table_id2;
    s = catalog->createTable(schema_info.schema_id, "test_table2", columns, table_id2, 0,
                             &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Create table reservations
    std::vector<ConnectionContext::TableReservation> reservations;
    reservations.push_back({ID{}, "test_table1", TableLockMode::SHARED, false});
    reservations.push_back({ID{}, "test_table2", TableLockMode::PROTECTED, true});

    s = conn->reserveTables(reservations, &err_ctx);
    EXPECT_EQ(s, Status::OK) << "Table reservation failed: " << err_ctx.message;
}

// Test isolation level transitions
TEST_F(ConnectionContextTest, IsolationLevelTransitions)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Start with SNAPSHOT
    EXPECT_EQ(conn->getIsolationLevel(), IsolationLevel::SNAPSHOT);

    // Change to READ_COMMITTED
    s = conn->startTransaction(false, IsolationLevel::READ_COMMITTED, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    EXPECT_EQ(conn->getIsolationLevel(), IsolationLevel::READ_COMMITTED);

    // Change to SNAPSHOT_TABLE_STABILITY
    s = conn->startTransaction(false, IsolationLevel::SNAPSHOT_TABLE_STABILITY, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    EXPECT_EQ(conn->getIsolationLevel(), IsolationLevel::SNAPSHOT_TABLE_STABILITY);
}

// Test read-only transaction
TEST_F(ConnectionContextTest, ReadOnlyTransaction)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Initially read-write
    EXPECT_FALSE(conn->isReadOnly());

    // Start read-only transaction
    s = conn->startTransaction(true, IsolationLevel::SNAPSHOT, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    EXPECT_TRUE(conn->isReadOnly());
}

// Test transaction start time
TEST_F(ConnectionContextTest, TransactionStartTime)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    auto start_time1 = conn->getTransactionStartTime();
    EXPECT_GT(start_time1.count(), 0) << "Transaction start time should be set";

    // Sleep a bit to ensure time difference
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Commit and start new transaction
    s = conn->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK);

    auto start_time2 = conn->getTransactionStartTime();
    EXPECT_GT(start_time2.count(), start_time1.count()) << "New transaction should have later start time";
}

// Test concurrent connections in separate threads
TEST_F(ConnectionContextTest, ConcurrentConnections)
{
    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, &success_count]() {
            ErrorContext err_ctx;
            std::unique_ptr<ConnectionContext> conn;

            Status s = db_.connect(conn, &err_ctx);
            if (s == Status::OK) {
                // Set as thread-local current
                ConnectionContext::setCurrent(conn.get());

                // Perform some operations
                s = conn->commit(&err_ctx);
                if (s == Status::OK) {
                    s = conn->rollback(&err_ctx);
                    if (s == Status::OK) {
                        success_count++;
                    }
                }
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads) << "All threads should successfully create and use connections";
}

// Test error handling - no database
TEST_F(ConnectionContextTest, ErrorNoDatabase)
{
    ErrorContext err_ctx;
    Database empty_db;
    std::unique_ptr<ConnectionContext> conn;

    // Try to connect to closed database
    Status s = empty_db.connect(conn, &err_ctx);
    EXPECT_NE(s, Status::OK) << "Should fail to connect to closed database";
    EXPECT_EQ(conn, nullptr);
}

// Test SNAPSHOT isolation - repeatable reads
TEST_F(ConnectionContextTest, SnapshotIsolationRepeatableReads)
{
    ErrorContext err_ctx;

    // Create two connections simulating concurrent transactions
    std::unique_ptr<ConnectionContext> conn1;
    std::unique_ptr<ConnectionContext> conn2;

    Status s = db_.connect(conn1, &err_ctx);
    ASSERT_EQ(s, Status::OK) << "Failed to create connection 1: " << err_ctx.message;

    s = db_.connect(conn2, &err_ctx);
    ASSERT_EQ(s, Status::OK) << "Failed to create connection 2: " << err_ctx.message;

    // Both connections should have SNAPSHOT isolation by default
    ASSERT_EQ(conn1->getIsolationLevel(), IsolationLevel::SNAPSHOT);
    ASSERT_EQ(conn2->getIsolationLevel(), IsolationLevel::SNAPSHOT);

    // Get XIDs
    uint64_t xid1 = conn1->getCurrentXid();
    uint64_t xid2 = conn2->getCurrentXid();
    EXPECT_NE(xid1, xid2) << "Connections should have different XIDs";

    // Test visibility using StorageEngine
    StorageEngine* storage = db_.storage_engine();
    ASSERT_NE(storage, nullptr);

    // Set conn1 as current to test visibility from conn1's perspective
    ConnectionContext::setCurrent(conn1.get());

    // Scenario 1: Conn1 should see its own changes
    // Tuple created by xid1 (not deleted)
    EXPECT_TRUE(storage->isVisible(xid1, 0, xid1))
        << "Transaction should see its own uncommitted changes (xmin=xid1)";

    // Scenario 2: Conn1 should NOT see tuple deleted by itself
    // Tuple created by xid1, deleted by xid1
    EXPECT_FALSE(storage->isVisible(xid1, xid1, xid1))
        << "Transaction should NOT see tuples it deleted (xmin=xid1, xmax=xid1)";

    // Scenario 3: Conn1 should NOT see active transaction (conn2) changes
    // Tuple created by xid2 (still active)
    EXPECT_FALSE(storage->isVisible(xid2, 0, xid1))
        << "SNAPSHOT isolation: should NOT see active transaction's changes";

    // Scenario 4: Now commit conn2
    s = conn2->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK) << "Conn2 commit failed";

    // Even after conn2 commits, conn1's snapshot should NOT see it
    // because xid2 was active when snapshot1 was taken
    EXPECT_FALSE(storage->isVisible(xid2, 0, xid1))
        << "SNAPSHOT isolation: should NOT see transactions that were active at snapshot time, even after they commit";

    // Scenario 5: Conn1 commits and gets new snapshot
    uint64_t old_xid1 = xid1;
    s = conn1->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK) << "Conn1 commit failed";

    xid1 = conn1->getCurrentXid();
    EXPECT_NE(xid1, old_xid1) << "Should have new XID after commit";

    // NOW conn1's new snapshot SHOULD see conn2's committed changes
    // because xid2 committed before the new snapshot was taken
    EXPECT_TRUE(storage->isVisible(xid2, 0, xid1))
        << "New snapshot should see previously committed transactions";
}

// Complex SNAPSHOT isolation scenario with overlapping update/delete
TEST_F(ConnectionContextTest, SnapshotIsolationComplexVisibility)
{
    ErrorContext err_ctx;

    StorageEngine* storage = db_.storage_engine();
    ASSERT_NE(storage, nullptr);

    // Seed transaction creates a base version and commits before snapshot starts.
    std::unique_ptr<ConnectionContext> conn_seed;
    ASSERT_EQ(db_.connect(conn_seed, &err_ctx), Status::OK);
    uint64_t xid_seed = conn_seed->getCurrentXid();
    ASSERT_EQ(conn_seed->commit(&err_ctx), Status::OK);

    // Snapshot reader starts after seed commit.
    std::unique_ptr<ConnectionContext> conn_snap;
    ASSERT_EQ(db_.connect(conn_snap, &err_ctx), Status::OK);
    uint64_t xid_snap = conn_snap->getCurrentXid();

    ConnectionContext::setCurrent(conn_snap.get());

    // Base version should be visible in snapshot.
    EXPECT_TRUE(storage->isVisible(xid_seed, 0, xid_snap));

    // Writer starts after snapshot and performs an update.
    std::unique_ptr<ConnectionContext> conn_writer;
    ASSERT_EQ(db_.connect(conn_writer, &err_ctx), Status::OK);
    uint64_t xid_writer = conn_writer->getCurrentXid();

    // Old version deleted by writer, new version created by writer.
    EXPECT_TRUE(storage->isVisible(xid_seed, xid_writer, xid_snap))
        << "Snapshot should see old version while writer is active";
    EXPECT_FALSE(storage->isVisible(xid_writer, 0, xid_snap))
        << "Snapshot should NOT see writer's new version";

    // Commit writer. Snapshot should still see old version, not new.
    ASSERT_EQ(conn_writer->commit(&err_ctx), Status::OK);
    EXPECT_TRUE(storage->isVisible(xid_seed, xid_writer, xid_snap))
        << "Snapshot should keep seeing old version after writer commits";
    EXPECT_FALSE(storage->isVisible(xid_writer, 0, xid_snap))
        << "Snapshot should NOT see new version committed after snapshot start";

    // Refresh snapshot by committing and getting a new XID.
    ASSERT_EQ(conn_snap->commit(&err_ctx), Status::OK);
    uint64_t xid_snap2 = conn_snap->getCurrentXid();
    EXPECT_NE(xid_snap2, xid_snap);

    ConnectionContext::setCurrent(conn_snap.get());
    EXPECT_FALSE(storage->isVisible(xid_seed, xid_writer, xid_snap2))
        << "New snapshot should NOT see old version (deletion visible)";
    EXPECT_TRUE(storage->isVisible(xid_writer, 0, xid_snap2))
        << "New snapshot should see committed new version";
}

// Test statement XID defaulting behavior
TEST_F(ConnectionContextTest, SnapshotIsolationProperties)
{
    ErrorContext err_ctx;

    // Create connection with SNAPSHOT isolation (default)
    std::unique_ptr<ConnectionContext> conn;
    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint64_t current_xid = conn->getCurrentXid();
    EXPECT_NE(current_xid, 0u);
    EXPECT_EQ(conn->getStatementXID(), current_xid)
        << "Statement XID should default to current XID when no statement is active";
}

// Test READ_COMMITTED vs SNAPSHOT isolation visibility differences
TEST_F(ConnectionContextTest, ReadCommittedVsSnapshotIsolation)
{
    ErrorContext err_ctx;

    // Create two connections: one READ_COMMITTED, one SNAPSHOT
    std::unique_ptr<ConnectionContext> conn_rc;  // READ_COMMITTED
    std::unique_ptr<ConnectionContext> conn_snap;  // SNAPSHOT

    Status s = db_.connect(conn_rc, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    s = db_.connect(conn_snap, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Create a third connection that will commit BEFORE we transition conn_rc
    // This ensures xid_writer < xid_rc (not a future transaction)
    std::unique_ptr<ConnectionContext> conn_writer;
    s = db_.connect(conn_writer, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint64_t xid_writer = conn_writer->getCurrentXid();

    // Writer commits
    s = conn_writer->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK);

    // NOW change conn_rc to READ_COMMITTED (this allocates a new, higher XID)
    s = conn_rc->startTransaction(false, IsolationLevel::READ_COMMITTED, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    ASSERT_EQ(conn_rc->getIsolationLevel(), IsolationLevel::READ_COMMITTED);

    // conn_snap stays as SNAPSHOT
    ASSERT_EQ(conn_snap->getIsolationLevel(), IsolationLevel::SNAPSHOT);

    uint64_t xid_rc = conn_rc->getCurrentXid();
    uint64_t xid_snap = conn_snap->getCurrentXid();

    // xid_writer should now be < xid_rc (since conn_writer started before conn_rc's transition)
    EXPECT_LT(xid_writer, xid_rc) << "Writer XID should be less than READ_COMMITTED XID";

    StorageEngine* storage = db_.storage_engine();
    ASSERT_NE(storage, nullptr);

    // Test READ_COMMITTED: should see committed transaction
    ConnectionContext::setCurrent(conn_rc.get());
    EXPECT_TRUE(storage->isVisible(xid_writer, 0, xid_rc))
        << "READ_COMMITTED should see committed transactions";

    // Test SNAPSHOT: xid_writer was active when conn_snap's snapshot was taken,
    // so it should NOT be visible even though it committed
    ConnectionContext::setCurrent(conn_snap.get());
    EXPECT_FALSE(storage->isVisible(xid_writer, 0, xid_snap))
        << "SNAPSHOT should NOT see transactions that were active at snapshot time";
}

// Test isolation level transitions and transaction lifecycle
TEST_F(ConnectionContextTest, IsolationTransitionSnapshotLifecycle)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Start with SNAPSHOT
    ASSERT_EQ(conn->getIsolationLevel(), IsolationLevel::SNAPSHOT);

    // Change to READ_COMMITTED
    s = conn->startTransaction(false, IsolationLevel::READ_COMMITTED, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    ASSERT_EQ(conn->getIsolationLevel(), IsolationLevel::READ_COMMITTED);

    // Change back to SNAPSHOT
    s = conn->startTransaction(false, IsolationLevel::SNAPSHOT, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    ASSERT_EQ(conn->getIsolationLevel(), IsolationLevel::SNAPSHOT);

    // Change to SNAPSHOT_TABLE_STABILITY
    s = conn->startTransaction(false, IsolationLevel::SNAPSHOT_TABLE_STABILITY, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    ASSERT_EQ(conn->getIsolationLevel(), IsolationLevel::SNAPSHOT_TABLE_STABILITY);
}

// Test READ_COMMITTED_READ_CONSISTENCY - statement XID lifecycle
TEST_F(ConnectionContextTest, ReadCommittedReadConsistencyStatementSnapshot)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Change to READ_COMMITTED_READ_CONSISTENCY
    s = conn->startTransaction(false, IsolationLevel::READ_COMMITTED_READ_CONSISTENCY, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    ASSERT_EQ(conn->getIsolationLevel(), IsolationLevel::READ_COMMITTED_READ_CONSISTENCY);

    // No statement XID between statements
    EXPECT_EQ(conn->getStatementXID(), conn->getCurrentXid())
        << "Should use current XID between statements";

    // Create statement XID (simulating statement execution start)
    ASSERT_EQ(conn->createStatementXID(&err_ctx), Status::OK);

    // Should now use the statement XID (equal to current XID at statement start)
    uint64_t stmt_xid = conn->getStatementXID();
    EXPECT_EQ(stmt_xid, conn->getCurrentXid());

    // Clear statement XID (simulating statement execution end)
    ASSERT_EQ(conn->clearStatementXID(&err_ctx), Status::OK);

    // Should fall back to current XID
    EXPECT_EQ(conn->getStatementXID(), conn->getCurrentXid())
        << "Statement XID should reset after clearStatementXID()";
}

// Test READ_COMMITTED_READ_CONSISTENCY - statement-level consistency
TEST_F(ConnectionContextTest, ReadCommittedReadConsistencyStatementConsistency)
{
    ErrorContext err_ctx;

    // Create writer first so its XID is lower than reader's XID
    std::unique_ptr<ConnectionContext> conn_writer;
    Status s = db_.connect(conn_writer, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    uint64_t xid_writer = conn_writer->getCurrentXid();

    // Writer commits BEFORE reader starts
    s = conn_writer->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK);

    // NOW create reader with READ_COMMITTED_READ_CONSISTENCY
    std::unique_ptr<ConnectionContext> conn_rcrc;
    s = db_.connect(conn_rcrc, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Set to READ_COMMITTED_READ_CONSISTENCY
    s = conn_rcrc->startTransaction(false, IsolationLevel::READ_COMMITTED_READ_CONSISTENCY, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint64_t xid_rcrc = conn_rcrc->getCurrentXid();

    // xid_writer should be < xid_rcrc
    EXPECT_LT(xid_writer, xid_rcrc) << "Writer XID should be less than reader XID";

    StorageEngine* storage = db_.storage_engine();
    ASSERT_NE(storage, nullptr);

    // WITHOUT statement XID: should behave like READ_COMMITTED
    ConnectionContext::setCurrent(conn_rcrc.get());

    // Should see previously committed writer transaction
    EXPECT_TRUE(storage->isVisible(xid_writer, 0, xid_rcrc))
        << "Without statement XID, should see committed transactions (READ_COMMITTED behavior)";

    // NOW create statement XID (simulating statement start)
    ASSERT_EQ(conn_rcrc->createStatementXID(&err_ctx), Status::OK);

    // Create another writer transaction that commits DURING the statement
    std::unique_ptr<ConnectionContext> conn_writer2;
    s = db_.connect(conn_writer2, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint64_t xid_writer2 = conn_writer2->getCurrentXid();

    s = conn_writer2->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK);

    // WITH statement XID: should NOT see the new commit during this statement
    // because it was active when statement XID was captured
    ConnectionContext::setCurrent(conn_rcrc.get());
    EXPECT_FALSE(storage->isVisible(xid_writer2, 0, xid_rcrc))
        << "With statement XID, should NOT see transactions active at statement start";

    // Clear statement XID (simulating statement end)
    ASSERT_EQ(conn_rcrc->clearStatementXID(&err_ctx), Status::OK);

    // After clearing: xid_writer2 is still a future transaction (> xid_rcrc)
    // so it won't be visible even without statement XID
    // This is correct READ_COMMITTED behavior - only see transactions that committed BEFORE you started
    EXPECT_FALSE(storage->isVisible(xid_writer2, 0, xid_rcrc))
        << "Future transactions (XID > current XID) are never visible";
}

// Test READ_COMMITTED_READ_CONSISTENCY vs other isolation levels
TEST_F(ConnectionContextTest, ReadCommittedReadConsistencyComparison)
{
    ErrorContext err_ctx;

    // Create writer FIRST so it has lowest XID
    std::unique_ptr<ConnectionContext> conn_writer;
    Status s = db_.connect(conn_writer, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    uint64_t xid_writer = conn_writer->getCurrentXid();

    // Writer commits BEFORE readers start
    s = conn_writer->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK);

    // NOW create readers with different isolation levels
    std::unique_ptr<ConnectionContext> conn_rc;      // READ_COMMITTED
    std::unique_ptr<ConnectionContext> conn_rcrc;    // READ_COMMITTED_READ_CONSISTENCY
    std::unique_ptr<ConnectionContext> conn_snap;    // SNAPSHOT

    s = db_.connect(conn_rc, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    s = db_.connect(conn_rcrc, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    s = db_.connect(conn_snap, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Set isolation levels
    s = conn_rc->startTransaction(false, IsolationLevel::READ_COMMITTED, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    s = conn_rcrc->startTransaction(false, IsolationLevel::READ_COMMITTED_READ_CONSISTENCY, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // conn_snap already has SNAPSHOT (default)
    ASSERT_EQ(conn_snap->getIsolationLevel(), IsolationLevel::SNAPSHOT);

    uint64_t xid_rc = conn_rc->getCurrentXid();
    uint64_t xid_rcrc = conn_rcrc->getCurrentXid();
    uint64_t xid_snap = conn_snap->getCurrentXid();

    // Verify xid_writer < all reader XIDs
    EXPECT_LT(xid_writer, xid_rc) << "Writer XID should be less than READ_COMMITTED XID";
    EXPECT_LT(xid_writer, xid_rcrc) << "Writer XID should be less than RCRC XID";
    EXPECT_LT(xid_writer, xid_snap) << "Writer XID should be less than SNAPSHOT XID";

    StorageEngine* storage = db_.storage_engine();
    ASSERT_NE(storage, nullptr);

    // Test READ_COMMITTED: sees committed change immediately
    ConnectionContext::setCurrent(conn_rc.get());
    EXPECT_TRUE(storage->isVisible(xid_writer, 0, xid_rc))
        << "READ_COMMITTED should see committed change";

    // Test READ_COMMITTED_READ_CONSISTENCY without statement XID:
    // should see committed change (between statements)
    ConnectionContext::setCurrent(conn_rcrc.get());
    EXPECT_TRUE(storage->isVisible(xid_writer, 0, xid_rcrc))
        << "RCRC without statement XID should see committed changes";

    // Create statement XID for RCRC (simulating statement start)
    ASSERT_EQ(conn_rcrc->createStatementXID(&err_ctx), Status::OK);

    // Test READ_COMMITTED_READ_CONSISTENCY with statement XID:
    // should still see xid_writer because it committed before statement XID
    bool visible_during_stmt = storage->isVisible(xid_writer, 0, xid_rcrc);
    EXPECT_TRUE(visible_during_stmt)
        << "RCRC with statement XID should see transactions committed before statement";

    // Clear statement XID
    ASSERT_EQ(conn_rcrc->clearStatementXID(&err_ctx), Status::OK);

    // Test SNAPSHOT: xid_writer committed BEFORE snapshot, so it IS visible
    ConnectionContext::setCurrent(conn_snap.get());
    EXPECT_TRUE(storage->isVisible(xid_writer, 0, xid_snap))
        << "SNAPSHOT should see transactions that committed before snapshot was taken";

    // To test SNAPSHOT NOT seeing a transaction, create one that's active during snapshot
    std::unique_ptr<ConnectionContext> conn_concurrent;
    s = db_.connect(conn_concurrent, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    uint64_t xid_concurrent = conn_concurrent->getCurrentXid();

    // This transaction is active while conn_snap's snapshot exists
    // So conn_snap should NOT see it even after it commits
    s = conn_concurrent->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK);

    // conn_snap's snapshot was taken before conn_concurrent started,
    // so conn_concurrent was in the active list - should NOT be visible
    EXPECT_FALSE(storage->isVisible(xid_concurrent, 0, xid_snap))
        << "SNAPSHOT should NOT see transactions that were active at snapshot time";
}

TEST_F(ConnectionContextTest, SnapshotIsolationUsesCapturedActiveSet)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> writer;
    std::unique_ptr<ConnectionContext> reader;

    ASSERT_EQ(db_.connect(writer, &err_ctx), Status::OK);
    uint64_t xid_writer = writer->getCurrentXid();

    ASSERT_EQ(db_.connect(reader, &err_ctx), Status::OK);
    uint64_t xid_reader = reader->getCurrentXid();
    ASSERT_LT(xid_writer, xid_reader);

    const TransactionSnapshot *retained = reader->getRetainedTransactionSnapshot();
    ASSERT_NE(retained, nullptr);
    EXPECT_TRUE(std::binary_search(retained->active_txid_set.begin(),
                                   retained->active_txid_set.end(),
                                   xid_writer));
    EXPECT_EQ(retained->snapshot_txid_low, xid_writer);

    ASSERT_EQ(writer->commit(&err_ctx), Status::OK);

    StorageEngine *storage = db_.storage_engine();
    ASSERT_NE(storage, nullptr);

    ConnectionContext::setCurrent(reader.get());
    EXPECT_FALSE(storage->isVisible(xid_writer, 0, xid_reader))
        << "Snapshot reader must not see a transaction that was active at capture";
}

TEST_F(ConnectionContextTest, ReadCommittedReadConsistencyUsesStatementSnapshotInventory)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> writer;
    std::unique_ptr<ConnectionContext> reader;

    ASSERT_EQ(db_.connect(writer, &err_ctx), Status::OK);
    uint64_t xid_writer = writer->getCurrentXid();

    ASSERT_EQ(db_.connect(reader, &err_ctx), Status::OK);
    ASSERT_EQ(reader->startTransaction(false,
                                       IsolationLevel::READ_COMMITTED_READ_CONSISTENCY,
                                       true,
                                       &err_ctx),
              Status::OK);
    uint64_t xid_reader = reader->getCurrentXid();
    ASSERT_LT(xid_writer, xid_reader);

    ASSERT_EQ(reader->beginStatementTracking("SELECT * FROM users.public.t", &err_ctx),
              Status::OK);
    const TransactionSnapshot *statement_snapshot = reader->getStatementTransactionSnapshot();
    ASSERT_NE(statement_snapshot, nullptr);
    EXPECT_TRUE(std::binary_search(statement_snapshot->active_txid_set.begin(),
                                   statement_snapshot->active_txid_set.end(),
                                   xid_writer));

    ASSERT_EQ(writer->commit(&err_ctx), Status::OK);

    StorageEngine *storage = db_.storage_engine();
    ASSERT_NE(storage, nullptr);

    ConnectionContext::setCurrent(reader.get());
    EXPECT_FALSE(storage->isVisible(xid_writer, 0, xid_reader))
        << "RCRC statement snapshot must hide transactions active at statement start";

    reader->endStatementTrackingSuccess(0);
    EXPECT_EQ(reader->getStatementTransactionSnapshot(), nullptr);

    ConnectionContext::setCurrent(reader.get());
    EXPECT_TRUE(storage->isVisible(xid_writer, 0, xid_reader))
        << "After statement end, READ COMMITTED visibility should refresh";
}

TEST_F(ConnectionContextTest, ReadCommittedReadConsistencyActiveStatementDoesNotFallbackWithoutSnapshot)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> writer;
    std::unique_ptr<ConnectionContext> reader;

    ASSERT_EQ(db_.connect(writer, &err_ctx), Status::OK);
    uint64_t xid_writer = writer->getCurrentXid();

    ASSERT_EQ(db_.connect(reader, &err_ctx), Status::OK);
    ASSERT_EQ(reader->startTransaction(false,
                                       IsolationLevel::READ_COMMITTED_READ_CONSISTENCY,
                                       true,
                                       &err_ctx),
              Status::OK);
    uint64_t xid_reader = reader->getCurrentXid();
    ASSERT_LT(xid_writer, xid_reader);

    ASSERT_EQ(reader->beginStatementTracking("SELECT * FROM users.public.t", &err_ctx),
              Status::OK);
    ASSERT_NE(reader->getStatementTransactionSnapshot(), nullptr);
    ASSERT_EQ(reader->clearStatementXID(&err_ctx), Status::OK);
    EXPECT_EQ(reader->getStatementTransactionSnapshot(), nullptr);

    TransactionManager *txn_mgr = db_.transaction_manager();
    ASSERT_NE(txn_mgr, nullptr);

    auto visibility_context = txn_mgr->resolveVisibilityContext(xid_reader, reader.get());
    EXPECT_FALSE(visibility_context.valid);
    EXPECT_EQ(visibility_context.reason, VisibilityReason::MISSING_SNAPSHOT);

    ASSERT_EQ(writer->commit(&err_ctx), Status::OK);

    StorageEngine *storage = db_.storage_engine();
    ASSERT_NE(storage, nullptr);

    ConnectionContext::setCurrent(reader.get());
    EXPECT_FALSE(storage->isVisible(xid_writer, 0, xid_reader))
        << "Active READ CONSISTENCY statements must not fall back to current-version reads";
}

TEST_F(ConnectionContextTest, VisibilityResolverUsesRetainedSnapshotForSnapshotIsolation)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> reader;

    ASSERT_EQ(db_.connect(reader, &err_ctx), Status::OK);
    ASSERT_EQ(reader->startTransaction(false, IsolationLevel::SNAPSHOT, true, &err_ctx),
              Status::OK);

    TransactionManager *txn_mgr = db_.transaction_manager();
    ASSERT_NE(txn_mgr, nullptr);

    const auto visibility_context =
        txn_mgr->resolveVisibilityContext(reader->getCurrentXid(), reader.get());
    EXPECT_TRUE(visibility_context.valid);
    EXPECT_EQ(visibility_context.mode, VisibilityMode::SNAPSHOT);
    EXPECT_EQ(visibility_context.reason, VisibilityReason::NONE);
    EXPECT_EQ(visibility_context.reader_xid, reader->getCurrentXid());
    EXPECT_EQ(visibility_context.snapshot, reader->getRetainedTransactionSnapshot());
}

TEST_F(ConnectionContextTest, RuntimeVisibilityServiceUsesCurrentSnapshotContext)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> writer;
    std::unique_ptr<ConnectionContext> reader;

    ASSERT_EQ(db_.connect(writer, &err_ctx), Status::OK);
    uint64_t xid_writer = writer->getCurrentXid();

    ASSERT_EQ(db_.connect(reader, &err_ctx), Status::OK);
    ASSERT_EQ(reader->startTransaction(false, IsolationLevel::SNAPSHOT, true, &err_ctx),
              Status::OK);
    uint64_t xid_reader = reader->getCurrentXid();
    ASSERT_LT(xid_writer, xid_reader);

    ASSERT_EQ(writer->commit(&err_ctx), Status::OK);

    TransactionManager *txn_mgr = db_.transaction_manager();
    ASSERT_NE(txn_mgr, nullptr);

    ConnectionContext::setCurrent(reader.get());

    const auto transaction_decision =
        txn_mgr->evaluateRuntimeTransactionVisibility(xid_writer, xid_reader, nullptr);
    EXPECT_FALSE(transaction_decision.visible);
    EXPECT_EQ(transaction_decision.mode, VisibilityMode::SNAPSHOT);
    EXPECT_EQ(transaction_decision.reason, VisibilityReason::ACTIVE_IN_SNAPSHOT);

    const auto record_decision =
        txn_mgr->evaluateRuntimeRecordVisibility(xid_writer, 0, xid_reader, nullptr);
    EXPECT_FALSE(record_decision.visible);
    EXPECT_FALSE(record_decision.create_visible);
    EXPECT_FALSE(record_decision.delete_visible);
    EXPECT_EQ(record_decision.mode, VisibilityMode::SNAPSHOT);
    EXPECT_EQ(record_decision.create_decision.reason, VisibilityReason::ACTIVE_IN_SNAPSHOT);
    EXPECT_EQ(record_decision.delete_decision.reason, VisibilityReason::DELETE_NOT_PRESENT);
}

TEST_F(ConnectionContextTest, RuntimeVisibilityServiceRejectsMissingStatementSnapshot)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> writer;
    std::unique_ptr<ConnectionContext> reader;

    ASSERT_EQ(db_.connect(writer, &err_ctx), Status::OK);
    uint64_t xid_writer = writer->getCurrentXid();

    ASSERT_EQ(db_.connect(reader, &err_ctx), Status::OK);
    ASSERT_EQ(reader->startTransaction(false,
                                       IsolationLevel::READ_COMMITTED_READ_CONSISTENCY,
                                       true,
                                       &err_ctx),
              Status::OK);
    uint64_t xid_reader = reader->getCurrentXid();
    ASSERT_LT(xid_writer, xid_reader);

    ASSERT_EQ(reader->beginStatementTracking("SELECT * FROM users.public.t", &err_ctx),
              Status::OK);
    ASSERT_NE(reader->getStatementTransactionSnapshot(), nullptr);
    ASSERT_EQ(reader->clearStatementXID(&err_ctx), Status::OK);
    EXPECT_EQ(reader->getStatementTransactionSnapshot(), nullptr);

    ASSERT_EQ(writer->commit(&err_ctx), Status::OK);

    TransactionManager *txn_mgr = db_.transaction_manager();
    ASSERT_NE(txn_mgr, nullptr);

    ConnectionContext::setCurrent(reader.get());

    const auto transaction_decision =
        txn_mgr->evaluateRuntimeTransactionVisibility(xid_writer, xid_reader, nullptr);
    EXPECT_FALSE(transaction_decision.visible);
    EXPECT_EQ(transaction_decision.reason, VisibilityReason::MISSING_SNAPSHOT);

    const auto record_decision =
        txn_mgr->evaluateRuntimeRecordVisibility(xid_writer, 0, xid_reader, nullptr);
    EXPECT_FALSE(record_decision.visible);
    EXPECT_FALSE(record_decision.create_visible);
    EXPECT_FALSE(record_decision.delete_visible);
    EXPECT_EQ(record_decision.create_decision.reason, VisibilityReason::MISSING_SNAPSHOT);
    EXPECT_EQ(record_decision.delete_decision.reason, VisibilityReason::DELETE_NOT_PRESENT);
}

TEST_F(ConnectionContextTest, SnapshotMarkersPersistAcrossCleanReopen)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    ASSERT_EQ(db_.connect(conn, &err_ctx), Status::OK);
    TransactionManager *txn_mgr = db_.transaction_manager();
    ASSERT_NE(txn_mgr, nullptr);

    uint64_t generation_before_shutdown = txn_mgr->getInventoryGeneration();
    const TransactionSnapshot *retained = conn->getRetainedTransactionSnapshot();
    ASSERT_NE(retained, nullptr);
    EXPECT_EQ(txn_mgr->getOldestActiveXid(), conn->getCurrentXid());
    EXPECT_EQ(txn_mgr->getOldestSnapshot(), retained->snapshot_txid_low);
    EXPECT_EQ(txn_mgr->getOldestSnapshotSerial(), retained->snapshot_serial);

    ASSERT_EQ(conn->shutdownTransaction(&err_ctx), Status::OK);
    uint64_t expected_next_xid = txn_mgr->getCurrentXid() + 1;
    EXPECT_EQ(txn_mgr->getOldestActiveXid(), expected_next_xid);
    EXPECT_EQ(txn_mgr->getOldestSnapshot(), expected_next_xid);
    EXPECT_EQ(txn_mgr->getOldestSnapshotSerial(), 0u);

    conn.reset();
    db_.close();

    ASSERT_EQ(db_.open(test_db_path_, &err_ctx), Status::OK);
    TransactionManager *reopened_txn_mgr = db_.transaction_manager();
    ASSERT_NE(reopened_txn_mgr, nullptr);
    EXPECT_TRUE(db_.last_shutdown_was_clean());
    EXPECT_EQ(db_.restart_generation(), 0u);
    EXPECT_EQ(db_.last_clean_shutdown_generation() + 1, db_.startup_generation());
    EXPECT_EQ(reopened_txn_mgr->getOldestActiveXid(), reopened_txn_mgr->getCurrentXid() + 1);
    EXPECT_EQ(reopened_txn_mgr->getOldestSnapshot(), reopened_txn_mgr->getCurrentXid() + 1);
    EXPECT_EQ(reopened_txn_mgr->getInventoryGeneration(), generation_before_shutdown);
    EXPECT_EQ(reopened_txn_mgr->getOldestSnapshotSerial(), 0u);
}

// Test READ_COMMITTED_READ_CONSISTENCY - transaction end clears statement XID
TEST_F(ConnectionContextTest, ReadCommittedReadConsistencyTransactionEnd)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Set to READ_COMMITTED_READ_CONSISTENCY
    s = conn->startTransaction(false, IsolationLevel::READ_COMMITTED_READ_CONSISTENCY, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Create statement XID
    ASSERT_EQ(conn->createStatementXID(&err_ctx), Status::OK);
    uint64_t stmt_xid = conn->getStatementXID();
    ASSERT_EQ(stmt_xid, conn->getCurrentXid());

    // Commit transaction - should clear statement XID
    s = conn->commit(&err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Statement XID should now follow the new transaction XID
    EXPECT_EQ(conn->getStatementXID(), conn->getCurrentXid())
        << "Statement XID should be cleared on commit";

    // Create another statement XID
    ASSERT_EQ(conn->createStatementXID(&err_ctx), Status::OK);
    uint64_t stmt_xid2 = conn->getStatementXID();
    ASSERT_EQ(stmt_xid2, conn->getCurrentXid());

    // Rollback transaction - should also clear statement XID
    s = conn->rollback(&err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Statement XID should now follow the new transaction XID
    EXPECT_EQ(conn->getStatementXID(), conn->getCurrentXid())
        << "Statement XID should be cleared on rollback";
}

// Test READ_COMMITTED_READ_CONSISTENCY - multiple statement XIDs
TEST_F(ConnectionContextTest, ReadCommittedReadConsistencyMultipleStatements)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Set to READ_COMMITTED_READ_CONSISTENCY
    s = conn->startTransaction(false, IsolationLevel::READ_COMMITTED_READ_CONSISTENCY, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Simulate multiple statements within same transaction
    for (int i = 0; i < 3; ++i) {
        // Statement start - create statement XID
        ASSERT_EQ(conn->createStatementXID(&err_ctx), Status::OK);
        EXPECT_EQ(conn->getStatementXID(), conn->getCurrentXid())
            << "Statement XID " << i << " should match current XID";

        // Statement end - clear statement XID
        ASSERT_EQ(conn->clearStatementXID(&err_ctx), Status::OK);
        EXPECT_EQ(conn->getStatementXID(), conn->getCurrentXid())
            << "Statement XID " << i << " should be cleared";
    }

    // Transaction should still be active
    EXPECT_NE(conn->getCurrentXid(), 0u) << "Transaction should still be active";
}
