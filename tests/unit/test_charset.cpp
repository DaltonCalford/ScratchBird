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
#include "scratchbird/core/charset.h"
#include "scratchbird/core/error_context.h"
#include <vector>
#include <string>

using namespace scratchbird::core;

class CharsetTest : public ::testing::Test
{
protected:
    CharsetManager charset_manager;
    ErrorContext error_ctx;
};

// ===== UTF-8 Validation Tests =====

TEST_F(CharsetTest, UTF8_ValidateASCII)
{
    std::string ascii_str = "Hello World";
    EXPECT_TRUE(utf8::validate(
        reinterpret_cast<const uint8_t*>(ascii_str.data()),
        ascii_str.length()
    ));
}

TEST_F(CharsetTest, UTF8_Validate2ByteSequence)
{
    // "café" - last character is é (U+00E9, 2-byte in UTF-8: 0xC3 0xA9)
    std::string str = "café";
    EXPECT_TRUE(utf8::validate(
        reinterpret_cast<const uint8_t*>(str.data()),
        str.length()
    ));
}

TEST_F(CharsetTest, UTF8_Validate3ByteSequence)
{
    // "你好" - Chinese characters (3 bytes each in UTF-8)
    std::string str = "你好";
    EXPECT_TRUE(utf8::validate(
        reinterpret_cast<const uint8_t*>(str.data()),
        str.length()
    ));
}

TEST_F(CharsetTest, UTF8_Validate4ByteSequence)
{
    // Emoji: "😀" (U+1F600, 4 bytes in UTF-8: 0xF0 0x9F 0x98 0x80)
    std::string str = "😀";
    EXPECT_TRUE(utf8::validate(
        reinterpret_cast<const uint8_t*>(str.data()),
        str.length()
    ));
}

TEST_F(CharsetTest, UTF8_ValidateInvalidSequence)
{
    // Invalid UTF-8: orphan continuation byte
    uint8_t invalid[] = {0x80, 0x41}; // 10000000 followed by 'A'
    EXPECT_FALSE(utf8::validate(invalid, 2));
}

TEST_F(CharsetTest, UTF8_ValidateInvalidStartByte)
{
    // Invalid UTF-8: invalid start byte
    uint8_t invalid[] = {0xFF, 0x41};
    EXPECT_FALSE(utf8::validate(invalid, 2));
}

TEST_F(CharsetTest, UTF8_ValidateTruncatedSequence)
{
    // Invalid UTF-8: truncated 2-byte sequence
    uint8_t invalid[] = {0xC3}; // Start of 2-byte sequence but no continuation
    EXPECT_FALSE(utf8::validate(invalid, 1));
}

// ===== UTF-8 Length Tests =====

TEST_F(CharsetTest, UTF8_CharLength_ASCII)
{
    std::string str = "Hello";
    uint32_t len = utf8::char_length(
        reinterpret_cast<const uint8_t*>(str.data()),
        str.length()
    );
    EXPECT_EQ(len, 5U); // 5 characters
}

TEST_F(CharsetTest, UTF8_CharLength_Mixed)
{
    std::string str = "café"; // c(1) a(1) f(1) é(2 bytes, 1 char) = 4 chars, 5 bytes
    uint32_t len = utf8::char_length(
        reinterpret_cast<const uint8_t*>(str.data()),
        str.length()
    );
    EXPECT_EQ(len, 4U); // 4 characters
    EXPECT_EQ(str.length(), 5U); // 5 bytes
}

TEST_F(CharsetTest, UTF8_CharLength_Emoji)
{
    std::string str = "Hi😀"; // H(1) i(1) 😀(4 bytes, 1 char) = 3 chars, 6 bytes
    uint32_t len = utf8::char_length(
        reinterpret_cast<const uint8_t*>(str.data()),
        str.length()
    );
    EXPECT_EQ(len, 3U); // 3 characters
    EXPECT_EQ(str.length(), 6U); // 6 bytes
}

TEST_F(CharsetTest, UTF8_ByteLength)
{
    std::string str = "café";
    const uint8_t* data = reinterpret_cast<const uint8_t*>(str.data());

    // Get byte length for first 2 characters: "ca"
    uint32_t byte_len = utf8::byte_length(data, 2);
    EXPECT_EQ(byte_len, 2U); // "ca" = 2 bytes

    // Get byte length for first 4 characters: "café"
    byte_len = utf8::byte_length(data, 4);
    EXPECT_EQ(byte_len, 5U); // "café" = 5 bytes (é is 2 bytes)
}

// ===== Character Set Conversion Tests =====

TEST_F(CharsetTest, Convert_IdentityUTF8)
{
    std::string input = "Hello World";
    std::vector<uint8_t> output;

    Status status = charset_manager.convert(
        reinterpret_cast<const uint8_t*>(input.data()),
        input.length(),
        CharacterSet::UTF8,
        output,
        CharacterSet::UTF8,
        &error_ctx
    );

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(output.size(), input.length());
    EXPECT_EQ(std::string(output.begin(), output.end()), input);
}

TEST_F(CharsetTest, Convert_Latin1ToUTF8)
{
    // Latin1: "café" where é is 0xE9 (single byte in Latin1)
    uint8_t latin1_input[] = {'c', 'a', 'f', 0xE9};
    std::vector<uint8_t> output;

    Status status = charset_manager.convert(
        latin1_input,
        4,
        CharacterSet::LATIN1,
        output,
        CharacterSet::UTF8,
        &error_ctx
    );

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(output.size(), 5U); // UTF-8: "caf" = 3 bytes, "é" = 2 bytes

    std::string result(output.begin(), output.end());
    EXPECT_EQ(result, "café");
}

TEST_F(CharsetTest, Convert_UTF8ToLatin1_Lossless)
{
    // UTF-8: "café" (5 bytes)
    std::string utf8_input = "café";
    std::vector<uint8_t> output;

    Status status = charset_manager.convert(
        reinterpret_cast<const uint8_t*>(utf8_input.data()),
        utf8_input.length(),
        CharacterSet::UTF8,
        output,
        CharacterSet::LATIN1,
        &error_ctx
    );

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(output.size(), 4U); // Latin1: "café" = 4 bytes
    EXPECT_EQ(output[3], 0xE9U); // é in Latin1
}

TEST_F(CharsetTest, Convert_UTF8ToLatin1_Lossy)
{
    // UTF-8: "你好" (Chinese, unmappable to Latin1)
    std::string utf8_input = "你好";
    std::vector<uint8_t> output;

    Status status = charset_manager.convert(
        reinterpret_cast<const uint8_t*>(utf8_input.data()),
        utf8_input.length(),
        CharacterSet::UTF8,
        output,
        CharacterSet::LATIN1,
        &error_ctx
    );

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(output.size(), 2U); // Two '?' characters
    EXPECT_EQ(output[0], '?');
    EXPECT_EQ(output[1], '?');
}

TEST_F(CharsetTest, Convert_UTF8ToASCII_Lossy)
{
    // UTF-8: "café"
    std::string utf8_input = "café";
    std::vector<uint8_t> output;

    Status status = charset_manager.convert(
        reinterpret_cast<const uint8_t*>(utf8_input.data()),
        utf8_input.length(),
        CharacterSet::UTF8,
        output,
        CharacterSet::ASCII,
        &error_ctx
    );

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(output.size(), 4U);
    EXPECT_EQ(std::string(output.begin(), output.end()), "caf?"); // é -> ?
}

// ===== Collation Tests =====

TEST_F(CharsetTest, Collation_BinaryComparison)
{
    std::string str1 = "abc";
    std::string str2 = "ABC";

    int result = charset_manager.compare(
        reinterpret_cast<const uint8_t*>(str1.data()), str1.length(),
        reinterpret_cast<const uint8_t*>(str2.data()), str2.length(),
        100 // utf8_bin
    );

    EXPECT_GT(result, 0); // "abc" > "ABC" in binary comparison (lowercase > uppercase in ASCII)
}

TEST_F(CharsetTest, Collation_CaseInsensitiveComparison)
{
    std::string str1 = "abc";
    std::string str2 = "ABC";

    int result = charset_manager.compare(
        reinterpret_cast<const uint8_t*>(str1.data()), str1.length(),
        reinterpret_cast<const uint8_t*>(str2.data()), str2.length(),
        101 // utf8_general_ci
    );

    EXPECT_EQ(result, 0); // "abc" == "ABC" in case-insensitive comparison
}

TEST_F(CharsetTest, Collation_CaseInsensitive_Different)
{
    std::string str1 = "abc";
    std::string str2 = "ABD";

    int result = charset_manager.compare(
        reinterpret_cast<const uint8_t*>(str1.data()), str1.length(),
        reinterpret_cast<const uint8_t*>(str2.data()), str2.length(),
        101 // utf8_general_ci
    );

    EXPECT_LT(result, 0); // "abc" < "ABD" even case-insensitive
}

// ===== CharsetManager Tests =====

TEST_F(CharsetTest, CharsetManager_GetCharsetInfo)
{
    const auto* info = charset_manager.getCharsetInfo(CharacterSet::UTF8);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->name, "utf8");
    EXPECT_EQ(info->min_bytes, 1U);
    EXPECT_EQ(info->max_bytes, 4U);
    EXPECT_TRUE(info->variable_width);
}

TEST_F(CharsetTest, CharsetManager_GetCharsetByName)
{
    CharacterSet cs = charset_manager.getCharsetByName("utf8");
    EXPECT_EQ(cs, CharacterSet::UTF8);

    cs = charset_manager.getCharsetByName("latin1");
    EXPECT_EQ(cs, CharacterSet::LATIN1);

    cs = charset_manager.getCharsetByName("ascii");
    EXPECT_EQ(cs, CharacterSet::ASCII);
}

TEST_F(CharsetTest, CharsetManager_GetCollationInfo)
{
    const auto* info = charset_manager.getCollationInfo(101); // utf8_general_ci
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->name, "utf8_general_ci");
    EXPECT_EQ(info->charset, CharacterSet::UTF8);
    EXPECT_EQ(info->type, CollationType::CASE_INSENSITIVE);
    EXPECT_TRUE(info->is_default); // Default for UTF-8
}

TEST_F(CharsetTest, CharsetManager_GetCollationByName)
{
    uint32_t collation_id = charset_manager.getCollationByName("utf8_bin");
    EXPECT_EQ(collation_id, 100U);

    collation_id = charset_manager.getCollationByName("utf8_general_ci");
    EXPECT_EQ(collation_id, 101U);
}

TEST_F(CharsetTest, CharsetManager_GetDefaultCollation)
{
    uint32_t collation_id = charset_manager.getDefaultCollation(CharacterSet::UTF8);
    EXPECT_EQ(collation_id, 101U); // utf8_general_ci
}

TEST_F(CharsetTest, CharsetManager_GetCharLength)
{
    std::string str = "café";
    uint32_t char_len = charset_manager.getCharLength(
        reinterpret_cast<const uint8_t*>(str.data()),
        str.length(),
        CharacterSet::UTF8
    );
    EXPECT_EQ(char_len, 4U); // 4 characters
}

TEST_F(CharsetTest, CharsetManager_GetMaxBytesPerChar)
{
    EXPECT_EQ(charset_manager.getMaxBytesPerChar(CharacterSet::ASCII), 1U);
    EXPECT_EQ(charset_manager.getMaxBytesPerChar(CharacterSet::LATIN1), 1U);
    EXPECT_EQ(charset_manager.getMaxBytesPerChar(CharacterSet::UTF8), 4U);
    EXPECT_EQ(charset_manager.getMaxBytesPerChar(CharacterSet::UTF8MB4), 4U);
}

// ===== String Function Tests =====

TEST_F(CharsetTest, UTF8_ToUpper_ASCII)
{
    std::string input = "hello world";
    std::string result = utf8::to_upper(input);
    EXPECT_EQ(result, "HELLO WORLD");
}

TEST_F(CharsetTest, UTF8_ToLower_ASCII)
{
    std::string input = "HELLO WORLD";
    std::string result = utf8::to_lower(input);
    EXPECT_EQ(result, "hello world");
}

TEST_F(CharsetTest, UTF8_CompareCaseInsensitive)
{
    std::string str1 = "Hello";
    std::string str2 = "HELLO";

    int result = utf8::compare_ci(
        reinterpret_cast<const uint8_t*>(str1.data()), str1.length(),
        reinterpret_cast<const uint8_t*>(str2.data()), str2.length()
    );

    EXPECT_EQ(result, 0); // Equal when case-insensitive
}

TEST_F(CharsetTest, UTF8_CompareBinary)
{
    std::string str1 = "abc";
    std::string str2 = "abc";

    int result = utf8::compare_bin(
        reinterpret_cast<const uint8_t*>(str1.data()), str1.length(),
        reinterpret_cast<const uint8_t*>(str2.data()), str2.length()
    );

    EXPECT_EQ(result, 0); // Equal

    str2 = "abd";
    result = utf8::compare_bin(
        reinterpret_cast<const uint8_t*>(str1.data()), str1.length(),
        reinterpret_cast<const uint8_t*>(str2.data()), str2.length()
    );

    EXPECT_LT(result, 0); // "abc" < "abd"
}
