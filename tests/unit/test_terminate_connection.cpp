/**
 * Test: TERMINATE_CONNECTION Policy for Long Transaction Monitor
 *
 * This test verifies that the long transaction monitor can successfully
 * terminate connections with long-running transactions when using the
 * TERMINATE_CONNECTION policy.
 *
 * Test Flow:
 * 1. Create database and connection
 * 2. Configure long transaction monitor with TERMINATE_CONNECTION policy
 * 3. Start monitoring thread
 * 4. Simulate long transaction by sleeping
 * 5. Verify connection termination is requested
 * 6. Verify commit/rollback operations fail with IO_ERROR
 * 7. Verify termination flag is properly cleared
 */

#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/long_transaction_monitor.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/logger.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace scratchbird::core;

void test_terminate_connection_policy()
{
    std::cout << "\n=== Test: TERMINATE_CONNECTION Policy ===" << std::endl;

    // Create test database
    const char *db_path = "/tmp/test_terminate_connection.db";
    std::remove(db_path);

    ErrorContext err_ctx;
    Status s = Database::create(db_path, 8192, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create database: " << err_ctx.message << std::endl;
        exit(1);
    }

    Database db;
    s = db.open(db_path, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to open database: " << err_ctx.message << std::endl;
        exit(1);
    }

    std::cout << "✓ Database created and opened" << std::endl;

    // Register backend
    uint32_t proc_id = 0;
    s = ProcArrayManager::registerBackend(&proc_id, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to register backend: " << err_ctx.message << std::endl;
        exit(1);
    }

    std::cout << "✓ Backend registered: proc_id=" << proc_id << std::endl;

    // Create connection context
    ConnectionContext conn(&db, proc_id);
    s = conn.initialize(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to initialize connection: " << err_ctx.message << std::endl;
        exit(1);
    }

    std::cout << "✓ Connection initialized: xid=" << conn.getCurrentXid() << std::endl;

    // Get long transaction monitor
    LongTransactionMonitor *monitor = db.long_transaction_monitor();
    if (!monitor)
    {
        std::cerr << "Long transaction monitor not available" << std::endl;
        exit(1);
    }

    // Configure monitor for TERMINATE_CONNECTION with very short thresholds for testing
    monitor->setWarningThreshold(1);    // 1 second warning
    monitor->setCriticalThreshold(2);   // 2 seconds critical (triggers action)
    monitor->setCheckInterval(1);       // Check every 1 second
    monitor->setPolicy(LongTransactionPolicy::TERMINATE_CONNECTION);
    monitor->enable();

    std::cout << "✓ Long transaction monitor configured: policy=TERMINATE_CONNECTION, "
              << "critical_threshold=2s" << std::endl;

    // Start monitoring thread
    s = monitor->startMonitoring(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to start monitoring: " << err_ctx.message << std::endl;
        exit(1);
    }

    std::cout << "✓ Monitoring thread started" << std::endl;

    // Sleep for 3 seconds to trigger termination (exceeds 2 second critical threshold)
    std::cout << "\n--- Sleeping for 3 seconds to trigger termination..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Check statistics
    LongTransactionStatistics stats = monitor->getStatistics();
    std::cout << "\nMonitor Statistics:" << std::endl;
    std::cout << "  Current long transactions: " << stats.current_long_transactions << std::endl;
    std::cout << "  Warnings logged: " << stats.warnings_logged << std::endl;
    std::cout << "  Connections terminated: " << stats.connections_terminated << std::endl;

    if (stats.connections_terminated == 0)
    {
        std::cerr << "ERROR: Expected termination request but got none" << std::endl;
        exit(1);
    }

    std::cout << "✓ Termination was requested" << std::endl;

    // Verify that termination flag is set
    bool termination_requested = false;
    s = ProcArrayManager::isTerminationRequested(proc_id, &termination_requested, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to check termination status: " << err_ctx.message << std::endl;
        exit(1);
    }

    if (!termination_requested)
    {
        std::cerr << "ERROR: Termination flag not set in ProcArray" << std::endl;
        exit(1);
    }

    std::cout << "✓ Termination flag is set in ProcArray" << std::endl;

    // Try to commit - should fail with IO_ERROR
    std::cout << "\n--- Testing commit with termination requested..." << std::endl;
    s = conn.commit(&err_ctx);
    if (s != Status::IO_ERROR)
    {
        std::cerr << "ERROR: Expected IO_ERROR on commit but got: " << static_cast<int>(s)
                  << std::endl;
        exit(1);
    }

    std::cout << "✓ Commit correctly failed with IO_ERROR" << std::endl;
    std::cout << "  Error message: " << err_ctx.message << std::endl;

    // Verify termination flag was cleared
    s = ProcArrayManager::isTerminationRequested(proc_id, &termination_requested, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to check termination status after commit: " << err_ctx.message
                  << std::endl;
        exit(1);
    }

    if (termination_requested)
    {
        std::cerr << "ERROR: Termination flag should be cleared after handling" << std::endl;
        exit(1);
    }

    std::cout << "✓ Termination flag was cleared after handling" << std::endl;

    // Stop monitoring
    s = monitor->stopMonitoring(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to stop monitoring: " << err_ctx.message << std::endl;
        exit(1);
    }

    std::cout << "✓ Monitoring thread stopped" << std::endl;

    // Unregister backend
    s = ProcArrayManager::unregisterBackend(proc_id, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to unregister backend: " << err_ctx.message << std::endl;
        exit(1);
    }

    std::cout << "✓ Backend unregistered" << std::endl;

    // Close database
    db.close();

    std::cout << "\n=== TERMINATE_CONNECTION Policy Test PASSED ===" << std::endl;
}

void test_terminate_connection_on_rollback()
{
    std::cout << "\n=== Test: TERMINATE_CONNECTION on Rollback ===" << std::endl;

    // Create test database
    const char *db_path = "/tmp/test_terminate_rollback.db";
    std::remove(db_path);

    ErrorContext err_ctx;
    Status s = Database::create(db_path, 8192, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create database: " << err_ctx.message << std::endl;
        exit(1);
    }

    Database db;
    s = db.open(db_path, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to open database: " << err_ctx.message << std::endl;
        exit(1);
    }

    // Register backend
    uint32_t proc_id = 0;
    s = ProcArrayManager::registerBackend(&proc_id, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to register backend: " << err_ctx.message << std::endl;
        exit(1);
    }

    // Create connection context
    ConnectionContext conn(&db, proc_id);
    s = conn.initialize(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to initialize connection: " << err_ctx.message << std::endl;
        exit(1);
    }

    std::cout << "✓ Connection initialized" << std::endl;

    // Get long transaction monitor
    LongTransactionMonitor *monitor = db.long_transaction_monitor();
    if (!monitor)
    {
        std::cerr << "Long transaction monitor not available" << std::endl;
        exit(1);
    }

    // Configure monitor for TERMINATE_CONNECTION
    monitor->setWarningThreshold(1);
    monitor->setCriticalThreshold(2);
    monitor->setCheckInterval(1);
    monitor->setPolicy(LongTransactionPolicy::TERMINATE_CONNECTION);
    monitor->enable();

    // Start monitoring thread
    s = monitor->startMonitoring(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to start monitoring: " << err_ctx.message << std::endl;
        exit(1);
    }

    // Sleep to trigger termination
    std::cout << "--- Sleeping for 3 seconds to trigger termination..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Try to rollback - should fail with IO_ERROR
    std::cout << "--- Testing rollback with termination requested..." << std::endl;
    s = conn.rollback(&err_ctx);
    if (s != Status::IO_ERROR)
    {
        std::cerr << "ERROR: Expected IO_ERROR on rollback but got: " << static_cast<int>(s)
                  << std::endl;
        exit(1);
    }

    std::cout << "✓ Rollback correctly failed with IO_ERROR" << std::endl;
    std::cout << "  Error message: " << err_ctx.message << std::endl;

    // Stop monitoring
    monitor->stopMonitoring(&err_ctx);

    // Cleanup
    ProcArrayManager::unregisterBackend(proc_id, &err_ctx);
    db.close();

    std::cout << "\n=== TERMINATE_CONNECTION on Rollback Test PASSED ===" << std::endl;
}


TEST(TerminateConnectionTest, Comprehensive) {

    try
    {
        test_terminate_connection_policy();
        test_terminate_connection_on_rollback();

        std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
        return;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        FAIL(); return;
    }

}
