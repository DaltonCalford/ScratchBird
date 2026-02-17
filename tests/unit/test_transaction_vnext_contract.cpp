#include <gtest/gtest.h>
#include <memory>

#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/vnext_error_codes.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class TransactionVNextContractTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>(
            "test_tx_vnext_contract", ".db");

        ErrorContext ctx;
        ASSERT_EQ(Status::OK, Database::create(test_db_file_->path(), 8192, &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.open(test_db_file_->path(), &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.initializeProcArray(32, &ctx)) << ctx.message;

        tm_ = db_.transaction_manager();
        ASSERT_NE(nullptr, tm_);
    }

    void TearDown() override
    {
        db_.close();
        test_db_file_.reset();
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_file_;
    Database db_;
    TransactionManager *tm_ = nullptr;
};

TEST_F(TransactionVNextContractTest, RejectsInvalidCommitTransitionAfterRollback)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, xid, &ctx)) << ctx.message;

    EXPECT_EQ(Status::INVALID_ARGUMENT, tm_->commitTransaction(proc_id, xid, &ctx));
    EXPECT_EQ(std::string("TXN_0201"), ctx.vnext_code);
    EXPECT_TRUE(isKnownVNextErrorCode(ctx.vnext_code));
    EXPECT_TRUE(statusMatchesVNextErrorCode(ctx.code, ctx.vnext_code));

    TransactionState state = TransactionState::ACTIVE;
    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::ABORTED, state);

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionVNextContractTest, StartupNormalizesResidualActiveToAborted)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid_active = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid_active, &ctx)) << ctx.message;

    // Simulate process drop with in-progress transaction left in TIP.
    ProcArrayManager::unregisterBackend(proc_id, &ctx);
    db_.close();

    ASSERT_EQ(Status::OK, db_.open(test_db_file_->path(), &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, db_.initializeProcArray(32, &ctx)) << ctx.message;
    tm_ = db_.transaction_manager();
    ASSERT_NE(nullptr, tm_);

    TransactionState state = TransactionState::ACTIVE;
    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid_active, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::ABORTED, state);
}

TEST_F(TransactionVNextContractTest, SnapshotVisibilityUsesActiveSetDeterministically)
{
    ErrorContext ctx;
    uint32_t writer1 = 0;
    uint32_t writer2 = 0;
    uint32_t reader = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&writer1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&writer2, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&reader, &ctx)) << ctx.message;

    uint64_t xid_committed = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(writer1, xid_committed, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(writer1, xid_committed, &ctx)) << ctx.message;

    uint64_t xid_active_then_commit = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(writer2, xid_active_then_commit, &ctx))
        << ctx.message;

    uint64_t xid_reader = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(reader, xid_reader, &ctx)) << ctx.message;

    TransactionSnapshot snapshot_before_commit;
    ASSERT_EQ(Status::OK, tm_->captureSnapshot(snapshot_before_commit, &ctx)) << ctx.message;

    EXPECT_TRUE(
        tm_->isCreateVisibleInSnapshot(xid_committed, xid_reader, snapshot_before_commit));
    EXPECT_FALSE(
        tm_->isCreateVisibleInSnapshot(xid_active_then_commit, xid_reader, snapshot_before_commit));
    EXPECT_TRUE(tm_->isRecordVersionVisibleInSnapshot(
        xid_committed, xid_active_then_commit, xid_reader, snapshot_before_commit));

    ASSERT_EQ(Status::OK, tm_->commitTransaction(writer2, xid_active_then_commit, &ctx))
        << ctx.message;

    // Old snapshot remains stable even after tx2 commits.
    EXPECT_FALSE(
        tm_->isCreateVisibleInSnapshot(xid_active_then_commit, xid_reader, snapshot_before_commit));
    EXPECT_TRUE(tm_->isRecordVersionVisibleInSnapshot(
        xid_committed, xid_active_then_commit, xid_reader, snapshot_before_commit));

    TransactionSnapshot snapshot_after_commit;
    ASSERT_EQ(Status::OK, tm_->captureSnapshot(snapshot_after_commit, &ctx)) << ctx.message;
    EXPECT_TRUE(
        tm_->isCreateVisibleInSnapshot(xid_active_then_commit, xid_reader, snapshot_after_commit));
    EXPECT_FALSE(tm_->isRecordVersionVisibleInSnapshot(
        xid_committed, xid_active_then_commit, xid_reader, snapshot_after_commit));

    ASSERT_EQ(Status::OK, tm_->commitTransaction(reader, xid_reader, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(writer1, &ctx);
    ProcArrayManager::unregisterBackend(writer2, &ctx);
    ProcArrayManager::unregisterBackend(reader, &ctx);
}
