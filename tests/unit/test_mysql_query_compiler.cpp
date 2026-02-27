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
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_opcodes.generated.h"
#include "test_helpers.h"

#include <algorithm>
#include <filesystem>

using namespace scratchbird;
using scratchbird::testing::TestDatabaseFile;

class MySQLQueryCompilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_file_ = std::make_unique<TestDatabaseFile>("test_mysql_compiler");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(test_db_file_->path(), 16384, &ctx), core::Status::OK)
            << ctx.message;
        ASSERT_EQ(db_.open(test_db_file_->path(), &ctx), core::Status::OK) << ctx.message;

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
        test_db_file_.reset();
    }

    std::unique_ptr<TestDatabaseFile> test_db_file_;

    sblr::ExecutionResult compileAndExecute(const std::string& sql) {
        return compileAndExecuteWithDefaultSchema(sql, "");
    }

    sblr::ExecutionResult compileAndExecuteWithDefaultSchema(const std::string& sql,
                                                             const std::string& default_schema) {
        sblr::MySQLQueryCompiler compiler(&db_);
        if (!default_schema.empty()) {
            compiler.setDefaultSchema(default_schema);
        }
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

TEST_F(MySQLQueryCompilerTest, IfFunctionAcceptsExistsSubqueryArgument) {
    auto create_result = compileAndExecute(
        "CREATE TABLE sb_tx_truth (id INT PRIMARY KEY, val INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO sb_tx_truth(id, val) VALUES (1, 10)").success());

    auto result = compileAndExecute(
        "SELECT CONCAT('ROW_RESULT|TX-001|', "
        "IF(EXISTS(SELECT 1 FROM sb_tx_truth WHERE id = 1), 'PASS', 'FAIL'), "
        "'|commit_visibility')");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toString(),
              "ROW_RESULT|TX-001|PASS|commit_visibility");
}

TEST_F(MySQLQueryCompilerTest, CreateDatabaseBuildsCanonicalEmulatedRootSchema) {
    constexpr const char* kRoot = "emulated.mysql.localhost.databases.main";

    sblr::MySQLQueryCompiler compiler(&db_);
    compiler.setDefaultSchema(kRoot);
    auto compile_result = compiler.compile("CREATE DATABASE IF NOT EXISTS main");
    ASSERT_TRUE(compile_result.success());

    scratchbird::sblr::v3::Container container;
    std::string decode_err;
    ASSERT_TRUE(scratchbird::sblr::v3::decodeContainer(compile_result.bytecode().data(),
                                                       compile_result.bytecode().size(),
                                                       container,
                                                       decode_err))
        << decode_err;

    bool saw_create_db = false;
    scratchbird::sblr::v3::DecodeError inst_err;
    size_t offset = 0;
    while (offset < container.bytecode_stream.size()) {
        scratchbird::sblr::v3::Instruction inst;
        ASSERT_TRUE(scratchbird::sblr::v3::decodeInstructionWithSchema(
            container.bytecode_stream.data(),
            container.bytecode_stream.size(),
            offset,
            inst,
            inst_err)) << inst_err.message;
        if (inst.opcode == static_cast<uint16_t>(scratchbird::sblr::v3::Opcode::SBLR3_CREATE_DATABASE)) {
            saw_create_db = true;
            break;
        }
    }
    ASSERT_TRUE(saw_create_db);

    auto create_db = executor_->execute(compile_result.bytecode());
    ASSERT_TRUE(create_db.success()) << create_db.error();

    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db_.catalog_manager()->getSchema(kRoot, schema_info, &ctx),
              core::Status::OK)
        << ctx.message;

    conn_ctx_->set_search_path({kRoot});
    conn_ctx_->setCurrentSchemaId(schema_info.schema_id);

    auto create_table = compileAndExecuteWithDefaultSchema(
        "CREATE TABLE IF NOT EXISTS sb_tx_truth_probe (id INT PRIMARY KEY)",
        kRoot);
    ASSERT_TRUE(create_table.success()) << create_table.error();

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_.catalog_manager()->getTable(schema_info.schema_id,
                                              "sb_tx_truth_probe",
                                              table_info,
                                              &ctx),
              core::Status::OK)
        << ctx.message;
}

TEST_F(MySQLQueryCompilerTest, QualifiedCrossDatabaseReferencesResolveUnderSharedMySqlRoot) {
    constexpr const char* kRoot = "emulated.mysql.localhost.databases.main";

    auto create_main = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS mymain",
        kRoot);
    ASSERT_TRUE(create_main.success()) << create_main.error();

    auto create_alt = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS mymain1",
        kRoot);
    ASSERT_TRUE(create_alt.success()) << create_alt.error();

    auto create_a = compileAndExecuteWithDefaultSchema(
        "CREATE TABLE mymain.t_a (id INT PRIMARY KEY, v INT)",
        kRoot);
    ASSERT_TRUE(create_a.success()) << create_a.error();

    auto create_b = compileAndExecuteWithDefaultSchema(
        "CREATE TABLE mymain1.t_b (id INT PRIMARY KEY, v INT)",
        kRoot);
    ASSERT_TRUE(create_b.success()) << create_b.error();

    ASSERT_TRUE(compileAndExecuteWithDefaultSchema(
        "INSERT INTO mymain.t_a(id, v) VALUES (1, 10)",
        kRoot).success());
    ASSERT_TRUE(compileAndExecuteWithDefaultSchema(
        "INSERT INTO mymain1.t_b(id, v) VALUES (2, 20)",
        kRoot).success());

    auto main_sum = compileAndExecuteWithDefaultSchema(
        "SELECT SUM(v) FROM mymain.t_a",
        kRoot);
    ASSERT_TRUE(main_sum.success()) << main_sum.error();
    ASSERT_TRUE(main_sum.hasResultSet());
    ASSERT_EQ(main_sum.resultSet()->rowCount(), 1u);
    EXPECT_EQ(main_sum.resultSet()->getValue(0, 0).toInt64(), 10);

    auto alt_sum = compileAndExecuteWithDefaultSchema(
        "SELECT SUM(v) FROM mymain1.t_b",
        kRoot);
    ASSERT_TRUE(alt_sum.success()) << alt_sum.error();
    ASSERT_TRUE(alt_sum.hasResultSet());
    ASSERT_EQ(alt_sum.resultSet()->rowCount(), 1u);
    EXPECT_EQ(alt_sum.resultSet()->getValue(0, 0).toInt64(), 20);
}
