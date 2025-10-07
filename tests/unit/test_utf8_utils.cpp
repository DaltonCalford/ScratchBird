/**
 * Unit Tests for UTF-8 Utilities
 * Tests character counting, validation, truncation, and identifier length checking
 * Phase 1: Foundation Infrastructure
 */

#include "scratchbird/core/utf8_utils.h"
#include <gtest/gtest.h>
#include <string>

using namespace scratchbird::core;

class UTF8UtilsTest : public ::testing::Test
{
protected:
    // Helper to create invalid UTF-8 sequences
    std::string makeInvalidUTF8()
    {
        return std::string("\xFF\xFE\xFD");
    }

    std::string makeOverlongEncoding()
    {
        // Overlong encoding of 'A' (should be 1 byte, encoded as 2)
        return std::string("\xC0\x81");
    }

    std::string makeUTF16Surrogate()
    {
        // UTF-16 surrogate (0xD800) encoded in UTF-8 (invalid)
        return std::string("\xED\xA0\x80");
    }
};

// Test 1: ASCII Character Counting
TEST_F(UTF8UtilsTest, ASCIICharacterCounting)
{
    EXPECT_EQ(UTF8Utils::countCharacters(""), 0u);
    EXPECT_EQ(UTF8Utils::countCharacters("a"), 1u);
    EXPECT_EQ(UTF8Utils::countCharacters("hello"), 5u);
    EXPECT_EQ(UTF8Utils::countCharacters("Hello, World!"), 13u);
    EXPECT_EQ(UTF8Utils::countCharacters("0123456789"), 10u);
}

// Test 2: Multi-Byte Character Counting (2-byte characters)
TEST_F(UTF8UtilsTest, TwoByteCharacterCounting)
{
    // é = C3 A9 (2 bytes, 1 character)
    EXPECT_EQ(UTF8Utils::countCharacters("café"), 4u); // 4 chars, 5 bytes

    // Various Latin-1 characters
    EXPECT_EQ(UTF8Utils::countCharacters("ñoño"), 4u); // ñ is 2 bytes
    EXPECT_EQ(UTF8Utils::countCharacters("Zürich"), 6u); // ü is 2 bytes
    EXPECT_EQ(UTF8Utils::countCharacters("naïve"), 5u); // ï is 2 bytes
}

// Test 3: Multi-Byte Character Counting (3-byte characters)
TEST_F(UTF8UtilsTest, ThreeByteCharacterCounting)
{
    // CJK characters are 3 bytes each
    EXPECT_EQ(UTF8Utils::countCharacters("你好"), 2u); // 2 chars, 6 bytes
    EXPECT_EQ(UTF8Utils::countCharacters("日本語"), 3u); // 3 chars, 9 bytes
    EXPECT_EQ(UTF8Utils::countCharacters("Hello世界"), 7u); // 5 ASCII + 2 CJK

    // Korean characters
    EXPECT_EQ(UTF8Utils::countCharacters("한글"), 2u); // 2 chars, 6 bytes
}

// Test 4: Multi-Byte Character Counting (4-byte characters / emoji)
TEST_F(UTF8UtilsTest, FourByteCharacterCounting)
{
    // Emoji are typically 4 bytes
    EXPECT_EQ(UTF8Utils::countCharacters("🎉"), 1u); // 1 char, 4 bytes
    EXPECT_EQ(UTF8Utils::countCharacters("Hello 🌍"), 7u); // 6 ASCII + 1 emoji
    EXPECT_EQ(UTF8Utils::countCharacters("👍👎"), 2u); // 2 emoji

    // Mathematical symbols (4 bytes)
    EXPECT_EQ(UTF8Utils::countCharacters("𝕳𝖊𝖑𝖑𝖔"), 5u); // Fraktur letters
}

// Test 5: Mixed Multi-Byte Characters
TEST_F(UTF8UtilsTest, MixedMultiByteCharacters)
{
    // Mix of ASCII, 2-byte, 3-byte, and 4-byte
    std::string mixed = "Hello café 你好 🎉!";
    // H e l l o _ c a f é _ 你 好 _ 🎉 !
    // That's 16 characters total
    EXPECT_EQ(UTF8Utils::countCharacters(mixed), 16u);
}

// Test 6: UTF-8 Validation - Valid Sequences
TEST_F(UTF8UtilsTest, UTF8ValidationValid)
{
    EXPECT_TRUE(UTF8Utils::isValidUTF8(""));
    EXPECT_TRUE(UTF8Utils::isValidUTF8("Hello"));
    EXPECT_TRUE(UTF8Utils::isValidUTF8("café"));
    EXPECT_TRUE(UTF8Utils::isValidUTF8("你好"));
    EXPECT_TRUE(UTF8Utils::isValidUTF8("🎉"));
    EXPECT_TRUE(UTF8Utils::isValidUTF8("Hello café 你好 🎉!"));
}

// Test 7: UTF-8 Validation - Invalid Sequences
TEST_F(UTF8UtilsTest, UTF8ValidationInvalid)
{
    // Invalid start bytes
    EXPECT_FALSE(UTF8Utils::isValidUTF8("\xFF"));
    EXPECT_FALSE(UTF8Utils::isValidUTF8("\xFE"));
    EXPECT_FALSE(UTF8Utils::isValidUTF8(makeInvalidUTF8()));

    // Incomplete sequences
    EXPECT_FALSE(UTF8Utils::isValidUTF8("\xC3")); // Incomplete 2-byte
    EXPECT_FALSE(UTF8Utils::isValidUTF8("\xE0\xA0")); // Incomplete 3-byte
    EXPECT_FALSE(UTF8Utils::isValidUTF8("\xF0\x90\x80")); // Incomplete 4-byte

    // Invalid continuation bytes
    EXPECT_FALSE(UTF8Utils::isValidUTF8("hello\x80world")); // Standalone continuation byte
}

// Test 8: UTF-8 Validation - Overlong Encodings
TEST_F(UTF8UtilsTest, UTF8ValidationOverlongEncodings)
{
    // Overlong encoding of 'A' (should be 1 byte, not 2)
    EXPECT_FALSE(UTF8Utils::isValidUTF8(makeOverlongEncoding()));

    // Overlong encoding tests for different byte lengths
    // 2-byte overlong encoding of '/'
    EXPECT_FALSE(UTF8Utils::isValidUTF8(std::string("\xC0\xAF")));

    // 3-byte overlong encoding of 'A'
    EXPECT_FALSE(UTF8Utils::isValidUTF8(std::string("\xE0\x80\x81")));

    // 4-byte overlong encoding of 'A'
    EXPECT_FALSE(UTF8Utils::isValidUTF8(std::string("\xF0\x80\x80\x81")));
}

// Test 9: UTF-8 Validation - UTF-16 Surrogates (Invalid)
TEST_F(UTF8UtilsTest, UTF8ValidationUTF16Surrogates)
{
    // UTF-16 surrogates (0xD800-0xDFFF) are invalid in UTF-8
    EXPECT_FALSE(UTF8Utils::isValidUTF8(makeUTF16Surrogate()));

    // High surrogate (0xD800)
    EXPECT_FALSE(UTF8Utils::isValidUTF8(std::string("\xED\xA0\x80")));

    // Low surrogate (0xDC00)
    EXPECT_FALSE(UTF8Utils::isValidUTF8(std::string("\xED\xB0\x80")));
}

// Test 10: UTF-8 Validation - Maximum Valid Code Point
TEST_F(UTF8UtilsTest, UTF8ValidationMaxCodePoint)
{
    // Maximum valid Unicode code point (U+10FFFF)
    // F4 8F BF BF
    EXPECT_TRUE(UTF8Utils::isValidUTF8(std::string("\xF4\x8F\xBF\xBF")));

    // Just above maximum (U+110000) - invalid
    EXPECT_FALSE(UTF8Utils::isValidUTF8(std::string("\xF4\x90\x80\x80")));
}

// Test 11: Truncate to Character Boundary
TEST_F(UTF8UtilsTest, TruncateToCharacterBoundary)
{
    EXPECT_EQ(UTF8Utils::truncate("hello", 3), "hel");
    EXPECT_EQ(UTF8Utils::truncate("hello", 10), "hello"); // More than length

    // Multi-byte characters
    EXPECT_EQ(UTF8Utils::truncate("café", 3), "caf"); // Stops before é
    EXPECT_EQ(UTF8Utils::truncate("你好世界", 2), "你好"); // 2 chars, 6 bytes

    // Emoji
    EXPECT_EQ(UTF8Utils::truncate("Hello 🎉!", 7), "Hello 🎉");
}

// Test 12: Truncate Respects Character Boundaries
TEST_F(UTF8UtilsTest, TruncateRespectsCharacterBoundaries)
{
    std::string str = "café";
    std::string truncated = UTF8Utils::truncate(str, 3);

    // Should not cut in middle of é (2 bytes)
    EXPECT_EQ(truncated, "caf");
    EXPECT_EQ(UTF8Utils::countCharacters(truncated), 3u);
    EXPECT_TRUE(UTF8Utils::isValidUTF8(truncated));
}

// Test 13: Substring by Character Position
TEST_F(UTF8UtilsTest, SubstringByCharacterPosition)
{
    EXPECT_EQ(UTF8Utils::substring("hello", 0, 5), "hello");
    EXPECT_EQ(UTF8Utils::substring("hello", 1, 3), "ell");
    EXPECT_EQ(UTF8Utils::substring("hello", 2, 2), "ll");

    // Multi-byte characters
    EXPECT_EQ(UTF8Utils::substring("café", 0, 4), "café");
    EXPECT_EQ(UTF8Utils::substring("café", 2, 2), "fé");

    // CJK characters
    EXPECT_EQ(UTF8Utils::substring("你好世界", 1, 2), "好世");
}

// Test 14: Substring Edge Cases
TEST_F(UTF8UtilsTest, SubstringEdgeCases)
{
    // Start beyond end
    EXPECT_EQ(UTF8Utils::substring("hello", 10, 5), "");

    // Length exceeds remaining
    EXPECT_EQ(UTF8Utils::substring("hello", 3, 100), "lo");

    // Zero length
    EXPECT_EQ(UTF8Utils::substring("hello", 2, 0), "");
}

// Test 15: Identifier Length Validation - Valid
TEST_F(UTF8UtilsTest, IdentifierLengthValidationValid)
{
    EXPECT_TRUE(UTF8Utils::isValidIdentifierLength("a")); // 1 char
    EXPECT_TRUE(UTF8Utils::isValidIdentifierLength("table_name")); // 10 chars
    EXPECT_TRUE(UTF8Utils::isValidIdentifierLength(std::string(128, 'x'))); // 128 chars
    EXPECT_TRUE(UTF8Utils::isValidIdentifierLength("café_table")); // 11 chars, 12 bytes
    EXPECT_TRUE(UTF8Utils::isValidIdentifierLength("你好_table")); // 9 chars, 15 bytes
}

// Test 16: Identifier Length Validation - Invalid
TEST_F(UTF8UtilsTest, IdentifierLengthValidationInvalid)
{
    EXPECT_FALSE(UTF8Utils::isValidIdentifierLength("")); // 0 chars (too short)
    EXPECT_FALSE(UTF8Utils::isValidIdentifierLength(std::string(129, 'x'))); // 129 chars
    EXPECT_FALSE(UTF8Utils::isValidIdentifierLength(std::string(200, 'x'))); // 200 chars

    // Multi-byte characters exceeding 128 characters
    std::string long_cjk;
    for (int i = 0; i < 129; ++i) {
        long_cjk += "你"; // 129 characters, 387 bytes
    }
    EXPECT_FALSE(UTF8Utils::isValidIdentifierLength(long_cjk));
}

// Test 17: Get Character Length
TEST_F(UTF8UtilsTest, GetCharacterLength)
{
    // ASCII (1 byte)
    EXPECT_EQ(UTF8Utils::getCharacterLength('A'), 1u);
    EXPECT_EQ(UTF8Utils::getCharacterLength('0'), 1u);

    // 2-byte start (110xxxxx)
    EXPECT_EQ(UTF8Utils::getCharacterLength(0xC3), 2u); // é starts with C3

    // 3-byte start (1110xxxx)
    EXPECT_EQ(UTF8Utils::getCharacterLength(0xE4), 3u); // CJK starts with E4

    // 4-byte start (11110xxx)
    EXPECT_EQ(UTF8Utils::getCharacterLength(0xF0), 4u); // Emoji starts with F0

    // Invalid start bytes
    EXPECT_EQ(UTF8Utils::getCharacterLength(0x80), 0u); // Continuation byte
    EXPECT_EQ(UTF8Utils::getCharacterLength(0xFF), 0u); // Invalid
}

// Test 18: Decode Character
TEST_F(UTF8UtilsTest, DecodeCharacter)
{
    size_t pos = 0;

    // ASCII
    std::string ascii = "A";
    pos = 0;
    auto cp1 = UTF8Utils::decodeChar(ascii, pos);
    ASSERT_TRUE(cp1.has_value());
    EXPECT_EQ(cp1.value(), 0x41u); // 'A'
    EXPECT_EQ(pos, 1u);

    // 2-byte character (é = U+00E9)
    std::string two_byte = "é";
    pos = 0;
    auto cp2 = UTF8Utils::decodeChar(two_byte, pos);
    ASSERT_TRUE(cp2.has_value());
    EXPECT_EQ(cp2.value(), 0xE9u);
    EXPECT_EQ(pos, 2u);

    // 3-byte character (你 = U+4F60)
    std::string three_byte = "你";
    pos = 0;
    auto cp3 = UTF8Utils::decodeChar(three_byte, pos);
    ASSERT_TRUE(cp3.has_value());
    EXPECT_EQ(cp3.value(), 0x4F60u);
    EXPECT_EQ(pos, 3u);

    // 4-byte character (🎉 = U+1F389)
    std::string four_byte = "🎉";
    pos = 0;
    auto cp4 = UTF8Utils::decodeChar(four_byte, pos);
    ASSERT_TRUE(cp4.has_value());
    EXPECT_EQ(cp4.value(), 0x1F389u);
    EXPECT_EQ(pos, 4u);
}

// Test 19: Decode Invalid Character
TEST_F(UTF8UtilsTest, DecodeInvalidCharacter)
{
    size_t pos = 0;

    // Invalid start byte
    std::string invalid = "\xFF";
    pos = 0;
    auto cp1 = UTF8Utils::decodeChar(invalid, pos);
    EXPECT_FALSE(cp1.has_value());

    // Incomplete sequence
    std::string incomplete = "\xC3";
    pos = 0;
    auto cp2 = UTF8Utils::decodeChar(incomplete, pos);
    EXPECT_FALSE(cp2.has_value());

    // Overlong encoding
    pos = 0;
    auto cp3 = UTF8Utils::decodeChar(makeOverlongEncoding(), pos);
    EXPECT_FALSE(cp3.has_value());

    // UTF-16 surrogate
    pos = 0;
    auto cp4 = UTF8Utils::decodeChar(makeUTF16Surrogate(), pos);
    EXPECT_FALSE(cp4.has_value());
}

// Test 20: Encode Character
TEST_F(UTF8UtilsTest, EncodeCharacter)
{
    // ASCII
    EXPECT_EQ(UTF8Utils::encodeChar(0x41), "A");
    EXPECT_EQ(UTF8Utils::encodeChar(0x30), "0");

    // 2-byte character (é = U+00E9)
    EXPECT_EQ(UTF8Utils::encodeChar(0xE9), "é");

    // 3-byte character (你 = U+4F60)
    EXPECT_EQ(UTF8Utils::encodeChar(0x4F60), "你");

    // 4-byte character (🎉 = U+1F389)
    EXPECT_EQ(UTF8Utils::encodeChar(0x1F389), "🎉");

    // Invalid code points
    EXPECT_EQ(UTF8Utils::encodeChar(0x110000), ""); // Above maximum
    EXPECT_EQ(UTF8Utils::encodeChar(0xD800), "");   // UTF-16 surrogate
    EXPECT_EQ(UTF8Utils::encodeChar(0xDFFF), "");   // UTF-16 surrogate
}

// Test 21: Encode-Decode Round Trip
TEST_F(UTF8UtilsTest, EncodeDecodeRoundTrip)
{
    std::vector<uint32_t> codepoints = {
        0x41,     // 'A'
        0xE9,     // 'é'
        0x4F60,   // '你'
        0x1F389   // '🎉'
    };

    for (uint32_t cp : codepoints) {
        std::string encoded = UTF8Utils::encodeChar(cp);
        EXPECT_FALSE(encoded.empty());

        size_t pos = 0;
        auto decoded = UTF8Utils::decodeChar(encoded, pos);
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(decoded.value(), cp);
    }
}

// Test 22: Count Invalid UTF-8 Returns Zero
TEST_F(UTF8UtilsTest, CountInvalidUTF8ReturnsZero)
{
    // Invalid start bytes
    EXPECT_EQ(UTF8Utils::countCharacters(makeInvalidUTF8()), 0u);
    EXPECT_EQ(UTF8Utils::countCharacters("\xFF"), 0u);

    // Note: countCharacters does basic validation (continuation bytes) but not advanced
    // validation (overlong encodings, surrogates). Use isValidUTF8() for full validation.
    EXPECT_FALSE(UTF8Utils::isValidUTF8(makeOverlongEncoding()));
    EXPECT_FALSE(UTF8Utils::isValidUTF8(makeUTF16Surrogate()));
}

// Test 23: Continuation Byte Check
TEST_F(UTF8UtilsTest, IsContinuationByte)
{
    // Valid continuation bytes (10xxxxxx)
    EXPECT_TRUE(UTF8Utils::isContinuationByte(0x80));
    EXPECT_TRUE(UTF8Utils::isContinuationByte(0xBF));
    EXPECT_TRUE(UTF8Utils::isContinuationByte(0xA0));

    // Not continuation bytes
    EXPECT_FALSE(UTF8Utils::isContinuationByte(0x00)); // ASCII
    EXPECT_FALSE(UTF8Utils::isContinuationByte(0x41)); // 'A'
    EXPECT_FALSE(UTF8Utils::isContinuationByte(0xC0)); // 2-byte start
    EXPECT_FALSE(UTF8Utils::isContinuationByte(0xE0)); // 3-byte start
    EXPECT_FALSE(UTF8Utils::isContinuationByte(0xF0)); // 4-byte start
    EXPECT_FALSE(UTF8Utils::isContinuationByte(0xFF)); // Invalid
}

// Test 24: Empty String Edge Cases
TEST_F(UTF8UtilsTest, EmptyStringEdgeCases)
{
    EXPECT_EQ(UTF8Utils::countCharacters(""), 0u);
    EXPECT_TRUE(UTF8Utils::isValidUTF8(""));
    EXPECT_EQ(UTF8Utils::truncate("", 10), "");
    EXPECT_EQ(UTF8Utils::substring("", 0, 10), "");
    EXPECT_FALSE(UTF8Utils::isValidIdentifierLength("")); // Empty is invalid
}

// Test 25: Practical SQL Identifier Examples
TEST_F(UTF8UtilsTest, PracticalSQLIdentifierExamples)
{
    // Valid identifiers
    EXPECT_TRUE(UTF8Utils::isValidIdentifierLength("users"));
    EXPECT_TRUE(UTF8Utils::isValidIdentifierLength("user_profiles"));
    EXPECT_TRUE(UTF8Utils::isValidIdentifierLength("café_orders"));
    EXPECT_TRUE(UTF8Utils::isValidIdentifierLength("用户表")); // "users table" in Chinese

    // Count characters correctly
    EXPECT_EQ(UTF8Utils::countCharacters("café_orders"), 11u); // Not 12 bytes
    EXPECT_EQ(UTF8Utils::countCharacters("用户表"), 3u);       // Not 9 bytes

    // Invalid: too long
    std::string long_identifier(129, 'x');
    EXPECT_FALSE(UTF8Utils::isValidIdentifierLength(long_identifier));
}
