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

#include <memory>
#include <string>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

class V3DerivedTableTest : public ::testing::Test
{
protected:
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    core::ID schema_id_;

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_v3_derived_table");

        ErrorContext ctx;
        auto status = Database::create(db_file_->path(), 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        status = db_->open(db_file_->path(), &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }

        CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx), Status::OK)
            << "Failed to get PUBLIC schema: " << ctx.message;
        schema_id_ = schema.schema_id;

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK) << ctx.message;
        core::ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user, true);
        conn_ctx_->setCurrentSchemaId(schema_id_);

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<Executor>(db_.get());
        executor_->setConnectionContext(conn_ctx_.get());
        executor_->setCurrentSchema(schema_id_);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        executor_.reset();
        compiler_.reset();
        db_.reset();
        db_file_.reset();
    }

    ExecutionResult executeSQL(const std::string& sql)
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
};

TEST_F(V3DerivedTableTest, SelectFromDerivedTable)
{
    ASSERT_TRUE(executeSQL("CREATE TABLE dt_source (id INT PRIMARY KEY, val INT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO dt_source VALUES (1, 10)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO dt_source VALUES (2, 20)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO dt_source VALUES (3, 30)").success());

    auto result = executeSQL(
        "SELECT d.id, d.val "
        "FROM (SELECT id, val FROM dt_source WHERE id >= 2) AS d "
        "ORDER BY d.id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 2u);
    ASSERT_EQ(result.resultSet()->columnCount(), 2u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(1, 0).toString(), "3");
}

TEST_F(V3DerivedTableTest, JoinWithDerivedTableRightSide)
{
    ASSERT_TRUE(executeSQL("CREATE TABLE dt_join_source (id INT PRIMARY KEY, note VARCHAR)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO dt_join_source VALUES (1, 'a')").success());
    ASSERT_TRUE(executeSQL("INSERT INTO dt_join_source VALUES (2, 'b')").success());

    auto result = executeSQL(
        "SELECT s.id, t.note "
        "FROM dt_join_source AS s "
        "JOIN (SELECT id, note FROM dt_join_source WHERE id = 2) AS t "
        "ON s.id = t.id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    ASSERT_EQ(result.resultSet()->columnCount(), 2u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "b");
}
