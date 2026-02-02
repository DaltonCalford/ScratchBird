/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include "scratchbird/core/status.h"

namespace scratchbird::core
{
    // Forward declaration
    struct ErrorContext;

    /**
     * UTF-8 Utility Functions
     * Provides character counting, validation, and manipulation for UTF-8 strings
     *
     * Important: SQL standard requires 128 CHARACTER limit for identifiers,
     * not 128 BYTES. UTF-8 characters can be 1-4 bytes each.
     */
    class UTF8Utils
    {
    public:
        /**
         * Count number of UTF-8 characters (code points) in string
         * Does NOT count bytes, counts actual characters
         *
         * @param str UTF-8 encoded string
         * @return Number of characters, or 0 if invalid UTF-8
         *
         * Examples:
         *   "hello" -> 5 characters (5 bytes)
         *   "café" -> 4 characters (5 bytes, é is 2 bytes)
         *   "你好" -> 2 characters (6 bytes, each CJK char is 3 bytes)
         *   "🎉" -> 1 character (4 bytes, emoji)
         */
        static size_t countCharacters(std::string_view str);

        /**
         * Validate UTF-8 encoding
         * @param str String to validate
         * @return true if valid UTF-8
         */
        static bool isValidUTF8(std::string_view str);

        /**
         * Get length of UTF-8 character at position (in bytes)
         * @param first_byte First byte of UTF-8 character
         * @return 1-4 bytes, or 0 if invalid
         */
        static size_t getCharacterLength(uint8_t first_byte);

        /**
         * Truncate string to maximum number of characters
         * Ensures result is valid UTF-8 (won't cut in middle of character)
         *
         * @param str UTF-8 string
         * @param max_chars Maximum number of characters
         * @return Truncated string (may be shorter in characters if truncation falls on multi-byte
         * char)
         */
        static std::string truncate(std::string_view str, size_t max_chars);

        /**
         * Get substring by character position (not byte position)
         * @param str UTF-8 string
         * @param start_char Character position to start (0-based)
         * @param num_chars Number of characters to extract
         * @return Substring
         */
        static std::string substring(std::string_view str, size_t start_char, size_t num_chars);

        /**
         * Decode next UTF-8 character to Unicode code point
         * @param str UTF-8 string
         * @param pos Position in string (in bytes), updated to next character
         * @return Unicode code point, or nullopt if invalid
         */
        static std::optional<uint32_t> decodeChar(std::string_view str, size_t &pos);

        /**
         * Encode Unicode code point to UTF-8
         * @param codepoint Unicode code point
         * @return UTF-8 encoded string, or empty if invalid code point
         */
        static std::string encodeChar(uint32_t codepoint);

        /**
         * Check if byte is a UTF-8 continuation byte (10xxxxxx)
         * @param byte Byte to check
         * @return true if continuation byte
         */
        static bool isContinuationByte(uint8_t byte);

        /**
         * Validate identifier length (SQL standard: 128 characters)
         * @param identifier Identifier string
         * @return true if identifier is valid (1-128 characters)
         */
        static bool isValidIdentifierLength(std::string_view identifier);

        /**
         * Truncate UTF-8 string to fit in maximum byte count without splitting characters
         *
         * Ensures the resulting string:
         * - Fits within max_bytes (including null terminator)
         * - Does not split UTF-8 multi-byte characters
         * - Is valid UTF-8
         *
         * @param str Input string
         * @param max_bytes Maximum byte count (including space for null terminator)
         * @return Truncated string that fits in max_bytes without splitting UTF-8 characters
         *
         * Examples:
         *   truncateToBytes("hello", 128) → "hello" (5 bytes + null = 6 bytes)
         *   truncateToBytes("hello world", 6) → "hello" (5 bytes + null = 6 bytes)
         *   truncateToBytes("你好世界", 7) → "你好" (6 bytes + null = 7 bytes)
         *   truncateToBytes("你好世界", 6) → "你" (3 bytes + null = 4 bytes, "你好" would be 7)
         */
        static std::string truncateToBytes(std::string_view str, size_t max_bytes);

        /**
         * Validate that a UTF-8 string can be stored in fixed-size storage
         *
         * Checks both character count and byte count limits.
         *
         * @param str Input string
         * @param max_chars Maximum character count (SQL standard: 128)
         * @param max_bytes Maximum byte count (storage capacity including null terminator)
         * @param ctx Error context for detailed error messages (optional)
         * @return Status::OK if valid, error status with message otherwise
         *
         * Error Cases:
         * - Returns INVALID_ARGUMENT if character count exceeds max_chars
         * - Returns INVALID_ARGUMENT if byte count exceeds max_bytes
         *
         * Examples:
         *   validateStorageCapacity("hello", 128, 512, ctx) → Status::OK
         *   validateStorageCapacity(std::string(200, 'a'), 128, 512, ctx) → INVALID_ARGUMENT (too many chars)
         *   validateStorageCapacity(std::string(200, '你'), 200, 512, ctx) → INVALID_ARGUMENT (too many bytes)
         */
        static Status validateStorageCapacity(std::string_view str,
                                              size_t max_chars,
                                              size_t max_bytes,
                                              ErrorContext* ctx = nullptr);

    private:
        // Helper to check if code point is valid Unicode
        static bool isValidCodePoint(uint32_t codepoint);
    };

} // namespace scratchbird::core
