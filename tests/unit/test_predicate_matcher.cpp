#include <gtest/gtest.h>
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
    Expression *parsePredicate(const std::string &pred_str, StringPool &pool)
    {
        // Parse as part of a SELECT statement
        std::string sql = "SELECT * FROM t WHERE " + pred_str;
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

TEST_F(PredicateMatcherTest, ExactMatchEquality)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("status = 'active'", pool1);
    auto *index_pred = parsePredicate("status = 'active'", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, ExactMatchComparison)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("age > 18", pool1);
    auto *index_pred = parsePredicate("age > 18", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

// ============================================================================
// Range Implication Tests
// ============================================================================

TEST_F(PredicateMatcherTest, RangeImplicationGreater)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("age > 30", pool1);
    auto *index_pred = parsePredicate("age > 18", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // age > 30 implies age > 18 (stricter implies weaker)
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, RangeImplicationLess)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("age < 10", pool1);
    auto *index_pred = parsePredicate("age < 20", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // age < 10 implies age < 20 (stricter implies weaker)
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, RangeImplicationGreaterEqual)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("age >= 30", pool1);
    auto *index_pred = parsePredicate("age >= 18", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, RangeImplicationLessEqual)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("age <= 10", pool1);
    auto *index_pred = parsePredicate("age <= 20", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, RangeDoesNotImplyWeaker)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("age > 18", pool1);
    auto *index_pred = parsePredicate("age > 30", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // age > 18 does NOT imply age > 30
    EXPECT_FALSE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

// ============================================================================
// Equality Implies Range Tests
// ============================================================================

TEST_F(PredicateMatcherTest, EqualityImpliesGreater)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("price = 100", pool1);
    auto *index_pred = parsePredicate("price > 50", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // price = 100 implies price > 50
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, EqualityImpliesLess)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("price = 50", pool1);
    auto *index_pred = parsePredicate("price < 100", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // price = 50 implies price < 100
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, EqualityImpliesGreaterEqual)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("price = 100", pool1);
    auto *index_pred = parsePredicate("price >= 50", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, EqualityImpliesLessEqual)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("price = 50", pool1);
    auto *index_pred = parsePredicate("price <= 100", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, EqualityDoesNotImplyWrongRange)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("price = 25", pool1);
    auto *index_pred = parsePredicate("price > 50", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // price = 25 does NOT imply price > 50
    EXPECT_FALSE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

// ============================================================================
// Conjunct Detection Tests
// ============================================================================

TEST_F(PredicateMatcherTest, ConjunctInAND)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("active = true AND verified = true", pool1);
    auto *index_pred = parsePredicate("active = true", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // (active = true AND verified = true) implies active = true
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, ConjunctInMultipleAND)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("a = 1 AND b = 2 AND c = 3", pool1);
    auto *index_pred = parsePredicate("b = 2", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // (a = 1 AND b = 2 AND c = 3) implies b = 2
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, ConjunctRightSide)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("a = 1 AND b = 2", pool1);
    auto *index_pred = parsePredicate("b = 2", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, NoConjunctInOR)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("active = true OR verified = true", pool1);
    auto *index_pred = parsePredicate("active = true", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // (active = true OR verified = true) does NOT imply active = true
    EXPECT_FALSE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

// ============================================================================
// Different Predicates (No Match)
// ============================================================================

TEST_F(PredicateMatcherTest, DifferentEquality)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("status = 'active'", pool1);
    auto *index_pred = parsePredicate("status = 'inactive'", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_FALSE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, DifferentColumn)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("age > 18", pool1);
    auto *index_pred = parsePredicate("salary > 18", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_FALSE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, CompletelyDifferent)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("email LIKE 'test%'", pool1);
    auto *index_pred = parsePredicate("age > 18", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_FALSE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

// ============================================================================
// containsConjunct() Tests
// ============================================================================

TEST_F(PredicateMatcherTest, ContainsConjunctSimple)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("a = 1 AND b = 2", pool1);
    auto *index_pred = parsePredicate("a = 1", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::containsConjunct(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, ContainsConjunctNested)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("a = 1 AND b = 2 AND c = 3", pool1);
    auto *index_pred = parsePredicate("c = 3", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::containsConjunct(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, DoesNotContainConjunct)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("a = 1 AND b = 2", pool1);
    auto *index_pred = parsePredicate("c = 3", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_FALSE(PredicateMatcher::containsConjunct(query_pred, index_pred, &pool1));
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
    StringPool pool;
    auto *pred = parsePredicate("status = 'active'", pool);

    EXPECT_FALSE(PredicateMatcher::implies(pred, nullptr, &pool));
    EXPECT_FALSE(PredicateMatcher::implies(nullptr, pred, &pool));
}

TEST_F(PredicateMatcherTest, NullStringPool)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("status = 'active'", pool1);
    auto *index_pred = parsePredicate("status = 'active'", pool2);

    EXPECT_FALSE(PredicateMatcher::implies(query_pred, index_pred, nullptr));
}

// ============================================================================
// Real-World Use Cases
// ============================================================================

TEST_F(PredicateMatcherTest, RealWorldActiveUsersFilter)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("status = 'active' AND verified = true", pool1);
    auto *index_pred = parsePredicate("status = 'active'", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // Query with both status = 'active' AND verified can use index with WHERE status = 'active'
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, RealWorldRecentOrdersFilter)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("created_at > '2024-10-15'", pool1);
    auto *index_pred = parsePredicate("created_at > '2024-10-01'", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // created_at > '2024-10-15' implies created_at > '2024-10-01'
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, RealWorldExpensiveProductsFilter)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("price = 1200", pool1);
    auto *index_pred = parsePredicate("price > 1000", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // price = 1200 implies price > 1000
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, RealWorldCannotUseWrongDateRange)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("created_at > '2024-09-01'", pool1);
    auto *index_pred = parsePredicate("created_at > '2024-10-01'", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // created_at > '2024-09-01' does NOT imply created_at > '2024-10-01'
    EXPECT_FALSE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

// ============================================================================
// String Comparison Tests
// ============================================================================

TEST_F(PredicateMatcherTest, StringRangeImplication)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("name > 'm'", pool1);
    auto *index_pred = parsePredicate("name > 'a'", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // name > 'm' implies name > 'a'
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, StringEqualityImpliesRange)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("name = 'test'", pool1);
    auto *index_pred = parsePredicate("name > 'a'", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    // name = 'test' implies name > 'a'
    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

// ============================================================================
// Numeric Type Tests
// ============================================================================

TEST_F(PredicateMatcherTest, FloatRangeImplication)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("price > 99.99", pool1);
    auto *index_pred = parsePredicate("price > 50.0", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

TEST_F(PredicateMatcherTest, IntegerRangeImplication)
{
    StringPool pool1, pool2;
    auto *query_pred = parsePredicate("quantity > 100", pool1);
    auto *index_pred = parsePredicate("quantity > 10", pool2);

    ASSERT_NE(query_pred, nullptr);
    ASSERT_NE(index_pred, nullptr);

    EXPECT_TRUE(PredicateMatcher::implies(query_pred, index_pred, &pool1));
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
