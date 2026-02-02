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
#include <cstring>

using namespace scratchbird::core;

// ===== TypedValue Creation Tests =====

TEST(TypeSystemTest, CreateNumericTypes)
{
    auto v_int8 = TypedValue::makeInt8(127);
    EXPECT_EQ(v_int8.type(), DataType::INT8);
    EXPECT_EQ(v_int8.getInt8(), 127);

    auto v_int16 = TypedValue::makeInt16(32000);
    EXPECT_EQ(v_int16.type(), DataType::INT16);
    EXPECT_EQ(v_int16.getInt16(), 32000);

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
    EXPECT_EQ(v_json.getJSON(), "{\"key\": \"value\"}");
}

// ===== Type Conversion Tests =====

TEST(TypeSystemTest, NumericConversions)
{
    auto v_int32 = TypedValue::makeInt32(42);

    // INT32 -> INT64
    auto v_int64 = v_int32.convertTo(DataType::INT64);
    ASSERT_TRUE(v_int64.has_value());
    EXPECT_EQ(v_int64->type(), DataType::INT64);
    EXPECT_EQ(v_int64->getInt64(), 42);

    // INT32 -> FLOAT64
    auto v_float = v_int32.convertTo(DataType::FLOAT64);
    ASSERT_TRUE(v_float.has_value());
    EXPECT_EQ(v_float->type(), DataType::FLOAT64);
    EXPECT_NEAR(v_float->getFloat64(), 42.0, 0.001);

    // INT32 -> INT8 (should succeed for small values)
    auto v_int8 = v_int32.convertTo(DataType::INT8);
    ASSERT_TRUE(v_int8.has_value());
    EXPECT_EQ(v_int8->getInt8(), 42);
}

TEST(TypeSystemTest, StringToNumericConversions)
{
    auto v_str = TypedValue::makeVarchar("123");

    auto v_int = v_str.convertTo(DataType::INT32);
    ASSERT_TRUE(v_int.has_value());
    EXPECT_EQ(v_int->getInt32(), 123);

    auto v_float = TypedValue::makeVarchar("3.14").convertTo(DataType::FLOAT64);
    ASSERT_TRUE(v_float.has_value());
    EXPECT_NEAR(v_float->getFloat64(), 3.14, 0.001);
}

TEST(TypeSystemTest, NumericToStringConversions)
{
    auto v_int = TypedValue::makeInt32(42);
    auto v_str = v_int.convertTo(DataType::VARCHAR);
    ASSERT_TRUE(v_str.has_value());
    EXPECT_EQ(v_str->getVarchar(), "42");

    auto v_float = TypedValue::makeFloat64(3.14159);
    auto v_str2 = v_float.convertTo(DataType::VARCHAR);
    ASSERT_TRUE(v_str2.has_value());
    // String representation may vary slightly
    EXPECT_NE(v_str2->getVarchar().find("3.14"), std::string::npos);
}

TEST(TypeSystemTest, BooleanConversions)
{
    // BOOLEAN to STRING conversion
    auto v_true = TypedValue::makeBoolean(true);
    auto v_str = v_true.convertTo(DataType::VARCHAR);
    ASSERT_TRUE(v_str.has_value());
    EXPECT_EQ(v_str->getVarchar(), "true");

    auto v_false = TypedValue::makeBoolean(false);
    auto v_str2 = v_false.convertTo(DataType::VARCHAR);
    ASSERT_TRUE(v_str2.has_value());
    EXPECT_EQ(v_str2->getVarchar(), "false");
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
    EXPECT_NEAR(TypedValue::makeBoolean(true).toDouble(), 1.0, 0.001);
}

TEST(TypeSystemTest, ToBooleanConversion)
{
    EXPECT_TRUE(TypedValue::makeBoolean(true).toBoolean());
    EXPECT_FALSE(TypedValue::makeBoolean(false).toBoolean());
    EXPECT_TRUE(TypedValue::makeInt32(1).toBoolean());
    EXPECT_FALSE(TypedValue::makeInt32(0).toBoolean());
    EXPECT_TRUE(TypedValue::makeVarchar("hello").toBoolean());
    EXPECT_FALSE(TypedValue::makeVarchar("").toBoolean());
}

// ===== Comparison Tests =====

TEST(TypeSystemTest, NumericComparisons)
{
    auto v1 = TypedValue::makeInt32(10);
    auto v2 = TypedValue::makeInt32(20);
    auto v3 = TypedValue::makeInt32(10);

    auto eq1 = v1.equals(v2);
    ASSERT_TRUE(eq1.has_value());
    EXPECT_FALSE(*eq1);

    auto eq2 = v1.equals(v3);
    ASSERT_TRUE(eq2.has_value());
    EXPECT_TRUE(*eq2);

    auto lt = v1.lessThan(v2);
    ASSERT_TRUE(lt.has_value());
    EXPECT_TRUE(*lt);

    auto gt = v2.greaterThan(v1);
    ASSERT_TRUE(gt.has_value());
    EXPECT_TRUE(*gt);
}

TEST(TypeSystemTest, StringComparisons)
{
    auto v1 = TypedValue::makeVarchar("apple");
    auto v2 = TypedValue::makeVarchar("banana");
    auto v3 = TypedValue::makeVarchar("apple");

    auto eq1 = v1.equals(v2);
    ASSERT_TRUE(eq1.has_value());
    EXPECT_FALSE(*eq1);

    auto eq2 = v1.equals(v3);
    ASSERT_TRUE(eq2.has_value());
    EXPECT_TRUE(*eq2);

    auto lt = v1.lessThan(v2);
    ASSERT_TRUE(lt.has_value());
    EXPECT_TRUE(*lt);
}

TEST(TypeSystemTest, NullComparisons)
{
    auto v_null = TypedValue::makeNull();
    auto v_int = TypedValue::makeInt32(42);

    auto eq = v_null.equals(v_int);
    EXPECT_FALSE(eq.has_value()); // NULL comparison returns NULL (no value)

    auto eq2 = v_null.equals(v_null);
    EXPECT_FALSE(eq2.has_value()); // Even NULL == NULL is NULL in SQL
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

TEST(TypeSystemTest, IsNumericCheck)
{
    EXPECT_TRUE(TypeSystem::isNumeric(DataType::INT8));
    EXPECT_TRUE(TypeSystem::isNumeric(DataType::INT16));
    EXPECT_TRUE(TypeSystem::isNumeric(DataType::INT32));
    EXPECT_TRUE(TypeSystem::isNumeric(DataType::INT64));
    EXPECT_TRUE(TypeSystem::isNumeric(DataType::FLOAT32));
    EXPECT_TRUE(TypeSystem::isNumeric(DataType::FLOAT64));
    EXPECT_TRUE(TypeSystem::isNumeric(DataType::DECIMAL));

    EXPECT_FALSE(TypeSystem::isNumeric(DataType::VARCHAR));
    EXPECT_FALSE(TypeSystem::isNumeric(DataType::BOOLEAN));
}

TEST(TypeSystemTest, IsStringCheck)
{
    EXPECT_TRUE(TypeSystem::isString(DataType::VARCHAR));
    EXPECT_TRUE(TypeSystem::isString(DataType::TEXT));
    EXPECT_TRUE(TypeSystem::isString(DataType::CHAR));

    EXPECT_FALSE(TypeSystem::isString(DataType::INT32));
    EXPECT_FALSE(TypeSystem::isString(DataType::BINARY));
}

TEST(TypeSystemTest, IsIntegerCheck)
{
    EXPECT_TRUE(TypeSystem::isInteger(DataType::INT8));
    EXPECT_TRUE(TypeSystem::isInteger(DataType::INT16));
    EXPECT_TRUE(TypeSystem::isInteger(DataType::INT32));
    EXPECT_TRUE(TypeSystem::isInteger(DataType::INT64));

    EXPECT_FALSE(TypeSystem::isInteger(DataType::FLOAT32));
    EXPECT_FALSE(TypeSystem::isInteger(DataType::FLOAT64));
}

TEST(TypeSystemTest, GetTypeName)
{
    EXPECT_EQ(TypeSystem::getTypeName(DataType::INT32), "INT32");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::INT64), "INT64");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::FLOAT64), "FLOAT64");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::VARCHAR), "VARCHAR");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::BOOLEAN), "BOOLEAN");
}

// ===== Hash Tests =====

TEST(TypeSystemTest, HashConsistency)
{
    auto v1 = TypedValue::makeInt32(42);
    auto v2 = TypedValue::makeInt32(42);
    auto v3 = TypedValue::makeInt32(43);

    // Same values should have same hash
    EXPECT_EQ(v1.hash(), v2.hash());

    // Different values should (likely) have different hash
    EXPECT_NE(v1.hash(), v3.hash());
}

TEST(TypeSystemTest, HashDifferentTypes)
{
    auto v_int = TypedValue::makeInt32(42);
    auto v_str = TypedValue::makeVarchar("42");

    // Different types should have different hashes even with same string representation
    EXPECT_NE(v_int.hash(), v_str.hash());
}
