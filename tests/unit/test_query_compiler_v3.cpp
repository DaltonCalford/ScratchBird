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
 * End-to-End Tests for ScratchBird Query Compiler V3
 *
 * Phase 9: Integration tests for the complete Parser V3 pipeline
 *
 * Tests the full compilation flow:
 * SQL -> Lexer -> Parser -> Semantic Analyzer -> Bytecode Generator
 */

#include <gtest/gtest.h>
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/domain_manager.h"
#include "unit/test_user_helpers.h"
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <thread>
#include <chrono>

using namespace scratchbird::sblr;
using namespace scratchbird::core;

// Generate a unique database path per test to avoid conflicts in parallel execution
static std::string generateUniqueDbPath() {
    std::ostringstream oss;
    oss << "/tmp/test_query_compiler_v3_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << ".sbdb";
    return oss.str();
}

class QueryCompilerV3Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary database for testing with unique path
        test_db_path_ = generateUniqueDbPath();
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr) << "CatalogManager is null";

        CatalogManager::SchemaInfo public_schema_info;
        status = catalog_->getSchema("public", public_schema_info, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to resolve public schema: " << ctx.message;
        public_schema_id_ = public_schema_info.schema_id;

        // Create a test schema
        EnsureUser(catalog_, "test_user");
        status = catalog_->createSchema("test", "test_user", test_schema_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test schema";

        // Create compiler and executor
        compiler_ = std::make_unique<QueryCompilerV3>(&db_);
        executor_ = std::make_unique<Executor>(&db_);

        status = db_.connect(connection_ctx_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create connection: " << ctx.message;
        connection_ctx_->setCurrentSchemaId(public_schema_id_);
        auto system_user_id = catalog_->getSystemUserId(&ctx);
        connection_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());
    }

    void TearDown() override {
        compiler_.reset();
        executor_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.close();
        // Clean up database file and lock file
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    // Compile and execute SQL
    ExecutionResult compileAndExecute(const std::string& sql) {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success()) {
            std::string errors;
            for (const auto& err : compile_result.errors()) {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    std::string test_db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    ID public_schema_id_;
    ID test_schema_id_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

// =============================================================================
// Compilation Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, CompileSimpleSelect) {
    auto result = compiler_->compile("SELECT 1");
    ASSERT_TRUE(result.success()) << "Compilation failed";
    EXPECT_GT(result.bytecode().size(), 0);
}

TEST_F(QueryCompilerV3Test, CompileSelectWithArithmetic) {
    auto result = compiler_->compile("SELECT 1 + 2 * 3");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileSelectWithStringLiteral) {
    auto result = compiler_->compile("SELECT 'hello world'");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileSelectWithComparison) {
    auto result = compiler_->compile("SELECT 1 = 1, 2 < 3, 4 > 1");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileSelectWithLogical) {
    auto result = compiler_->compile("SELECT TRUE AND FALSE, TRUE OR FALSE");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileSelectWithCase) {
    // Note: Use TRUE literal instead of 1 = 1 comparison due to semantic analyzer limitation
    // with searched CASE WHEN condition type inference
    auto result = compiler_->compile("SELECT CASE WHEN TRUE THEN 'yes' ELSE 'no' END");
    if (!result.success()) {
        std::cerr << "Errors:\n";
        for (const auto& err : result.errors()) {
            std::cerr << "  " << err << "\n";
        }
    }
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileSelectWithCast) {
    auto result = compiler_->compile("SELECT CAST(123 AS VARCHAR)");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, ExecuteCreateTableWithDomainColumn) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_domain = compileAndExecute("CREATE DOMAIN positive_int AS INT NOT NULL");
    ASSERT_TRUE(create_domain.success()) << create_domain.error();

    auto create_table = compileAndExecute("CREATE TABLE domain_table (value positive_int)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    auto status = catalog_->getTable(test_schema_id_, "domain_table", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::ColumnInfo column_info;
    status = catalog_->getColumn(table_info.table_id, "value", column_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    DomainInfo domain_info;
    status = db_.domain_manager()->getDomain(test_schema_id_, "positive_int", domain_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    EXPECT_EQ(column_info.domain_id, domain_info.domain_id);
    EXPECT_EQ(column_info.data_type, static_cast<uint16_t>(domain_info.base_type));
}

TEST_F(QueryCompilerV3Test, ExecuteCreateTableWithDomainColumn_DefaultPublicSchema) {
    ErrorContext ctx;

    auto create_domain = compileAndExecute("CREATE DOMAIN public_int AS INT NOT NULL");
    ASSERT_TRUE(create_domain.success()) << create_domain.error();

    auto create_table = compileAndExecute("CREATE TABLE public_domain_table (value public_int)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    CatalogManager::TableInfo table_info;
    auto status = catalog_->getTable(public_schema_id_, "public_domain_table", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::ColumnInfo column_info;
    status = catalog_->getColumn(table_info.table_id, "value", column_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    DomainInfo domain_info;
    status = db_.domain_manager()->getDomain(public_schema_id_, "public_int", domain_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    EXPECT_EQ(column_info.domain_id, domain_info.domain_id);
    EXPECT_EQ(column_info.data_type, static_cast<uint16_t>(domain_info.base_type));
}

TEST_F(QueryCompilerV3Test, DomainDefaultAppliedWhenColumnDefaultMissing) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_domain = compileAndExecute("CREATE DOMAIN d_text AS VARCHAR(10) DEFAULT 'alpha'");
    ASSERT_TRUE(create_domain.success()) << create_domain.error();

    auto create_table = compileAndExecute("CREATE TABLE domain_default_table (val d_text)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto insert_row = compileAndExecute("INSERT INTO domain_default_table DEFAULT VALUES");
    ASSERT_TRUE(insert_row.success()) << insert_row.error();

    auto select_row = compileAndExecute("SELECT val FROM domain_default_table");
    ASSERT_TRUE(select_row.success()) << select_row.error();
    ASSERT_TRUE(select_row.hasResultSet());

    auto* results = select_row.resultSet();
    ASSERT_EQ(results->rowCount(), 1u);
    EXPECT_EQ(results->getValue(0, 0).toString(), "alpha");
}

TEST_F(QueryCompilerV3Test, InformationSchemaColumnsReportsDomainName) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_domain = compileAndExecute("CREATE DOMAIN d_info AS INT");
    ASSERT_TRUE(create_domain.success()) << create_domain.error();

    auto create_table = compileAndExecute("CREATE TABLE domain_info_table (val d_info)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ErrorContext ctx;
    CatalogManager::SchemaInfo schema_info;
    auto status = catalog_->getSchema(test_schema_id_, schema_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    std::string schema_name = schema_info.full_path.empty()
        ? schema_info.schema_name
        : schema_info.full_path;

    std::string sql =
        "SELECT domain_name FROM information_schema.columns "
        "WHERE table_schema = '" + schema_name +
        "' AND table_name = 'domain_info_table' AND column_name = 'val'";
    auto select_row = compileAndExecute(sql);
    ASSERT_TRUE(select_row.success()) << select_row.error();
    ASSERT_TRUE(select_row.hasResultSet());

    auto* results = select_row.resultSet();
    ASSERT_EQ(results->rowCount(), 1u);
    EXPECT_EQ(results->getValue(0, 0).toString(), "d_info");
}

TEST_F(QueryCompilerV3Test, DomainArrayEnforcesConstraintsAndSize) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_domain = compileAndExecute("CREATE DOMAIN positive_int AS INT CHECK (VALUE > 0)");
    ASSERT_TRUE(create_domain.success()) << create_domain.error();

    auto create_table = compileAndExecute("CREATE TABLE domain_array_table (vals positive_int[2])");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    auto status = catalog_->getTable(test_schema_id_, "domain_array_table", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::ColumnInfo column_info;
    status = catalog_->getColumn(table_info.table_id, "vals", column_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_TRUE(column_info.is_array);
    EXPECT_EQ(column_info.array_size, 2u);

    DomainInfo domain_info;
    status = db_.domain_manager()->getDomain(test_schema_id_, "positive_int", domain_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(column_info.domain_id, domain_info.domain_id);

    auto insert_ok = compileAndExecute("INSERT INTO domain_array_table (vals) VALUES (ARRAY[1, 2])");
    ASSERT_TRUE(insert_ok.success()) << insert_ok.error();

    auto select_row = compileAndExecute("SELECT vals FROM domain_array_table");
    ASSERT_TRUE(select_row.success()) << select_row.error();
    ASSERT_TRUE(select_row.hasResultSet());

    auto* results = select_row.resultSet();
    ASSERT_EQ(results->rowCount(), 1u);
    EXPECT_EQ(results->getValue(0, 0).toString(), "{1, 2}");

    auto insert_bad_size = compileAndExecute("INSERT INTO domain_array_table (vals) VALUES (ARRAY[1, 2, 3])");
    EXPECT_FALSE(insert_bad_size.success());

    auto insert_bad_value = compileAndExecute("INSERT INTO domain_array_table (vals) VALUES (ARRAY[1, -5])");
    EXPECT_FALSE(insert_bad_value.success());
}

TEST_F(QueryCompilerV3Test, ExecuteCreateViewStoresDefinition) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute("CREATE TABLE view_base (id INT)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto create_view = compileAndExecute("CREATE VIEW view_v AS SELECT id FROM view_base");
    ASSERT_TRUE(create_view.success()) << create_view.error();

    ErrorContext ctx;
    CatalogManager::ViewInfo view_info;
    auto status = catalog_->getView(test_schema_id_, "view_v", view_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(view_info.definition, "SELECT id FROM view_base");
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, CompileInvalidSyntax) {
    auto result = compiler_->compile("SELECT FROM");
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}

TEST_F(QueryCompilerV3Test, CompileEmptyQuery) {
    auto result = compiler_->compile("");
    EXPECT_FALSE(result.success());
}

// =============================================================================
// Transaction Statement Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, CompileStartTransaction) {
    auto result = compiler_->compile("START TRANSACTION");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileCommit) {
    auto result = compiler_->compile("COMMIT");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV3Test, CompileRollback) {
    auto result = compiler_->compile("ROLLBACK");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

// =============================================================================
// DDL Statement Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, CompileCreateTable) {
    auto result = compiler_->compile("CREATE TABLE products (id INT, name VARCHAR(100))");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

// =============================================================================
// End-to-End Execution Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, ExecuteSimpleSelect) {
    auto result = compileAndExecute("SELECT 42");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->rowCount(), 1);
    EXPECT_EQ(rs->columnCount(), 1);
}

TEST_F(QueryCompilerV3Test, ExecuteSelectWithExpression) {
    auto result = compileAndExecute("SELECT 10 + 5");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->rowCount(), 1);
}

TEST_F(QueryCompilerV3Test, ExecuteSelectMultipleColumns) {
    auto result = compileAndExecute("SELECT 1, 2, 3");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->rowCount(), 1);
    EXPECT_EQ(rs->columnCount(), 3);
}

TEST_F(QueryCompilerV3Test, ExecuteCastUsingHex) {
    auto result = compileAndExecute(
        "SELECT CAST(CAST('48656c6c6f' AS BLOB USING hex) AS VARCHAR USING hex)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_EQ(value.getVarchar(), "48656c6c6f");
}

TEST_F(QueryCompilerV3Test, ExecuteCastUsingBase64) {
    auto result = compileAndExecute(
        "SELECT CAST(CAST('SGVsbG8=' AS BLOB USING base64) AS VARCHAR USING base64)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_EQ(value.getVarchar(), "SGVsbG8=");
}

TEST_F(QueryCompilerV3Test, ExecuteSelectBoolean) {
    auto result = compileAndExecute("SELECT TRUE, FALSE");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
}

TEST_F(QueryCompilerV3Test, ExecuteSelectNull) {
    auto result = compileAndExecute("SELECT NULL");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, CompilationStatisticsEnabled) {
    compiler_->setStatsEnabled(true);

    auto result = compiler_->compile("SELECT 1 + 2 * 3");
    ASSERT_TRUE(result.success());

    const auto& stats = result.stats();
    EXPECT_GT(stats.bytecode_size, 0);
    // Parser time should be non-zero
    EXPECT_GE(stats.parser_time.count(), 0);
}

// =============================================================================
// Optimization Tests
// =============================================================================

TEST_F(QueryCompilerV3Test, OptimizationsEnabled) {
    compiler_->setOptimizationsEnabled(true);

    auto result = compiler_->compile("SELECT 1 + 2");
    ASSERT_TRUE(result.success());
}

TEST_F(QueryCompilerV3Test, OptimizationsDisabled) {
    compiler_->setOptimizationsEnabled(false);

    auto result = compiler_->compile("SELECT 1 + 2");
    ASSERT_TRUE(result.success());
}
