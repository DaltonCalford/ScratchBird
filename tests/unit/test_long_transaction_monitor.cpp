#include <gtest/gtest.h>
#include "scratchbird/core/long_transaction_monitor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <thread>
#include <chrono>

using namespace scratchbird::core;

class LongTransactionMonitorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test database
        test_db_path_ = "test_long_txn_monitor.sbrd";

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

// Test basic monitor initialization
TEST_F(LongTransactionMonitorTest, BasicInitialization)
{
    ErrorContext err_ctx;

    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr) << "Database should have long transaction monitor";

    // Monitor should be running (started by Database::open())
    EXPECT_TRUE(monitor->isMonitoring()) << "Monitor should be running";
    EXPECT_TRUE(monitor->isEnabled()) << "Monitor should be enabled";

    // Check default settings
    EXPECT_GT(monitor->getWarningThreshold(), 0u);
    EXPECT_GT(monitor->getCriticalThreshold(), 0u);
    EXPECT_GT(monitor->getCheckInterval(), 0u);
}

// Test monitor statistics
TEST_F(LongTransactionMonitorTest, Statistics)
{
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    LongTransactionStatistics stats = monitor->getStatistics();

    // Initially, all counters should be zero
    EXPECT_EQ(stats.warnings_logged, 0u);
    EXPECT_EQ(stats.readonly_rolled_back, 0u);
    EXPECT_EQ(stats.readwrite_rolled_back, 0u);
    EXPECT_EQ(stats.connections_terminated, 0u);
    EXPECT_EQ(stats.current_long_transactions, 0u);
}

// Test manual check for long transactions
TEST_F(LongTransactionMonitorTest, ManualCheck)
{
    ErrorContext err_ctx;
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    // Create a connection
    std::unique_ptr<ConnectionContext> conn;
    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Set very low thresholds for testing
    monitor->setWarningThreshold(1);  // 1 second
    monitor->setCriticalThreshold(2); // 2 seconds

    // Initially, transaction is new - should not be flagged
    uint32_t long_txn_count = monitor->checkLongTransactions(&err_ctx);
    EXPECT_EQ(long_txn_count, 0u) << "New transaction should not be flagged as long";

    // Wait for transaction to age
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Now check again - transaction should be flagged
    long_txn_count = monitor->checkLongTransactions(&err_ctx);
    EXPECT_GT(long_txn_count, 0u) << "Aged transaction should be flagged as long";

    LongTransactionStatistics stats = monitor->getStatistics();
    EXPECT_GT(stats.warnings_logged, 0u) << "Warnings should be logged";
}

// Test enable/disable functionality
TEST_F(LongTransactionMonitorTest, EnableDisable)
{
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    // Initially enabled
    EXPECT_TRUE(monitor->isEnabled());

    // Disable
    monitor->disable();
    EXPECT_FALSE(monitor->isEnabled());

    // Re-enable
    monitor->enable();
    EXPECT_TRUE(monitor->isEnabled());
}

// Test policy configuration
TEST_F(LongTransactionMonitorTest, PolicyConfiguration)
{
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    // Test each policy
    monitor->setPolicy(LongTransactionPolicy::LOG);
    EXPECT_EQ(monitor->getPolicy(), LongTransactionPolicy::LOG);

    monitor->setPolicy(LongTransactionPolicy::ROLLBACK_READONLY);
    EXPECT_EQ(monitor->getPolicy(), LongTransactionPolicy::ROLLBACK_READONLY);

    monitor->setPolicy(LongTransactionPolicy::ROLLBACK_ALL);
    EXPECT_EQ(monitor->getPolicy(), LongTransactionPolicy::ROLLBACK_ALL);

    monitor->setPolicy(LongTransactionPolicy::TERMINATE_CONNECTION);
    EXPECT_EQ(monitor->getPolicy(), LongTransactionPolicy::TERMINATE_CONNECTION);
}

// Test threshold configuration
TEST_F(LongTransactionMonitorTest, ThresholdConfiguration)
{
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    // Set custom thresholds
    monitor->setWarningThreshold(120);  // 2 minutes
    monitor->setCriticalThreshold(600); // 10 minutes
    monitor->setCheckInterval(30);      // 30 seconds

    EXPECT_EQ(monitor->getWarningThreshold(), 120u);
    EXPECT_EQ(monitor->getCriticalThreshold(), 600u);
    EXPECT_EQ(monitor->getCheckInterval(), 30u);
}

// Test monitoring with multiple connections
TEST_F(LongTransactionMonitorTest, MultipleConnections)
{
    ErrorContext err_ctx;
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    // Set low thresholds for testing
    monitor->setWarningThreshold(1);

    // Create multiple connections
    std::vector<std::unique_ptr<ConnectionContext>> connections;
    for (int i = 0; i < 3; ++i) {
        std::unique_ptr<ConnectionContext> conn;
        Status s = db_.connect(conn, &err_ctx);
        ASSERT_EQ(s, Status::OK) << "Failed to create connection " << i;
        connections.push_back(std::move(conn));
    }

    // Wait for transactions to age
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check - all 3 transactions should be flagged
    uint32_t long_txn_count = monitor->checkLongTransactions(&err_ctx);
    EXPECT_EQ(long_txn_count, 3u) << "All 3 connections should have long transactions";

    LongTransactionStatistics stats = monitor->getStatistics();
    EXPECT_EQ(stats.current_long_transactions, 3u);
}

// Test transaction age tracking in PCB
TEST_F(LongTransactionMonitorTest, TransactionAgeTracking)
{
    ErrorContext err_ctx;

    // Create a connection
    std::unique_ptr<ConnectionContext> conn;
    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint32_t proc_id = conn->getProcId();

    // Get all active backends and find our connection
    std::vector<ProcessControlBlock> backends;
    s = ProcArrayManager::getAllActiveBackends(&backends, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    bool found = false;
    for (const auto& backend : backends) {
        if (backend.proc_id == proc_id) {
            found = true;
            EXPECT_NE(backend.xid, 0u) << "Backend should have active transaction";
            EXPECT_NE(backend.xact_start_time, 0u) << "Backend should have transaction start time";
            EXPECT_GE(backend.isolation_level, 0u) << "Backend should have isolation level set";
            break;
        }
    }

    EXPECT_TRUE(found) << "Should find backend in ProcArray";
}

// Test read-only flag in PCB
TEST_F(LongTransactionMonitorTest, ReadOnlyFlagTracking)
{
    ErrorContext err_ctx;

    // Create a read-write connection
    std::unique_ptr<ConnectionContext> conn_rw;
    Status s = db_.connect(conn_rw, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    uint32_t proc_id_rw = conn_rw->getProcId();

    // Create a read-only connection
    std::unique_ptr<ConnectionContext> conn_ro;
    s = db_.connect(conn_ro, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Make it read-only
    s = conn_ro->startTransaction(true, IsolationLevel::SNAPSHOT, true, &err_ctx);
    ASSERT_EQ(s, Status::OK);
    ASSERT_TRUE(conn_ro->isReadOnly());

    uint32_t proc_id_ro = conn_ro->getProcId();

    // Get all active backends
    std::vector<ProcessControlBlock> backends;
    s = ProcArrayManager::getAllActiveBackends(&backends, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Check flags
    for (const auto& backend : backends) {
        if (backend.proc_id == proc_id_rw) {
            EXPECT_FALSE(backend.is_read_only) << "Read-write connection should not be marked read-only";
        }
        if (backend.proc_id == proc_id_ro) {
            EXPECT_TRUE(backend.is_read_only) << "Read-only connection should be marked read-only";
        }
    }
}

// Test transaction age calculation
TEST_F(LongTransactionMonitorTest, TransactionAgeCalculation)
{
    ErrorContext err_ctx;

    // Create a connection
    std::unique_ptr<ConnectionContext> conn;
    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    auto start_time_1 = conn->getTransactionStartTime();

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get current time
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    );

    // Calculate age
    uint64_t age_microseconds = (now - start_time_1).count();
    uint64_t age_milliseconds = age_microseconds / 1000;

    EXPECT_GE(age_milliseconds, 100u) << "Transaction should be at least 100ms old";
}

// Test monitor start/stop
TEST_F(LongTransactionMonitorTest, StartStopMonitoring)
{
    ErrorContext err_ctx;
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    // Initially monitoring (started by Database::open())
    EXPECT_TRUE(monitor->isMonitoring());

    // Stop monitoring
    Status s = monitor->stopMonitoring(&err_ctx);
    EXPECT_EQ(s, Status::OK);
    EXPECT_FALSE(monitor->isMonitoring());

    // Start monitoring again
    s = monitor->startMonitoring(&err_ctx);
    EXPECT_EQ(s, Status::OK);
    EXPECT_TRUE(monitor->isMonitoring());
}

// Test LOG policy (default - just logs)
TEST_F(LongTransactionMonitorTest, LogPolicy)
{
    ErrorContext err_ctx;
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    // Set LOG policy
    monitor->setPolicy(LongTransactionPolicy::LOG);
    monitor->setWarningThreshold(1);
    monitor->setCriticalThreshold(1);

    // Create connection
    std::unique_ptr<ConnectionContext> conn;
    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Wait for transaction to age
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check - should just log
    uint32_t long_txn_count = monitor->checkLongTransactions(&err_ctx);
    EXPECT_GT(long_txn_count, 0u);

    LongTransactionStatistics stats = monitor->getStatistics();
    EXPECT_GT(stats.warnings_logged, 0u) << "Should have logged warnings";
    EXPECT_EQ(stats.readonly_rolled_back, 0u) << "Should not rollback with LOG policy";
    EXPECT_EQ(stats.readwrite_rolled_back, 0u);
    EXPECT_EQ(stats.connections_terminated, 0u);
}

// Test warning vs critical thresholds
TEST_F(LongTransactionMonitorTest, WarningVsCriticalThresholds)
{
    ErrorContext err_ctx;
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    // Set thresholds
    monitor->setWarningThreshold(1);  // 1 second
    monitor->setCriticalThreshold(3); // 3 seconds

    // Create connection
    std::unique_ptr<ConnectionContext> conn;
    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    // Wait 1.5 seconds - should hit warning but not critical
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    monitor->checkLongTransactions(&err_ctx);
    LongTransactionStatistics stats1 = monitor->getStatistics();
    uint64_t warnings_after_warning = stats1.warnings_logged;

    // Wait another 2 seconds - should hit critical
    std::this_thread::sleep_for(std::chrono::seconds(2));

    monitor->checkLongTransactions(&err_ctx);
    LongTransactionStatistics stats2 = monitor->getStatistics();

    EXPECT_GT(warnings_after_warning, 0u) << "Should log warning after 1.5 seconds";
    EXPECT_GT(stats2.warnings_logged, warnings_after_warning) << "Should continue logging";
}

// Test concurrent monitoring
TEST_F(LongTransactionMonitorTest, ConcurrentMonitoring)
{
    ErrorContext err_ctx;
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    monitor->setWarningThreshold(1);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Create connections in multiple threads
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, &success_count]() {
            ErrorContext thread_err_ctx;
            std::unique_ptr<ConnectionContext> conn;

            Status s = db_.connect(conn, &thread_err_ctx);
            if (s == Status::OK) {
                // Keep transaction active
                std::this_thread::sleep_for(std::chrono::seconds(2));
                success_count++;
            }
        });
    }

    // Wait a bit then check
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // Manual check while threads are running
    uint32_t long_txn_count = monitor->checkLongTransactions(&err_ctx);
    EXPECT_GT(long_txn_count, 0u) << "Should detect long transactions in concurrent scenario";

    // Wait for threads
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), 3) << "All threads should create connections";
}

// Test statistics accuracy
TEST_F(LongTransactionMonitorTest, StatisticsAccuracy)
{
    ErrorContext err_ctx;
    auto* monitor = db_.long_transaction_monitor();
    ASSERT_NE(monitor, nullptr);

    monitor->setWarningThreshold(1);
    monitor->setCriticalThreshold(1);

    // Get initial stats
    LongTransactionStatistics stats_before = monitor->getStatistics();

    // Create connection and wait
    std::unique_ptr<ConnectionContext> conn;
    Status s = db_.connect(conn, &err_ctx);
    ASSERT_EQ(s, Status::OK);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check twice
    monitor->checkLongTransactions(&err_ctx);
    monitor->checkLongTransactions(&err_ctx);

    LongTransactionStatistics stats_after = monitor->getStatistics();

    // Warnings should increase
    EXPECT_GT(stats_after.warnings_logged, stats_before.warnings_logged);

    // Last check time should be updated
    EXPECT_GT(stats_after.last_check_time, stats_before.last_check_time);

    // Current long transactions should be set
    EXPECT_EQ(stats_after.current_long_transactions, 1u);
}
