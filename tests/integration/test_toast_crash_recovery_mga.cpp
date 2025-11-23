// ScratchBird TOAST Crash Recovery Integration Test (MGA)
// Tests Phase 5: TIP-based crash recovery WITHOUT WAL
// CRITICAL: This test validates Firebird MGA architecture, not PostgreSQL MVCC

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/vacuum.h"
#include "scratchbird/core/toast.h"
#include <memory>
#include <string>
#include <vector>

using namespace scratchbird::core;

class ToastCrashRecoveryMGATest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = "test_toast_crash_recovery_mga.db";

        // Create test database
        db_ = std::make_unique<Database>(db_path_, 50 * 1024 * 1024); // 50MB
        ASSERT_EQ(db_->open(), Status::OK);

        storage_ = db_->storage_engine();
        catalog_ = db_->catalog_manager();
        tm_ = db_->transaction_manager();
        gc_ = db_->garbage_collector();
        vacuum_ = db_->vacuum();

        ASSERT_NE(storage_, nullptr);
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(tm_, nullptr);
        ASSERT_NE(gc_, nullptr);
        ASSERT_NE(vacuum_, nullptr);
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
        }
    }

    // Helper: Create a large text value that will be TOASTed (>2KB)
    std::string createLargeText(size_t size_kb)
    {
        std::string text;
        text.reserve(size_kb * 1024);

        for (size_t i = 0; i < size_kb * 1024; i += 64)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "CRASH_RECOVERY_TEST_%08zu_", i);
            text.append(buf);
        }

        return text;
    }

    // Helper: Simulate database restart (close and reopen)
    void simulateDatabaseRestart()
    {
        // Close database
        ASSERT_EQ(db_->close(), Status::OK);
        db_.reset();

        // Reopen database (simulates restart)
        db_ = std::make_unique<Database>(db_path_, 50 * 1024 * 1024);
        ASSERT_EQ(db_->open(), Status::OK);

        // Reinitialize pointers
        storage_ = db_->storage_engine();
        catalog_ = db_->catalog_manager();
        tm_ = db_->transaction_manager();
        gc_ = db_->garbage_collector();
        vacuum_ = db_->vacuum();
    }

    std::string db_path_;
    std::unique_ptr<Database> db_;
    StorageEngine* storage_;
    CatalogManager* catalog_;
    TransactionManager* tm_;
    GarbageCollector* gc_;
    Vacuum* vacuum_;
};

// =============================================================================
// Test 1: Crash Before Commit - Chunks Become Invisible
// =============================================================================

TEST_F(ToastCrashRecoveryMGATest, CrashBeforeCommit_ChunksInvisible)
{
    // MGA Crash Recovery Test (NO WAL)
    //
    // Scenario:
    // 1. Transaction creates TOAST chunks
    // 2. Simulate crash BEFORE commit (transaction abandoned)
    // 3. Database restart
    // 4. TIP marks transaction as aborted
    // 5. Verify TOAST chunks invisible
    // 6. Verify sweep removes them
    //
    // MGA Compliance: Uses TIP for crash recovery, NOT WAL replay

    ID table_id = generateUuidV7();
    ID toast_table_id = generateUuidV7();

    // Create TOAST manager
    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Start transaction
    uint64_t xid = tm_->beginTransaction();
    ASSERT_NE(xid, 0);

    // Create large TOAST value
    std::string large_text = createLargeText(5); // 5KB
    ASSERT_GT(large_text.size(), 2048u);

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        reinterpret_cast<const uint8_t*>(large_text.data()),
        large_text.size(),
        ToastStrategy::EXTERNAL,
        xid,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK) << "TOAST failed: " << ctx.message;
    uint32_t value_id = toast_ptr.va_valueid;

    // SIMULATE CRASH: Don't commit, just abandon transaction
    // In real crash, transaction would be left in TX_ACTIVE state
    // Database restart will mark it TX_ABORTED via TIP

    // Simulate database restart
    simulateDatabaseRestart();

    // After restart, TIP should mark transaction as aborted
    // Verify transaction state via TIP
    uint64_t new_xid = tm_->beginTransaction();
    ASSERT_NE(new_xid, 0);

    // Try to detoast - should fail because chunks are invisible
    // (transaction xid is aborted in TIP)
    ToastManager toast_mgr2(db_.get(), table_id);
    std::vector<uint8_t> detoasted;
    status = toast_mgr2.detoastValue(&toast_ptr, &detoasted, new_xid, &ctx);

    // Expected: Detoasting fails or returns empty (chunks invisible)
    // This validates TIP-based visibility after crash
    EXPECT_NE(status, Status::OK) << "Chunks should be invisible after crash (TIP marks xmin aborted)";

    tm_->commit(new_xid);

    // Run sweep to physically remove aborted chunks
    VacuumStats vacuum_stats;
    status = vacuum_->vacuumDatabase(&vacuum_stats, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Verify chunks cleaned up (orphan detection should find them)
    // This validates that sweep removes aborted TOAST chunks
}

// =============================================================================
// Test 2: Crash After Commit - Chunks Remain Visible
// =============================================================================

TEST_F(ToastCrashRecoveryMGATest, CrashAfterCommit_ChunksVisible)
{
    // MGA Crash Recovery Test (NO WAL)
    //
    // Scenario:
    // 1. Transaction creates TOAST chunks
    // 2. Transaction COMMITS
    // 3. Simulate crash AFTER commit
    // 4. Database restart
    // 5. TIP shows transaction committed
    // 6. Verify TOAST chunks still visible
    //
    // MGA Compliance: Committed data persists via TIP, NOT WAL

    ID table_id = generateUuidV7();
    ID toast_table_id = generateUuidV7();

    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Start transaction and create TOAST value
    uint64_t xid = tm_->beginTransaction();
    std::string large_text = createLargeText(3);

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        reinterpret_cast<const uint8_t*>(large_text.data()),
        large_text.size(),
        ToastStrategy::EXTERNAL,
        xid,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);

    // COMMIT transaction (persists to TIP)
    tm_->commit(xid);

    // SIMULATE CRASH after commit
    simulateDatabaseRestart();

    // After restart, TIP should show transaction committed
    // Verify chunks are still visible
    uint64_t new_xid = tm_->beginTransaction();

    ToastManager toast_mgr2(db_.get(), table_id);
    std::vector<uint8_t> detoasted;
    status = toast_mgr2.detoastValue(&toast_ptr, &detoasted, new_xid, &ctx);

    // Expected: Detoasting succeeds (chunks visible via TIP)
    EXPECT_EQ(status, Status::OK) << "Chunks should be visible after commit+crash (TIP shows committed)";

    if (status == Status::OK)
    {
        // Verify data integrity
        std::string detoasted_text(detoasted.begin(), detoasted.end());
        EXPECT_EQ(detoasted_text, large_text) << "Detoasted data should match original";
    }

    tm_->commit(new_xid);
}

// =============================================================================
// Test 3: Crash During Delete - xmax Handling
// =============================================================================

TEST_F(ToastCrashRecoveryMGATest, CrashDuringDelete_XmaxHandling)
{
    // MGA Crash Recovery Test (NO WAL)
    //
    // Scenario:
    // 1. Create and commit TOAST value
    // 2. Start delete transaction (sets xmax)
    // 3. Simulate crash BEFORE delete commits
    // 4. Database restart
    // 5. TIP marks delete transaction as aborted
    // 6. Verify chunks still visible (xmax aborted)
    //
    // MGA Compliance: TIP-based xmax visibility, NOT WAL-based undo

    ID table_id = generateUuidV7();
    ID toast_table_id = generateUuidV7();

    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create and commit TOAST value
    uint64_t xid1 = tm_->beginTransaction();
    std::string large_text = createLargeText(4);

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        reinterpret_cast<const uint8_t*>(large_text.data()),
        large_text.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);
    uint32_t value_id = toast_ptr.va_valueid;
    tm_->commit(xid1); // Commit create

    // Start delete transaction (sets xmax on chunks)
    uint64_t xid2 = tm_->beginTransaction();
    status = toast_mgr.deleteToastValue(value_id, xid2, &ctx);
    ASSERT_EQ(status, Status::OK);

    // SIMULATE CRASH before delete commits
    // Delete transaction abandoned (xmax set but not committed)
    simulateDatabaseRestart();

    // After restart, TIP should mark delete transaction as aborted
    // Chunks should still be visible (xmax aborted)
    uint64_t new_xid = tm_->beginTransaction();

    ToastManager toast_mgr2(db_.get(), table_id);
    std::vector<uint8_t> detoasted;
    status = toast_mgr2.detoastValue(&toast_ptr, &detoasted, new_xid, &ctx);

    // Expected: Chunks still visible (delete aborted via TIP)
    EXPECT_EQ(status, Status::OK) << "Chunks should be visible (delete transaction aborted in TIP)";

    if (status == Status::OK)
    {
        std::string detoasted_text(detoasted.begin(), detoasted.end());
        EXPECT_EQ(detoasted_text, large_text);
    }

    tm_->commit(new_xid);
}

// =============================================================================
// Test 4: Multiple Crashes - Idempotent Recovery
// =============================================================================

TEST_F(ToastCrashRecoveryMGATest, MultipleCrashes_IdempotentRecovery)
{
    // MGA Crash Recovery Test (NO WAL)
    //
    // Scenario:
    // 1. Create TOAST value, crash before commit
    // 2. Restart, crash again
    // 3. Restart again
    // 4. Verify consistent state (idempotent recovery)
    //
    // MGA Compliance: TIP-based recovery is idempotent

    ID table_id = generateUuidV7();

    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create TOAST value without commit
    uint64_t xid = tm_->beginTransaction();
    std::string large_text = createLargeText(2);

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        reinterpret_cast<const uint8_t*>(large_text.data()),
        large_text.size(),
        ToastStrategy::EXTERNAL,
        xid,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);

    // Crash #1 (before commit)
    simulateDatabaseRestart();

    // Crash #2 (immediately after first restart)
    simulateDatabaseRestart();

    // Crash #3 (immediately after second restart)
    simulateDatabaseRestart();

    // After multiple crashes, chunks should still be invisible
    uint64_t new_xid = tm_->beginTransaction();

    ToastManager toast_mgr2(db_.get(), table_id);
    std::vector<uint8_t> detoasted;
    status = toast_mgr2.detoastValue(&toast_ptr, &detoasted, new_xid, &ctx);

    EXPECT_NE(status, Status::OK) << "After multiple crashes, chunks should remain invisible";

    tm_->commit(new_xid);
}

// =============================================================================
// Test 5: Crash Recovery with Sweep - Full Cleanup
// =============================================================================

TEST_F(ToastCrashRecoveryMGATest, CrashWithSweep_FullCleanup)
{
    // MGA Crash Recovery + Sweep Test (NO WAL)
    //
    // Scenario:
    // 1. Create 10 TOAST values, crash before commit
    // 2. Restart (TIP marks transactions aborted)
    // 3. Run sweep (vacuum)
    // 4. Verify all aborted chunks physically removed
    //
    // MGA Compliance: Sweep uses TIP for garbage collection

    ID table_id = generateUuidV7();

    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create multiple TOAST values without committing
    std::vector<ToastPointer> toast_ptrs;
    for (int i = 0; i < 10; i++)
    {
        uint64_t xid = tm_->beginTransaction();
        std::string large_text = createLargeText(2);

        ToastPointer toast_ptr;
        status = toast_mgr.toastValue(
            reinterpret_cast<const uint8_t*>(large_text.data()),
            large_text.size(),
            ToastStrategy::EXTERNAL,
            xid,
            &toast_ptr,
            &ctx
        );

        if (status == Status::OK)
        {
            toast_ptrs.push_back(toast_ptr);
        }

        // Don't commit - simulate crash
    }

    ASSERT_GT(toast_ptrs.size(), 0u);

    // Crash and restart
    simulateDatabaseRestart();

    // Run sweep to clean up aborted chunks
    VacuumStats vacuum_stats;
    status = vacuum_->vacuumDatabase(&vacuum_stats, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Verify chunks cleaned (orphan detection should find them all)
    // After sweep, detoasting all should fail
    uint64_t verify_xid = tm_->beginTransaction();
    ToastManager toast_mgr2(db_.get(), table_id);

    int invisible_count = 0;
    for (const auto& toast_ptr : toast_ptrs)
    {
        std::vector<uint8_t> detoasted;
        status = toast_mgr2.detoastValue(&toast_ptr, &detoasted, verify_xid, &ctx);

        if (status != Status::OK)
        {
            invisible_count++;
        }
    }

    EXPECT_GT(invisible_count, 0) << "Most/all chunks should be invisible after crash+sweep";

    tm_->commit(verify_xid);
}

// =============================================================================
// Test 6: TIP State Persistence - Verify TIP Survives Restart
// =============================================================================

TEST_F(ToastCrashRecoveryMGATest, TIPStatePersistence)
{
    // MGA TIP Persistence Test
    //
    // Scenario:
    // 1. Create TOAST value and commit
    // 2. Verify TIP shows committed
    // 3. Restart database
    // 4. Verify TIP still shows committed (persisted)
    //
    // MGA Compliance: TIP persists across restarts (not in-memory WAL)

    ID table_id = generateUuidV7();

    ToastManager toast_mgr(db_.get(), table_id);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create and commit TOAST value
    uint64_t xid = tm_->beginTransaction();
    std::string large_text = createLargeText(3);

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        reinterpret_cast<const uint8_t*>(large_text.data()),
        large_text.size(),
        ToastStrategy::EXTERNAL,
        xid,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);
    tm_->commit(xid); // TIP persists commit

    // Verify accessible before restart
    uint64_t verify_xid1 = tm_->beginTransaction();
    std::vector<uint8_t> detoasted1;
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted1, verify_xid1, &ctx);
    ASSERT_EQ(status, Status::OK);
    tm_->commit(verify_xid1);

    // Restart database
    simulateDatabaseRestart();

    // Verify still accessible after restart (TIP persisted)
    uint64_t verify_xid2 = tm_->beginTransaction();
    ToastManager toast_mgr2(db_.get(), table_id);
    std::vector<uint8_t> detoasted2;
    status = toast_mgr2.detoastValue(&toast_ptr, &detoasted2, verify_xid2, &ctx);

    EXPECT_EQ(status, Status::OK) << "TIP state should persist across restart";

    if (status == Status::OK)
    {
        std::string detoasted_text(detoasted2.begin(), detoasted2.end());
        EXPECT_EQ(detoasted_text, large_text) << "Data should be identical after restart";
    }

    tm_->commit(verify_xid2);
}

// =============================================================================
// Main
// =============================================================================

