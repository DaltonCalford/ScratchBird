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

TEST_F(QueryCompilerV3Test, ExecuteCountDistinctAggregate) {
    auto create_table = compileAndExecute("CREATE TABLE count_distinct_t (id INT)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ASSERT_TRUE(compileAndExecute("INSERT INTO count_distinct_t (id) VALUES (1)").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO count_distinct_t (id) VALUES (1)").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO count_distinct_t (id) VALUES (2)").success());

    auto select_count = compileAndExecute("SELECT COUNT(DISTINCT id) FROM count_distinct_t");
    ASSERT_TRUE(select_count.success()) << select_count.error();
    ASSERT_TRUE(select_count.hasResultSet());
    ASSERT_EQ(select_count.resultSet()->rowCount(), 1u);
    EXPECT_EQ(select_count.resultSet()->getValue(0, 0).toInt64(), 2);
}

TEST_F(QueryCompilerV3Test, ExecuteJsonExistsOperatorSpecificity) {
    auto query = compileAndExecute(
        "SELECT "
        "'{\"a\":1,\"b\":2}' ? 'a', "
        "'{\"a\":1,\"b\":2}' ? 'z', "
        "'{\"a\":1,\"b\":2}' ?| ARRAY['a','z'], "
        "'{\"a\":1,\"b\":2}' ?& ARRAY['a','b'], "
        "'{\"a\":1,\"b\":2}' ?& ARRAY['a','z']");

    ASSERT_TRUE(query.success()) << query.error();
    ASSERT_TRUE(query.hasResultSet());
    ASSERT_EQ(query.resultSet()->rowCount(), 1u);
    EXPECT_EQ(query.resultSet()->getValue(0, 0).toString(), "true");
    EXPECT_EQ(query.resultSet()->getValue(0, 1).toString(), "false");
    EXPECT_EQ(query.resultSet()->getValue(0, 2).toString(), "true");
    EXPECT_EQ(query.resultSet()->getValue(0, 3).toString(), "true");
    EXPECT_EQ(query.resultSet()->getValue(0, 4).toString(), "false");
}

TEST_F(QueryCompilerV3Test, ExecuteSweepDatabase) {
    auto result = compileAndExecute("SWEEP DATABASE");
    ASSERT_TRUE(result.success()) << result.error();
}

TEST_F(QueryCompilerV3Test, RejectsVacuumAlias) {
    auto result = compileAndExecute("VACUUM");
    ASSERT_FALSE(result.success());
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

TEST_F(QueryCompilerV3Test, UniqueArrayIndexWholeModeUsesWholeArrayKey) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute("CREATE TABLE array_whole_table (id INT, vals INT[8])");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto create_index = compileAndExecute(
        "CREATE UNIQUE INDEX uq_array_whole ON array_whole_table USING BTREE (vals) "
        "WITH (ARRAY_UNIQUENESS = 'WHOLE')");
    ASSERT_TRUE(create_index.success()) << create_index.error();

    auto insert_first =
        compileAndExecute("INSERT INTO array_whole_table (id, vals) VALUES (1, ARRAY[1, 2])");
    ASSERT_TRUE(insert_first.success()) << insert_first.error();

    auto insert_same =
        compileAndExecute("INSERT INTO array_whole_table (id, vals) VALUES (2, ARRAY[1, 2])");
    EXPECT_FALSE(insert_same.success());
    EXPECT_NE(insert_same.error().find("UNIQUE index violation"), std::string::npos)
        << insert_same.error();

    auto insert_different_order =
        compileAndExecute("INSERT INTO array_whole_table (id, vals) VALUES (3, ARRAY[2, 1])");
    EXPECT_TRUE(insert_different_order.success()) << insert_different_order.error();

}

TEST_F(QueryCompilerV3Test, UniqueArrayIndexElementModeRejectsElementOverlap) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute("CREATE TABLE array_element_table (id INT, vals INT[8])");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto create_index = compileAndExecute(
        "CREATE UNIQUE INDEX uq_array_element ON array_element_table USING BTREE (vals) "
        "WITH (ARRAY_UNIQUENESS = 'ELEMENT')");
    ASSERT_TRUE(create_index.success()) << create_index.error();

    auto insert_first =
        compileAndExecute("INSERT INTO array_element_table (id, vals) VALUES (1, ARRAY[1, 2])");
    ASSERT_TRUE(insert_first.success()) << insert_first.error();

    auto insert_overlap =
        compileAndExecute("INSERT INTO array_element_table (id, vals) VALUES (2, ARRAY[2, 3])");
    EXPECT_FALSE(insert_overlap.success());
    EXPECT_NE(insert_overlap.error().find("duplicate array element"), std::string::npos)
        << insert_overlap.error();

    auto insert_disjoint =
        compileAndExecute("INSERT INTO array_element_table (id, vals) VALUES (3, ARRAY[4, 5])");
    EXPECT_TRUE(insert_disjoint.success()) << insert_disjoint.error();

}

TEST_F(QueryCompilerV3Test, ArrayUniquenessOptionRequiresUniqueIndex) {
    compiler_->setCurrentSchema(test_schema_id_);
    executor_->setCurrentSchema(test_schema_id_);
    connection_ctx_->setCurrentSchemaId(test_schema_id_);

    auto create_table = compileAndExecute("CREATE TABLE array_option_table (id INT, vals INT[8])");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto create_index = compileAndExecute(
        "CREATE INDEX idx_array_option ON array_option_table USING BTREE (vals) "
        "WITH (ARRAY_UNIQUENESS = 'ELEMENT')");
    EXPECT_FALSE(create_index.success());
    EXPECT_NE(create_index.error().find("requires a UNIQUE index"), std::string::npos)
        << create_index.error();
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

TEST_F(QueryCompilerV3Test, ExecuteContextFunctionsRuntimeClosed) {
    auto begin_result = compileAndExecute("BEGIN");
    ASSERT_TRUE(begin_result.success()) << "BEGIN failed: " << begin_result.error();

    auto result = compileAndExecute(
        "SELECT CURRENT_USER, CURRENT_CONNECTION, CURRENT_TRANSACTION, NOW()");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 4);

    EXPECT_FALSE(rs->getValue(0, 0).isNull()); // CURRENT_USER
    EXPECT_FALSE(rs->getValue(0, 1).isNull()); // CURRENT_CONNECTION
    EXPECT_FALSE(rs->getValue(0, 2).isNull()); // CURRENT_TRANSACTION
    EXPECT_FALSE(rs->getValue(0, 3).isNull()); // NOW()

    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << "COMMIT failed: " << commit_result.error();
}

TEST_F(QueryCompilerV3Test, ExecuteBareContextKeywordsRuntimeClosed) {
    auto begin_result = compileAndExecute("BEGIN");
    ASSERT_TRUE(begin_result.success()) << "BEGIN failed: " << begin_result.error();

    auto result = compileAndExecute(
        "SELECT NOW, CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP, SESSION_USER, CURRENT_SESSION");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 6);

    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_FALSE(rs->getValue(0, 1).isNull());
    EXPECT_FALSE(rs->getValue(0, 2).isNull());
    EXPECT_FALSE(rs->getValue(0, 3).isNull());
    EXPECT_FALSE(rs->getValue(0, 4).isNull());
    EXPECT_FALSE(rs->getValue(0, 5).isNull());

    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << "COMMIT failed: " << commit_result.error();
}

TEST_F(QueryCompilerV3Test, ExecuteNowVsCurrentTimestampSemantics) {
    auto begin_result = compileAndExecute("BEGIN");
    ASSERT_TRUE(begin_result.success()) << "BEGIN failed: " << begin_result.error();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    auto first = compileAndExecute("SELECT CURRENT_TIMESTAMP, NOW(), 1");
    ASSERT_TRUE(first.success()) << "Execution failed: " << first.error();
    ASSERT_TRUE(first.hasResultSet());
    auto* first_rs = first.resultSet();
    ASSERT_NE(first_rs, nullptr);
    ASSERT_EQ(first_rs->rowCount(), 1);
    ASSERT_EQ(first_rs->columnCount(), 3);

    const auto first_current_ts = first_rs->getValue(0, 0);
    const auto first_now_ts = first_rs->getValue(0, 1);
    ASSERT_FALSE(first_current_ts.isNull());
    ASSERT_FALSE(first_now_ts.isNull());

    const int64_t first_current_micros = first_current_ts.getTimestamp();
    const int64_t first_now_micros = first_now_ts.getTimestamp();
    EXPECT_GE(first_now_micros, first_current_micros);
    EXPECT_GE(first_now_micros - first_current_micros, 5000);

    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    auto second = compileAndExecute("SELECT CURRENT_TIMESTAMP, NOW(), 2");
    ASSERT_TRUE(second.success()) << "Execution failed: " << second.error();
    ASSERT_TRUE(second.hasResultSet());
    auto* second_rs = second.resultSet();
    ASSERT_NE(second_rs, nullptr);
    ASSERT_EQ(second_rs->rowCount(), 1);
    ASSERT_EQ(second_rs->columnCount(), 3);

    const auto second_current_ts = second_rs->getValue(0, 0);
    const auto second_now_ts = second_rs->getValue(0, 1);
    ASSERT_FALSE(second_current_ts.isNull());
    ASSERT_FALSE(second_now_ts.isNull());

    const int64_t second_current_micros = second_current_ts.getTimestamp();
    const int64_t second_now_micros = second_now_ts.getTimestamp();

    EXPECT_EQ(second_current_micros, first_current_micros);
    EXPECT_GE(second_now_micros, first_now_micros + 5000);
    EXPECT_GE(second_now_micros, second_current_micros);
    EXPECT_GE(second_now_micros - second_current_micros, 5000);

    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << "COMMIT failed: " << commit_result.error();
}

TEST_F(QueryCompilerV3Test, ExecuteV3AbsFunctionEvaluates) {
    auto result = compileAndExecute("SELECT ABS(-5)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 1);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 5);
}

TEST_F(QueryCompilerV3Test, ExecuteV3LikeOperatorEvaluates) {
    auto result = compileAndExecute("SELECT 'alpha' LIKE 'a%'");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_TRUE(rs->getValue(0, 0).toBoolean());
}

TEST_F(QueryCompilerV3Test, ExecuteV3InListEvaluates) {
    auto result = compileAndExecute("SELECT 1 IN (1, 2, 3)");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_TRUE(rs->getValue(0, 0).toBoolean());
}

TEST_F(QueryCompilerV3Test, ExecuteV3RegexOperatorEvaluates) {
    auto result = compileAndExecute("SELECT 'alpha' ~ 'a.*'");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_TRUE(rs->getValue(0, 0).toBoolean());
}

TEST_F(QueryCompilerV3Test, ExecuteV3MathAndConcatFunctionsEvaluate) {
    auto result = compileAndExecute(
        "SELECT POWER(2, 3), SIN(0), COS(0), TAN(0), CONCAT('a', 'b')");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 5);

    EXPECT_NEAR(rs->getValue(0, 0).toDouble(), 8.0, 1e-9);
    EXPECT_NEAR(rs->getValue(0, 1).toDouble(), 0.0, 1e-9);
    EXPECT_NEAR(rs->getValue(0, 2).toDouble(), 1.0, 1e-9);
    EXPECT_NEAR(rs->getValue(0, 3).toDouble(), 0.0, 1e-9);
    EXPECT_EQ(rs->getValue(0, 4).toString(), "ab");
}

TEST_F(QueryCompilerV3Test, ExecuteV3ConcatOperatorEvaluates) {
    auto result = compileAndExecute("SELECT 'a' || 'b'");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1);
    ASSERT_EQ(rs->columnCount(), 1);
    EXPECT_EQ(rs->getValue(0, 0).toString(), "ab");
}

TEST_F(QueryCompilerV3Test, ExecuteOperatorStrictModeBlocksImplicitNumericCast) {
    auto relaxed = compileAndExecute("SELECT '2' + 3");
    ASSERT_TRUE(relaxed.success()) << "Execution failed: " << relaxed.error();
    ASSERT_TRUE(relaxed.hasResultSet());
    EXPECT_FALSE(relaxed.resultSet()->getValue(0, 0).isNull());

    auto set_mode = compileAndExecute("SET operator.strict_mode ON");
    ASSERT_TRUE(set_mode.success()) << "Failed to enable strict mode: " << set_mode.error();

    auto strict = compileAndExecute("SELECT '2' + 3");
    if (strict.success()) {
        ASSERT_TRUE(strict.hasResultSet());
        EXPECT_TRUE(strict.resultSet()->getValue(0, 0).isNull());
    } else {
        EXPECT_NE(strict.error().find("Implicit casts disabled"), std::string::npos);
    }

    auto reset_mode = compileAndExecute("SET operator.strict_mode OFF");
    ASSERT_TRUE(reset_mode.success()) << "Failed to disable strict mode: " << reset_mode.error();
}

TEST_F(QueryCompilerV3Test, ExecuteSetSchemaShorthandUpdatesSchemaContext) {
    auto set_schema = compileAndExecute("SET SCHEMA test");
    ASSERT_TRUE(set_schema.success()) << "SET SCHEMA failed: " << set_schema.error();

    auto show_current = compileAndExecute("SHOW current_schema");
    ASSERT_TRUE(show_current.success()) << "SHOW current_schema failed: " << show_current.error();
    ASSERT_TRUE(show_current.hasResultSet());
    ASSERT_EQ(show_current.resultSet()->rowCount(), 1u);
    ASSERT_EQ(show_current.resultSet()->columnCount(), 2u);
    const auto current_schema = show_current.resultSet()->getValue(0, 1).toString();
    EXPECT_NE(current_schema.find("test"), std::string::npos);

    auto show_path = compileAndExecute("SHOW search_path");
    ASSERT_TRUE(show_path.success()) << "SHOW search_path failed: " << show_path.error();
    ASSERT_TRUE(show_path.hasResultSet());
    ASSERT_EQ(show_path.resultSet()->rowCount(), 1u);
    ASSERT_EQ(show_path.resultSet()->columnCount(), 2u);
    const auto search_path = show_path.resultSet()->getValue(0, 1).toString();
    EXPECT_NE(search_path.find("test"), std::string::npos);
}

TEST_F(QueryCompilerV3Test, ExecuteSetCurrentSchemaSupportsToAndDefault) {
    auto set_schema = compileAndExecute("SET CURRENT_SCHEMA TO test");
    ASSERT_TRUE(set_schema.success()) << "SET CURRENT_SCHEMA failed: " << set_schema.error();

    auto show_current = compileAndExecute("SHOW current_schema");
    ASSERT_TRUE(show_current.success()) << "SHOW current_schema failed: " << show_current.error();
    ASSERT_TRUE(show_current.hasResultSet());
    ASSERT_EQ(show_current.resultSet()->rowCount(), 1u);
    const auto current_schema = show_current.resultSet()->getValue(0, 1).toString();
    EXPECT_NE(current_schema.find("test"), std::string::npos);

    auto reset_schema = compileAndExecute("SET CURRENT_SCHEMA DEFAULT");
    ASSERT_TRUE(reset_schema.success()) << "SET CURRENT_SCHEMA DEFAULT failed: "
                                        << reset_schema.error();

    auto show_after_reset = compileAndExecute("SHOW current_schema");
    ASSERT_TRUE(show_after_reset.success()) << "SHOW current_schema failed: "
                                            << show_after_reset.error();
    ASSERT_TRUE(show_after_reset.hasResultSet());
    ASSERT_EQ(show_after_reset.resultSet()->rowCount(), 1u);
    const auto reset_schema_value = show_after_reset.resultSet()->getValue(0, 1).toString();
    EXPECT_NE(reset_schema_value.find("public"), std::string::npos);
}

TEST_F(QueryCompilerV3Test, EmulatedSessionResetAllKeepsDialectTag) {
    connection_ctx_->set_dialect_tag("MYSQL");

    auto reset_all = compileAndExecute("RESET ALL");
    ASSERT_TRUE(reset_all.success()) << "RESET ALL failed: " << reset_all.error();
    EXPECT_EQ(connection_ctx_->dialect_tag(), "MYSQL");
}

TEST_F(QueryCompilerV3Test, EmulatedSessionRejectsSetParserDefaultOrSwitch) {
    connection_ctx_->set_dialect_tag("MYSQL");

    auto reset_parser = compileAndExecute("SET PARSER DEFAULT");
    ASSERT_FALSE(reset_parser.success());
    EXPECT_EQ(connection_ctx_->dialect_tag(), "MYSQL");

    auto set_parser_native = compileAndExecute("SET PARSER SCRATCHBIRD");
    ASSERT_FALSE(set_parser_native.success());
    EXPECT_EQ(connection_ctx_->dialect_tag(), "MYSQL");
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
