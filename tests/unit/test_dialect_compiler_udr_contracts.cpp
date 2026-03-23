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
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/dynamic_sql_bridge.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/udr/dialect_compiler_udr.h"
#include "test_helpers.h"

using scratchbird::core::CatalogManager;
using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::sblr::DialectCompilerRequest;
using scratchbird::sblr::DialectCompilerResponse;
using scratchbird::sblr::DynamicSqlCompileResponse;
using scratchbird::sblr::Executor;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::testing::TestDatabaseFile;

namespace {

class DialectCompilerUdrContractTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("test_dialect_compiler_udr_contracts");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.connect(conn_ctx_, &ctx), Status::OK) << ctx.message;

        auto* catalog = db_.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        CatalogManager::SchemaInfo public_schema;
        ASSERT_EQ(catalog->getSchema("public", public_schema, &ctx), Status::OK) << ctx.message;
        conn_ctx_->setCurrentSchemaId(public_schema.schema_id);
        conn_ctx_->set_current_schema("public");
        conn_ctx_->set_search_path({"public"});
        conn_ctx_->set_dialect_tag("scratchbird");

        auto system_user_id = catalog->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(conn_ctx_.get());

        executor_ = std::make_unique<Executor>(&db_);
        executor_->setConnectionContext(conn_ctx_.get());
    }

    void TearDown() override {
        ConnectionContext::setCurrent(nullptr);
        executor_.reset();
        conn_ctx_.reset();
        db_.close();
        db_file_.reset();
    }

    auto executeScalar(const std::vector<uint8_t>& bytecode) -> std::string {
        auto result = executor_->execute(bytecode);
        EXPECT_TRUE(result.success()) << result.error();
        EXPECT_TRUE(result.hasResultSet());
        EXPECT_NE(result.resultSet(), nullptr);
        EXPECT_EQ(result.resultSet()->rowCount(), 1u);
        EXPECT_EQ(result.resultSet()->columnCount(), 1u);
        if (!result.success() || !result.hasResultSet() || result.resultSet() == nullptr ||
            result.resultSet()->rowCount() != 1u || result.resultSet()->columnCount() != 1u) {
            return {};
        }
        return result.resultSet()->getValue(0, 0).toString();
    }

    auto executeStatement(const std::vector<uint8_t>& bytecode) -> scratchbird::sblr::ExecutionResult {
        return executor_->execute(bytecode);
    }

    auto compileNative(const std::string& sql) -> std::vector<uint8_t> {
        QueryCompilerV3 compiler(&db_);
        compiler.setCurrentSchema(conn_ctx_->getCurrentSchemaId());
        auto result = compiler.compile(sql);
        EXPECT_TRUE(result.success());
        if (!result.success()) {
            return {};
        }
        return result.bytecode();
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    Database db_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    std::unique_ptr<Executor> executor_;
};

TEST_F(DialectCompilerUdrContractTest, ScratchBirdProfileCompilesSqlToSblr) {
    DialectCompilerRequest request{};
    request.module_name = "scratchbird_native";
    request.session.profile_id = "scratchbird_v3";
    request.session.dialect_tag = "scratchbird";
    request.session.current_schema_id = conn_ctx_->getCurrentSchemaId();
    request.session.current_schema_name = conn_ctx_->current_schema();
    request.session.search_path = conn_ctx_->search_path();
    const std::string sql = "SELECT 1";
    request.payload.assign(sql.begin(), sql.end());

    DialectCompilerResponse response{};
    ErrorContext ctx;
    const auto status = scratchbird::sblr::compileDialectToSblr(&db_, request, response, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_TRUE(response.success);
    ASSERT_FALSE(response.bytecode.empty());
    EXPECT_EQ(response.profile_id, "scratchbird");
    EXPECT_EQ(executeScalar(response.bytecode), "1");
}

TEST_F(DialectCompilerUdrContractTest, DynamicSqlBridgeUsesScratchBirdCompilerUdrForNativeSessions) {
    DynamicSqlCompileResponse response{};
    ErrorContext ctx;
    const auto status = scratchbird::sblr::compileEngineDynamicSql(&db_,
                                                                   conn_ctx_.get(),
                                                                   "SELECT 7",
                                                                   response,
                                                                   &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_TRUE(response.success);
    ASSERT_FALSE(response.bytecode.empty());
    EXPECT_EQ(response.profile_id, "scratchbird");
    EXPECT_EQ(executeScalar(response.bytecode), "7");
}

TEST_F(DialectCompilerUdrContractTest, DynamicSqlBridgePreservesQualifiedShowTableNames) {
    ASSERT_TRUE(executeStatement(compileNative("CREATE SCHEMA users.public")).success());
    ASSERT_TRUE(
        executeStatement(compileNative(
            "CREATE TABLE users.public.bridge_show_table (id INTEGER PRIMARY KEY, payload VARCHAR(16))"))
            .success());

    DynamicSqlCompileResponse response{};
    ErrorContext ctx;
    const auto status = scratchbird::sblr::compileEngineDynamicSql(
        &db_,
        conn_ctx_.get(),
        "SHOW TABLE users.public.bridge_show_table",
        response,
        &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_TRUE(response.success);

    auto result = executeStatement(response.bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_GE(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "bridge_show_table");
}

TEST_F(DialectCompilerUdrContractTest, DynamicSqlBridgeAppliesSearchPathLists) {
    ASSERT_TRUE(executeStatement(compileNative("CREATE SCHEMA v3inet")).success());
    ASSERT_TRUE(executeStatement(compileNative("CREATE SCHEMA users.public")).success());

    DynamicSqlCompileResponse set_response{};
    ErrorContext ctx;
    auto status = scratchbird::sblr::compileEngineDynamicSql(
        &db_,
        conn_ctx_.get(),
        "SET SEARCH_PATH TO v3inet, users.public",
        set_response,
        &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_TRUE(set_response.success);
    ASSERT_TRUE(executeStatement(set_response.bytecode).success());

    const auto& paths = conn_ctx_->search_path();
    ASSERT_EQ(paths.size(), 2u);
    EXPECT_EQ(paths[0], "v3inet");
    EXPECT_EQ(paths[1], "users.public");
}

} // namespace
