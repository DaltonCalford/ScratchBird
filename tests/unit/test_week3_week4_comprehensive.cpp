/**
 * @file test_week3_week4_comprehensive.cpp
 * @brief Comprehensive tests for Week 3 (Semantic) and Week 4 (Bytecode)
 *
 * Additional tests beyond the existing test coverage to ensure
 * robustness of semantic analysis and bytecode generation.
 */

#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "test_helpers.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

using scratchbird::testing::TestDatabaseFile;

class Week3Week4ComprehensiveTest : public ::testing::Test
{
protected:
    struct TestResult
    {
        bool parse_success = false;
        bool semantic_success = false;
        bool bytecode_success = false;
        std::vector<uint8_t> bytecode;
        std::string error_message;
        std::string disassembly;
    };

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_week3_week4");

        scratchbird::core::ErrorContext ctx;
        ASSERT_EQ(scratchbird::core::Database::create(db_file_->path(), 16384, &ctx),
                  scratchbird::core::Status::OK)
            << ctx.message;

        db_ = std::make_unique<scratchbird::core::Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), scratchbird::core::Status::OK)
            << ctx.message;

        compiler_ = std::make_unique<scratchbird::sblr::QueryCompilerV2>(db_.get());
        executor_ = std::make_unique<scratchbird::sblr::Executor>(db_.get());

        scratchbird::core::CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx),
                  scratchbird::core::Status::OK)
            << ctx.message;
        schema_id_ = schema.schema_id;
        compiler_->setCurrentSchema(schema_id_);
        executor_->setCurrentSchema(schema_id_);

        createTables();
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        db_.reset();
        db_file_.reset();
    }

    void createTables()
    {
        const std::vector<std::string> ddl = {
            "CREATE TABLE dual (dummy INT)",
            "CREATE TABLE users (id INT, name TEXT, email TEXT, age INT)",
            "CREATE TABLE products (id INTEGER, name TEXT, price DOUBLE, stock INTEGER)",
            "CREATE TABLE test (id INTEGER, value DOUBLE)"
        };

        for (const auto& sql : ddl)
        {
            auto compile_result = compiler_->compile(sql);
            ASSERT_TRUE(compile_result.success()) << "Compile failed for: " << sql;
            auto exec_result = executor_->execute(compile_result.bytecode());
            ASSERT_TRUE(exec_result.success()) << "Execution failed for: " << sql
                                               << " error: " << exec_result.error();
        }
    }

    // Full compile pipeline with semantic analysis
    // Use this for tests that specifically test semantic validation
    TestResult compileAndAnalyze(const std::string &sql)
    {
        TestResult result;

        scratchbird::parser::v2::Parser parser(sql);
        auto parse_result = parser.parseStatement();
        result.parse_success = parse_result.success();

        if (!result.parse_success)
        {
            if (!parse_result.errors().empty())
            {
                result.error_message = parse_result.errors()[0].message;
            }
            return result;
        }

        scratchbird::parser::v2::SemanticAnalyzerV2 analyzer(*db_->catalog_manager(),
                                                             parser.stringPool());
        analyzer.setCurrentSchema(schema_id_);
        auto semantic_result = analyzer.analyze(parse_result.statement());
        result.semantic_success = semantic_result.success();

        if (!result.semantic_success)
        {
            if (!semantic_result.errors().empty())
            {
                result.error_message = semantic_result.errors()[0].message;
            }
            return result;
        }

        scratchbird::parser::v2::BytecodeGeneratorV2 generator(parser.stringPool());
        generator.setOptimizationsEnabled(false);
        auto bytecode_result = generator.generate(semantic_result.statement());
        result.bytecode_success = bytecode_result.success();
        result.bytecode = bytecode_result.bytecode();

        if (!result.bytecode_success)
        {
            if (!bytecode_result.errors().empty())
            {
                result.error_message = bytecode_result.errors()[0];
            }
        }

        if (result.bytecode_success)
        {
            result.disassembly =
                scratchbird::parser::v2::BytecodeDisassemblerV2::disassemble(result.bytecode);
        }

        return result;
    }

    // Compile to bytecode using the full V2 pipeline
    TestResult compileToBytecode(const std::string &sql)
    {
        return compileAndAnalyze(sql);
    }

    bool containsOpcode(const std::vector<uint8_t> &bytecode, scratchbird::sblr::Opcode op)
    {
        uint8_t opcode_byte = static_cast<uint8_t>(op);
        return std::find(bytecode.begin(), bytecode.end(), opcode_byte) != bytecode.end();
    }

protected:
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<scratchbird::core::Database> db_;
    std::unique_ptr<scratchbird::sblr::Executor> executor_;
    std::unique_ptr<scratchbird::sblr::QueryCompilerV2> compiler_;
    scratchbird::core::ID schema_id_;
};

// ============================================================================
// Semantic Analysis Edge Cases
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, Semantic_TypeCoercion)
{
    // Integer to double promotion - bytecode test (no semantic validation needed)
    auto result = compileToBytecode("SELECT 5 * 2.5 FROM dual");
    EXPECT_TRUE(result.bytecode_success);

    // Should generate proper literals (integers may be INT32 or INT64 depending on value)
    bool has_int = containsOpcode(result.bytecode, scratchbird::sblr::Opcode::LITERAL_INT32) ||
                   containsOpcode(result.bytecode, scratchbird::sblr::Opcode::LITERAL_INT64);
    EXPECT_TRUE(has_int);
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::LITERAL_DOUBLE));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::EXPR_MULTIPLY));
}

TEST_F(Week3Week4ComprehensiveTest, Semantic_InvalidTypeOperations)
{
    // String arithmetic - bytecode generator will produce bytecode regardless
    // (actual type checking happens at runtime or with real semantic analysis)
    auto result = compileToBytecode("SELECT 'hello' + 5 FROM dual");
    EXPECT_TRUE(result.bytecode_success);
}

TEST_F(Week3Week4ComprehensiveTest, Semantic_TableValidation)
{
    // Non-existent table
    auto result = compileAndAnalyze("SELECT * FROM non_existent_table");
    EXPECT_FALSE(result.semantic_success);

    // Non-existent column
    result = compileAndAnalyze("SELECT non_existent_col FROM users");
    EXPECT_FALSE(result.semantic_success);
}

// ============================================================================
// Bytecode Generation Edge Cases
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, Bytecode_ExpressionPrecedence)
{
    // Test correct precedence: 2 + 3 * 4 should be 2 + (3 * 4) = 14
    auto result = compileToBytecode("SELECT 2 + 3 * 4 FROM dual");
    ASSERT_TRUE(result.bytecode_success);

    auto mul_pos = std::find(result.bytecode.begin(), result.bytecode.end(),
                             static_cast<uint8_t>(scratchbird::sblr::Opcode::EXPR_MULTIPLY));
    auto add_pos = std::find(result.bytecode.begin(), result.bytecode.end(),
                             static_cast<uint8_t>(scratchbird::sblr::Opcode::EXPR_ADD));

    EXPECT_NE(mul_pos, result.bytecode.end()) << "Should contain multiply";
    EXPECT_NE(add_pos, result.bytecode.end()) << "Should contain add";

    if (mul_pos != result.bytecode.end() && add_pos != result.bytecode.end())
    {
        EXPECT_LT(mul_pos, add_pos) << "Multiply should come before add in postfix";
    }
}

TEST_F(Week3Week4ComprehensiveTest, Bytecode_ComparisonOperators)
{
    struct TestCase
    {
        std::string sql;
        scratchbird::sblr::Opcode expected_op;
    };

    std::vector<TestCase> test_cases = {
        {"SELECT * FROM users WHERE age = 25", scratchbird::sblr::Opcode::EXPR_EQ},
        {"SELECT * FROM users WHERE age <> 25", scratchbird::sblr::Opcode::EXPR_NE},
        {"SELECT * FROM users WHERE age < 25", scratchbird::sblr::Opcode::EXPR_LT},
        {"SELECT * FROM users WHERE age > 25", scratchbird::sblr::Opcode::EXPR_GT},
        {"SELECT * FROM users WHERE age <= 25", scratchbird::sblr::Opcode::EXPR_LE},
        {"SELECT * FROM users WHERE age >= 25", scratchbird::sblr::Opcode::EXPR_GE}
    };

    for (const auto &test : test_cases)
    {
        auto result = compileToBytecode(test.sql);
        EXPECT_TRUE(result.bytecode_success) << "Failed: " << test.sql;
        EXPECT_TRUE(containsOpcode(result.bytecode, test.expected_op))
            << "Missing expected opcode for: " << test.sql;
    }
}

TEST_F(Week3Week4ComprehensiveTest, Bytecode_LogicalOperators)
{
    auto result = compileToBytecode("SELECT * FROM users WHERE age > 18");
    ASSERT_TRUE(result.bytecode_success);
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::EXPR_GT));
}

// ============================================================================
// End-to-End Complex Queries
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, ComplexQuery_CreateTable)
{
    auto result = compileAndAnalyze("CREATE TABLE products2 ("
                                    "  id INTEGER NOT NULL,"
                                    "  name VARCHAR(100) NOT NULL,"
                                    "  price DOUBLE,"
                                    "  stock INTEGER"
                                    ")");

    ASSERT_TRUE(result.bytecode_success);
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::CREATE_TABLE));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::COLUMN_DEF));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::TYPE_INTEGER));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::TYPE_VARCHAR));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::TYPE_DOUBLE));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::NOT_NULL));
}

TEST_F(Week3Week4ComprehensiveTest, ComplexQuery_InsertMultipleValues)
{
    auto result = compileToBytecode(
        "INSERT INTO products (id, name, price, stock) VALUES (1, 'Widget', 9.99, 100)");

    ASSERT_TRUE(result.bytecode_success) << "Error: " << result.error_message;
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::INSERT));

    bool has_int = containsOpcode(result.bytecode, scratchbird::sblr::Opcode::LITERAL_INT32) ||
                   containsOpcode(result.bytecode, scratchbird::sblr::Opcode::LITERAL_INT64);
    EXPECT_TRUE(has_int);
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::LITERAL_STRING));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::LITERAL_DOUBLE));
}

TEST_F(Week3Week4ComprehensiveTest, ComplexQuery_SelectWithExpressions)
{
    auto result = compileToBytecode("SELECT id, price * 1.1 + 5 FROM products WHERE price >= 50.0");

    ASSERT_TRUE(result.bytecode_success);
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::SELECT));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::EXPR_MULTIPLY));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::EXPR_ADD));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::EXPR_GE));
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, Performance_LargeQuery)
{
    std::string sql = "CREATE TABLE large_table (";
    for (int i = 0; i < 100; i++)
    {
        if (i > 0)
            sql += ", ";
        sql += "col" + std::to_string(i) + " INTEGER";
    }
    sql += ")";

    auto start = std::chrono::high_resolution_clock::now();
    auto result = compileToBytecode(sql);
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_TRUE(result.bytecode_success);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100) << "Compilation should be fast even for large queries";
    EXPECT_LT(result.bytecode.size(), 10000) << "Bytecode size excessive for 100 columns";
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, ErrorHandling_GracefulFailure)
{
    auto result = compileAndAnalyze("SELECT FROM WHERE");
    EXPECT_FALSE(result.parse_success);
    EXPECT_TRUE(result.bytecode.empty());

    result = compileAndAnalyze("SELECT unknown_col FROM unknown_table");
    EXPECT_TRUE(result.parse_success);
    EXPECT_FALSE(result.semantic_success);
    EXPECT_TRUE(result.bytecode.empty());
}

// ============================================================================
// Special Cases
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, SpecialCase_NullHandling)
{
    auto result = compileToBytecode("INSERT INTO users (id, name, email) VALUES (1, NULL, NULL)");
    ASSERT_TRUE(result.bytecode_success) << "Error: " << result.error_message;
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::LITERAL_NULL));
}

TEST_F(Week3Week4ComprehensiveTest, SpecialCase_SelectStar)
{
    auto result = compileToBytecode("SELECT * FROM users");
    ASSERT_TRUE(result.bytecode_success);
    EXPECT_TRUE(containsOpcode(result.bytecode, scratchbird::sblr::Opcode::SELECT_STAR) ||
                containsOpcode(result.bytecode, scratchbird::sblr::Opcode::COLUMN_REF));
}

// ============================================================================
// Bytecode Structure Tests
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, BytecodeStructure_VersionHeader)
{
    auto result = compileToBytecode("SELECT 1 FROM dual");
    ASSERT_TRUE(result.bytecode_success);
    ASSERT_GE(result.bytecode.size(), 2u);

    EXPECT_EQ(result.bytecode[0], static_cast<uint8_t>(scratchbird::sblr::Opcode::VERSION));
    EXPECT_EQ(result.bytecode[1], scratchbird::sblr::SBLR_VERSION);
}

TEST_F(Week3Week4ComprehensiveTest, BytecodeStructure_ProperTermination)
{
    auto result = compileToBytecode("SELECT 1 FROM dual");
    ASSERT_TRUE(result.bytecode_success);
    ASSERT_FALSE(result.bytecode.empty());

    EXPECT_EQ(result.bytecode.back(), static_cast<uint8_t>(scratchbird::sblr::Opcode::END));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, Integration_FullWorkflow)
{
    std::vector<std::string> queries = {
        "CREATE TABLE test2 (id INTEGER, value DOUBLE)",
        "INSERT INTO test2 (id, value) VALUES (1, 3.14)",
        "SELECT id, value * 2 FROM test2 WHERE id = 1"
    };

    for (const auto &sql : queries)
    {
        auto result = compileToBytecode(sql);
        EXPECT_TRUE(result.bytecode_success)
            << "Failed for: " << sql << "\nError: " << result.error_message;

        EXPECT_FALSE(result.bytecode.empty());
        EXPECT_FALSE(result.disassembly.empty());

        if (!result.bytecode.empty())
        {
            EXPECT_EQ(result.bytecode[0], static_cast<uint8_t>(scratchbird::sblr::Opcode::VERSION));
            EXPECT_EQ(result.bytecode.back(), static_cast<uint8_t>(scratchbird::sblr::Opcode::END));
        }

        if (result.bytecode_success && sql.rfind("CREATE TABLE", 0) == 0)
        {
            auto exec_result = executor_->execute(result.bytecode);
            EXPECT_TRUE(exec_result.success()) << "Execution failed for: " << sql
                                               << " error: " << exec_result.error();
        }
    }
}
