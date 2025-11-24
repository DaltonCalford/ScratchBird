#pragma once

#include <string>
#include <vector>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/charset_parser.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{

// Forward declarations
class CatalogManager;
class Database;

// Character set loader - loads character set and collation data into catalog
class CharsetLoader
{
public:
    CharsetLoader(CatalogManager *catalog, Database *db)
        : catalog_(catalog), db_(db) {}

    ~CharsetLoader() = default;

    // Load single character set into catalog (pg_charsets)
    Status loadCharset(const CharacterSet &charset, ErrorContext *ctx);

    // Load collation into catalog (pg_collations)
    Status loadCollation(const Collation &collation, ErrorContext *ctx);

    // Load built-in character sets (UTF-8, ASCII, ISO-8859-1, UTF-16, UTF-32)
    Status loadBuiltinCharsets(ErrorContext *ctx);

    // Load all character sets from JSON file
    Status loadFromJSONFile(const std::string &json_filepath, ErrorContext *ctx);

    // Load all collations from JSON file
    Status loadCollationsFromJSONFile(const std::string &json_filepath, ErrorContext *ctx);

    // Load from directory (charsets.json and collations.json)
    Status loadFromDirectory(const std::string &charset_dir, ErrorContext *ctx);

    // Check if a character set exists in catalog
    bool charsetExists(const std::string &charset_name, ErrorContext *ctx);

    // Check if a collation exists in catalog
    bool collationExists(const std::string &collation_name, ErrorContext *ctx);

    // Get character set ID by name
    Status getCharsetID(const std::string &charset_name, UuidV7Bytes &charset_id, ErrorContext *ctx);

private:
    CatalogManager *catalog_;
    Database *db_;

    // Generate UUID for a charset based on its name
    UuidV7Bytes generateCharsetID(const std::string &charset_name);

    // Generate UUID for a collation based on its name
    UuidV7Bytes generateCollationID(const std::string &collation_name);

    // Get current timestamp in microseconds
    uint64_t getCurrentTimestamp();
};

} // namespace scratchbird::core
