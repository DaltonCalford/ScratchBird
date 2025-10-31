#include <gtest/gtest.h>
#include <memory>
#include "scratchbird/optimizer/predicate_matcher.h"
#include "scratchbird/parser/parser.h"

using namespace scratchbird::optimizer;
using namespace scratchbird::parser;

/**
 * Test suite for PredicateMatcher (Task 17 Phase 9)
 *
 * Tests predicate implication logic for filtered index support in query planner.
 */
class PredicateMatcherTest : public ::testing::Test
{
protected:
    struct ParseResult {
        std::unique_ptr<Lexer> lexer;
        std::unique_ptr<ASTArena> arena;
        std::unique_ptr<Parser> parser;
        Expression *expr;

        ParseResult() : expr(nullptr) {}
    };

    ParseResult parsePredicate(const std::string &pred_str)
    {
        ParseResult result;

        // Parse as part of a SELECT statement
        std::string sql = "SELECT * FROM t WHERE " + pred_str;
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

TEST_F(PredicateMatcherTest, ExactMatchEquality)
{
    auto result_query = parsePredicate("status = 'active'");
    auto result_index = parsePredicate("status = 'active'");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, ExactMatchComparison)
{
    auto result_query = parsePredicate("age > 18");
    auto result_index = parsePredicate("age > 18");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

// ============================================================================
// Range Implication Tests
// ============================================================================

TEST_F(PredicateMatcherTest, RangeImplicationGreater)
{
    auto result_query = parsePredicate("age > 30");
    auto result_index = parsePredicate("age > 18");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, RangeImplicationLess)
{
    auto result_query = parsePredicate("age < 10");
    auto result_index = parsePredicate("age < 20");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, RangeImplicationGreaterEqual)
{
    auto result_query = parsePredicate("age >= 30");
    auto result_index = parsePredicate("age >= 18");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, RangeImplicationLessEqual)
{
    auto result_query = parsePredicate("age <= 10");
    auto result_index = parsePredicate("age <= 20");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, RangeDoesNotImplyWeaker)
{
    auto result_query = parsePredicate("age > 18");
    auto result_index = parsePredicate("age > 30");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_FALSE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

// ============================================================================
// Equality Implies Range Tests
// ============================================================================

TEST_F(PredicateMatcherTest, EqualityImpliesGreater)
{
    auto result_query = parsePredicate("price = 100");
    auto result_index = parsePredicate("price > 50");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, EqualityImpliesLess)
{
    auto result_query = parsePredicate("price = 50");
    auto result_index = parsePredicate("price < 100");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, EqualityImpliesGreaterEqual)
{
    auto result_query = parsePredicate("price = 100");
    auto result_index = parsePredicate("price >= 50");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, EqualityImpliesLessEqual)
{
    auto result_query = parsePredicate("price = 50");
    auto result_index = parsePredicate("price <= 100");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, EqualityDoesNotImplyWrongRange)
{
    auto result_query = parsePredicate("price = 25");
    auto result_index = parsePredicate("price > 50");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_FALSE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

// ============================================================================
// Conjunct Detection Tests
// ============================================================================

TEST_F(PredicateMatcherTest, ConjunctInAND)
{
    auto result_query = parsePredicate("active = true AND verified = true");
    auto result_index = parsePredicate("active = true");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, ConjunctInMultipleAND)
{
    auto result_query = parsePredicate("a = 1 AND b = 2 AND c = 3");
    auto result_index = parsePredicate("b = 2");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, ConjunctRightSide)
{
    auto result_query = parsePredicate("a = 1 AND b = 2");
    auto result_index = parsePredicate("b = 2");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, NoConjunctInOR)
{
    auto result_query = parsePredicate("active = true OR verified = true");
    auto result_index = parsePredicate("active = true");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_FALSE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

// ============================================================================
// Different Predicates (No Match)
// ============================================================================

TEST_F(PredicateMatcherTest, DifferentEquality)
{
    auto result_query = parsePredicate("status = 'active'");
    auto result_index = parsePredicate("status = 'inactive'");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_FALSE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, DifferentColumn)
{
    auto result_query = parsePredicate("age > 18");
    auto result_index = parsePredicate("salary > 18");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_FALSE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, CompletelyDifferent)
{
    auto result_query = parsePredicate("email LIKE 'test%'");
    auto result_index = parsePredicate("age > 18");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_FALSE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

// ============================================================================
// containsConjunct() Tests
// ============================================================================

TEST_F(PredicateMatcherTest, ContainsConjunctSimple)
{
    auto result_query = parsePredicate("a = 1 AND b = 2");
    auto result_index = parsePredicate("a = 1");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::containsConjunct(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, ContainsConjunctNested)
{
    auto result_query = parsePredicate("a = 1 AND b = 2 AND c = 3");
    auto result_index = parsePredicate("c = 3");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::containsConjunct(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, DoesNotContainConjunct)
{
    auto result_query = parsePredicate("a = 1 AND b = 2");
    auto result_index = parsePredicate("c = 3");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_FALSE(PredicateMatcher::containsConjunct(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(PredicateMatcherTest, NullPredicates)
{
    StringPool pool;
    EXPECT_FALSE(PredicateMatcher::implies(nullptr, nullptr, &pool));
}

TEST_F(PredicateMatcherTest, OneNullPredicate)
{
    auto result = parsePredicate("status = 'active'");

    EXPECT_FALSE(PredicateMatcher::implies(result.expr, nullptr, &result.parser->stringPool()));
    EXPECT_FALSE(PredicateMatcher::implies(nullptr, result.expr, &result.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, NullStringPool)
{
    auto result_query = parsePredicate("status = 'active'");
    auto result_index = parsePredicate("status = 'active'");

    EXPECT_FALSE(PredicateMatcher::implies(result_query.expr, result_index.expr, nullptr));
}

// ============================================================================
// Real-World Use Cases
// ============================================================================

TEST_F(PredicateMatcherTest, RealWorldActiveUsersFilter)
{
    auto result_query = parsePredicate("status = 'active' AND verified = true");
    auto result_index = parsePredicate("status = 'active'");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, RealWorldRecentOrdersFilter)
{
    auto result_query = parsePredicate("created_at > '2024-10-15'");
    auto result_index = parsePredicate("created_at > '2024-10-01'");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, RealWorldExpensiveProductsFilter)
{
    auto result_query = parsePredicate("price = 1200");
    auto result_index = parsePredicate("price > 1000");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, RealWorldCannotUseWrongDateRange)
{
    auto result_query = parsePredicate("created_at > '2024-09-01'");
    auto result_index = parsePredicate("created_at > '2024-10-01'");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_FALSE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

// ============================================================================
// String Comparison Tests
// ============================================================================

TEST_F(PredicateMatcherTest, StringRangeImplication)
{
    auto result_query = parsePredicate("name > 'm'");
    auto result_index = parsePredicate("name > 'a'");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, StringEqualityImpliesRange)
{
    auto result_query = parsePredicate("name = 'test'");
    auto result_index = parsePredicate("name > 'a'");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

// ============================================================================
// Numeric Type Tests
// ============================================================================

TEST_F(PredicateMatcherTest, FloatRangeImplication)
{
    auto result_query = parsePredicate("price > 99.99");
    auto result_index = parsePredicate("price > 50.0");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}

TEST_F(PredicateMatcherTest, IntegerRangeImplication)
{
    auto result_query = parsePredicate("quantity > 100");
    auto result_index = parsePredicate("quantity > 10");

    ASSERT_NE(result_query.expr, nullptr);
    ASSERT_NE(result_index.expr, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(result_query.expr, result_index.expr, &result_query.parser->stringPool()));
}
