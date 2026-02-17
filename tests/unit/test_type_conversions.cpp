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
#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/config.h"

using namespace scratchbird::core;

static std::vector<uint8_t> int128ToBytes(int128_t value)
{
    std::vector<uint8_t> bytes(16, 0);
    uint128_t uvalue = static_cast<uint128_t>(value);
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        bytes[i] = static_cast<uint8_t>((uvalue >> (i * 8)) & 0xFF);
    }
    return bytes;
}

static TypedValue makeInt128Value(int128_t value)
{
    return TypedValue::makeInt128(int128ToBytes(value));
}

static void appendU16LE(std::vector<uint8_t>& out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
}

static void appendU32LE(std::vector<uint8_t>& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}

static void appendI64LE(std::vector<uint8_t>& out, int64_t value)
{
    uint64_t raw = static_cast<uint64_t>(value);
    for (int i = 0; i < 8; ++i)
    {
        out.push_back(static_cast<uint8_t>((raw >> (i * 8)) & 0xFFu));
    }
}

static Status convertValue(const TypedValue& src, DataType target, TypedValue& out, ErrorContext& ctx)
{
    return src.convertTo(TypeInfo(target), out, CastFormat::DEFAULT, &ctx);
}

class TypeConversionTest : public ::testing::Test {
protected:
    ErrorContext ctx;

    void SetUp() override
    {
        Config::getInstance().set("types", "coercion_context", "STRICT");
        Config::getInstance().set("types", "decimal_rounding_mode", "HALF_EVEN");
    }
};

// Test ARRAY to VARCHAR conversion (PostgreSQL format {1,2,3})
TEST_F(TypeConversionTest, ARRAY_ToString_PostgreSQL_Format) {
    // Create an ARRAY of INT32
    std::vector<TypedValue> values = {
        TypedValue::makeInt32(1),
        TypedValue::makeInt32(2),
        TypedValue::makeInt32(3)
    };
    auto array_val = TypedValue::makeArray(values);

    // Test toString() - should use PostgreSQL format
    std::string str = array_val.toString();
    EXPECT_EQ(str, "{1, 2, 3}");
}

// Test ARRAY to VARCHAR explicit conversion
TEST_F(TypeConversionTest, ARRAY_ToVarchar) {
    // Create an ARRAY of INT64
    std::vector<TypedValue> values = {
        TypedValue::makeInt64(10),
        TypedValue::makeInt64(20),
        TypedValue::makeInt64(30)
    };
    auto array_val = TypedValue::makeArray(values);

    // Convert to VARCHAR
    TypedValue varchar_val;
    Status status = array_val.convertTo(TypeInfo(DataType::VARCHAR), varchar_val, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(varchar_val.getVarchar(), "{10, 20, 30}");
}

// Test ARRAY to JSON conversion (JSON format [1,2,3])
TEST_F(TypeConversionTest, ARRAY_ToJSON) {
    // Create an ARRAY of FLOAT64
    std::vector<TypedValue> values = {
        TypedValue::makeFloat64(1.5),
        TypedValue::makeFloat64(2.5),
        TypedValue::makeFloat64(3.5)
    };
    auto array_val = TypedValue::makeArray(values);

    // Convert to JSON
    TypedValue json_val;
    Status status = array_val.convertTo(TypeInfo(DataType::JSON), json_val, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(json_val.toString(), "{1.5, 2.5, 3.5}");
}

// Test ARRAY with strings
TEST_F(TypeConversionTest, ARRAY_Strings_ToString) {
    // Create an ARRAY of strings
    std::vector<TypedValue> values = {
        TypedValue::makeVarchar("foo"),
        TypedValue::makeVarchar("bar"),
        TypedValue::makeVarchar("baz")
    };
    auto array_val = TypedValue::makeArray(values);

    // Test toString()
    std::string str = array_val.toString();
    EXPECT_EQ(str, "{foo, bar, baz}");
}

// Test 2D ARRAY conversion
TEST_F(TypeConversionTest, ARRAY_2D_ToString) {
    // Create a 2D ARRAY (2x3)
    std::vector<TypedValue> row1 = {
        TypedValue::makeInt32(1),
        TypedValue::makeInt32(2),
        TypedValue::makeInt32(3)
    };
    std::vector<TypedValue> row2 = {
        TypedValue::makeInt32(4),
        TypedValue::makeInt32(5),
        TypedValue::makeInt32(6)
    };
    auto array_row1 = TypedValue::makeArray(row1);
    auto array_row2 = TypedValue::makeArray(row2);
    std::vector<TypedValue> rows = {array_row1, array_row2};
    auto array_2d = TypedValue::makeArray(rows);

    EXPECT_EQ(array_2d.toString(), "{{1, 2, 3}, {4, 5, 6}}");
}

// Test NULL ARRAY
TEST_F(TypeConversionTest, ARRAY_Null_ToString) {
    auto null_val = TypedValue::makeNull();
    std::string str = null_val.toString();
    EXPECT_EQ(str, "NULL");
}

// Test MULTIPOINT to VARCHAR conversion
TEST_F(TypeConversionTest, MULTIPOINT_ToVarchar) {
    std::vector<Point> points = {{0, 0}, {1, 1}, {2, 2}};
    MultiPoint mp_value(points);
    auto mp = TypedValue::makeMultiPoint(mp_value);

    TypedValue varchar_val;
    Status status = mp.convertTo(TypeInfo(DataType::VARCHAR), varchar_val, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(varchar_val.getVarchar(), "MULTIPOINT((0 0), (1 1), (2 2))");
}

// Test MULTILINESTRING to VARCHAR conversion
TEST_F(TypeConversionTest, MULTILINESTRING_ToVarchar) {
    std::vector<Point> line1_points = {{0, 0}, {1, 1}};
    std::vector<Point> line2_points = {{2, 2}, {3, 3}};
    std::vector<LineString> linestrings = {
        LineString(line1_points),
        LineString(line2_points)
    };
    MultiLineString mls_value(linestrings);
    auto mls = TypedValue::makeMultiLineString(mls_value);

    TypedValue varchar_val;
    Status status = mls.convertTo(TypeInfo(DataType::VARCHAR), varchar_val, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(varchar_val.getVarchar(), "MULTILINESTRING((0 0, 1 1), (2 2, 3 3))");
}

// Test MULTIPOLYGON to VARCHAR conversion
TEST_F(TypeConversionTest, MULTIPOLYGON_ToVarchar) {
    std::vector<Point> ring1 = {{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
    std::vector<Point> ring2 = {{1, 1}, {2, 1}, {2, 2}, {1, 2}, {1, 1}};
    MultiPolygon mpoly_value;
    mpoly_value.polygons = {Polygon({ring1}), Polygon({ring2})};
    auto mpoly = TypedValue::makeMultiPolygon(mpoly_value);

    TypedValue varchar_val;
    Status status = mpoly.convertTo(TypeInfo(DataType::VARCHAR), varchar_val, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    // Check that it starts with MULTIPOLYGON and has proper structure
    std::string str = varchar_val.getVarchar();
    EXPECT_TRUE(str.find("MULTIPOLYGON") == 0);
    EXPECT_TRUE(str.find("((0 0, 4 0, 4 4, 0 4, 0 0))") != std::string::npos);
}

// ========== UINT Conversion Tests ==========

// Test UINT8 to larger signed types (should always succeed)
TEST_F(TypeConversionTest, UINT8_ToInt16) {
    auto u8 = TypedValue::makeUInt8(200);
    TypedValue i16;
    Status status = u8.convertTo(TypeInfo(DataType::INT16), i16, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(i16.getInt32(), 200);
}

// Test UINT32 to INT32 - overflow case
TEST_F(TypeConversionTest, UINT32_ToInt32_Overflow) {
    auto u32 = TypedValue::makeUInt32(3000000000); // > INT32_MAX
    TypedValue i32;
    Status status = u32.convertTo(TypeInfo(DataType::INT32), i32, CastFormat::DEFAULT, &ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test UINT64 to INT64 - overflow case
TEST_F(TypeConversionTest, UINT64_ToInt64_Overflow) {
    auto u64 = TypedValue::makeUInt64(UINT64_MAX);
    TypedValue i64;
    Status status = u64.convertTo(TypeInfo(DataType::INT64), i64, CastFormat::DEFAULT, &ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test UINT64 to INT64 - valid case
TEST_F(TypeConversionTest, UINT64_ToInt64_Valid) {
    auto u64 = TypedValue::makeUInt64(1000);
    TypedValue i64;
    Status status = u64.convertTo(TypeInfo(DataType::INT64), i64, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(i64.getInt64(), 1000);
}

// Test INT32 to UINT32 - negative value
TEST_F(TypeConversionTest, INT32_ToUInt32_Negative) {
    auto i32 = TypedValue::makeInt32(-100);
    TypedValue u32;
    Status status = i32.convertTo(TypeInfo(DataType::UINT32), u32, CastFormat::DEFAULT, &ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test INT32 to UINT32 - positive value
TEST_F(TypeConversionTest, INT32_ToUInt32_Positive) {
    auto i32 = TypedValue::makeInt32(1000);
    TypedValue u32;
    Status status = i32.convertTo(TypeInfo(DataType::UINT32), u32, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(u32.getUInt32(), 1000u);
}

// Test UINT8 to UINT64 upconversion
TEST_F(TypeConversionTest, UINT8_ToUInt64) {
    auto u8 = TypedValue::makeUInt8(255);
    TypedValue u64;
    Status status = u8.convertTo(TypeInfo(DataType::UINT64), u64, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(u64.getUInt64(), 255u);
}

// Test UINT64 to UINT8 downconversion - overflow
TEST_F(TypeConversionTest, UINT64_ToUInt8_Overflow) {
    auto u64 = TypedValue::makeUInt64(1000);
    TypedValue u8;
    Status status = u64.convertTo(TypeInfo(DataType::UINT8), u8, CastFormat::DEFAULT, &ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test UINT64 to UINT8 downconversion - valid
TEST_F(TypeConversionTest, UINT64_ToUInt8_Valid) {
    auto u64 = TypedValue::makeUInt64(100);
    TypedValue u8;
    Status status = u64.convertTo(TypeInfo(DataType::UINT8), u8, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(u8.getUInt8(), 100);
}

// Test UINT to FLOAT64
TEST_F(TypeConversionTest, UINT64_ToFloat64) {
    auto u64 = TypedValue::makeUInt64(1234567890);
    TypedValue f64;
    Status status = u64.convertTo(TypeInfo(DataType::FLOAT64), f64, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_DOUBLE_EQ(f64.getFloat64(), 1234567890.0);
}

// Test FLOAT to UINT - negative value
TEST_F(TypeConversionTest, Float64_ToUInt32_Negative) {
    auto f64 = TypedValue::makeFloat64(-100.5);
    TypedValue u32;
    Status status = f64.convertTo(TypeInfo(DataType::UINT32), u32, CastFormat::DEFAULT, &ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test FLOAT to UINT - positive value
TEST_F(TypeConversionTest, Float64_ToUInt32_Positive) {
    auto f64 = TypedValue::makeFloat64(1000.5);
    TypedValue u32;
    Status status = f64.convertTo(TypeInfo(DataType::UINT32), u32, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(u32.getUInt32(), 1000u);
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
    TypedValue dec;
    Status status = u32.convertTo(TypeInfo(DataType::DECIMAL), dec, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(dec.toString(), "123456");
}

// Test UINT to BOOLEAN
TEST_F(TypeConversionTest, UINT_ToBoolean) {
    auto u0 = TypedValue::makeUInt32(0);
    TypedValue b0;
    Status status = u0.convertTo(TypeInfo(DataType::BOOLEAN), b0, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_FALSE(b0.getBoolean());

    auto u1 = TypedValue::makeUInt32(1);
    TypedValue b1;
    status = u1.convertTo(TypeInfo(DataType::BOOLEAN), b1, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(b1.getBoolean());
}

// ========== INT128 Conversion Tests ==========

// Test INT128 to smaller signed types - valid values
TEST_F(TypeConversionTest, INT128_ToInt64_Valid) {
    auto i128 = makeInt128Value(1000000);
    TypedValue i64;
    Status status = convertValue(i128, DataType::INT64, i64, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(i64.getInt64(), 1000000);
}

// Test INT128 to INT64 - overflow
TEST_F(TypeConversionTest, INT128_ToInt64_Overflow) {
    // Create a large INT128 value beyond INT64_MAX
    int128_t large = static_cast<int128_t>(std::numeric_limits<int64_t>::max()) + 1;
    auto i128 = makeInt128Value(large);
    TypedValue i64;
    Status status = convertValue(i128, DataType::INT64, i64, ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test INT128 to INT32 - overflow
TEST_F(TypeConversionTest, INT128_ToInt32_Overflow) {
    auto i128 = makeInt128Value(static_cast<int128_t>(1) << 32);
    TypedValue i32;
    Status status = convertValue(i128, DataType::INT32, i32, ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test INT128 to INT8 - valid
TEST_F(TypeConversionTest, INT128_ToInt8_Valid) {
    auto i128 = makeInt128Value(100);
    TypedValue i8;
    Status status = convertValue(i128, DataType::INT8, i8, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(i8.getInt32(), 100);
}

// Test INT128 to UINT64 - negative value
TEST_F(TypeConversionTest, INT128_ToUInt64_Negative) {
    auto i128 = makeInt128Value(-1000);
    TypedValue u64;
    Status status = convertValue(i128, DataType::UINT64, u64, ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test INT128 to UINT64 - positive value
TEST_F(TypeConversionTest, INT128_ToUInt64_Positive) {
    auto i128 = makeInt128Value(1000);
    TypedValue u64;
    Status status = convertValue(i128, DataType::UINT64, u64, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(u64.getUInt64(), 1000u);
}

// Test INT128 to UINT32 - overflow
TEST_F(TypeConversionTest, INT128_ToUInt32_Overflow) {
    auto i128 = makeInt128Value(static_cast<int128_t>(1) << 33);
    TypedValue u32;
    Status status = convertValue(i128, DataType::UINT32, u32, ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test INT64 to INT128 upconversion
TEST_F(TypeConversionTest, INT64_ToInt128) {
    auto i64 = TypedValue::makeInt64(-9223372036854775807LL);
    TypedValue i128;
    Status status = convertValue(i64, DataType::INT128, i128, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(i128.getInt128(), static_cast<int128_t>(-9223372036854775807LL));
}

// Test UINT64 to INT128 upconversion
TEST_F(TypeConversionTest, UINT64_ToInt128) {
    auto u64 = TypedValue::makeUInt64(18446744073709551615ULL);
    TypedValue i128;
    Status status = convertValue(u64, DataType::INT128, i128, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(i128.getInt128(), static_cast<int128_t>(18446744073709551615ULL));
}

// Test INT128 to FLOAT64
TEST_F(TypeConversionTest, INT128_ToFloat64) {
    auto i128 = makeInt128Value(123456789012345LL);
    TypedValue f64;
    Status status = convertValue(i128, DataType::FLOAT64, f64, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_DOUBLE_EQ(f64.getFloat64(), 123456789012345.0);
}

// Test INT128 to DECIMAL
TEST_F(TypeConversionTest, INT128_ToDecimal) {
    auto i128 = makeInt128Value(123456789);
    TypedValue dec;
    Status status = convertValue(i128, DataType::DECIMAL, dec, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(dec.toString(), "123456789");
}

// Test INT128 to VARCHAR via toString
TEST_F(TypeConversionTest, INT128_ToVarchar) {
    auto i128 = makeInt128Value(123456789012345LL);
    auto str = i128.toString();
    EXPECT_EQ(str, "123456789012345");
}

// Test INT128 to BOOLEAN
TEST_F(TypeConversionTest, INT128_ToBoolean) {
    auto i128_zero = makeInt128Value(0);
    TypedValue b0;
    Status status = convertValue(i128_zero, DataType::BOOLEAN, b0, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_FALSE(b0.getBoolean());

    auto i128_one = makeInt128Value(1);
    TypedValue b1;
    status = convertValue(i128_one, DataType::BOOLEAN, b1, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(b1.getBoolean());
}

// ========== MONEY Conversion Tests ==========

// Test INT32 to MONEY - treats as cents
TEST_F(TypeConversionTest, INT32_ToMoney) {
    auto i32 = TypedValue::makeInt32(12345); // 123.45 in cents
    TypedValue money;
    Status status = convertValue(i32, DataType::MONEY, money, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money.toInt64(), 123450000);
}

// Test INT64 to MONEY - treats as cents
TEST_F(TypeConversionTest, INT64_ToMoney) {
    auto i64 = TypedValue::makeInt64(999999999); // 9999999.99 in cents
    TypedValue money;
    Status status = convertValue(i64, DataType::MONEY, money, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money.toInt64(), 9999999990000);
}

// Test negative INT to MONEY
TEST_F(TypeConversionTest, NegativeInt_ToMoney) {
    auto i32 = TypedValue::makeInt32(-5000); // -50.00
    TypedValue money;
    Status status = convertValue(i32, DataType::MONEY, money, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money.toInt64(), -50000000);
}

// Test FLOAT64 to MONEY - multiply by 100 and round
TEST_F(TypeConversionTest, Float64_ToMoney) {
    auto f64 = TypedValue::makeFloat64(123.456); // Should round to 12346 cents
    TypedValue money;
    Status status = convertValue(f64, DataType::MONEY, money, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money.toInt64(), 1234560); // 123.4560 * 10000
}

// Test FLOAT64 to MONEY - exact conversion
TEST_F(TypeConversionTest, Float64_ToMoney_Exact) {
    auto f64 = TypedValue::makeFloat64(99.99);
    TypedValue money;
    Status status = convertValue(f64, DataType::MONEY, money, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money.toInt64(), 999900);
}

// Test negative FLOAT to MONEY
TEST_F(TypeConversionTest, NegativeFloat_ToMoney) {
    auto f64 = TypedValue::makeFloat64(-50.25);
    TypedValue money;
    Status status = convertValue(f64, DataType::MONEY, money, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money.toInt64(), -502500);
}

// Test MONEY to INT64 - returns cents
TEST_F(TypeConversionTest, Money_ToInt64) {
    auto money = TypedValue::makeMoney(12345);
    TypedValue i64;
    Status status = convertValue(money, DataType::INT64, i64, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(i64.getInt64(), 1);
}

// Test MONEY to FLOAT64 - returns cents as float
TEST_F(TypeConversionTest, Money_ToFloat64) {
    auto money = TypedValue::makeMoney(12345);
    TypedValue f64;
    Status status = convertValue(money, DataType::FLOAT64, f64, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_DOUBLE_EQ(f64.getFloat64(), 1.2345);
}

// Test MONEY to VARCHAR via toString
TEST_F(TypeConversionTest, Money_ToVarchar) {
    auto money = TypedValue::makeMoney(12345);
    auto str = money.toString();
    // MONEY should format as currency string like "$123.45"
    EXPECT_EQ(str, "1.2345");
}

// Test negative MONEY to VARCHAR
TEST_F(TypeConversionTest, NegativeMoney_ToVarchar) {
    auto money = TypedValue::makeMoney(-5025);
    auto str = money.toString();
    EXPECT_EQ(str, "-0.5025");
}

// Test MONEY to DECIMAL
TEST_F(TypeConversionTest, Money_ToDecimal) {
    auto money = TypedValue::makeMoney(12345);
    TypedValue dec;
    Status status = convertValue(money, DataType::DECIMAL, dec, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(dec.toString(), "1");
}

// Test UINT32 to MONEY - treats as cents
TEST_F(TypeConversionTest, UINT32_ToMoney) {
    auto u32 = TypedValue::makeUInt32(50000);
    TypedValue money;
    Status status = convertValue(u32, DataType::MONEY, money, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money.toInt64(), 500000000);
}

// Test UINT64 to MONEY - overflow case
TEST_F(TypeConversionTest, UINT64_ToMoney_Overflow) {
    auto u64 = TypedValue::makeUInt64(static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1);
    TypedValue money;
    Status status = convertValue(u64, DataType::MONEY, money, ctx);
    EXPECT_NE(status, Status::OK); // Should fail - exceeds INT64_MAX
}

// Test UINT64 to MONEY - valid case
TEST_F(TypeConversionTest, UINT64_ToMoney_Valid) {
    auto u64 = TypedValue::makeUInt64(1000000);
    TypedValue money;
    Status status = convertValue(u64, DataType::MONEY, money, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money.toInt64(), 10000000000);
}

// Test INT128 to MONEY - overflow case (positive)
TEST_F(TypeConversionTest, INT128_ToMoney_Overflow_Positive) {
    int128_t large = static_cast<int128_t>(std::numeric_limits<int64_t>::max()) + 1;
    auto i128 = makeInt128Value(large);
    TypedValue money;
    Status status = convertValue(i128, DataType::MONEY, money, ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test INT128 to MONEY - overflow case (negative)
TEST_F(TypeConversionTest, INT128_ToMoney_Overflow_Negative) {
    int128_t large = static_cast<int128_t>(std::numeric_limits<int64_t>::min()) - 1;
    auto i128 = makeInt128Value(large);
    TypedValue money;
    Status status = convertValue(i128, DataType::MONEY, money, ctx);
    EXPECT_NE(status, Status::OK); // Should fail
}

// Test INT128 to MONEY - valid case
TEST_F(TypeConversionTest, INT128_ToMoney_Valid) {
    auto i128 = makeInt128Value(999999);
    TypedValue money;
    Status status = convertValue(i128, DataType::MONEY, money, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money.toInt64(), 9999990000);
}

// Test MONEY to BOOLEAN
TEST_F(TypeConversionTest, Money_ToBoolean) {
    auto money_zero = TypedValue::makeMoney(0);
    TypedValue b0;
    Status status = convertValue(money_zero, DataType::BOOLEAN, b0, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_FALSE(b0.getBoolean());

    auto money_nonzero = TypedValue::makeMoney(100);
    TypedValue b1;
    status = convertValue(money_nonzero, DataType::BOOLEAN, b1, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(b1.getBoolean());
}

// ========== INTERVAL Conversion Tests ==========

// Test INTERVAL to VARCHAR (toString)
TEST_F(TypeConversionTest, Interval_ToVarchar) {
    // 1 year 2 months 3 days 04:05:06
    Interval interval(14, 3, 4LL * 3600 * 1000000 + 5LL * 60 * 1000000 + 6LL * 1000000);
    auto interval_val = TypedValue::makeInterval(interval);

    std::string str = interval_val.toString();
    EXPECT_TRUE(str.find("interval") != std::string::npos);
}

// Test VARCHAR to INTERVAL - simple time
TEST_F(TypeConversionTest, Varchar_ToInterval_Time) {
    auto varchar_val = TypedValue::makeVarchar("04:05:06");
    TypedValue interval_val;
    Status status = convertValue(varchar_val, DataType::INTERVAL, interval_val, ctx);
    ASSERT_EQ(status, Status::OK);
    auto interval = interval_val.getInterval();
    EXPECT_EQ(interval.months, 0);
    EXPECT_EQ(interval.days, 0);
    EXPECT_EQ(interval.microseconds, (4LL * 3600 + 5LL * 60 + 6LL) * 1000000);
}

// Test VARCHAR to INTERVAL - days
TEST_F(TypeConversionTest, Varchar_ToInterval_Days) {
    auto varchar_val = TypedValue::makeVarchar("5 days");
    TypedValue interval_val;
    Status status = convertValue(varchar_val, DataType::INTERVAL, interval_val, ctx);
    ASSERT_EQ(status, Status::OK);
    auto interval = interval_val.getInterval();
    EXPECT_EQ(interval.months, 0);
    EXPECT_EQ(interval.days, 5);
    EXPECT_EQ(interval.microseconds, 0);
}

// Test VARCHAR to INTERVAL - months
TEST_F(TypeConversionTest, Varchar_ToInterval_Months) {
    auto varchar_val = TypedValue::makeVarchar("3 mons");
    TypedValue interval_val;
    Status status = convertValue(varchar_val, DataType::INTERVAL, interval_val, ctx);
    ASSERT_EQ(status, Status::OK);
    auto interval = interval_val.getInterval();
    EXPECT_EQ(interval.months, 3);
    EXPECT_EQ(interval.days, 0);
    EXPECT_EQ(interval.microseconds, 0);
}

// Test VARCHAR to INTERVAL - years
TEST_F(TypeConversionTest, Varchar_ToInterval_Years) {
    auto varchar_val = TypedValue::makeVarchar("2 years");
    TypedValue interval_val;
    Status status = convertValue(varchar_val, DataType::INTERVAL, interval_val, ctx);
    ASSERT_EQ(status, Status::OK);
    auto interval = interval_val.getInterval();
    EXPECT_EQ(interval.months, 24);
    EXPECT_EQ(interval.days, 0);
    EXPECT_EQ(interval.microseconds, 0);
}

// Test VARCHAR to INTERVAL - combined
TEST_F(TypeConversionTest, Varchar_ToInterval_Combined) {
    auto varchar_val = TypedValue::makeVarchar("1 year 2 mons 3 days 04:05:06");
    TypedValue interval_val;
    Status status = convertValue(varchar_val, DataType::INTERVAL, interval_val, ctx);
    ASSERT_EQ(status, Status::OK);
    auto interval = interval_val.getInterval();
    EXPECT_EQ(interval.months, 14);
    EXPECT_EQ(interval.days, 3);
    EXPECT_EQ(interval.microseconds, (4LL * 3600 + 5LL * 60 + 6LL) * 1000000);
}

// Test VARCHAR to INTERVAL - with microseconds
TEST_F(TypeConversionTest, Varchar_ToInterval_Microseconds) {
    auto varchar_val = TypedValue::makeVarchar("01:02:03.456789");
    TypedValue interval_val;
    Status status = convertValue(varchar_val, DataType::INTERVAL, interval_val, ctx);
    ASSERT_EQ(status, Status::OK);
    auto interval = interval_val.getInterval();
    EXPECT_EQ(interval.months, 0);
    EXPECT_EQ(interval.days, 0);
    EXPECT_EQ(interval.microseconds, (1LL * 3600 + 2LL * 60 + 3LL) * 1000000 + 456789);
}

// Test VARCHAR to INTERVAL - negative time
TEST_F(TypeConversionTest, Varchar_ToInterval_NegativeTime) {
    auto varchar_val = TypedValue::makeVarchar("-04:05:06");
    TypedValue interval_val;
    Status status = convertValue(varchar_val, DataType::INTERVAL, interval_val, ctx);
    ASSERT_EQ(status, Status::OK);
    auto interval = interval_val.getInterval();
    EXPECT_EQ(interval.months, 0);
    EXPECT_EQ(interval.days, 0);
    EXPECT_EQ(interval.microseconds, -1 * (4LL * 3600 + 5LL * 60 + 6LL) * 1000000);
}

// Test round-trip conversion
TEST_F(TypeConversionTest, Interval_RoundTrip) {
    Interval interval(14, 3, 4LL * 3600 * 1000000 + 5LL * 60 * 1000000 + 6LL * 1000000);
    auto original = TypedValue::makeInterval(interval);
    std::string text = original.toString();

    auto varchar_val = TypedValue::makeVarchar(text);
    TypedValue roundtrip;
    Status status = convertValue(varchar_val, DataType::INTERVAL, roundtrip, ctx);
    ASSERT_EQ(status, Status::OK);

    auto parsed = roundtrip.getInterval();
    EXPECT_EQ(parsed.months, interval.months);
    EXPECT_EQ(parsed.days, interval.days);
    EXPECT_EQ(parsed.microseconds, interval.microseconds);
}

// Test INTERVAL with zero values
TEST_F(TypeConversionTest, Interval_Zero) {
    auto varchar_val = TypedValue::makeVarchar("interval 00:00:00");
    TypedValue interval_val;
    Status status = convertValue(varchar_val, DataType::INTERVAL, interval_val, ctx);
    ASSERT_EQ(status, Status::OK);
    auto interval = interval_val.getInterval();
    EXPECT_EQ(interval.months, 0);
    EXPECT_EQ(interval.days, 0);
    EXPECT_EQ(interval.microseconds, 0);
}

// Test invalid INTERVAL format
TEST_F(TypeConversionTest, Varchar_ToInterval_Invalid) {
    auto varchar_val = TypedValue::makeVarchar("invalid interval");
    TypedValue interval_val;
    Status status = convertValue(varchar_val, DataType::INTERVAL, interval_val, ctx);
    EXPECT_NE(status, Status::OK);
}

// ===== String-to-INT128 Tests =====

TEST_F(TypeConversionTest, Varchar_ToInt128_Valid) {
    auto varchar_val = TypedValue::makeVarchar("123456789012345678901234567890");
    TypedValue int128_val;
    Status status = convertValue(varchar_val, DataType::INT128, int128_val, ctx);
    ASSERT_EQ(status, Status::OK);
    // Note: Can't easily test exact value without int128 comparison operators
    EXPECT_EQ(int128_val.type(), DataType::INT128);
}

TEST_F(TypeConversionTest, Varchar_ToInt128_Negative) {
    auto varchar_val = TypedValue::makeVarchar("-999999999999999999");
    TypedValue int128_val;
    Status status = convertValue(varchar_val, DataType::INT128, int128_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(int128_val.type(), DataType::INT128);
}

TEST_F(TypeConversionTest, Varchar_ToInt128_Zero) {
    auto varchar_val = TypedValue::makeVarchar("0");
    TypedValue int128_val;
    Status status = convertValue(varchar_val, DataType::INT128, int128_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(int128_val.type(), DataType::INT128);
}

TEST_F(TypeConversionTest, Varchar_ToInt128_Invalid) {
    auto varchar_val = TypedValue::makeVarchar("not_a_number");
    TypedValue int128_val;
    Status status = convertValue(varchar_val, DataType::INT128, int128_val, ctx);
    EXPECT_NE(status, Status::OK);
}

// ===== String-to-UINT Tests =====

TEST_F(TypeConversionTest, Varchar_ToUInt8_Valid) {
    auto varchar_val = TypedValue::makeVarchar("255");
    TypedValue uint8_val;
    Status status = convertValue(varchar_val, DataType::UINT8, uint8_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(uint8_val.getUInt8(), 255);
}

TEST_F(TypeConversionTest, Varchar_ToUInt8_Zero) {
    auto varchar_val = TypedValue::makeVarchar("0");
    TypedValue uint8_val;
    Status status = convertValue(varchar_val, DataType::UINT8, uint8_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(uint8_val.getUInt8(), 0);
}

TEST_F(TypeConversionTest, Varchar_ToUInt8_Overflow) {
    auto varchar_val = TypedValue::makeVarchar("256");
    TypedValue uint8_val;
    Status status = convertValue(varchar_val, DataType::UINT8, uint8_val, ctx);
    EXPECT_NE(status, Status::OK);
}

TEST_F(TypeConversionTest, Varchar_ToUInt16_Valid) {
    auto varchar_val = TypedValue::makeVarchar("65535");
    TypedValue uint16_val;
    Status status = convertValue(varchar_val, DataType::UINT16, uint16_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(uint16_val.getUInt16(), 65535);
}

TEST_F(TypeConversionTest, Varchar_ToUInt16_Overflow) {
    auto varchar_val = TypedValue::makeVarchar("65536");
    TypedValue uint16_val;
    Status status = convertValue(varchar_val, DataType::UINT16, uint16_val, ctx);
    EXPECT_NE(status, Status::OK);
}

TEST_F(TypeConversionTest, Varchar_ToUInt32_Valid) {
    auto varchar_val = TypedValue::makeVarchar("4294967295");
    TypedValue uint32_val;
    Status status = convertValue(varchar_val, DataType::UINT32, uint32_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(uint32_val.getUInt32(), 4294967295U);
}

TEST_F(TypeConversionTest, Varchar_ToUInt32_Small) {
    auto varchar_val = TypedValue::makeVarchar("12345");
    TypedValue uint32_val;
    Status status = convertValue(varchar_val, DataType::UINT32, uint32_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(uint32_val.getUInt32(), 12345U);
}

TEST_F(TypeConversionTest, Varchar_ToUInt64_Valid) {
    auto varchar_val = TypedValue::makeVarchar("18446744073709551615");
    TypedValue uint64_val;
    Status status = convertValue(varchar_val, DataType::UINT64, uint64_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(uint64_val.getUInt64(), 18446744073709551615ULL);
}

TEST_F(TypeConversionTest, Varchar_ToUInt64_Small) {
    auto varchar_val = TypedValue::makeVarchar("123456");
    TypedValue uint64_val;
    Status status = convertValue(varchar_val, DataType::UINT64, uint64_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(uint64_val.getUInt64(), 123456ULL);
}

// ===== String-to-MONEY Tests =====

TEST_F(TypeConversionTest, Varchar_ToMoney_DollarSign) {
    auto varchar_val = TypedValue::makeVarchar("$123.45");
    TypedValue money_val;
    Status status = convertValue(varchar_val, DataType::MONEY, money_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money_val.toInt64(), 0);
}

TEST_F(TypeConversionTest, Varchar_ToMoney_NoDollarSign) {
    auto varchar_val = TypedValue::makeVarchar("456.78");
    TypedValue money_val;
    Status status = convertValue(varchar_val, DataType::MONEY, money_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money_val.toInt64(), 4567800);
}

TEST_F(TypeConversionTest, Varchar_ToMoney_Negative) {
    auto varchar_val = TypedValue::makeVarchar("-$50.25");
    TypedValue money_val;
    Status status = convertValue(varchar_val, DataType::MONEY, money_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money_val.toInt64(), 0);
}

TEST_F(TypeConversionTest, Varchar_ToMoney_NegativeNoDollar) {
    auto varchar_val = TypedValue::makeVarchar("-75.99");
    TypedValue money_val;
    Status status = convertValue(varchar_val, DataType::MONEY, money_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money_val.toInt64(), -759900);
}

TEST_F(TypeConversionTest, Varchar_ToMoney_Zero) {
    auto varchar_val = TypedValue::makeVarchar("$0.00");
    TypedValue money_val;
    Status status = convertValue(varchar_val, DataType::MONEY, money_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money_val.toInt64(), 0);
}

TEST_F(TypeConversionTest, Varchar_ToMoney_Integer) {
    auto varchar_val = TypedValue::makeVarchar("100");
    TypedValue money_val;
    Status status = convertValue(varchar_val, DataType::MONEY, money_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money_val.toInt64(), 1000000);
}

TEST_F(TypeConversionTest, Varchar_ToMoney_WithSpaces) {
    auto varchar_val = TypedValue::makeVarchar("  $  123.45  ");
    TypedValue money_val;
    Status status = convertValue(varchar_val, DataType::MONEY, money_val, ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(money_val.toInt64(), 0);
}

TEST_F(TypeConversionTest, Varchar_ToMoney_Invalid) {
    auto varchar_val = TypedValue::makeVarchar("not_money");
    TypedValue money_val;
    Status status = convertValue(varchar_val, DataType::MONEY, money_val, ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(money_val.toInt64(), 0);
}

// ===== Deterministic Scalar Error Code Tests =====

TEST_F(TypeConversionTest, ScalarError_InvalidTextToInt_IsDeterministic) {
    auto varchar_val = TypedValue::makeVarchar("abc123");
    TypedValue int_val;
    Status status = convertValue(varchar_val, DataType::INT32, int_val, ctx);
    EXPECT_EQ(status, Status::INVALID_TEXT_REPRESENTATION);
    EXPECT_EQ(ctx.code, Status::INVALID_TEXT_REPRESENTATION);
}

TEST_F(TypeConversionTest, ScalarError_UnsignedRejectsNegativeText_IsDeterministic) {
    auto varchar_val = TypedValue::makeVarchar("-1");
    TypedValue uint_val;
    Status status = convertValue(varchar_val, DataType::UINT32, uint_val, ctx);
    EXPECT_EQ(status, Status::INVALID_TEXT_REPRESENTATION);
    EXPECT_EQ(ctx.code, Status::INVALID_TEXT_REPRESENTATION);
}

TEST_F(TypeConversionTest, ScalarError_IntOverflow_IsDeterministic) {
    auto varchar_val = TypedValue::makeVarchar("9223372036854775808");
    TypedValue int_val;
    Status status = convertValue(varchar_val, DataType::INT64, int_val, ctx);
    EXPECT_EQ(status, Status::NUMERIC_VALUE_OUT_OF_RANGE);
    EXPECT_EQ(ctx.code, Status::NUMERIC_VALUE_OUT_OF_RANGE);
}

TEST_F(TypeConversionTest, ScalarError_InvalidUuidText_IsDeterministic) {
    auto varchar_val = TypedValue::makeVarchar("invalid-uuid");
    TypedValue uuid_val;
    Status status = convertValue(varchar_val, DataType::UUID, uuid_val, ctx);
    EXPECT_EQ(status, Status::INVALID_TEXT_REPRESENTATION);
    EXPECT_EQ(ctx.code, Status::INVALID_TEXT_REPRESENTATION);
}

TEST_F(TypeConversionTest, ScalarError_InvalidBooleanText_IsDeterministic) {
    auto varchar_val = TypedValue::makeVarchar("not_bool");
    TypedValue bool_val;
    Status status = convertValue(varchar_val, DataType::BOOLEAN, bool_val, ctx);
    EXPECT_EQ(status, Status::INVALID_TEXT_REPRESENTATION);
    EXPECT_EQ(ctx.code, Status::INVALID_TEXT_REPRESENTATION);
}

TEST_F(TypeConversionTest, ScalarError_InvalidHexToBinary_IsDeterministic) {
    auto varchar_val = TypedValue::makeVarchar("zz");
    TypedValue binary_val;
    Status status = convertValue(varchar_val, DataType::BINARY, binary_val, ctx);
    EXPECT_EQ(status, Status::INVALID_TEXT_REPRESENTATION);
    EXPECT_EQ(ctx.code, Status::INVALID_TEXT_REPRESENTATION);
}

TEST_F(TypeConversionTest, ScalarError_CharLengthOverflow_IsDeterministic) {
    auto varchar_val = TypedValue::makeVarchar("toolong");
    TypedValue char_val;
    TypeInfo target(DataType::CHAR);
    target.precision = 3;
    Status status = varchar_val.convertTo(target, char_val, CastFormat::DEFAULT, &ctx);
    EXPECT_EQ(status, Status::STRING_DATA_RIGHT_TRUNCATION);
    EXPECT_EQ(ctx.code, Status::STRING_DATA_RIGHT_TRUNCATION);
}

TEST_F(TypeConversionTest, CastMatrix_BooleanToUUID_IsDatatypeMismatch) {
    auto bool_val = TypedValue::makeBoolean(true);
    TypedValue uuid_val;
    Status status = convertValue(bool_val, DataType::UUID, uuid_val, ctx);
    EXPECT_EQ(status, Status::DATATYPE_MISMATCH);
    EXPECT_EQ(ctx.code, Status::DATATYPE_MISMATCH);
}

TEST_F(TypeConversionTest, CastMatrix_UUIDToInt32_IsDatatypeMismatch) {
    std::vector<uint8_t> uuid = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
    };
    auto uuid_val = TypedValue::makeUUID(uuid);
    TypedValue int_val;
    Status status = convertValue(uuid_val, DataType::INT32, int_val, ctx);
    EXPECT_EQ(status, Status::DATATYPE_MISMATCH);
    EXPECT_EQ(ctx.code, Status::DATATYPE_MISMATCH);
}

TEST_F(TypeConversionTest, CastMatrix_BinaryToTimestamp_IsDatatypeMismatch) {
    auto binary_val = TypedValue::makeBinary({0x01, 0x02, 0x03});
    TypedValue ts_val;
    Status status = convertValue(binary_val, DataType::TIMESTAMP, ts_val, ctx);
    EXPECT_EQ(status, Status::DATATYPE_MISMATCH);
    EXPECT_EQ(ctx.code, Status::DATATYPE_MISMATCH);
}

TEST_F(TypeConversionTest, VNextCoercion_INT256_To_DECIMAL256_Exact) {
    std::vector<uint8_t> int256_payload(32, 0);
    int256_payload[0] = 0x2A;
    TypedValue int256(DataType::INT256);
    ASSERT_EQ(int256.deserializePlainValue(int256_payload, &ctx), Status::OK) << ctx.message;

    TypeInfo target(DataType::DECIMAL256);
    target.precision = 76;
    target.scale = 0;
    TypedValue decimal256;
    Status status = int256.convertTo(target, decimal256, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(decimal256.type(), DataType::DECIMAL256);
    EXPECT_EQ(decimal256.getDecimalScale(), 0u);
    EXPECT_EQ(decimal256.getBinary(), int256_payload);
}

TEST_F(TypeConversionTest, VNextCoercion_UINT256_To_DECIMAL256_OverflowReject) {
    std::vector<uint8_t> uint256_payload(32, 0);
    uint256_payload[31] = 0x80; // >= 2^255, outside DECIMAL256 signed mantissa range.
    TypedValue uint256(DataType::UINT256);
    ASSERT_EQ(uint256.deserializePlainValue(uint256_payload, &ctx), Status::OK) << ctx.message;

    TypeInfo target(DataType::DECIMAL256);
    target.precision = 76;
    target.scale = 0;
    TypedValue decimal256;
    Status status = uint256.convertTo(target, decimal256, CastFormat::DEFAULT, &ctx);
    EXPECT_EQ(status, Status::NUMERIC_VALUE_OUT_OF_RANGE);
    EXPECT_EQ(ctx.code, Status::NUMERIC_VALUE_OUT_OF_RANGE);
}

TEST_F(TypeConversionTest, VNextCoercion_DECIMAL256_To_INT256_StrictRejectsLossy) {
    std::vector<uint8_t> dec_payload;
    appendU16LE(dec_payload, 10u);
    appendU16LE(dec_payload, 1u); // scale = 1 -> lossy to integer for 1.5
    dec_payload.resize(4 + 32, 0);
    dec_payload[4] = 15; // mantissa = 15 => 1.5

    TypedValue decimal256(DataType::DECIMAL256);
    ASSERT_EQ(decimal256.deserializePlainValue(dec_payload, &ctx), Status::OK) << ctx.message;

    TypedValue int256;
    Status status = decimal256.convertTo(TypeInfo(DataType::INT256), int256, CastFormat::DEFAULT, &ctx);
    EXPECT_EQ(status, Status::DATATYPE_MISMATCH);
    EXPECT_EQ(ctx.code, Status::DATATYPE_MISMATCH);
}

TEST_F(TypeConversionTest, VNextCoercion_DECIMAL256_To_INT256_PermissiveHalfUp) {
    Config::getInstance().set("types", "coercion_context", "PERMISSIVE");
    Config::getInstance().set("types", "decimal_rounding_mode", "HALF_UP");

    std::vector<uint8_t> dec_payload;
    appendU16LE(dec_payload, 10u);
    appendU16LE(dec_payload, 1u); // scale = 1
    dec_payload.resize(4 + 32, 0);
    dec_payload[4] = 15; // 1.5 -> 2 in HALF_UP

    TypedValue decimal256(DataType::DECIMAL256);
    ASSERT_EQ(decimal256.deserializePlainValue(dec_payload, &ctx), Status::OK) << ctx.message;

    TypedValue int256;
    Status status = decimal256.convertTo(TypeInfo(DataType::INT256), int256, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_EQ(int256.type(), DataType::INT256);
    const auto& bytes = int256.getBinary();
    ASSERT_EQ(bytes.size(), 32u);
    EXPECT_EQ(bytes[0], 2u);
    for (size_t i = 1; i < bytes.size(); ++i) {
        EXPECT_EQ(bytes[i], 0u);
    }
}

TEST_F(TypeConversionTest, VNextCoercion_INT256_To_UINT256_Rejected) {
    std::vector<uint8_t> int256_payload(32, 0);
    int256_payload[0] = 1;
    TypedValue int256(DataType::INT256);
    ASSERT_EQ(int256.deserializePlainValue(int256_payload, &ctx), Status::OK) << ctx.message;

    TypedValue uint256;
    Status status = int256.convertTo(TypeInfo(DataType::UINT256), uint256, CastFormat::DEFAULT, &ctx);
    EXPECT_EQ(status, Status::DATATYPE_MISMATCH);
    EXPECT_EQ(ctx.code, Status::DATATYPE_MISMATCH);
}

TEST_F(TypeConversionTest, VNextCoercion_UINT256_To_INT256_OverflowReject) {
    std::vector<uint8_t> uint256_payload(32, 0);
    uint256_payload[31] = 0xFF; // outside signed range
    TypedValue uint256(DataType::UINT256);
    ASSERT_EQ(uint256.deserializePlainValue(uint256_payload, &ctx), Status::OK) << ctx.message;

    TypedValue int256;
    Status status = uint256.convertTo(TypeInfo(DataType::INT256), int256, CastFormat::DEFAULT, &ctx);
    EXPECT_EQ(status, Status::NUMERIC_VALUE_OUT_OF_RANGE);
    EXPECT_EQ(ctx.code, Status::NUMERIC_VALUE_OUT_OF_RANGE);
}

TEST_F(TypeConversionTest, VNextCoercion_TIMESTAMP_NS_StrictRejectsLossyToTimestamp) {
    std::vector<uint8_t> ns_payload;
    appendI64LE(ns_payload, 1500); // 1.5us
    TypedValue ts_ns(DataType::TIMESTAMP_NS);
    ASSERT_EQ(ts_ns.deserializePlainValue(ns_payload, &ctx), Status::OK) << ctx.message;

    TypedValue ts;
    Status status = ts_ns.convertTo(TypeInfo(DataType::TIMESTAMP), ts, CastFormat::DEFAULT, &ctx);
    EXPECT_EQ(status, Status::DATATYPE_MISMATCH);
    EXPECT_EQ(ctx.code, Status::DATATYPE_MISMATCH);
}

TEST_F(TypeConversionTest, VNextCoercion_TIMESTAMP_NS_PermissiveHalfEvenToTimestamp) {
    Config::getInstance().set("types", "coercion_context", "PERMISSIVE");
    Config::getInstance().set("types", "decimal_rounding_mode", "HALF_EVEN");

    std::vector<uint8_t> ns_payload;
    appendI64LE(ns_payload, 1500); // 1.5us -> 2 with HALF_EVEN from odd quotient.
    TypedValue ts_ns(DataType::TIMESTAMP_NS);
    ASSERT_EQ(ts_ns.deserializePlainValue(ns_payload, &ctx), Status::OK) << ctx.message;

    TypedValue ts;
    Status status = ts_ns.convertTo(TypeInfo(DataType::TIMESTAMP), ts, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(ts.type(), DataType::TIMESTAMP);
    EXPECT_EQ(ts.getTimestamp(), 2);
}

TEST_F(TypeConversionTest, VNextCoercion_TAGGED_UNION_ScalarRoundTrip) {
    TypedValue tagged;
    Status status = convertValue(TypedValue::makeInt32(42), DataType::TAGGED_UNION, tagged, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(tagged.type(), DataType::TAGGED_UNION);

    TypedValue back;
    status = tagged.convertTo(TypeInfo(DataType::INT32), back, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(back.getInt32(), 42);
}

TEST_F(TypeConversionTest, VNextCoercion_DICT_ENCODED_ScalarRoundTrip) {
    TypedValue encoded;
    Status status = convertValue(TypedValue::makeVarchar("dict-value"),
                                 DataType::DICT_ENCODED, encoded, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(encoded.type(), DataType::DICT_ENCODED);

    TypedValue decoded;
    status = encoded.convertTo(TypeInfo(DataType::VARCHAR), decoded, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(decoded.getVarchar(), "dict-value");
}

TEST_F(TypeConversionTest, VNextCoercion_COMPLETION_FIELD_RejectsObjectSource) {
    std::vector<TypedValue> values = {TypedValue::makeInt32(1), TypedValue::makeInt32(2)};
    auto array_val = TypedValue::makeArray(values);

    TypedValue completion;
    Status status = array_val.convertTo(TypeInfo(DataType::COMPLETION_FIELD),
                                        completion, CastFormat::DEFAULT, &ctx);
    EXPECT_EQ(status, Status::DATATYPE_MISMATCH);
    EXPECT_EQ(ctx.code, Status::DATATYPE_MISMATCH);
}

TEST_F(TypeConversionTest, VNextCoercion_FLAT_OBJECT_ToJSON) {
    std::vector<uint8_t> flat_payload;
    appendU32LE(flat_payload, 1u); // pair_count
    appendU32LE(flat_payload, 3u); // key len
    flat_payload.push_back('f');
    flat_payload.push_back('o');
    flat_payload.push_back('o');
    appendU16LE(flat_payload, static_cast<uint16_t>(DataType::INT32));
    appendU32LE(flat_payload, 4u);
    appendU32LE(flat_payload, 7u);

    TypedValue flat(DataType::FLAT_OBJECT);
    ASSERT_EQ(flat.deserializePlainValue(flat_payload, &ctx), Status::OK) << ctx.message;

    TypedValue json_val;
    Status status = flat.convertTo(TypeInfo(DataType::JSON), json_val, CastFormat::DEFAULT, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    const std::string json_text = json_val.toString();
    EXPECT_NE(json_text.find("\"foo\""), std::string::npos);
    EXPECT_NE(json_text.find("7"), std::string::npos);
}
