/**
 * UTF-8 Utilities Implementation
 * Provides character-based operations for UTF-8 strings
 * Phase 1: Foundation Infrastructure
 */

#include "scratchbird/core/utf8_utils.h"
#include <algorithm>

namespace scratchbird::core
{

    size_t UTF8Utils::countCharacters(std::string_view str)
    {
        size_t count = 0;
        size_t pos = 0;

        while (pos < str.length())
        {
            uint8_t byte = static_cast<uint8_t>(str[pos]);

            // Determine character length
            size_t char_len = getCharacterLength(byte);
            if (char_len == 0)
            {
                // Invalid UTF-8
                return 0;
            }

            // Check if we have enough bytes
            if (pos + char_len > str.length())
            {
                // Incomplete character at end
                return 0;
            }

            // Validate continuation bytes
            for (size_t i = 1; i < char_len; ++i)
            {
                if (!isContinuationByte(static_cast<uint8_t>(str[pos + i])))
                {
                    // Invalid continuation byte
                    return 0;
                }
            }

            count++;
            pos += char_len;
        }

        return count;
    }

    bool UTF8Utils::isValidUTF8(std::string_view str)
    {
        size_t pos = 0;

        while (pos < str.length())
        {
            uint8_t byte = static_cast<uint8_t>(str[pos]);
            size_t char_len = getCharacterLength(byte);

            if (char_len == 0)
            {
                return false;
            }

            if (pos + char_len > str.length())
            {
                return false;
            }

            // Validate continuation bytes
            for (size_t i = 1; i < char_len; ++i)
            {
                if (!isContinuationByte(static_cast<uint8_t>(str[pos + i])))
                {
                    return false;
                }
            }

            // Additional validation for overlong encodings and invalid code points
            auto codepoint = decodeChar(str, pos);
            if (!codepoint.has_value())
            {
                return false;
            }

            // pos already advanced by decodeChar
        }

        return true;
    }

    size_t UTF8Utils::getCharacterLength(uint8_t first_byte)
    {
        // ASCII (0xxxxxxx) - 1 byte
        if ((first_byte & 0x80) == 0)
        {
            return 1;
        }

        // 2-byte character (110xxxxx)
        if ((first_byte & 0xE0) == 0xC0)
        {
            return 2;
        }

        // 3-byte character (1110xxxx)
        if ((first_byte & 0xF0) == 0xE0)
        {
            return 3;
        }

        // 4-byte character (11110xxx)
        if ((first_byte & 0xF8) == 0xF0)
        {
            return 4;
        }

        // Invalid start byte
        return 0;
    }

    std::string UTF8Utils::truncate(std::string_view str, size_t max_chars)
    {
        size_t char_count = 0;
        size_t byte_pos = 0;

        while (byte_pos < str.length() && char_count < max_chars)
        {
            size_t char_len = getCharacterLength(static_cast<uint8_t>(str[byte_pos]));
            if (char_len == 0)
            {
                // Invalid UTF-8, truncate here
                break;
            }

            // Check if adding this character would exceed limits
            if (byte_pos + char_len > str.length())
            {
                // Incomplete character, truncate before it
                break;
            }

            byte_pos += char_len;
            char_count++;
        }

        return std::string(str.substr(0, byte_pos));
    }

    std::string UTF8Utils::substring(std::string_view str, size_t start_char, size_t num_chars)
    {
        size_t char_count = 0;
        size_t byte_start = 0;
        size_t byte_end = 0;
        size_t byte_pos = 0;

        // Find start position
        while (byte_pos < str.length() && char_count < start_char)
        {
            size_t char_len = getCharacterLength(static_cast<uint8_t>(str[byte_pos]));
            if (char_len == 0)
                break;
            byte_pos += char_len;
            char_count++;
        }

        byte_start = byte_pos;
        char_count = 0;

        // Find end position
        while (byte_pos < str.length() && char_count < num_chars)
        {
            size_t char_len = getCharacterLength(static_cast<uint8_t>(str[byte_pos]));
            if (char_len == 0)
                break;
            byte_pos += char_len;
            char_count++;
        }

        byte_end = byte_pos;

        return std::string(str.substr(byte_start, byte_end - byte_start));
    }

    std::optional<uint32_t> UTF8Utils::decodeChar(std::string_view str, size_t &pos)
    {
        if (pos >= str.length())
        {
            return std::nullopt;
        }

        uint8_t first_byte = static_cast<uint8_t>(str[pos]);
        size_t char_len = getCharacterLength(first_byte);

        if (char_len == 0 || pos + char_len > str.length())
        {
            return std::nullopt;
        }

        uint32_t codepoint = 0;

        if (char_len == 1)
        {
            // ASCII
            codepoint = first_byte;
        }
        else if (char_len == 2)
        {
            // 2-byte character
            uint8_t byte2 = static_cast<uint8_t>(str[pos + 1]);
            if (!isContinuationByte(byte2))
            {
                return std::nullopt;
            }
            codepoint = ((first_byte & 0x1F) << 6) | (byte2 & 0x3F);

            // Check for overlong encoding
            if (codepoint < 0x80)
            {
                return std::nullopt;
            }
        }
        else if (char_len == 3)
        {
            // 3-byte character
            uint8_t byte2 = static_cast<uint8_t>(str[pos + 1]);
            uint8_t byte3 = static_cast<uint8_t>(str[pos + 2]);
            if (!isContinuationByte(byte2) || !isContinuationByte(byte3))
            {
                return std::nullopt;
            }
            codepoint = ((first_byte & 0x0F) << 12) | ((byte2 & 0x3F) << 6) | (byte3 & 0x3F);

            // Check for overlong encoding
            if (codepoint < 0x800)
            {
                return std::nullopt;
            }

            // Check for UTF-16 surrogates (invalid in UTF-8)
            if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
            {
                return std::nullopt;
            }
        }
        else if (char_len == 4)
        {
            // 4-byte character
            uint8_t byte2 = static_cast<uint8_t>(str[pos + 1]);
            uint8_t byte3 = static_cast<uint8_t>(str[pos + 2]);
            uint8_t byte4 = static_cast<uint8_t>(str[pos + 3]);
            if (!isContinuationByte(byte2) || !isContinuationByte(byte3) ||
                !isContinuationByte(byte4))
            {
                return std::nullopt;
            }
            codepoint = ((first_byte & 0x07) << 18) | ((byte2 & 0x3F) << 12) |
                        ((byte3 & 0x3F) << 6) | (byte4 & 0x3F);

            // Check for overlong encoding
            if (codepoint < 0x10000)
            {
                return std::nullopt;
            }
        }

        // Validate code point
        if (!isValidCodePoint(codepoint))
        {
            return std::nullopt;
        }

        pos += char_len;
        return codepoint;
    }

    std::string UTF8Utils::encodeChar(uint32_t codepoint)
    {
        if (!isValidCodePoint(codepoint))
        {
            return "";
        }

        std::string result;

        if (codepoint <= 0x7F)
        {
            // 1-byte (ASCII)
            result += static_cast<char>(codepoint);
        }
        else if (codepoint <= 0x7FF)
        {
            // 2-byte
            result += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        else if (codepoint <= 0xFFFF)
        {
            // 3-byte
            result += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        else if (codepoint <= 0x10FFFF)
        {
            // 4-byte
            result += static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
            result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }

        return result;
    }

    bool UTF8Utils::isContinuationByte(uint8_t byte)
    {
        return (byte & 0xC0) == 0x80;
    }

    bool UTF8Utils::isValidIdentifierLength(std::string_view identifier)
    {
        size_t char_count = countCharacters(identifier);
        return char_count >= 1 && char_count <= 128;
    }

    bool UTF8Utils::isValidCodePoint(uint32_t codepoint)
    {
        // Maximum valid Unicode code point
        if (codepoint > 0x10FFFF)
        {
            return false;
        }

        // UTF-16 surrogates are invalid in UTF-8
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
        {
            return false;
        }

        return true;
    }

} // namespace scratchbird::core
