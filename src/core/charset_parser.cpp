/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/charset_parser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace scratchbird::core
{

Status CharsetParser::parseJSONFile(const std::string &filepath,
                                    std::vector<CharacterSet> &charsets,
                                    ErrorContext *ctx)
{
    // Open the JSON file
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND,
                         ("Cannot open character set file: " + filepath).c_str());
        return Status::FILE_NOT_FOUND;
    }

    // Parse JSON
    json j;
    try
    {
        file >> j;
    }
    catch (const json::parse_error &e)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("JSON parse error: " + std::string(e.what())).c_str());
        return Status::INVALID_ARGUMENT;
    }

    // Validate structure
    if (!j.contains("character_sets") || !j["character_sets"].is_array())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "JSON file missing 'character_sets' array");
        return Status::INVALID_ARGUMENT;
    }

    // Parse each character set
    for (const auto &charset_json : j["character_sets"])
    {
        CharacterSet charset;
        Status status = parseCharsetJSON(&charset_json, charset, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        charsets.push_back(charset);
    }

    return Status::OK;
}

Status CharsetParser::parseCollationsFile(const std::string &filepath,
                                          std::vector<Collation> &collations,
                                          ErrorContext *ctx)
{
    // Open the JSON file
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND,
                         ("Cannot open collations file: " + filepath).c_str());
        return Status::FILE_NOT_FOUND;
    }

    // Parse JSON
    json j;
    try
    {
        file >> j;
    }
    catch (const json::parse_error &e)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("JSON parse error: " + std::string(e.what())).c_str());
        return Status::INVALID_ARGUMENT;
    }

    // Validate structure
    if (!j.contains("collations") || !j["collations"].is_array())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "JSON file missing 'collations' array");
        return Status::INVALID_ARGUMENT;
    }

    // Parse each collation
    for (const auto &collation_json : j["collations"])
    {
        Collation collation;
        Status status = parseCollationJSON(&collation_json, collation, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        collations.push_back(collation);
    }

    return Status::OK;
}

Status CharsetParser::parseCharsetJSON(const void *json_obj, CharacterSet &charset, ErrorContext *ctx)
{
    const json *j = static_cast<const json *>(json_obj);

    try
    {
        // Required fields
        if (!j->contains("name") || !(*j)["name"].is_string())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Character set missing 'name' field");
            return Status::INVALID_ARGUMENT;
        }
        charset.name = (*j)["name"].get<std::string>();

        if (!j->contains("max_bytes") || !(*j)["max_bytes"].is_number())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Character set '" + charset.name + "' missing 'max_bytes' field").c_str());
            return Status::INVALID_ARGUMENT;
        }
        charset.max_bytes = (*j)["max_bytes"].get<uint8_t>();

        if (!j->contains("min_bytes") || !(*j)["min_bytes"].is_number())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Character set '" + charset.name + "' missing 'min_bytes' field").c_str());
            return Status::INVALID_ARGUMENT;
        }
        charset.min_bytes = (*j)["min_bytes"].get<uint8_t>();

        if (!j->contains("is_variable_width") || !(*j)["is_variable_width"].is_boolean())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Character set '" + charset.name + "' missing 'is_variable_width' field").c_str());
            return Status::INVALID_ARGUMENT;
        }
        charset.is_variable_width = (*j)["is_variable_width"].get<bool>();

        // Optional fields with defaults
        charset.description = j->contains("description") ? (*j)["description"].get<std::string>() : "";
        charset.encoding_type = j->contains("encoding_type") ? (*j)["encoding_type"].get<std::string>() : "";
        charset.iana_name = j->contains("iana_name") ? (*j)["iana_name"].get<std::string>() : charset.name;

        // Parse aliases array into comma-separated string
        if (j->contains("aliases") && (*j)["aliases"].is_array())
        {
            std::ostringstream aliases_stream;
            bool first = true;
            for (const auto &alias : (*j)["aliases"])
            {
                if (!first)
                    aliases_stream << ",";
                aliases_stream << alias.get<std::string>();
                first = false;
            }
            charset.aliases = aliases_stream.str();
        }
        else
        {
            charset.aliases = "";
        }

        // For now, we don't parse explicit mappings from JSON
        // Most character sets use algorithmic encoding (UTF-8, ASCII, ISO-8859-1, etc.)
        charset.mappings.clear();

        return Status::OK;
    }
    catch (const json::exception &e)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("JSON parsing error: " + std::string(e.what())).c_str());
        return Status::INVALID_ARGUMENT;
    }
}

Status CharsetParser::parseCollationJSON(const void *json_obj, Collation &collation, ErrorContext *ctx)
{
    const json *j = static_cast<const json *>(json_obj);

    try
    {
        // Required fields
        if (!j->contains("name") || !(*j)["name"].is_string())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Collation missing 'name' field");
            return Status::INVALID_ARGUMENT;
        }
        collation.name = (*j)["name"].get<std::string>();

        if (!j->contains("charset") || !(*j)["charset"].is_string())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Collation '" + collation.name + "' missing 'charset' field").c_str());
            return Status::INVALID_ARGUMENT;
        }
        collation.charset_name = (*j)["charset"].get<std::string>();

        if (!j->contains("case_insensitive") || !(*j)["case_insensitive"].is_boolean())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Collation '" + collation.name + "' missing 'case_insensitive' field").c_str());
            return Status::INVALID_ARGUMENT;
        }
        collation.case_insensitive = (*j)["case_insensitive"].get<bool>();

        if (!j->contains("accent_insensitive") || !(*j)["accent_insensitive"].is_boolean())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Collation '" + collation.name + "' missing 'accent_insensitive' field").c_str());
            return Status::INVALID_ARGUMENT;
        }
        collation.accent_insensitive = (*j)["accent_insensitive"].get<bool>();

        // Optional fields
        collation.language = j->contains("language") ? (*j)["language"].get<std::string>() : "";
        collation.description = j->contains("description") ? (*j)["description"].get<std::string>() : "";

        return Status::OK;
    }
    catch (const json::exception &e)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("JSON parsing error: " + std::string(e.what())).c_str());
        return Status::INVALID_ARGUMENT;
    }
}

Status CharsetParser::generateBuiltinCharsets(std::vector<CharacterSet> &charsets,
                                              ErrorContext *ctx)
{
    Status status;

    // UTF-8
    CharacterSet utf8;
    status = generateUTF8(utf8);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to generate UTF-8 charset");
        return status;
    }
    charsets.push_back(utf8);

    // UTF-8MB4 (MySQL compatible)
    CharacterSet utf8mb4;
    status = generateUTF8MB4(utf8mb4);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to generate UTF-8MB4 charset");
        return status;
    }
    charsets.push_back(utf8mb4);

    // ASCII
    CharacterSet ascii;
    status = generateASCII(ascii);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to generate ASCII charset");
        return status;
    }
    charsets.push_back(ascii);

    // ISO-8859-1 (Latin-1)
    CharacterSet latin1;
    status = generateLatin1(latin1);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to generate Latin-1 charset");
        return status;
    }
    charsets.push_back(latin1);

    // UTF-16
    CharacterSet utf16;
    status = generateUTF16(utf16);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to generate UTF-16 charset");
        return status;
    }
    charsets.push_back(utf16);

    // UTF-32
    CharacterSet utf32;
    status = generateUTF32(utf32);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to generate UTF-32 charset");
        return status;
    }
    charsets.push_back(utf32);

    return Status::OK;
}

Status CharsetParser::generateUTF8(CharacterSet &charset)
{
    charset.name = "UTF-8";
    charset.description = "Unicode Transformation Format, 8-bit";
    charset.max_bytes = 4;
    charset.min_bytes = 1;
    charset.is_variable_width = true;
    charset.aliases = "utf8,UTF8";
    charset.encoding_type = "unicode";
    charset.iana_name = "UTF-8";
    // UTF-8 encoding is algorithmic, no explicit mappings needed
    charset.mappings.clear();
    return Status::OK;
}

Status CharsetParser::generateUTF8MB4(CharacterSet &charset)
{
    charset.name = "UTF8MB4";
    charset.description = "UTF-8 Unicode (MySQL utf8mb4 compatible)";
    charset.max_bytes = 4;
    charset.min_bytes = 1;
    charset.is_variable_width = true;
    charset.aliases.clear();
    charset.encoding_type = "unicode";
    charset.iana_name = "UTF-8";
    charset.mappings.clear();
    return Status::OK;
}

Status CharsetParser::generateASCII(CharacterSet &charset)
{
    charset.name = "ASCII";
    charset.description = "American Standard Code for Information Interchange";
    charset.max_bytes = 1;
    charset.min_bytes = 1;
    charset.is_variable_width = false;
    charset.aliases = "US-ASCII,ANSI_X3.4-1968";
    charset.encoding_type = "single_byte";
    charset.iana_name = "US-ASCII";

    // ASCII is 0x00-0x7F direct mapping
    charset.mappings.reserve(128);
    for (uint32_t i = 0; i < 128; i++)
    {
        CharacterMapping mapping;
        mapping.byte_sequence = i;
        mapping.unicode_codepoint = i;
        mapping.byte_length = 1;
        charset.mappings.push_back(mapping);
    }

    return Status::OK;
}

Status CharsetParser::generateLatin1(CharacterSet &charset)
{
    charset.name = "ISO-8859-1";
    charset.description = "Latin-1 Western European";
    charset.max_bytes = 1;
    charset.min_bytes = 1;
    charset.is_variable_width = false;
    charset.aliases = "latin1,ISO_8859-1,CP819";
    charset.encoding_type = "single_byte";
    charset.iana_name = "ISO-8859-1";

    // ISO-8859-1 is direct mapping to Unicode (0x00-0xFF)
    charset.mappings.reserve(256);
    for (uint32_t i = 0; i < 256; i++)
    {
        CharacterMapping mapping;
        mapping.byte_sequence = i;
        mapping.unicode_codepoint = i;
        mapping.byte_length = 1;
        charset.mappings.push_back(mapping);
    }

    return Status::OK;
}

Status CharsetParser::generateUTF16(CharacterSet &charset)
{
    charset.name = "UTF-16";
    charset.description = "Unicode Transformation Format, 16-bit";
    charset.max_bytes = 4;
    charset.min_bytes = 2;
    charset.is_variable_width = true;
    charset.aliases = "utf16,UTF16";
    charset.encoding_type = "unicode";
    charset.iana_name = "UTF-16";
    // UTF-16 encoding is algorithmic, no explicit mappings needed
    charset.mappings.clear();
    return Status::OK;
}

Status CharsetParser::generateUTF32(CharacterSet &charset)
{
    charset.name = "UTF-32";
    charset.description = "Unicode Transformation Format, 32-bit";
    charset.max_bytes = 4;
    charset.min_bytes = 4;
    charset.is_variable_width = false;
    charset.aliases = "utf32,UTF32";
    charset.encoding_type = "unicode";
    charset.iana_name = "UTF-32";
    // UTF-32 encoding is algorithmic, no explicit mappings needed
    charset.mappings.clear();
    return Status::OK;
}

} // namespace scratchbird::core
