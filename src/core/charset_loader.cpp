#include "scratchbird/core/charset_loader.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>

namespace scratchbird::core
{

namespace {

std::string normalizeName(std::string value)
{
    std::string out;
    out.reserve(value.size());
    for (char c : value)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return out;
}

bool resolveBuiltinCharsetId(const std::string& name, uint16_t& id_out)
{
    std::string normalized = normalizeName(name);
    if (normalized == "ascii")
    {
        id_out = 0; // CharacterSet::ASCII
        return true;
    }
    if (normalized == "latin1" || normalized == "iso88591")
    {
        id_out = 1; // CharacterSet::LATIN1
        return true;
    }
    if (normalized == "utf8" || normalized == "utf8mb4")
    {
        id_out = 2; // CharacterSet::UTF8
        return true;
    }
    if (normalized == "utf16")
    {
        id_out = 3; // CharacterSet::UTF16
        return true;
    }
    if (normalized == "utf32")
    {
        id_out = 4; // CharacterSet::UTF32
        return true;
    }
    return false;
}

uint32_t resolveBuiltinCollationId(const std::string& name)
{
    std::string normalized = normalizeName(name);
    if (normalized == "asciibin") return 1;
    if (normalized == "asciigeneralci") return 2;
    if (normalized == "latin1bin") return 10;
    if (normalized == "latin1generalci") return 11;
    if (normalized == "latin1generalcs") return 12;
    if (normalized == "utf8bin") return 100;
    if (normalized == "utf8generalci") return 101;
    if (normalized == "utf8unicodeci") return 102;
    if (normalized == "utf8unicodecs") return 103;
    if (normalized == "utf8enusci") return 110;
    if (normalized == "utf8dedeci") return 111;
    if (normalized == "utf16bin") return 200;
    if (normalized == "utf16generalci") return 201;
    if (normalized == "utf32bin") return 300;
    if (normalized == "utf32generalci") return 301;
    return 0;
}

uint32_t resolveDefaultCollationId(const std::string& charset_name)
{
    std::string normalized = normalizeName(charset_name);
    if (normalized == "ascii")
    {
        return 1;
    }
    if (normalized == "latin1" || normalized == "iso88591")
    {
        return 11;
    }
    if (normalized == "utf8" || normalized == "utf8mb4")
    {
        return 101;
    }
    if (normalized == "utf16")
    {
        return 201;
    }
    if (normalized == "utf32")
    {
        return 301;
    }
    return 0;
}

} // namespace

Status CharsetLoader::loadCharset(const CharacterSet &charset, ErrorContext *ctx)
{
    if (charset.name.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Character set name cannot be empty");
        return Status::INVALID_ARGUMENT;
    }

    if (charset.max_bytes == 0 || charset.max_bytes > 4)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("Invalid max_bytes for charset " + charset.name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    if (charset.min_bytes == 0 || charset.min_bytes > charset.max_bytes)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("Invalid min_bytes for charset " + charset.name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    if (!catalog_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
        return Status::INVALID_ARGUMENT;
    }

    // Check if charset already exists (case-insensitive)
    if (charsetExists(charset.name, ctx))
    {
        // Character set already exists - skip
        return Status::OK;
    }

    uint16_t charset_id = 0;
    if (!resolveBuiltinCharsetId(charset.name, charset_id))
    {
        std::vector<CatalogManager::CharsetInfo> existing;
        Status status = catalog_->listCharsets(existing, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        uint16_t max_id = 0;
        for (const auto& info : existing)
        {
            max_id = std::max(max_id, info.charset_id);
        }
        charset_id = static_cast<uint16_t>(max_id + 1);
        if (charset_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Charset ID space exhausted");
            return Status::OUT_OF_RANGE;
        }
    }

    CatalogManager::CharsetInfo info;
    info.charset_id = charset_id;
    info.name = charset.name;
    info.description = charset.description;
    info.min_bytes = charset.min_bytes;
    info.max_bytes = charset.max_bytes;
    info.variable_width = charset.is_variable_width ? 1 : 0;
    info.default_collation_id = resolveDefaultCollationId(charset.name);
    info.created_time = getCurrentTimestamp();
    info.last_modified_time = info.created_time;

    return catalog_->createCharset(info, ctx);
}

Status CharsetLoader::loadCollation(const Collation &collation, ErrorContext *ctx)
{
    if (collation.name.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Collation name cannot be empty");
        return Status::INVALID_ARGUMENT;
    }

    if (collation.charset_name.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("Collation " + collation.name + " missing charset reference").c_str());
        return Status::INVALID_ARGUMENT;
    }

    if (!catalog_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
        return Status::INVALID_ARGUMENT;
    }

    // Check if charset exists (case-insensitive)
    if (!charsetExists(collation.charset_name, ctx))
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("Charset " + collation.charset_name + " not found for collation " + collation.name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    // Check if collation already exists
    if (collationExists(collation.name, ctx))
    {
        // Collation already exists - skip
        return Status::OK;
    }

    // Get charset ID
    uint16_t charset_id = 0;
    Status status = getCharsetID(collation.charset_name, charset_id, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint32_t collation_id = resolveBuiltinCollationId(collation.name);
    if (collation_id == 0)
    {
        std::vector<CatalogManager::CollationCatalogInfo> existing;
        status = catalog_->listCollations(existing, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        uint32_t max_id = 0;
        for (const auto& info : existing)
        {
            max_id = std::max(max_id, info.collation_id);
        }
        collation_id = max_id + 1;
        if (collation_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Collation ID space exhausted");
            return Status::OUT_OF_RANGE;
        }
    }

    auto lower_name = normalizeName(collation.name);
    uint8_t collation_type = 1; // CollationType::CASE_SENSITIVE
    if (lower_name.find("bin") != std::string::npos)
    {
        collation_type = 0; // CollationType::BINARY
    }
    else if (collation.case_insensitive && collation.accent_insensitive)
    {
        collation_type = 4; // CollationType::CI_AI
    }
    else if (collation.case_insensitive)
    {
        collation_type = 2; // CollationType::CASE_INSENSITIVE
    }
    else if (collation.accent_insensitive)
    {
        collation_type = 3; // CollationType::ACCENT_INSENSITIVE
    }

    uint8_t strength = 3; // CollationStrength::TERTIARY
    if (collation_type == 0)
    {
        strength = 5; // CollationStrength::IDENTICAL
    }

    CatalogManager::CollationCatalogInfo info;
    info.collation_id = collation_id;
    info.name = collation.name;
    info.charset_id = charset_id;
    info.collation_type = collation_type;
    info.strength = strength;
    info.pad_space = 1;
    info.is_default = (collation_id == resolveDefaultCollationId(collation.charset_name)) ? 1 : 0;
    std::memset(info.locale, 0, sizeof(info.locale));
    if (!collation.language.empty())
    {
        std::strncpy(info.locale, collation.language.c_str(), sizeof(info.locale) - 1);
    }
    info.created_time = getCurrentTimestamp();
    info.last_modified_time = info.created_time;

    return catalog_->createCollation(info, ctx);
}

Status CharsetLoader::loadBuiltinCharsets(ErrorContext *ctx)
{
    CharsetParser parser;
    std::vector<CharacterSet> charsets;

    Status status = parser.generateBuiltinCharsets(charsets, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Load each built-in charset
    for (const auto &charset : charsets)
    {
        status = loadCharset(charset, ctx);
        if (status != Status::OK)
        {
            // Continue on error - best effort
            // LOG_WARNING("Failed to load charset: " + charset.name);
        }
    }

    // Load default collations for built-in charsets
    std::vector<Collation> default_collations = {
        {"utf8_general_ci", "UTF-8", true, false, "", "UTF-8 general case-insensitive"},
        {"utf8_bin", "UTF-8", false, false, "", "UTF-8 binary (case-sensitive)"},
        {"ascii_general_ci", "ASCII", true, false, "", "ASCII general case-insensitive"},
        {"ascii_bin", "ASCII", false, false, "", "ASCII binary (case-sensitive)"},
        {"latin1_general_ci", "ISO-8859-1", true, false, "", "Latin-1 general case-insensitive"},
        {"latin1_bin", "ISO-8859-1", false, false, "", "Latin-1 binary (case-sensitive)"},
    };

    for (const auto &collation : default_collations)
    {
        status = loadCollation(collation, ctx);
        if (status != Status::OK)
        {
            // Continue on error - best effort
            // LOG_WARNING("Failed to load collation: " + collation.name);
        }
    }

    return Status::OK;
}

Status CharsetLoader::loadFromJSONFile(const std::string &json_filepath, ErrorContext *ctx)
{
    CharsetParser parser;
    std::vector<CharacterSet> charsets;

    Status status = parser.parseJSONFile(json_filepath, charsets, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Load each charset
    for (const auto &charset : charsets)
    {
        status = loadCharset(charset, ctx);
        if (status != Status::OK)
        {
            // Continue on error - best effort
            // LOG_WARNING("Failed to load charset: " + charset.name);
        }
    }

    return Status::OK;
}

Status CharsetLoader::loadCollationsFromJSONFile(const std::string &json_filepath, ErrorContext *ctx)
{
    CharsetParser parser;
    std::vector<Collation> collations;

    Status status = parser.parseCollationsFile(json_filepath, collations, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Load each collation
    for (const auto &collation : collations)
    {
        status = loadCollation(collation, ctx);
        if (status != Status::OK)
        {
            // Continue on error - best effort
            // LOG_WARNING("Failed to load collation: " + collation.name);
        }
    }

    return Status::OK;
}

Status CharsetLoader::loadFromDirectory(const std::string &charset_dir, ErrorContext *ctx)
{
    // Load built-in charsets first
    Status status = loadBuiltinCharsets(ctx);
    if (status != Status::OK)
    {
        // Continue anyway - best effort
    }

    // Load from charsets.json
    std::string charsets_file = charset_dir + "/charsets.json";
    status = loadFromJSONFile(charsets_file, ctx);
    if (status != Status::OK)
    {
        // Continue anyway - charsets.json might not exist
    }

    // Load from collations.json
    std::string collations_file = charset_dir + "/collations.json";
    status = loadCollationsFromJSONFile(collations_file, ctx);
    if (status != Status::OK)
    {
        // Continue anyway - collations.json might not exist
    }

    return Status::OK;
}

bool CharsetLoader::charsetExists(const std::string &charset_name, ErrorContext *ctx)
{
    if (!catalog_)
    {
        return false;
    }

    std::vector<CatalogManager::CharsetInfo> existing;
    Status status = catalog_->listCharsets(existing, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return false;
    }

    std::string needle = normalizeName(charset_name);
    for (const auto& info : existing)
    {
        if (normalizeName(info.name) == needle)
        {
            return true;
        }
    }

    return false;
}

bool CharsetLoader::collationExists(const std::string &collation_name, ErrorContext *ctx)
{
    if (!catalog_)
    {
        return false;
    }

    std::vector<CatalogManager::CollationCatalogInfo> existing;
    Status status = catalog_->listCollations(existing, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return false;
    }

    std::string needle = normalizeName(collation_name);
    for (const auto& info : existing)
    {
        if (normalizeName(info.name) == needle)
        {
            return true;
        }
    }

    return false;
}

Status CharsetLoader::getCharsetID(const std::string &charset_name,
                                   uint16_t &charset_id,
                                   ErrorContext *ctx)
{
    if (!catalog_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
        return Status::INVALID_ARGUMENT;
    }

    std::vector<CatalogManager::CharsetInfo> existing;
    Status status = catalog_->listCharsets(existing, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }

    std::string needle = normalizeName(charset_name);
    for (const auto& info : existing)
    {
        if (normalizeName(info.name) == needle)
        {
            charset_id = info.charset_id;
            return Status::OK;
        }
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Charset not found");
    return Status::NOT_FOUND;
}

uint64_t CharsetLoader::getCurrentTimestamp()
{
    // Get current time in microseconds since epoch
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
    return static_cast<uint64_t>(microseconds.count());
}

} // namespace scratchbird::core
