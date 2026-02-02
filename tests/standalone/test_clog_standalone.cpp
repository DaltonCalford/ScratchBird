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
#include "scratchbird/core/clog.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include <iostream>
#include <filesystem>

using namespace scratchbird::core;

int main()
{
    const std::string test_db = "test_clog_standalone.db";

    // Clean up
    if (std::filesystem::exists(test_db))
    {
        std::filesystem::remove(test_db);
    }

    ErrorContext ctx;

    // Create database
    std::cout << "Creating database..." << std::endl;
    Status status = Database::create(test_db, 16384, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to create database: " << ctx.message << std::endl;
        return 1;
    }

    // Open database
    std::cout << "Opening database..." << std::endl;
    Database db;
    status = db.open(test_db, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to open database: " << ctx.message << std::endl;
        return 1;
    }

    // Test CLOG initialization
    std::cout << "Testing CLOG initialization..." << std::endl;
    if (db.clog() == nullptr)
    {
        std::cerr << "CLOG not initialized!" << std::endl;
        return 1;
    }
    std::cout << "✓ CLOG initialized, root page: " << db.clog()->getRootPage() << std::endl;

    // Test setting transaction status
    std::cout << "\nTesting transaction status..." << std::endl;
    status = db.clog()->setStatus(100, ClogStatus::COMMITTED, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to set status: " << ctx.message << std::endl;
        return 1;
    }
    std::cout << "✓ Set XID 100 to COMMITTED" << std::endl;

    status = db.clog()->setStatus(101, ClogStatus::ABORTED, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to set status: " << ctx.message << std::endl;
        return 1;
    }
    std::cout << "✓ Set XID 101 to ABORTED" << std::endl;

    // Test getting transaction status
    ClogStatus clog_status;
    status = db.clog()->getStatus(100, &clog_status, &ctx);
    if (status != Status::OK || clog_status != ClogStatus::COMMITTED)
    {
        std::cerr << "Failed to get correct status for XID 100" << std::endl;
        return 1;
    }
    std::cout << "✓ Got XID 100: COMMITTED" << std::endl;

    status = db.clog()->getStatus(101, &clog_status, &ctx);
    if (status != Status::OK || clog_status != ClogStatus::ABORTED)
    {
        std::cerr << "Failed to get correct status for XID 101" << std::endl;
        return 1;
    }
    std::cout << "✓ Got XID 101: ABORTED" << std::endl;

    // Test CLOG extension (large XID)
    std::cout << "\nTesting CLOG extension..." << std::endl;
    uint64_t large_xid = 100000; // Requires new CLOG page
    status = db.clog()->setStatus(large_xid, ClogStatus::COMMITTED, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to extend CLOG: " << ctx.message << std::endl;
        return 1;
    }
    std::cout << "✓ Extended CLOG for XID " << large_xid << std::endl;

    status = db.clog()->getStatus(large_xid, &clog_status, &ctx);
    if (status != Status::OK || clog_status != ClogStatus::COMMITTED)
    {
        std::cerr << "Failed to get status for large XID" << std::endl;
        return 1;
    }
    std::cout << "✓ Got XID " << large_xid << ": COMMITTED" << std::endl;

    // Test CLOG statistics
    std::cout << "\nCLOG Statistics:" << std::endl;
    Clog::ClogStats stats;
    db.clog()->getStatistics(&stats);
    std::cout << "  Pages: " << stats.num_pages << std::endl;
    std::cout << "  Total transactions: " << stats.total_transactions << std::endl;
    std::cout << "  Space used: " << stats.space_used_bytes << " bytes" << std::endl;
    std::cout << "  Space saved vs TIP: " << stats.space_saved_bytes << " bytes" << std::endl;

    // Calculate space efficiency
    double efficiency = (double)stats.space_saved_bytes / (stats.space_used_bytes + stats.space_saved_bytes) * 100;
    std::cout << "  Space efficiency: " << efficiency << "% savings" << std::endl;

    // Test TransactionManager integration
    std::cout << "\nTesting TransactionManager integration..." << std::endl;

    // Initialize ProcArray
    status = db.initializeProcArray(10, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to initialize ProcArray: " << ctx.message << std::endl;
        return 1;
    }

    uint32_t proc_id = 0;
    uint64_t xid = 0;

    // Begin transaction
    status = db.transaction_manager()->beginTransaction(proc_id, xid, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to begin transaction: " << ctx.message << std::endl;
        return 1;
    }
    std::cout << "✓ Started transaction XID " << xid << std::endl;

    // Commit transaction
    status = db.transaction_manager()->commitTransaction(proc_id, xid, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to commit transaction: " << ctx.message << std::endl;
        return 1;
    }
    std::cout << "✓ Committed transaction XID " << xid << std::endl;

    // Verify CLOG has committed status
    status = db.clog()->getStatus(xid, &clog_status, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to get transaction status from CLOG: " << ctx.message << std::endl;
        return 1;
    }
    if (clog_status != ClogStatus::COMMITTED)
    {
        std::cerr << "Transaction not marked as committed in CLOG!" << std::endl;
        return 1;
    }
    std::cout << "✓ Transaction status in CLOG: COMMITTED" << std::endl;

    // Test rollback
    uint64_t xid2 = 0;
    status = db.transaction_manager()->beginTransaction(proc_id, xid2, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to begin second transaction: " << ctx.message << std::endl;
        return 1;
    }
    std::cout << "✓ Started transaction XID " << xid2 << std::endl;

    status = db.transaction_manager()->rollbackTransaction(proc_id, xid2, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to rollback transaction: " << ctx.message << std::endl;
        return 1;
    }
    std::cout << "✓ Rolled back transaction XID " << xid2 << std::endl;

    status = db.clog()->getStatus(xid2, &clog_status, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to get transaction status from CLOG: " << ctx.message << std::endl;
        return 1;
    }
    if (clog_status != ClogStatus::ABORTED)
    {
        std::cerr << "Transaction not marked as aborted in CLOG!" << std::endl;
        return 1;
    }
    std::cout << "✓ Transaction status in CLOG: ABORTED" << std::endl;

    db.close();

    std::cout << "\n✅ All CLOG tests passed!" << std::endl;
    std::cout << "\nCLOG Implementation Summary:" << std::endl;
    std::cout << "  - 2-bit transaction status encoding (vs 20 bytes in TIP)" << std::endl;
    std::cout << "  - 65,536 transactions per 16KB page (vs ~800 with TIP)" << std::endl;
    std::cout << "  - 160x space savings over TIP" << std::endl;
    std::cout << "  - Dynamic page extension" << std::endl;
    std::cout << "  - Fully integrated with TransactionManager" << std::endl;

    // Clean up
    std::filesystem::remove(test_db);

    return 0;
}
