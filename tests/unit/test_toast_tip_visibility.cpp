// ScratchBird TOAST TIP Visibility Unit Test
// Tests Phase 5: TOAST chunk visibility using TIP (Transaction Inventory Pages)
//
// MGA Compliance: All visibility checks use TIP, not WAL

#include <gtest/gtest.h>
#include "scratchbird/core/toast.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/database.h"
#include <memory>
#include <vector>

using namespace scratchbird::core;

class ToastTIPVisibilityTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test database
        db_ = std::make_unique<Database>("test_toast_tip_vis.db", 20 * 1024 * 1024);
        ASSERT_EQ(db_->open(), Status::OK);

        tm_ = db_->transaction_manager();
        ASSERT_NE(tm_, nullptr);

        table_id_ = generateUuidV7();
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
        }
    }

    std::unique_ptr<Database> db_;
    TransactionManager* tm_;
    ID table_id_;
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
    uint64_t xid1 = tm_->beginTransaction();
    ASSERT_NE(xid1, 0);

    std::string data = "TOAST_VISIBILITY_TEST_DATA_ACTIVE_TRANSACTION";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        data_vec.data(),
        data_vec.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);

    // Transaction T2: Try to read chunk
    uint64_t xid2 = tm_->beginTransaction();
    ASSERT_NE(xid2, 0);

    std::vector<uint8_t> detoasted_data;
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid2, &ctx);

    // Chunk should be INVISIBLE to T2 (T1 still active)
    EXPECT_NE(status, Status::OK) << "Chunk should be invisible to T2";

    tm_->abort(xid1);
    tm_->abort(xid2);
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
    uint64_t xid1 = tm_->beginTransaction();
    std::string data = "TOAST_VISIBILITY_TEST_DATA_COMMITTED_TRANSACTION";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        data_vec.data(),
        data_vec.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);
    tm_->commit(xid1); // COMMIT - TIP now shows TX_COMMITTED

    // Transaction T2: Try to read chunk
    uint64_t xid2 = tm_->beginTransaction();
    std::vector<uint8_t> detoasted_data;
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid2, &ctx);

    // Chunk should be VISIBLE to T2 (T1 committed)
    EXPECT_EQ(status, Status::OK) << "Chunk should be visible to T2: " << ctx.message;
    EXPECT_EQ(detoasted_data, data_vec) << "Detoasted data should match original";

    tm_->commit(xid2);
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
    uint64_t xid1 = tm_->beginTransaction();
    std::string data = "TOAST_VISIBILITY_TEST_DATA_ABORTED_TRANSACTION";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        data_vec.data(),
        data_vec.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);
    tm_->abort(xid1); // ABORT - TIP now shows TX_ABORTED

    // Transaction T2: Try to read chunk
    uint64_t xid2 = tm_->beginTransaction();
    std::vector<uint8_t> detoasted_data;
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid2, &ctx);

    // Chunk should be INVISIBLE to T2 (T1 aborted)
    EXPECT_NE(status, Status::OK) << "Chunk should be invisible to T2 (T1 aborted)";

    tm_->commit(xid2);
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
    uint64_t xid1 = tm_->beginTransaction();
    std::string data = "TOAST_VISIBILITY_TEST_DATA_DELETED";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        data_vec.data(),
        data_vec.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);
    uint32_t value_id = toast_ptr.va_valueid;
    tm_->commit(xid1);

    // T2: Delete chunk
    uint64_t xid2 = tm_->beginTransaction();
    status = toast_mgr.deleteToastValue(value_id, xid2, &ctx);
    ASSERT_EQ(status, Status::OK);
    tm_->commit(xid2); // xmax=T2, committed

    // T3: Try to read chunk
    uint64_t xid3 = tm_->beginTransaction();
    std::vector<uint8_t> detoasted_data;
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid3, &ctx);

    // Chunk should be INVISIBLE to T3 (xmax committed)
    EXPECT_NE(status, Status::OK) << "Chunk should be invisible to T3 (deleted)";

    tm_->commit(xid3);
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
    uint64_t xid1 = tm_->beginTransaction();
    std::string data = "TOAST_VISIBILITY_TEST_DATA_ABORTED_DELETE";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        data_vec.data(),
        data_vec.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);
    uint32_t value_id = toast_ptr.va_valueid;
    tm_->commit(xid1);

    // T2: Delete chunk but ABORT
    uint64_t xid2 = tm_->beginTransaction();
    status = toast_mgr.deleteToastValue(value_id, xid2, &ctx);
    ASSERT_EQ(status, Status::OK);
    tm_->abort(xid2); // xmax=T2, aborted

    // T3: Try to read chunk
    uint64_t xid3 = tm_->beginTransaction();
    std::vector<uint8_t> detoasted_data;
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid3, &ctx);

    // Chunk should be VISIBLE to T3 (xmax aborted - delete rolled back)
    EXPECT_EQ(status, Status::OK) << "Chunk should be visible to T3 (delete aborted): " << ctx.message;
    EXPECT_EQ(detoasted_data, data_vec) << "Detoasted data should match original";

    tm_->commit(xid3);
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
    uint64_t xid1 = tm_->beginTransaction();
    std::string data = "TOAST_VISIBILITY_TEST_DATA_ACTIVE_DELETE";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        data_vec.data(),
        data_vec.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);
    uint32_t value_id = toast_ptr.va_valueid;
    tm_->commit(xid1);

    // T2: Delete chunk but DON'T commit
    uint64_t xid2 = tm_->beginTransaction();
    status = toast_mgr.deleteToastValue(value_id, xid2, &ctx);
    ASSERT_EQ(status, Status::OK);
    // T2 still active - don't commit/abort yet

    // T3: Try to read chunk
    uint64_t xid3 = tm_->beginTransaction();
    std::vector<uint8_t> detoasted_data;
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid3, &ctx);

    // Chunk should be VISIBLE to T3 (xmax still active)
    EXPECT_EQ(status, Status::OK) << "Chunk should be visible to T3 (xmax active): " << ctx.message;
    EXPECT_EQ(detoasted_data, data_vec) << "Detoasted data should match original";

    tm_->abort(xid2);
    tm_->commit(xid3);
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
    uint64_t xid1 = tm_->beginTransaction();
    std::string data = "TOAST_TIP_STATE_TRANSITION_TEST";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        data_vec.data(),
        data_vec.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);

    // T2: Try to read while T1 active
    uint64_t xid2 = tm_->beginTransaction();
    std::vector<uint8_t> detoasted_data;
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid2, &ctx);

    // Should be invisible (T1 active)
    EXPECT_NE(status, Status::OK) << "Chunk should be invisible while T1 active";

    // Commit T1 (TX_ACTIVE → TX_COMMITTED)
    tm_->commit(xid1);

    // T2: Try to read again after T1 committed
    detoasted_data.clear();
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid2, &ctx);

    // Should be visible (T1 committed)
    EXPECT_EQ(status, Status::OK) << "Chunk should be visible after T1 commits: " << ctx.message;
    EXPECT_EQ(detoasted_data, data_vec) << "Detoasted data should match original";

    tm_->commit(xid2);
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
    uint64_t xid1 = tm_->beginTransaction();
    std::string data = "TOAST_SNAPSHOT_ISOLATION_TEST";
    std::vector<uint8_t> data_vec(data.begin(), data.end());

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        data_vec.data(),
        data_vec.size(),
        ToastStrategy::EXTERNAL,
        xid1,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);

    // T1: Read own chunk (before commit)
    std::vector<uint8_t> detoasted_data;
    status = toast_mgr.detoastValue(&toast_ptr, &detoasted_data, xid1, &ctx);

    // T1 should see its own chunk (snapshot isolation)
    EXPECT_EQ(status, Status::OK) << "T1 should see its own uncommitted chunk: " << ctx.message;
    EXPECT_EQ(detoasted_data, data_vec) << "Detoasted data should match original";

    tm_->commit(xid1);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
