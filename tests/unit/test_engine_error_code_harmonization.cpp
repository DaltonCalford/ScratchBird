#include <gtest/gtest.h>
#include <memory>

#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/vnext_error_codes.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace
{
class EngineErrorCodeHarmonizationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>(
            "test_engine_error_code_harmonization", ".db");

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
} // namespace

TEST_F(EngineErrorCodeHarmonizationTest, VNextRegistryContainsCanonicalPageAndTransactionCodes)
{
    Status mapped = Status::OK;

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("PGX_0001", mapped));
    EXPECT_EQ(Status::PAGE_CORRUPT, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("PGX_0003", mapped));
    EXPECT_EQ(Status::CHECKSUM_MISMATCH, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("TXN_0201", mapped));
    EXPECT_EQ(Status::INVALID_ARGUMENT, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("TXN_0215", mapped));
    EXPECT_EQ(Status::PAGE_CORRUPT, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("SEC_1240", mapped));
    EXPECT_EQ(Status::INVALID_AUTHORIZATION, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("SEC_1259", mapped));
    EXPECT_EQ(Status::PAGE_CORRUPT, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("SEC_1289", mapped));
    EXPECT_EQ(Status::INVALID_AUTHORIZATION, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("SEC_1295", mapped));
    EXPECT_EQ(Status::NOT_SUPPORTED, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("IRX_0401", mapped));
    EXPECT_EQ(Status::INVALID_ARGUMENT, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("IRX_0403", mapped));
    EXPECT_EQ(Status::NOT_SUPPORTED, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("IRX_0407", mapped));
    EXPECT_EQ(Status::INVALID_ARGUMENT, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("OPT_0301", mapped));
    EXPECT_EQ(Status::INVALID_ARGUMENT, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("OPT_0304", mapped));
    EXPECT_EQ(Status::NOT_SUPPORTED, mapped);

    EXPECT_TRUE(tryGetStatusForVNextErrorCode("OPT_0307", mapped));
    EXPECT_EQ(Status::INVALID_ARGUMENT, mapped);

    EXPECT_FALSE(tryGetStatusForVNextErrorCode("TXN_9999", mapped));
}

TEST_F(EngineErrorCodeHarmonizationTest, PrepareRejectsNonActiveWithDeterministicVNextCode)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;

    const ID owner_id{};
    const Status prepare_status =
        tm_->prepareTransaction(proc_id, xid, "gid_en007_non_active_prepare", owner_id, &ctx);
    EXPECT_EQ(Status::INVALID_ARGUMENT, prepare_status);
    EXPECT_EQ(std::string("TXN_0210"), ctx.vnext_code);
    EXPECT_TRUE(isKnownVNextErrorCode(ctx.vnext_code));
    EXPECT_TRUE(statusMatchesVNextErrorCode(prepare_status, ctx.vnext_code));

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}
