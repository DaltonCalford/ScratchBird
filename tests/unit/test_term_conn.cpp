/**
 * Test: TERMINATE_CONNECTION Policy for Long Transaction Monitor
 *
 * This test verifies that the long transaction monitor can successfully
 * terminate connections with long-running transactions when using the
 * TERMINATE_CONNECTION policy.
 */

#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/long_transaction_monitor.h"
#include "scratchbird/core/proc_array.h"
#include "test_helpers.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;


TEST(TermConnTest, Comprehensive) {

    std::cout << "\n=== Test: TERMINATE_CONNECTION Policy ===" << std::endl;

    // Create test database
    std::string db_path = uniqueTestDbPath("test_terminate_conn");
    std::remove(db_path.c_str());

    ErrorContext err_ctx;
    Status s = Database::create(db_path.c_str(), 8192, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create database: " << err_ctx.message << std::endl;
        FAIL(); return;
    }

    Database db;
    s = db.open(db_path.c_str(), &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to open database: " << err_ctx.message << std::endl;
        FAIL(); return;
    }

    std::cout << "✓ Database created and opened" << std::endl;

    // Register backend
    uint32_t proc_id = 0;
    s = ProcArrayManager::registerBackend(&proc_id, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to register backend: " << err_ctx.message << std::endl;
        FAIL(); return;
    }

    std::cout << "✓ Backend registered: proc_id=" << proc_id << std::endl;

    // Create connection context
    ConnectionContext conn(&db, proc_id);
    s = conn.initialize(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to initialize connection: " << err_ctx.message << std::endl;
        FAIL(); return;
    }

    std::cout << "✓ Connection initialized: xid=" << conn.getCurrentXid() << std::endl;

    // Get long transaction monitor
    LongTransactionMonitor *monitor = db.long_transaction_monitor();
    if (!monitor)
    {
        std::cerr << "Long transaction monitor not available" << std::endl;
        FAIL(); return;
    }

    // Configure monitor for TERMINATE_CONNECTION with very short thresholds for testing
    monitor->setWarningThreshold(1);    // 1 second warning
    monitor->setCriticalThreshold(2);   // 2 seconds critical (triggers action)
    monitor->setCheckInterval(1);       // Check every 1 second
    monitor->setPolicy(LongTransactionPolicy::TERMINATE_CONNECTION);
    monitor->enable();

    std::cout << "✓ Monitor configured: policy=TERMINATE_CONNECTION, critical=2s" << std::endl;

    // Start monitoring thread
    s = monitor->startMonitoring(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to start monitoring: " << err_ctx.message << std::endl;
        FAIL(); return;
    }

    std::cout << "✓ Monitoring thread started" << std::endl;

    // Sleep for 3 seconds to trigger termination (exceeds 2 second critical threshold)
    std::cout << "\n--- Sleeping for 3 seconds to trigger termination..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Check statistics
    LongTransactionStatistics stats = monitor->getStatistics();
    std::cout << "\nMonitor Statistics:" << std::endl;
    std::cout << "  Connections terminated: " << stats.connections_terminated << std::endl;

    if (stats.connections_terminated == 0)
    {
        std::cerr << "ERROR: Expected termination request but got none" << std::endl;
        FAIL(); return;
    }

    std::cout << "✓ Termination was requested" << std::endl;

    // Try to commit - should fail with IO_ERROR
    std::cout << "\n--- Testing commit with termination requested..." << std::endl;
    s = conn.commit(&err_ctx);
    if (s != Status::IO_ERROR)
    {
        std::cerr << "ERROR: Expected IO_ERROR on commit but got: " << static_cast<int>(s)
                  << std::endl;
        FAIL(); return;
    }

    std::cout << "✓ Commit correctly failed with IO_ERROR" << std::endl;
    std::cout << "  Error message: " << err_ctx.message << std::endl;

    // Stop monitoring
    monitor->stopMonitoring(&err_ctx);

    // Cleanup
    ProcArrayManager::unregisterBackend(proc_id, &err_ctx);
    db.close();

    std::cout << "\n=== TERMINATE_CONNECTION Policy Test PASSED ===" << std::endl;
    return;

}
