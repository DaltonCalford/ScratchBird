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
#include <vector>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird::core
{

    /**
     * Character Set Enumeration
     *
     * Supported character encodings for string storage and manipulation.
     * UTF-8 is the default and recommended encoding.
     */
    enum class CharacterSet : uint16_t
    {
        ASCII = 0,   // 7-bit ASCII (1 byte per char)
        LATIN1 = 1,  // ISO-8859-1 (1 byte per char)
        UTF8 = 2,    // UTF-8 (1-4 bytes per char) - DEFAULT
        UTF16 = 3,   // UTF-16 (2-4 bytes per char)
        UTF32 = 4,   // UTF-32 (4 bytes per char)
        UTF8MB4 = 5, // UTF-8 with full Unicode support (MySQL compatible, same as UTF8)

        // Future extensions
        LATIN2 = 10,  // ISO-8859-2 (Central European)
        LATIN5 = 11,  // ISO-8859-9 (Turkish)
        LATIN7 = 12,  // ISO-8859-13 (Baltic)
        WIN1252 = 20, // Windows-1252 (Western European)
        WIN1251 = 21, // Windows-1251 (Cyrillic)
        SJIS = 30,    // Shift-JIS (Japanese)
        GBK = 31,     // GBK (Chinese)
        BIG5 = 32,    // Big5 (Traditional Chinese)
        EUC_KR = 33   // EUC-KR (Korean)
    };

    /**
     * Collation Type
     *
     * Defines how string comparison should be performed.
     */
    enum class CollationType : uint8_t
    {
        BINARY = 0,             // Byte-by-byte comparison (fastest)
        CASE_SENSITIVE = 1,     // Case-sensitive, accent-sensitive
        CASE_INSENSITIVE = 2,   // Case-insensitive, accent-sensitive (ci)
        ACCENT_INSENSITIVE = 3, // Case-sensitive, accent-insensitive (ai)
        CI_AI = 4,              // Case-insensitive, accent-insensitive
        UNICODE = 5,            // Unicode Collation Algorithm (UCA)
        NATURAL = 6,            // Natural/human sorting
        NUMERIC = 7             // Numeric substring sorting
    };

    /**
     * Collation Strength
     *
     * Determines the level of detail in string comparison.
     */
    enum class CollationStrength : uint8_t
    {
        PRIMARY = 1,    // Base characters only
        SECONDARY = 2,  // Base + accents
        TERTIARY = 3,   // Base + accents + case
        QUATERNARY = 4, // Base + accents + case + punctuation
        IDENTICAL = 5   // All differences matter (binary)
    };

    /**
     * Character Set Information
     *
     * Metadata about a character encoding.
     */
    struct CharacterSetInfo
    {
        CharacterSet id;
        std::string name;              // e.g., "utf8", "latin1"
        std::string description;       // Human-readable description
        uint8_t min_bytes;             // Minimum bytes per character
        uint8_t max_bytes;             // Maximum bytes per character
        bool variable_width;           // True for multi-byte encodings
        uint32_t default_collation_id; // Default collation ID
    };

    /**
     * Collation Information
     *
     * Metadata about a collation (string comparison rules).
     */
    struct CollationInfo
    {
        uint32_t id;
        std::string name;     // e.g., "utf8_general_ci"
        CharacterSet charset; // Associated character set
        CollationType type;
        CollationStrength strength;
        bool pad_space;     // PAD SPACE vs NO PAD
        std::string locale; // Locale string (e.g., "en_US")
        bool is_default;    // Default for this character set
    };

    // Forward declaration
    class Database;

    /**
     * Character Set Manager
     *
     * Manages character set and collation metadata.
     */
    class CharsetManager
    {
    public:
        CharsetManager();
        ~CharsetManager();

        // Loading from catalog
        auto loadFromCatalog(Database *db, ErrorContext *ctx = nullptr) -> Status;

        // Character set operations
        auto getCharsetInfo(CharacterSet charset) const -> const CharacterSetInfo *;
        auto getCharsetByName(const std::string &name) const -> CharacterSet;
        auto getCharsetName(CharacterSet charset) const -> std::string;
        auto getDefaultCharset() const -> CharacterSet
        {
            return CharacterSet::UTF8;
        }

        // Collation operations
        auto getCollationInfo(uint32_t collation_id) const -> const CollationInfo *;
        auto getCollationByName(const std::string &name) const -> uint32_t;
        auto getCollationName(uint32_t collation_id) const -> std::string;
        auto getDefaultCollation(CharacterSet charset) const -> uint32_t;

        // String length operations
        auto getCharLength(const uint8_t *str, uint32_t byte_len, CharacterSet charset) const
            -> uint32_t;
        auto getByteLength(const uint8_t *str, uint32_t char_count, CharacterSet charset) const
            -> uint32_t;
        auto getMaxBytesPerChar(CharacterSet charset) const -> uint8_t;

        // Validation
        auto validate(const uint8_t *str, uint32_t byte_len, CharacterSet charset,
                      ErrorContext *ctx = nullptr) const -> Status;

        // String comparison
        auto compare(const uint8_t *s1, uint32_t len1, const uint8_t *s2, uint32_t len2,
                     uint32_t collation_id) const -> int;

        // Character set conversion
        auto convert(const uint8_t *input, uint32_t input_len, CharacterSet from_cs,
                     std::vector<uint8_t> &output, CharacterSet to_cs,
                     ErrorContext *ctx = nullptr) const -> Status;

    private:
        void initializeCharsets();
        void initializeCollations();

        std::vector<CharacterSetInfo> charsets_;
        std::vector<CollationInfo> collations_;
    };

    // UTF-8 utility functions
    namespace utf8
    {
        // Validate UTF-8 byte sequence
        auto validate(const uint8_t *str, uint32_t byte_len) -> bool;

        // Get character count from UTF-8 byte sequence
        auto char_length(const uint8_t *str, uint32_t byte_len) -> uint32_t;

        // Get byte length for N characters
        auto byte_length(const uint8_t *str, uint32_t char_count) -> uint32_t;

        // Get length of first character in bytes
        auto char_byte_length(const uint8_t *str) -> uint32_t;

        // Convert to uppercase (simple ASCII-only for now)
        auto to_upper(const std::string &str) -> std::string;

        // Convert to lowercase (simple ASCII-only for now)
        auto to_lower(const std::string &str) -> std::string;

        // Compare case-insensitive (simple ASCII-only for now)
        auto compare_ci(const uint8_t *s1, uint32_t len1, const uint8_t *s2, uint32_t len2) -> int;

        // Compare binary (memcmp wrapper)
        auto compare_bin(const uint8_t *s1, uint32_t len1, const uint8_t *s2, uint32_t len2) -> int;
    }

    // UTF-16 utility functions
    namespace utf16
    {
        // Validate UTF-16 byte sequence (little-endian)
        auto validate(const uint8_t *str, uint32_t byte_len) -> bool;

        // Get character count from UTF-16 byte sequence
        auto char_length(const uint8_t *str, uint32_t byte_len) -> uint32_t;

        // Get byte length for N characters
        auto byte_length(const uint8_t *str, uint32_t char_count) -> uint32_t;

        // Get length of first character in bytes (2 or 4)
        auto char_byte_length(const uint8_t *str) -> uint32_t;
    }

    // UTF-32 utility functions
    namespace utf32
    {
        // Validate UTF-32 byte sequence (little-endian)
        auto validate(const uint8_t *str, uint32_t byte_len) -> bool;

        // Get character count from UTF-32 byte sequence
        auto char_length(const uint8_t *str, uint32_t byte_len) -> uint32_t;

        // Get byte length for N characters (always char_count * 4)
        auto byte_length(const uint8_t *str, uint32_t char_count) -> uint32_t;

        // Get length of first character in bytes (always 4)
        auto char_byte_length(const uint8_t *str) -> uint32_t;
    }

} // namespace scratchbird::core
