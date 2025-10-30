/**
 * Unit tests for Range Type Lexer Support
 * Task 15 Phase 4: SQL Parser Integration - Lexer Tests
 *
 * Tests lexer recognition of:
 * - Range type keywords (INT4RANGE, INT8RANGE, NUMRANGE, DATERANGE, TSRANGE, TSTZRANGE)
 * - Range operators (<<, >>, -|-)
 * - Reuse of existing operators (&&, @>, <@, [, ])
 */

#include "scratchbird/parser/lexer.h"
#include <iostream>
#include <cassert>

using namespace scratchbird::parser;

// Test harness
int g_tests_passed = 0;
int g_tests_failed = 0;

#define test_assert(condition, message)                                                            \
    do                                                                                             \
    {                                                                                              \
        if (condition)                                                                             \
        {                                                                                          \
            std::cout << "[PASS] " << message << std::endl;                                       \
            g_tests_passed++;                                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            std::cout << "[FAIL] " << message << std::endl;                                       \
            g_tests_failed++;                                                                      \
        }                                                                                          \
    } while (0)

// ============================================================================
// Range Type Keywords
// ============================================================================

void test_range_type_keywords()
{
    std::cout << "\n=== test_range_type_keywords ===" << std::endl;

    // Test INT4RANGE keyword
    {
        Lexer lexer("INT4RANGE");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::KW_INT4RANGE, "Recognizes INT4RANGE keyword");
    }

    // Test INT8RANGE keyword
    {
        Lexer lexer("INT8RANGE");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::KW_INT8RANGE, "Recognizes INT8RANGE keyword");
    }

    // Test NUMRANGE keyword
    {
        Lexer lexer("NUMRANGE");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::KW_NUMRANGE, "Recognizes NUMRANGE keyword");
    }

    // Test DATERANGE keyword
    {
        Lexer lexer("DATERANGE");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::KW_DATERANGE, "Recognizes DATERANGE keyword");
    }

    // Test TSRANGE keyword
    {
        Lexer lexer("TSRANGE");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::KW_TSRANGE, "Recognizes TSRANGE keyword");
    }

    // Test TSTZRANGE keyword
    {
        Lexer lexer("TSTZRANGE");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::KW_TSTZRANGE, "Recognizes TSTZRANGE keyword");
    }

    // Test case insensitivity
    {
        Lexer lexer("int4range");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::KW_INT4RANGE, "INT4RANGE keyword is case-insensitive");
    }

    {
        Lexer lexer("DateRange");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::KW_DATERANGE, "DATERANGE keyword is case-insensitive");
    }
}

// ============================================================================
// Range Operators
// ============================================================================

void test_range_operators()
{
    std::cout << "\n=== test_range_operators ===" << std::endl;

    // Test << operator (strictly left of)
    {
        Lexer lexer("<<");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::SHIFT_LEFT, "Recognizes << operator");
    }

    // Test >> operator (strictly right of)
    {
        Lexer lexer(">>");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::SHIFT_RIGHT, "Recognizes >> operator");
    }

    // Test -|- operator (adjacent)
    {
        Lexer lexer("-|-");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::MINUS_PIPE_MINUS, "Recognizes -|- operator");
    }

    // Test && operator (overlaps - already exists for arrays)
    {
        Lexer lexer("&&");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::AMPERSAND_AMPERSAND, "Recognizes && operator");
    }

    // Test @> operator (contains - already exists for arrays)
    {
        Lexer lexer("@>");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::AT_GREATER, "Recognizes @> operator");
    }

    // Test <@ operator (contained by - already exists for arrays)
    {
        Lexer lexer("<@");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::LESS_AT, "Recognizes <@ operator");
    }
}

// ============================================================================
// Range Literal Syntax
// ============================================================================

void test_range_literal_syntax()
{
    std::cout << "\n=== test_range_literal_syntax ===" << std::endl;

    // Test '[1,10)' tokenization
    {
        Lexer lexer("'[1,10)'");
        Token t = lexer.nextToken();
        test_assert(t.type == TokenType::STRING_LITERAL, "Range literal is recognized as string");
        std::string_view text = lexer.stringPool().get(t.value.string_id);
        test_assert(text == "[1,10)", "Range literal string value is '[1,10)'");
    }

    // Test type cast syntax: '::int4range'
    {
        Lexer lexer("::int4range");
        Token t1 = lexer.nextToken();
        test_assert(t1.type == TokenType::COLON, "First colon tokenizes as COLON");
        Token t2 = lexer.nextToken();
        test_assert(t2.type == TokenType::COLON, "Second colon tokenizes as COLON");
        Token t3 = lexer.nextToken();
        test_assert(t3.type == TokenType::KW_INT4RANGE, "Type name after :: is INT4RANGE");
    }

    // Test complete cast expression: '[1,10)'::int4range
    {
        Lexer lexer("'[1,10)'::int4range");

        Token t1 = lexer.nextToken();
        test_assert(t1.type == TokenType::STRING_LITERAL, "Range literal string");

        Token t2 = lexer.nextToken();
        test_assert(t2.type == TokenType::COLON, "First colon of ::");

        Token t3 = lexer.nextToken();
        test_assert(t3.type == TokenType::COLON, "Second colon of ::");

        Token t4 = lexer.nextToken();
        test_assert(t4.type == TokenType::KW_INT4RANGE, "Target type int4range");
    }
}

// ============================================================================
// Range Constructor Functions
// ============================================================================

void test_range_constructor_syntax()
{
    std::cout << "\n=== test_range_constructor_syntax ===" << std::endl;

    // Test: int4range(1, 10)
    {
        Lexer lexer("int4range(1, 10)");

        Token t1 = lexer.nextToken();
        test_assert(t1.type == TokenType::KW_INT4RANGE, "int4range function name");

        Token t2 = lexer.nextToken();
        test_assert(t2.type == TokenType::LEFT_PAREN, "Opening parenthesis");

        Token t3 = lexer.nextToken();
        test_assert(t3.type == TokenType::INTEGER_LITERAL && t3.value.int_value == 1,
                   "First parameter: 1");

        Token t4 = lexer.nextToken();
        test_assert(t4.type == TokenType::COMMA, "Comma separator");

        Token t5 = lexer.nextToken();
        test_assert(t5.type == TokenType::INTEGER_LITERAL && t5.value.int_value == 10,
                   "Second parameter: 10");

        Token t6 = lexer.nextToken();
        test_assert(t6.type == TokenType::RIGHT_PAREN, "Closing parenthesis");
    }

    // Test: daterange('2023-01-01', '2024-01-01')
    {
        Lexer lexer("daterange('2023-01-01', '2024-01-01')");

        Token t1 = lexer.nextToken();
        test_assert(t1.type == TokenType::KW_DATERANGE, "daterange function name");

        Token t2 = lexer.nextToken();
        test_assert(t2.type == TokenType::LEFT_PAREN, "Opening parenthesis");

        Token t3 = lexer.nextToken();
        test_assert(t3.type == TokenType::STRING_LITERAL, "First date string");

        Token t4 = lexer.nextToken();
        test_assert(t4.type == TokenType::COMMA, "Comma separator");

        Token t5 = lexer.nextToken();
        test_assert(t5.type == TokenType::STRING_LITERAL, "Second date string");

        Token t6 = lexer.nextToken();
        test_assert(t6.type == TokenType::RIGHT_PAREN, "Closing parenthesis");
    }
}

// ============================================================================
// Range Operator Expressions
// ============================================================================

void test_range_operator_expressions()
{
    std::cout << "\n=== test_range_operator_expressions ===" << std::endl;

    // Test: r1 && r2 (overlaps)
    {
        Lexer lexer("r1 && r2");

        Token t1 = lexer.nextToken();
        test_assert(t1.type == TokenType::IDENTIFIER, "First identifier r1");

        Token t2 = lexer.nextToken();
        test_assert(t2.type == TokenType::AMPERSAND_AMPERSAND, "&& operator");

        Token t3 = lexer.nextToken();
        test_assert(t3.type == TokenType::IDENTIFIER, "Second identifier r2");
    }

    // Test: r1 << r2 (strictly left of)
    {
        Lexer lexer("r1 << r2");

        Token t1 = lexer.nextToken();
        test_assert(t1.type == TokenType::IDENTIFIER, "First identifier r1");

        Token t2 = lexer.nextToken();
        test_assert(t2.type == TokenType::SHIFT_LEFT, "<< operator");

        Token t3 = lexer.nextToken();
        test_assert(t3.type == TokenType::IDENTIFIER, "Second identifier r2");
    }

    // Test: r1 >> r2 (strictly right of)
    {
        Lexer lexer("r1 >> r2");

        Token t1 = lexer.nextToken();
        test_assert(t1.type == TokenType::IDENTIFIER, "First identifier r1");

        Token t2 = lexer.nextToken();
        test_assert(t2.type == TokenType::SHIFT_RIGHT, ">> operator");

        Token t3 = lexer.nextToken();
        test_assert(t3.type == TokenType::IDENTIFIER, "Second identifier r2");
    }

    // Test: r1 -|- r2 (adjacent)
    {
        Lexer lexer("r1 -|- r2");

        Token t1 = lexer.nextToken();
        test_assert(t1.type == TokenType::IDENTIFIER, "First identifier r1");

        Token t2 = lexer.nextToken();
        test_assert(t2.type == TokenType::MINUS_PIPE_MINUS, "-|- operator");

        Token t3 = lexer.nextToken();
        test_assert(t3.type == TokenType::IDENTIFIER, "Second identifier r2");
    }

    // Test: r @> 5 (contains element)
    {
        Lexer lexer("r @> 5");

        Token t1 = lexer.nextToken();
        test_assert(t1.type == TokenType::IDENTIFIER, "Identifier r");

        Token t2 = lexer.nextToken();
        test_assert(t2.type == TokenType::AT_GREATER, "@> operator");

        Token t3 = lexer.nextToken();
        test_assert(t3.type == TokenType::INTEGER_LITERAL, "Element value 5");
    }
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===========================================\n";
    std::cout << "Range Type Lexer Tests\n";
    std::cout << "Task 15 Phase 4: SQL Parser Integration\n";
    std::cout << "===========================================\n";

    test_range_type_keywords();
    test_range_operators();
    test_range_literal_syntax();
    test_range_constructor_syntax();
    test_range_operator_expressions();

    // Summary
    std::cout << "\n===========================================\n";
    std::cout << "Tests passed: " << g_tests_passed << "\n";
    std::cout << "Tests failed: " << g_tests_failed << "\n";
    std::cout << "===========================================\n";

    return g_tests_failed > 0 ? 1 : 0;
}
