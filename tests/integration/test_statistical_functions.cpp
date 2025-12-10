/**
 * @file test_statistical_functions.cpp
 * @brief Integration tests for statistical aggregate functions
 *
 * Tests the 7 statistical aggregate functions:
 * - STDDEV / STDDEV_SAMP
 * - STDDEV_POP
 * - VARIANCE / VAR_SAMP
 * - VAR_POP
 * - CORR
 * - COVAR_POP
 *
 * November 14, 2025
 */

#include <gtest/gtest.h>
#include <scratchbird/core/database.h>
#include <scratchbird/core/catalog_manager.h>


#include <scratchbird/sblr/executor.h>
#include "scratchbird/sblr/query_compiler_v2.h"
#include <cmath>

using namespace scratchbird;

class StatisticalFunctionsTest : public ::testing::Test
{
protected:
    std::shared_ptr<core::Database> db;
    std::string test_db_path = "/tmp/test_stats.db";

    void SetUp() override
    {
        // Clean up any existing database
        std::remove(test_db_path.c_str());

        // Create new database
        core::ErrorContext ctx;
        auto status = core::Database::create(test_db_path, 16384, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to create database: " << ctx.message;

        db = core::Database::open(test_db_path, &ctx);
        ASSERT_NE(db, nullptr) << "Failed to open database: " << ctx.message;
        ASSERT_EQ(status, core::Status::OK);
    }

    void TearDown() override
    {
        db.reset();
        std::remove(test_db_path.c_str());
    }

    bool executeSQL(const std::string& sql, std::string* error_msg = nullptr)
    {
        parser::Lexer lexer(sql);
        parser::ASTArena arena;
        parser::Parser parser(lexer, arena);

        auto stmt = parser.parseStatement();
        if (!stmt)
        {
            if (error_msg) *error_msg = "Parse failed";
            return false;
        }

        sblr::BytecodeGenerator gen;
        auto bytecode = gen.generate(stmt);

        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode);

        if (!exec_result.isSuccess())
        {
            if (error_msg) *error_msg = "Execution failed";
            return false;
        }

        return true;
    }

    std::unique_ptr<sblr::ResultSet> executeQuery(const std::string& sql, std::string* error_msg = nullptr)
    {
        parser::Lexer lexer(sql);
        parser::ASTArena arena;
        parser::Parser parser(lexer, arena);

        auto stmt = parser.parseStatement();
        if (!stmt)
        {
            if (error_msg) *error_msg = "Parse failed";
            return nullptr;
        }

        sblr::BytecodeGenerator gen;
        auto bytecode = gen.generate(stmt);

        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode);

        if (!exec_result.isSuccess())
        {
            if (error_msg) *error_msg = "Execution failed";
            return nullptr;
        }

        return executor.getResultSet();
    }
};

TEST_F(StatisticalFunctionsTest, STDDEV_SAMP_BasicTest)
{
    // Create test table
    ASSERT_TRUE(executeSQL("CREATE TABLE numbers (value FLOAT64)"));

    // Insert test data: [1, 2, 3, 4, 5]
    // Sample std dev = sqrt((10/4)) = sqrt(2.5) ≈ 1.5811
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (1.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (2.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (3.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (4.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (5.0)"));

    std::string error_msg;
    auto result = executeQuery("SELECT STDDEV(value) FROM numbers", &error_msg);
    ASSERT_NE(result, nullptr) << "Query failed: " << error_msg;
    ASSERT_EQ(result->rowCount(), 1);

    auto row = result->getRow(0);
    ASSERT_EQ(row.size(), 1);

    double stddev = row[0].toDouble();
    EXPECT_NEAR(stddev, 1.5811, 0.001);
}

TEST_F(StatisticalFunctionsTest, STDDEV_POP_BasicTest)
{
    // Create test table
    ASSERT_TRUE(executeSQL("CREATE TABLE numbers (value FLOAT64)"));

    // Insert test data: [1, 2, 3, 4, 5]
    // Population std dev = sqrt((10/5)) = sqrt(2.0) ≈ 1.4142
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (1.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (2.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (3.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (4.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (5.0)"));

    std::string error_msg;
    auto result = executeQuery("SELECT STDDEV_POP(value) FROM numbers", &error_msg);
    ASSERT_NE(result, nullptr) << "Query failed: " << error_msg;
    ASSERT_EQ(result->rowCount(), 1);

    auto row = result->getRow(0);
    ASSERT_EQ(row.size(), 1);

    double stddev_pop = row[0].toDouble();
    EXPECT_NEAR(stddev_pop, 1.4142, 0.001);
}

TEST_F(StatisticalFunctionsTest, VAR_SAMP_BasicTest)
{
    // Create test table
    ASSERT_TRUE(executeSQL("CREATE TABLE numbers (value FLOAT64)"));

    // Insert test data: [1, 2, 3, 4, 5]
    // Sample variance = 10/4 = 2.5
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (1.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (2.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (3.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (4.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (5.0)"));

    std::string error_msg;
    auto result = executeQuery("SELECT VARIANCE(value) FROM numbers", &error_msg);
    ASSERT_NE(result, nullptr) << "Query failed: " << error_msg;
    ASSERT_EQ(result->rowCount(), 1);

    auto row = result->getRow(0);
    ASSERT_EQ(row.size(), 1);

    double var_samp = row[0].toDouble();
    EXPECT_NEAR(var_samp, 2.5, 0.001);
}

TEST_F(StatisticalFunctionsTest, VAR_POP_BasicTest)
{
    // Create test table
    ASSERT_TRUE(executeSQL("CREATE TABLE numbers (value FLOAT64)"));

    // Insert test data: [1, 2, 3, 4, 5]
    // Population variance = 10/5 = 2.0
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (1.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (2.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (3.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (4.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO numbers VALUES (5.0)"));

    std::string error_msg;
    auto result = executeQuery("SELECT VAR_POP(value) FROM numbers", &error_msg);
    ASSERT_NE(result, nullptr) << "Query failed: " << error_msg;
    ASSERT_EQ(result->rowCount(), 1);

    auto row = result->getRow(0);
    ASSERT_EQ(row.size(), 1);

    double var_pop = row[0].toDouble();
    EXPECT_NEAR(var_pop, 2.0, 0.001);
}

TEST_F(StatisticalFunctionsTest, CORR_BasicTest)
{
    // Create test table
    ASSERT_TRUE(executeSQL("CREATE TABLE data (x FLOAT64, y FLOAT64)"));

    // Insert perfectly correlated data: y = 2x
    // Correlation coefficient should be 1.0
    ASSERT_TRUE(executeSQL("INSERT INTO data VALUES (1.0, 2.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO data VALUES (2.0, 4.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO data VALUES (3.0, 6.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO data VALUES (4.0, 8.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO data VALUES (5.0, 10.0)"));

    std::string error_msg;
    auto result = executeQuery("SELECT CORR(y, x) FROM data", &error_msg);
    ASSERT_NE(result, nullptr) << "Query failed: " << error_msg;
    ASSERT_EQ(result->rowCount(), 1);

    auto row = result->getRow(0);
    ASSERT_EQ(row.size(), 1);

    double corr = row[0].toDouble();
    EXPECT_NEAR(corr, 1.0, 0.001);
}

TEST_F(StatisticalFunctionsTest, COVAR_POP_BasicTest)
{
    // Create test table
    ASSERT_TRUE(executeSQL("CREATE TABLE data (x FLOAT64, y FLOAT64)"));

    // Insert test data
    ASSERT_TRUE(executeSQL("INSERT INTO data VALUES (1.0, 2.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO data VALUES (2.0, 4.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO data VALUES (3.0, 6.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO data VALUES (4.0, 8.0)"));
    ASSERT_TRUE(executeSQL("INSERT INTO data VALUES (5.0, 10.0)"));

    std::string error_msg;
    auto result = executeQuery("SELECT COVAR_POP(y, x) FROM data", &error_msg);
    ASSERT_NE(result, nullptr) << "Query failed: " << error_msg;
    ASSERT_EQ(result->rowCount(), 1);

    auto row = result->getRow(0);
    ASSERT_EQ(row.size(), 1);

    // For y = 2x, covariance should be 2 * variance(x)
    // variance(x) = 2.0 (population), so covar = 4.0
    double covar_pop = row[0].toDouble();
    EXPECT_NEAR(covar_pop, 4.0, 0.001);
}

