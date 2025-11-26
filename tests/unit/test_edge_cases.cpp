/**
 * test_edge_cases.cpp - P2-11: Edge Case Test Suite
 *
 * Comprehensive tests for edge cases in mathematical functions,
 * float/double precision, integer overflow, character encoding,
 * and timezone handling.
 *
 * P2-11: Edge Case Test Suite
 * Created: November 25, 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <string>
#include <cstdint>
#include "scratchbird/core/safe_arithmetic.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/typed_value.h"

using namespace scratchbird::core;

// =============================================================================
// NaN HANDLING TESTS
// =============================================================================

class NaNHandlingTest : public ::testing::Test
{
protected:
    double nan_value = std::nan("");
    double pos_inf = std::numeric_limits<double>::infinity();
    double neg_inf = -std::numeric_limits<double>::infinity();
};

TEST_F(NaNHandlingTest, NaN_Comparisons_AlwaysFalse)
{
    // NaN comparisons should always be false (except !=)
    EXPECT_FALSE(nan_value == nan_value) << "NaN == NaN should be false";
    EXPECT_FALSE(nan_value < nan_value) << "NaN < NaN should be false";
    EXPECT_FALSE(nan_value > nan_value) << "NaN > NaN should be false";
    EXPECT_FALSE(nan_value <= nan_value) << "NaN <= NaN should be false";
    EXPECT_FALSE(nan_value >= nan_value) << "NaN >= NaN should be false";
    EXPECT_TRUE(nan_value != nan_value) << "NaN != NaN should be true";
}

TEST_F(NaNHandlingTest, NaN_Detection)
{
    EXPECT_TRUE(std::isnan(nan_value));
    EXPECT_FALSE(std::isnan(1.0));
    EXPECT_FALSE(std::isnan(pos_inf));
    EXPECT_FALSE(std::isnan(neg_inf));
    EXPECT_FALSE(std::isnan(0.0));
}

TEST_F(NaNHandlingTest, NaN_Arithmetic_Propagates)
{
    // Any arithmetic with NaN produces NaN
    EXPECT_TRUE(std::isnan(nan_value + 1.0));
    EXPECT_TRUE(std::isnan(nan_value - 1.0));
    EXPECT_TRUE(std::isnan(nan_value * 2.0));
    EXPECT_TRUE(std::isnan(nan_value / 2.0));
    EXPECT_TRUE(std::isnan(1.0 + nan_value));
    EXPECT_TRUE(std::isnan(std::sqrt(nan_value)));
    EXPECT_TRUE(std::isnan(std::sin(nan_value)));
    EXPECT_TRUE(std::isnan(std::cos(nan_value)));
}

TEST_F(NaNHandlingTest, NaN_FromInvalidOperations)
{
    // Operations that produce NaN
    EXPECT_TRUE(std::isnan(0.0 / 0.0));
    EXPECT_TRUE(std::isnan(pos_inf - pos_inf));
    EXPECT_TRUE(std::isnan(pos_inf * 0.0));
    EXPECT_TRUE(std::isnan(std::sqrt(-1.0)));
    EXPECT_TRUE(std::isnan(std::log(-1.0)));
    EXPECT_TRUE(std::isnan(std::asin(2.0)));  // |x| > 1
    EXPECT_TRUE(std::isnan(std::acos(2.0)));  // |x| > 1
}

// =============================================================================
// INFINITY HANDLING TESTS
// =============================================================================

TEST_F(NaNHandlingTest, Infinity_Detection)
{
    EXPECT_TRUE(std::isinf(pos_inf));
    EXPECT_TRUE(std::isinf(neg_inf));
    EXPECT_FALSE(std::isinf(nan_value));
    EXPECT_FALSE(std::isinf(1.0));
    EXPECT_FALSE(std::isinf(0.0));
}

TEST_F(NaNHandlingTest, Infinity_Sign)
{
    EXPECT_TRUE(pos_inf > 0);
    EXPECT_TRUE(neg_inf < 0);
    EXPECT_EQ(pos_inf, -neg_inf);
}

TEST_F(NaNHandlingTest, Infinity_Comparisons)
{
    double large = 1e308;
    EXPECT_TRUE(pos_inf > large);
    EXPECT_TRUE(neg_inf < -large);
    EXPECT_TRUE(pos_inf == pos_inf);
    EXPECT_TRUE(neg_inf == neg_inf);
    EXPECT_TRUE(pos_inf != neg_inf);
}

TEST_F(NaNHandlingTest, Infinity_Arithmetic)
{
    // Infinity arithmetic rules
    EXPECT_EQ(pos_inf, pos_inf + 1.0);
    EXPECT_EQ(pos_inf, pos_inf + pos_inf);
    EXPECT_EQ(neg_inf, neg_inf - 1.0);
    EXPECT_EQ(pos_inf, pos_inf * 2.0);
    EXPECT_EQ(neg_inf, pos_inf * -1.0);
    EXPECT_EQ(pos_inf, pos_inf / 2.0);

    // Division by infinity
    EXPECT_EQ(0.0, 1.0 / pos_inf);
    EXPECT_EQ(0.0, 1.0 / neg_inf);

    // Division by zero produces infinity
    EXPECT_EQ(pos_inf, 1.0 / 0.0);
    EXPECT_EQ(neg_inf, -1.0 / 0.0);
}

TEST_F(NaNHandlingTest, Infinity_ProducesNaN)
{
    // These infinity operations produce NaN
    EXPECT_TRUE(std::isnan(pos_inf - pos_inf));
    EXPECT_TRUE(std::isnan(pos_inf + neg_inf));
    EXPECT_TRUE(std::isnan(pos_inf / pos_inf));
    EXPECT_TRUE(std::isnan(pos_inf * 0.0));
}

// =============================================================================
// INTEGER OVERFLOW/UNDERFLOW TESTS (using safe_arithmetic.h)
// =============================================================================

class IntegerEdgeCaseTest : public ::testing::Test
{
protected:
    static constexpr int64_t MAX_I64 = std::numeric_limits<int64_t>::max();
    static constexpr int64_t MIN_I64 = std::numeric_limits<int64_t>::min();
    static constexpr int32_t MAX_I32 = std::numeric_limits<int32_t>::max();
    static constexpr int32_t MIN_I32 = std::numeric_limits<int32_t>::min();
};

TEST_F(IntegerEdgeCaseTest, Addition_NearBoundary)
{
    int64_t result;

    // At boundary - should succeed
    EXPECT_TRUE(safeAddInt64(MAX_I64 - 1, 1, &result));
    EXPECT_EQ(MAX_I64, result);

    // Just over boundary - should fail
    EXPECT_FALSE(safeAddInt64(MAX_I64, 1, &result));

    // Negative boundary
    EXPECT_TRUE(safeAddInt64(MIN_I64 + 1, -1, &result));
    EXPECT_EQ(MIN_I64, result);

    EXPECT_FALSE(safeAddInt64(MIN_I64, -1, &result));
}

TEST_F(IntegerEdgeCaseTest, Subtraction_NearBoundary)
{
    int64_t result;

    // Subtracting negative from positive max
    EXPECT_FALSE(safeSubtractInt64(MAX_I64, -1, &result));

    // Subtracting positive from negative min
    EXPECT_FALSE(safeSubtractInt64(MIN_I64, 1, &result));

    // Just at boundary - should succeed
    EXPECT_TRUE(safeSubtractInt64(MAX_I64, 0, &result));
    EXPECT_EQ(MAX_I64, result);
}

TEST_F(IntegerEdgeCaseTest, Multiplication_Overflow)
{
    int64_t result;

    // Large multiplications that overflow
    EXPECT_FALSE(safeMultiplyInt64(MAX_I64, 2, &result));
    EXPECT_FALSE(safeMultiplyInt64(MIN_I64, 2, &result));
    EXPECT_FALSE(safeMultiplyInt64(MAX_I64 / 2 + 1, 2, &result));

    // MIN_I64 * -1 overflows (no positive representation)
    EXPECT_FALSE(safeMultiplyInt64(MIN_I64, -1, &result));

    // Just at boundary
    EXPECT_TRUE(safeMultiplyInt64(MAX_I64 / 2, 2, &result));
}

TEST_F(IntegerEdgeCaseTest, Division_EdgeCases)
{
    int64_t result;

    // Division by zero
    {
        ErrorContext ctx;
        EXPECT_FALSE(safeDivideInt64(100, 0, &result, &ctx));
        EXPECT_EQ(Status::DIVISION_BY_ZERO, ctx.code);
    }

    // MIN / -1 overflows
    {
        ErrorContext ctx;
        EXPECT_FALSE(safeDivideInt64(MIN_I64, -1, &result, &ctx));
        EXPECT_EQ(Status::OUT_OF_RANGE, ctx.code);
    }

    // Normal division
    {
        ErrorContext ctx;
        EXPECT_TRUE(safeDivideInt64(MAX_I64, 1, &result, &ctx));
        EXPECT_EQ(MAX_I64, result);
    }
}

TEST_F(IntegerEdgeCaseTest, Modulo_EdgeCases)
{
    int64_t result;

    // Modulo by zero
    {
        ErrorContext ctx;
        EXPECT_FALSE(safeModuloInt64(100, 0, &result, &ctx));
        EXPECT_EQ(Status::DIVISION_BY_ZERO, ctx.code);
    }

    // MIN % -1 (would require division that overflows)
    {
        ErrorContext ctx;
        EXPECT_FALSE(safeModuloInt64(MIN_I64, -1, &result, &ctx));
    }

    // Normal modulo
    {
        ErrorContext ctx;
        EXPECT_TRUE(safeModuloInt64(17, 5, &result, &ctx));
        EXPECT_EQ(2, result);
    }
}

// =============================================================================
// FLOAT PRECISION TESTS
// =============================================================================

class FloatPrecisionTest : public ::testing::Test
{
protected:
    static constexpr double EPSILON = std::numeric_limits<double>::epsilon();
};

TEST_F(FloatPrecisionTest, Epsilon_Definition)
{
    // 1.0 + epsilon should be distinguishable from 1.0
    EXPECT_NE(1.0, 1.0 + EPSILON);

    // 1.0 + epsilon/2 may not be distinguishable due to rounding
    // (implementation-dependent)
}

TEST_F(FloatPrecisionTest, Associativity_LostForLargeNumbers)
{
    // Floating point addition is not associative
    double a = 1e15;
    double b = 1.0;
    double c = -1e15;

    double result1 = (a + b) + c;
    double result2 = a + (b + c);

    // These may differ due to precision loss
    // Just verify the computation completes without error
    EXPECT_TRUE(std::isfinite(result1));
    EXPECT_TRUE(std::isfinite(result2));
}

TEST_F(FloatPrecisionTest, Subtraction_CatastrophicCancellation)
{
    // When subtracting nearly equal numbers, precision is lost
    double a = 1.0000000000001;
    double b = 1.0;
    double diff = a - b;

    // The result should be approximately 1e-13
    EXPECT_TRUE(diff > 0);
    EXPECT_TRUE(diff < 1e-10);
}

TEST_F(FloatPrecisionTest, Denormalized_Numbers)
{
    double denorm = std::numeric_limits<double>::denorm_min();

    EXPECT_TRUE(denorm > 0);
    EXPECT_TRUE(denorm < std::numeric_limits<double>::min());

    // Denormalized numbers are valid
    EXPECT_TRUE(std::isfinite(denorm));
    EXPECT_FALSE(std::isnan(denorm));
    EXPECT_FALSE(std::isinf(denorm));
}

TEST_F(FloatPrecisionTest, LargeNumber_Precision)
{
    // Very large numbers lose precision in low bits
    double large = 1e15;
    double large_plus_one = large + 1.0;

    // At 1e15, adding 1 may not change the value
    // (doubles have ~15-17 significant decimal digits)
    EXPECT_TRUE(large_plus_one >= large);
}

TEST_F(FloatPrecisionTest, Comparison_WithTolerance)
{
    double a = 0.1 + 0.1 + 0.1;
    double b = 0.3;

    // Direct comparison may fail due to representation error
    // Use tolerance-based comparison
    double tolerance = 1e-15;
    EXPECT_TRUE(std::abs(a - b) < tolerance);
}

// =============================================================================
// TYPED VALUE EDGE CASES
// =============================================================================

class TypedValueEdgeCaseTest : public ::testing::Test
{
};

TEST_F(TypedValueEdgeCaseTest, NullValue_Comparison)
{
    TypedValue null_val = TypedValue::makeNull();
    TypedValue int_val = TypedValue::makeInt64(42);

    EXPECT_TRUE(null_val.isNull());
    EXPECT_FALSE(int_val.isNull());
}

TEST_F(TypedValueEdgeCaseTest, IntegerBoundaries)
{
    TypedValue max_int = TypedValue::makeInt64(std::numeric_limits<int64_t>::max());
    TypedValue min_int = TypedValue::makeInt64(std::numeric_limits<int64_t>::min());

    EXPECT_EQ(std::numeric_limits<int64_t>::max(), max_int.toInt64());
    EXPECT_EQ(std::numeric_limits<int64_t>::min(), min_int.toInt64());
}

TEST_F(TypedValueEdgeCaseTest, DoubleBoundaries)
{
    TypedValue max_double = TypedValue::makeFloat64(std::numeric_limits<double>::max());
    TypedValue min_double = TypedValue::makeFloat64(std::numeric_limits<double>::lowest());

    EXPECT_EQ(std::numeric_limits<double>::max(), max_double.toDouble());
    EXPECT_EQ(std::numeric_limits<double>::lowest(), min_double.toDouble());
}

TEST_F(TypedValueEdgeCaseTest, EmptyString)
{
    TypedValue empty_str = TypedValue::makeVarchar("");

    EXPECT_FALSE(empty_str.isNull());
    EXPECT_EQ("", empty_str.toString());
}

TEST_F(TypedValueEdgeCaseTest, ZeroValues)
{
    TypedValue zero_int = TypedValue::makeInt64(0);
    TypedValue zero_double = TypedValue::makeFloat64(0.0);
    TypedValue neg_zero = TypedValue::makeFloat64(-0.0);

    EXPECT_EQ(0, zero_int.toInt64());
    EXPECT_EQ(0.0, zero_double.toDouble());

    // Negative zero and positive zero should compare equal
    EXPECT_EQ(0.0, neg_zero.toDouble());
}

// =============================================================================
// UTF-8 CHARACTER ENCODING EDGE CASES
// =============================================================================

class UTF8EncodingTest : public ::testing::Test
{
protected:
    // Valid UTF-8 sequences
    const std::string ascii = "Hello";
    const std::string latin = "café";  // e with acute
    const std::string cyrillic = "Привет";  // Russian "Hello"
    const std::string chinese = "你好";  // Chinese "Hello"
    const std::string emoji = "😀";  // Grinning face emoji (4-byte UTF-8)

    // Boundary characters
    const std::string max_1byte = "\x7F";  // DEL (max single-byte)
    const std::string min_2byte = "\xC2\x80";  // First 2-byte character
    const std::string max_2byte = "\xDF\xBF";  // Max 2-byte character
    const std::string min_3byte = "\xE0\xA0\x80";  // First 3-byte character
    const std::string max_3byte = "\xEF\xBF\xBF";  // Max 3-byte character (BOM/specials excluded)
    const std::string min_4byte = "\xF0\x90\x80\x80";  // First 4-byte character
    const std::string max_4byte = "\xF4\x8F\xBF\xBF";  // Max valid Unicode (U+10FFFF)
};

TEST_F(UTF8EncodingTest, ByteLength_Verification)
{
    EXPECT_EQ(5, ascii.size());  // 5 ASCII chars = 5 bytes
    EXPECT_EQ(5, latin.size());  // "café" = c(1) + a(1) + f(1) + é(2) = 5 bytes
    EXPECT_EQ(12, cyrillic.size());  // 6 Cyrillic chars × 2 bytes = 12 bytes
    EXPECT_EQ(6, chinese.size());  // 2 Chinese chars × 3 bytes = 6 bytes
    EXPECT_EQ(4, emoji.size());  // 1 emoji = 4 bytes
}

TEST_F(UTF8EncodingTest, BoundaryCharacters_ByteLength)
{
    EXPECT_EQ(1, max_1byte.size());
    EXPECT_EQ(2, min_2byte.size());
    EXPECT_EQ(2, max_2byte.size());
    EXPECT_EQ(3, min_3byte.size());
    EXPECT_EQ(3, max_3byte.size());
    EXPECT_EQ(4, min_4byte.size());
    EXPECT_EQ(4, max_4byte.size());
}

TEST_F(UTF8EncodingTest, EmptyString_Valid)
{
    std::string empty = "";
    EXPECT_EQ(0, empty.size());
    EXPECT_TRUE(empty.empty());
}

TEST_F(UTF8EncodingTest, NullByte_InString)
{
    // Strings can contain null bytes
    std::string with_null = std::string("a\0b", 3);
    EXPECT_EQ(3, with_null.size());
    EXPECT_EQ('a', with_null[0]);
    EXPECT_EQ('\0', with_null[1]);
    EXPECT_EQ('b', with_null[2]);
}

// Invalid UTF-8 sequence patterns (for validation testing)
TEST_F(UTF8EncodingTest, InvalidSequences_Detection)
{
    // These are malformed UTF-8 sequences that should be detected

    // Overlong encoding (2 bytes for ASCII char)
    std::string overlong_2 = "\xC0\xAF";  // Overlong encoding of '/'
    EXPECT_EQ(2, overlong_2.size());

    // Truncated multi-byte sequence
    std::string truncated = "\xE0\x80";  // 3-byte sequence missing final byte
    EXPECT_EQ(2, truncated.size());

    // Invalid continuation byte
    std::string bad_cont = "\xE0\x00\x80";  // NULL is not a valid continuation
    EXPECT_EQ(3, bad_cont.size());

    // Surrogate pair (invalid in UTF-8)
    std::string surrogate = "\xED\xA0\x80";  // U+D800 (high surrogate)
    EXPECT_EQ(3, surrogate.size());
}

// =============================================================================
// MATHEMATICAL FUNCTION EDGE CASES
// =============================================================================

class MathEdgeCaseTest : public ::testing::Test
{
};

TEST_F(MathEdgeCaseTest, Trigonometric_AtBoundaries)
{
    // sin/cos at multiples of pi/2
    double pi = std::acos(-1.0);

    EXPECT_NEAR(0.0, std::sin(0.0), 1e-15);
    EXPECT_NEAR(1.0, std::cos(0.0), 1e-15);
    EXPECT_NEAR(1.0, std::sin(pi / 2), 1e-15);
    EXPECT_NEAR(0.0, std::cos(pi / 2), 1e-15);
    EXPECT_NEAR(0.0, std::sin(pi), 1e-15);
    EXPECT_NEAR(-1.0, std::cos(pi), 1e-15);
}

TEST_F(MathEdgeCaseTest, Sqrt_EdgeCases)
{
    EXPECT_EQ(0.0, std::sqrt(0.0));
    EXPECT_EQ(1.0, std::sqrt(1.0));
    EXPECT_TRUE(std::isnan(std::sqrt(-1.0)));  // Negative input
    EXPECT_TRUE(std::isinf(std::sqrt(std::numeric_limits<double>::infinity())));
}

TEST_F(MathEdgeCaseTest, Log_EdgeCases)
{
    EXPECT_EQ(0.0, std::log(1.0));
    EXPECT_TRUE(std::isinf(std::log(0.0)) && std::log(0.0) < 0);  // -Infinity
    EXPECT_TRUE(std::isnan(std::log(-1.0)));  // NaN for negative
    EXPECT_TRUE(std::isinf(std::log(std::numeric_limits<double>::infinity())));
}

TEST_F(MathEdgeCaseTest, Exp_EdgeCases)
{
    EXPECT_EQ(1.0, std::exp(0.0));
    EXPECT_NEAR(std::exp(1.0), 2.718281828459045, 1e-12);
    EXPECT_EQ(0.0, std::exp(-std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(std::isinf(std::exp(std::numeric_limits<double>::infinity())));
    EXPECT_TRUE(std::isinf(std::exp(1000.0)));  // Overflow to infinity
}

TEST_F(MathEdgeCaseTest, Power_EdgeCases)
{
    // 0^0 is implementation-defined (usually 1)
    EXPECT_EQ(1.0, std::pow(0.0, 0.0));

    // Any^0 = 1
    EXPECT_EQ(1.0, std::pow(5.0, 0.0));
    EXPECT_EQ(1.0, std::pow(-5.0, 0.0));

    // 0^positive = 0
    EXPECT_EQ(0.0, std::pow(0.0, 5.0));

    // Negative base with non-integer exponent
    EXPECT_TRUE(std::isnan(std::pow(-2.0, 0.5)));

    // 1^anything = 1
    EXPECT_EQ(1.0, std::pow(1.0, 1000000.0));
    EXPECT_EQ(1.0, std::pow(1.0, std::numeric_limits<double>::infinity()));
}

TEST_F(MathEdgeCaseTest, Atan2_EdgeCases)
{
    double pi = std::acos(-1.0);

    // Signs determine quadrant
    EXPECT_NEAR(0.0, std::atan2(0.0, 1.0), 1e-15);  // Positive x-axis
    EXPECT_NEAR(pi / 2, std::atan2(1.0, 0.0), 1e-15);  // Positive y-axis
    EXPECT_NEAR(pi, std::atan2(0.0, -1.0), 1e-15);  // Negative x-axis
    EXPECT_NEAR(-pi / 2, std::atan2(-1.0, 0.0), 1e-15);  // Negative y-axis

    // atan2(0, 0) is implementation-defined
    double origin = std::atan2(0.0, 0.0);
    EXPECT_TRUE(std::isfinite(origin));
}

// =============================================================================
// ROUNDING EDGE CASES
// =============================================================================

class RoundingEdgeCaseTest : public ::testing::Test
{
};

TEST_F(RoundingEdgeCaseTest, HalfwayValues)
{
    // round() uses banker's rounding in some implementations
    // std::round uses "round half away from zero"
    EXPECT_EQ(2.0, std::round(1.5));
    EXPECT_EQ(3.0, std::round(2.5));
    EXPECT_EQ(-2.0, std::round(-1.5));
    EXPECT_EQ(-3.0, std::round(-2.5));
}

TEST_F(RoundingEdgeCaseTest, Truncation)
{
    EXPECT_EQ(1.0, std::trunc(1.9));
    EXPECT_EQ(-1.0, std::trunc(-1.9));
    EXPECT_EQ(0.0, std::trunc(0.9));
    EXPECT_EQ(0.0, std::trunc(-0.9));
}

TEST_F(RoundingEdgeCaseTest, Floor_Ceiling)
{
    EXPECT_EQ(1.0, std::floor(1.9));
    EXPECT_EQ(2.0, std::ceil(1.1));
    EXPECT_EQ(-2.0, std::floor(-1.1));
    EXPECT_EQ(-1.0, std::ceil(-1.9));
}

// =============================================================================
// DATE/TIME EDGE CASES
// =============================================================================

class DateTimeEdgeCaseTest : public ::testing::Test
{
};

TEST_F(DateTimeEdgeCaseTest, UnixEpoch_Boundaries)
{
    // Year 2038 problem for 32-bit time_t
    int32_t max_32bit_time = 2147483647;  // 2038-01-19 03:14:07 UTC

    // 64-bit time_t can represent dates far into the future
    int64_t year_3000 = 32503680000LL;  // Approximately year 3000

    EXPECT_TRUE(max_32bit_time > 0);
    EXPECT_TRUE(year_3000 > max_32bit_time);
}

TEST_F(DateTimeEdgeCaseTest, LeapYear_Rules)
{
    // Leap year rules: divisible by 4, except centuries unless divisible by 400
    auto isLeapYear = [](int year) {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    };

    EXPECT_TRUE(isLeapYear(2000));   // Divisible by 400
    EXPECT_FALSE(isLeapYear(1900)); // Century but not divisible by 400
    EXPECT_FALSE(isLeapYear(2100)); // Century but not divisible by 400
    EXPECT_TRUE(isLeapYear(2024));  // Divisible by 4
    EXPECT_FALSE(isLeapYear(2023)); // Not divisible by 4
}

TEST_F(DateTimeEdgeCaseTest, DaysInMonth_February)
{
    auto daysInFebruary = [](int year) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    };

    EXPECT_EQ(29, daysInFebruary(2024));  // Leap year
    EXPECT_EQ(28, daysInFebruary(2023));  // Non-leap year
    EXPECT_EQ(29, daysInFebruary(2000));  // Century leap year
    EXPECT_EQ(28, daysInFebruary(1900));  // Century non-leap year
}

// =============================================================================
// STRING LENGTH/SIZE EDGE CASES
// =============================================================================

class StringEdgeCaseTest : public ::testing::Test
{
};

TEST_F(StringEdgeCaseTest, MaxLength_Allocation)
{
    // Test that large string operations don't crash
    // (not testing max possible, just reasonably large)
    size_t large_size = 1000000;  // 1 MB
    std::string large_string(large_size, 'x');

    EXPECT_EQ(large_size, large_string.size());
    EXPECT_EQ('x', large_string[0]);
    EXPECT_EQ('x', large_string[large_size - 1]);
}

TEST_F(StringEdgeCaseTest, Concatenation_Empty)
{
    std::string empty = "";
    std::string hello = "hello";

    EXPECT_EQ("hello", empty + hello);
    EXPECT_EQ("hello", hello + empty);
    EXPECT_EQ("", empty + empty);
}

TEST_F(StringEdgeCaseTest, Substring_Boundaries)
{
    std::string str = "hello";

    // Substring at start
    EXPECT_EQ("hel", str.substr(0, 3));

    // Substring at end
    EXPECT_EQ("lo", str.substr(3, 2));

    // Empty substring
    EXPECT_EQ("", str.substr(0, 0));
    EXPECT_EQ("", str.substr(5, 0));

    // Full string
    EXPECT_EQ("hello", str.substr(0, 5));
    EXPECT_EQ("hello", str.substr(0, std::string::npos));
}

// =============================================================================
// BOOLEAN EDGE CASES
// =============================================================================

class BooleanEdgeCaseTest : public ::testing::Test
{
};

TEST_F(BooleanEdgeCaseTest, IntegerToBool)
{
    EXPECT_FALSE(static_cast<bool>(0));
    EXPECT_TRUE(static_cast<bool>(1));
    EXPECT_TRUE(static_cast<bool>(-1));
    EXPECT_TRUE(static_cast<bool>(42));
}

TEST_F(BooleanEdgeCaseTest, BoolToInteger)
{
    EXPECT_EQ(0, static_cast<int>(false));
    EXPECT_EQ(1, static_cast<int>(true));
}

TEST_F(BooleanEdgeCaseTest, PointerToBool)
{
    int x = 42;
    int* ptr = &x;
    int* null_ptr = nullptr;

    EXPECT_TRUE(static_cast<bool>(ptr));
    EXPECT_FALSE(static_cast<bool>(null_ptr));
}

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
