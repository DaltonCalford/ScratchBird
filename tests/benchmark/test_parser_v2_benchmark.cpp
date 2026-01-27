/**
 * Parser V2 Performance Benchmarks
 *
 * Phase 9: Parser V2 pipeline performance
 *
 * Benchmark categories:
 * 1. Simple SELECT (constant expressions)
 * 2. Complex SELECT (multiple columns, operators)
 * 3. DDL statements
 * 4. Transaction statements
 */

#include <gtest/gtest.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <filesystem>
#include <sstream>
#include <thread>

#include "scratchbird/sblr/query_compiler_v2.h"

#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"

using namespace scratchbird;

// Generate a unique database path per test
static std::string generateUniqueDbPath() {
    std::ostringstream oss;
    oss << "/tmp/test_parser_benchmark_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << ".sbdb";
    return oss.str();
}

class ParserBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = generateUniqueDbPath();
        std::filesystem::remove(test_db_path_);

        core::ErrorContext ctx;
        core::Status status = core::Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to create test database";

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to open test database";

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        core::CatalogManager::SchemaInfo schema;
        status = catalog_->getSchema("PUBLIC", schema, &ctx);
        ASSERT_EQ(status, core::Status::OK);
        test_schema_id_ = schema.schema_id;

        // Create V2 compiler
        compiler_v2_ = std::make_unique<sblr::QueryCompilerV2>(&db_);
        compiler_v2_->setCurrentSchema(test_schema_id_);
    }

    void TearDown() override {
        compiler_v2_.reset();
        db_.close();
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    // Benchmark V2 parser pipeline
    std::chrono::microseconds benchmarkV2(const std::string& sql, int iterations) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i) {
            auto result = compiler_v2_->compile(sql);
            (void)result;  // Suppress unused warning
        }

        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    }

    void printBenchmarkResult(const std::string& test_name,
                              const std::string& sql,
                              int iterations,
                              std::chrono::microseconds v2_time) {
        double v2_avg = static_cast<double>(v2_time.count()) / iterations;

        std::cout << "\n=== " << test_name << " ===\n";
        std::cout << "SQL: " << sql.substr(0, 60) << (sql.length() > 60 ? "..." : "") << "\n";
        std::cout << "Iterations: " << iterations << "\n";
        std::cout << "V2 Total: " << v2_time.count() << " µs"
                  << " (avg: " << std::fixed << std::setprecision(2) << v2_avg << " µs/iter)\n";
        std::cout << "\n";
    }

    std::string test_db_path_;
    core::Database db_;
    core::CatalogManager* catalog_ = nullptr;
    core::ID test_schema_id_;

    std::unique_ptr<sblr::QueryCompilerV2> compiler_v2_;
};

// =============================================================================
// Simple SELECT Benchmarks
// =============================================================================

TEST_F(ParserBenchmarkTest, SimpleSelectConstant) {
    const std::string sql = "SELECT 42";
    const int iterations = 1000;

    auto v2_time = benchmarkV2(sql, iterations);

    printBenchmarkResult("Simple SELECT Constant", sql, iterations, v2_time);

    // Both should complete successfully
    EXPECT_GT(v2_time.count(), 0);
}

TEST_F(ParserBenchmarkTest, SimpleSelectMultipleConstants) {
    const std::string sql = "SELECT 1, 2, 3, 4, 5";
    const int iterations = 1000;

    auto v2_time = benchmarkV2(sql, iterations);

    printBenchmarkResult("Multiple Constants", sql, iterations, v2_time);

    EXPECT_GT(v2_time.count(), 0);
}

// =============================================================================
// Expression Benchmarks
// =============================================================================

TEST_F(ParserBenchmarkTest, ArithmeticExpression) {
    const std::string sql = "SELECT 1 + 2 * 3 - 4 / 2";
    const int iterations = 1000;

    auto v2_time = benchmarkV2(sql, iterations);

    printBenchmarkResult("Arithmetic Expression", sql, iterations, v2_time);

    EXPECT_GT(v2_time.count(), 0);
}

TEST_F(ParserBenchmarkTest, LogicalExpression) {
    const std::string sql = "SELECT TRUE AND FALSE OR TRUE";
    const int iterations = 1000;

    auto v2_time = benchmarkV2(sql, iterations);

    printBenchmarkResult("Logical Expression", sql, iterations, v2_time);

    EXPECT_GT(v2_time.count(), 0);
}

TEST_F(ParserBenchmarkTest, StringExpression) {
    const std::string sql = "SELECT 'hello' || ' ' || 'world'";
    const int iterations = 1000;

    auto v2_time = benchmarkV2(sql, iterations);

    printBenchmarkResult("String Concatenation", sql, iterations, v2_time);

    EXPECT_GT(v2_time.count(), 0);
}

// =============================================================================
// Complex SELECT Benchmarks
// =============================================================================

TEST_F(ParserBenchmarkTest, CaseExpression) {
    const std::string sql = "SELECT CASE WHEN TRUE THEN 'yes' ELSE 'no' END";
    const int iterations = 1000;

    auto v2_time = benchmarkV2(sql, iterations);

    printBenchmarkResult("CASE Expression", sql, iterations, v2_time);

    EXPECT_GT(v2_time.count(), 0);
}

TEST_F(ParserBenchmarkTest, CastExpression) {
    const std::string sql = "SELECT CAST(123 AS VARCHAR)";
    const int iterations = 1000;

    auto v2_time = benchmarkV2(sql, iterations);

    printBenchmarkResult("CAST Expression", sql, iterations, v2_time);

    EXPECT_GT(v2_time.count(), 0);
}

// =============================================================================
// DDL Benchmarks
// =============================================================================

TEST_F(ParserBenchmarkTest, CreateTableSimple) {
    const std::string sql = "CREATE TABLE test_table (id INT, name VARCHAR(100))";
    const int iterations = 1000;

    auto v2_time = benchmarkV2(sql, iterations);

    printBenchmarkResult("CREATE TABLE Simple", sql, iterations, v2_time);

    EXPECT_GT(v2_time.count(), 0);
}

// =============================================================================
// Transaction Benchmarks
// =============================================================================

TEST_F(ParserBenchmarkTest, TransactionStatements) {
    const int iterations = 1000;

    // START TRANSACTION
    auto v2_start = benchmarkV2("START TRANSACTION", iterations);
    printBenchmarkResult("START TRANSACTION", "START TRANSACTION", iterations, v2_start);

    // COMMIT
    auto v2_commit = benchmarkV2("COMMIT", iterations);
    printBenchmarkResult("COMMIT", "COMMIT", iterations, v2_commit);

    // ROLLBACK
    auto v2_rollback = benchmarkV2("ROLLBACK", iterations);
    printBenchmarkResult("ROLLBACK", "ROLLBACK", iterations, v2_rollback);

    EXPECT_GT(v2_start.count(), 0);
}

// =============================================================================
// Summary Test
// =============================================================================

TEST_F(ParserBenchmarkTest, Summary) {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "                   PARSER V2 BENCHMARK SUMMARY              \n";
    std::cout << "============================================================\n";
    std::cout << "\n";
    std::cout << "Parser V2 provides:\n";
    std::cout << "- Resolved AST with UUIDs and type information\n";
    std::cout << "- Semantic analysis before bytecode generation\n";
    std::cout << "- Better error messages with source locations\n";
    std::cout << "- Support for query result caching\n";
    std::cout << "\n";
    std::cout << "Note: V2 may be slightly slower due to additional\n";
    std::cout << "semantic analysis, but provides more features.\n";
    std::cout << "============================================================\n";

    SUCCEED();
}
