#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"

#include <algorithm>
#include <thread>
#include <vector>

using namespace scratchbird::core;

class TransactionManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>(
            "test_transactions", ".db");

        ErrorContext ctx;
        ASSERT_EQ(Status::OK, Database::create(test_db_file_->path(), 8192, &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.open(test_db_file_->path(), &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.initializeProcArray(16, &ctx)) << ctx.message;

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

TEST_F(TransactionManagerTest, BasicTransaction)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
    EXPECT_GT(xid, 0u);

    TransactionState state;
    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::ACTIVE, state);

    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::COMMITTED, state);

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionManagerTest, TransactionRollback)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, xid, &ctx)) << ctx.message;

    TransactionState state;
    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::ABORTED, state);

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionManagerTest, MultipleBackendsAllowed)
{
    ErrorContext ctx;
    uint32_t proc1 = 0;
    uint32_t proc2 = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc2, &ctx)) << ctx.message;

    uint64_t xid1 = 0;
    uint64_t xid2 = 0;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc1, xid1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc2, xid2, &ctx)) << ctx.message;

    EXPECT_NE(xid1, xid2);

    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc1, xid1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc2, xid2, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(proc1, &ctx);
    ProcArrayManager::unregisterBackend(proc2, &ctx);
}

TEST_F(TransactionManagerTest, XIDGeneration)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    std::vector<uint64_t> xids;
    for (int i = 0; i < 10; i++)
    {
        uint64_t xid = 0;
        ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
        xids.push_back(xid);
        ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;
    }

    for (size_t i = 1; i < xids.size(); i++)
    {
        EXPECT_GT(xids[i], xids[i - 1]);
    }

    for (uint64_t xid : xids)
    {
        EXPECT_GT(xid, 2u);
    }

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionManagerTest, TransactionVisibility)
{
    ErrorContext ctx;
    uint32_t proc1 = 0;
    uint32_t proc2 = 0;
    uint32_t proc3 = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc2, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc3, &ctx)) << ctx.message;

    uint64_t xid1 = 0;
    uint64_t xid2 = 0;
    uint64_t xid3 = 0;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc1, xid1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc1, xid1, &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc2, xid2, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc2, xid2, &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc3, xid3, &ctx)) << ctx.message;

    EXPECT_TRUE(tm_->isTransactionVisible(xid1, xid3));
    EXPECT_FALSE(tm_->isTransactionVisible(xid2, xid3));
    EXPECT_TRUE(tm_->isTransactionVisible(xid3, xid3));
    EXPECT_FALSE(tm_->isTransactionVisible(xid3 + 100, xid3));

    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc3, xid3, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(proc1, &ctx);
    ProcArrayManager::unregisterBackend(proc2, &ctx);
    ProcArrayManager::unregisterBackend(proc3, &ctx);
}

TEST_F(TransactionManagerTest, BootstrapVersionTraversalDecisionOwnsFollowStopAndCorruption)
{
    const uint64_t reader_xid = 100;

    const auto follow = TransactionManager::evaluateBootstrapVersionTraversalStep(
        150, 0, true, reader_xid);
    EXPECT_EQ(follow.action, VersionTraversalAction::FOLLOW_BACK_VERSION);
    EXPECT_EQ(follow.status, Status::OK);
    EXPECT_EQ(follow.reason, VisibilityReason::FUTURE_XID);

    const auto stop = TransactionManager::evaluateBootstrapVersionTraversalStep(
        90, 95, false, reader_xid);
    EXPECT_EQ(stop.action, VersionTraversalAction::TERMINAL_NOT_VISIBLE);
    EXPECT_EQ(stop.status, Status::OK);
    EXPECT_EQ(stop.reason, VisibilityReason::COMMITTED_VISIBLE);
    EXPECT_TRUE(stop.record_decision.create_visible);
    EXPECT_TRUE(stop.record_decision.delete_visible);

    const auto corrupt =
        TransactionManager::evaluateBootstrapVersionTraversalStep(0, 0, false, reader_xid);
    EXPECT_EQ(corrupt.action, VersionTraversalAction::CORRUPT_VERSION);
    EXPECT_EQ(corrupt.status, Status::PAGE_CORRUPT);
    EXPECT_EQ(corrupt.reason, VisibilityReason::INVALID_XID);
}

TEST_F(TransactionManagerTest, TransactionPersistence)
{
    ErrorContext ctx;
    uint64_t xid1 = 0;
    uint64_t xid2 = 0;

    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid1, &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid2, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, xid2, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
    db_.close();

    ASSERT_EQ(Status::OK, db_.open(test_db_file_->path(), &ctx)) << ctx.message;
    tm_ = db_.transaction_manager();
    ASSERT_NE(tm_, nullptr);

    TransactionState state;
    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid1, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::COMMITTED, state);

    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid2, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::ABORTED, state);

    ASSERT_EQ(Status::OK, db_.initializeProcArray(16, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid3 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid3, &ctx)) << ctx.message;
    EXPECT_GT(xid3, xid2);
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid3, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionManagerTest, ProcArrayActiveTransactions)
{
    ErrorContext ctx;
    uint32_t proc1 = 0;
    uint32_t proc2 = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc2, &ctx)) << ctx.message;

    uint64_t xid1 = 0;
    uint64_t xid2 = 0;

    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc1, xid1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc2, xid2, &ctx)) << ctx.message;

    std::vector<uint64_t> active_xids;
    uint64_t oldest_xmin = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::getActiveTransactions(&active_xids, &oldest_xmin, &ctx))
        << ctx.message;

    EXPECT_NE(std::find(active_xids.begin(), active_xids.end(), xid1), active_xids.end());
    EXPECT_NE(std::find(active_xids.begin(), active_xids.end(), xid2), active_xids.end());

    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc1, xid1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc2, xid2, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(proc1, &ctx);
    ProcArrayManager::unregisterBackend(proc2, &ctx);
}

TEST_F(TransactionManagerTest, TIPPageValidation)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    const int num_transactions = 50;
    std::vector<uint64_t> xids;

    for (int i = 0; i < num_transactions; i++)
    {
        uint64_t xid = 0;
        ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
        xids.push_back(xid);

        if (i % 2 == 0)
        {
            ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;
        }
        else
        {
            ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, xid, &ctx)) << ctx.message;
        }
    }

    for (size_t i = 0; i < xids.size(); i++)
    {
        TransactionState state;
        ASSERT_EQ(Status::OK, tm_->getTransactionState(xids[i], state, &ctx)) << ctx.message;
        if (i % 2 == 0)
        {
            EXPECT_EQ(TransactionState::COMMITTED, state);
        }
        else
        {
            EXPECT_EQ(TransactionState::ABORTED, state);
        }
    }

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionManagerTest, InventoryWalkAdvancesPastAllTerminalTransactions)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    for (int i = 0; i < 6; ++i)
    {
        uint64_t xid = 0;
        ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
        if ((i % 2) == 0)
        {
            ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;
        }
        else
        {
            ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, xid, &ctx)) << ctx.message;
        }
    }

    uint64_t oldest_interesting = 0;
    ASSERT_EQ(Status::OK, tm_->findOldestInterestingXidFromInventory(oldest_interesting, &ctx))
        << ctx.message;
    EXPECT_EQ(oldest_interesting, tm_->getCurrentXid() + 1);

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionManagerTest, InventoryWalkStopsAtPreparedTransaction)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t committed_xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, committed_xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, committed_xid, &ctx)) << ctx.message;

    uint64_t aborted_xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, aborted_xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, aborted_xid, &ctx)) << ctx.message;

    uint64_t prepared_xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, prepared_xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK,
              tm_->prepareTransaction(proc_id,
                                      prepared_xid,
                                      "tm_inventory_walk_prepared",
                                      generateUuidV7(),
                                      &ctx))
        << ctx.message;

    uint64_t later_committed_xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, later_committed_xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, later_committed_xid, &ctx)) << ctx.message;

    uint64_t oldest_interesting = 0;
    ASSERT_EQ(Status::OK, tm_->findOldestInterestingXidFromInventory(oldest_interesting, &ctx))
        << ctx.message;
    EXPECT_EQ(oldest_interesting, prepared_xid);

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionManagerTest, CaptureReclaimHorizonsReflectsCanonicalMarkers)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t committed_xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, committed_xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, committed_xid, &ctx)) << ctx.message;

    uint64_t active_xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, active_xid, &ctx)) << ctx.message;

    ReclaimHorizonSnapshot horizons{};
    ASSERT_EQ(Status::OK, tm_->captureReclaimHorizons(horizons, &ctx)) << ctx.message;

    EXPECT_EQ(horizons.oldest_interesting_xid, tm_->getOldestXid());
    EXPECT_EQ(horizons.oldest_active_xid, tm_->getOldestActiveXid());
    EXPECT_EQ(horizons.oldest_snapshot_xid, tm_->getOldestSnapshot());
    EXPECT_EQ(horizons.current_xid, tm_->getCurrentXid());
    EXPECT_EQ(horizons.heap_reclaim_horizon, tm_->getOldestSnapshot());
    EXPECT_EQ(horizons.toast_reclaim_horizon,
              std::min(tm_->getOldestActiveXid(), tm_->getOldestSnapshot()));

    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, active_xid, &ctx)) << ctx.message;
    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}
