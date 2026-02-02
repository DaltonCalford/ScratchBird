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
#include <filesystem>
#include <fstream>

#include "scratchbird/core/charset_parser.h"
#include "scratchbird/core/charset_loader.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"

using namespace scratchbird::core;

/**
 * Test character set parser and loader functionality
 * Agent B: Character Set Loader Tests
 */

class CharsetLoaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a temporary database for testing
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_charset_loader", ".sbrd");
        std::filesystem::remove(test_db_path_);

        auto status = Database::create(test_db_path_, 16384, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(test_db_path_, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        catalog_ = db_.getCatalog();
        ASSERT_NE(catalog_, nullptr);

        // Create temporary JSON test files
        createTestJSONFiles();
    }

    void TearDown() override
    {
        db_.close();
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_charsets_json_);
        std::filesystem::remove(test_collations_json_);
    }

    void createTestJSONFiles()
    {
        // Create test charset JSON file
        test_charsets_json_ = "test_charsets.json";
        std::ofstream charset_file(test_charsets_json_);
        charset_file << R"({
  "character_sets": [
    {
      "name": "TEST-UTF-8",
      "description": "Test UTF-8 charset",
      "aliases": ["test-utf8", "TEST8"],
      "max_bytes": 4,
      "min_bytes": 1,
      "is_variable_width": true,
      "encoding_type": "unicode",
      "iana_name": "UTF-8"
    },
    {
      "name": "TEST-ASCII",
      "description": "Test ASCII charset",
      "aliases": ["test-ascii"],
      "max_bytes": 1,
      "min_bytes": 1,
      "is_variable_width": false,
      "encoding_type": "single_byte",
      "iana_name": "US-ASCII"
    }
  ]
})";
        charset_file.close();

        // Create test collation JSON file
        test_collations_json_ = "test_collations.json";
        std::ofstream collation_file(test_collations_json_);
        collation_file << R"({
  "collations": [
    {
      "name": "test_utf8_ci",
      "charset": "TEST-UTF-8",
      "case_insensitive": true,
      "accent_insensitive": false,
      "language": "en",
      "description": "Test UTF-8 case-insensitive"
    },
    {
      "name": "test_ascii_bin",
      "charset": "TEST-ASCII",
      "case_insensitive": false,
      "accent_insensitive": false,
      "language": "",
      "description": "Test ASCII binary"
    }
  ]
})";
        collation_file.close();
    }

    std::string test_db_path_;
    std::string test_charsets_json_;
    std::string test_collations_json_;
    Database db_;
    CatalogManager *catalog_ = nullptr;
};

// Test 1: Parse built-in character sets
TEST_F(CharsetLoaderTest, GenerateBuiltinCharsets)
{
    CharsetParser parser;
    std::vector<CharacterSet> charsets;
    ErrorContext ctx;

    Status status = parser.generateBuiltinCharsets(charsets, &ctx);
    EXPECT_EQ(Status::OK, status) << "Failed to generate built-in charsets: " << ctx.message;

    // Should generate 5 built-in charsets
    EXPECT_EQ(5, charsets.size());

    // Verify UTF-8
    auto utf8_it = std::find_if(charsets.begin(), charsets.end(),
                                [](const CharacterSet &cs) { return cs.name == "UTF-8"; });
    ASSERT_NE(utf8_it, charsets.end()) << "UTF-8 charset not found";
    EXPECT_EQ(4, utf8_it->max_bytes);
    EXPECT_EQ(1, utf8_it->min_bytes);
    EXPECT_TRUE(utf8_it->is_variable_width);
    EXPECT_EQ("utf8,UTF8", utf8_it->aliases);

    // Verify ASCII
    auto ascii_it = std::find_if(charsets.begin(), charsets.end(),
                                 [](const CharacterSet &cs) { return cs.name == "ASCII"; });
    ASSERT_NE(ascii_it, charsets.end()) << "ASCII charset not found";
    EXPECT_EQ(1, ascii_it->max_bytes);
    EXPECT_EQ(1, ascii_it->min_bytes);
    EXPECT_FALSE(ascii_it->is_variable_width);
    EXPECT_EQ(128, ascii_it->mappings.size()); // ASCII has 128 explicit mappings

    // Verify ISO-8859-1
    auto latin1_it = std::find_if(charsets.begin(), charsets.end(),
                                  [](const CharacterSet &cs) { return cs.name == "ISO-8859-1"; });
    ASSERT_NE(latin1_it, charsets.end()) << "ISO-8859-1 charset not found";
    EXPECT_EQ(1, latin1_it->max_bytes);
    EXPECT_EQ(1, latin1_it->min_bytes);
    EXPECT_FALSE(latin1_it->is_variable_width);
    EXPECT_EQ(256, latin1_it->mappings.size()); // Latin-1 has 256 explicit mappings

    // Verify UTF-16
    auto utf16_it = std::find_if(charsets.begin(), charsets.end(),
                                 [](const CharacterSet &cs) { return cs.name == "UTF-16"; });
    ASSERT_NE(utf16_it, charsets.end()) << "UTF-16 charset not found";
    EXPECT_EQ(4, utf16_it->max_bytes);
    EXPECT_EQ(2, utf16_it->min_bytes);
    EXPECT_TRUE(utf16_it->is_variable_width);

    // Verify UTF-32
    auto utf32_it = std::find_if(charsets.begin(), charsets.end(),
                                 [](const CharacterSet &cs) { return cs.name == "UTF-32"; });
    ASSERT_NE(utf32_it, charsets.end()) << "UTF-32 charset not found";
    EXPECT_EQ(4, utf32_it->max_bytes);
    EXPECT_EQ(4, utf32_it->min_bytes);
    EXPECT_FALSE(utf32_it->is_variable_width);
}

// Test 2: Parse JSON charset file
TEST_F(CharsetLoaderTest, ParseJSONFile)
{
    CharsetParser parser;
    std::vector<CharacterSet> charsets;
    ErrorContext ctx;

    Status status = parser.parseJSONFile(test_charsets_json_, charsets, &ctx);
    EXPECT_EQ(Status::OK, status) << "Failed to parse JSON file: " << ctx.message;

    EXPECT_EQ(2, charsets.size());

    // Verify first charset
    EXPECT_EQ("TEST-UTF-8", charsets[0].name);
    EXPECT_EQ("Test UTF-8 charset", charsets[0].description);
    EXPECT_EQ(4, charsets[0].max_bytes);
    EXPECT_EQ(1, charsets[0].min_bytes);
    EXPECT_TRUE(charsets[0].is_variable_width);
    EXPECT_EQ("test-utf8,TEST8", charsets[0].aliases);
    EXPECT_EQ("unicode", charsets[0].encoding_type);
    EXPECT_EQ("UTF-8", charsets[0].iana_name);

    // Verify second charset
    EXPECT_EQ("TEST-ASCII", charsets[1].name);
    EXPECT_EQ(1, charsets[1].max_bytes);
    EXPECT_FALSE(charsets[1].is_variable_width);
}

// Test 3: Parse collations JSON file
TEST_F(CharsetLoaderTest, ParseCollationsFile)
{
    CharsetParser parser;
    std::vector<Collation> collations;
    ErrorContext ctx;

    Status status = parser.parseCollationsFile(test_collations_json_, collations, &ctx);
    EXPECT_EQ(Status::OK, status) << "Failed to parse collations file: " << ctx.message;

    EXPECT_EQ(2, collations.size());

    // Verify first collation
    EXPECT_EQ("test_utf8_ci", collations[0].name);
    EXPECT_EQ("TEST-UTF-8", collations[0].charset_name);
    EXPECT_TRUE(collations[0].case_insensitive);
    EXPECT_FALSE(collations[0].accent_insensitive);
    EXPECT_EQ("en", collations[0].language);
    EXPECT_EQ("Test UTF-8 case-insensitive", collations[0].description);

    // Verify second collation
    EXPECT_EQ("test_ascii_bin", collations[1].name);
    EXPECT_EQ("TEST-ASCII", collations[1].charset_name);
    EXPECT_FALSE(collations[1].case_insensitive);
    EXPECT_FALSE(collations[1].accent_insensitive);
}

// Test 4: Parse non-existent file
TEST_F(CharsetLoaderTest, ParseNonExistentFile)
{
    CharsetParser parser;
    std::vector<CharacterSet> charsets;
    ErrorContext ctx;

    Status status = parser.parseJSONFile("nonexistent.json", charsets, &ctx);
    EXPECT_EQ(Status::FILE_NOT_FOUND, status);
    EXPECT_NE("", ctx.message);
}

// Test 5: Parse invalid JSON
TEST_F(CharsetLoaderTest, ParseInvalidJSON)
{
    // Create invalid JSON file
    std::string invalid_json = "test_invalid.json";
    std::ofstream file(invalid_json);
    file << "{ invalid json }";
    file.close();

    CharsetParser parser;
    std::vector<CharacterSet> charsets;
    ErrorContext ctx;

    Status status = parser.parseJSONFile(invalid_json, charsets, &ctx);
    EXPECT_EQ(Status::INVALID_ARGUMENT, status);

    std::filesystem::remove(invalid_json);
}

// Test 6: Load built-in charsets into database
TEST_F(CharsetLoaderTest, LoadBuiltinCharsets)
{
    CharsetLoader loader(catalog_, &db_);
    ErrorContext ctx;

    Status status = loader.loadBuiltinCharsets(&ctx);
    EXPECT_EQ(Status::OK, status) << "Failed to load built-in charsets: " << ctx.message;

    // TODO: Verify charsets were loaded into catalog
    // This requires catalog manager to implement charset query methods
}

// Test 7: Load charsets from JSON file
TEST_F(CharsetLoaderTest, LoadFromJSONFile)
{
    CharsetLoader loader(catalog_, &db_);
    ErrorContext ctx;

    Status status = loader.loadFromJSONFile(test_charsets_json_, &ctx);
    EXPECT_EQ(Status::OK, status) << "Failed to load charsets from JSON: " << ctx.message;

    // TODO: Verify charsets were loaded into catalog
}

// Test 8: Load collations from JSON file
TEST_F(CharsetLoaderTest, LoadCollationsFromJSONFile)
{
    CharsetLoader loader(catalog_, &db_);
    ErrorContext ctx;

    // First load the charsets so collations can reference them
    Status status = loader.loadFromJSONFile(test_charsets_json_, &ctx);
    EXPECT_EQ(Status::OK, status);

    // Now load collations
    status = loader.loadCollationsFromJSONFile(test_collations_json_, &ctx);
    EXPECT_EQ(Status::OK, status) << "Failed to load collations from JSON: " << ctx.message;

    // TODO: Verify collations were loaded into catalog
}

// Test 9: Character mapping validation (ASCII)
TEST_F(CharsetLoaderTest, ASCIICharacterMappings)
{
    CharsetParser parser;
    CharacterSet ascii;

    Status status = parser.generateASCII(ascii);
    EXPECT_EQ(Status::OK, status);

    EXPECT_EQ(128, ascii.mappings.size());

    // Verify some specific mappings
    EXPECT_EQ(0x00, ascii.mappings[0].byte_sequence);
    EXPECT_EQ(0x00, ascii.mappings[0].unicode_codepoint);
    EXPECT_EQ(1, ascii.mappings[0].byte_length);

    EXPECT_EQ(0x41, ascii.mappings[0x41].byte_sequence); // 'A'
    EXPECT_EQ(0x41, ascii.mappings[0x41].unicode_codepoint);

    EXPECT_EQ(0x7F, ascii.mappings[127].byte_sequence); // DEL
    EXPECT_EQ(0x7F, ascii.mappings[127].unicode_codepoint);
}

// Test 10: Character mapping validation (ISO-8859-1)
TEST_F(CharsetLoaderTest, Latin1CharacterMappings)
{
    CharsetParser parser;
    CharacterSet latin1;

    Status status = parser.generateLatin1(latin1);
    EXPECT_EQ(Status::OK, status);

    EXPECT_EQ(256, latin1.mappings.size());

    // Verify some specific mappings
    EXPECT_EQ(0x00, latin1.mappings[0].byte_sequence);
    EXPECT_EQ(0x00, latin1.mappings[0].unicode_codepoint);

    EXPECT_EQ(0xA0, latin1.mappings[0xA0].byte_sequence); // Non-breaking space
    EXPECT_EQ(0xA0, latin1.mappings[0xA0].unicode_codepoint);

    EXPECT_EQ(0xFF, latin1.mappings[255].byte_sequence); // ÿ
    EXPECT_EQ(0xFF, latin1.mappings[255].unicode_codepoint);
}

// Test 11: Load from actual project resources (if they exist)
TEST_F(CharsetLoaderTest, LoadFromProjectResources)
{
    CharsetParser parser;
    std::vector<CharacterSet> charsets;
    ErrorContext ctx;

    std::string resources_charsets = "resources/charsets/charsets.json";
    if (std::filesystem::exists(resources_charsets))
    {
        Status status = parser.parseJSONFile(resources_charsets, charsets, &ctx);
        EXPECT_EQ(Status::OK, status) << "Failed to parse project charsets: " << ctx.message;
        EXPECT_GT(charsets.size(), 0) << "No charsets loaded from project resources";

        // Verify we can parse UTF-8
        auto utf8_it = std::find_if(charsets.begin(), charsets.end(),
                                    [](const CharacterSet &cs) { return cs.name == "UTF-8"; });
        EXPECT_NE(utf8_it, charsets.end()) << "UTF-8 not found in project resources";
    }
    else
    {
        GTEST_SKIP() << "Project resources not found, skipping test";
    }
}

// Test 12: Load collations from actual project resources (if they exist)
TEST_F(CharsetLoaderTest, LoadCollationsFromProjectResources)
{
    CharsetParser parser;
    std::vector<Collation> collations;
    ErrorContext ctx;

    std::string resources_collations = "resources/collations/collations.json";
    if (std::filesystem::exists(resources_collations))
    {
        Status status = parser.parseCollationsFile(resources_collations, collations, &ctx);
        EXPECT_EQ(Status::OK, status) << "Failed to parse project collations: " << ctx.message;
        EXPECT_GT(collations.size(), 0) << "No collations loaded from project resources";

        // Verify we have some common collations
        auto utf8_ci = std::find_if(collations.begin(), collations.end(),
                                    [](const Collation &col) { return col.name == "utf8_general_ci"; });
        EXPECT_NE(utf8_ci, collations.end()) << "utf8_general_ci not found in project resources";
    }
    else
    {
        GTEST_SKIP() << "Project collation resources not found, skipping test";
    }
}
