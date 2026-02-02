/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>

#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/sblr/executor.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

class PSQLRuntimeIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_psql_runtime");
        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;
        auto sys_user = db_.catalog_manager()->getSystemUserId(&ctx);
        conn_->setCurrentUser(sys_user, true);
        ConnectionContext::setCurrent(conn_.get());
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
    }

    ExecutionResult executeSql(const std::string& sql)
    {
        parser::v2::Parser parser(sql);
        auto parse_result = parser.parseStatement();
        if (!parse_result.success())
        {
            ADD_FAILURE() << (parse_result.errors().empty()
                ? "Parse failed"
                : parse_result.errors().front().message);
            return ExecutionResult("Parse failed");
        }

        auto& pool = parser.stringPool();
        parser::v2::SemanticAnalyzerV2 analyzer(*db_.catalog_manager(), pool);
        auto sem_result = analyzer.analyze(parse_result.statement());
        if (!sem_result.success())
        {
            ADD_FAILURE() << (sem_result.errors().empty()
                ? "Semantic analysis failed"
                : sem_result.errors().front().message);
            return ExecutionResult("Semantic analysis failed");
        }

        parser::v2::BytecodeGeneratorV2 generator(pool);
        generator.setSourceSql(sql);
        auto bytecode = generator.generate(sem_result.statement());
        if (!bytecode.success())
        {
            ADD_FAILURE() << (bytecode.errors().empty()
                ? "Bytecode generation failed"
                : bytecode.errors().front());
            return ExecutionResult("Bytecode generation failed");
        }

        Executor executor(&db_);
        if (conn_)
        {
            executor.setConnectionContext(conn_.get());
        }
        return executor.execute(bytecode.bytecode());
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    Database db_;
    std::unique_ptr<ConnectionContext> conn_;
};

TEST_F(PSQLRuntimeIntegrationTest, ExecuteBlockVariableAssignment)
{
    auto result = executeSql(
        "EXECUTE BLOCK RETURNS (x INT) AS "
        "DECLARE VARIABLE i INT; "
        "BEGIN "
        "i = 4; "
        "x = i + 1; "
        "SUSPEND; "
        "END;"
    );
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toInt64(), 5);
}

TEST_F(PSQLRuntimeIntegrationTest, ExecuteBlockIfElseChoosesBranch)
{
    auto result = executeSql(
        "EXECUTE BLOCK RETURNS (x INT) AS "
        "BEGIN "
        "IF (1 = 1) THEN x = 10 "
        "ELSE x = 20; "
        "SUSPEND; "
        "END;"
    );
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toInt64(), 10);
}

TEST_F(PSQLRuntimeIntegrationTest, ExecuteBlockReturnStopsExecution)
{
    auto result = executeSql(
        "EXECUTE BLOCK RETURNS (x INT) AS "
        "BEGIN "
        "x = 1; "
        "SUSPEND; "
        "RETURN; "
        "x = 2; "
        "SUSPEND; "
        "END;"
    );
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toInt64(), 1);
}

TEST_F(PSQLRuntimeIntegrationTest, ExecuteBlockWhileLoopAccumulation)
{
    auto result = executeSql(
        "EXECUTE BLOCK RETURNS (x INT) AS "
        "DECLARE VARIABLE i INT; "
        "BEGIN "
        "i = 0; "
        "x = 0; "
        "WHILE (i < 3) DO "
        "BEGIN "
        "x = x + 2; "
        "i = i + 1; "
        "END; "
        "SUSPEND; "
        "END;"
    );
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toInt64(), 6);
}

TEST_F(PSQLRuntimeIntegrationTest, ExecuteBlockLoopLeave)
{
    auto result = executeSql(
        "EXECUTE BLOCK RETURNS (x INT) AS "
        "DECLARE VARIABLE i INT; "
        "BEGIN "
        "i = 0; "
        "LOOP "
        "i = i + 1; "
        "IF (i = 3) THEN LEAVE; "
        "END LOOP; "
        "x = i; "
        "SUSPEND; "
        "END;"
    );
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toInt64(), 3);
}

TEST_F(PSQLRuntimeIntegrationTest, ExecuteBlockExecuteStatementInto)
{
    auto result = executeSql(
        "EXECUTE BLOCK RETURNS (x INT) AS "
        "BEGIN "
        "EXECUTE STATEMENT 'SELECT 42' INTO x; "
        "SUSPEND; "
        "END;"
    );
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toInt64(), 42);
}

TEST_F(PSQLRuntimeIntegrationTest, ExecuteBlockForSelectAccumulation)
{
    ASSERT_TRUE(executeSql("CREATE TABLE t_for_select (val INT)").success());
    ASSERT_TRUE(executeSql("INSERT INTO t_for_select (val) VALUES (1)").success());
    ASSERT_TRUE(executeSql("INSERT INTO t_for_select (val) VALUES (2)").success());
    ASSERT_TRUE(executeSql("INSERT INTO t_for_select (val) VALUES (3)").success());

    auto result = executeSql(
        "EXECUTE BLOCK RETURNS (total INT) AS "
        "DECLARE VARIABLE v INT; "
        "BEGIN "
        "total = 0; "
        "FOR SELECT val FROM t_for_select INTO v DO "
        "BEGIN "
        "total = total + v; "
        "END; "
        "SUSPEND; "
        "END;"
    );
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toInt64(), 6);
}

TEST_F(PSQLRuntimeIntegrationTest, CreateProcedureAndExecute)
{
    ASSERT_TRUE(executeSql("CREATE TABLE psql_proc_test (id INT)").success());
    auto create_proc = executeSql(
        "CREATE PROCEDURE add_row(p_id INT) AS "
        "BEGIN "
        "INSERT INTO psql_proc_test (id) VALUES (p_id); "
        "END"
    );
    ASSERT_TRUE(create_proc.success()) << create_proc.error();

    auto exec_proc = executeSql("EXECUTE PROCEDURE add_row(5)");
    ASSERT_TRUE(exec_proc.success()) << exec_proc.error();

    auto result = executeSql("SELECT id FROM psql_proc_test");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toInt64(), 5);
}

TEST_F(PSQLRuntimeIntegrationTest, CreateFunctionAndSelect)
{
    auto create_func = executeSql(
        "CREATE FUNCTION add_one(a INT) RETURNS INT AS "
        "BEGIN "
        "RETURN a + 1; "
        "END"
    );
    ASSERT_TRUE(create_func.success()) << create_func.error();

    auto result = executeSql("SELECT add_one(4)");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toInt64(), 5);
}
