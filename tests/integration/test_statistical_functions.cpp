/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

#include <cmath>
#include <memory>
#include <string>

using namespace scratchbird;
using scratchbird::testing::TestDatabaseFile;

class StatisticalFunctionsTest : public ::testing::Test
{
protected:
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<sblr::Executor> executor_;
    std::unique_ptr<sblr::QueryCompilerV3> compiler_;
    std::unique_ptr<TestDatabaseFile> db_file_;
    core::ID schema_id_;

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_stats");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(db_file_->path(), 16384, &ctx), core::Status::OK)
            << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<core::Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), core::Status::OK)
            << "Failed to open database: " << ctx.message;

        core::CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx), core::Status::OK)
            << "Failed to get PUBLIC schema: " << ctx.message;
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<sblr::QueryCompilerV3>(db_.get());
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<sblr::Executor>(db_.get());
        executor_->setCurrentSchema(schema_id_);
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        db_.reset();
        db_file_.reset();
    }

    bool executeSQL(const std::string& sql, std::string* error_msg = nullptr)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            if (error_msg && !compile_result.errors().empty())
            {
                *error_msg = compile_result.errors().front();
            }
            return false;
        }

        auto exec_result = executor_->execute(compile_result.bytecode());
        if (!exec_result.success())
        {
            if (error_msg)
            {
                *error_msg = exec_result.error();
            }
            return false;
        }

        return true;
    }

    sblr::ExecutionResult executeQuery(const std::string& sql, std::string* error_msg = nullptr)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            if (error_msg && !compile_result.errors().empty())
            {
                *error_msg = compile_result.errors().front();
            }
            return sblr::ExecutionResult("Compile failed");
        }

        auto exec_result = executor_->execute(compile_result.bytecode());
        if (!exec_result.success() && error_msg)
        {
            *error_msg = exec_result.error();
        }

        return exec_result;
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
    ASSERT_TRUE(result.success()) << "Query failed: " << error_msg;
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);

    double stddev = rs->getValue(0, 0).toDouble();
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
    ASSERT_TRUE(result.success()) << "Query failed: " << error_msg;
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);

    double stddev_pop = rs->getValue(0, 0).toDouble();
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
    ASSERT_TRUE(result.success()) << "Query failed: " << error_msg;
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);

    double var_samp = rs->getValue(0, 0).toDouble();
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
    ASSERT_TRUE(result.success()) << "Query failed: " << error_msg;
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);

    double var_pop = rs->getValue(0, 0).toDouble();
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
    ASSERT_TRUE(result.success()) << "Query failed: " << error_msg;
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);

    double corr = rs->getValue(0, 0).toDouble();
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
    ASSERT_TRUE(result.success()) << "Query failed: " << error_msg;
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);

    // For y = 2x, covariance should be 2 * variance(x)
    // variance(x) = 2.0 (population), so covar = 4.0
    double covar_pop = rs->getValue(0, 0).toDouble();
    EXPECT_NEAR(covar_pop, 4.0, 0.001);
}
