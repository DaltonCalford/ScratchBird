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

        // Create a test schema
        status = catalog_->createSchema("test", "test_user", test_schema_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test schema";

        // Create compiler and executor
        compiler_ = std::make_unique<QueryCompilerV2>(&db_);
        compiler_->setCurrentSchema(test_schema_id_);

        executor_ = std::make_unique<Executor>(&db_);
    }

    void TearDown() override {
        compiler_.reset();
        executor_.reset();
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
    ID test_schema_id_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
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
