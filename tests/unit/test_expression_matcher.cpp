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

#include "scratchbird/optimizer/expression_matcher.h"

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

std::unique_ptr<Expression> bin(BinaryOp op, std::unique_ptr<Expression> left,
                                std::unique_ptr<Expression> right)
{
    return std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right));
}

template <typename... Args>
std::vector<std::unique_ptr<Expression>> makeArgs(Args&&... args)
{
    std::vector<std::unique_ptr<Expression>> out;
    out.reserve(sizeof...(Args));
    (out.emplace_back(std::forward<Args>(args)), ...);
    return out;
}

std::unique_ptr<Expression> func(std::string name, std::vector<std::unique_ptr<Expression>> args)
{
    return std::make_unique<FunctionCallExpr>(std::move(name), std::move(args));
}

std::unique_ptr<Expression> extractYear(std::unique_ptr<Expression> source)
{
    return std::make_unique<ExtractExpr>(1, "YEAR", std::move(source));
}

} // namespace

// ============================================================================
// Exact Match Tests
// ============================================================================

TEST(ExpressionMatcherTest, ExactMatchLiteral)
{
    auto result1 = litInt(42);
    auto result2 = litInt(42);

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, ExactMatchIdentifier)
{
    auto result1 = col("email");
    auto result2 = col("email");

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, ExactMatchFunctionCall)
{
    auto result1 = func("LOWER", makeArgs(col("email")));
    auto result2 = func("LOWER", makeArgs(col("email")));

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, ExactMatchBinaryOp)
{
    auto result1 = bin(BinaryOp::MULTIPLY, col("price"), col("quantity"));
    auto result2 = bin(BinaryOp::MULTIPLY, col("price"), col("quantity"));

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, ExactMatchComplex)
{
    auto result1 = bin(BinaryOp::ADD,
                       bin(BinaryOp::MULTIPLY, col("price"), litFloat(1.1)),
                       col("tax"));
    auto result2 = bin(BinaryOp::ADD,
                       bin(BinaryOp::MULTIPLY, col("price"), litFloat(1.1)),
                       col("tax"));

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

// ============================================================================
// Commutative Operator Tests
// ============================================================================

TEST(ExpressionMatcherTest, CommutativeAddition)
{
    auto result1 = bin(BinaryOp::ADD, col("a"), col("b"));
    auto result2 = bin(BinaryOp::ADD, col("b"), col("a"));

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, CommutativeMultiplication)
{
    auto result1 = bin(BinaryOp::MULTIPLY, col("price"), col("quantity"));
    auto result2 = bin(BinaryOp::MULTIPLY, col("quantity"), col("price"));

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, CommutativeEquality)
{
    auto result1 = bin(BinaryOp::EQ, col("a"), col("b"));
    auto result2 = bin(BinaryOp::EQ, col("b"), col("a"));

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, NonCommutativeSubtraction)
{
    auto result1 = bin(BinaryOp::SUBTRACT, col("a"), col("b"));
    auto result2 = bin(BinaryOp::SUBTRACT, col("b"), col("a"));

    EXPECT_FALSE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, NonCommutativeDivision)
{
    auto result1 = bin(BinaryOp::DIVIDE, col("a"), col("b"));
    auto result2 = bin(BinaryOp::DIVIDE, col("b"), col("a"));

    EXPECT_FALSE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

// ============================================================================
// No Match Tests
// ============================================================================

TEST(ExpressionMatcherTest, NoMatchDifferentLiterals)
{
    auto result1 = litInt(42);
    auto result2 = litInt(99);

    EXPECT_FALSE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, NoMatchDifferentIdentifiers)
{
    auto result1 = col("email");
    auto result2 = col("name");

    EXPECT_FALSE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, NoMatchDifferentFunctions)
{
    auto result1 = func("LOWER", makeArgs(col("email")));
    auto result2 = func("UPPER", makeArgs(col("email")));

    EXPECT_FALSE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, NoMatchDifferentOperators)
{
    auto result1 = bin(BinaryOp::ADD, col("a"), col("b"));
    auto result2 = bin(BinaryOp::SUBTRACT, col("a"), col("b"));

    EXPECT_FALSE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

// ============================================================================
// Can Use Tests
// ============================================================================

TEST(ExpressionMatcherTest, CanUseExactMatch)
{
    auto result_query = bin(BinaryOp::EQ,
                            func("LOWER", makeArgs(col("email"))),
                            litString("test@example.com"));
    auto result_index = func("LOWER", makeArgs(col("email")));

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.get(), result_index.get());
    EXPECT_EQ(match_type, ExpressionMatchType::EXACT_MATCH);
}

TEST(ExpressionMatcherTest, CanUseRangeScanGreater)
{
    auto result_query = bin(BinaryOp::GT,
                            func("LOWER", makeArgs(col("email"))),
                            litString("a"));
    auto result_index = func("LOWER", makeArgs(col("email")));

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.get(), result_index.get());
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}

TEST(ExpressionMatcherTest, CanUseRangeScanLess)
{
    auto result_query = bin(BinaryOp::LT,
                            func("LOWER", makeArgs(col("email"))),
                            litString("z"));
    auto result_index = func("LOWER", makeArgs(col("email")));

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.get(), result_index.get());
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}

TEST(ExpressionMatcherTest, CanUseLikePrefixScan)
{
    auto result_query = bin(BinaryOp::LIKE,
                            func("LOWER", makeArgs(col("email"))),
                            litString("test%"));
    auto result_index = func("LOWER", makeArgs(col("email")));

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.get(), result_index.get());
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}

TEST(ExpressionMatcherTest, CannotUseLikeSuffixScan)
{
    auto result_query = bin(BinaryOp::LIKE,
                            func("LOWER", makeArgs(col("email"))),
                            litString("%@example.com"));
    auto result_index = func("LOWER", makeArgs(col("email")));

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.get(), result_index.get());
    EXPECT_EQ(match_type, ExpressionMatchType::NO_MATCH);
}

TEST(ExpressionMatcherTest, CannotUseDifferentExpression)
{
    auto result_query = bin(BinaryOp::EQ,
                            func("UPPER", makeArgs(col("email"))),
                            litString("TEST"));
    auto result_index = func("LOWER", makeArgs(col("email")));

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.get(), result_index.get());
    EXPECT_EQ(match_type, ExpressionMatchType::NO_MATCH);
}

// ============================================================================
// Case Insensitive Matching
// ============================================================================

TEST(ExpressionMatcherTest, CaseInsensitiveColumnNames)
{
    auto result1 = col("Email");
    auto result2 = col("email");

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, CaseInsensitiveFunctionNames)
{
    auto result1 = func("LOWER", makeArgs(col("email")));
    auto result2 = func("lower", makeArgs(col("email")));

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

// ============================================================================
// Complex Expression Tests
// ============================================================================

TEST(ExpressionMatcherTest, ComplexArithmeticExpression)
{
    auto result1 = bin(BinaryOp::ADD,
                       bin(BinaryOp::MULTIPLY, col("price"), col("quantity")),
                       col("tax"));
    auto result2 = bin(BinaryOp::ADD,
                       bin(BinaryOp::MULTIPLY, col("price"), col("quantity")),
                       col("tax"));

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, NestedFunctionCalls)
{
    auto result1 = func("LOWER", makeArgs(func("TRIM", makeArgs(col("email")))));
    auto result2 = func("LOWER", makeArgs(func("TRIM", makeArgs(col("email")))));

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

TEST(ExpressionMatcherTest, MultipleArguments)
{
    auto result1 = func("SUBSTRING", makeArgs(col("email"), litInt(1), litInt(10)));
    auto result2 = func("SUBSTRING", makeArgs(col("email"), litInt(1), litInt(10)));

    EXPECT_TRUE(ExpressionMatcher::matches(result1.get(), result2.get()));
}

// ============================================================================
// Null Checks
// ============================================================================

TEST(ExpressionMatcherTest, NullExpressions)
{
    EXPECT_FALSE(ExpressionMatcher::matches(nullptr, nullptr));
}

TEST(ExpressionMatcherTest, OneNullExpression)
{
    auto result = col("email");
    EXPECT_FALSE(ExpressionMatcher::matches(result.get(), nullptr));
    EXPECT_FALSE(ExpressionMatcher::matches(nullptr, result.get()));
}

// ============================================================================
// Real-World Examples
// ============================================================================

TEST(ExpressionMatcherTest, RealWorldLowerEmailIndex)
{
    auto result_query = bin(BinaryOp::EQ,
                            func("LOWER", makeArgs(col("email"))),
                            litString("test@example.com"));
    auto result_index = func("LOWER", makeArgs(col("email")));

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.get(), result_index.get());
    EXPECT_EQ(match_type, ExpressionMatchType::EXACT_MATCH);
}

TEST(ExpressionMatcherTest, RealWorldExtractYearIndex)
{
    auto result_query = bin(BinaryOp::EQ,
                            extractYear(col("created_at")),
                            litInt(2024));
    auto result_index = extractYear(col("created_at"));

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.get(), result_index.get());
    EXPECT_EQ(match_type, ExpressionMatchType::EXACT_MATCH);
}

TEST(ExpressionMatcherTest, RealWorldComputedPriceIndex)
{
    auto result_query = bin(BinaryOp::GT,
                            bin(BinaryOp::MULTIPLY, col("price"), col("quantity")),
                            litInt(1000));
    auto result_index = bin(BinaryOp::MULTIPLY, col("price"), col("quantity"));

    ExpressionMatchType match_type = ExpressionMatcher::canUse(result_query.get(), result_index.get());
    EXPECT_EQ(match_type, ExpressionMatchType::RANGE_SCAN);
}
