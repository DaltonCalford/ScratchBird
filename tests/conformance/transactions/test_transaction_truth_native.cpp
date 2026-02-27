#include <gtest/gtest.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "test_helpers.h"

namespace scratchbird::tests {
namespace {
using namespace scratchbird::core;

class TransactionTruthNativeTest : public ::testing::Test {
protected:
    class ScopedCurrentConnection {
    public:
        explicit ScopedCurrentConnection(ConnectionContext* ctx)
            : prev_(ConnectionContext::getCurrent()) {
            ConnectionContext::setCurrent(ctx);
        }

        ~ScopedCurrentConnection() {
            ConnectionContext::setCurrent(prev_);
        }

    private:
        ConnectionContext* prev_;
    };

    void SetUp() override {
        test_db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>(
            "a55_transaction_truth_native", ".db");

        ErrorContext ctx;
        ASSERT_EQ(Status::OK, Database::create(test_db_file_->path(), 8192, &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.open(test_db_file_->path(), &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.initializeProcArray(32, &ctx)) << ctx.message;

        tm_ = db_.transaction_manager();
        ASSERT_NE(tm_, nullptr);

        lock_mgr_ = db_.lock_manager();
        ASSERT_NE(lock_mgr_, nullptr);
    }

    void TearDown() override {
        db_.close();
        test_db_file_.reset();
    }

    static void emitRowResult(const std::string& row_id,
                              bool pass,
                              const std::string& details) {
        std::cout << "ROW_RESULT|" << row_id << "|" << (pass ? "PASS" : "FAIL")
                  << "|" << details << std::endl;
    }

    static LockTag makeLockTag() {
        LockTag tag{};
        tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
        for (size_t i = 0; i < tag.object_uuid.bytes.size(); ++i) {
            tag.object_uuid.bytes[i] = static_cast<uint8_t>(0x20 + i);
        }
        tag.page_num = 7;
        tag.offset_num = 1;
        tag.padding = 0;
        return tag;
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_file_;
    Database db_;
    TransactionManager* tm_ = nullptr;
    LockManager* lock_mgr_ = nullptr;
};

TEST_F(TransactionTruthNativeTest, TX001CommitVisibility) {
    ErrorContext ctx;
    uint32_t proc_a = 0;
    uint32_t proc_b = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_a, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_b, &ctx)) << ctx.message;

    uint64_t xid_a = 0;
    uint64_t xid_b = 0;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_a, xid_a, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_a, xid_a, &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_b, xid_b, &ctx)) << ctx.message;
    const bool visible = tm_->isTransactionVisible(xid_a, xid_b);
    EXPECT_TRUE(visible);
    emitRowResult("TX-001", visible, visible ? "committed transaction visible" : "committed transaction not visible");

    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_b, xid_b, &ctx)) << ctx.message;
    ProcArrayManager::unregisterBackend(proc_a, &ctx);
    ProcArrayManager::unregisterBackend(proc_b, &ctx);
}

TEST_F(TransactionTruthNativeTest, TX002RollbackInvisibility) {
    ErrorContext ctx;
    uint32_t proc_a = 0;
    uint32_t proc_b = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_a, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_b, &ctx)) << ctx.message;

    uint64_t xid_a = 0;
    uint64_t xid_b = 0;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_a, xid_a, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_a, xid_a, &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_b, xid_b, &ctx)) << ctx.message;
    const bool visible = tm_->isTransactionVisible(xid_a, xid_b);
    EXPECT_FALSE(visible);
    emitRowResult("TX-002", !visible, visible ? "aborted transaction unexpectedly visible" : "aborted transaction not visible");

    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_b, xid_b, &ctx)) << ctx.message;
    ProcArrayManager::unregisterBackend(proc_a, &ctx);
    ProcArrayManager::unregisterBackend(proc_b, &ctx);
}

TEST_F(TransactionTruthNativeTest, TX003SavepointRollbackSemantics) {
    ErrorContext ctx;
    std::unique_ptr<ConnectionContext> conn;
    ASSERT_EQ(Status::OK, db_.connect(conn, &ctx)) << ctx.message;
    ScopedCurrentConnection scope(conn.get());

    ASSERT_EQ(Status::OK,
              conn->startTransaction(false, IsolationLevel::READ_COMMITTED, true, &ctx))
        << ctx.message;

    ASSERT_EQ(Status::OK, conn->createSavepoint("sp1", &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, conn->createSavepoint("sp2", &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, conn->rollbackToSavepoint("sp1", &ctx)) << ctx.message;

    ErrorContext missing_ctx;
    const auto missing_status = conn->rollbackToSavepoint("sp2", &missing_ctx);
    const bool pass = (missing_status != Status::OK);
    EXPECT_TRUE(pass);
    emitRowResult("TX-003", pass,
                  pass ? "rollback to earlier savepoint invalidated nested savepoint"
                       : "nested savepoint still accessible after rollback");

    ASSERT_EQ(Status::OK, conn->commit(&ctx)) << ctx.message;
}

TEST_F(TransactionTruthNativeTest, TX004ErrorRollbackBehavior) {
    ErrorContext ctx;
    uint32_t proc_id = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;

    ErrorContext bad_commit_ctx;
    const auto bad_commit_status = tm_->commitTransaction(proc_id, xid + 1, &bad_commit_ctx);
    EXPECT_NE(bad_commit_status, Status::OK);

    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, xid, &ctx)) << ctx.message;

    TransactionState state;
    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
    const bool pass = (state == TransactionState::ABORTED);
    EXPECT_TRUE(pass);
    emitRowResult("TX-004", pass,
                  pass ? "failed commit path followed by rollback produced ABORTED"
                       : "transaction not aborted after rollback");

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionTruthNativeTest, TX005IsolationSnapshotBaseline) {
    ErrorContext ctx;
    uint32_t proc_a = 0;
    uint32_t proc_b = 0;
    uint32_t proc_c = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_a, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_b, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_c, &ctx)) << ctx.message;

    uint64_t xid_a = 0;
    uint64_t xid_b = 0;
    uint64_t xid_c = 0;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_a, xid_a, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_b, xid_b, &ctx)) << ctx.message;

    TransactionSnapshot snapshot_b;
    ASSERT_EQ(Status::OK, tm_->captureSnapshot(snapshot_b, &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_a, xid_a, &ctx)) << ctx.message;

    const bool old_snapshot_visible = tm_->isCreateVisibleInSnapshot(xid_a, xid_b, snapshot_b);

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_c, xid_c, &ctx)) << ctx.message;
    TransactionSnapshot snapshot_c;
    ASSERT_EQ(Status::OK, tm_->captureSnapshot(snapshot_c, &ctx)) << ctx.message;
    const bool new_snapshot_visible = tm_->isCreateVisibleInSnapshot(xid_a, xid_c, snapshot_c);

    const bool pass = (!old_snapshot_visible) && new_snapshot_visible;
    EXPECT_TRUE(pass);
    emitRowResult("TX-005", pass,
                  pass ? "older snapshot hid active xid; newer snapshot saw committed xid"
                       : "snapshot visibility invariant failed");

    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_b, xid_b, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_c, xid_c, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(proc_a, &ctx);
    ProcArrayManager::unregisterBackend(proc_b, &ctx);
    ProcArrayManager::unregisterBackend(proc_c, &ctx);
}

TEST_F(TransactionTruthNativeTest, TX006LockConflictDeterministicError) {
    ErrorContext ctx1;
    ErrorContext ctx2;

    const uint32_t proc_a = 1001;
    const uint32_t proc_b = 1002;
    const LockTag tag = makeLockTag();

    ASSERT_EQ(lock_mgr_->acquireLock(proc_a, tag, LockMode::LOCK_ACCESS_EXCLUSIVE, false, 0, &ctx1),
              Status::OK)
        << ctx1.message;

    const auto conflict_status = lock_mgr_->acquireLock(
        proc_b, tag, LockMode::LOCK_ROW_EXCLUSIVE, false, 0, &ctx2);

    const bool pass = (conflict_status != Status::OK);
    EXPECT_TRUE(pass);
    emitRowResult("TX-006", pass,
                  pass ? ("conflict status=" + std::to_string(static_cast<int>(conflict_status)))
                       : "conflicting lock unexpectedly granted");

    lock_mgr_->releaseLock(proc_a, tag, LockMode::LOCK_ACCESS_EXCLUSIVE, nullptr);
}

} // namespace
} // namespace scratchbird::tests
