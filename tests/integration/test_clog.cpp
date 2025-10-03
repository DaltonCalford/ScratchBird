#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/clog.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <cstdio>

using namespace scratchbird::core;

class ClogTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Clean up any existing test database
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    void TearDown() override
    {
        // Clean up test database
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    const std::string test_db_path_ = "test_clog.db";
};

// Test CLOG initialization
TEST_F(ClogTest, Initialization)
{
    ErrorContext ctx;

    // Create database
    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    // Open database
    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    // CLOG should be initialized
    ASSERT_NE(db.clog(), nullptr);
    ASSERT_GT(db.clog()->getRootPage(), 0u);

    db.close();
}

// Test setting and getting transaction status
TEST_F(ClogTest, SetAndGetStatus)
{
    ErrorContext ctx;

    // Create and open database
    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Set transaction statuses
    status = db.clog()->setStatus(100, ClogStatus::COMMITTED, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = db.clog()->setStatus(101, ClogStatus::ABORTED, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = db.clog()->setStatus(102, ClogStatus::IN_PROGRESS, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    // Get transaction statuses
    ClogStatus clog_status;

    status = db.clog()->getStatus(100, &clog_status, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(clog_status, ClogStatus::COMMITTED);

    status = db.clog()->getStatus(101, &clog_status, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(clog_status, ClogStatus::ABORTED);

    status = db.clog()->getStatus(102, &clog_status, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(clog_status, ClogStatus::IN_PROGRESS);

    db.close();
}

// Test CLOG persistence
TEST_F(ClogTest, Persistence)
{
    ErrorContext ctx;

    // Create and open database
    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    {
        Database db;
        status = db.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK);

        // Set some transaction statuses
        status = db.clog()->setStatus(1000, ClogStatus::COMMITTED, &ctx);
        ASSERT_EQ(status, Status::OK);

        status = db.clog()->setStatus(2000, ClogStatus::ABORTED, &ctx);
        ASSERT_EQ(status, Status::OK);

        db.close();
    }

    // Reopen database
    {
        Database db;
        status = db.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK);

        // Verify statuses persist
        ClogStatus clog_status;

        status = db.clog()->getStatus(1000, &clog_status, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(clog_status, ClogStatus::COMMITTED);

        status = db.clog()->getStatus(2000, &clog_status, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(clog_status, ClogStatus::ABORTED);

        db.close();
    }
}

// Test CLOG extension for large XIDs
TEST_F(ClogTest, Extension)
{
    ErrorContext ctx;

    // Create and open database
    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Test with a large XID that requires CLOG extension
    // Each CLOG page holds 65,536 XIDs, so xid=100000 will require extension
    uint64_t large_xid = 100000;

    status = db.clog()->setStatus(large_xid, ClogStatus::COMMITTED, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ClogStatus clog_status;
    status = db.clog()->getStatus(large_xid, &clog_status, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(clog_status, ClogStatus::COMMITTED);

    db.close();
}

// Test CLOG statistics
TEST_F(ClogTest, Statistics)
{
    ErrorContext ctx;

    // Create and open database
    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Set some transactions
    for (uint64_t xid = 0; xid < 1000; xid++)
    {
        status = db.clog()->setStatus(xid, ClogStatus::COMMITTED, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Get statistics
    Clog::ClogStats stats;
    db.clog()->getStatistics(&stats);

    ASSERT_GT(stats.num_pages, 0u);
    ASSERT_GT(stats.total_transactions, 0u);
    ASSERT_GT(stats.space_used_bytes, 0u);
    ASSERT_GT(stats.space_saved_bytes, 0u);

    // Space saved should be significant (160x savings)
    // TIP uses 20 bytes/transaction, CLOG uses 2 bits (0.25 bytes)
    uint64_t tip_space = stats.total_transactions * 20;
    ASSERT_GT(stats.space_saved_bytes, tip_space / 2); // At least 50% savings

    db.close();
}

// Test TransactionManager integration with CLOG
TEST_F(ClogTest, TransactionManagerIntegration)
{
    ErrorContext ctx;

    // Create and open database
    Status status = Database::create(test_db_path_, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db_path_, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Initialize ProcArray for transaction support
    status = db.initializeProcArray(10, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Begin transaction
    uint32_t proc_id = 0;
    uint64_t xid = 0;
    status = db.transaction_manager()->beginTransaction(proc_id, xid, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_GT(xid, 0u);

    // Commit transaction - should write to CLOG
    status = db.transaction_manager()->commitTransaction(proc_id, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify CLOG has the committed status
    ClogStatus clog_status;
    status = db.clog()->getStatus(xid, &clog_status, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(clog_status, ClogStatus::COMMITTED);

    // Test abort
    uint64_t xid2 = 0;
    status = db.transaction_manager()->beginTransaction(proc_id, xid2, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = db.transaction_manager()->rollbackTransaction(proc_id, xid2, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = db.clog()->getStatus(xid2, &clog_status, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(clog_status, ClogStatus::ABORTED);

    db.close();
}
