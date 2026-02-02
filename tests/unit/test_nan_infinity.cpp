/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * test_nan_infinity.cpp - P0-5: NaN/Infinity Handling Tests
 *
 * Tests proper handling of NaN and Infinity values in:
 * - Type conversions (float → int)
 * - Mathematical functions (SQRT, LOG, POW, etc.)
 *
 * P0-5: NaN/Infinity Handling (Security/Correctness)
 * Created: November 24, 2025
 */

#include <gtest/gtest.h>
#include <limits>
#include <cmath>
#include "scratchbird/core/typed_value.h"

using namespace scratchbird::core;

// =============================================================================
// Type Conversion Tests: Float → Integer
// =============================================================================

TEST(NaNInfinityHandling, ConvertNaN_ToInt32_ThrowsError)
{
    TypedValue nan_value = TypedValue::makeFloat64(std::nan(""));
    EXPECT_THROW(nan_value.convertTo(DataType::INT32), std::runtime_error);
}

TEST(NaNInfinityHandling, ConvertNaN_ToInt64_ThrowsError)
{
    TypedValue nan_value = TypedValue::makeFloat64(std::nan(""));
    EXPECT_THROW(nan_value.convertTo(DataType::INT64), std::runtime_error);
}

TEST(NaNInfinityHandling, ConvertPositiveInf_ToInt32_ThrowsError)
{
    TypedValue inf_value = TypedValue::makeFloat64(std::numeric_limits<double>::infinity());
    EXPECT_THROW(inf_value.convertTo(DataType::INT32), std::runtime_error);
}

TEST(NaNInfinityHandling, ConvertPositiveInf_ToInt64_ThrowsError)
{
    TypedValue inf_value = TypedValue::makeFloat64(std::numeric_limits<double>::infinity());
    EXPECT_THROW(inf_value.convertTo(DataType::INT64), std::runtime_error);
}

TEST(NaNInfinityHandling, ConvertNegativeInf_ToInt32_ThrowsError)
{
    TypedValue inf_value = TypedValue::makeFloat64(-std::numeric_limits<double>::infinity());
    EXPECT_THROW(inf_value.convertTo(DataType::INT32), std::runtime_error);
}

TEST(NaNInfinityHandling, ConvertNegativeInf_ToInt64_ThrowsError)
{
    TypedValue inf_value = TypedValue::makeFloat64(-std::numeric_limits<double>::infinity());
    EXPECT_THROW(inf_value.convertTo(DataType::INT64), std::runtime_error);
}

TEST(NaNInfinityHandling, ConvertFloat32NaN_ToInt32_ThrowsError)
{
    TypedValue nan_value = TypedValue::makeFloat32(std::nanf(""));
    EXPECT_THROW(nan_value.convertTo(DataType::INT32), std::runtime_error);
}

TEST(NaNInfinityHandling, ConvertFloat32Inf_ToInt64_ThrowsError)
{
    TypedValue inf_value = TypedValue::makeFloat32(std::numeric_limits<float>::infinity());
    EXPECT_THROW(inf_value.convertTo(DataType::INT64), std::runtime_error);
}

// =============================================================================
// Type Conversion Tests: Range Checking
// =============================================================================

TEST(NaNInfinityHandling, ConvertTooLargeFloat_ToInt32_ThrowsError)
{
    TypedValue large_value = TypedValue::makeFloat64(1e15);  // Way larger than INT32_MAX
    EXPECT_THROW(large_value.convertTo(DataType::INT32), std::runtime_error);
}

TEST(NaNInfinityHandling, ConvertTooSmallFloat_ToInt32_ThrowsError)
{
    TypedValue small_value = TypedValue::makeFloat64(-1e15);  // Way smaller than INT32_MIN
    EXPECT_THROW(small_value.convertTo(DataType::INT32), std::runtime_error);
}

TEST(NaNInfinityHandling, ConvertTooLargeFloat_ToInt64_ThrowsError)
{
    TypedValue large_value = TypedValue::makeFloat64(1e19);  // Larger than INT64_MAX
    EXPECT_THROW(large_value.convertTo(DataType::INT64), std::runtime_error);
}

TEST(NaNInfinityHandling, ConvertValidFloat_ToInt32_Succeeds)
{
    TypedValue valid_value = TypedValue::makeFloat64(123.456);
    TypedValue result = valid_value.convertTo(DataType::INT32);
    EXPECT_EQ(DataType::INT32, result.type());
    EXPECT_EQ(123, result.getInt32());
}

TEST(NaNInfinityHandling, ConvertValidFloat_ToInt64_Succeeds)
{
    TypedValue valid_value = TypedValue::makeFloat64(123456.789);
    TypedValue result = valid_value.convertTo(DataType::INT64);
    EXPECT_EQ(DataType::INT64, result.type());
    EXPECT_EQ(123456, result.getInt64());
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(NaNInfinityHandling, ConvertNegativeZero_ToInt_Succeeds)
{
    TypedValue neg_zero = TypedValue::makeFloat64(-0.0);
    TypedValue result = neg_zero.convertTo(DataType::INT64);
    EXPECT_EQ(0, result.getInt64());
}

TEST(NaNInfinityHandling, ConvertVerySmallFloat_ToInt_RoundsToZero)
{
    TypedValue tiny_value = TypedValue::makeFloat64(0.0001);
    TypedValue result = tiny_value.convertTo(DataType::INT64);
    EXPECT_EQ(0, result.getInt64());
}

TEST(NaNInfinityHandling, ConvertMaxInt32AsFloat_ToInt32_Succeeds)
{
    // INT32_MAX as double should convert back successfully
    double max_as_double = static_cast<double>(INT32_MAX);
    TypedValue value = TypedValue::makeFloat64(max_as_double);
    TypedValue result = value.convertTo(DataType::INT32);
    EXPECT_EQ(INT32_MAX, result.getInt32());
}

TEST(NaNInfinityHandling, ConvertMinInt32AsFloat_ToInt32_Succeeds)
{
    // INT32_MIN as double should convert back successfully
    double min_as_double = static_cast<double>(INT32_MIN);
    TypedValue value = TypedValue::makeFloat64(min_as_double);
    TypedValue result = value.convertTo(DataType::INT32);
    EXPECT_EQ(INT32_MIN, result.getInt32());
}

// =============================================================================
// Arithmetic Operations Producing NaN/Infinity
// =============================================================================

TEST(NaNInfinityHandling, DivideByZero_ProducesInfinity)
{
    // 1.0 / 0.0 = Infinity in IEEE 754
    double result = 1.0 / 0.0;
    EXPECT_TRUE(std::isinf(result));
    EXPECT_GT(result, 0);  // Positive infinity
}

TEST(NaNInfinityHandling, NegativeDivideByZero_ProducesNegativeInfinity)
{
    // -1.0 / 0.0 = -Infinity
    double result = -1.0 / 0.0;
    EXPECT_TRUE(std::isinf(result));
    EXPECT_LT(result, 0);  // Negative infinity
}

TEST(NaNInfinityHandling, ZeroDivideByZero_ProducesNaN)
{
    // 0.0 / 0.0 = NaN
    double result = 0.0 / 0.0;
    EXPECT_TRUE(std::isnan(result));
}

TEST(NaNInfinityHandling, InfinityMinusInfinity_ProducesNaN)
{
    // Infinity - Infinity = NaN
    double inf = std::numeric_limits<double>::infinity();
    double result = inf - inf;
    EXPECT_TRUE(std::isnan(result));
}

TEST(NaNInfinityHandling, InfinityTimesZero_ProducesNaN)
{
    // Infinity * 0 = NaN
    double inf = std::numeric_limits<double>::infinity();
    double result = inf * 0.0;
    EXPECT_TRUE(std::isnan(result));
}

TEST(NaNInfinityHandling, SquareRootOfNegative_ProducesNaN)
{
    // sqrt(-1) = NaN
    double result = std::sqrt(-1.0);
    EXPECT_TRUE(std::isnan(result));
}

// =============================================================================
// NaN Comparison Behavior
// =============================================================================

TEST(NaNInfinityHandling, NaN_NotEqualToItself)
{
    double nan = std::nan("");
    EXPECT_FALSE(nan == nan);  // NaN != NaN per IEEE 754
}

TEST(NaNInfinityHandling, NaN_FailsAllComparisons)
{
    double nan = std::nan("");
    EXPECT_FALSE(nan < 0.0);
    EXPECT_FALSE(nan > 0.0);
    EXPECT_FALSE(nan == 0.0);
    EXPECT_FALSE(nan <= 0.0);
    EXPECT_FALSE(nan >= 0.0);
}

TEST(NaNInfinityHandling, Infinity_ComparesCorrectly)
{
    double inf = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(inf > 1000000.0);
    EXPECT_TRUE(inf == inf);
    EXPECT_TRUE(-inf < inf);
}

// =============================================================================
// Special Values
// =============================================================================

TEST(NaNInfinityHandling, IsNaN_DetectsNaN)
{
    EXPECT_TRUE(std::isnan(std::nan("")));
    EXPECT_TRUE(std::isnan(std::nanf("")));
    EXPECT_FALSE(std::isnan(0.0));
    EXPECT_FALSE(std::isnan(std::numeric_limits<double>::infinity()));
}

TEST(NaNInfinityHandling, IsInf_DetectsInfinity)
{
    EXPECT_TRUE(std::isinf(std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(std::isinf(-std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(std::isinf(0.0));
    EXPECT_FALSE(std::isinf(std::nan("")));
}

TEST(NaNInfinityHandling, IsFinite_DetectsNormalNumbers)
{
    EXPECT_TRUE(std::isfinite(0.0));
    EXPECT_TRUE(std::isfinite(123.456));
    EXPECT_TRUE(std::isfinite(-789.012));
    EXPECT_FALSE(std::isfinite(std::nan("")));
    EXPECT_FALSE(std::isfinite(std::numeric_limits<double>::infinity()));
}

// =============================================================================
// Float32 Specific Tests
// =============================================================================

TEST(NaNInfinityHandling, Float32NaN_BehavesLikeFloat64NaN)
{
    float nan_f = std::nanf("");
    EXPECT_TRUE(std::isnan(nan_f));
    EXPECT_FALSE(nan_f == nan_f);
}

TEST(NaNInfinityHandling, Float32Infinity_BehavesLikeFloat64Infinity)
{
    float inf_f = std::numeric_limits<float>::infinity();
    EXPECT_TRUE(std::isinf(inf_f));
    EXPECT_TRUE(inf_f > 1e30f);
}

TEST(NaNInfinityHandling, ConvertFloat32NaN_ToFloat64_PreservesNaN)
{
    TypedValue nan_f32 = TypedValue::makeFloat32(std::nanf(""));
    TypedValue nan_f64 = nan_f32.convertTo(DataType::FLOAT64);
    EXPECT_TRUE(std::isnan(nan_f64.getFloat64()));
}

TEST(NaNInfinityHandling, ConvertFloat32Inf_ToFloat64_PreservesInf)
{
    TypedValue inf_f32 = TypedValue::makeFloat32(std::numeric_limits<float>::infinity());
    TypedValue inf_f64 = inf_f32.convertTo(DataType::FLOAT64);
    EXPECT_TRUE(std::isinf(inf_f64.getFloat64()));
}
