#include <gtest/gtest.h>
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <thread>

using namespace scratchbird::core;

class ConnectionContextTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test database
        test_db_path_ = "/tmp/test_connection_context.sbrd";

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

// Test snapshot creation for SNAPSHOT isolation
TEST_F(ConnectionContextTest, SnapshotCreation)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Default isolation is SNAPSHOT, so snapshot should exist
    const TransactionManager::Snapshot* snapshot = conn->getSnapshot();
    EXPECT_NE(snapshot, nullptr) << "Snapshot should exist for SNAPSHOT isolation";

    if (snapshot != nullptr) {
        EXPECT_NE(snapshot->xmax, 0u) << "Snapshot should have valid xmax";
    }
}

// Test table reservation
TEST_F(ConnectionContextTest, TableReservation)
{
    ErrorContext err_ctx;
    std::unique_ptr<ConnectionContext> conn;

    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Create table reservations
    std::vector<ConnectionContext::TableReservation> reservations;
    reservations.push_back({"test_table1", TableLockMode::SHARED, false});
    reservations.push_back({"test_table2", TableLockMode::PROTECTED, true});

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
