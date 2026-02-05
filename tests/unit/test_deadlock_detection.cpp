/**
 * @file test_deadlock_detection.cpp
 * @brief Deadlock detection integration test for LockManager + ProcArray + TransactionManager
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <filesystem>
#include "scratchbird/core/database.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

namespace
{
    LockTag makeTag(uint64_t page_num)
    {
        LockTag tag{};
        tag.target_type = LockTarget::LOCK_TARGET_PAGE;
        for (size_t i = 0; i < tag.object_uuid.bytes.size(); ++i)
        {
            tag.object_uuid.bytes[i] = static_cast<uint8_t>(i + 1);
        }
        tag.page_num = page_num;
        tag.offset_num = 0;
        tag.padding = 0;
        return tag;
    }
}

class DeadlockDetectionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = std::filesystem::temp_directory_path() / "test_deadlock_detection.sbrd";
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_.string(), 8192, &ctx), Status::OK)
            << ctx.error_message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_.string(), &ctx), Status::OK)
            << ctx.error_message;

        // Ensure ProcArray is initialized for deadlock victim selection.
        ASSERT_EQ(db_->initializeProcArray(8, &ctx), Status::OK)
            << ctx.error_message;

        lock_mgr_ = db_->lock_manager();
        ASSERT_NE(lock_mgr_, nullptr);
    }

    void TearDown() override
    {
        ErrorContext ctx;
        ProcArrayManager::shutdown(&ctx);
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<Database> db_;
    LockManager* lock_mgr_ = nullptr;
};

TEST_F(DeadlockDetectionTest, DetectsAndResolvesDeadlock)
{
    ErrorContext ctx;
    TransactionManager *txn_mgr = db_->transaction_manager();
    ASSERT_NE(txn_mgr, nullptr);

    uint32_t proc1 = 0;
    uint32_t proc2 = 0;
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc1, &ctx), Status::OK);
    ASSERT_EQ(ProcArrayManager::registerBackend(&proc2, &ctx), Status::OK);

    uint64_t xid1 = 0;
    uint64_t xid2 = 0;
    ASSERT_EQ(txn_mgr->beginTransaction(proc1, xid1, &ctx), Status::OK);
    ASSERT_EQ(txn_mgr->beginTransaction(proc2, xid2, &ctx), Status::OK);

    const LockTag tagA = makeTag(100);
    const LockTag tagB = makeTag(200);

    std::atomic<int> stage1{0};
    std::atomic<int> stage2{0};
    std::atomic<Status> t1_status{Status::OK};
    std::atomic<Status> t2_status{Status::OK};

    auto worker = [&](uint32_t proc_id, const LockTag &first, const LockTag &second,
                      std::atomic<Status> &status_out)
    {
        ErrorContext tctx;

        Status first_status = lock_mgr_->acquireLock(proc_id, first, LockMode::LOCK_EXCLUSIVE,
                                                     false, 0, &tctx);
        if (first_status != Status::OK)
        {
            status_out.store(first_status);
            return;
        }
        stage1.fetch_add(1);

        // Wait until both threads have acquired their first lock.
        auto start_wait = std::chrono::steady_clock::now();
        while (stage1.load() < 2)
        {
            if (std::chrono::steady_clock::now() - start_wait > std::chrono::seconds(1))
            {
                status_out.store(Status::LOCK_TIMEOUT);
                lock_mgr_->releaseAllLocks(proc_id, nullptr);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        stage2.fetch_add(1);

        // This call will block until deadlock detection breaks the cycle.
        Status status = lock_mgr_->acquireLock(proc_id, second, LockMode::LOCK_EXCLUSIVE,
                                               true, 2000, &tctx);
        status_out.store(status);

        // Clean up any locks held by this proc.
        lock_mgr_->releaseAllLocks(proc_id, nullptr);
    };

    std::thread t1(worker, proc1, tagA, tagB, std::ref(t1_status));
    std::thread t2(worker, proc2, tagB, tagA, std::ref(t2_status));

    // Wait for both threads to start waiting on the second lock.
    bool deadlock_ready = false;
    auto wait_start = std::chrono::steady_clock::now();
    while (stage2.load() < 2)
    {
        if (std::chrono::steady_clock::now() - wait_start > std::chrono::seconds(2))
        {
            ADD_FAILURE() << "Timed out waiting for deadlock setup";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (stage2.load() >= 2)
    {
        deadlock_ready = true;
    }

    // Trigger deadlock detection.
    if (deadlock_ready)
    {
        ASSERT_EQ(lock_mgr_->detectDeadlocks(&ctx), Status::OK);
    }

    t1.join();
    t2.join();

    LockStats stats{};
    lock_mgr_->getStatistics(&stats);
    EXPECT_GE(stats.deadlocks_detected, 1u);

    // At least one thread should have acquired its second lock after resolution.
    const Status s1 = t1_status.load();
    const Status s2 = t2_status.load();
    EXPECT_TRUE(s1 == Status::OK || s2 == Status::OK);

    ProcArrayManager::unregisterBackend(proc1, &ctx);
    ProcArrayManager::unregisterBackend(proc2, &ctx);
}
