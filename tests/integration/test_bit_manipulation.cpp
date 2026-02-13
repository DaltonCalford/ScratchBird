/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// Test Bit Manipulation Functions - November 14, 2025
// Tests all 14 bit manipulation functions implemented for ScratchBird
//
// NOTE: These tests currently only verify parsing and bytecode generation.
// Full execution testing is disabled because the executor doesn't support
// scalar SELECT queries (SELECT without FROM clause). The executor expects
// a TABLE_REF and only supports COLUMN_REF in select lists.
//
// See executor.cpp:5877-5882 and executor.cpp:5908-5912
//
// TODO: Enable full execution testing when scalar SELECT support is added

#include <gtest/gtest.h>
#include <scratchbird/core/database.h>
#include <filesystem>
#include <unistd.h>

#include "scratchbird/sblr/query_compiler_v3.h"
#include <scratchbird/sblr/executor.h>

using namespace scratchbird;
using scratchbird::sblr::QueryCompilerV3;

class BitManipulationTest : public ::testing::Test
{
protected:
    std::unique_ptr<core::Database> db;
    std::string db_path;

    void SetUp() override
    {
        db = std::make_unique<core::Database>();
        db_path = (std::filesystem::temp_directory_path() /
                   ("sb_bit_manipulation_" + std::to_string(::getpid()) + ".db"))
                      .string();
        std::filesystem::remove(db_path);

        core::ErrorContext ctx;
        auto status = core::Database::create(db_path, 16384, &ctx);
        ASSERT_EQ(status, core::Status::OK) << ctx.message;

        status = db->open(db_path, &ctx);
        ASSERT_EQ(status, core::Status::OK) << ctx.message;
    }

    void TearDown() override
    {
        if (db) {
            db->close();
        }
        if (!db_path.empty()) {
            std::filesystem::remove(db_path);
        }
    }

    // Helper to test that bit manipulation functions parse via full SQL
    // NOTE: Executor doesn't support scalar SELECT execution, so only compile.
    bool testSQL(const std::string& sql)
    {
        std::cout << "Testing SQL: " << sql << std::endl;

        QueryCompilerV3 compiler(db.get());
        auto result = compiler.compile(sql);

        if (!result.success()) {
            std::cout << "  Compile FAILED" << std::endl;
            EXPECT_TRUE(result.success()) << "Compile failed for: " << sql;
            return false;
        }
        std::cout << "  Compile OK" << std::endl;
        return true;
    }

    bool testExpression(const std::string& expr_str)
    {
        return testSQL("SELECT " + expr_str);
    }

    // Test helpers that extract expressions from SQL and test them
    bool testFunction(const std::string& sql)
    {
        if (sql.substr(0, 7) == "SELECT ") {
            return testSQL(sql);
        }
        return testExpression(sql);
    }
};

// ===== Byte Access Functions =====

TEST_F(BitManipulationTest, GET_BYTE_Function)
{
    // Test extracting first byte of 'hello' (ASCII 'h' = 104)
    testFunction("SELECT GET_BYTE('hello', 0)");
    // TODO: EXPECT_EQ(result, 104) when execution is supported

    // Test extracting second byte (ASCII 'e' = 101)
    testFunction("SELECT GET_BYTE('hello', 1)");
    // TODO: EXPECT_EQ(result, 101) when execution is supported

    // Test extracting last byte (ASCII 'o' = 111)
    testFunction("SELECT GET_BYTE('hello', 4)");
    // TODO: EXPECT_EQ(result, 111) when execution is supported
}

TEST_F(BitManipulationTest, SET_BYTE_Function)
{
    // Test setting first byte to 'H' (ASCII 72)
    testFunction("SELECT SET_BYTE('hello', 0, 72)");
    // TODO: EXPECT_EQ(result, "Hello") when execution is supported

    // Test setting middle byte
    testFunction("SELECT SET_BYTE('hello', 2, 88)");  // 'X'
    // TODO: EXPECT_EQ(result, "heXlo") when execution is supported
}

TEST_F(BitManipulationTest, GET_BIT_Function)
{
    // Test getting MSB of byte 0x80 (binary 10000000)
    // Using char with value 128 (0x80)
    testFunction("SELECT GET_BIT(CHR(128), 0)");
    // TODO: EXPECT_EQ(result, 1) when execution is supported  // MSB is set

    // Test getting LSB of byte 0x80
    testFunction("SELECT GET_BIT(CHR(128), 7)");
    // TODO: EXPECT_EQ(result, 0) when execution is supported  // LSB is not set

    // Test getting bit from 'A' (ASCII 65 = 01000001)
    testFunction("SELECT GET_BIT('A', 1)");
    // TODO: EXPECT_EQ(result, 1) when execution is supported  // Second bit from MSB is set
}

TEST_F(BitManipulationTest, SET_BIT_Function)
{
    // Test setting MSB of null byte
    testFunction("SELECT SET_BIT(CHR(0), 0, 1)");
    // TODO: EXPECT_EQ(result.length(), 1) when execution is supported
    // TODO: EXPECT_EQ(static_cast<uint8_t>(result[0]), 128) when execution is supported  // 0x80

    // Test clearing MSB
    testFunction("SELECT SET_BIT(CHR(128), 0, 0)");
    // TODO: EXPECT_EQ(result.length(), 1) when execution is supported
    // TODO: EXPECT_EQ(static_cast<uint8_t>(result[0]), 0) when execution is supported  // 0x00
}

// ===== Bitwise Operations =====

TEST_F(BitManipulationTest, BIT_AND_Function)
{
    // Test 12 & 10 = 8 (binary: 1100 & 1010 = 1000)
    testFunction("SELECT BIT_AND(12, 10)");
    // TODO: EXPECT_EQ(result, 8) when execution is supported

    // Test with 0
    testFunction("SELECT BIT_AND(255, 0)");
    // TODO: EXPECT_EQ(result, 0) when execution is supported

    // Test with all bits set
    testFunction("SELECT BIT_AND(255, 255)");
    // TODO: EXPECT_EQ(result, 255) when execution is supported
}

TEST_F(BitManipulationTest, BIT_OR_Function)
{
    // Test 12 | 10 = 14 (binary: 1100 | 1010 = 1110)
    testFunction("SELECT BIT_OR(12, 10)");
    // TODO: EXPECT_EQ(result, 14) when execution is supported

    // Test with 0
    testFunction("SELECT BIT_OR(10, 0)");
    // TODO: EXPECT_EQ(result, 10) when execution is supported

    // Test combining bits
    testFunction("SELECT BIT_OR(1, 2)");
    // TODO: EXPECT_EQ(result, 3) when execution is supported  // 01 | 10 = 11
}

TEST_F(BitManipulationTest, BIT_XOR_Function)
{
    // Test 12 ^ 10 = 6 (binary: 1100 ^ 1010 = 0110)
    testFunction("SELECT BIT_XOR(12, 10)");
    // TODO: EXPECT_EQ(result, 6) when execution is supported

    // Test XOR with itself (should be 0)
    testFunction("SELECT BIT_XOR(255, 255)");
    // TODO: EXPECT_EQ(result, 0) when execution is supported

    // Test XOR with 0 (should be unchanged)
    testFunction("SELECT BIT_XOR(123, 0)");
    // TODO: EXPECT_EQ(result, 123) when execution is supported
}

TEST_F(BitManipulationTest, BIT_NOT_Function)
{
    // Test ~5 = -6 (two's complement)
    testFunction("SELECT BIT_NOT(5)");
    // TODO: EXPECT_EQ(result, -6) when execution is supported

    // Test ~0 = -1
    testFunction("SELECT BIT_NOT(0)");
    // TODO: EXPECT_EQ(result, -1) when execution is supported

    // Test ~(-1) = 0
    // NOTE: Skipped - parser doesn't recognize negative literals in expression-only mode
    // testFunction("SELECT BIT_NOT(-1)");
    // TODO: EXPECT_EQ(result, 0) when execution is supported
}

// ===== Shift Operations =====

TEST_F(BitManipulationTest, BIT_SHIFT_LEFT_Function)
{
    // Test 5 << 2 = 20 (binary: 101 << 2 = 10100)
    testFunction("SELECT BIT_SHIFT_LEFT(5, 2)");
    // TODO: EXPECT_EQ(result, 20) when execution is supported

    // Test shift by 0
    testFunction("SELECT BIT_SHIFT_LEFT(10, 0)");
    // TODO: EXPECT_EQ(result, 10) when execution is supported

    // Test power of 2 (1 << n)
    testFunction("SELECT BIT_SHIFT_LEFT(1, 10)");
    // TODO: EXPECT_EQ(result, 1024) when execution is supported
}

TEST_F(BitManipulationTest, BIT_SHIFT_RIGHT_Function)
{
    // Test 20 >> 2 = 5 (binary: 10100 >> 2 = 101)
    testFunction("SELECT BIT_SHIFT_RIGHT(20, 2)");
    // TODO: EXPECT_EQ(result, 5) when execution is supported

    // Test shift by 0
    testFunction("SELECT BIT_SHIFT_RIGHT(10, 0)");
    // TODO: EXPECT_EQ(result, 10) when execution is supported

    // Test arithmetic right shift (sign extension)
    // NOTE: Skipped - parser doesn't recognize negative literals in expression-only mode
    // testFunction("SELECT BIT_SHIFT_RIGHT(-8, 1)");
    // TODO: EXPECT_EQ(result, -4) when execution is supported
}

TEST_F(BitManipulationTest, BIT_SHIFT_RIGHT_LOGICAL_Function)
{
    // Test logical right shift (zero-fill)
    // For positive numbers, same as arithmetic shift
    testFunction("SELECT BIT_SHIFT_RIGHT_LOGICAL(20, 2)");
    // TODO: EXPECT_EQ(result, 5) when execution is supported

    // Test shift by 0
    testFunction("SELECT BIT_SHIFT_RIGHT_LOGICAL(10, 0)");
    // TODO: EXPECT_EQ(result, 10) when execution is supported
}

// ===== Utility Functions =====

TEST_F(BitManipulationTest, BIT_COUNT_Function)
{
    // Test population count (Brian Kernighan's algorithm)
    // 7 = binary 111 (3 ones)
    testFunction("SELECT BIT_COUNT(7)");
    // TODO: EXPECT_EQ(result, 3) when execution is supported

    // 15 = binary 1111 (4 ones)
    testFunction("SELECT BIT_COUNT(15)");
    // TODO: EXPECT_EQ(result, 4) when execution is supported

    // 0 = binary 0 (0 ones)
    testFunction("SELECT BIT_COUNT(0)");
    // TODO: EXPECT_EQ(result, 0) when execution is supported

    // 255 = binary 11111111 (8 ones)
    testFunction("SELECT BIT_COUNT(255)");
    // TODO: EXPECT_EQ(result, 8) when execution is supported

    // Powers of 2 have exactly 1 bit set
    testFunction("SELECT BIT_COUNT(1024)");
    // TODO: EXPECT_EQ(result, 1) when execution is supported
}

TEST_F(BitManipulationTest, BIT_LENGTH_Function)
{
    // Test 'hello' (5 bytes = 40 bits)
    testFunction("SELECT BIT_LENGTH('hello')");
    // TODO: EXPECT_EQ(result, 40) when execution is supported

    // Test empty string
    testFunction("SELECT BIT_LENGTH('')");
    // TODO: EXPECT_EQ(result, 0) when execution is supported

    // Test single character
    testFunction("SELECT BIT_LENGTH('A')");
    // TODO: EXPECT_EQ(result, 8) when execution is supported
}

TEST_F(BitManipulationTest, BIT_MASK_Function)
{
    // Test creating mask of 4 ones: 1111 = 15
    testFunction("SELECT BIT_MASK(4)");
    // TODO: EXPECT_EQ(result, 15) when execution is supported

    // Test mask of 8 ones: 11111111 = 255
    testFunction("SELECT BIT_MASK(8)");
    // TODO: EXPECT_EQ(result, 255) when execution is supported

    // Test mask of 0 ones: 0
    testFunction("SELECT BIT_MASK(0)");
    // TODO: EXPECT_EQ(result, 0) when execution is supported

    // Test mask of 1 one: 1
    testFunction("SELECT BIT_MASK(1)");
    // TODO: EXPECT_EQ(result, 1) when execution is supported

    // Test mask of 16 ones
    testFunction("SELECT BIT_MASK(16)");
    // TODO: EXPECT_EQ(result, 65535) when execution is supported
}

// ===== Combined Operations Tests =====

TEST_F(BitManipulationTest, CombinedOperations_ExtractAndSetBits)
{
    // Test extracting a byte, modifying it with bitwise ops, and setting it back
    // Example: Get byte, set a bit, put it back
    testFunction("SELECT SET_BYTE('hello', 0, BIT_OR(GET_BYTE('hello', 0), 128))");
    // First byte of 'hello' is 'h' (104 = 01101000)
    // OR with 128 (10000000) gives 232 (11101000)
    // TODO: EXPECT_EQ(result.length(), 5) when execution is supported
    // TODO: EXPECT_EQ(static_cast<uint8_t>(result[0]), 232) when execution is supported
}

TEST_F(BitManipulationTest, CombinedOperations_BitwiseMasking)
{
    // Test using BIT_MASK to extract lower bits
    // Extract lower 4 bits of 255
    testFunction("SELECT BIT_AND(255, BIT_MASK(4))");
    // TODO: EXPECT_EQ(result, 15) when execution is supported  // Lower 4 bits of 11111111 are 1111

    // Extract lower 3 bits of 10
    testFunction("SELECT BIT_AND(10, BIT_MASK(3))");
    // TODO: EXPECT_EQ(result, 2) when execution is supported  // Lower 3 bits of 1010 are 010
}

TEST_F(BitManipulationTest, CombinedOperations_ShiftAndMask)
{
    // Test extracting specific bits using shift and mask
    // Extract bits 4-7 of byte 170 (10101010)
    // Shift right by 4: 00001010
    // Mask lower 4 bits: 00001010 & 00001111 = 00001010 (10)
    testFunction("SELECT BIT_AND(BIT_SHIFT_RIGHT(170, 4), BIT_MASK(4))");
    // TODO: EXPECT_EQ(result, 10) when execution is supported
}

TEST_F(BitManipulationTest, CombinedOperations_CountBitsInRange)
{
    // Count set bits in lower byte of a larger number
    testFunction("SELECT BIT_COUNT(BIT_AND(1023, BIT_MASK(8)))");
    // 1023 = 1111111111, lower 8 bits = 11111111 = 8 set bits
    // TODO: EXPECT_EQ(result, 8) when execution is supported
}
