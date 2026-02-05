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
#include <cstring>

using namespace scratchbird::core;

// ===== TypedValue Creation Tests =====

TEST(TypeSystemTest, CreateNumericTypes)
{
    auto v_int8 = TypedValue::makeInt8(127);
    EXPECT_EQ(v_int8.type(), DataType::INT8);
    EXPECT_EQ(v_int8.getInt32(), 127);

    auto v_int16 = TypedValue::makeInt16(32000);
    EXPECT_EQ(v_int16.type(), DataType::INT16);
    EXPECT_EQ(v_int16.getInt32(), 32000);

    auto v_int32 = TypedValue::makeInt32(2000000);
    EXPECT_EQ(v_int32.type(), DataType::INT32);
    EXPECT_EQ(v_int32.getInt32(), 2000000);

    auto v_int64 = TypedValue::makeInt64(9000000000LL);
    EXPECT_EQ(v_int64.type(), DataType::INT64);
    EXPECT_EQ(v_int64.getInt64(), 9000000000LL);

    auto v_float32 = TypedValue::makeFloat32(3.14f);
    EXPECT_EQ(v_float32.type(), DataType::FLOAT32);
    EXPECT_NEAR(v_float32.getFloat32(), 3.14f, 0.001f);

    auto v_float64 = TypedValue::makeFloat64(2.718281828);
    EXPECT_EQ(v_float64.type(), DataType::FLOAT64);
    EXPECT_NEAR(v_float64.getFloat64(), 2.718281828, 0.000001);
}

TEST(TypeSystemTest, CreateStringTypes)
{
    auto v_varchar = TypedValue::makeVarchar("Hello World");
    EXPECT_EQ(v_varchar.type(), DataType::VARCHAR);
    EXPECT_EQ(v_varchar.getVarchar(), "Hello World");

    auto v_text = TypedValue::makeText("Long text content");
    EXPECT_EQ(v_text.type(), DataType::TEXT);
    EXPECT_EQ(v_text.getText(), "Long text content");

    auto v_char = TypedValue::makeChar("ABCD");
    EXPECT_EQ(v_char.type(), DataType::CHAR);
    EXPECT_EQ(v_char.getChar(), "ABCD");
}

TEST(TypeSystemTest, CreateBinaryTypes)
{
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};

    auto v_binary = TypedValue::makeBinary(data);
    EXPECT_EQ(v_binary.type(), DataType::BINARY);
    EXPECT_EQ(v_binary.getBinary(), data);
}

TEST(TypeSystemTest, CreateTemporalTypes)
{
    auto v_date = TypedValue::makeDate(19000); // Days since epoch
    EXPECT_EQ(v_date.type(), DataType::DATE);
    EXPECT_EQ(v_date.getDate(), 19000);

    auto v_time = TypedValue::makeTime(43200000000LL); // Microseconds since midnight
    EXPECT_EQ(v_time.type(), DataType::TIME);
    EXPECT_EQ(v_time.getTime(), 43200000000LL);

    auto v_timestamp = TypedValue::makeTimestamp(1609459200000000LL);
    EXPECT_EQ(v_timestamp.type(), DataType::TIMESTAMP);
    EXPECT_EQ(v_timestamp.getTimestamp(), 1609459200000000LL);
}

TEST(TypeSystemTest, CreateSpecialTypes)
{
    auto v_bool = TypedValue::makeBoolean(true);
    EXPECT_EQ(v_bool.type(), DataType::BOOLEAN);
    EXPECT_TRUE(v_bool.getBoolean());

    auto v_null = TypedValue::makeNull();
    EXPECT_EQ(v_null.type(), DataType::NULL_TYPE);
    EXPECT_TRUE(v_null.isNull());

    std::vector<uint8_t> uuid = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
    };
    auto v_uuid = TypedValue::makeUUID(uuid);
    EXPECT_EQ(v_uuid.type(), DataType::UUID);
    EXPECT_EQ(v_uuid.getUUID(), uuid);

    auto v_json = TypedValue::makeJSON("{\"key\": \"value\"}");
    EXPECT_EQ(v_json.type(), DataType::JSON);
    EXPECT_EQ(v_json.toString(), "{\"key\": \"value\"}");
}

// ===== Type Conversion Tests =====

TEST(TypeSystemTest, NumericConversions)
{
    auto v_int32 = TypedValue::makeInt32(42);

    // INT32 -> INT64
    auto v_int64 = v_int32.convertTo(DataType::INT64);
    EXPECT_EQ(v_int64.type(), DataType::INT64);
    EXPECT_EQ(v_int64.getInt64(), 42);

    // INT32 -> FLOAT64
    auto v_float = v_int32.convertTo(DataType::FLOAT64);
    EXPECT_EQ(v_float.type(), DataType::FLOAT64);
    EXPECT_NEAR(v_float.getFloat64(), 42.0, 0.001);

    // INT32 -> INT8 (should succeed for small values)
    auto v_int8 = v_int32.convertTo(DataType::INT8);
    EXPECT_EQ(v_int8.getInt32(), 42);
}

TEST(TypeSystemTest, StringToNumericConversions)
{
    auto v_str = TypedValue::makeVarchar("123");

    auto v_int = v_str.convertTo(DataType::INT32);
    EXPECT_EQ(v_int.getInt32(), 123);

    auto v_float = TypedValue::makeVarchar("3.14").convertTo(DataType::FLOAT64);
    EXPECT_NEAR(v_float.getFloat64(), 3.14, 0.001);
}

TEST(TypeSystemTest, NumericToStringConversions)
{
    auto v_int = TypedValue::makeInt32(42);
    auto v_str = v_int.convertTo(DataType::VARCHAR);
    EXPECT_EQ(v_str.getVarchar(), "42");

    auto v_float = TypedValue::makeFloat64(3.14159);
    auto v_str2 = v_float.convertTo(DataType::VARCHAR);
    // String representation may vary slightly
    EXPECT_NE(v_str2.getVarchar().find("3.14"), std::string::npos);
}

TEST(TypeSystemTest, BooleanConversions)
{
    // BOOLEAN to STRING conversion
    auto v_true = TypedValue::makeBoolean(true);
    auto v_str = v_true.convertTo(DataType::VARCHAR);
    EXPECT_EQ(v_str.getVarchar(), "true");

    auto v_false = TypedValue::makeBoolean(false);
    auto v_str2 = v_false.convertTo(DataType::VARCHAR);
    EXPECT_EQ(v_str2.getVarchar(), "false");
}

// ===== Convenience Conversion Methods Tests =====

TEST(TypeSystemTest, ToInt64Conversion)
{
    EXPECT_EQ(TypedValue::makeInt8(10).toInt64(), 10);
    EXPECT_EQ(TypedValue::makeInt16(1000).toInt64(), 1000);
    EXPECT_EQ(TypedValue::makeInt32(100000).toInt64(), 100000);
    EXPECT_EQ(TypedValue::makeInt64(9000000000LL).toInt64(), 9000000000LL);
    EXPECT_EQ(TypedValue::makeFloat32(3.9f).toInt64(), 3);
    EXPECT_EQ(TypedValue::makeFloat64(2.1).toInt64(), 2);
    EXPECT_EQ(TypedValue::makeBoolean(true).toInt64(), 1);
    EXPECT_EQ(TypedValue::makeBoolean(false).toInt64(), 0);
}

TEST(TypeSystemTest, ToDoubleConversion)
{
    EXPECT_NEAR(TypedValue::makeInt32(42).toDouble(), 42.0, 0.001);
    EXPECT_NEAR(TypedValue::makeFloat32(3.14f).toDouble(), 3.14, 0.01);
    EXPECT_NEAR(TypedValue::makeFloat64(2.718).toDouble(), 2.718, 0.001);
}

TEST(TypeSystemTest, ToBooleanConversion)
{
    EXPECT_TRUE(TypedValue::makeBoolean(true).toBoolean());
    EXPECT_FALSE(TypedValue::makeBoolean(false).toBoolean());
}

// ===== Comparison Tests =====

TEST(TypeSystemTest, NumericComparisons)
{
    auto v1 = TypedValue::makeInt32(10);
    auto v2 = TypedValue::makeInt32(20);
    auto v3 = TypedValue::makeInt32(10);

    EXPECT_FALSE(v1 == v2);
    EXPECT_TRUE(v1 == v3);
    EXPECT_TRUE(v1 < v2);
    EXPECT_TRUE(v2 > v1);
}

TEST(TypeSystemTest, StringComparisons)
{
    auto v1 = TypedValue::makeVarchar("apple");
    auto v2 = TypedValue::makeVarchar("banana");
    auto v3 = TypedValue::makeVarchar("apple");

    EXPECT_FALSE(v1 == v2);
    EXPECT_TRUE(v1 == v3);
    EXPECT_TRUE(v1 < v2);
}

TEST(TypeSystemTest, NullComparisons)
{
    auto v_null = TypedValue::makeNull();
    auto v_int = TypedValue::makeInt32(42);

    EXPECT_FALSE(v_null == v_int);
    EXPECT_FALSE(v_null == v_null);
}

// ===== ToString Tests =====

TEST(TypeSystemTest, ToStringConversion)
{
    EXPECT_EQ(TypedValue::makeNull().toString(), "NULL");
    EXPECT_EQ(TypedValue::makeInt32(42).toString(), "42");
    EXPECT_EQ(TypedValue::makeBoolean(true).toString(), "true");
    EXPECT_EQ(TypedValue::makeBoolean(false).toString(), "false");
    EXPECT_EQ(TypedValue::makeVarchar("hello").toString(), "hello");
}

// ===== TypeSystem Utility Tests =====

TEST(TypeSystemTest, IsStringCheck)
{
    EXPECT_TRUE(TypeSystem::isString(DataType::VARCHAR));
    EXPECT_TRUE(TypeSystem::isString(DataType::TEXT));
    EXPECT_TRUE(TypeSystem::isString(DataType::CHAR));

    EXPECT_FALSE(TypeSystem::isString(DataType::INT32));
    EXPECT_FALSE(TypeSystem::isString(DataType::BINARY));
}

TEST(TypeSystemTest, GetTypeName)
{
    EXPECT_EQ(TypeSystem::getTypeName(DataType::INT32), "INT32");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::INT64), "INT64");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::FLOAT64), "FLOAT64");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::VARCHAR), "VARCHAR");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::BOOLEAN), "BOOLEAN");
}
