#include <gtest/gtest.h>
#include <memory>
#include "scratchbird/optimizer/expression_matcher.h"
#include "scratchbird/parser/parser.h"

using namespace scratchbird::optimizer;
using namespace scratchbird::parser;

/**
 * Test suite for ExpressionMatcher (Task 17 Phase 8)
 *
 * Tests expression matching for expression index support in query planner.
 */
class ExpressionMatcherTest : public ::testing::Test
{
protected:
    struct ParseResult {
        std::unique_ptr<Lexer> lexer;
        std::unique_ptr<ASTArena> arena;
        std::unique_ptr<Parser> parser;
        Expression *expr;

        ParseResult() : expr(nullptr) {}
    };

    ParseResult parseExpression(const std::string &expr_str)
    {
        ParseResult result;

        // Parse as part of a SELECT statement
        std::string sql = "SELECT * FROM t WHERE " + expr_str;
        result.lexer = std::make_unique<Lexer>(sql);
        result.arena = std::make_unique<ASTArena>();
        result.parser = std::make_unique<Parser>(*result.lexer, *result.arena);

        auto parse_result = result.parser->parseStatement();
        if (!parse_result.success())
        {
            return result;
        }

        auto *select_stmt = dynamic_cast<SelectStmt *>(parse_result.statement());
        if (!select_stmt || !select_stmt->whereClause())
        {
            return result;
        }

        result.expr = select_stmt->whereClause();
        return result;
    }
};

// ============================================================================
// Exact Match Tests
// ============================================================================

TEST_F(ExpressionMatcherTest, ExactMatchLiteral)
{
    auto result1 = parseExpression("42");
    auto result2 = parseExpression("42");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    // Use result1's StringPool since both expressions should match structurally
    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, ExactMatchIdentifier)
{
    auto result1 = parseExpression("email");
    auto result2 = parseExpression("email");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, ExactMatchFunctionCall)
{
    auto result1 = parseExpression("LOWER(email)");
    auto result2 = parseExpression("LOWER(email)");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, ExactMatchBinaryOp)
{
    auto result1 = parseExpression("price * quantity");
    auto result2 = parseExpression("price * quantity");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, ExactMatchComplex)
{
    auto result1 = parseExpression("(price * 1.1) + tax");
    auto result2 = parseExpression("(price * 1.1) + tax");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

// ============================================================================
// Commutative Operator Tests
// ============================================================================

TEST_F(ExpressionMatcherTest, CommutativeAddition)
{
    auto result1 = parseExpression("a + b");
    auto result2 = parseExpression("b + a");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, CommutativeMultiplication)
{
    auto result1 = parseExpression("price * quantity");
    auto result2 = parseExpression("quantity * price");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, CommutativeEquality)
{
    auto result1 = parseExpression("a = b");
    auto result2 = parseExpression("b = a");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, NonCommutativeSubtraction)
{
    auto result1 = parseExpression("a - b");
    auto result2 = parseExpression("b - a");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                            &result1.parser->stringPool(),
                                            &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, NonCommutativeDivision)
{
    auto result1 = parseExpression("a / b");
    auto result2 = parseExpression("b / a");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                            &result1.parser->stringPool(),
                                            &result2.parser->stringPool()));
}

// ============================================================================
// No Match Tests
// ============================================================================

TEST_F(ExpressionMatcherTest, NoMatchDifferentLiterals)
{
    auto result1 = parseExpression("42");
    auto result2 = parseExpression("99");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                            &result1.parser->stringPool(),
                                            &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, NoMatchDifferentIdentifiers)
{
    auto result1 = parseExpression("email");
    auto result2 = parseExpression("name");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                            &result1.parser->stringPool(),
                                            &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, NoMatchDifferentFunctions)
{
    auto result1 = parseExpression("LOWER(email)");
    auto result2 = parseExpression("UPPER(email)");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                            &result1.parser->stringPool(),
                                            &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, NoMatchDifferentOperators)
{
    auto result1 = parseExpression("a + b");
    auto result2 = parseExpression("a - b");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                            &result1.parser->stringPool(),
                                            &result2.parser->stringPool()));
}

// ============================================================================
// canUse() Tests - Operator Compatibility
// ============================================================================

TEST_F(ExpressionMatcherTest, CanUseExactMatch)
{
    auto result_query = parseExpression("LOWER(email) = 'test@example.com'");
    auto result_index = parseExpression("LOWER(email)");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.expr, result_index.expr,
                                                              &result_query.parser->stringPool(),
                                                              &result_index.parser->stringPool());
    EXPECT_EQ(match_type, ExpressionMatchType::EXACT_MATCH);
}

TEST_F(ExpressionMatcherTest, CanUseRangeScanGreater)
{
    auto result_query = parseExpression("LOWER(email) > 'a'");
    auto result_index = parseExpression("LOWER(email)");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.expr, result_index.expr,
                                                              &result_query.parser->stringPool(),
                                                              &result_index.parser->stringPool());
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}

TEST_F(ExpressionMatcherTest, CanUseRangeScanLess)
{
    auto result_query = parseExpression("LOWER(email) < 'z'");
    auto result_index = parseExpression("LOWER(email)");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.expr, result_index.expr,
                                                              &result_query.parser->stringPool(),
                                                              &result_index.parser->stringPool());
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}

TEST_F(ExpressionMatcherTest, CanUseLikePrefixScan)
{
    auto result_query = parseExpression("LOWER(email) LIKE 'test%'");
    auto result_index = parseExpression("LOWER(email)");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.expr, result_index.expr,
                                                              &result_query.parser->stringPool(),
                                                              &result_index.parser->stringPool());
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}

TEST_F(ExpressionMatcherTest, CannotUseLikeSuffixScan)
{
    auto result_query = parseExpression("LOWER(email) LIKE '%@example.com'");
    auto result_index = parseExpression("LOWER(email)");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.expr, result_index.expr,
                                                              &result_query.parser->stringPool(),
                                                              &result_index.parser->stringPool());
    EXPECT_EQ(match_type, ExpressionMatchType::NO_MATCH);
}

TEST_F(ExpressionMatcherTest, CannotUseDifferentExpression)
{
    auto result_query = parseExpression("UPPER(email) = 'TEST'");
    auto result_index = parseExpression("LOWER(email)");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.expr, result_index.expr,
                                                              &result_query.parser->stringPool(),
                                                              &result_index.parser->stringPool());
    EXPECT_EQ(match_type, ExpressionMatchType::NO_MATCH);
}

// ============================================================================
// Case-Insensitive Matching Tests
// ============================================================================

TEST_F(ExpressionMatcherTest, CaseInsensitiveColumnNames)
{
    auto result1 = parseExpression("Email");
    auto result2 = parseExpression("email");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, CaseInsensitiveFunctionNames)
{
    auto result1 = parseExpression("LOWER(email)");
    auto result2 = parseExpression("lower(email)");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

// ============================================================================
// Complex Expression Tests
// ============================================================================

TEST_F(ExpressionMatcherTest, ComplexArithmeticExpression)
{
    auto result1 = parseExpression("(price * quantity) + tax");
    auto result2 = parseExpression("(price * quantity) + tax");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, NestedFunctionCalls)
{
    auto result1 = parseExpression("LOWER(TRIM(email))");
    auto result2 = parseExpression("LOWER(TRIM(email))");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, MultipleArguments)
{
    auto result1 = parseExpression("SUBSTRING(email, 1, 10)");
    auto result2 = parseExpression("SUBSTRING(email, 1, 10)");

    ASSERT_NE(result1.expr, nullptr);
    ASSERT_NE(result2.expr, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.expr, result2.expr,
                                           &result1.parser->stringPool(),
                                           &result2.parser->stringPool()));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ExpressionMatcherTest, NullExpressions)
{
    StringPool pool;
    EXPECT_FALSE(ExpressionMatcher::matches(nullptr, nullptr, &pool));
}

TEST_F(ExpressionMatcherTest, OneNullExpression)
{
    auto result = parseExpression("email");

    EXPECT_FALSE(ExpressionMatcher::matches(result.expr, nullptr, &result.parser->stringPool()));
    EXPECT_FALSE(ExpressionMatcher::matches(nullptr, result.expr, &result.parser->stringPool()));
}

TEST_F(ExpressionMatcherTest, NullStringPool)
{
    auto result1 = parseExpression("email");
    auto result2 = parseExpression("email");

    EXPECT_FALSE(ExpressionMatcher::matches(result1.expr, result2.expr, nullptr));
}

// ============================================================================
// Real-World Use Cases
// ============================================================================

TEST_F(ExpressionMatcherTest, RealWorldLowerEmailIndex)
{
    auto result_query = parseExpression("LOWER(email) = 'test@example.com'");
    auto result_index = parseExpression("LOWER(email)");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.expr, result_index.expr,
                                                              &result_query.parser->stringPool(),
                                                              &result_index.parser->stringPool());
    EXPECT_EQ(match_type, ExpressionMatchType::EXACT_MATCH);
}

TEST_F(ExpressionMatcherTest, RealWorldExtractYearIndex)
{
    auto result_query = parseExpression("EXTRACT(YEAR FROM created_at) = 2024");
    auto result_index = parseExpression("EXTRACT(YEAR FROM created_at)");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.expr, result_index.expr,
                                                              &result_query.parser->stringPool(),
                                                              &result_index.parser->stringPool());
    EXPECT_EQ(match_type, ExpressionMatchType::EXACT_MATCH);
}

TEST_F(ExpressionMatcherTest, RealWorldComputedPriceIndex)
{
    auto result_query = parseExpression("(price * quantity) > 1000");
    auto result_index = parseExpression("price * quantity");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.expr, result_index.expr,
                                                              &result_query.parser->stringPool(),
                                                              &result_index.parser->stringPool());
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}
