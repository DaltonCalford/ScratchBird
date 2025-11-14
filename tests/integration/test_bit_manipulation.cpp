// Test Bit Manipulation Functions - November 14, 2025
// Tests all 14 bit manipulation functions implemented for ScratchBird

#include <gtest/gtest.h>
#include <scratchbird/core/database.h>
#include <scratchbird/parser/parser.h>
#include <scratchbird/sblr/bytecode_generator.h>
#include <scratchbird/sblr/executor.h>

using namespace scratchbird;

class BitManipulationTest : public ::testing::Test
{
protected:
    std::unique_ptr<core::Database> db;

    void SetUp() override
    {
        // Create test database
        core::ErrorContext ctx;
        auto status = core::Database::create("test_bit_manipulation.db", 16384, &ctx);
        ASSERT_TRUE(status == core::Status::OK) << "Failed to create database: " << ctx.message;

        db = std::make_unique<core::Database>();
        status = db->open("test_bit_manipulation.db", &ctx);
        ASSERT_TRUE(status == core::Status::OK) << "Failed to open database: " << ctx.message;
    }

    void TearDown() override
    {
        db.reset();
        // Clean up test database
        std::remove("test_bit_manipulation.db");
    }

    // Helper to execute a simple SELECT statement and get integer result
    int64_t executeScalarInt(const std::string& sql)
    {
        parser::Lexer lexer(sql);
        parser::ASTArena arena;
        parser::Parser parser(lexer, arena);
        auto parse_result = parser.parseStatement();
        EXPECT_TRUE(parse_result.success()) << "Parse failed";
        if (!parse_result.success()) return 0;

        sblr::BytecodeGenerator generator(lexer.stringPool(), db.get());
        auto bytecode_result = generator.generate(parse_result.statement());
        if (!bytecode_result.success()) {
            for (const auto& err : bytecode_result.errors()) {
                EXPECT_TRUE(false) << "Bytecode generation failed: " << err;
            }
            return 0;
        }

        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());
        EXPECT_TRUE(exec_result.success()) << "Execution failed: " << exec_result.error();
        if (!exec_result.success()) return 0;

        auto result_set = exec_result.resultSet();
        EXPECT_NE(result_set, nullptr);
        if (!result_set) return 0;

        EXPECT_EQ(result_set->rowCount(), 1) << "Expected 1 row";
        EXPECT_EQ(result_set->columnCount(), 1) << "Expected 1 column";
        if (result_set->rowCount() != 1 || result_set->columnCount() != 1) return 0;

        return result_set->getValue(0, 0).toInt64();
    }

    // Helper to execute a simple SELECT statement and get string result
    std::string executeScalarString(const std::string& sql)
    {
        parser::Lexer lexer(sql);
        parser::ASTArena arena;
        parser::Parser parser(lexer, arena);
        auto parse_result = parser.parseStatement();
        EXPECT_TRUE(parse_result.success()) << "Parse failed";
        if (!parse_result.success()) return "";

        sblr::BytecodeGenerator generator(lexer.stringPool(), db.get());
        auto bytecode_result = generator.generate(parse_result.statement());
        if (!bytecode_result.success()) {
            for (const auto& err : bytecode_result.errors()) {
                EXPECT_TRUE(false) << "Bytecode generation failed: " << err;
            }
            return "";
        }

        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());
        EXPECT_TRUE(exec_result.success()) << "Execution failed: " << exec_result.error();
        if (!exec_result.success()) return "";

        auto result_set = exec_result.resultSet();
        EXPECT_NE(result_set, nullptr);
        if (!result_set) return "";

        EXPECT_EQ(result_set->rowCount(), 1) << "Expected 1 row";
        EXPECT_EQ(result_set->columnCount(), 1) << "Expected 1 column";
        if (result_set->rowCount() != 1 || result_set->columnCount() != 1) return "";

        return result_set->getValue(0, 0).toString();
    }
};

// ===== Byte Access Functions =====

TEST_F(BitManipulationTest, GET_BYTE_Function)
{
    // Test extracting first byte of 'hello' (ASCII 'h' = 104)
    int64_t result = executeScalarInt("SELECT GET_BYTE('hello', 0)");
    EXPECT_EQ(result, 104);

    // Test extracting second byte (ASCII 'e' = 101)
    result = executeScalarInt("SELECT GET_BYTE('hello', 1)");
    EXPECT_EQ(result, 101);

    // Test extracting last byte (ASCII 'o' = 111)
    result = executeScalarInt("SELECT GET_BYTE('hello', 4)");
    EXPECT_EQ(result, 111);
}

TEST_F(BitManipulationTest, SET_BYTE_Function)
{
    // Test setting first byte to 'H' (ASCII 72)
    std::string result = executeScalarString("SELECT SET_BYTE('hello', 0, 72)");
    EXPECT_EQ(result, "Hello");

    // Test setting middle byte
    result = executeScalarString("SELECT SET_BYTE('hello', 2, 88)");  // 'X'
    EXPECT_EQ(result, "heXlo");
}

TEST_F(BitManipulationTest, GET_BIT_Function)
{
    // Test getting MSB of byte 0x80 (binary 10000000)
    // Using char with value 128 (0x80)
    int64_t result = executeScalarInt("SELECT GET_BIT(CHR(128), 0)");
    EXPECT_EQ(result, 1);  // MSB is set

    // Test getting LSB of byte 0x80
    result = executeScalarInt("SELECT GET_BIT(CHR(128), 7)");
    EXPECT_EQ(result, 0);  // LSB is not set

    // Test getting bit from 'A' (ASCII 65 = 01000001)
    result = executeScalarInt("SELECT GET_BIT('A', 1)");
    EXPECT_EQ(result, 1);  // Second bit from MSB is set
}

TEST_F(BitManipulationTest, SET_BIT_Function)
{
    // Test setting MSB of null byte
    std::string result = executeScalarString("SELECT SET_BIT(CHR(0), 0, 1)");
    EXPECT_EQ(result.length(), 1);
    EXPECT_EQ(static_cast<uint8_t>(result[0]), 128);  // 0x80

    // Test clearing MSB
    result = executeScalarString("SELECT SET_BIT(CHR(128), 0, 0)");
    EXPECT_EQ(result.length(), 1);
    EXPECT_EQ(static_cast<uint8_t>(result[0]), 0);  // 0x00
}

// ===== Bitwise Operations =====

TEST_F(BitManipulationTest, BIT_AND_Function)
{
    // Test 12 & 10 = 8 (binary: 1100 & 1010 = 1000)
    int64_t result = executeScalarInt("SELECT BIT_AND(12, 10)");
    EXPECT_EQ(result, 8);

    // Test with 0
    result = executeScalarInt("SELECT BIT_AND(255, 0)");
    EXPECT_EQ(result, 0);

    // Test with all bits set
    result = executeScalarInt("SELECT BIT_AND(255, 255)");
    EXPECT_EQ(result, 255);
}

TEST_F(BitManipulationTest, BIT_OR_Function)
{
    // Test 12 | 10 = 14 (binary: 1100 | 1010 = 1110)
    int64_t result = executeScalarInt("SELECT BIT_OR(12, 10)");
    EXPECT_EQ(result, 14);

    // Test with 0
    result = executeScalarInt("SELECT BIT_OR(10, 0)");
    EXPECT_EQ(result, 10);

    // Test combining bits
    result = executeScalarInt("SELECT BIT_OR(1, 2)");
    EXPECT_EQ(result, 3);  // 01 | 10 = 11
}

TEST_F(BitManipulationTest, BIT_XOR_Function)
{
    // Test 12 ^ 10 = 6 (binary: 1100 ^ 1010 = 0110)
    int64_t result = executeScalarInt("SELECT BIT_XOR(12, 10)");
    EXPECT_EQ(result, 6);

    // Test XOR with itself (should be 0)
    result = executeScalarInt("SELECT BIT_XOR(255, 255)");
    EXPECT_EQ(result, 0);

    // Test XOR with 0 (should be unchanged)
    result = executeScalarInt("SELECT BIT_XOR(123, 0)");
    EXPECT_EQ(result, 123);
}

TEST_F(BitManipulationTest, BIT_NOT_Function)
{
    // Test ~5 = -6 (two's complement)
    int64_t result = executeScalarInt("SELECT BIT_NOT(5)");
    EXPECT_EQ(result, -6);

    // Test ~0 = -1
    result = executeScalarInt("SELECT BIT_NOT(0)");
    EXPECT_EQ(result, -1);

    // Test ~(-1) = 0
    result = executeScalarInt("SELECT BIT_NOT(-1)");
    EXPECT_EQ(result, 0);
}

// ===== Shift Operations =====

TEST_F(BitManipulationTest, BIT_SHIFT_LEFT_Function)
{
    // Test 5 << 2 = 20 (binary: 101 << 2 = 10100)
    int64_t result = executeScalarInt("SELECT BIT_SHIFT_LEFT(5, 2)");
    EXPECT_EQ(result, 20);

    // Test shift by 0
    result = executeScalarInt("SELECT BIT_SHIFT_LEFT(10, 0)");
    EXPECT_EQ(result, 10);

    // Test power of 2 (1 << n)
    result = executeScalarInt("SELECT BIT_SHIFT_LEFT(1, 10)");
    EXPECT_EQ(result, 1024);
}

TEST_F(BitManipulationTest, BIT_SHIFT_RIGHT_Function)
{
    // Test 20 >> 2 = 5 (binary: 10100 >> 2 = 101)
    int64_t result = executeScalarInt("SELECT BIT_SHIFT_RIGHT(20, 2)");
    EXPECT_EQ(result, 5);

    // Test shift by 0
    result = executeScalarInt("SELECT BIT_SHIFT_RIGHT(10, 0)");
    EXPECT_EQ(result, 10);

    // Test arithmetic right shift (sign extension)
    result = executeScalarInt("SELECT BIT_SHIFT_RIGHT(-8, 1)");
    EXPECT_EQ(result, -4);
}

TEST_F(BitManipulationTest, BIT_SHIFT_RIGHT_LOGICAL_Function)
{
    // Test logical right shift (zero-fill)
    // For positive numbers, same as arithmetic shift
    int64_t result = executeScalarInt("SELECT BIT_SHIFT_RIGHT_LOGICAL(20, 2)");
    EXPECT_EQ(result, 5);

    // Test shift by 0
    result = executeScalarInt("SELECT BIT_SHIFT_RIGHT_LOGICAL(10, 0)");
    EXPECT_EQ(result, 10);
}

// ===== Utility Functions =====

TEST_F(BitManipulationTest, BIT_COUNT_Function)
{
    // Test population count (Brian Kernighan's algorithm)
    // 7 = binary 111 (3 ones)
    int64_t result = executeScalarInt("SELECT BIT_COUNT(7)");
    EXPECT_EQ(result, 3);

    // 15 = binary 1111 (4 ones)
    result = executeScalarInt("SELECT BIT_COUNT(15)");
    EXPECT_EQ(result, 4);

    // 0 = binary 0 (0 ones)
    result = executeScalarInt("SELECT BIT_COUNT(0)");
    EXPECT_EQ(result, 0);

    // 255 = binary 11111111 (8 ones)
    result = executeScalarInt("SELECT BIT_COUNT(255)");
    EXPECT_EQ(result, 8);

    // Powers of 2 have exactly 1 bit set
    result = executeScalarInt("SELECT BIT_COUNT(1024)");
    EXPECT_EQ(result, 1);
}

TEST_F(BitManipulationTest, BIT_LENGTH_Function)
{
    // Test 'hello' (5 bytes = 40 bits)
    int64_t result = executeScalarInt("SELECT BIT_LENGTH('hello')");
    EXPECT_EQ(result, 40);

    // Test empty string
    result = executeScalarInt("SELECT BIT_LENGTH('')");
    EXPECT_EQ(result, 0);

    // Test single character
    result = executeScalarInt("SELECT BIT_LENGTH('A')");
    EXPECT_EQ(result, 8);
}

TEST_F(BitManipulationTest, BIT_MASK_Function)
{
    // Test creating mask of 4 ones: 1111 = 15
    int64_t result = executeScalarInt("SELECT BIT_MASK(4)");
    EXPECT_EQ(result, 15);

    // Test mask of 8 ones: 11111111 = 255
    result = executeScalarInt("SELECT BIT_MASK(8)");
    EXPECT_EQ(result, 255);

    // Test mask of 0 ones: 0
    result = executeScalarInt("SELECT BIT_MASK(0)");
    EXPECT_EQ(result, 0);

    // Test mask of 1 one: 1
    result = executeScalarInt("SELECT BIT_MASK(1)");
    EXPECT_EQ(result, 1);

    // Test mask of 16 ones
    result = executeScalarInt("SELECT BIT_MASK(16)");
    EXPECT_EQ(result, 65535);
}

// ===== Combined Operations Tests =====

TEST_F(BitManipulationTest, CombinedOperations_ExtractAndSetBits)
{
    // Test extracting a byte, modifying it with bitwise ops, and setting it back
    // Example: Get byte, set a bit, put it back
    std::string sql = "SELECT SET_BYTE('hello', 0, BIT_OR(GET_BYTE('hello', 0), 128))";
    std::string result = executeScalarString(sql);

    // First byte of 'hello' is 'h' (104 = 01101000)
    // OR with 128 (10000000) gives 232 (11101000)
    EXPECT_EQ(result.length(), 5);
    EXPECT_EQ(static_cast<uint8_t>(result[0]), 232);
}

TEST_F(BitManipulationTest, CombinedOperations_BitwiseMasking)
{
    // Test using BIT_MASK to extract lower bits
    // Extract lower 4 bits of 255
    int64_t result = executeScalarInt("SELECT BIT_AND(255, BIT_MASK(4))");
    EXPECT_EQ(result, 15);  // Lower 4 bits of 11111111 are 1111

    // Extract lower 3 bits of 10
    result = executeScalarInt("SELECT BIT_AND(10, BIT_MASK(3))");
    EXPECT_EQ(result, 2);  // Lower 3 bits of 1010 are 010
}

TEST_F(BitManipulationTest, CombinedOperations_ShiftAndMask)
{
    // Test extracting specific bits using shift and mask
    // Extract bits 4-7 of byte 170 (10101010)
    // Shift right by 4: 00001010
    // Mask lower 4 bits: 00001010 & 00001111 = 00001010 (10)
    int64_t result = executeScalarInt("SELECT BIT_AND(BIT_SHIFT_RIGHT(170, 4), BIT_MASK(4))");
    EXPECT_EQ(result, 10);
}

TEST_F(BitManipulationTest, CombinedOperations_CountBitsInRange)
{
    // Count set bits in lower byte of a larger number
    int64_t result = executeScalarInt("SELECT BIT_COUNT(BIT_AND(1023, BIT_MASK(8)))");
    // 1023 = 1111111111, lower 8 bits = 11111111 = 8 set bits
    EXPECT_EQ(result, 8);
}
