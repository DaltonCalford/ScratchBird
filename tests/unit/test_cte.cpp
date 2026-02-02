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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>

using namespace scratchbird;
using namespace scratchbird::parser::v2;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

class CTETest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_cte_unit");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(db_file_->path(), 16384, &ctx), core::Status::OK)
            << ctx.message;

        db_ = std::make_unique<core::Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), core::Status::OK) << ctx.message;

        auto status = core::ProcArrayManager::initialize(db_.get(), 10, &ctx);
        ASSERT_EQ(status, core::Status::OK) << ctx.message;

        status = core::ProcArrayManager::registerBackend(&proc_id_, &ctx);
        ASSERT_EQ(status, core::Status::OK) << ctx.message;

        conn_ctx_ = std::make_unique<core::ConnectionContext>(db_.get(), proc_id_);
        status = conn_ctx_->initialize(&ctx);
        ASSERT_EQ(status, core::Status::OK) << ctx.message;

        core::CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx), core::Status::OK)
            << ctx.message;
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<QueryCompilerV2>(db_.get());
        compiler_->setCurrentSchema(schema_id_);

        executor_ = std::make_unique<Executor>(db_.get());
        executor_->setConnectionContext(conn_ctx_.get());
        executor_->setCurrentSchema(schema_id_);

        createTables();
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        conn_ctx_.reset();

        core::ErrorContext ctx;
        core::ProcArrayManager::unregisterBackend(proc_id_, &ctx);
        core::ProcArrayManager::shutdown(&ctx);

        db_.reset();
        db_file_.reset();
    }

    void expectParse(const std::string &sql, bool should_succeed = true)
    {
        Parser parser(sql);
        auto result = parser.parseStatement();
        if (should_succeed)
        {
            ASSERT_TRUE(result.success())
                << "Parse failed for: " << sql
                << " error: " << (result.errors().empty() ? "unknown" : result.errors()[0].message);
        }
        else
        {
            EXPECT_FALSE(result.success()) << "Parse should have failed for: " << sql;
        }
    }

    void expectCompile(const std::string &sql)
    {
        auto result = compiler_->compile(sql);
        ASSERT_TRUE(result.success())
            << "Compile failed for: " << sql << " error: " << formatErrors(result.errors());
    }

    ExecutionResult executeSQL(const std::string &sql)
    {
        auto result = compiler_->compile(sql);
        if (!result.success())
        {
            if (!result.errors().empty())
            {
                return ExecutionResult(result.errors().front());
            }
            return ExecutionResult("Compile error");
        }

        return executor_->execute(result.bytecode());
    }

    void testExecution(const std::string &setup_sql, const std::string &test_sql)
    {
        if (!setup_sql.empty())
        {
            auto setup_result = executeSQL(setup_sql);
            ASSERT_TRUE(setup_result.success()) << "Setup failed: " << setup_result.error();

            core::ErrorContext ctx;
            auto status = conn_ctx_->commit(&ctx);
            ASSERT_EQ(status, core::Status::OK) << "Setup commit failed: " << ctx.message;
        }

        auto exec_result = executeSQL(test_sql);
        ASSERT_TRUE(exec_result.success()) << "Execution failed: " << exec_result.error();
    }

    void createTables()
    {
        const std::vector<std::string> ddl = {
            "CREATE TABLE users (id INT, name TEXT, status TEXT, age INT)",
            "CREATE TABLE orders (user_id INT, amount DOUBLE)"
        };

        for (const auto &sql : ddl)
        {
            auto result = executeSQL(sql);
            ASSERT_TRUE(result.success()) << "Failed to create table: " << result.error();
        }
    }

private:
    static std::string formatErrors(const std::vector<std::string> &errors)
    {
        if (errors.empty())
        {
            return "unknown";
        }

        std::string out;
        for (const auto &err : errors)
        {
            if (!out.empty())
            {
                out.append("; ");
            }
            out.append(err);
        }
        return out;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<core::ConnectionContext> conn_ctx_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    core::ID schema_id_{};
    uint32_t proc_id_ = 0;
};

// ===== Parsing Tests =====

TEST_F(CTETest, ParseSimpleCTE)
{
    std::string sql = "WITH temp AS (SELECT * FROM users) SELECT * FROM temp";
    expectParse(sql, true);
}

TEST_F(CTETest, ParseMultipleCTEs)
{
    std::string sql = "WITH "
                      "  cte1 AS (SELECT id FROM users), "
                      "  cte2 AS (SELECT user_id FROM orders) "
                      "SELECT * FROM cte1 JOIN cte2 ON cte1.id = cte2.user_id";
    expectParse(sql, true);
}

TEST_F(CTETest, ParseCTEWithColumnAliases)
{
    std::string sql = "WITH summary(total, count) AS "
                      "  (SELECT SUM(amount), COUNT(*) FROM orders) "
                      "SELECT * FROM summary";
    expectParse(sql, true);
}

TEST_F(CTETest, ParseNestedCTE)
{
    std::string sql = "WITH outer_cte AS ( "
                      "  WITH inner_cte AS (SELECT id FROM users) "
                      "  SELECT * FROM inner_cte "
                      ") SELECT * FROM outer_cte";
    expectParse(sql, true);
}

TEST_F(CTETest, ParseCTEWithWhere)
{
    std::string sql = "WITH active_users AS "
                      "  (SELECT * FROM users WHERE status = 'active') "
                      "SELECT name FROM active_users WHERE age > 18";
    expectParse(sql, true);
}

TEST_F(CTETest, ParseCTEWithJoin)
{
    std::string sql = "WITH user_orders AS ( "
                      "  SELECT u.name, o.amount "
                      "  FROM users u JOIN orders o ON u.id = o.user_id "
                      ") SELECT * FROM user_orders";
    expectParse(sql, true);
}

TEST_F(CTETest, ParseCTEWithGroupBy)
{
    std::string sql = "WITH totals AS ( "
                      "  SELECT user_id, SUM(amount) as total "
                      "  FROM orders "
                      "  GROUP BY user_id "
                      ") SELECT * FROM totals WHERE total > 1000";
    expectParse(sql, true);
}

// ===== Bytecode Generation Tests =====

TEST_F(CTETest, BytecodeSimpleCTE)
{
    expectCompile("WITH temp AS (SELECT * FROM users) SELECT * FROM temp");
}

TEST_F(CTETest, BytecodeMultipleCTEs)
{
    expectCompile("WITH "
                  "  cte1 AS (SELECT id FROM users), "
                  "  cte2 AS (SELECT user_id FROM orders) "
                  "SELECT * FROM cte1");
}

TEST_F(CTETest, BytecodeCTEWithColumnAliases)
{
    expectCompile("WITH summary(total, count) AS "
                  "  (SELECT SUM(amount), COUNT(*) FROM orders) "
                  "SELECT * FROM summary");
}

// ===== Execution Tests =====

TEST_F(CTETest, DISABLED_ExecuteSimpleCTE)
{
    std::string setup = "CREATE TABLE test_users (id INT32, name VARCHAR(100))";
    std::string test = "WITH temp AS (SELECT * FROM test_users) SELECT * FROM temp";
    testExecution(setup, test);
}

TEST_F(CTETest, DISABLED_ExecuteMultipleCTEs)
{
    std::string setup = "CREATE TABLE test_data (id INT32, value INT32)";
    std::string test = "WITH "
                       "  cte1 AS (SELECT id FROM test_data WHERE value > 0), "
                       "  cte2 AS (SELECT id FROM test_data WHERE value < 100) "
                       "SELECT * FROM cte1";
    testExecution(setup, test);
}

// ===== Error Cases =====

TEST_F(CTETest, ErrorUndefinedCTE)
{
    std::string sql = "SELECT * FROM undefined_cte";
    expectParse(sql, true);
}
