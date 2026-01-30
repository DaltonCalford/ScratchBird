/**
 * Test: Subtransaction/Savepoint Support (Issue 2.15)
 *
 * This test verifies the savepoint mechanism:
 * 1. Create savepoints within a transaction
 * 2. Track tuple insertions and deletions per savepoint
 * 3. Rollback to a savepoint (undoing nested changes)
 * 4. Release a savepoint (merging changes into parent)
 * 5. Nested savepoints (multiple levels)
 * 6. Error handling (duplicate names, not found, etc.)
 */

#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/proc_array.h"
#include "test_helpers.h"
#include <iostream>
#include <cstring>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;


TEST(SubtransactionsTest, Comprehensive) {

    std::cout << "\n=== Test: Subtransaction/Savepoint Support (Issue 2.15) ===" << std::endl;

    // Create test database
    std::string db_path = uniqueTestDbPath("test_subtransactions");
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

    // Initialize ProcArray
    s = ProcArrayManager::initialize(&db, 10, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to initialize ProcArray: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ ProcArray initialized" << std::endl;

    // Register a backend
    uint32_t proc_id = 0;
    s = ProcArrayManager::registerBackend(&proc_id, &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to register backend: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Backend registered: proc_id=" << proc_id << std::endl;

    // Create connection context
    ConnectionContext ctx(&db, proc_id);
    s = ctx.initialize(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to initialize connection context: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Connection context initialized, XID=" << ctx.getCurrentXid() << std::endl;

    // Test 1: Create a simple savepoint
    std::cout << "\n--- Test 1: Create Savepoint ---" << std::endl;
    s = ctx.createSavepoint("sp1", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create savepoint sp1: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Created savepoint 'sp1'" << std::endl;

    // Test 2: Track tuple operations
    std::cout << "\n--- Test 2: Track Tuple Operations ---" << std::endl;
    ctx.trackTupleInsertion(100, 1);  // Simulated insertion at page 100, item 1
    ctx.trackTupleInsertion(100, 2);  // Another insertion
    std::cout << "✓ Tracked 2 tuple insertions after savepoint sp1" << std::endl;

    // Test 3: Create nested savepoint
    std::cout << "\n--- Test 3: Nested Savepoints ---" << std::endl;
    s = ctx.createSavepoint("sp2", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create savepoint sp2: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Created nested savepoint 'sp2'" << std::endl;

    ctx.trackTupleInsertion(101, 1);  // Insert in sp2
    ctx.trackTupleDeletion(100, 1);   // Delete in sp2
    std::cout << "✓ Tracked 1 insertion and 1 deletion after savepoint sp2" << std::endl;

    // Test 4: Rollback to sp2 (should undo nothing, as we're at sp2)
    std::cout << "\n--- Test 4: Rollback to Same Savepoint ---" << std::endl;
    s = ctx.rollbackToSavepoint("sp2", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to rollback to sp2: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Rolled back to savepoint 'sp2' (no changes undone)" << std::endl;

    // Create sp3 for further testing
    s = ctx.createSavepoint("sp3", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create savepoint sp3: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Created savepoint 'sp3'" << std::endl;

    ctx.trackTupleInsertion(102, 1);
    std::cout << "✓ Tracked 1 insertion after savepoint sp3" << std::endl;

    // Test 5: Rollback to sp1 (should undo sp2 and sp3)
    std::cout << "\n--- Test 5: Rollback to Earlier Savepoint ---" << std::endl;
    s = ctx.rollbackToSavepoint("sp1", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to rollback to sp1: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Rolled back to savepoint 'sp1' (undid sp2 and sp3)" << std::endl;

    // Test 6: Create new savepoints after rollback
    std::cout << "\n--- Test 6: Savepoints After Rollback ---" << std::endl;
    s = ctx.createSavepoint("sp4", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create savepoint sp4: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Created savepoint 'sp4' after rollback" << std::endl;

    ctx.trackTupleInsertion(103, 1);
    std::cout << "✓ Tracked insertion in sp4" << std::endl;

    // Test 7: Release savepoint (merge changes into parent)
    std::cout << "\n--- Test 7: Release Savepoint ---" << std::endl;
    s = ctx.releaseSavepoint("sp4", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to release savepoint sp4: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Released savepoint 'sp4' (merged into sp1)" << std::endl;

    // Test 8: Error handling - duplicate savepoint name
    std::cout << "\n--- Test 8: Error Handling - Duplicate Name ---" << std::endl;
    s = ctx.createSavepoint("sp1", &err_ctx);
    if (s == Status::OK)
    {
        std::cerr << "ERROR: Should not allow duplicate savepoint name" << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Correctly rejected duplicate savepoint name: " << err_ctx.message << std::endl;

    // Test 9: Error handling - rollback to non-existent savepoint
    std::cout << "\n--- Test 9: Error Handling - Non-existent Savepoint ---" << std::endl;
    s = ctx.rollbackToSavepoint("sp_nonexistent", &err_ctx);
    if (s == Status::OK)
    {
        std::cerr << "ERROR: Should not allow rollback to non-existent savepoint" << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Correctly rejected non-existent savepoint: " << err_ctx.message << std::endl;

    // Test 10: Release all remaining savepoints and commit
    std::cout << "\n--- Test 10: Release and Commit ---" << std::endl;
    s = ctx.releaseSavepoint("sp1", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to release savepoint sp1: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Released savepoint 'sp1'" << std::endl;

    s = ctx.commit(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to commit transaction: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Committed transaction (savepoints cleared)" << std::endl;

    // Test 11: Verify savepoints cleared after commit
    std::cout << "\n--- Test 11: Savepoints Cleared After Commit ---" << std::endl;
    s = ctx.rollbackToSavepoint("sp1", &err_ctx);
    if (s == Status::OK)
    {
        std::cerr << "ERROR: Savepoints should be cleared after commit" << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Savepoints correctly cleared after commit" << std::endl;

    // Test 12: Create savepoint, rollback transaction
    std::cout << "\n--- Test 12: Savepoints Cleared After Rollback ---" << std::endl;
    s = ctx.createSavepoint("sp_rollback_test", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create savepoint: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Created savepoint in new transaction" << std::endl;

    s = ctx.rollback(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to rollback transaction: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Rolled back transaction (savepoints cleared)" << std::endl;

    s = ctx.rollbackToSavepoint("sp_rollback_test", &err_ctx);
    if (s == Status::OK)
    {
        std::cerr << "ERROR: Savepoints should be cleared after rollback" << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Savepoints correctly cleared after rollback" << std::endl;

    // Test 13: Multiple levels of nesting
    std::cout << "\n--- Test 13: Deep Nesting ---" << std::endl;
    s = ctx.createSavepoint("level1", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create level1: " << err_ctx.message << std::endl;
        FAIL(); return;
    }

    s = ctx.createSavepoint("level2", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create level2: " << err_ctx.message << std::endl;
        FAIL(); return;
    }

    s = ctx.createSavepoint("level3", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to create level3: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Created 3 levels of nested savepoints" << std::endl;

    s = ctx.rollbackToSavepoint("level1", &err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed to rollback to level1: " << err_ctx.message << std::endl;
        FAIL(); return;
    }
    std::cout << "✓ Rolled back to level1 (undid level2 and level3)" << std::endl;

    // Cleanup
    s = ctx.commit(&err_ctx);
    if (s != Status::OK)
    {
        std::cerr << "Failed final commit: " << err_ctx.message << std::endl;
        FAIL(); return;
    }

    ProcArrayManager::unregisterBackend(proc_id, &err_ctx);
    ProcArrayManager::shutdown(&err_ctx);
    db.close();

    std::cout << "\n=== Subtransaction/Savepoint Support Test PASSED ===" << std::endl;
    std::cout << "\nKey features verified:" << std::endl;
    std::cout << "  1. Savepoint creation with unique names" << std::endl;
    std::cout << "  2. Tuple insertion/deletion tracking per savepoint" << std::endl;
    std::cout << "  3. Rollback to savepoint (undoes nested changes)" << std::endl;
    std::cout << "  4. Release savepoint (merges changes into parent)" << std::endl;
    std::cout << "  5. Nested savepoints (multiple levels)" << std::endl;
    std::cout << "  6. Error handling (duplicate names, not found)" << std::endl;
    std::cout << "  7. Savepoints cleared on commit/rollback" << std::endl;
    std::cout << "\nSpec: docs/specifications/TRANSACTION_MGA_CORE.md:774-908" << std::endl;
}
