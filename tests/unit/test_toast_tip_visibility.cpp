/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// ScratchBird TOAST TIP Visibility Unit Test
// Tests Phase 5: TOAST chunk visibility using TIP (Transaction Inventory Pages)
//
// MGA Compliance: All visibility checks use TIP, not WAL

#include <gtest/gtest.h>
#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "test_helpers.h"
#include <memory>
#include <vector>

using namespace scratchbird::core;

class ToastTIPVisibilityTest : public ::testing::Test
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
        // Create test database
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_toast_tip_vis", ".db");
        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 8192, &ctx), Status::OK)
            << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK)
            << ctx.message;

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

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

        ASSERT_EQ(catalog_->createTable(schema_id, "toast_tip_vis", columns, table_id_, 0, &ctx),
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
    }

    std::unique_ptr<Database> db_;
    CatalogManager* catalog_;
    ID table_id_;
    std::string test_db_path_;
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
// Test 1: Chunk Created by Active Transaction - Invisible to Others
// =============================================================================

TEST_F(ToastTIPVisibilityTest, ActiveTransaction_ChunkInvisible)
{
    // MGA Rule: Chunks created by active transactions are invisible to other transactions
    //
    // Scenario:
    // 1. Transaction T1 creates TOAST chunk (xmin=T1, xmax=0)
    // 2. T1 does NOT commit
    // 3. Transaction T2 tries to read chunk
    // 4. Verify chunk is INVISIBLE to T2 (TIP shows T1 as TX_ACTIVE)

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    // Create TOAST table
    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Transaction T1: Create chunk but don't commit
    uint64_t xid1 = beginTxn(conn_ctx_.get(), &ctx);
    ASSERT_NE(xid1, 0);

    std::string data = "TOAST_VISIBILITY_TEST_DATA_ACTIVE_TRANSACTION";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            data_vec.data(),
            data_vec.size(),
            ToastStrategy::EXTERNAL,
            xid1,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);

    // Transaction T2: Try to read chunk
    auto conn2 = createConnection(&ctx);
    uint64_t xid2 = beginTxn(conn2.get(), &ctx);
    ASSERT_NE(xid2, 0);

    std::vector<uint8_t> detoasted_data;
    {
        ScopedCurrentConnection scope(conn2.get());
        status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid2, &ctx);
    }

    // Chunk should be INVISIBLE to T2 (T1 still active)
    EXPECT_NE(status, Status::OK) << "Chunk should be invisible to T2";

    rollbackTxn(conn_ctx_.get(), &ctx);
    rollbackTxn(conn2.get(), &ctx);
}

// =============================================================================
// Test 2: Chunk Created by Committed Transaction - Visible to Others
// =============================================================================

TEST_F(ToastTIPVisibilityTest, CommittedTransaction_ChunkVisible)
{
    // MGA Rule: Chunks created by committed transactions are visible to later transactions
    //
    // Scenario:
    // 1. Transaction T1 creates TOAST chunk (xmin=T1, xmax=0)
    // 2. T1 COMMITS
    // 3. Transaction T2 tries to read chunk
    // 4. Verify chunk is VISIBLE to T2 (TIP shows T1 as TX_COMMITTED)

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Transaction T1: Create chunk and COMMIT
    uint64_t xid1 = beginTxn(conn_ctx_.get(), &ctx);
    std::string data = "TOAST_VISIBILITY_TEST_DATA_COMMITTED_TRANSACTION";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            data_vec.data(),
            data_vec.size(),
            ToastStrategy::EXTERNAL,
            xid1,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);
    commitTxn(conn_ctx_.get(), &ctx); // COMMIT - TIP now shows TX_COMMITTED

    // Transaction T2: Try to read chunk
    auto conn2 = createConnection(&ctx);
    uint64_t xid2 = beginTxn(conn2.get(), &ctx);
    std::vector<uint8_t> detoasted_data;
    {
        ScopedCurrentConnection scope(conn2.get());
        status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid2, &ctx);
    }

    // Chunk should be VISIBLE to T2 (T1 committed)
    EXPECT_EQ(status, Status::OK) << "Chunk should be visible to T2: " << ctx.message;
    EXPECT_EQ(detoasted_data, data_vec) << "Detoasted data should match original";

    commitTxn(conn2.get(), &ctx);
}

// =============================================================================
// Test 3: Chunk Created by Aborted Transaction - Invisible to All
// =============================================================================

TEST_F(ToastTIPVisibilityTest, AbortedTransaction_ChunkInvisible)
{
    // MGA Rule: Chunks created by aborted transactions are invisible to all transactions
    //
    // Scenario:
    // 1. Transaction T1 creates TOAST chunk (xmin=T1, xmax=0)
    // 2. T1 ABORTS
    // 3. Transaction T2 tries to read chunk
    // 4. Verify chunk is INVISIBLE to T2 (TIP shows T1 as TX_ABORTED)

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Transaction T1: Create chunk and ABORT
    uint64_t xid1 = beginTxn(conn_ctx_.get(), &ctx);
    std::string data = "TOAST_VISIBILITY_TEST_DATA_ABORTED_TRANSACTION";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            data_vec.data(),
            data_vec.size(),
            ToastStrategy::EXTERNAL,
            xid1,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);
    rollbackTxn(conn_ctx_.get(), &ctx); // ABORT - TIP now shows TX_ABORTED

    // Transaction T2: Try to read chunk
    auto conn2 = createConnection(&ctx);
    uint64_t xid2 = beginTxn(conn2.get(), &ctx);
    std::vector<uint8_t> detoasted_data;
    {
        ScopedCurrentConnection scope(conn2.get());
        status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid2, &ctx);
    }

    // Chunk should be INVISIBLE to T2 (T1 aborted)
    EXPECT_NE(status, Status::OK) << "Chunk should be invisible to T2 (T1 aborted)";

    commitTxn(conn2.get(), &ctx);
}

// =============================================================================
// Test 4: Chunk Deleted by Committed Transaction - Invisible to Later Transactions
// =============================================================================

TEST_F(ToastTIPVisibilityTest, DeletedByCommittedTxn_ChunkInvisible)
{
    // MGA Rule: Chunks with committed xmax are invisible to later transactions
    //
    // Scenario:
    // 1. Transaction T1 creates TOAST chunk (xmin=T1, xmax=0), commits
    // 2. Transaction T2 deletes chunk (xmax=T2), commits
    // 3. Transaction T3 tries to read chunk
    // 4. Verify chunk is INVISIBLE to T3 (TIP shows T2 as TX_COMMITTED)

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // T1: Create chunk
    uint64_t xid1 = beginTxn(conn_ctx_.get(), &ctx);
    std::string data = "TOAST_VISIBILITY_TEST_DATA_DELETED";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            data_vec.data(),
            data_vec.size(),
            ToastStrategy::EXTERNAL,
            xid1,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);
    ID value_id = toast_ptr.lob_uuid;
    commitTxn(conn_ctx_.get(), &ctx);

    // T2: Delete chunk
    auto conn2 = createConnection(&ctx);
    uint64_t xid2 = beginTxn(conn2.get(), &ctx);
    {
        ScopedCurrentConnection scope(conn2.get());
        status = toast_mgr.deleteToastValue(value_id, xid2, &ctx);
    }
    ASSERT_EQ(status, Status::OK);
    commitTxn(conn2.get(), &ctx); // xmax=T2, committed

    // T3: Try to read chunk
    auto conn3 = createConnection(&ctx);
    uint64_t xid3 = beginTxn(conn3.get(), &ctx);
    std::vector<uint8_t> detoasted_data;
    {
        ScopedCurrentConnection scope(conn3.get());
        status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid3, &ctx);
    }

    // Chunk should be INVISIBLE to T3 (xmax committed)
    EXPECT_NE(status, Status::OK) << "Chunk should be invisible to T3 (deleted)";

    commitTxn(conn3.get(), &ctx);
}

// =============================================================================
// Test 5: Chunk Deleted by Aborted Transaction - Visible to Later Transactions
// =============================================================================

TEST_F(ToastTIPVisibilityTest, DeletedByAbortedTxn_ChunkVisible)
{
    // MGA Rule: Chunks with aborted xmax are still visible (delete was rolled back)
    //
    // Scenario:
    // 1. Transaction T1 creates TOAST chunk (xmin=T1, xmax=0), commits
    // 2. Transaction T2 deletes chunk (xmax=T2), ABORTS
    // 3. Transaction T3 tries to read chunk
    // 4. Verify chunk is VISIBLE to T3 (TIP shows T2 as TX_ABORTED)

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // T1: Create chunk
    uint64_t xid1 = beginTxn(conn_ctx_.get(), &ctx);
    std::string data = "TOAST_VISIBILITY_TEST_DATA_ABORTED_DELETE";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            data_vec.data(),
            data_vec.size(),
            ToastStrategy::EXTERNAL,
            xid1,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);
    ID value_id = toast_ptr.lob_uuid;
    commitTxn(conn_ctx_.get(), &ctx);

    // T2: Delete chunk but ABORT
    auto conn2 = createConnection(&ctx);
    uint64_t xid2 = beginTxn(conn2.get(), &ctx);
    {
        ScopedCurrentConnection scope(conn2.get());
        status = toast_mgr.deleteToastValue(value_id, xid2, &ctx);
    }
    ASSERT_EQ(status, Status::OK);
    rollbackTxn(conn2.get(), &ctx); // xmax=T2, aborted

    // T3: Try to read chunk
    auto conn3 = createConnection(&ctx);
    uint64_t xid3 = beginTxn(conn3.get(), &ctx);
    std::vector<uint8_t> detoasted_data;
    {
        ScopedCurrentConnection scope(conn3.get());
        status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid3, &ctx);
    }

    // Chunk should be VISIBLE to T3 (xmax aborted - delete rolled back)
    EXPECT_EQ(status, Status::OK) << "Chunk should be visible to T3 (delete aborted): " << ctx.message;
    EXPECT_EQ(detoasted_data, data_vec) << "Detoasted data should match original";

    commitTxn(conn3.get(), &ctx);
}

// =============================================================================
// Test 6: Chunk Deleted by Active Transaction - Visible to Others
// =============================================================================

TEST_F(ToastTIPVisibilityTest, DeletedByActiveTxn_ChunkVisibleToOthers)
{
    // MGA Rule: Chunks with active xmax are visible to other transactions
    //           (deleting transaction hasn't committed yet)
    //
    // Scenario:
    // 1. Transaction T1 creates TOAST chunk (xmin=T1, xmax=0), commits
    // 2. Transaction T2 deletes chunk (xmax=T2), does NOT commit
    // 3. Transaction T3 tries to read chunk
    // 4. Verify chunk is VISIBLE to T3 (TIP shows T2 as TX_ACTIVE)

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // T1: Create chunk
    uint64_t xid1 = beginTxn(conn_ctx_.get(), &ctx);
    std::string data = "TOAST_VISIBILITY_TEST_DATA_ACTIVE_DELETE";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            data_vec.data(),
            data_vec.size(),
            ToastStrategy::EXTERNAL,
            xid1,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);
    ID value_id = toast_ptr.lob_uuid;
    commitTxn(conn_ctx_.get(), &ctx);

    // T2: Delete chunk but DON'T commit
    auto conn2 = createConnection(&ctx);
    uint64_t xid2 = beginTxn(conn2.get(), &ctx);
    {
        ScopedCurrentConnection scope(conn2.get());
        status = toast_mgr.deleteToastValue(value_id, xid2, &ctx);
    }
    ASSERT_EQ(status, Status::OK);
    // T2 still active - don't commit/abort yet

    // T3: Try to read chunk
    auto conn3 = createConnection(&ctx);
    uint64_t xid3 = beginTxn(conn3.get(), &ctx);
    std::vector<uint8_t> detoasted_data;
    {
        ScopedCurrentConnection scope(conn3.get());
        status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid3, &ctx);
    }

    // Chunk should be VISIBLE to T3 (xmax still active)
    EXPECT_EQ(status, Status::OK) << "Chunk should be visible to T3 (xmax active): " << ctx.message;
    EXPECT_EQ(detoasted_data, data_vec) << "Detoasted data should match original";

    rollbackTxn(conn2.get(), &ctx);
    commitTxn(conn3.get(), &ctx);
}

// =============================================================================
// Test 7: TIP State Transitions - Transaction Lifecycle
// =============================================================================

TEST_F(ToastTIPVisibilityTest, TIPStateTransitions_TransactionLifecycle)
{
    // Validates TIP state transitions: TX_ACTIVE → TX_COMMITTED or TX_ABORTED
    //
    // Scenario:
    // 1. Create chunk in T1 (TIP: T1=TX_ACTIVE)
    // 2. Verify invisible to T2
    // 3. Commit T1 (TIP: T1=TX_COMMITTED)
    // 4. Verify visible to T2

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // T1: Create chunk (TX_ACTIVE)
    uint64_t xid1 = beginTxn(conn_ctx_.get(), &ctx);
    std::string data = "TOAST_TIP_STATE_TRANSITION_TEST";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            data_vec.data(),
            data_vec.size(),
            ToastStrategy::EXTERNAL,
            xid1,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);

    // T2: Try to read while T1 active
    auto conn2 = createConnection(&ctx);
    uint64_t xid2 = beginTxn(conn2.get(), &ctx);
    std::vector<uint8_t> detoasted_data;
    {
        ScopedCurrentConnection scope(conn2.get());
        status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid2, &ctx);
    }

    // Should be invisible (T1 active)
    EXPECT_NE(status, Status::OK) << "Chunk should be invisible while T1 active";

    // Commit T1 (TX_ACTIVE → TX_COMMITTED)
    commitTxn(conn_ctx_.get(), &ctx);

    // T2: Try to read again after T1 committed
    detoasted_data.clear();
    {
        ScopedCurrentConnection scope(conn2.get());
        status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid2, &ctx);
    }

    // Should be visible (T1 committed)
    EXPECT_EQ(status, Status::OK) << "Chunk should be visible after T1 commits: " << ctx.message;
    EXPECT_EQ(detoasted_data, data_vec) << "Detoasted data should match original";

    commitTxn(conn2.get(), &ctx);
}

// =============================================================================
// Test 8: MGA Snapshot Isolation - Same Transaction Sees Own Changes
// =============================================================================

TEST_F(ToastTIPVisibilityTest, SnapshotIsolation_SameTransactionSeesOwnChanges)
{
    // MGA Rule: Transaction sees its own uncommitted changes
    //
    // Scenario:
    // 1. Transaction T1 creates TOAST chunk
    // 2. T1 immediately reads chunk (before commit)
    // 3. Verify T1 can see its own chunk (snapshot isolation)

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // T1: Create chunk
    uint64_t xid1 = beginTxn(conn_ctx_.get(), &ctx);
    std::string data = "TOAST_SNAPSHOT_ISOLATION_TEST";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.toastValue(
            data_vec.data(),
            data_vec.size(),
            ToastStrategy::EXTERNAL,
            xid1,
            &toast_ptr,
            &ctx
        );
    }

    ASSERT_EQ(status, Status::OK);

    // T1: Read own chunk (before commit)
    std::vector<uint8_t> detoasted_data;
    {
        ScopedCurrentConnection scope(conn_ctx_.get());
        status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid1, &ctx);
    }

    // T1 should see its own chunk (snapshot isolation)
    EXPECT_EQ(status, Status::OK) << "T1 should see its own uncommitted chunk: " << ctx.message;
    EXPECT_EQ(detoasted_data, data_vec) << "Detoasted data should match original";

    commitTxn(conn_ctx_.get(), &ctx);
}

// =============================================================================
// Main
// =============================================================================
