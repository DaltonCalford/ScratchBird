#include <gtest/gtest.h>
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/global_uniqueness_index.h"
#include <chrono>
#include <filesystem>
#include <sstream>
#include <thread>

using namespace scratchbird::core;

namespace
{
    std::string generateUniqueDbPath()
    {
        std::ostringstream oss;
        oss << "/tmp/test_global_uniqueness_index_"
            << std::this_thread::get_id() << "_"
            << std::chrono::steady_clock::now().time_since_epoch().count()
            << ".sbdb";
        return oss.str();
    }
}

class GlobalUniquenessIndexTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = generateUniqueDbPath();
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        index_ = std::make_unique<GlobalUniquenessIndex>(&db_);
        domain_id_ = generateUuidV7();
        table_id_ = generateUuidV7();
        column_id_ = generateUuidV7();
        row_tid_1_ = makeTID(0, 1, 1);
        row_tid_2_ = makeTID(0, 1, 2);

        status = index_->enableUniqueness(domain_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        index_.reset();
        db_.close();
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    std::unique_ptr<ConnectionContext> connect(ErrorContext &ctx)
    {
        std::unique_ptr<ConnectionContext> conn;
        Status status = db_.connect(conn, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return conn;
    }

    std::string test_db_path_;
    Database db_;
    std::unique_ptr<GlobalUniquenessIndex> index_;
    ID domain_id_;
    ID table_id_;
    ID column_id_;
    TID row_tid_1_;
    TID row_tid_2_;
};

TEST_F(GlobalUniquenessIndexTest, InsertAndVisibility)
{
    ErrorContext ctx;
    TypedValue value = TypedValue::makeInt32(42);

    auto conn1 = connect(ctx);
    ASSERT_NE(conn1, nullptr);
    ConnectionContext::setCurrent(conn1.get());
    uint64_t xid1 = conn1->getCurrentXid();

    bool is_unique = false;
    EXPECT_EQ(index_->checkUniqueness(domain_id_, value, xid1, is_unique, &ctx), Status::OK);
    EXPECT_TRUE(is_unique);

    EXPECT_EQ(index_->insertValue(domain_id_, table_id_, column_id_, row_tid_1_,
                                  value, xid1, &ctx), Status::OK);

    EXPECT_EQ(index_->checkUniqueness(domain_id_, value, xid1, is_unique, &ctx), Status::OK);
    EXPECT_FALSE(is_unique);

    auto conn2 = connect(ctx);
    ASSERT_NE(conn2, nullptr);
    ConnectionContext::setCurrent(conn2.get());
    uint64_t xid2 = conn2->getCurrentXid();

    EXPECT_EQ(index_->checkUniqueness(domain_id_, value, xid2, is_unique, &ctx), Status::OK);
    EXPECT_TRUE(is_unique);

    ConnectionContext::setCurrent(conn1.get());
    EXPECT_EQ(conn1->commit(&ctx), Status::OK) << ctx.message;

    ConnectionContext::setCurrent(conn2.get());
    EXPECT_EQ(index_->checkUniqueness(domain_id_, value, xid2, is_unique, &ctx), Status::OK);
    EXPECT_FALSE(is_unique);
}

TEST_F(GlobalUniquenessIndexTest, UpdateAndDelete)
{
    ErrorContext ctx;
    TypedValue old_value = TypedValue::makeInt32(10);
    TypedValue new_value = TypedValue::makeInt32(11);

    auto conn1 = connect(ctx);
    ASSERT_NE(conn1, nullptr);
    ConnectionContext::setCurrent(conn1.get());
    uint64_t xid1 = conn1->getCurrentXid();

    EXPECT_EQ(index_->insertValue(domain_id_, table_id_, column_id_, row_tid_1_,
                                  old_value, xid1, &ctx), Status::OK);
    EXPECT_EQ(conn1->commit(&ctx), Status::OK) << ctx.message;

    auto conn2 = connect(ctx);
    ASSERT_NE(conn2, nullptr);
    ConnectionContext::setCurrent(conn2.get());
    uint64_t xid2 = conn2->getCurrentXid();

    EXPECT_EQ(index_->updateValue(domain_id_, table_id_, column_id_, row_tid_1_,
                                  old_value, new_value, xid2, &ctx), Status::OK);
    EXPECT_EQ(conn2->commit(&ctx), Status::OK) << ctx.message;

    auto conn3 = connect(ctx);
    ASSERT_NE(conn3, nullptr);
    ConnectionContext::setCurrent(conn3.get());
    uint64_t xid3 = conn3->getCurrentXid();

    bool is_unique = false;
    EXPECT_EQ(index_->checkUniqueness(domain_id_, old_value, xid3, is_unique, &ctx), Status::OK);
    EXPECT_TRUE(is_unique);

    EXPECT_EQ(index_->checkUniqueness(domain_id_, new_value, xid3, is_unique, &ctx), Status::OK);
    EXPECT_FALSE(is_unique);

    EXPECT_EQ(index_->deleteValue(domain_id_, table_id_, column_id_, row_tid_1_,
                                  new_value, xid3, &ctx), Status::OK);
    EXPECT_EQ(conn3->commit(&ctx), Status::OK) << ctx.message;

    auto conn4 = connect(ctx);
    ASSERT_NE(conn4, nullptr);
    ConnectionContext::setCurrent(conn4.get());
    uint64_t xid4 = conn4->getCurrentXid();

    EXPECT_EQ(index_->checkUniqueness(domain_id_, new_value, xid4, is_unique, &ctx), Status::OK);
    EXPECT_TRUE(is_unique);
}

TEST_F(GlobalUniquenessIndexTest, NullsAndDisable)
{
    ErrorContext ctx;
    TypedValue null_value = TypedValue::makeNull();
    TypedValue value = TypedValue::makeInt32(7);

    auto conn = connect(ctx);
    ASSERT_NE(conn, nullptr);
    ConnectionContext::setCurrent(conn.get());
    uint64_t xid = conn->getCurrentXid();

    bool is_unique = false;
    EXPECT_EQ(index_->checkUniqueness(domain_id_, null_value, xid, is_unique, &ctx), Status::OK);
    EXPECT_TRUE(is_unique);

    EXPECT_EQ(index_->insertValue(domain_id_, table_id_, column_id_, row_tid_2_,
                                  null_value, xid, &ctx), Status::OK);
    EXPECT_EQ(index_->checkUniqueness(domain_id_, null_value, xid, is_unique, &ctx), Status::OK);
    EXPECT_TRUE(is_unique);

    EXPECT_EQ(index_->insertValue(domain_id_, table_id_, column_id_, row_tid_2_,
                                  value, xid, &ctx), Status::OK);
    EXPECT_EQ(index_->checkUniqueness(domain_id_, value, xid, is_unique, &ctx), Status::OK);
    EXPECT_FALSE(is_unique);

    EXPECT_EQ(index_->disableUniqueness(domain_id_, &ctx), Status::OK);
    EXPECT_EQ(index_->checkUniqueness(domain_id_, value, xid, is_unique, &ctx), Status::OK);
    EXPECT_TRUE(is_unique);
}

TEST_F(GlobalUniquenessIndexTest, MultipleDomainsAreIsolated)
{
    ErrorContext ctx;
    TypedValue value = TypedValue::makeInt32(5);

    ID domain_two = generateUuidV7();
    EXPECT_EQ(index_->enableUniqueness(domain_two, &ctx), Status::OK);

    auto conn = connect(ctx);
    ASSERT_NE(conn, nullptr);
    ConnectionContext::setCurrent(conn.get());
    uint64_t xid = conn->getCurrentXid();

    EXPECT_EQ(index_->insertValue(domain_id_, table_id_, column_id_, row_tid_1_,
                                  value, xid, &ctx), Status::OK);

    bool is_unique = false;
    EXPECT_EQ(index_->checkUniqueness(domain_two, value, xid, is_unique, &ctx), Status::OK);
    EXPECT_TRUE(is_unique);

    EXPECT_EQ(index_->insertValue(domain_two, table_id_, column_id_, row_tid_2_,
                                  value, xid, &ctx), Status::OK);
    EXPECT_EQ(index_->checkUniqueness(domain_two, value, xid, is_unique, &ctx), Status::OK);
    EXPECT_FALSE(is_unique);
}

TEST_F(GlobalUniquenessIndexTest, HandlesManyEntries)
{
    ErrorContext ctx;
    auto conn = connect(ctx);
    ASSERT_NE(conn, nullptr);
    ConnectionContext::setCurrent(conn.get());
    uint64_t xid = conn->getCurrentXid();

    for (int32_t i = 0; i < 1000; ++i)
    {
        TypedValue value = TypedValue::makeInt32(i);
        TID tid = makeTID(0, 1, static_cast<uint16_t>(i + 1));
        EXPECT_EQ(index_->insertValue(domain_id_, table_id_, column_id_, tid,
                                      value, xid, &ctx), Status::OK);
    }

    bool is_unique = false;
    TypedValue existing = TypedValue::makeInt32(0);
    EXPECT_EQ(index_->checkUniqueness(domain_id_, existing, xid, is_unique, &ctx), Status::OK);
    EXPECT_FALSE(is_unique);

    TypedValue new_value = TypedValue::makeInt32(2000);
    EXPECT_EQ(index_->checkUniqueness(domain_id_, new_value, xid, is_unique, &ctx), Status::OK);
    EXPECT_TRUE(is_unique);
}
