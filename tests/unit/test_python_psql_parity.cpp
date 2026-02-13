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

#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

#include <memory>
#include <string>

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::sblr::Executor;
using scratchbird::sblr::ExecutionResult;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::testing::TestDatabaseFile;

class PythonPSQLParityTest : public ::testing::Test
{
protected:
    std::unique_ptr<Database> db_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<TestDatabaseFile> db_file_;

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_python_psql_parity");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        executor_ = std::make_unique<Executor>(db_.get());
        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
    }

    void TearDown() override
    {
        compiler_.reset();
        executor_.reset();
        db_.reset();
        db_file_.reset();
    }

    ExecutionResult executeSQL(const std::string& sql)
    {
        auto result = compiler_->compile(sql);
        if (!result.success())
        {
            std::string message = "Compile error";
            if (!result.errors().empty())
            {
                message += ": " + result.errors().front();
            }
            return ExecutionResult(message);
        }
        return executor_->execute(result.bytecode());
    }

    scratchbird::core::TypedValue querySingleValue(const std::string& sql)
    {
        auto result = executeSQL(sql);
        EXPECT_TRUE(result.success()) << result.error();
        EXPECT_TRUE(result.hasResultSet());

        auto rs = result.resultSet();
        EXPECT_EQ(rs->rowCount(), 1);
        EXPECT_EQ(rs->columnCount(), 1);
        return rs->getValue(0, 0);
    }
};

TEST_F(PythonPSQLParityTest, DivIntegerOperator)
{
    auto value = querySingleValue("SELECT 9 DIV 2");
    EXPECT_EQ(value.toString(), "4");
}

TEST_F(PythonPSQLParityTest, DivByZeroFails)
{
    auto result = executeSQL("SELECT 1 DIV 0");
    EXPECT_FALSE(result.success());
}

TEST_F(PythonPSQLParityTest, StartingWithPredicate)
{
    auto value = querySingleValue("SELECT 'hello' STARTING WITH 'he'");
    EXPECT_EQ(value.toString(), "true");
}

TEST_F(PythonPSQLParityTest, StartingWithPredicateFalse)
{
    auto value = querySingleValue("SELECT 'hello' STARTING WITH 'lo'");
    EXPECT_EQ(value.toString(), "false");
}

TEST_F(PythonPSQLParityTest, ContainingPredicateCaseInsensitive)
{
    auto value = querySingleValue("SELECT 'AbC' CONTAINING 'b'");
    EXPECT_EQ(value.toString(), "true");
}

TEST_F(PythonPSQLParityTest, ContainingPredicateFalse)
{
    auto value = querySingleValue("SELECT 'abc' CONTAINING 'z'");
    EXPECT_EQ(value.toString(), "false");
}

TEST_F(PythonPSQLParityTest, ReplaceFunction)
{
    auto value = querySingleValue("SELECT REPLACE('bananas', 'na', 'ha')");
    EXPECT_EQ(value.toString(), "bahahas");
}

TEST_F(PythonPSQLParityTest, ReplaceEmptySearchNoChange)
{
    auto value = querySingleValue("SELECT REPLACE('abc', '', 'x')");
    EXPECT_EQ(value.toString(), "abc");
}

TEST_F(PythonPSQLParityTest, EndsWithFunction)
{
    auto value = querySingleValue("SELECT ENDS_WITH('scratchbird', 'bird')");
    EXPECT_EQ(value.toString(), "true");
}

TEST_F(PythonPSQLParityTest, EndsWithFunctionFalse)
{
    auto value = querySingleValue("SELECT ENDS_WITH('scratchbird', 'scratch')");
    EXPECT_EQ(value.toString(), "false");
}

TEST_F(PythonPSQLParityTest, ArrayPositionFound)
{
    auto value = querySingleValue("SELECT ARRAY_POSITION(ARRAY[10,20,30], 20)");
    EXPECT_EQ(value.toString(), "2");
}

TEST_F(PythonPSQLParityTest, ArrayPositionNotFound)
{
    auto value = querySingleValue("SELECT ARRAY_POSITION(ARRAY[10,20,30], 99)");
    EXPECT_TRUE(value.isNull());
}

TEST_F(PythonPSQLParityTest, ArraySliceBothBounds)
{
    auto value = querySingleValue("SELECT ARRAY[1,2,3,4][2:3]");
    EXPECT_EQ(value.toString(), "[2,3]");
}

TEST_F(PythonPSQLParityTest, ArraySliceLowerOnly)
{
    auto value = querySingleValue("SELECT ARRAY[1,2,3,4][3:]");
    EXPECT_EQ(value.toString(), "[3,4]");
}

TEST_F(PythonPSQLParityTest, ArraySliceUpperOnly)
{
    auto value = querySingleValue("SELECT ARRAY[1,2,3,4][:2]");
    EXPECT_EQ(value.toString(), "[1,2]");
}

TEST_F(PythonPSQLParityTest, JsonExistsTrue)
{
    auto value = querySingleValue("SELECT JSON_EXISTS('{\"a\":1}', '$.a')");
    EXPECT_EQ(value.toString(), "true");
}

TEST_F(PythonPSQLParityTest, JsonExistsFalse)
{
    auto value = querySingleValue("SELECT JSON_EXISTS('{\"a\":1}', '$.b')");
    EXPECT_EQ(value.toString(), "false");
}

TEST_F(PythonPSQLParityTest, JsonHasKeyTrue)
{
    auto value = querySingleValue("SELECT JSON_HAS_KEY('{\"a\":1}', 'a')");
    EXPECT_EQ(value.toString(), "true");
}

TEST_F(PythonPSQLParityTest, JsonHasKeyFalseOnNonObject)
{
    auto value = querySingleValue("SELECT JSON_HAS_KEY('[1,2,3]', 'a')");
    EXPECT_EQ(value.toString(), "false");
}

TEST_F(PythonPSQLParityTest, ToCharDateFormat)
{
    auto value = querySingleValue("SELECT TO_CHAR(CAST('2024-01-02' AS DATE), 'YYYY-MM-DD')");
    EXPECT_EQ(value.toString(), "2024-01-02");
}

TEST_F(PythonPSQLParityTest, ToCharTimestampFormat)
{
    auto value = querySingleValue(
        "SELECT TO_CHAR(CAST('2024-01-02 03:04:05' AS TIMESTAMP), "
        "'YYYY-MM-DD HH24:MI:SS')");
    EXPECT_EQ(value.toString(), "2024-01-02 03:04:05");
}

TEST_F(PythonPSQLParityTest, ToDateFromFormat)
{
    auto value = querySingleValue("SELECT TO_DATE('2024-01-02', 'YYYY-MM-DD')");
    EXPECT_EQ(value.toString(), "2024-01-02");
}

TEST_F(PythonPSQLParityTest, ToDateInvalidFormatReturnsNull)
{
    auto value = querySingleValue("SELECT TO_DATE('2024-01-02', 'YYYY/MM/DD')");
    EXPECT_TRUE(value.isNull());
}

TEST_F(PythonPSQLParityTest, ToTimestampFromFormat)
{
    auto value = querySingleValue(
        "SELECT TO_TIMESTAMP('2024-01-02 03:04:05', 'YYYY-MM-DD HH24:MI:SS')");
    EXPECT_EQ(value.toString(), "2024-01-02 03:04:05");
}

TEST_F(PythonPSQLParityTest, LeastGreatestNumeric)
{
    auto least = querySingleValue("SELECT LEAST(3, 1, 2)");
    EXPECT_EQ(least.toString(), "1");

    auto greatest = querySingleValue("SELECT GREATEST(3, 1, 2)");
    EXPECT_EQ(greatest.toString(), "3");
}

TEST_F(PythonPSQLParityTest, LeastGreatestWithNulls)
{
    auto least = querySingleValue("SELECT LEAST(NULL, 5, 2)");
    EXPECT_EQ(least.toString(), "2");

    auto greatest = querySingleValue("SELECT GREATEST(NULL, 5, 2)");
    EXPECT_EQ(greatest.toString(), "5");
}

TEST_F(PythonPSQLParityTest, LeastGreatestAllNull)
{
    auto value = querySingleValue("SELECT LEAST(NULL, NULL)");
    EXPECT_TRUE(value.isNull());
}

TEST_F(PythonPSQLParityTest, LeastGreatestStrings)
{
    auto value = querySingleValue("SELECT LEAST('b', 'a')");
    EXPECT_EQ(value.toString(), "a");
}
