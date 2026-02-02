/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/types.h"
#include <iostream>
#include "gtest/gtest.h"

using namespace scratchbird::core;


TEST(NewTypesStandaloneTest, Comprehensive) {

    std::cout << "Testing INT128 and Unsigned Integer Types...\n\n";

    // Test INT128
    std::cout << "Testing INT128:\n";
    auto int128_val = TypedValue::makeInt128(12345);
    ASSERT_EQ(int128_val.type(), DataType::INT128);
    ASSERT_EQ(int128_val.getInt128(), 12345);
    std::cout << "  INT128 value: " << int128_val.toString() << "\n";
    std::cout << "  ✓ INT128 basic operations passed\n\n";

    // Test UINT8
    std::cout << "Testing UINT8:\n";
    auto uint8_min = TypedValue::makeUInt8(0);
    auto uint8_max = TypedValue::makeUInt8(255);
    ASSERT_EQ(uint8_min.type(), DataType::UINT8);
    ASSERT_EQ(uint8_max.type(), DataType::UINT8);
    ASSERT_EQ(uint8_min.getUInt8(), 0);
    ASSERT_EQ(uint8_max.getUInt8(), 255);
    std::cout << "  UINT8 min: " << uint8_min.toString() << "\n";
    std::cout << "  UINT8 max: " << uint8_max.toString() << "\n";
    std::cout << "  ✓ UINT8 basic operations passed\n\n";

    // Test UINT16
    std::cout << "Testing UINT16:\n";
    auto uint16_min = TypedValue::makeUInt16(0);
    auto uint16_max = TypedValue::makeUInt16(65535);
    ASSERT_EQ(uint16_min.type(), DataType::UINT16);
    ASSERT_EQ(uint16_max.type(), DataType::UINT16);
    ASSERT_EQ(uint16_min.getUInt16(), 0);
    ASSERT_EQ(uint16_max.getUInt16(), 65535);
    std::cout << "  UINT16 min: " << uint16_min.toString() << "\n";
    std::cout << "  UINT16 max: " << uint16_max.toString() << "\n";
    std::cout << "  ✓ UINT16 basic operations passed\n\n";

    // Test UINT32
    std::cout << "Testing UINT32:\n";
    auto uint32_min = TypedValue::makeUInt32(0);
    auto uint32_max = TypedValue::makeUInt32(4294967295U);
    ASSERT_EQ(uint32_min.type(), DataType::UINT32);
    ASSERT_EQ(uint32_max.type(), DataType::UINT32);
    ASSERT_EQ(uint32_min.getUInt32(), 0);
    ASSERT_EQ(uint32_max.getUInt32(), 4294967295U);
    std::cout << "  UINT32 min: " << uint32_min.toString() << "\n";
    std::cout << "  UINT32 max: " << uint32_max.toString() << "\n";
    std::cout << "  ✓ UINT32 basic operations passed\n\n";

    // Test UINT64
    std::cout << "Testing UINT64:\n";
    auto uint64_min = TypedValue::makeUInt64(0);
    auto uint64_max = TypedValue::makeUInt64(18446744073709551615ULL);
    ASSERT_EQ(uint64_min.type(), DataType::UINT64);
    ASSERT_EQ(uint64_max.type(), DataType::UINT64);
    ASSERT_EQ(uint64_min.getUInt64(), 0);
    ASSERT_EQ(uint64_max.getUInt64(), 18446744073709551615ULL);
    std::cout << "  UINT64 min: " << uint64_min.toString() << "\n";
    std::cout << "  UINT64 max: " << uint64_max.toString() << "\n";
    std::cout << "  ✓ UINT64 basic operations passed\n\n";

    // Test TypeSystem utilities
    std::cout << "Testing TypeSystem utilities:\n";
    ASSERT_TRUE(TypeSystem::isInteger(DataType::INT128));
    ASSERT_TRUE(TypeSystem::isInteger(DataType::UINT8));
    ASSERT_TRUE(TypeSystem::isInteger(DataType::UINT16));
    ASSERT_TRUE(TypeSystem::isInteger(DataType::UINT32));
    ASSERT_TRUE(TypeSystem::isInteger(DataType::UINT64));
    std::cout << "  ✓ isInteger() passed\n";

    ASSERT_TRUE(TypeSystem::isNumeric(DataType::INT128));
    ASSERT_TRUE(TypeSystem::isNumeric(DataType::UINT8));
    ASSERT_TRUE(TypeSystem::isNumeric(DataType::UINT16));
    ASSERT_TRUE(TypeSystem::isNumeric(DataType::UINT32));
    ASSERT_TRUE(TypeSystem::isNumeric(DataType::UINT64));
    std::cout << "  ✓ isNumeric() passed\n";

    ASSERT_TRUE(TypeSystem::isFixedLength(DataType::INT128));
    ASSERT_TRUE(TypeSystem::isFixedLength(DataType::UINT8));
    ASSERT_TRUE(TypeSystem::isFixedLength(DataType::UINT16));
    ASSERT_TRUE(TypeSystem::isFixedLength(DataType::UINT32));
    ASSERT_TRUE(TypeSystem::isFixedLength(DataType::UINT64));
    std::cout << "  ✓ isFixedLength() passed\n";

    ASSERT_EQ(TypeSystem::getFixedSize(DataType::INT128), 16);
    ASSERT_EQ(TypeSystem::getFixedSize(DataType::UINT8), 1);
    ASSERT_EQ(TypeSystem::getFixedSize(DataType::UINT16), 2);
    ASSERT_EQ(TypeSystem::getFixedSize(DataType::UINT32), 4);
    ASSERT_EQ(TypeSystem::getFixedSize(DataType::UINT64), 8);
    std::cout << "  ✓ getFixedSize() passed\n";

    ASSERT_EQ(TypeSystem::getTypeName(DataType::INT128), "INT128");
    ASSERT_EQ(TypeSystem::getTypeName(DataType::UINT8), "UINT8");
    ASSERT_EQ(TypeSystem::getTypeName(DataType::UINT16), "UINT16");
    ASSERT_EQ(TypeSystem::getTypeName(DataType::UINT32), "UINT32");
    ASSERT_EQ(TypeSystem::getTypeName(DataType::UINT64), "UINT64");
    std::cout << "  ✓ getTypeName() passed\n";

    ASSERT_EQ(TypeSystem::parseTypeName("INT128"), DataType::INT128);
    ASSERT_EQ(TypeSystem::parseTypeName("UINT8"), DataType::UINT8);
    ASSERT_EQ(TypeSystem::parseTypeName("UINT16"), DataType::UINT16);
    ASSERT_EQ(TypeSystem::parseTypeName("UINT32"), DataType::UINT32);
    ASSERT_EQ(TypeSystem::parseTypeName("UINT64"), DataType::UINT64);
    std::cout << "  ✓ parseTypeName() passed\n\n";

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "INT128 and unsigned integer types are fully functional.\n";
    std::cout << "========================================\n";
}

