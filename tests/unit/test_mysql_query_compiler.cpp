#include "gtest/gtest.h"

#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/mysql_query_compiler.h"
#include "scratchbird/sblr/executor.h"

#include <filesystem>

using namespace scratchbird;

namespace {
std::filesystem::path testDbPath() {
    return std::filesystem::path("build") / "database" / "test_mysql_compiler.sbdb";
}
}

class MySQLQueryCompilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories(testDbPath().parent_path());
        std::error_code ec;
        std::filesystem::remove(testDbPath(), ec);

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(testDbPath().string(), 16384, &ctx), core::Status::OK)
            << ctx.message;
        ASSERT_EQ(db_.open(testDbPath().string(), &ctx), core::Status::OK) << ctx.message;

        ASSERT_EQ(db_.connect(conn_ctx_, &ctx), core::Status::OK) << ctx.message;
        core::ConnectionContext::setCurrent(conn_ctx_.get());
    }

    void TearDown() override {
        core::ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_.close();
    }

    core::Database db_;
    std::unique_ptr<core::ConnectionContext> conn_ctx_;
};

TEST_F(MySQLQueryCompilerTest, CompileAndExecuteSelectLiteral) {
    sblr::MySQLQueryCompiler compiler(&db_);
    auto compile_result = compiler.compile("SELECT 1 FROM dual");

    ASSERT_TRUE(compile_result.success());
    EXPECT_FALSE(compile_result.bytecode().empty());
}
