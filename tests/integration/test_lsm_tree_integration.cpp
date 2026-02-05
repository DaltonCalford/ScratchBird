/**
 * LSM-Tree Integration Tests (GTest)
 *
 * Covers:
 * - Create/open/close lifecycle
 * - Put/get/remove operations
 * - Memtable flush to SSTable
 * - MGA visibility via TransactionManager
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/proc_array.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class LsmTreeIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = scratchbird::testing::uniqueTestDbPath("lsm_tree_integration", ".db");
        std::filesystem::remove(db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;

        Status status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK) << ctx.message;

        ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user, true);

        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(txn_mgr_, nullptr);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_.reset();
        std::filesystem::remove(db_path_);

        if (!index_path_.empty())
        {
            std::filesystem::remove_all(index_path_);
        }
    }

    std::vector<uint8_t> makeKey(size_t index)
    {
        std::string key_str = "key_" + std::to_string(index);
        return std::vector<uint8_t>(key_str.begin(), key_str.end());
    }

    std::vector<uint8_t> makeValue(size_t index)
    {
        std::string value_str = "value_" + std::to_string(index) + "_data";
        return std::vector<uint8_t>(value_str.begin(), value_str.end());
    }

    uint64_t beginTxn(ErrorContext *ctx)
    {
        uint64_t xid = 0;
        uint32_t proc_id = conn_ctx_ ? conn_ctx_->getProcId() : 0;
        Status status = txn_mgr_->beginTransaction(proc_id, xid, ctx);
        EXPECT_EQ(status, Status::OK) << (ctx ? ctx->message : "");
        return xid;
    }

    void commitTxn(uint64_t xid, ErrorContext *ctx)
    {
        uint32_t proc_id = conn_ctx_ ? conn_ctx_->getProcId() : 0;
        Status status = txn_mgr_->commitTransaction(proc_id, xid, ctx);
        EXPECT_EQ(status, Status::OK) << (ctx ? ctx->message : "");
    }

    std::string db_path_;
    std::string index_path_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    TransactionManager *txn_mgr_ = nullptr;
};

TEST_F(LsmTreeIntegrationTest, CreateOpenClose)
{
    index_path_ = scratchbird::testing::uniqueTestShortPath("lsm_tree_create_open");
    std::filesystem::remove_all(index_path_);

    LSMTreeIndex index(db_.get(), index_path_, txn_mgr_, 4);
    ErrorContext ctx;
    ASSERT_EQ(index.create(&ctx), Status::OK) << ctx.message;
    ASSERT_EQ(index.close(&ctx), Status::OK) << ctx.message;

    LSMTreeIndex reopened(db_.get(), index_path_, txn_mgr_, 4);
    ASSERT_EQ(reopened.open(&ctx), Status::OK) << ctx.message;
    ASSERT_EQ(reopened.close(&ctx), Status::OK) << ctx.message;
}

TEST_F(LsmTreeIntegrationTest, PutGet)
{
    index_path_ = scratchbird::testing::uniqueTestShortPath("lsm_tree_putget");
    std::filesystem::remove_all(index_path_);

    LSMTreeIndex index(db_.get(), index_path_, txn_mgr_, 4);
    ErrorContext ctx;
    ASSERT_EQ(index.create(&ctx), Status::OK) << ctx.message;

    uint64_t xid = beginTxn(&ctx);
    for (size_t i = 0; i < 100; i++)
    {
        ASSERT_EQ(index.put(makeKey(i), makeValue(i), xid, &ctx), Status::OK) << ctx.message;
    }
    commitTxn(xid, &ctx);

    uint64_t read_xid = beginTxn(&ctx);
    size_t found_count = 0;
    for (size_t i = 0; i < 100; i++)
    {
        std::vector<uint8_t> actual_value;
        bool found = false;
        ASSERT_EQ(index.get(makeKey(i), read_xid, &actual_value, &found, &ctx), Status::OK)
            << ctx.message;
        if (found && actual_value == makeValue(i))
        {
            found_count++;
        }
    }

    EXPECT_EQ(found_count, 100u);
    commitTxn(read_xid, &ctx);

    ASSERT_EQ(index.close(&ctx), Status::OK) << ctx.message;
}

TEST_F(LsmTreeIntegrationTest, FlushCreatesSSTable)
{
    index_path_ = scratchbird::testing::uniqueTestShortPath("lsm_tree_flush");
    std::filesystem::remove_all(index_path_);

    LSMTreeIndex index(db_.get(), index_path_, txn_mgr_, 1);
    ErrorContext ctx;
    ASSERT_EQ(index.create(&ctx), Status::OK) << ctx.message;

    uint64_t xid = beginTxn(&ctx);
    for (size_t i = 0; i < 100; i++)
    {
        ASSERT_EQ(index.put(makeKey(i), makeValue(i), xid, &ctx), Status::OK) << ctx.message;
    }
    commitTxn(xid, &ctx);

    ASSERT_EQ(index.flush(&ctx), Status::OK) << ctx.message;

    Statistics stats;
    ASSERT_EQ(index.getStatistics(&stats, &ctx), Status::OK) << ctx.message;
    EXPECT_GT(stats.level0_sstables, 0u);

    uint64_t read_xid = beginTxn(&ctx);
    size_t found_count = 0;
    for (size_t i = 0; i < 100; i++)
    {
        std::vector<uint8_t> value;
        bool found = false;
        ASSERT_EQ(index.get(makeKey(i), read_xid, &value, &found, &ctx), Status::OK)
            << ctx.message;
        if (found)
        {
            found_count++;
        }
    }
    EXPECT_EQ(found_count, 100u);
    commitTxn(read_xid, &ctx);

    ASSERT_EQ(index.close(&ctx), Status::OK) << ctx.message;
}

TEST_F(LsmTreeIntegrationTest, DeleteInsertsTombstones)
{
    index_path_ = scratchbird::testing::uniqueTestShortPath("lsm_tree_delete");
    std::filesystem::remove_all(index_path_);

    LSMTreeIndex index(db_.get(), index_path_, txn_mgr_, 4);
    ErrorContext ctx;
    ASSERT_EQ(index.create(&ctx), Status::OK) << ctx.message;

    uint64_t xid = beginTxn(&ctx);
    for (size_t i = 0; i < 50; i++)
    {
        ASSERT_EQ(index.put(makeKey(i), makeValue(i), xid, &ctx), Status::OK) << ctx.message;
    }
    commitTxn(xid, &ctx);

    uint64_t delete_xid = beginTxn(&ctx);
    for (size_t i = 0; i < 25; i++)
    {
        ASSERT_EQ(index.remove(makeKey(i), delete_xid, &ctx), Status::OK) << ctx.message;
    }
    commitTxn(delete_xid, &ctx);

    uint64_t read_xid = beginTxn(&ctx);
    size_t deleted_count = 0;
    size_t found_count = 0;
    for (size_t i = 0; i < 25; i++)
    {
        std::vector<uint8_t> value;
        bool found = false;
        ASSERT_EQ(index.get(makeKey(i), read_xid, &value, &found, &ctx), Status::OK)
            << ctx.message;
        if (!found)
        {
            deleted_count++;
        }
    }
    for (size_t i = 25; i < 50; i++)
    {
        std::vector<uint8_t> value;
        bool found = false;
        ASSERT_EQ(index.get(makeKey(i), read_xid, &value, &found, &ctx), Status::OK)
            << ctx.message;
        if (found)
        {
            found_count++;
        }
    }

    EXPECT_EQ(deleted_count, 25u);
    EXPECT_EQ(found_count, 25u);
    commitTxn(read_xid, &ctx);

    ASSERT_EQ(index.close(&ctx), Status::OK) << ctx.message;
}
