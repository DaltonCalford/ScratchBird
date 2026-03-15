/**
 * @file test_transaction_fixes_corrected.cpp
 * @brief Corrected tests that match actual implementation behavior
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class TransactionFixCorrectedTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>(
            "test_tx_fixes_corrected", ".db");

        ErrorContext ctx;
        ASSERT_EQ(Status::OK, Database::create(test_db_file_->path(), 8192, &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.open(test_db_file_->path(), &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.initializeProcArray(32, &ctx)) << ctx.message;

        tm_ = db_.transaction_manager();
        ASSERT_NE(tm_, nullptr);
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

TEST_F(TransactionFixCorrectedTest, NoHanging)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 100; i++)
    {
        uint64_t xid = 0;
        ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;

        if (i % 2 == 0)
        {
            ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;
        }
        else
        {
            ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, xid, &ctx)) << ctx.message;
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // This gate verifies "no hanging" behavior, not micro-benchmark performance.
    // CI/system load and domain bootstrap can add >1s variance, so use a stable upper bound.
    EXPECT_LT(duration.count(), 3000) << "Operations took too long: " << duration.count() << "ms";

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionFixCorrectedTest, XIDStartsAfterFrozen)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid1 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid1, &ctx)) << ctx.message;
    EXPECT_GT(xid1, 2u) << "XID should start after FROZEN_XID (2)";
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid1, &ctx)) << ctx.message;

    uint64_t xid2 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid2, &ctx)) << ctx.message;
    EXPECT_GT(xid2, xid1) << "XIDs should increase monotonically";
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid2, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionFixCorrectedTest, NoDeadlock)
{
    std::atomic<bool> deadlock{false};
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++)
    {
        threads.emplace_back([this, &deadlock, &completed]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = 0;
            if (ProcArrayManager::registerBackend(&proc_id, &thread_ctx) != Status::OK)
            {
                deadlock = true;
                completed++;
                return;
            }

            auto start = std::chrono::steady_clock::now();

            for (int j = 0; j < 100; j++)
            {
                uint64_t xid = 0;
                if (tm_->beginTransaction(proc_id, xid, &thread_ctx) != Status::OK)
                {
                    break;
                }

                auto now = std::chrono::steady_clock::now();
                // This gate checks for lock-order deadlock, not threaded throughput. Under
                // heavy CI/domain-bootstrap load, 400 begin/commit cycles can exceed 2s without
                // any deadlock, so keep a stable upper bound.
                if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > 6)
                {
                    deadlock = true;
                    break;
                }

                tm_->commitTransaction(proc_id, xid, &thread_ctx);
            }

            ProcArrayManager::unregisterBackend(proc_id, &thread_ctx);
            completed++;
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    EXPECT_FALSE(deadlock) << "Deadlock detected";
    EXPECT_EQ(completed, 4) << "Not all threads completed";
}

TEST_F(TransactionFixCorrectedTest, BasicOperations)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;

    TransactionState state;
    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::ACTIVE, state);

    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;

    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::COMMITTED, state);

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionFixCorrectedTest, TransactionStatesInMemory)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    std::vector<uint64_t> committed_xids;
    std::vector<uint64_t> aborted_xids;

    for (int i = 0; i < 5; i++)
    {
        uint64_t xid = 0;
        ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;
        committed_xids.push_back(xid);
    }

    for (int i = 0; i < 5; i++)
    {
        uint64_t xid = 0;
        ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, xid, &ctx)) << ctx.message;
        aborted_xids.push_back(xid);
    }

    for (uint64_t xid : committed_xids)
    {
        TransactionState state;
        ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
        EXPECT_EQ(TransactionState::COMMITTED, state);
    }

    for (uint64_t xid : aborted_xids)
    {
        TransactionState state;
        ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
        EXPECT_EQ(TransactionState::ABORTED, state);
    }

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}
