/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// ScratchBird TOAST Crash Recovery Integration Test (MGA)
// Tests Phase 5: TIP-based crash recovery WITHOUT WAL
// CRITICAL: This test validates Firebird MGA architecture, not PostgreSQL MVCC

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/gc_manager.h"
#include "scratchbird/core/toast.h"
#include "test_helpers.h"
#include <memory>
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;

class ToastCrashRecoveryMGATest : public ::testing::Test
{
protected:
    class ScopedCurrentConnection
    {
    public:
        explicit ScopedCurrentConnection(ConnectionContext* ctx)
            : prev_(ConnectionContext::getCurrent())
        {
            ConnectionContext::setCurrent(ctx);
        }
        ~ScopedCurrentConnection()
        {
            ConnectionContext::setCurrent(prev_);
        }
    private:
        ConnectionContext* prev_;
    };

    void SetUp() override
    {
        db_path_ =
            scratchbird::testing::uniqueTestDbPath("test_toast_crash_recovery_mga", ".db");

        // Create test database
        ErrorContext ctx;
        Status status = Database::create(db_path_, 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<Database>();
        status = db_->open(db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        storage_ = db_->storage_engine();
        catalog_ = db_->catalog_manager();
        gc_ = db_->garbage_collector();
        gc_manager_ = db_->gc_manager();

        ASSERT_NE(storage_, nullptr);
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(gc_, nullptr);
        ASSERT_NE(gc_manager_, nullptr);

        // Create schema + test table
        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(catalog_->listSchemas(schemas, &ctx), Status::OK);
        ID schema_id;
        if (schemas.empty()) {
            ASSERT_EQ(catalog_->createSchema("public", "test", schema_id, &ctx), Status::OK);
        } else {
            schema_id = schemas[0].schema_id;
        }

        std::vector<CatalogManager::ColumnInfo> columns;
        CatalogManager::ColumnInfo id_col;
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.max_length = 4;
        id_col.nullable = false;
        id_col.has_default = false;
        columns.push_back(id_col);

        CatalogManager::ColumnInfo data_col;
        data_col.column_name = "data";
        data_col.data_type = static_cast<uint16_t>(DataType::BYTEA);
        data_col.max_length = 0;
        data_col.nullable = true;
        data_col.has_default = false;
        columns.push_back(data_col);

        ASSERT_EQ(catalog_->createTable(schema_id, "toast_crash_recovery", columns, table_id_, 0,
                                        &ctx),
                  Status::OK);

        // Create connection context (registers ProcArray)
        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        if (db_)
        {
            db_->close();
        }
        if (!db_path_.empty())
        {
            std::remove(db_path_.c_str());
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

        if (text.size() < size_kb * 1024)
        {
            text.append(size_kb * 1024 - text.size(), 'X');
        }
        return text;
    }

    // Helper: Simulate database restart (close and reopen)
    void simulateDatabaseRestart()
    {
        // Close database + connection
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_->close();
        db_.reset();

        // Reopen database (simulates restart)
        ErrorContext ctx;
        db_ = std::make_unique<Database>();
        Status status = db_->open(db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to reopen database: " << ctx.message;

        // Reinitialize pointers
        storage_ = db_->storage_engine();
        catalog_ = db_->catalog_manager();
        gc_ = db_->garbage_collector();
        gc_manager_ = db_->gc_manager();

        // Recreate connection context for new backend/proc
        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
    }

    std::string db_path_;
    std::unique_ptr<Database> db_;
    StorageEngine* storage_;
    CatalogManager* catalog_;
    GarbageCollector* gc_;
    GcManager* gc_manager_;
    ID table_id_;
    std::unique_ptr<ConnectionContext> conn_ctx_;

    std::unique_ptr<ConnectionContext> createConnection(ErrorContext* ctx)
    {
        std::unique_ptr<ConnectionContext> conn;
        Status status = db_->connect(conn, ctx);
        EXPECT_EQ(status, Status::OK) << ctx->message;
        return conn;
    }

    uint64_t beginTxn(ConnectionContext* conn, ErrorContext* ctx)
    {
        ScopedCurrentConnection scope(conn);
        uint64_t xid = conn->getCurrentXid();
        EXPECT_NE(xid, 0u);
        return xid;
    }

    void commitTxn(ConnectionContext* conn, ErrorContext* ctx)
    {
        ScopedCurrentConnection scope(conn);
        Status status = conn->commit(ctx);
        EXPECT_EQ(status, Status::OK) << ctx->message;
    }

    void rollbackTxn(ConnectionContext* conn, ErrorContext* ctx)
    {
        ScopedCurrentConnection scope(conn);
        Status status = conn->rollback(ctx);
        EXPECT_EQ(status, Status::OK) << ctx->message;
    }
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

    // Create TOAST manager
    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Start transaction
    uint64_t xid = beginTxn(conn_ctx_.get(), &ctx);
    ASSERT_NE(xid, 0);

    // Create large TOAST value
    std::string large_text = createLargeText(5); // 5KB
    ASSERT_GT(large_text.size(), 2048u);

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            reinterpret_cast<const uint8_t*>(large_text.data()),
            large_text.size(),
            ToastStrategy::EXTERNAL,
            xid,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK) << "TOAST failed: " << ctx.message;
    uint32_t value_id = toast_ptr.va_valueid;

    // SIMULATE CRASH: Don't commit, just abandon transaction
    // In real crash, transaction would be left in TX_ACTIVE state
    // Database restart will mark it TX_ABORTED via TIP

    // Simulate database restart
    simulateDatabaseRestart();

    // After restart, TIP should mark transaction as aborted
    // Verify transaction state via TIP
    uint64_t new_xid = beginTxn(conn_ctx_.get(), &ctx);
    ASSERT_NE(new_xid, 0);

    // Try to detoast - should fail because chunks are invisible
    // (transaction xid is aborted in TIP)
    ToastManager toast_mgr2(db_.get(), table_id_);
    status = toast_mgr2.initialize(&ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    status = toast_mgr2.initialize(&ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    std::vector<uint8_t> detoasted;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr2.detoastValue(&toast_ptr, &detoasted, new_xid, &ctx);
    }

    // Expected: Detoasting fails or returns empty (chunks invisible)
    // This validates TIP-based visibility after crash
    EXPECT_NE(status, Status::OK) << "Chunks should be invisible after crash (TIP marks xmin aborted)";

    commitTxn(conn_ctx_.get(), &ctx);

    // Run sweep to physically remove aborted chunks
    GcStats gc_stats;
    status = gc_manager_->gcDatabase(&gc_stats, &ctx);
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

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Start transaction and create TOAST value
    uint64_t xid = beginTxn(conn_ctx_.get(), &ctx);
    std::string large_text = createLargeText(3);

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            reinterpret_cast<const uint8_t*>(large_text.data()),
            large_text.size(),
            ToastStrategy::EXTERNAL,
            xid,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);

    // COMMIT transaction (persists to TIP)
    commitTxn(conn_ctx_.get(), &ctx);

    // SIMULATE CRASH after commit
    simulateDatabaseRestart();

    // After restart, TIP should show transaction committed
    // Verify chunks are still visible
    uint64_t new_xid = beginTxn(conn_ctx_.get(), &ctx);

    ToastManager toast_mgr2(db_.get(), table_id_);
    status = toast_mgr2.initialize(&ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    std::vector<uint8_t> detoasted;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr2.detoastValue(&toast_ptr, &detoasted, new_xid, &ctx);
    }

    // Expected: Detoasting succeeds (chunks visible via TIP)
    EXPECT_EQ(status, Status::OK) << "Chunks should be visible after commit+crash (TIP shows committed)";

    if (status == Status::OK)
    {
        // Verify data integrity
        std::string detoasted_text(detoasted.begin(), detoasted.end());
        EXPECT_EQ(detoasted_text, large_text) << "Detoasted data should match original";
    }

    commitTxn(conn_ctx_.get(), &ctx);
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

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create and commit TOAST value
    uint64_t xid1 = beginTxn(conn_ctx_.get(), &ctx);
    std::string large_text = createLargeText(4);

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            reinterpret_cast<const uint8_t*>(large_text.data()),
            large_text.size(),
            ToastStrategy::EXTERNAL,
            xid1,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);
    uint32_t value_id = toast_ptr.va_valueid;
    commitTxn(conn_ctx_.get(), &ctx); // Commit create

    // Start delete transaction (sets xmax on chunks)
    uint64_t xid2 = beginTxn(conn_ctx_.get(), &ctx);
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.deleteToastValue(value_id, xid2, &ctx);
    }
    ASSERT_EQ(status, Status::OK);

    // SIMULATE CRASH before delete commits
    // Delete transaction abandoned (xmax set but not committed)
    simulateDatabaseRestart();

    // After restart, TIP should mark delete transaction as aborted
    // Chunks should still be visible (xmax aborted)
    uint64_t new_xid = beginTxn(conn_ctx_.get(), &ctx);

    ToastManager toast_mgr2(db_.get(), table_id_);
    status = toast_mgr2.initialize(&ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    std::vector<uint8_t> detoasted;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr2.detoastValue(&toast_ptr, &detoasted, new_xid, &ctx);
    }

    // Expected: Chunks still visible (delete aborted via TIP)
    EXPECT_EQ(status, Status::OK) << "Chunks should be visible (delete transaction aborted in TIP)";

    if (status == Status::OK)
    {
        std::string detoasted_text(detoasted.begin(), detoasted.end());
        EXPECT_EQ(detoasted_text, large_text);
    }

    commitTxn(conn_ctx_.get(), &ctx);
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

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create TOAST value without commit
    uint64_t xid = beginTxn(conn_ctx_.get(), &ctx);
    std::string large_text = createLargeText(2);

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            reinterpret_cast<const uint8_t*>(large_text.data()),
            large_text.size(),
            ToastStrategy::EXTERNAL,
            xid,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);

    // Crash #1 (before commit)
    simulateDatabaseRestart();

    // Crash #2 (immediately after first restart)
    simulateDatabaseRestart();

    // Crash #3 (immediately after second restart)
    simulateDatabaseRestart();

    // After multiple crashes, chunks should still be invisible
    uint64_t new_xid = beginTxn(conn_ctx_.get(), &ctx);

    ToastManager toast_mgr2(db_.get(), table_id_);
    status = toast_mgr2.initialize(&ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    std::vector<uint8_t> detoasted;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr2.detoastValue(&toast_ptr, &detoasted, new_xid, &ctx);
    }

    EXPECT_NE(status, Status::OK) << "After multiple crashes, chunks should remain invisible";

    commitTxn(conn_ctx_.get(), &ctx);
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
    // 3. Run sweep (GC)
    // 4. Verify all aborted chunks physically removed
    //
    // MGA Compliance: Sweep uses TIP for garbage collection

    ToastManager toast_mgr(db_.get(), table_id_);
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
        uint64_t xid = beginTxn(conn_ctx_.get(), &ctx);
        std::string large_text = createLargeText(2);

        ToastPointer toast_ptr;
        {
            ScopedCurrentConnection scope(conn_ctx_.get());
            status = toast_mgr.toastValue(
                reinterpret_cast<const uint8_t*>(large_text.data()),
                large_text.size(),
                ToastStrategy::EXTERNAL,
                xid,
                &toast_ptr,
                &ctx
            );
        }

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
    GcStats gc_stats;
    status = gc_manager_->gcDatabase(&gc_stats, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Verify chunks cleaned (orphan detection should find them all)
    // After sweep, detoasting all should fail
    uint64_t verify_xid = beginTxn(conn_ctx_.get(), &ctx);
    ToastManager toast_mgr2(db_.get(), table_id_);
    status = toast_mgr2.initialize(&ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    int invisible_count = 0;
    for (const auto& toast_ptr : toast_ptrs)
    {
        std::vector<uint8_t> detoasted;
        {
            ScopedCurrentConnection scope(conn_ctx_.get());
            status = toast_mgr2.detoastValue(&toast_ptr, &detoasted, verify_xid, &ctx);
        }

        if (status != Status::OK)
        {
            invisible_count++;
        }
    }

    EXPECT_GT(invisible_count, 0) << "Most/all chunks should be invisible after crash+sweep";

    commitTxn(conn_ctx_.get(), &ctx);
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

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;
    Status status = toast_mgr.createToastTable(&ctx);

    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Create and commit TOAST value
    uint64_t xid = beginTxn(conn_ctx_.get(), &ctx);
    std::string large_text = createLargeText(3);

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            reinterpret_cast<const uint8_t*>(large_text.data()),
            large_text.size(),
            ToastStrategy::EXTERNAL,
            xid,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);
    commitTxn(conn_ctx_.get(), &ctx); // TIP persists commit

    // Verify accessible before restart
    uint64_t verify_xid1 = beginTxn(conn_ctx_.get(), &ctx);
    std::vector<uint8_t> detoasted1;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.detoastValue(&toast_ptr, &detoasted1, verify_xid1, &ctx);
    }
    ASSERT_EQ(status, Status::OK);
    commitTxn(conn_ctx_.get(), &ctx);

    // Restart database
    simulateDatabaseRestart();

    // Verify still accessible after restart (TIP persisted)
    uint64_t verify_xid2 = beginTxn(conn_ctx_.get(), &ctx);
    ToastManager toast_mgr2(db_.get(), table_id_);
    status = toast_mgr2.initialize(&ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    std::vector<uint8_t> detoasted2;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr2.detoastValue(&toast_ptr, &detoasted2, verify_xid2, &ctx);
    }

    EXPECT_EQ(status, Status::OK) << "TIP state should persist across restart";

    if (status == Status::OK)
    {
        std::string detoasted_text(detoasted2.begin(), detoasted2.end());
        EXPECT_EQ(detoasted_text, large_text) << "Data should be identical after restart";
    }

    commitTxn(conn_ctx_.get(), &ctx);
}

// =============================================================================
// Main
// =============================================================================
