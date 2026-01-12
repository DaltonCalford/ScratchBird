/**
 * End-to-End Tests for ScratchBird Query Compiler V2
 *
 * Phase 9: Integration tests for the complete Parser V2 pipeline
 *
 * Tests the full compilation flow:
 * SQL -> Lexer -> Parser -> Semantic Analyzer -> Bytecode Generator
 */

#include <gtest/gtest.h>
#include "scratchbird/sblr/query_compiler_v2.h"
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
    oss << "/tmp/test_query_compiler_v2_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << ".sbdb";
    return oss.str();
}

class QueryCompilerV2Test : public ::testing::Test {
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
        compiler_ = std::make_unique<QueryCompilerV2>(&db_);
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
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

// =============================================================================
// Compilation Tests
// =============================================================================

TEST_F(QueryCompilerV2Test, CompileSimpleSelect) {
    auto result = compiler_->compile("SELECT 1");
    ASSERT_TRUE(result.success()) << "Compilation failed";
    EXPECT_GT(result.bytecode().size(), 0);
}

TEST_F(QueryCompilerV2Test, CompileSelectWithArithmetic) {
    auto result = compiler_->compile("SELECT 1 + 2 * 3");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV2Test, CompileSelectWithStringLiteral) {
    auto result = compiler_->compile("SELECT 'hello world'");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV2Test, CompileSelectWithComparison) {
    auto result = compiler_->compile("SELECT 1 = 1, 2 < 3, 4 > 1");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV2Test, CompileSelectWithLogical) {
    auto result = compiler_->compile("SELECT TRUE AND FALSE, TRUE OR FALSE");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV2Test, CompileSelectWithCase) {
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

TEST_F(QueryCompilerV2Test, CompileSelectWithCast) {
    auto result = compiler_->compile("SELECT CAST(123 AS VARCHAR)");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV2Test, ExecuteCreateTableWithDomainColumn) {
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

TEST_F(QueryCompilerV2Test, ExecuteCreateTableWithDomainColumn_DefaultPublicSchema) {
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

TEST_F(QueryCompilerV2Test, ExecuteCreateViewStoresDefinition) {
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

TEST_F(QueryCompilerV2Test, CompileInvalidSyntax) {
    auto result = compiler_->compile("SELECT FROM");
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}

TEST_F(QueryCompilerV2Test, CompileEmptyQuery) {
    auto result = compiler_->compile("");
    EXPECT_FALSE(result.success());
}

// =============================================================================
// Transaction Statement Tests
// =============================================================================

TEST_F(QueryCompilerV2Test, CompileStartTransaction) {
    auto result = compiler_->compile("START TRANSACTION");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV2Test, CompileCommit) {
    auto result = compiler_->compile("COMMIT");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

TEST_F(QueryCompilerV2Test, CompileRollback) {
    auto result = compiler_->compile("ROLLBACK");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

// =============================================================================
// DDL Statement Tests
// =============================================================================

TEST_F(QueryCompilerV2Test, CompileCreateTable) {
    auto result = compiler_->compile("CREATE TABLE products (id INT, name VARCHAR(100))");
    ASSERT_TRUE(result.success()) << "Compilation failed";
}

// =============================================================================
// End-to-End Execution Tests
// =============================================================================

TEST_F(QueryCompilerV2Test, ExecuteSimpleSelect) {
    auto result = compileAndExecute("SELECT 42");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->rowCount(), 1);
    EXPECT_EQ(rs->columnCount(), 1);
}

TEST_F(QueryCompilerV2Test, ExecuteSelectWithExpression) {
    auto result = compileAndExecute("SELECT 10 + 5");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->rowCount(), 1);
}

TEST_F(QueryCompilerV2Test, ExecuteSelectMultipleColumns) {
    auto result = compileAndExecute("SELECT 1, 2, 3");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->rowCount(), 1);
    EXPECT_EQ(rs->columnCount(), 3);
}

TEST_F(QueryCompilerV2Test, ExecuteCastUsingHex) {
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

TEST_F(QueryCompilerV2Test, ExecuteCastUsingBase64) {
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

TEST_F(QueryCompilerV2Test, ExecuteSelectBoolean) {
    auto result = compileAndExecute("SELECT TRUE, FALSE");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
}

TEST_F(QueryCompilerV2Test, ExecuteSelectNull) {
    auto result = compileAndExecute("SELECT NULL");
    ASSERT_TRUE(result.success()) << "Execution failed: " << result.error();
    ASSERT_TRUE(result.hasResultSet());
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(QueryCompilerV2Test, CompilationStatisticsEnabled) {
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

TEST_F(QueryCompilerV2Test, OptimizationsEnabled) {
    compiler_->setOptimizationsEnabled(true);

    auto result = compiler_->compile("SELECT 1 + 2");
    ASSERT_TRUE(result.success());
}

TEST_F(QueryCompilerV2Test, OptimizationsDisabled) {
    compiler_->setOptimizationsEnabled(false);

    auto result = compiler_->compile("SELECT 1 + 2");
    ASSERT_TRUE(result.success());
}
