/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "scratchbird/optimizer/predicate_matcher.h"

using namespace scratchbird::optimizer;
using namespace scratchbird::core;

namespace {

std::unique_ptr<Expression> litInt(int64_t value)
{
    auto expr = std::make_unique<LiteralExpr>(LiteralExpr::LiteralType::INTEGER);
    expr->setIntValue(value);
    return expr;
}

std::unique_ptr<Expression> litFloat(double value)
{
    auto expr = std::make_unique<LiteralExpr>(LiteralExpr::LiteralType::FLOAT);
    expr->setFloatValue(value);
    return expr;
}

std::unique_ptr<Expression> litString(std::string value)
{
    auto expr = std::make_unique<LiteralExpr>(LiteralExpr::LiteralType::STRING);
    expr->setStringValue(std::move(value));
    return expr;
}

std::unique_ptr<Expression> col(std::string name)
{
    return std::make_unique<IdentifierExpr>(std::move(name));
}

std::unique_ptr<Expression> bin(BinaryOp op,
                                std::unique_ptr<Expression> left,
                                std::unique_ptr<Expression> right)
{
    return std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right));
}

} // namespace

// ============================================================================
// Exact Match Tests
// ============================================================================

TEST(PredicateMatcherTest, ExactMatchEquality)
{
    auto query = bin(BinaryOp::EQ, col("status"), litString("active"));
    auto index = bin(BinaryOp::EQ, col("status"), litString("active"));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, ExactMatchComparison)
{
    auto query = bin(BinaryOp::GT, col("age"), litInt(18));
    auto index = bin(BinaryOp::GT, col("age"), litInt(18));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

// ============================================================================
// Range Implication Tests
// ============================================================================

TEST(PredicateMatcherTest, RangeImplicationGreater)
{
    auto query = bin(BinaryOp::GT, col("age"), litInt(30));
    auto index = bin(BinaryOp::GT, col("age"), litInt(18));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, RangeImplicationLess)
{
    auto query = bin(BinaryOp::LT, col("age"), litInt(10));
    auto index = bin(BinaryOp::LT, col("age"), litInt(20));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, RangeImplicationGreaterEqual)
{
    auto query = bin(BinaryOp::GE, col("age"), litInt(30));
    auto index = bin(BinaryOp::GE, col("age"), litInt(18));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, RangeImplicationLessEqual)
{
    auto query = bin(BinaryOp::LE, col("age"), litInt(10));
    auto index = bin(BinaryOp::LE, col("age"), litInt(20));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, RangeDoesNotImplyWeaker)
{
    auto query = bin(BinaryOp::GT, col("age"), litInt(18));
    auto index = bin(BinaryOp::GT, col("age"), litInt(30));

    EXPECT_FALSE(PredicateMatcher::implies(query.get(), index.get()));
}

// ============================================================================
// Equality Implies Range Tests
// ============================================================================

TEST(PredicateMatcherTest, EqualityImpliesGreater)
{
    auto query = bin(BinaryOp::EQ, col("price"), litInt(100));
    auto index = bin(BinaryOp::GT, col("price"), litInt(50));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, EqualityImpliesLess)
{
    auto query = bin(BinaryOp::EQ, col("price"), litInt(50));
    auto index = bin(BinaryOp::LT, col("price"), litInt(100));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, EqualityImpliesGreaterEqual)
{
    auto query = bin(BinaryOp::EQ, col("price"), litInt(100));
    auto index = bin(BinaryOp::GE, col("price"), litInt(50));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, EqualityImpliesLessEqual)
{
    auto query = bin(BinaryOp::EQ, col("price"), litInt(50));
    auto index = bin(BinaryOp::LE, col("price"), litInt(100));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, EqualityDoesNotImplyWrongRange)
{
    auto query = bin(BinaryOp::EQ, col("price"), litInt(25));
    auto index = bin(BinaryOp::GT, col("price"), litInt(50));

    EXPECT_FALSE(PredicateMatcher::implies(query.get(), index.get()));
}

// ============================================================================
// Conjunct Detection Tests
// ============================================================================

TEST(PredicateMatcherTest, ConjunctInAND)
{
    auto query = bin(BinaryOp::AND,
                     bin(BinaryOp::EQ, col("is_active"), litInt(1)),
                     bin(BinaryOp::EQ, col("verified"), litInt(1)));
    auto index = bin(BinaryOp::EQ, col("is_active"), litInt(1));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, ConjunctInMultipleAND)
{
    auto query = bin(BinaryOp::AND,
                     bin(BinaryOp::AND,
                         bin(BinaryOp::EQ, col("a"), litInt(1)),
                         bin(BinaryOp::EQ, col("b"), litInt(2))),
                     bin(BinaryOp::EQ, col("c"), litInt(3)));
    auto index = bin(BinaryOp::EQ, col("b"), litInt(2));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, ConjunctRightSide)
{
    auto query = bin(BinaryOp::AND,
                     bin(BinaryOp::EQ, col("a"), litInt(1)),
                     bin(BinaryOp::EQ, col("b"), litInt(2)));
    auto index = bin(BinaryOp::EQ, col("b"), litInt(2));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, NoConjunctInOR)
{
    auto query = bin(BinaryOp::OR,
                     bin(BinaryOp::EQ, col("is_active"), litInt(1)),
                     bin(BinaryOp::EQ, col("verified"), litInt(1)));
    auto index = bin(BinaryOp::EQ, col("is_active"), litInt(1));

    EXPECT_FALSE(PredicateMatcher::implies(query.get(), index.get()));
}

// ============================================================================
// Different Predicates (No Match)
// ============================================================================

TEST(PredicateMatcherTest, DifferentEquality)
{
    auto query = bin(BinaryOp::EQ, col("status"), litString("active"));
    auto index = bin(BinaryOp::EQ, col("status"), litString("inactive"));

    EXPECT_FALSE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, DifferentColumn)
{
    auto query = bin(BinaryOp::GT, col("age"), litInt(18));
    auto index = bin(BinaryOp::GT, col("salary"), litInt(18));

    EXPECT_FALSE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, CompletelyDifferent)
{
    auto query = bin(BinaryOp::LIKE, col("email"), litString("test%"));
    auto index = bin(BinaryOp::GT, col("age"), litInt(18));

    EXPECT_FALSE(PredicateMatcher::implies(query.get(), index.get()));
}

// ============================================================================
// containsConjunct() Tests
// ============================================================================

TEST(PredicateMatcherTest, ContainsConjunctSimple)
{
    auto query = bin(BinaryOp::AND,
                     bin(BinaryOp::EQ, col("a"), litInt(1)),
                     bin(BinaryOp::EQ, col("b"), litInt(2)));
    auto index = bin(BinaryOp::EQ, col("a"), litInt(1));

    EXPECT_TRUE(PredicateMatcher::containsConjunct(query.get(), index.get()));
}

TEST(PredicateMatcherTest, ContainsConjunctNested)
{
    auto query = bin(BinaryOp::AND,
                     bin(BinaryOp::AND,
                         bin(BinaryOp::EQ, col("a"), litInt(1)),
                         bin(BinaryOp::EQ, col("b"), litInt(2))),
                     bin(BinaryOp::EQ, col("c"), litInt(3)));
    auto index = bin(BinaryOp::EQ, col("c"), litInt(3));

    EXPECT_TRUE(PredicateMatcher::containsConjunct(query.get(), index.get()));
}

TEST(PredicateMatcherTest, DoesNotContainConjunct)
{
    auto query = bin(BinaryOp::AND,
                     bin(BinaryOp::EQ, col("a"), litInt(1)),
                     bin(BinaryOp::EQ, col("b"), litInt(2)));
    auto index = bin(BinaryOp::EQ, col("c"), litInt(3));

    EXPECT_FALSE(PredicateMatcher::containsConjunct(query.get(), index.get()));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(PredicateMatcherTest, NullPredicates)
{
    EXPECT_FALSE(PredicateMatcher::implies(nullptr, nullptr));
}

TEST(PredicateMatcherTest, OneNullPredicate)
{
    auto query = bin(BinaryOp::EQ, col("status"), litString("active"));

    EXPECT_FALSE(PredicateMatcher::implies(query.get(), nullptr));
    EXPECT_FALSE(PredicateMatcher::implies(nullptr, query.get()));
}

// ============================================================================
// Real-World Use Cases
// ============================================================================

TEST(PredicateMatcherTest, RealWorldActiveUsersFilter)
{
    auto query = bin(BinaryOp::AND,
                     bin(BinaryOp::EQ, col("status"), litString("active")),
                     bin(BinaryOp::EQ, col("verified"), litInt(1)));
    auto index = bin(BinaryOp::EQ, col("status"), litString("active"));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, RealWorldRecentOrdersFilter)
{
    auto query = bin(BinaryOp::GT, col("created_at"), litString("2024-10-15"));
    auto index = bin(BinaryOp::GT, col("created_at"), litString("2024-10-01"));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, RealWorldExpensiveProductsFilter)
{
    auto query = bin(BinaryOp::EQ, col("price"), litInt(1200));
    auto index = bin(BinaryOp::GT, col("price"), litInt(1000));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, RealWorldCannotUseWrongDateRange)
{
    auto query = bin(BinaryOp::GT, col("created_at"), litString("2024-09-01"));
    auto index = bin(BinaryOp::GT, col("created_at"), litString("2024-10-01"));

    EXPECT_FALSE(PredicateMatcher::implies(query.get(), index.get()));
}

// ============================================================================
// String Comparison Tests
// ============================================================================

TEST(PredicateMatcherTest, StringRangeImplication)
{
    auto query = bin(BinaryOp::GT, col("name"), litString("m"));
    auto index = bin(BinaryOp::GT, col("name"), litString("a"));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, StringEqualityImpliesRange)
{
    auto query = bin(BinaryOp::EQ, col("name"), litString("test"));
    auto index = bin(BinaryOp::GT, col("name"), litString("a"));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

// ============================================================================
// Numeric Type Tests
// ============================================================================

TEST(PredicateMatcherTest, FloatRangeImplication)
{
    auto query = bin(BinaryOp::GT, col("price"), litFloat(99.99));
    auto index = bin(BinaryOp::GT, col("price"), litFloat(50.0));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}

TEST(PredicateMatcherTest, IntegerRangeImplication)
{
    auto query = bin(BinaryOp::GT, col("quantity"), litInt(100));
    auto index = bin(BinaryOp::GT, col("quantity"), litInt(10));

    EXPECT_TRUE(PredicateMatcher::implies(query.get(), index.get()));
}
