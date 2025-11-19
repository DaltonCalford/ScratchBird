#include <gtest/gtest.h>
#include "scratchbird/core/types.h"
#include "scratchbird/core/array.h"

using namespace scratchbird::core;

class TypeConversionTest : public ::testing::Test {
protected:
    ErrorContext ctx;
};

// Test ARRAY to VARCHAR conversion (PostgreSQL format {1,2,3})
TEST_F(TypeConversionTest, ARRAY_ToString_PostgreSQL_Format) {
    // Create an ARRAY of INT32
    std::vector<int32_t> values = {1, 2, 3};
    std::vector<size_t> dimensions = {3};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);
    auto array_val = TypedValue::makeArray(arr);

    // Test toString() - should use PostgreSQL format
    std::string str = array_val.toString();
    EXPECT_EQ(str, "{1, 2, 3}");
}

// Test ARRAY to VARCHAR explicit conversion
TEST_F(TypeConversionTest, ARRAY_ToVarchar) {
    // Create an ARRAY of INT64
    std::vector<int64_t> values = {10, 20, 30};
    std::vector<size_t> dimensions = {3};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);
    auto array_val = TypedValue::makeArray(arr);

    // Convert to VARCHAR
    auto varchar_val = array_val.convertTo(DataType::VARCHAR, &ctx);
    ASSERT_TRUE(varchar_val.has_value());
    EXPECT_EQ(varchar_val->getVarchar(), "{10, 20, 30}");
}

// Test ARRAY to JSON conversion (JSON format [1,2,3])
TEST_F(TypeConversionTest, ARRAY_ToJSON) {
    // Create an ARRAY of FLOAT64
    std::vector<double> values = {1.5, 2.5, 3.5};
    std::vector<size_t> dimensions = {3};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);
    auto array_val = TypedValue::makeArray(arr);

    // Convert to JSON
    auto json_val = array_val.convertTo(DataType::JSON, &ctx);
    ASSERT_TRUE(json_val.has_value());
    EXPECT_EQ(json_val->getJSON(), "[1.5, 2.5, 3.5]");
}

// Test ARRAY with strings
TEST_F(TypeConversionTest, ARRAY_Strings_ToString) {
    // Create an ARRAY of strings
    std::vector<std::string> values = {"foo", "bar", "baz"};
    std::vector<size_t> dimensions = {3};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);
    auto array_val = TypedValue::makeArray(arr);

    // Test toString()
    std::string str = array_val.toString();
    EXPECT_EQ(str, "{\"foo\", \"bar\", \"baz\"}");
}

// Test 2D ARRAY conversion
TEST_F(TypeConversionTest, ARRAY_2D_ToString) {
    // Create a 2D ARRAY (2x3)
    std::vector<int32_t> values = {1, 2, 3, 4, 5, 6};
    std::vector<size_t> dimensions = {2, 3};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);
    auto array_val = TypedValue::makeArray(arr);

    // Test toString()
    std::string str = array_val.toString();
    EXPECT_EQ(str, "{{1, 2, 3}, {4, 5, 6}}");
}

// Test NULL ARRAY
TEST_F(TypeConversionTest, ARRAY_Null_ToString) {
    auto null_val = TypedValue::makeNull();
    null_val = TypedValue::makeArray(nullptr);

    // toString() should handle NULL
    std::string str = null_val.toString();
    EXPECT_EQ(str, "NULL");
}

// Test MULTIPOINT to VARCHAR conversion
TEST_F(TypeConversionTest, MULTIPOINT_ToVarchar) {
    std::vector<Point> points = {{0, 0}, {1, 1}, {2, 2}};
    auto mp = TypedValue::makeMultiPoint(points);

    auto varchar_val = mp.convertTo(DataType::VARCHAR, &ctx);
    ASSERT_TRUE(varchar_val.has_value());
    EXPECT_EQ(varchar_val->getVarchar(), "MULTIPOINT((0 0), (1 1), (2 2))");
}

// Test MULTILINESTRING to VARCHAR conversion
TEST_F(TypeConversionTest, MULTILINESTRING_ToVarchar) {
    std::vector<Point> line1_points = {{0, 0}, {1, 1}};
    std::vector<Point> line2_points = {{2, 2}, {3, 3}};
    std::vector<LineString> linestrings = {
        LineString(line1_points),
        LineString(line2_points)
    };
    auto mls = TypedValue::makeMultiLineString(linestrings);

    auto varchar_val = mls.convertTo(DataType::VARCHAR, &ctx);
    ASSERT_TRUE(varchar_val.has_value());
    EXPECT_EQ(varchar_val->getVarchar(), "MULTILINESTRING((0 0, 1 1), (2 2, 3 3))");
}

// Test MULTIPOLYGON to VARCHAR conversion
TEST_F(TypeConversionTest, MULTIPOLYGON_ToVarchar) {
    std::vector<Point> ring1 = {{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
    std::vector<Point> ring2 = {{1, 1}, {2, 1}, {2, 2}, {1, 2}, {1, 1}};
    std::vector<Polygon> polygons = {
        Polygon({ring1}),
        Polygon({ring2})
    };
    auto mpoly = TypedValue::makeMultiPolygon(polygons);

    auto varchar_val = mpoly.convertTo(DataType::VARCHAR, &ctx);
    ASSERT_TRUE(varchar_val.has_value());
    // Check that it starts with MULTIPOLYGON and has proper structure
    std::string str = varchar_val->getVarchar();
    EXPECT_TRUE(str.find("MULTIPOLYGON") == 0);
    EXPECT_TRUE(str.find("((0 0, 4 0, 4 4, 0 4, 0 0))") != std::string::npos);
}

// ========== UINT Conversion Tests ==========

// Test UINT8 to larger signed types (should always succeed)
TEST_F(TypeConversionTest, UINT8_ToInt16) {
    auto u8 = TypedValue::makeUInt8(200);
    auto i16 = u8.convertTo(DataType::INT16, &ctx);
    ASSERT_TRUE(i16.has_value());
    EXPECT_EQ(i16->getInt16(), 200);
}

// Test UINT32 to INT32 - overflow case
TEST_F(TypeConversionTest, UINT32_ToInt32_Overflow) {
    auto u32 = TypedValue::makeUInt32(3000000000); // > INT32_MAX
    auto i32 = u32.convertTo(DataType::INT32, &ctx);
    EXPECT_FALSE(i32.has_value()); // Should fail
}

// Test UINT64 to INT64 - overflow case
TEST_F(TypeConversionTest, UINT64_ToInt64_Overflow) {
    auto u64 = TypedValue::makeUInt64(UINT64_MAX);
    auto i64 = u64.convertTo(DataType::INT64, &ctx);
    EXPECT_FALSE(i64.has_value()); // Should fail
}

// Test UINT64 to INT64 - valid case
TEST_F(TypeConversionTest, UINT64_ToInt64_Valid) {
    auto u64 = TypedValue::makeUInt64(1000);
    auto i64 = u64.convertTo(DataType::INT64, &ctx);
    ASSERT_TRUE(i64.has_value());
    EXPECT_EQ(i64->getInt64(), 1000);
}

// Test INT32 to UINT32 - negative value
TEST_F(TypeConversionTest, INT32_ToUInt32_Negative) {
    auto i32 = TypedValue::makeInt32(-100);
    auto u32 = i32.convertTo(DataType::UINT32, &ctx);
    EXPECT_FALSE(u32.has_value()); // Should fail
}

// Test INT32 to UINT32 - positive value
TEST_F(TypeConversionTest, INT32_ToUInt32_Positive) {
    auto i32 = TypedValue::makeInt32(1000);
    auto u32 = i32.convertTo(DataType::UINT32, &ctx);
    ASSERT_TRUE(u32.has_value());
    EXPECT_EQ(u32->getUInt32(), 1000u);
}

// Test UINT8 to UINT64 upconversion
TEST_F(TypeConversionTest, UINT8_ToUInt64) {
    auto u8 = TypedValue::makeUInt8(255);
    auto u64 = u8.convertTo(DataType::UINT64, &ctx);
    ASSERT_TRUE(u64.has_value());
    EXPECT_EQ(u64->getUInt64(), 255u);
}

// Test UINT64 to UINT8 downconversion - overflow
TEST_F(TypeConversionTest, UINT64_ToUInt8_Overflow) {
    auto u64 = TypedValue::makeUInt64(1000);
    auto u8 = u64.convertTo(DataType::UINT8, &ctx);
    EXPECT_FALSE(u8.has_value()); // Should fail
}

// Test UINT64 to UINT8 downconversion - valid
TEST_F(TypeConversionTest, UINT64_ToUInt8_Valid) {
    auto u64 = TypedValue::makeUInt64(100);
    auto u8 = u64.convertTo(DataType::UINT8, &ctx);
    ASSERT_TRUE(u8.has_value());
    EXPECT_EQ(u8->getUInt8(), 100);
}

// Test UINT to FLOAT64
TEST_F(TypeConversionTest, UINT64_ToFloat64) {
    auto u64 = TypedValue::makeUInt64(1234567890);
    auto f64 = u64.convertTo(DataType::FLOAT64, &ctx);
    ASSERT_TRUE(f64.has_value());
    EXPECT_DOUBLE_EQ(f64->getFloat64(), 1234567890.0);
}

// Test FLOAT to UINT - negative value
TEST_F(TypeConversionTest, Float64_ToUInt32_Negative) {
    auto f64 = TypedValue::makeFloat64(-100.5);
    auto u32 = f64.convertTo(DataType::UINT32, &ctx);
    EXPECT_FALSE(u32.has_value()); // Should fail
}

// Test FLOAT to UINT - positive value
TEST_F(TypeConversionTest, Float64_ToUInt32_Positive) {
    auto f64 = TypedValue::makeFloat64(1000.5);
    auto u32 = f64.convertTo(DataType::UINT32, &ctx);
    ASSERT_TRUE(u32.has_value());
    EXPECT_EQ(u32->getUInt32(), 1000u);
}

// Test UINT to VARCHAR
TEST_F(TypeConversionTest, UINT64_ToVarchar) {
    auto u64 = TypedValue::makeUInt64(18446744073709551615ULL); // UINT64_MAX
    auto str = u64.toString();
    EXPECT_EQ(str, "18446744073709551615");
}

// Test UINT to DECIMAL
TEST_F(TypeConversionTest, UINT32_ToDecimal) {
    auto u32 = TypedValue::makeUInt32(123456);
    auto dec = u32.convertTo(DataType::DECIMAL, &ctx);
    ASSERT_TRUE(dec.has_value());
    EXPECT_EQ(dec->getDecimal(), "123456");
}

// Test UINT to BOOLEAN
TEST_F(TypeConversionTest, UINT_ToBoolean) {
    auto u0 = TypedValue::makeUInt32(0);
    auto b0 = u0.convertTo(DataType::BOOLEAN, &ctx);
    ASSERT_TRUE(b0.has_value());
    EXPECT_FALSE(b0->getBoolean());

    auto u1 = TypedValue::makeUInt32(1);
    auto b1 = u1.convertTo(DataType::BOOLEAN, &ctx);
    ASSERT_TRUE(b1.has_value());
    EXPECT_TRUE(b1->getBoolean());
}

// ========== INT128 Conversion Tests ==========

// Test INT128 to smaller signed types - valid values
TEST_F(TypeConversionTest, INT128_ToInt64_Valid) {
    auto i128 = TypedValue::makeInt128(1000000);
    auto i64 = i128.convertTo(DataType::INT64, &ctx);
    ASSERT_TRUE(i64.has_value());
    EXPECT_EQ(i64->getInt64(), 1000000);
}

// Test INT128 to INT64 - overflow
TEST_F(TypeConversionTest, INT128_ToInt64_Overflow) {
    // Create a large INT128 value beyond INT64_MAX
    int128_t large = static_cast<int128_t>(std::numeric_limits<int64_t>::max()) + 1;
    auto i128 = TypedValue::makeInt128(large);
    auto i64 = i128.convertTo(DataType::INT64, &ctx);
    EXPECT_FALSE(i64.has_value()); // Should fail
}

// Test INT128 to INT32 - overflow
TEST_F(TypeConversionTest, INT128_ToInt32_Overflow) {
    auto i128 = TypedValue::makeInt128(static_cast<int128_t>(1) << 32);
    auto i32 = i128.convertTo(DataType::INT32, &ctx);
    EXPECT_FALSE(i32.has_value()); // Should fail
}

// Test INT128 to INT8 - valid
TEST_F(TypeConversionTest, INT128_ToInt8_Valid) {
    auto i128 = TypedValue::makeInt128(100);
    auto i8 = i128.convertTo(DataType::INT8, &ctx);
    ASSERT_TRUE(i8.has_value());
    EXPECT_EQ(i8->getInt8(), 100);
}

// Test INT128 to UINT64 - negative value
TEST_F(TypeConversionTest, INT128_ToUInt64_Negative) {
    auto i128 = TypedValue::makeInt128(-1000);
    auto u64 = i128.convertTo(DataType::UINT64, &ctx);
    EXPECT_FALSE(u64.has_value()); // Should fail
}

// Test INT128 to UINT64 - positive value
TEST_F(TypeConversionTest, INT128_ToUInt64_Positive) {
    auto i128 = TypedValue::makeInt128(1000);
    auto u64 = i128.convertTo(DataType::UINT64, &ctx);
    ASSERT_TRUE(u64.has_value());
    EXPECT_EQ(u64->getUInt64(), 1000u);
}

// Test INT128 to UINT32 - overflow
TEST_F(TypeConversionTest, INT128_ToUInt32_Overflow) {
    auto i128 = TypedValue::makeInt128(static_cast<int128_t>(1) << 33);
    auto u32 = i128.convertTo(DataType::UINT32, &ctx);
    EXPECT_FALSE(u32.has_value()); // Should fail
}

// Test INT64 to INT128 upconversion
TEST_F(TypeConversionTest, INT64_ToInt128) {
    auto i64 = TypedValue::makeInt64(-9223372036854775807LL);
    auto i128 = i64.convertTo(DataType::INT128, &ctx);
    ASSERT_TRUE(i128.has_value());
    EXPECT_EQ(i128->getInt128(), static_cast<int128_t>(-9223372036854775807LL));
}

// Test UINT64 to INT128 upconversion
TEST_F(TypeConversionTest, UINT64_ToInt128) {
    auto u64 = TypedValue::makeUInt64(18446744073709551615ULL);
    auto i128 = u64.convertTo(DataType::INT128, &ctx);
    ASSERT_TRUE(i128.has_value());
    EXPECT_EQ(i128->getInt128(), static_cast<int128_t>(18446744073709551615ULL));
}

// Test INT128 to FLOAT64
TEST_F(TypeConversionTest, INT128_ToFloat64) {
    auto i128 = TypedValue::makeInt128(123456789012345LL);
    auto f64 = i128.convertTo(DataType::FLOAT64, &ctx);
    ASSERT_TRUE(f64.has_value());
    EXPECT_DOUBLE_EQ(f64->getFloat64(), 123456789012345.0);
}

// Test INT128 to DECIMAL
TEST_F(TypeConversionTest, INT128_ToDecimal) {
    auto i128 = TypedValue::makeInt128(123456789);
    auto dec = i128.convertTo(DataType::DECIMAL, &ctx);
    ASSERT_TRUE(dec.has_value());
    EXPECT_EQ(dec->getDecimal(), "123456789");
}

// Test INT128 to VARCHAR via toString
TEST_F(TypeConversionTest, INT128_ToVarchar) {
    auto i128 = TypedValue::makeInt128(123456789012345LL);
    auto str = i128.toString();
    EXPECT_EQ(str, "123456789012345");
}

// Test INT128 to BOOLEAN
TEST_F(TypeConversionTest, INT128_ToBoolean) {
    auto i128_zero = TypedValue::makeInt128(0);
    auto b0 = i128_zero.convertTo(DataType::BOOLEAN, &ctx);
    ASSERT_TRUE(b0.has_value());
    EXPECT_FALSE(b0->getBoolean());

    auto i128_one = TypedValue::makeInt128(1);
    auto b1 = i128_one.convertTo(DataType::BOOLEAN, &ctx);
    ASSERT_TRUE(b1.has_value());
    EXPECT_TRUE(b1->getBoolean());
}

// ========== MONEY Conversion Tests ==========

// Test INT32 to MONEY - treats as cents
TEST_F(TypeConversionTest, INT32_ToMoney) {
    auto i32 = TypedValue::makeInt32(12345); // 123.45 in cents
    auto money = i32.convertTo(DataType::MONEY, &ctx);
    ASSERT_TRUE(money.has_value());
    EXPECT_EQ(money->getMoney(), 12345);
}

// Test INT64 to MONEY - treats as cents
TEST_F(TypeConversionTest, INT64_ToMoney) {
    auto i64 = TypedValue::makeInt64(999999999); // 9999999.99 in cents
    auto money = i64.convertTo(DataType::MONEY, &ctx);
    ASSERT_TRUE(money.has_value());
    EXPECT_EQ(money->getMoney(), 999999999);
}

// Test negative INT to MONEY
TEST_F(TypeConversionTest, NegativeInt_ToMoney) {
    auto i32 = TypedValue::makeInt32(-5000); // -50.00
    auto money = i32.convertTo(DataType::MONEY, &ctx);
    ASSERT_TRUE(money.has_value());
    EXPECT_EQ(money->getMoney(), -5000);
}

// Test FLOAT64 to MONEY - multiply by 100 and round
TEST_F(TypeConversionTest, Float64_ToMoney) {
    auto f64 = TypedValue::makeFloat64(123.456); // Should round to 12346 cents
    auto money = f64.convertTo(DataType::MONEY, &ctx);
    ASSERT_TRUE(money.has_value());
    EXPECT_EQ(money->getMoney(), 12346); // Rounds 123.456 * 100 = 12345.6 -> 12346
}

// Test FLOAT64 to MONEY - exact conversion
TEST_F(TypeConversionTest, Float64_ToMoney_Exact) {
    auto f64 = TypedValue::makeFloat64(99.99);
    auto money = f64.convertTo(DataType::MONEY, &ctx);
    ASSERT_TRUE(money.has_value());
    EXPECT_EQ(money->getMoney(), 9999);
}

// Test negative FLOAT to MONEY
TEST_F(TypeConversionTest, NegativeFloat_ToMoney) {
    auto f64 = TypedValue::makeFloat64(-50.25);
    auto money = f64.convertTo(DataType::MONEY, &ctx);
    ASSERT_TRUE(money.has_value());
    EXPECT_EQ(money->getMoney(), -5025);
}

// Test MONEY to INT64 - returns cents
TEST_F(TypeConversionTest, Money_ToInt64) {
    auto money = TypedValue::makeMoney(12345);
    auto i64 = money.convertTo(DataType::INT64, &ctx);
    ASSERT_TRUE(i64.has_value());
    EXPECT_EQ(i64->getInt64(), 12345);
}

// Test MONEY to FLOAT64 - returns cents as float
TEST_F(TypeConversionTest, Money_ToFloat64) {
    auto money = TypedValue::makeMoney(12345);
    auto f64 = money.convertTo(DataType::FLOAT64, &ctx);
    ASSERT_TRUE(f64.has_value());
    EXPECT_DOUBLE_EQ(f64->getFloat64(), 12345.0);
}

// Test MONEY to VARCHAR via toString
TEST_F(TypeConversionTest, Money_ToVarchar) {
    auto money = TypedValue::makeMoney(12345);
    auto str = money.toString();
    // MONEY should format as currency string like "$123.45"
    EXPECT_EQ(str, "$123.45");
}

// Test negative MONEY to VARCHAR
TEST_F(TypeConversionTest, NegativeMoney_ToVarchar) {
    auto money = TypedValue::makeMoney(-5025);
    auto str = money.toString();
    EXPECT_EQ(str, "-$50.25");
}

// Test MONEY to DECIMAL
TEST_F(TypeConversionTest, Money_ToDecimal) {
    auto money = TypedValue::makeMoney(12345);
    auto dec = money.convertTo(DataType::DECIMAL, &ctx);
    ASSERT_TRUE(dec.has_value());
    EXPECT_EQ(dec->getDecimal(), "$123.45");
}

// Test UINT32 to MONEY - treats as cents
TEST_F(TypeConversionTest, UINT32_ToMoney) {
    auto u32 = TypedValue::makeUInt32(50000);
    auto money = u32.convertTo(DataType::MONEY, &ctx);
    ASSERT_TRUE(money.has_value());
    EXPECT_EQ(money->getMoney(), 50000);
}

// Test UINT64 to MONEY - overflow case
TEST_F(TypeConversionTest, UINT64_ToMoney_Overflow) {
    auto u64 = TypedValue::makeUInt64(static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1);
    auto money = u64.convertTo(DataType::MONEY, &ctx);
    EXPECT_FALSE(money.has_value()); // Should fail - exceeds INT64_MAX
}

// Test UINT64 to MONEY - valid case
TEST_F(TypeConversionTest, UINT64_ToMoney_Valid) {
    auto u64 = TypedValue::makeUInt64(1000000);
    auto money = u64.convertTo(DataType::MONEY, &ctx);
    ASSERT_TRUE(money.has_value());
    EXPECT_EQ(money->getMoney(), 1000000);
}

// Test INT128 to MONEY - overflow case (positive)
TEST_F(TypeConversionTest, INT128_ToMoney_Overflow_Positive) {
    int128_t large = static_cast<int128_t>(std::numeric_limits<int64_t>::max()) + 1;
    auto i128 = TypedValue::makeInt128(large);
    auto money = i128.convertTo(DataType::MONEY, &ctx);
    EXPECT_FALSE(money.has_value()); // Should fail
}

// Test INT128 to MONEY - overflow case (negative)
TEST_F(TypeConversionTest, INT128_ToMoney_Overflow_Negative) {
    int128_t large = static_cast<int128_t>(std::numeric_limits<int64_t>::min()) - 1;
    auto i128 = TypedValue::makeInt128(large);
    auto money = i128.convertTo(DataType::MONEY, &ctx);
    EXPECT_FALSE(money.has_value()); // Should fail
}

// Test INT128 to MONEY - valid case
TEST_F(TypeConversionTest, INT128_ToMoney_Valid) {
    auto i128 = TypedValue::makeInt128(999999);
    auto money = i128.convertTo(DataType::MONEY, &ctx);
    ASSERT_TRUE(money.has_value());
    EXPECT_EQ(money->getMoney(), 999999);
}

// Test MONEY to BOOLEAN
TEST_F(TypeConversionTest, Money_ToBoolean) {
    auto money_zero = TypedValue::makeMoney(0);
    auto b0 = money_zero.convertTo(DataType::BOOLEAN, &ctx);
    ASSERT_TRUE(b0.has_value());
    EXPECT_FALSE(b0->getBoolean());

    auto money_nonzero = TypedValue::makeMoney(100);
    auto b1 = money_nonzero.convertTo(DataType::BOOLEAN, &ctx);
    ASSERT_TRUE(b1.has_value());
    EXPECT_TRUE(b1->getBoolean());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
