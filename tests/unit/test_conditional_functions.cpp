#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/semantic_analyzer.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include <string>

using namespace scratchbird::parser;
using namespace scratchbird::sblr;
using namespace scratchbird::core;

class ConditionalFunctionTest : public ::testing::Test
{
protected:
    void testParse(const std::string &sql, bool should_succeed = true)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        if (should_succeed)
        {
            ASSERT_TRUE(result.success()) << "Parse failed for: " << sql;
            if (!parser.hasErrors())
            {
                // Also test semantic analysis
                SemanticAnalyzer analyzer(parser.stringPool());
                auto semantic_result = analyzer.analyze(result.statement());
                EXPECT_TRUE(semantic_result.success()) << "Semantic analysis failed";
            }
        }
        else
        {
            EXPECT_FALSE(result.success()) << "Parse should have failed for: " << sql;
        }
    }

    void testBytecodeGeneration(const std::string &sql, bool should_succeed = true)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "Parse failed for: " << sql;

        BytecodeGenerator generator(parser.stringPool());
        auto bytecode_result = generator.generate(result.statement());

        if (should_succeed)
        {
            EXPECT_TRUE(bytecode_result.success()) << "Bytecode generation failed for: " << sql;
            EXPECT_FALSE(bytecode_result.bytecode().empty()) << "Bytecode is empty";
        }
        else
        {
            EXPECT_FALSE(bytecode_result.success()) << "Bytecode generation should have failed for: " << sql;
        }
    }
};

// ===== COALESCE Function Tests =====

TEST_F(ConditionalFunctionTest, COALESCE_Basic)
{
    std::string sql = "SELECT COALESCE(NULL, 'fallback') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_MultipleArgs)
{
    std::string sql = "SELECT COALESCE(NULL, NULL, 'third', 'fourth') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_WithColumns)
{
    std::string sql = "SELECT COALESCE(optional_email, backup_email, 'no-email') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_TwoArgs)
{
    std::string sql = "SELECT COALESCE(name, 'Anonymous') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_Nested)
{
    std::string sql = "SELECT COALESCE(a, COALESCE(b, c, d)) FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_WithExpressions)
{
    std::string sql = "SELECT COALESCE(age + 1, default_age * 2) FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, COALESCE_InWhereClause)
{
    std::string sql = "SELECT * FROM users WHERE COALESCE(status, 'active') = 'active'";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== NULLIF Function Tests =====

TEST_F(ConditionalFunctionTest, NULLIF_Basic)
{
    std::string sql = "SELECT NULLIF(value, 0) FROM stats";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, NULLIF_WithStrings)
{
    std::string sql = "SELECT NULLIF(name, '') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, NULLIF_WithColumns)
{
    std::string sql = "SELECT NULLIF(current_value, previous_value) FROM changes";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, NULLIF_WithExpressions)
{
    std::string sql = "SELECT NULLIF(a + b, c * d) FROM calculations";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, NULLIF_InWhereClause)
{
    std::string sql = "SELECT * FROM users WHERE NULLIF(status, 'deleted') IS NOT NULL";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, NULLIF_Nested)
{
    std::string sql = "SELECT NULLIF(NULLIF(a, b), c) FROM data";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== CASE Expression Tests - Simple Form =====

TEST_F(ConditionalFunctionTest, CASE_Simple_Basic)
{
    std::string sql = "SELECT CASE status WHEN 'active' THEN 1 WHEN 'inactive' THEN 0 END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Simple_WithElse)
{
    std::string sql = "SELECT CASE status WHEN 'active' THEN 1 ELSE 0 END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Simple_MultipleWhen)
{
    std::string sql = "SELECT CASE grade WHEN 'A' THEN 4.0 WHEN 'B' THEN 3.0 WHEN 'C' THEN 2.0 WHEN 'D' THEN 1.0 ELSE 0.0 END FROM grades";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Simple_WithStrings)
{
    std::string sql = "SELECT CASE color WHEN 'red' THEN '#FF0000' WHEN 'green' THEN '#00FF00' WHEN 'blue' THEN '#0000FF' END FROM colors";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Simple_WithExpressions)
{
    std::string sql = "SELECT CASE status * 2 WHEN 2 THEN 'double' WHEN 4 THEN 'quad' END FROM data";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== CASE Expression Tests - Searched Form =====

TEST_F(ConditionalFunctionTest, CASE_Searched_Basic)
{
    std::string sql = "SELECT CASE WHEN age < 18 THEN 'minor' WHEN age >= 18 THEN 'adult' END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Searched_WithElse)
{
    std::string sql = "SELECT CASE WHEN score >= 90 THEN 'A' WHEN score >= 80 THEN 'B' ELSE 'F' END FROM scores";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Searched_ComplexConditions)
{
    std::string sql = "SELECT CASE WHEN age < 13 THEN 'child' WHEN age >= 13 AND age < 20 THEN 'teen' WHEN age >= 20 AND age < 65 THEN 'adult' ELSE 'senior' END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Searched_WithNullChecks)
{
    std::string sql = "SELECT CASE WHEN email IS NULL THEN 'no-email' WHEN email = '' THEN 'empty' ELSE email END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Searched_InWhereClause)
{
    std::string sql = "SELECT * FROM users WHERE CASE WHEN age < 18 THEN 0 ELSE 1 END = 1";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, CASE_Searched_MultipleConditions)
{
    std::string sql = "SELECT CASE WHEN x > 0 AND y > 0 THEN 'Q1' WHEN x < 0 AND y > 0 THEN 'Q2' WHEN x < 0 AND y < 0 THEN 'Q3' WHEN x > 0 AND y < 0 THEN 'Q4' ELSE 'origin' END FROM points";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== Nested Conditional Functions Tests =====

TEST_F(ConditionalFunctionTest, Nested_COALESCE_In_CASE)
{
    std::string sql = "SELECT CASE WHEN COALESCE(status, 'unknown') = 'active' THEN 1 ELSE 0 END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Nested_CASE_In_COALESCE)
{
    std::string sql = "SELECT COALESCE(name, CASE WHEN id > 0 THEN 'User' || id ELSE 'Unknown' END) FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Nested_NULLIF_In_CASE)
{
    std::string sql = "SELECT CASE WHEN NULLIF(value, 0) IS NULL THEN 'zero' ELSE 'nonzero' END FROM data";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Nested_Multiple_Levels)
{
    std::string sql = "SELECT COALESCE(CASE WHEN age < 18 THEN NULLIF(child_name, '') ELSE NULLIF(adult_name, '') END, 'Anonymous') FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Nested_CASE_In_CASE)
{
    std::string sql = "SELECT CASE WHEN status = 'active' THEN CASE WHEN premium = 1 THEN 'premium' ELSE 'regular' END ELSE 'inactive' END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== Combined Tests =====

TEST_F(ConditionalFunctionTest, Combined_All_Three_Functions)
{
    std::string sql = "SELECT COALESCE(NULLIF(name, ''), CASE WHEN id > 0 THEN 'User' ELSE 'Guest' END) FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Combined_In_Aggregates)
{
    std::string sql = "SELECT COUNT(CASE WHEN status = 'active' THEN 1 END) FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

TEST_F(ConditionalFunctionTest, Combined_Multiple_Select_Columns)
{
    std::string sql = "SELECT COALESCE(name, 'Unknown'), NULLIF(email, ''), CASE WHEN age >= 18 THEN 'adult' ELSE 'minor' END FROM users";
    testParse(sql, true);
    testBytecodeGeneration(sql, true);
}

// ===== Error Cases =====

TEST_F(ConditionalFunctionTest, Error_COALESCE_NoArgs)
{
    std::string sql = "SELECT COALESCE() FROM users";
    testParse(sql, false);
}

TEST_F(ConditionalFunctionTest, Error_NULLIF_OneArg)
{
    std::string sql = "SELECT NULLIF(value) FROM users";
    testParse(sql, false);
}

TEST_F(ConditionalFunctionTest, Error_NULLIF_ThreeArgs)
{
    std::string sql = "SELECT NULLIF(a, b, c) FROM users";
    testParse(sql, false);
}

TEST_F(ConditionalFunctionTest, Error_CASE_NoWhen)
{
    std::string sql = "SELECT CASE status END FROM users";
    testParse(sql, false);
}

TEST_F(ConditionalFunctionTest, Error_CASE_NoEnd)
{
    std::string sql = "SELECT CASE WHEN age > 18 THEN 'adult' FROM users";
    testParse(sql, false);
}

TEST_F(ConditionalFunctionTest, Error_CASE_WhenWithoutThen)
{
    std::string sql = "SELECT CASE WHEN age > 18 'adult' END FROM users";
    testParse(sql, false);
}
