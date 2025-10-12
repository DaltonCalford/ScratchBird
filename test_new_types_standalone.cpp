#include "scratchbird/core/types.h"
#include <iostream>
#include <cassert>

using namespace scratchbird::core;

int main() {
    std::cout << "Testing INT128 and Unsigned Integer Types...\n\n";

    // Test INT128
    std::cout << "Testing INT128:\n";
    auto int128_val = TypedValue::makeInt128(12345);
    assert(int128_val.type() == DataType::INT128);
    assert(int128_val.getInt128() == 12345);
    std::cout << "  INT128 value: " << int128_val.toString() << "\n";
    std::cout << "  ✓ INT128 basic operations passed\n\n";

    // Test UINT8
    std::cout << "Testing UINT8:\n";
    auto uint8_min = TypedValue::makeUInt8(0);
    auto uint8_max = TypedValue::makeUInt8(255);
    assert(uint8_min.type() == DataType::UINT8);
    assert(uint8_max.type() == DataType::UINT8);
    assert(uint8_min.getUInt8() == 0);
    assert(uint8_max.getUInt8() == 255);
    std::cout << "  UINT8 min: " << uint8_min.toString() << "\n";
    std::cout << "  UINT8 max: " << uint8_max.toString() << "\n";
    std::cout << "  ✓ UINT8 basic operations passed\n\n";

    // Test UINT16
    std::cout << "Testing UINT16:\n";
    auto uint16_min = TypedValue::makeUInt16(0);
    auto uint16_max = TypedValue::makeUInt16(65535);
    assert(uint16_min.type() == DataType::UINT16);
    assert(uint16_max.type() == DataType::UINT16);
    assert(uint16_min.getUInt16() == 0);
    assert(uint16_max.getUInt16() == 65535);
    std::cout << "  UINT16 min: " << uint16_min.toString() << "\n";
    std::cout << "  UINT16 max: " << uint16_max.toString() << "\n";
    std::cout << "  ✓ UINT16 basic operations passed\n\n";

    // Test UINT32
    std::cout << "Testing UINT32:\n";
    auto uint32_min = TypedValue::makeUInt32(0);
    auto uint32_max = TypedValue::makeUInt32(4294967295U);
    assert(uint32_min.type() == DataType::UINT32);
    assert(uint32_max.type() == DataType::UINT32);
    assert(uint32_min.getUInt32() == 0);
    assert(uint32_max.getUInt32() == 4294967295U);
    std::cout << "  UINT32 min: " << uint32_min.toString() << "\n";
    std::cout << "  UINT32 max: " << uint32_max.toString() << "\n";
    std::cout << "  ✓ UINT32 basic operations passed\n\n";

    // Test UINT64
    std::cout << "Testing UINT64:\n";
    auto uint64_min = TypedValue::makeUInt64(0);
    auto uint64_max = TypedValue::makeUInt64(18446744073709551615ULL);
    assert(uint64_min.type() == DataType::UINT64);
    assert(uint64_max.type() == DataType::UINT64);
    assert(uint64_min.getUInt64() == 0);
    assert(uint64_max.getUInt64() == 18446744073709551615ULL);
    std::cout << "  UINT64 min: " << uint64_min.toString() << "\n";
    std::cout << "  UINT64 max: " << uint64_max.toString() << "\n";
    std::cout << "  ✓ UINT64 basic operations passed\n\n";

    // Test TypeSystem utilities
    std::cout << "Testing TypeSystem utilities:\n";
    assert(TypeSystem::isInteger(DataType::INT128));
    assert(TypeSystem::isInteger(DataType::UINT8));
    assert(TypeSystem::isInteger(DataType::UINT16));
    assert(TypeSystem::isInteger(DataType::UINT32));
    assert(TypeSystem::isInteger(DataType::UINT64));
    std::cout << "  ✓ isInteger() passed\n";

    assert(TypeSystem::isNumeric(DataType::INT128));
    assert(TypeSystem::isNumeric(DataType::UINT8));
    assert(TypeSystem::isNumeric(DataType::UINT16));
    assert(TypeSystem::isNumeric(DataType::UINT32));
    assert(TypeSystem::isNumeric(DataType::UINT64));
    std::cout << "  ✓ isNumeric() passed\n";

    assert(TypeSystem::isFixedLength(DataType::INT128));
    assert(TypeSystem::isFixedLength(DataType::UINT8));
    assert(TypeSystem::isFixedLength(DataType::UINT16));
    assert(TypeSystem::isFixedLength(DataType::UINT32));
    assert(TypeSystem::isFixedLength(DataType::UINT64));
    std::cout << "  ✓ isFixedLength() passed\n";

    assert(TypeSystem::getFixedSize(DataType::INT128) == 16);
    assert(TypeSystem::getFixedSize(DataType::UINT8) == 1);
    assert(TypeSystem::getFixedSize(DataType::UINT16) == 2);
    assert(TypeSystem::getFixedSize(DataType::UINT32) == 4);
    assert(TypeSystem::getFixedSize(DataType::UINT64) == 8);
    std::cout << "  ✓ getFixedSize() passed\n";

    assert(TypeSystem::getTypeName(DataType::INT128) == "INT128");
    assert(TypeSystem::getTypeName(DataType::UINT8) == "UINT8");
    assert(TypeSystem::getTypeName(DataType::UINT16) == "UINT16");
    assert(TypeSystem::getTypeName(DataType::UINT32) == "UINT32");
    assert(TypeSystem::getTypeName(DataType::UINT64) == "UINT64");
    std::cout << "  ✓ getTypeName() passed\n";

    assert(TypeSystem::parseTypeName("INT128") == DataType::INT128);
    assert(TypeSystem::parseTypeName("UINT8") == DataType::UINT8);
    assert(TypeSystem::parseTypeName("UINT16") == DataType::UINT16);
    assert(TypeSystem::parseTypeName("UINT32") == DataType::UINT32);
    assert(TypeSystem::parseTypeName("UINT64") == DataType::UINT64);
    std::cout << "  ✓ parseTypeName() passed\n\n";

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "INT128 and unsigned integer types are fully functional.\n";
    std::cout << "========================================\n";

    return 0;
}
