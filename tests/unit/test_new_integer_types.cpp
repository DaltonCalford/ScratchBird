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
#include <limits>

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

// Test INT128 basic functionality
TEST(NewIntegerTypesTest, INT128_BasicOperations)
{
    // Test makeInt128 and getInt128
    auto val1 = TypedValue::makeInt128(int128ToBytes(0));
    EXPECT_EQ(val1.type(), DataType::INT128);
    EXPECT_EQ(val1.getInt128(), 0);

    auto val2 = TypedValue::makeInt128(int128ToBytes(42));
    EXPECT_EQ(val2.type(), DataType::INT128);
    EXPECT_EQ(val2.getInt128(), 42);

    auto val3 = TypedValue::makeInt128(int128ToBytes(-100));
    EXPECT_EQ(val3.type(), DataType::INT128);
    EXPECT_EQ(val3.getInt128(), -100);
}

// Test UINT8 basic functionality
TEST(NewIntegerTypesTest, UINT8_BasicOperations)
{
    // Test makeUInt8 and getUInt8
    auto val1 = TypedValue::makeUInt8(0);
    EXPECT_EQ(val1.type(), DataType::UINT8);
    EXPECT_EQ(val1.getUInt8(), 0);

    auto val2 = TypedValue::makeUInt8(255);
    EXPECT_EQ(val2.type(), DataType::UINT8);
    EXPECT_EQ(val2.getUInt8(), 255);

    auto val3 = TypedValue::makeUInt8(128);
    EXPECT_EQ(val3.type(), DataType::UINT8);
    EXPECT_EQ(val3.getUInt8(), 128);

    // Test toString
    EXPECT_EQ(val1.toString(), "0");
    EXPECT_EQ(val2.toString(), "255");
    EXPECT_EQ(val3.toString(), "128");
}

// Test UINT16 basic functionality
TEST(NewIntegerTypesTest, UINT16_BasicOperations)
{
    // Test makeUInt16 and getUInt16
    auto val1 = TypedValue::makeUInt16(0);
    EXPECT_EQ(val1.type(), DataType::UINT16);
    EXPECT_EQ(val1.getUInt16(), 0);

    auto val2 = TypedValue::makeUInt16(65535);
    EXPECT_EQ(val2.type(), DataType::UINT16);
    EXPECT_EQ(val2.getUInt16(), 65535);

    auto val3 = TypedValue::makeUInt16(32768);
    EXPECT_EQ(val3.type(), DataType::UINT16);
    EXPECT_EQ(val3.getUInt16(), 32768);

    // Test toString
    EXPECT_EQ(val1.toString(), "0");
    EXPECT_EQ(val2.toString(), "65535");
    EXPECT_EQ(val3.toString(), "32768");
}

// Test UINT32 basic functionality
TEST(NewIntegerTypesTest, UINT32_BasicOperations)
{
    // Test makeUInt32 and getUInt32
    auto val1 = TypedValue::makeUInt32(0);
    EXPECT_EQ(val1.type(), DataType::UINT32);
    EXPECT_EQ(val1.getUInt32(), 0);

    auto val2 = TypedValue::makeUInt32(4294967295U);
    EXPECT_EQ(val2.type(), DataType::UINT32);
    EXPECT_EQ(val2.getUInt32(), 4294967295U);

    auto val3 = TypedValue::makeUInt32(2147483648U);
    EXPECT_EQ(val3.type(), DataType::UINT32);
    EXPECT_EQ(val3.getUInt32(), 2147483648U);

    // Test toString
    EXPECT_EQ(val1.toString(), "0");
    EXPECT_EQ(val2.toString(), "4294967295");
    EXPECT_EQ(val3.toString(), "2147483648");
}

// Test UINT64 basic functionality
TEST(NewIntegerTypesTest, UINT64_BasicOperations)
{
    // Test makeUInt64 and getUInt64
    auto val1 = TypedValue::makeUInt64(0);
    EXPECT_EQ(val1.type(), DataType::UINT64);
    EXPECT_EQ(val1.getUInt64(), 0);

    auto val2 = TypedValue::makeUInt64(18446744073709551615ULL);
    EXPECT_EQ(val2.type(), DataType::UINT64);
    EXPECT_EQ(val2.getUInt64(), 18446744073709551615ULL);

    auto val3 = TypedValue::makeUInt64(9223372036854775808ULL);
    EXPECT_EQ(val3.type(), DataType::UINT64);
    EXPECT_EQ(val3.getUInt64(), 9223372036854775808ULL);

    // Test toString
    EXPECT_EQ(val1.toString(), "0");
    EXPECT_EQ(val2.toString(), "18446744073709551615");
    EXPECT_EQ(val3.toString(), "9223372036854775808");
}

TEST(NewIntegerTypesTest, TypeSystem_GetTypeName)
{
    EXPECT_EQ(TypeSystem::getTypeName(DataType::INT128), "INT128");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::UINT8), "UINT8");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::UINT16), "UINT16");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::UINT32), "UINT32");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::UINT64), "UINT64");
}

// Test type mismatch errors
TEST(NewIntegerTypesTest, TypeMismatch_ThrowsException)
{
    auto uint8_val = TypedValue::makeUInt8(42);
    auto uint16_val = TypedValue::makeUInt16(1000);

    // Attempting to get wrong type should throw
    EXPECT_THROW(uint8_val.getUInt16(), std::runtime_error);
    EXPECT_THROW(uint16_val.getUInt8(), std::runtime_error);
    EXPECT_THROW(uint8_val.getUInt32(), std::runtime_error);
}

// Test NULL values
TEST(NewIntegerTypesTest, NullValue)
{
    auto null_val = TypedValue::makeNull();
    EXPECT_TRUE(null_val.isNull());
    EXPECT_EQ(null_val.toString(), "NULL");
}

// Test INT128 toString (basic)
TEST(NewIntegerTypesTest, INT128_ToString)
{
    auto val_zero = TypedValue::makeInt128(int128ToBytes(0));
    EXPECT_EQ(val_zero.toString(), "0");

    auto val_positive = TypedValue::makeInt128(int128ToBytes(123));
    EXPECT_EQ(val_positive.toString(), "123");

    auto val_negative = TypedValue::makeInt128(int128ToBytes(-456));
    EXPECT_EQ(val_negative.toString(), "-456");
}

// Test boundary values
TEST(NewIntegerTypesTest, UINT8_BoundaryValues)
{
    auto min_val = TypedValue::makeUInt8(0);
    auto max_val = TypedValue::makeUInt8(255);

    EXPECT_EQ(min_val.getUInt8(), 0);
    EXPECT_EQ(max_val.getUInt8(), 255);
    EXPECT_EQ(min_val.toString(), "0");
    EXPECT_EQ(max_val.toString(), "255");
}

TEST(NewIntegerTypesTest, UINT16_BoundaryValues)
{
    auto min_val = TypedValue::makeUInt16(0);
    auto max_val = TypedValue::makeUInt16(65535);

    EXPECT_EQ(min_val.getUInt16(), 0);
    EXPECT_EQ(max_val.getUInt16(), 65535);
    EXPECT_EQ(min_val.toString(), "0");
    EXPECT_EQ(max_val.toString(), "65535");
}

TEST(NewIntegerTypesTest, UINT32_BoundaryValues)
{
    auto min_val = TypedValue::makeUInt32(0);
    auto max_val = TypedValue::makeUInt32(4294967295U);

    EXPECT_EQ(min_val.getUInt32(), 0);
    EXPECT_EQ(max_val.getUInt32(), 4294967295U);
    EXPECT_EQ(min_val.toString(), "0");
    EXPECT_EQ(max_val.toString(), "4294967295");
}

TEST(NewIntegerTypesTest, UINT64_BoundaryValues)
{
    auto min_val = TypedValue::makeUInt64(0);
    auto max_val = TypedValue::makeUInt64(18446744073709551615ULL);

    EXPECT_EQ(min_val.getUInt64(), 0);
    EXPECT_EQ(max_val.getUInt64(), 18446744073709551615ULL);
    EXPECT_EQ(min_val.toString(), "0");
    EXPECT_EQ(max_val.toString(), "18446744073709551615");
}
