#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>

namespace scratchbird::core
{

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

    private:
        // Helper to check if code point is valid Unicode
        static bool isValidCodePoint(uint32_t codepoint);
    };

} // namespace scratchbird::core
