/**
 * @file test_lock_conflict_matrix.cpp
 * @brief Lock Manager Conflict Matrix Test
 *
 * Suite 4 (MGA & Concurrency Control) Compliance Test
 *
 * This test validates the lock conflict matrix by testing all 64
 * combinations of lock mode pairs (8×8) to ensure that the lock
 * manager correctly grants or conflicts locks according to the
 * PostgreSQL-style compatibility rules.
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <string>
#include <vector>

using namespace scratchbird::core;

class LockConflictMatrixTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = std::filesystem::temp_directory_path() / "test_lock_matrix.sbrd";
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_.string(), 8192, &ctx), Status::OK)
            << ctx.error_message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_.string(), &ctx), Status::OK)
            << ctx.error_message;

        lock_mgr_ = db_->lock_manager();
        ASSERT_NE(lock_mgr_, nullptr);
    }

    void TearDown() override
    {
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }

    static LockTag makeTag()
    {
        LockTag tag{};
        tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
        for (size_t i = 0; i < tag.object_uuid.bytes.size(); ++i)
        {
            tag.object_uuid.bytes[i] = static_cast<uint8_t>(i + 1);
        }
        tag.page_num = 42;
        tag.offset_num = 7;
        tag.padding = 0;
        return tag;
    }

    const std::vector<LockMode> lock_modes_ = {
        LockMode::LOCK_ACCESS_SHARE,
        LockMode::LOCK_ROW_SHARE,
        LockMode::LOCK_ROW_EXCLUSIVE,
        LockMode::LOCK_SHARE_UPDATE_EXCLUSIVE,
        LockMode::LOCK_SHARE,
        LockMode::LOCK_SHARE_ROW_EXCLUSIVE,
        LockMode::LOCK_EXCLUSIVE,
        LockMode::LOCK_ACCESS_EXCLUSIVE
    };

    static const char* lockModeToString(LockMode mode)
    {
        switch (mode)
        {
            case LockMode::LOCK_ACCESS_SHARE: return "ACCESS_SHARE";
            case LockMode::LOCK_ROW_SHARE: return "ROW_SHARE";
            case LockMode::LOCK_ROW_EXCLUSIVE: return "ROW_EXCLUSIVE";
            case LockMode::LOCK_SHARE_UPDATE_EXCLUSIVE: return "SHARE_UPDATE_EXCLUSIVE";
            case LockMode::LOCK_SHARE: return "SHARE";
            case LockMode::LOCK_SHARE_ROW_EXCLUSIVE: return "SHARE_ROW_EXCLUSIVE";
            case LockMode::LOCK_EXCLUSIVE: return "EXCLUSIVE";
            case LockMode::LOCK_ACCESS_EXCLUSIVE: return "ACCESS_EXCLUSIVE";
            default: return "UNKNOWN";
        }
    }

    static bool shouldConflict(LockMode mode1, LockMode mode2)
    {
        // Conflict matrix: rows = held, columns = requested.
        static const bool conflict_matrix[8][8] = {
            // AS    RS    RX    SUE   S     SRE   E     AE
            {false, false, false, false, false, false, false, true},  // ACCESS_SHARE
            {false, false, false, false, false, false, true,  true},  // ROW_SHARE
            {false, false, false, false, true,  true,  true,  true},  // ROW_EXCLUSIVE
            {false, false, false, true,  true,  true,  true,  true},  // SHARE_UPDATE_EXCLUSIVE
            {false, false, true,  true,  false, true,  true,  true},  // SHARE
            {false, false, true,  true,  true,  true,  true,  true},  // SHARE_ROW_EXCLUSIVE
            {false, true,  true,  true,  true,  true,  true,  true},  // EXCLUSIVE
            {true,  true,  true,  true,  true,  true,  true,  true}   // ACCESS_EXCLUSIVE
        };

        const int idx1 = static_cast<int>(mode1) - 1;
        const int idx2 = static_cast<int>(mode2) - 1;
        return conflict_matrix[idx1][idx2];
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<Database> db_;
    LockManager* lock_mgr_ = nullptr;
};

TEST_F(LockConflictMatrixTest, CompleteLockMatrix)
{
    const uint32_t proc1 = 1001;
    const uint32_t proc2 = 1002;
    const LockTag tag = makeTag();

    int test_count = 0;
    int conflict_count = 0;
    int compatible_count = 0;

    for (LockMode mode1 : lock_modes_)
    {
        for (LockMode mode2 : lock_modes_)
        {
            test_count++;

            ErrorContext ctx1;
            Status status1 = lock_mgr_->acquireLock(proc1, tag, mode1, false, 0, &ctx1);
            ASSERT_EQ(status1, Status::OK)
                << "Proc1 failed to acquire " << lockModeToString(mode1)
                << " lock (test " << test_count << "/64): " << ctx1.message;

            ErrorContext ctx2;
            Status status2 = lock_mgr_->acquireLock(proc2, tag, mode2, false, 0, &ctx2);

            const bool expected_conflict = shouldConflict(mode1, mode2);
            if (expected_conflict)
            {
                EXPECT_NE(status2, Status::OK)
                    << "Lock should CONFLICT: Proc1 has " << lockModeToString(mode1)
                    << ", Proc2 requests " << lockModeToString(mode2)
                    << " (test " << test_count << "/64)";
                if (status2 != Status::OK)
                {
                    conflict_count++;
                }
            }
            else
            {
                EXPECT_EQ(status2, Status::OK)
                    << "Lock should be COMPATIBLE: Proc1 has " << lockModeToString(mode1)
                    << ", Proc2 requests " << lockModeToString(mode2)
                    << " (test " << test_count << "/64)";
                if (status2 == Status::OK)
                {
                    compatible_count++;
                    lock_mgr_->releaseLock(proc2, tag, mode2, nullptr);
                }
            }

            lock_mgr_->releaseLock(proc1, tag, mode1, nullptr);
        }
    }

    EXPECT_EQ(test_count, 64);
    EXPECT_GT(conflict_count, 0);
    EXPECT_GT(compatible_count, 0);
}

TEST_F(LockConflictMatrixTest, SelfCompatibility)
{
    const uint32_t proc1 = 1001;
    const LockTag tag = makeTag();

    EXPECT_EQ(lock_mgr_->acquireLock(proc1, tag, LockMode::LOCK_ACCESS_SHARE, false, 0, nullptr),
              Status::OK);
    EXPECT_EQ(lock_mgr_->acquireLock(proc1, tag, LockMode::LOCK_ACCESS_SHARE, false, 0, nullptr),
              Status::OK);

    lock_mgr_->releaseLock(proc1, tag, LockMode::LOCK_ACCESS_SHARE, nullptr);
    lock_mgr_->releaseLock(proc1, tag, LockMode::LOCK_ACCESS_SHARE, nullptr);
}

TEST_F(LockConflictMatrixTest, AccessExclusiveBlocksAll)
{
    const uint32_t proc1 = 1001;
    const uint32_t proc2 = 1002;
    const LockTag tag = makeTag();

    ASSERT_EQ(lock_mgr_->acquireLock(proc1, tag, LockMode::LOCK_ACCESS_EXCLUSIVE, false, 0, nullptr),
              Status::OK);

    for (LockMode mode : lock_modes_)
    {
        Status status = lock_mgr_->acquireLock(proc2, tag, mode, false, 0, nullptr);
        EXPECT_NE(status, Status::OK)
            << "ACCESS_EXCLUSIVE should block " << lockModeToString(mode);
    }

    lock_mgr_->releaseLock(proc1, tag, LockMode::LOCK_ACCESS_EXCLUSIVE, nullptr);
}

TEST_F(LockConflictMatrixTest, AccessShareMostPermissive)
{
    const uint32_t proc1 = 1001;
    const uint32_t proc2 = 1002;
    const LockTag tag = makeTag();

    ASSERT_EQ(lock_mgr_->acquireLock(proc1, tag, LockMode::LOCK_ACCESS_SHARE, false, 0, nullptr),
              Status::OK);

    for (LockMode mode : lock_modes_)
    {
        Status status = lock_mgr_->acquireLock(proc2, tag, mode, false, 0, nullptr);

        if (mode == LockMode::LOCK_ACCESS_EXCLUSIVE)
        {
            EXPECT_NE(status, Status::OK);
        }
        else
        {
            EXPECT_EQ(status, Status::OK)
                << "ACCESS_SHARE should be compatible with " << lockModeToString(mode);
            if (status == Status::OK)
            {
                lock_mgr_->releaseLock(proc2, tag, mode, nullptr);
            }
        }
    }

    lock_mgr_->releaseLock(proc1, tag, LockMode::LOCK_ACCESS_SHARE, nullptr);
}
