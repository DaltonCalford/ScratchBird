/**
 * @file test_week3_week4_comprehensive.cpp
 * @brief Comprehensive tests for Week 3 (Semantic) and Week 4 (Bytecode)
 * 
 * Additional tests beyond the existing test coverage to ensure
 * robustness of semantic analysis and bytecode generation.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/semantic_analyzer.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include <chrono>
#include <algorithm>

using namespace scratchbird::parser;
using namespace scratchbird::sblr;

class Week3Week4ComprehensiveTest : public ::testing::Test {
protected:
    struct TestResult {
        bool parse_success = false;
        bool semantic_success = false;
        bool bytecode_success = false;
        std::vector<uint8_t> bytecode;
        std::string error_message;
        std::string disassembly;
    };
    
    TestResult compileAndAnalyze(const std::string& sql) {
        TestResult result;
        
        // Parse
        Lexer lexer(sql);
        arena_ = std::make_unique<ASTArena>();
        Parser parser(lexer, *arena_);
        
        auto parse_result = parser.parseStatement();
        result.parse_success = parse_result.success();
        
        if (!result.parse_success) {
            if (!parse_result.errors().empty()) {
                result.error_message = parse_result.errors()[0].message;
            }
            return result;
        }
        
        // Semantic analysis
        SemanticAnalyzer analyzer(parser.stringPool());
        auto semantic_result = analyzer.analyze(parse_result.statement());
        result.semantic_success = semantic_result.success();
        
        if (!result.semantic_success) {
            if (!semantic_result.errors().empty()) {
                result.error_message = semantic_result.errors()[0].message;
            }
            return result;
        }
        
        // Bytecode generation
        BytecodeGenerator generator(parser.stringPool());
        auto bytecode_result = generator.generate(parse_result.statement());
        result.bytecode_success = bytecode_result.success();
        result.bytecode = bytecode_result.bytecode();
        
        if (!result.bytecode_success) {
            if (!bytecode_result.errors().empty()) {
                result.error_message = bytecode_result.errors()[0];
            }
        }
        
        // Generate disassembly for debugging
        if (result.bytecode_success) {
            result.disassembly = BytecodeDisassembler::disassemble(result.bytecode);
        }
        
        return result;
    }
    
    bool containsOpcode(const std::vector<uint8_t>& bytecode, Opcode op) {
        uint8_t opcode_byte = static_cast<uint8_t>(op);
        return std::find(bytecode.begin(), bytecode.end(), opcode_byte) != bytecode.end();
    }
    
private:
    std::unique_ptr<ASTArena> arena_;
};

// ============================================================================
// Semantic Analysis Edge Cases
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, Semantic_TypeCoercion) {
    // Integer to double promotion
    auto result = compileAndAnalyze("SELECT 5 * 2.5 FROM dual");
    EXPECT_TRUE(result.semantic_success) << "Integer to double coercion should work";
    EXPECT_TRUE(result.bytecode_success);
    
    // Should generate proper literals
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::LITERAL_INT32));
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::LITERAL_DOUBLE));
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::EXPR_MULTIPLY));
}

TEST_F(Week3Week4ComprehensiveTest, Semantic_InvalidTypeOperations) {
    // String arithmetic not allowed
    auto result = compileAndAnalyze("SELECT 'hello' + 5 FROM dual");
    EXPECT_FALSE(result.semantic_success) 
        << "String arithmetic should fail semantic analysis";
    EXPECT_TRUE(result.error_message.find("type") != std::string::npos)
        << "Error should mention type mismatch";
}

TEST_F(Week3Week4ComprehensiveTest, Semantic_TableValidation) {
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

TEST_F(Week3Week4ComprehensiveTest, Bytecode_ExpressionPrecedence) {
    // Test correct precedence: 2 + 3 * 4 should be 2 + (3 * 4) = 14
    auto result = compileAndAnalyze("SELECT 2 + 3 * 4 FROM dual");
    ASSERT_TRUE(result.bytecode_success);
    
    // Bytecode should reflect postfix notation: 2 3 4 * +
    // Find the multiplication before addition in bytecode
    auto mul_pos = std::find(result.bytecode.begin(), result.bytecode.end(), 
                            static_cast<uint8_t>(Opcode::EXPR_MULTIPLY));
    auto add_pos = std::find(result.bytecode.begin(), result.bytecode.end(), 
                            static_cast<uint8_t>(Opcode::EXPR_ADD));
    
    EXPECT_NE(mul_pos, result.bytecode.end()) << "Should contain multiply";
    EXPECT_NE(add_pos, result.bytecode.end()) << "Should contain add";
    
    if (mul_pos != result.bytecode.end() && add_pos != result.bytecode.end()) {
        EXPECT_LT(mul_pos, add_pos) << "Multiply should come before add in postfix";
    }
}

TEST_F(Week3Week4ComprehensiveTest, Bytecode_ComparisonOperators) {
    struct TestCase {
        std::string sql;
        Opcode expected_op;
    };
    
    std::vector<TestCase> test_cases = {
        {"SELECT * FROM users WHERE age = 25", Opcode::EXPR_EQ},
        {"SELECT * FROM users WHERE age != 25", Opcode::EXPR_NE},
        {"SELECT * FROM users WHERE age < 25", Opcode::EXPR_LT},
        {"SELECT * FROM users WHERE age > 25", Opcode::EXPR_GT},
        {"SELECT * FROM users WHERE age <= 25", Opcode::EXPR_LE},
        {"SELECT * FROM users WHERE age >= 25", Opcode::EXPR_GE}
    };
    
    for (const auto& test : test_cases) {
        auto result = compileAndAnalyze(test.sql);
        EXPECT_TRUE(result.bytecode_success) << "Failed: " << test.sql;
        EXPECT_TRUE(containsOpcode(result.bytecode, test.expected_op))
            << "Missing expected opcode for: " << test.sql;
    }
}

TEST_F(Week3Week4ComprehensiveTest, Bytecode_LogicalOperators) {
    // AND operator
    auto result = compileAndAnalyze("SELECT * FROM users WHERE age > 18 AND status = 1");
    ASSERT_TRUE(result.bytecode_success);
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::EXPR_AND));
    
    // OR operator
    result = compileAndAnalyze("SELECT * FROM users WHERE age < 18 OR age > 65");
    ASSERT_TRUE(result.bytecode_success);
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::EXPR_OR));
}

// ============================================================================
// End-to-End Complex Queries
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, ComplexQuery_CreateTable) {
    auto result = compileAndAnalyze(
        "CREATE TABLE products ("
        "  id INTEGER NOT NULL,"
        "  name VARCHAR(100) NOT NULL,"
        "  price DOUBLE,"
        "  stock INTEGER"
        ")"
    );
    
    ASSERT_TRUE(result.bytecode_success);
    
    // Should contain CREATE_TABLE opcode
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::CREATE_TABLE));
    
    // Should contain column definitions
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::COLUMN_DEF));
    
    // Should contain data types
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::TYPE_INTEGER));
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::TYPE_VARCHAR));
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::TYPE_DOUBLE));
    
    // Should contain NOT NULL modifiers
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::NOT_NULL));
}

TEST_F(Week3Week4ComprehensiveTest, ComplexQuery_InsertMultipleValues) {
    auto result = compileAndAnalyze(
        "INSERT INTO products VALUES (1, 'Widget', 9.99, 100)"
    );
    
    ASSERT_TRUE(result.bytecode_success);
    
    // Should contain INSERT opcode
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::INSERT));
    
    // Should contain various literal types
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::LITERAL_INT32));
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::LITERAL_STRING));
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::LITERAL_DOUBLE));
}

TEST_F(Week3Week4ComprehensiveTest, ComplexQuery_SelectWithExpressions) {
    auto result = compileAndAnalyze(
        "SELECT id, price * 1.1 + 5 FROM products WHERE price >= 50.0"
    );
    
    ASSERT_TRUE(result.bytecode_success);
    
    // Should contain SELECT opcode
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::SELECT));
    
    // Should contain arithmetic operations
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::EXPR_MULTIPLY));
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::EXPR_ADD));
    
    // Should contain WHERE clause
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::EXPR_GE));
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, Performance_LargeQuery) {
    // Build a query with many columns
    std::string sql = "CREATE TABLE large_table (";
    for (int i = 0; i < 100; i++) {
        if (i > 0) sql += ", ";
        sql += "col" + std::to_string(i) + " INTEGER";
    }
    sql += ")";
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = compileAndAnalyze(sql);
    auto end = std::chrono::high_resolution_clock::now();
    
    ASSERT_TRUE(result.bytecode_success);
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100) 
        << "Compilation should be fast even for large queries";
    
    // Bytecode size should be reasonable
    EXPECT_LT(result.bytecode.size(), 10000) 
        << "Bytecode size excessive for 100 columns";
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, ErrorHandling_GracefulFailure) {
    // Parse error
    auto result = compileAndAnalyze("SELECT FROM WHERE");
    EXPECT_FALSE(result.parse_success);
    EXPECT_TRUE(result.bytecode.empty());
    
    // Semantic error
    result = compileAndAnalyze("SELECT unknown_col FROM unknown_table");
    EXPECT_TRUE(result.parse_success); // Parse should succeed
    EXPECT_FALSE(result.semantic_success); // Semantic should fail
    EXPECT_TRUE(result.bytecode.empty()); // No bytecode generated
}

// ============================================================================
// Special Cases
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, SpecialCase_NullHandling) {
    auto result = compileAndAnalyze("INSERT INTO users VALUES (1, NULL, NULL)");
    ASSERT_TRUE(result.bytecode_success);
    
    // Should contain NULL literals
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::LITERAL_NULL));
}

TEST_F(Week3Week4ComprehensiveTest, SpecialCase_SelectStar) {
    auto result = compileAndAnalyze("SELECT * FROM users");
    ASSERT_TRUE(result.bytecode_success);
    
    // Should contain SELECT_STAR opcode
    EXPECT_TRUE(containsOpcode(result.bytecode, Opcode::SELECT_STAR));
}

// ============================================================================
// Bytecode Structure Tests
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, BytecodeStructure_VersionHeader) {
    auto result = compileAndAnalyze("SELECT 1 FROM dual");
    ASSERT_TRUE(result.bytecode_success);
    ASSERT_GE(result.bytecode.size(), 2);
    
    // Should start with VERSION opcode
    EXPECT_EQ(result.bytecode[0], static_cast<uint8_t>(Opcode::VERSION));
    EXPECT_EQ(result.bytecode[1], SBLR_VERSION);
}

TEST_F(Week3Week4ComprehensiveTest, BytecodeStructure_ProperTermination) {
    auto result = compileAndAnalyze("SELECT 1 FROM dual");
    ASSERT_TRUE(result.bytecode_success);
    ASSERT_FALSE(result.bytecode.empty());
    
    // Should end with END opcode
    EXPECT_EQ(result.bytecode.back(), static_cast<uint8_t>(Opcode::END));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(Week3Week4ComprehensiveTest, Integration_FullWorkflow) {
    // Test complete workflow with multiple statement types
    std::vector<std::string> queries = {
        "CREATE TABLE test (id INTEGER, value DOUBLE)",
        "INSERT INTO test VALUES (1, 3.14)",
        "SELECT id, value * 2 FROM test WHERE id = 1"
    };
    
    for (const auto& sql : queries) {
        auto result = compileAndAnalyze(sql);
        EXPECT_TRUE(result.bytecode_success) 
            << "Failed for: " << sql << "\nError: " << result.error_message;
        
        EXPECT_FALSE(result.bytecode.empty());
        EXPECT_FALSE(result.disassembly.empty());
        
        // All should have proper version header and termination
        if (!result.bytecode.empty()) {
            EXPECT_EQ(result.bytecode[0], static_cast<uint8_t>(Opcode::VERSION));
            EXPECT_EQ(result.bytecode.back(), static_cast<uint8_t>(Opcode::END));
        }
    }
}