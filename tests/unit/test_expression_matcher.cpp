#include <gtest/gtest.h>
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
    Expression *parseExpression(const std::string &expr_str, StringPool &pool)
    {
        // Parse as part of a SELECT statement
        std::string sql = "SELECT * FROM t WHERE " + expr_str;
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        if (!result.success())
        {
            return nullptr;
        }

        auto *select_stmt = dynamic_cast<SelectStmt *>(result.statement());
        if (!select_stmt || !select_stmt->whereClause())
        {
            return nullptr;
        }

        pool = parser.stringPool();
        return select_stmt->whereClause();
    }
};

// ============================================================================
// Exact Match Tests
// ============================================================================

TEST_F(ExpressionMatcherTest, ExactMatchLiteral)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("42", pool1);
    auto *expr2 = parseExpression("42", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, ExactMatchIdentifier)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("email", pool1);
    auto *expr2 = parseExpression("email", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, ExactMatchFunctionCall)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("LOWER(email)", pool1);
    auto *expr2 = parseExpression("LOWER(email)", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, ExactMatchBinaryOp)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("price * quantity", pool1);
    auto *expr2 = parseExpression("price * quantity", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, ExactMatchComplex)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("(price * 1.1) + tax", pool1);
    auto *expr2 = parseExpression("(price * 1.1) + tax", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

// ============================================================================
// Commutative Operator Tests
// ============================================================================

TEST_F(ExpressionMatcherTest, CommutativeAddition)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("a + b", pool1);
    auto *expr2 = parseExpression("b + a", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, CommutativeMultiplication)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("price * quantity", pool1);
    auto *expr2 = parseExpression("quantity * price", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, CommutativeEquality)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("a = b", pool1);
    auto *expr2 = parseExpression("b = a", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, NonCommutativeSubtraction)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("a - b", pool1);
    auto *expr2 = parseExpression("b - a", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, NonCommutativeDivision)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("a / b", pool1);
    auto *expr2 = parseExpression("b / a", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

// ============================================================================
// No Match Tests
// ============================================================================

TEST_F(ExpressionMatcherTest, NoMatchDifferentLiterals)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("42", pool1);
    auto *expr2 = parseExpression("99", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, NoMatchDifferentIdentifiers)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("email", pool1);
    auto *expr2 = parseExpression("name", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, NoMatchDifferentFunctions)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("LOWER(email)", pool1);
    auto *expr2 = parseExpression("UPPER(email)", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, NoMatchDifferentOperators)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("a + b", pool1);
    auto *expr2 = parseExpression("a - b", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_FALSE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

// ============================================================================
// canUse() Tests - Operator Compatibility
// ============================================================================

TEST_F(ExpressionMatcherTest, CanUseExactMatch)
{
    StringPool pool1, pool2;
    auto *query = parseExpression("LOWER(email) = 'test@example.com'", pool1);
    auto *index = parseExpression("LOWER(email)", pool2);

    ASSERT_NE(query, nullptr);
    ASSERT_NE(index, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(query, index, &pool1);
    EXPECT_EQ(match_type, ExpressionMatchType::EXACT_MATCH);
}

TEST_F(ExpressionMatcherTest, CanUseRangeScanGreater)
{
    StringPool pool1, pool2;
    auto *query = parseExpression("LOWER(email) > 'a'", pool1);
    auto *index = parseExpression("LOWER(email)", pool2);

    ASSERT_NE(query, nullptr);
    ASSERT_NE(index, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(query, index, &pool1);
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}

TEST_F(ExpressionMatcherTest, CanUseRangeScanLess)
{
    StringPool pool1, pool2;
    auto *query = parseExpression("LOWER(email) < 'z'", pool1);
    auto *index = parseExpression("LOWER(email)", pool2);

    ASSERT_NE(query, nullptr);
    ASSERT_NE(index, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(query, index, &pool1);
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}

TEST_F(ExpressionMatcherTest, CanUseLikePrefixScan)
{
    StringPool pool1, pool2;
    auto *query = parseExpression("LOWER(email) LIKE 'test%'", pool1);
    auto *index = parseExpression("LOWER(email)", pool2);

    ASSERT_NE(query, nullptr);
    ASSERT_NE(index, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(query, index, &pool1);
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}

TEST_F(ExpressionMatcherTest, CannotUseLikeSuffixScan)
{
    StringPool pool1, pool2;
    auto *query = parseExpression("LOWER(email) LIKE '%@example.com'", pool1);
    auto *index = parseExpression("LOWER(email)", pool2);

    ASSERT_NE(query, nullptr);
    ASSERT_NE(index, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(query, index, &pool1);
    EXPECT_EQ(match_type, ExpressionMatchType::NO_MATCH);
}

TEST_F(ExpressionMatcherTest, CannotUseDifferentExpression)
{
    StringPool pool1, pool2;
    auto *query = parseExpression("UPPER(email) = 'TEST'", pool1);
    auto *index = parseExpression("LOWER(email)", pool2);

    ASSERT_NE(query, nullptr);
    ASSERT_NE(index, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(query, index, &pool1);
    EXPECT_EQ(match_type, ExpressionMatchType::NO_MATCH);
}

// ============================================================================
// Case-Insensitive Matching Tests
// ============================================================================

TEST_F(ExpressionMatcherTest, CaseInsensitiveColumnNames)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("Email", pool1);
    auto *expr2 = parseExpression("email", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, CaseInsensitiveFunctionNames)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("LOWER(email)", pool1);
    auto *expr2 = parseExpression("lower(email)", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

// ============================================================================
// Complex Expression Tests
// ============================================================================

TEST_F(ExpressionMatcherTest, ComplexArithmeticExpression)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("(price * quantity) + tax", pool1);
    auto *expr2 = parseExpression("(price * quantity) + tax", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, NestedFunctionCalls)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("LOWER(TRIM(email))", pool1);
    auto *expr2 = parseExpression("LOWER(TRIM(email))", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
}

TEST_F(ExpressionMatcherTest, MultipleArguments)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("SUBSTRING(email, 1, 10)", pool1);
    auto *expr2 = parseExpression("SUBSTRING(email, 1, 10)", pool2);

    ASSERT_NE(expr1, nullptr);
    ASSERT_NE(expr2, nullptr);

    EXPECT_TRUE(ExpressionMatcher::matches(expr1, expr2, &pool1));
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
    StringPool pool;
    auto *expr = parseExpression("email", pool);

    EXPECT_FALSE(ExpressionMatcher::matches(expr, nullptr, &pool));
    EXPECT_FALSE(ExpressionMatcher::matches(nullptr, expr, &pool));
}

TEST_F(ExpressionMatcherTest, NullStringPool)
{
    StringPool pool1, pool2;
    auto *expr1 = parseExpression("email", pool1);
    auto *expr2 = parseExpression("email", pool2);

    EXPECT_FALSE(ExpressionMatcher::matches(expr1, expr2, nullptr));
}

// ============================================================================
// Real-World Use Cases
// ============================================================================

TEST_F(ExpressionMatcherTest, RealWorldLowerEmailIndex)
{
    StringPool pool1, pool2;
    auto *query = parseExpression("LOWER(email) = 'test@example.com'", pool1);
    auto *index = parseExpression("LOWER(email)", pool2);

    ASSERT_NE(query, nullptr);
    ASSERT_NE(index, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(query, index, &pool1);
    EXPECT_EQ(match_type, ExpressionMatchType::EXACT_MATCH);
}

TEST_F(ExpressionMatcherTest, RealWorldExtractYearIndex)
{
    StringPool pool1, pool2;
    auto *query = parseExpression("EXTRACT(YEAR FROM created_at) = 2024", pool1);
    auto *index = parseExpression("EXTRACT(YEAR FROM created_at)", pool2);

    ASSERT_NE(query, nullptr);
    ASSERT_NE(index, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(query, index, &pool1);
    EXPECT_EQ(match_type, ExpressionMatchType::EXACT_MATCH);
}

TEST_F(ExpressionMatcherTest, RealWorldComputedPriceIndex)
{
    StringPool pool1, pool2;
    auto *query = parseExpression("(price * quantity) > 1000", pool1);
    auto *index = parseExpression("price * quantity", pool2);

    ASSERT_NE(query, nullptr);
    ASSERT_NE(index, nullptr);

    ExpressionMatchType match_type = ExpressionMatcher::canUse(query, index, &pool1);
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
