/**
 * test_arithmetic_overflow.cpp - P0-4: Arithmetic Overflow Checking Tests
 *
 * Tests safe arithmetic operations for INT64 and INT32 types.
 * Verifies overflow detection using compiler intrinsics.
 *
 * P0-4: Arithmetic Overflow Checking (CWE-190)
 * Created: November 24, 2025
 */

#include <gtest/gtest.h>
#include <limits>
#include "scratchbird/core/safe_arithmetic.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

// =============================================================================
// INT64 Overflow Tests
// =============================================================================

TEST(ArithmeticOverflow, Int64Addition_Overflow_Positive)
{
    int64_t result;
    bool success = safeAddInt64(INT64_MAX, 1, &result);
    EXPECT_FALSE(success) << "INT64_MAX + 1 should overflow";
}

TEST(ArithmeticOverflow, Int64Addition_Overflow_Negative)
{
    int64_t result;
    bool success = safeAddInt64(INT64_MIN, -1, &result);
    EXPECT_FALSE(success) << "INT64_MIN + (-1) should overflow";
}

TEST(ArithmeticOverflow, Int64Addition_NoOverflow)
{
    int64_t result;
    bool success = safeAddInt64(100, 200, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(300, result);
}

TEST(ArithmeticOverflow, Int64Addition_LargeButValid)
{
    int64_t result;
    bool success = safeAddInt64(INT64_MAX - 100, 50, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(INT64_MAX - 50, result);
}

TEST(ArithmeticOverflow, Int64Subtraction_Overflow_Positive)
{
    int64_t result;
    bool success = safeSubtractInt64(INT64_MAX, -1, &result);
    EXPECT_FALSE(success) << "INT64_MAX - (-1) should overflow";
}

TEST(ArithmeticOverflow, Int64Subtraction_Overflow_Negative)
{
    int64_t result;
    bool success = safeSubtractInt64(INT64_MIN, 1, &result);
    EXPECT_FALSE(success) << "INT64_MIN - 1 should overflow";
}

TEST(ArithmeticOverflow, Int64Subtraction_NoOverflow)
{
    int64_t result;
    bool success = safeSubtractInt64(300, 100, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(200, result);
}

TEST(ArithmeticOverflow, Int64Multiplication_Overflow_Positive)
{
    int64_t result;
    bool success = safeMultiplyInt64(INT64_MAX, 2, &result);
    EXPECT_FALSE(success) << "INT64_MAX * 2 should overflow";
}

TEST(ArithmeticOverflow, Int64Multiplication_Overflow_Negative)
{
    int64_t result;
    bool success = safeMultiplyInt64(INT64_MIN, 2, &result);
    EXPECT_FALSE(success) << "INT64_MIN * 2 should overflow";
}

TEST(ArithmeticOverflow, Int64Multiplication_Overflow_Mixed)
{
    int64_t result;
    bool success = safeMultiplyInt64(INT64_MIN, -1, &result);
    EXPECT_FALSE(success) << "INT64_MIN * (-1) should overflow";
}

TEST(ArithmeticOverflow, Int64Multiplication_NoOverflow)
{
    int64_t result;
    bool success = safeMultiplyInt64(100, 200, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(20000, result);
}

TEST(ArithmeticOverflow, Int64Multiplication_Zero)
{
    int64_t result;
    bool success = safeMultiplyInt64(INT64_MAX, 0, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(0, result);
}

TEST(ArithmeticOverflow, Int64Division_ByZero)
{
    ErrorContext ctx;
    int64_t result;
    bool success = safeDivideInt64(100, 0, &result, &ctx);
    EXPECT_FALSE(success);
    EXPECT_EQ(Status::DIVISION_BY_ZERO, ctx.code);
}

TEST(ArithmeticOverflow, Int64Division_Overflow)
{
    ErrorContext ctx;
    int64_t result;
    bool success = safeDivideInt64(INT64_MIN, -1, &result, &ctx);
    EXPECT_FALSE(success) << "INT64_MIN / (-1) should overflow";
    EXPECT_EQ(Status::OUT_OF_RANGE, ctx.code);
}

TEST(ArithmeticOverflow, Int64Division_NoOverflow)
{
    ErrorContext ctx;
    int64_t result;
    bool success = safeDivideInt64(100, 5, &result, &ctx);
    EXPECT_TRUE(success);
    EXPECT_EQ(20, result);
}

TEST(ArithmeticOverflow, Int64Modulo_ByZero)
{
    ErrorContext ctx;
    int64_t result;
    bool success = safeModuloInt64(100, 0, &result, &ctx);
    EXPECT_FALSE(success);
    EXPECT_EQ(Status::DIVISION_BY_ZERO, ctx.code);
}

TEST(ArithmeticOverflow, Int64Modulo_SpecialCase)
{
    ErrorContext ctx;
    int64_t result;
    bool success = safeModuloInt64(INT64_MIN, -1, &result, &ctx);
    EXPECT_TRUE(success) << "INT64_MIN % (-1) = 0 (no overflow)";
    EXPECT_EQ(0, result);
}

TEST(ArithmeticOverflow, Int64Modulo_NoOverflow)
{
    ErrorContext ctx;
    int64_t result;
    bool success = safeModuloInt64(100, 7, &result, &ctx);
    EXPECT_TRUE(success);
    EXPECT_EQ(2, result);
}

TEST(ArithmeticOverflow, Int64Negate_Overflow)
{
    ErrorContext ctx;
    int64_t result;
    bool success = safeNegateInt64(INT64_MIN, &result, &ctx);
    EXPECT_FALSE(success) << "-INT64_MIN should overflow";
    EXPECT_EQ(Status::OUT_OF_RANGE, ctx.code);
}

TEST(ArithmeticOverflow, Int64Negate_NoOverflow)
{
    ErrorContext ctx;
    int64_t result;
    bool success = safeNegateInt64(12345, &result, &ctx);
    EXPECT_TRUE(success);
    EXPECT_EQ(-12345, result);
}

// =============================================================================
// INT32 Overflow Tests
// =============================================================================

TEST(ArithmeticOverflow, Int32Addition_Overflow_Positive)
{
    int32_t result;
    bool success = safeAddInt32(INT32_MAX, 1, &result);
    EXPECT_FALSE(success) << "INT32_MAX + 1 should overflow";
}

TEST(ArithmeticOverflow, Int32Addition_Overflow_Negative)
{
    int32_t result;
    bool success = safeAddInt32(INT32_MIN, -1, &result);
    EXPECT_FALSE(success) << "INT32_MIN + (-1) should overflow";
}

TEST(ArithmeticOverflow, Int32Addition_NoOverflow)
{
    int32_t result;
    bool success = safeAddInt32(100, 200, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(300, result);
}

TEST(ArithmeticOverflow, Int32Subtraction_Overflow_Positive)
{
    int32_t result;
    bool success = safeSubtractInt32(INT32_MAX, -1, &result);
    EXPECT_FALSE(success) << "INT32_MAX - (-1) should overflow";
}

TEST(ArithmeticOverflow, Int32Subtraction_Overflow_Negative)
{
    int32_t result;
    bool success = safeSubtractInt32(INT32_MIN, 1, &result);
    EXPECT_FALSE(success) << "INT32_MIN - 1 should overflow";
}

TEST(ArithmeticOverflow, Int32Subtraction_NoOverflow)
{
    int32_t result;
    bool success = safeSubtractInt32(300, 100, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(200, result);
}

TEST(ArithmeticOverflow, Int32Multiplication_Overflow_Positive)
{
    int32_t result;
    bool success = safeMultiplyInt32(INT32_MAX, 2, &result);
    EXPECT_FALSE(success) << "INT32_MAX * 2 should overflow";
}

TEST(ArithmeticOverflow, Int32Multiplication_NoOverflow)
{
    int32_t result;
    bool success = safeMultiplyInt32(100, 200, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(20000, result);
}

TEST(ArithmeticOverflow, Int32Division_ByZero)
{
    ErrorContext ctx;
    int32_t result;
    bool success = safeDivideInt32(100, 0, &result, &ctx);
    EXPECT_FALSE(success);
    EXPECT_EQ(Status::DIVISION_BY_ZERO, ctx.code);
}

TEST(ArithmeticOverflow, Int32Division_Overflow)
{
    ErrorContext ctx;
    int32_t result;
    bool success = safeDivideInt32(INT32_MIN, -1, &result, &ctx);
    EXPECT_FALSE(success) << "INT32_MIN / (-1) should overflow";
    EXPECT_EQ(Status::OUT_OF_RANGE, ctx.code);
}

TEST(ArithmeticOverflow, Int32Negate_Overflow)
{
    ErrorContext ctx;
    int32_t result;
    bool success = safeNegateInt32(INT32_MIN, &result, &ctx);
    EXPECT_FALSE(success) << "-INT32_MIN should overflow";
    EXPECT_EQ(Status::OUT_OF_RANGE, ctx.code);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(ArithmeticOverflow, Int64_NearBoundary_Add)
{
    int64_t result;

    // Just under max
    EXPECT_TRUE(safeAddInt64(INT64_MAX - 1, 1, &result));
    EXPECT_EQ(INT64_MAX, result);

    // Just over max
    EXPECT_FALSE(safeAddInt64(INT64_MAX - 1, 2, &result));
}

TEST(ArithmeticOverflow, Int64_NearBoundary_Subtract)
{
    int64_t result;

    // Just above min
    EXPECT_TRUE(safeSubtractInt64(INT64_MIN + 1, 1, &result));
    EXPECT_EQ(INT64_MIN, result);

    // Just below min
    EXPECT_FALSE(safeSubtractInt64(INT64_MIN + 1, 2, &result));
}

TEST(ArithmeticOverflow, Int64_LargeMultiplication)
{
    int64_t result;

    // Sqrt(INT64_MAX) ≈ 3.037e9
    int64_t sqrt_max = 3037000499LL;

    // Should succeed
    EXPECT_TRUE(safeMultiplyInt64(sqrt_max, sqrt_max, &result));

    // Should fail
    EXPECT_FALSE(safeMultiplyInt64(sqrt_max, sqrt_max + 1, &result));
}
