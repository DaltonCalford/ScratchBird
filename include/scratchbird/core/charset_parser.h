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

#include <string>
#include <vector>
#include <cstdint>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird::core
{

// Character mapping structure (for explicit byte-to-Unicode mappings)
struct CharacterMapping
{
    uint32_t byte_sequence;     // 1-4 bytes (stored as uint32)
    uint32_t unicode_codepoint; // Unicode code point
    uint8_t byte_length;        // 1, 2, 3, or 4
};

// Character set definition
struct CharacterSet
{
    std::string name;            // e.g., "UTF-8", "ISO-8859-1"
    std::string description;     // Human-readable description
    uint8_t max_bytes;           // Maximum bytes per character
    uint8_t min_bytes;           // Minimum bytes per character
    bool is_variable_width;      // Variable vs fixed width
    std::vector<CharacterMapping> mappings; // Explicit mappings (empty for algorithmic encodings)
    std::string aliases;         // Comma-separated aliases
    std::string encoding_type;   // "unicode", "single_byte", "multi_byte"
    std::string iana_name;       // Official IANA name
};

// Collation definition
struct Collation
{
    std::string name;            // e.g., "utf8_general_ci"
    std::string charset_name;    // Parent character set
    bool case_insensitive;       // Case-insensitive comparison
    bool accent_insensitive;     // Accent-insensitive comparison
    std::string language;        // Language code (e.g., "en", "fr", "")
    std::string description;     // Human-readable description
};

// Parser for character set and collation files
class CharsetParser
{
public:
    CharsetParser() = default;
    ~CharsetParser() = default;

    // Parse JSON character set file (resources/charsets/charsets.json)
    Status parseJSONFile(const std::string &filepath,
                        std::vector<CharacterSet> &charsets,
                        ErrorContext *ctx);

    // Parse collations JSON file (resources/collations/collations.json)
    Status parseCollationsFile(const std::string &filepath,
                              std::vector<Collation> &collations,
                              ErrorContext *ctx);

    // Generate built-in character sets (UTF-8, ASCII, etc.)
    // These don't require external files
    Status generateBuiltinCharsets(std::vector<CharacterSet> &charsets,
                                   ErrorContext *ctx);

private:
    // Parse a single charset from JSON object
    Status parseCharsetJSON(const void *json_obj, CharacterSet &charset, ErrorContext *ctx);

    // Parse a single collation from JSON object
    Status parseCollationJSON(const void *json_obj, Collation &collation, ErrorContext *ctx);

    // Generate ASCII character set
    Status generateASCII(CharacterSet &charset);

    // Generate ISO-8859-1 (Latin-1) character set
    Status generateLatin1(CharacterSet &charset);

    // Generate UTF-8 character set
    Status generateUTF8(CharacterSet &charset);

    // Generate UTF-8MB4 character set
    Status generateUTF8MB4(CharacterSet &charset);

    // Generate UTF-16 character set
    Status generateUTF16(CharacterSet &charset);

    // Generate UTF-32 character set
    Status generateUTF32(CharacterSet &charset);
};

} // namespace scratchbird::core
