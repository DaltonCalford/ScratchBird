/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "gtest/gtest.h"

#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/mysql_query_compiler.h"
#include "scratchbird/sblr/executor.h"

#include <algorithm>
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
        core::CatalogManager::SchemaInfo public_schema_info;
        ASSERT_EQ(db_.catalog_manager()->getSchema("public", public_schema_info, &ctx),
                  core::Status::OK) << ctx.message;
        conn_ctx_->setCurrentSchemaId(public_schema_info.schema_id);
        auto system_user_id = db_.catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user_id, true);
        core::ConnectionContext::setCurrent(conn_ctx_.get());

        executor_ = std::make_unique<sblr::Executor>(&db_);
        executor_->setConnectionContext(conn_ctx_.get());
    }

    void TearDown() override {
        core::ConnectionContext::setCurrent(nullptr);
        executor_.reset();
        conn_ctx_.reset();
        db_.close();
    }

    sblr::ExecutionResult compileAndExecute(const std::string& sql) {
        sblr::MySQLQueryCompiler compiler(&db_);
        auto compile_result = compiler.compile(sql);
        if (!compile_result.success()) {
            std::string errors;
            for (const auto& err : compile_result.errors()) {
                errors += err + "\n";
            }
            return sblr::ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    core::Database db_;
    std::unique_ptr<core::ConnectionContext> conn_ctx_;
    std::unique_ptr<sblr::Executor> executor_;
};

TEST_F(MySQLQueryCompilerTest, CompileAndExecuteSelectLiteral) {
    auto result = compileAndExecute("SELECT 1 FROM dual");
    ASSERT_TRUE(result.success()) << result.error();
}

TEST_F(MySQLQueryCompilerTest, AlterTableModifyChangeCharsetCollation) {
    core::ErrorContext ctx;
    auto create_result = compileAndExecute(
        "CREATE TABLE t1 (name VARCHAR(16) CHARACTER SET utf8mb4)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto modify_result = compileAndExecute(
        "ALTER TABLE t1 MODIFY COLUMN name VARCHAR(32) "
        "CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci");
    ASSERT_TRUE(modify_result.success()) << modify_result.error();

    auto change_result = compileAndExecute(
        "ALTER TABLE t1 CHANGE COLUMN name name VARCHAR(64) "
        "CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci");
    ASSERT_TRUE(change_result.success()) << change_result.error();

    core::CatalogManager::SchemaInfo public_schema_info;
    ASSERT_EQ(db_.catalog_manager()->getSchema("public", public_schema_info, &ctx),
              core::Status::OK) << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_.catalog_manager()->getTable(
                  public_schema_info.schema_id, "t1", table_info, &ctx),
              core::Status::OK) << ctx.message;

    std::vector<core::CatalogManager::ColumnInfo> columns;
    ASSERT_EQ(db_.catalog_manager()->getColumns(table_info.table_id, columns, &ctx),
              core::Status::OK) << ctx.message;

    core::CatalogManager::CharsetInfo charset_info;
    ASSERT_EQ(db_.catalog_manager()->getCharsetByName("utf8mb4", charset_info, &ctx),
              core::Status::OK) << ctx.message;

    core::CatalogManager::CollationCatalogInfo coll_info;
    ASSERT_EQ(db_.catalog_manager()->getCollationByName("utf8mb4_general_ci", coll_info, &ctx),
              core::Status::OK) << ctx.message;

    auto it = std::find_if(columns.begin(), columns.end(), [](const auto& col) {
        return col.column_name == "name";
    });
    ASSERT_NE(it, columns.end());
    EXPECT_EQ(it->charset, charset_info.charset_id);
    EXPECT_EQ(it->collation_id, coll_info.collation_id);
}

TEST_F(MySQLQueryCompilerTest, WindowFunctionRowNumberExec) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_window (id INT, dept INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO t_window (id, dept) VALUES (10, 1), (20, 1), (30, 2)").success());

    auto result = compileAndExecute(
        "SELECT id, ROW_NUMBER() OVER (ORDER BY id) AS rn "
        "FROM t_window ORDER BY id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet()) << "Expected result set";

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 3u);
    EXPECT_EQ(rs->getValue(0, 1).toInt64(), 1);
    EXPECT_EQ(rs->getValue(1, 1).toInt64(), 2);
    EXPECT_EQ(rs->getValue(2, 1).toInt64(), 3);
}

TEST_F(MySQLQueryCompilerTest, MatchAgainstExec) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_docs (id INT, content TEXT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO t_docs (id, content) VALUES "
        "(1, 'alpha beta'), (2, 'gamma delta')").success());

    auto result = compileAndExecute(
        "SELECT id FROM t_docs WHERE MATCH(content) AGAINST ('alpha') > 0");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet()) << "Expected result set";

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
}
