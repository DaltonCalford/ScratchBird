#include "scratchbird/core/charset.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include <cstring>
#include <algorithm>
#include <cctype>

namespace scratchbird::core
{

    CharsetManager::CharsetManager()
    {
        initializeCharsets();
        initializeCollations();
    }

    CharsetManager::~CharsetManager() = default;

    auto CharsetManager::loadFromCatalog(Database *db, ErrorContext *ctx) -> Status
    {
        if (db == nullptr || db->catalog_manager() == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Database or catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        // Load character sets from catalog
        std::vector<CatalogManager::CharsetInfo> catalog_charsets;
        Status status = db->catalog_manager()->listCharsets(catalog_charsets, ctx);

        if (status == Status::OK && !catalog_charsets.empty())
        {
            // Clear existing charsets and reload from catalog
            charsets_.clear();

            for (const auto &cs : catalog_charsets)
            {
                CharacterSetInfo info;
                info.id = static_cast<CharacterSet>(cs.charset_id);
                info.name = cs.name;
                info.description = cs.description;
                info.min_bytes = cs.min_bytes;
                info.max_bytes = cs.max_bytes;
                info.variable_width = cs.variable_width != 0;
                info.default_collation_id = cs.default_collation_id;
                charsets_.push_back(info);
            }
        }
        else if (status == Status::NOT_FOUND || catalog_charsets.empty())
        {
            // Catalog is empty, use hardcoded defaults
            // initializeCharsets() already called in constructor
        }
        else if (status != Status::OK)
        {
            return status;
        }

        // Load collations from catalog
        std::vector<CatalogManager::CollationCatalogInfo> catalog_collations;
        status = db->catalog_manager()->listCollations(catalog_collations, ctx);

        if (status == Status::OK && !catalog_collations.empty())
        {
            // Clear existing collations and reload from catalog
            collations_.clear();

            for (const auto &col : catalog_collations)
            {
                CollationInfo info;
                info.id = col.collation_id;
                info.name = col.name;
                info.charset = static_cast<CharacterSet>(col.charset_id);
                info.type = static_cast<CollationType>(col.collation_type);
                info.strength = static_cast<CollationStrength>(col.strength);
                info.pad_space = col.pad_space != 0;
                info.locale = col.locale;
                info.is_default = col.is_default != 0;
                collations_.push_back(info);
            }
        }
        else if (status == Status::NOT_FOUND || catalog_collations.empty())
        {
            // Catalog is empty, use hardcoded defaults
            // initializeCollations() already called in constructor
        }
        else if (status != Status::OK)
        {
            return status;
        }

        return Status::OK;
    }

    void CharsetManager::initializeCharsets()
    {
        // ASCII
        charsets_.push_back({
            CharacterSet::ASCII, "ascii", "7-bit ASCII",
            1,     // min_bytes
            1,     // max_bytes
            false, // variable_width
            1      // default_collation_id (ascii_bin)
        });

        // Latin1
        charsets_.push_back({
            CharacterSet::LATIN1, "latin1", "ISO-8859-1 Western European",
            1,     // min_bytes
            1,     // max_bytes
            false, // variable_width
            10     // default_collation_id (latin1_general_ci)
        });

        // UTF-8 (default)
        charsets_.push_back({
            CharacterSet::UTF8, "utf8", "UTF-8 Unicode",
            1,    // min_bytes
            4,    // max_bytes
            true, // variable_width
            100   // default_collation_id (utf8_general_ci)
        });

        // UTF-8MB4 (same as UTF-8 for our purposes)
        charsets_.push_back({
            CharacterSet::UTF8MB4, "utf8mb4", "UTF-8 Unicode (MySQL compatible)",
            1,    // min_bytes
            4,    // max_bytes
            true, // variable_width
            100   // default_collation_id (utf8_general_ci)
        });

        // UTF-16
        charsets_.push_back({
            CharacterSet::UTF16, "utf16", "UTF-16 Unicode",
            2,    // min_bytes
            4,    // max_bytes
            true, // variable_width
            200   // default_collation_id (utf16_general_ci)
        });

        // UTF-32
        charsets_.push_back({
            CharacterSet::UTF32, "utf32", "UTF-32 Unicode",
            4,     // min_bytes
            4,     // max_bytes
            false, // variable_width
            300    // default_collation_id (utf32_general_ci)
        });
    }

    void CharsetManager::initializeCollations()
    {
        // ASCII collations
        collations_.push_back({
            1, // id
            "ascii_bin", CharacterSet::ASCII, CollationType::BINARY, CollationStrength::IDENTICAL,
            true, // pad_space
            "",   // locale
            true  // is_default
        });

        collations_.push_back({
            2, // id
            "ascii_general_ci", CharacterSet::ASCII, CollationType::CASE_INSENSITIVE,
            CollationStrength::TERTIARY,
            true, // pad_space
            "",   // locale
            false // is_default
        });

        // Latin1 collations
        collations_.push_back({
            10, // id
            "latin1_bin", CharacterSet::LATIN1, CollationType::BINARY, CollationStrength::IDENTICAL,
            true, // pad_space
            "",   // locale
            false // is_default
        });

        collations_.push_back({
            11, // id
            "latin1_general_ci", CharacterSet::LATIN1, CollationType::CASE_INSENSITIVE,
            CollationStrength::TERTIARY,
            true, // pad_space
            "",   // locale
            true  // is_default
        });

        collations_.push_back({
            12, // id
            "latin1_general_cs", CharacterSet::LATIN1, CollationType::CASE_SENSITIVE,
            CollationStrength::TERTIARY,
            true, // pad_space
            "",   // locale
            false // is_default
        });

        // UTF-8 collations
        collations_.push_back({
            100, // id
            "utf8_bin", CharacterSet::UTF8, CollationType::BINARY, CollationStrength::IDENTICAL,
            true, // pad_space
            "",   // locale
            false // is_default
        });

        collations_.push_back({
            101, // id
            "utf8_general_ci", CharacterSet::UTF8, CollationType::CASE_INSENSITIVE,
            CollationStrength::TERTIARY,
            true, // pad_space
            "",   // locale
            true  // is_default (DEFAULT for UTF-8)
        });

        collations_.push_back({
            102, // id
            "utf8_unicode_ci", CharacterSet::UTF8, CollationType::UNICODE,
            CollationStrength::TERTIARY,
            true, // pad_space
            "",   // locale
            false // is_default
        });

        collations_.push_back({
            103, // id
            "utf8_unicode_cs", CharacterSet::UTF8, CollationType::UNICODE,
            CollationStrength::TERTIARY,
            true, // pad_space
            "",   // locale
            false // is_default
        });

        // Locale-specific UTF-8 collations
        collations_.push_back({
            110, // id
            "utf8_en_US_ci", CharacterSet::UTF8, CollationType::UNICODE,
            CollationStrength::TERTIARY,
            true,    // pad_space
            "en_US", // locale
            false    // is_default
        });

        collations_.push_back({
            111, // id
            "utf8_de_DE_ci", CharacterSet::UTF8, CollationType::UNICODE,
            CollationStrength::TERTIARY,
            true,    // pad_space
            "de_DE", // locale
            false    // is_default
        });

        // UTF-16 collations
        collations_.push_back({
            200, // id
            "utf16_bin", CharacterSet::UTF16, CollationType::BINARY, CollationStrength::IDENTICAL,
            true, // pad_space
            "",   // locale
            false // is_default
        });

        collations_.push_back({
            201, // id
            "utf16_general_ci", CharacterSet::UTF16, CollationType::CASE_INSENSITIVE,
            CollationStrength::TERTIARY,
            true, // pad_space
            "",   // locale
            true  // is_default
        });

        // UTF-32 collations
        collations_.push_back({
            300, // id
            "utf32_bin", CharacterSet::UTF32, CollationType::BINARY, CollationStrength::IDENTICAL,
            true, // pad_space
            "",   // locale
            false // is_default
        });

        collations_.push_back({
            301, // id
            "utf32_general_ci", CharacterSet::UTF32, CollationType::CASE_INSENSITIVE,
            CollationStrength::TERTIARY,
            true, // pad_space
            "",   // locale
            true  // is_default
        });
    }

    auto CharsetManager::getCharsetInfo(CharacterSet charset) const -> const CharacterSetInfo *
    {
        for (const auto &cs : charsets_)
        {
            if (cs.id == charset)
            {
                return &cs;
            }
        }
        return nullptr;
    }

    auto CharsetManager::getCharsetByName(const std::string &name) const -> CharacterSet
    {
        for (const auto &cs : charsets_)
        {
            if (cs.name == name)
            {
                return cs.id;
            }
        }
        return CharacterSet::UTF8; // Default
    }

    auto CharsetManager::getCharsetName(CharacterSet charset) const -> std::string
    {
        const auto *info = getCharsetInfo(charset);
        return info ? info->name : "utf8";
    }

    auto CharsetManager::getCollationInfo(uint32_t collation_id) const -> const CollationInfo *
    {
        for (const auto &coll : collations_)
        {
            if (coll.id == collation_id)
            {
                return &coll;
            }
        }
        return nullptr;
    }

    auto CharsetManager::getCollationByName(const std::string &name) const -> uint32_t
    {
        for (const auto &coll : collations_)
        {
            if (coll.name == name)
            {
                return coll.id;
            }
        }
        return 101; // Default: utf8_general_ci
    }

    auto CharsetManager::getCollationName(uint32_t collation_id) const -> std::string
    {
        const auto *info = getCollationInfo(collation_id);
        return info ? info->name : "utf8_general_ci";
    }

    auto CharsetManager::getDefaultCollation(CharacterSet charset) const -> uint32_t
    {
        for (const auto &coll : collations_)
        {
            if (coll.charset == charset && coll.is_default)
            {
                return coll.id;
            }
        }
        return 101; // Fallback: utf8_general_ci
    }

    auto CharsetManager::getCharLength(const uint8_t *str, uint32_t byte_len,
                                       CharacterSet charset) const -> uint32_t
    {
        if (charset == CharacterSet::UTF8 || charset == CharacterSet::UTF8MB4)
        {
            return utf8::char_length(str, byte_len);
        }
        else if (charset == CharacterSet::UTF16)
        {
            return utf16::char_length(str, byte_len);
        }
        else if (charset == CharacterSet::UTF32)
        {
            return utf32::char_length(str, byte_len);
        }
        // For fixed-width encodings, char count = byte count / bytes_per_char
        const auto *info = getCharsetInfo(charset);
        if (info && !info->variable_width)
        {
            return byte_len / info->min_bytes;
        }
        return byte_len; // Fallback
    }

    auto CharsetManager::getByteLength(const uint8_t *str, uint32_t char_count,
                                       CharacterSet charset) const -> uint32_t
    {
        if (charset == CharacterSet::UTF8 || charset == CharacterSet::UTF8MB4)
        {
            return utf8::byte_length(str, char_count);
        }
        else if (charset == CharacterSet::UTF16)
        {
            return utf16::byte_length(str, char_count);
        }
        else if (charset == CharacterSet::UTF32)
        {
            return utf32::byte_length(str, char_count);
        }
        const auto *info = getCharsetInfo(charset);
        if (info && !info->variable_width)
        {
            return char_count * info->min_bytes;
        }
        return char_count; // Fallback
    }

    auto CharsetManager::getMaxBytesPerChar(CharacterSet charset) const -> uint8_t
    {
        const auto *info = getCharsetInfo(charset);
        return info ? info->max_bytes : 4; // Default to UTF-8
    }

    auto CharsetManager::validate(const uint8_t *str, uint32_t byte_len, CharacterSet charset,
                                  ErrorContext *ctx) const -> Status
    {
        if (charset == CharacterSet::UTF8 || charset == CharacterSet::UTF8MB4)
        {
            if (!utf8::validate(str, byte_len))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UTF-8 sequence");
                return Status::INVALID_ARGUMENT;
            }
        }
        else if (charset == CharacterSet::UTF16)
        {
            if (!utf16::validate(str, byte_len))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UTF-16 sequence");
                return Status::INVALID_ARGUMENT;
            }
        }
        else if (charset == CharacterSet::UTF32)
        {
            if (!utf32::validate(str, byte_len))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UTF-32 sequence");
                return Status::INVALID_ARGUMENT;
            }
        }
        // For other charsets, assume valid for now
        return Status::OK;
    }

    auto CharsetManager::compare(const uint8_t *s1, uint32_t len1, const uint8_t *s2, uint32_t len2,
                                 uint32_t collation_id) const -> int
    {
        const auto *coll = getCollationInfo(collation_id);
        if (!coll)
        {
            // Fallback to binary comparison
            return utf8::compare_bin(s1, len1, s2, len2);
        }

        switch (coll->type)
        {
            case CollationType::BINARY:
                return utf8::compare_bin(s1, len1, s2, len2);

            case CollationType::CASE_INSENSITIVE:
                if (coll->charset == CharacterSet::UTF8 || coll->charset == CharacterSet::UTF8MB4)
                {
                    return utf8::compare_ci(s1, len1, s2, len2);
                }
                return utf8::compare_bin(s1, len1, s2, len2);

            default:
                // For now, fall back to binary comparison
                // TODO: Implement full UCA and locale-specific comparison
                return utf8::compare_bin(s1, len1, s2, len2);
        }
    }

    auto CharsetManager::convert(const uint8_t *input, uint32_t input_len, CharacterSet from_cs,
                                 std::vector<uint8_t> &output, CharacterSet to_cs,
                                 ErrorContext *ctx) const -> Status
    {
        // Identity conversion
        if (from_cs == to_cs)
        {
            output.assign(input, input + input_len);
            return Status::OK;
        }

        output.clear();
        output.reserve(input_len * 2); // Reserve space (estimate)

        // ASCII -> any UTF variant (simple copy, ASCII is subset)
        if (from_cs == CharacterSet::ASCII &&
            (to_cs == CharacterSet::UTF8 || to_cs == CharacterSet::UTF8MB4 ||
             to_cs == CharacterSet::UTF16 || to_cs == CharacterSet::UTF32))
        {
            if (to_cs == CharacterSet::UTF8 || to_cs == CharacterSet::UTF8MB4)
            {
                output.assign(input, input + input_len);
                return Status::OK;
            }
            // UTF-16/32 conversion would require encoding each byte
            // For now, fall through to not implemented
        }

        // Latin1 -> UTF-8 (straightforward, each byte is a codepoint)
        if (from_cs == CharacterSet::LATIN1 &&
            (to_cs == CharacterSet::UTF8 || to_cs == CharacterSet::UTF8MB4))
        {
            for (uint32_t i = 0; i < input_len; i++)
            {
                uint8_t c = input[i];
                if (c < 0x80)
                {
                    // ASCII range: 1 byte
                    output.push_back(c);
                }
                else
                {
                    // Latin1 range (0x80-0xFF): 2 bytes in UTF-8
                    output.push_back(0xC0 | (c >> 6));
                    output.push_back(0x80 | (c & 0x3F));
                }
            }
            return Status::OK;
        }

        // UTF-8 -> Latin1 (lossy, unmappable chars -> '?')
        if ((from_cs == CharacterSet::UTF8 || from_cs == CharacterSet::UTF8MB4) &&
            to_cs == CharacterSet::LATIN1)
        {
            uint32_t i = 0;
            while (i < input_len)
            {
                uint8_t c = input[i];

                // 1-byte sequence (ASCII): 0xxxxxxx
                if ((c & 0x80) == 0)
                {
                    output.push_back(c);
                    i++;
                }
                // 2-byte sequence: 110xxxxx 10xxxxxx
                else if ((c & 0xE0) == 0xC0 && i + 1 < input_len)
                {
                    uint32_t codepoint = ((c & 0x1F) << 6) | (input[i + 1] & 0x3F);
                    if (codepoint <= 0xFF)
                    {
                        output.push_back(static_cast<uint8_t>(codepoint));
                    }
                    else
                    {
                        output.push_back('?'); // Unmappable character
                    }
                    i += 2;
                }
                // 3-byte and 4-byte sequences cannot be represented in Latin1
                else if ((c & 0xF0) == 0xE0 && i + 2 < input_len)
                {
                    output.push_back('?'); // Unmappable character
                    i += 3;
                }
                else if ((c & 0xF8) == 0xF0 && i + 3 < input_len)
                {
                    output.push_back('?'); // Unmappable character
                    i += 4;
                }
                else
                {
                    // Invalid UTF-8 sequence
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UTF-8 sequence");
                    return Status::INVALID_ARGUMENT;
                }
            }
            return Status::OK;
        }

        // UTF-8 -> ASCII (very lossy, only ASCII chars pass through)
        if ((from_cs == CharacterSet::UTF8 || from_cs == CharacterSet::UTF8MB4) &&
            to_cs == CharacterSet::ASCII)
        {
            uint32_t i = 0;
            while (i < input_len)
            {
                uint8_t c = input[i];

                // 1-byte sequence (ASCII): 0xxxxxxx
                if ((c & 0x80) == 0)
                {
                    output.push_back(c);
                    i++;
                }
                // Any multi-byte sequence -> '?'
                else if ((c & 0xE0) == 0xC0 && i + 1 < input_len)
                {
                    output.push_back('?');
                    i += 2;
                }
                else if ((c & 0xF0) == 0xE0 && i + 2 < input_len)
                {
                    output.push_back('?');
                    i += 3;
                }
                else if ((c & 0xF8) == 0xF0 && i + 3 < input_len)
                {
                    output.push_back('?');
                    i += 4;
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UTF-8 sequence");
                    return Status::INVALID_ARGUMENT;
                }
            }
            return Status::OK;
        }

        // UTF-8MB4 and UTF-8 are identical in our implementation
        if ((from_cs == CharacterSet::UTF8 && to_cs == CharacterSet::UTF8MB4) ||
            (from_cs == CharacterSet::UTF8MB4 && to_cs == CharacterSet::UTF8))
        {
            output.assign(input, input + input_len);
            return Status::OK;
        }

        // All other conversions not yet implemented
        if (ctx != nullptr)
        {
            std::string msg = "Conversion from " + getCharsetName(from_cs) + " to " +
                              getCharsetName(to_cs) + " not yet implemented";
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, msg.c_str());
        }
        return Status::NOT_IMPLEMENTED;
    }

    // UTF-8 utility functions implementation
    namespace utf8
    {
        auto validate(const uint8_t *str, uint32_t byte_len) -> bool
        {
            uint32_t i = 0;
            while (i < byte_len)
            {
                uint8_t c = str[i];

                // 1-byte sequence (ASCII): 0xxxxxxx
                if ((c & 0x80) == 0)
                {
                    i += 1;
                }
                // 2-byte sequence: 110xxxxx 10xxxxxx
                else if ((c & 0xE0) == 0xC0)
                {
                    if (i + 1 >= byte_len || (str[i + 1] & 0xC0) != 0x80)
                        return false;
                    i += 2;
                }
                // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
                else if ((c & 0xF0) == 0xE0)
                {
                    if (i + 2 >= byte_len || (str[i + 1] & 0xC0) != 0x80 ||
                        (str[i + 2] & 0xC0) != 0x80)
                        return false;
                    i += 3;
                }
                // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                else if ((c & 0xF8) == 0xF0)
                {
                    if (i + 3 >= byte_len || (str[i + 1] & 0xC0) != 0x80 ||
                        (str[i + 2] & 0xC0) != 0x80 || (str[i + 3] & 0xC0) != 0x80)
                        return false;
                    i += 4;
                }
                else
                {
                    return false; // Invalid UTF-8 start byte
                }
            }
            return true;
        }

        auto char_length(const uint8_t *str, uint32_t byte_len) -> uint32_t
        {
            uint32_t char_count = 0;
            uint32_t i = 0;

            while (i < byte_len)
            {
                uint8_t c = str[i];
                if ((c & 0x80) == 0)
                    i += 1;
                else if ((c & 0xE0) == 0xC0)
                    i += 2;
                else if ((c & 0xF0) == 0xE0)
                    i += 3;
                else if ((c & 0xF8) == 0xF0)
                    i += 4;
                else
                    i += 1; // Invalid, skip byte

                char_count++;
            }

            return char_count;
        }

        auto byte_length(const uint8_t *str, uint32_t char_count) -> uint32_t
        {
            uint32_t byte_count = 0;
            uint32_t chars_seen = 0;
            uint32_t i = 0;

            while (chars_seen < char_count && str[i] != '\0')
            {
                uint8_t c = str[i];
                uint32_t char_bytes;

                if ((c & 0x80) == 0)
                    char_bytes = 1;
                else if ((c & 0xE0) == 0xC0)
                    char_bytes = 2;
                else if ((c & 0xF0) == 0xE0)
                    char_bytes = 3;
                else if ((c & 0xF8) == 0xF0)
                    char_bytes = 4;
                else
                    char_bytes = 1; // Invalid, count as 1

                byte_count += char_bytes;
                i += char_bytes;
                chars_seen++;
            }

            return byte_count;
        }

        auto char_byte_length(const uint8_t *str) -> uint32_t
        {
            uint8_t c = str[0];
            if ((c & 0x80) == 0)
                return 1;
            if ((c & 0xE0) == 0xC0)
                return 2;
            if ((c & 0xF0) == 0xE0)
                return 3;
            if ((c & 0xF8) == 0xF0)
                return 4;
            return 1; // Invalid
        }

        auto to_upper(const std::string &str) -> std::string
        {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return std::toupper(c); });
            return result;
        }

        auto to_lower(const std::string &str) -> std::string
        {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            return result;
        }

        auto compare_ci(const uint8_t *s1, uint32_t len1, const uint8_t *s2, uint32_t len2) -> int
        {
            // Simple ASCII case-insensitive comparison
            // TODO: Implement proper Unicode case folding
            uint32_t min_len = std::min(len1, len2);
            for (uint32_t i = 0; i < min_len; ++i)
            {
                unsigned char c1 = std::tolower(s1[i]);
                unsigned char c2 = std::tolower(s2[i]);
                if (c1 < c2)
                    return -1;
                if (c1 > c2)
                    return 1;
            }
            if (len1 < len2)
                return -1;
            if (len1 > len2)
                return 1;
            return 0;
        }

        auto compare_bin(const uint8_t *s1, uint32_t len1, const uint8_t *s2, uint32_t len2) -> int
        {
            uint32_t min_len = std::min(len1, len2);
            int result = std::memcmp(s1, s2, min_len);
            if (result != 0)
                return result;
            if (len1 < len2)
                return -1;
            if (len1 > len2)
                return 1;
            return 0;
        }

    } // namespace utf8

    // UTF-16 utility functions (little-endian)
    namespace utf16
    {
        auto validate(const uint8_t *str, uint32_t byte_len) -> bool
        {
            // UTF-16 must have even number of bytes
            if (byte_len % 2 != 0)
            {
                return false;
            }

            uint32_t i = 0;
            while (i < byte_len)
            {
                // Read 16-bit code unit (little-endian)
                uint16_t unit =
                    static_cast<uint16_t>(str[i]) | (static_cast<uint16_t>(str[i + 1]) << 8);

                // Check for high surrogate (0xD800-0xDBFF)
                if (unit >= 0xD800 && unit <= 0xDBFF)
                {
                    // Must be followed by low surrogate
                    if (i + 3 >= byte_len)
                    {
                        return false; // Incomplete surrogate pair
                    }

                    uint16_t low_unit = static_cast<uint16_t>(str[i + 2]) |
                                        (static_cast<uint16_t>(str[i + 3]) << 8);

                    // Validate low surrogate (0xDC00-0xDFFF)
                    if (low_unit < 0xDC00 || low_unit > 0xDFFF)
                    {
                        return false; // Invalid surrogate pair
                    }

                    i += 4; // Surrogate pair is 4 bytes
                }
                // Check for low surrogate without high surrogate (invalid)
                else if (unit >= 0xDC00 && unit <= 0xDFFF)
                {
                    return false; // Unpaired low surrogate
                }
                else
                {
                    // Regular BMP character
                    i += 2;
                }
            }

            return true;
        }

        auto char_length(const uint8_t *str, uint32_t byte_len) -> uint32_t
        {
            if (byte_len % 2 != 0)
            {
                return 0; // Invalid UTF-16
            }

            uint32_t char_count = 0;
            uint32_t i = 0;

            while (i < byte_len)
            {
                uint16_t unit =
                    static_cast<uint16_t>(str[i]) | (static_cast<uint16_t>(str[i + 1]) << 8);

                if (unit >= 0xD800 && unit <= 0xDBFF)
                {
                    // High surrogate - surrogate pair (1 character, 4 bytes)
                    i += 4;
                }
                else
                {
                    // BMP character (1 character, 2 bytes)
                    i += 2;
                }

                char_count++;
            }

            return char_count;
        }

        auto byte_length(const uint8_t *str, uint32_t char_count) -> uint32_t
        {
            uint32_t byte_count = 0;
            uint32_t chars_seen = 0;
            uint32_t i = 0;

            while (chars_seen < char_count && i < 10000) // Safety limit
            {
                uint16_t unit =
                    static_cast<uint16_t>(str[i]) | (static_cast<uint16_t>(str[i + 1]) << 8);

                if (unit >= 0xD800 && unit <= 0xDBFF)
                {
                    // Surrogate pair - 4 bytes
                    byte_count += 4;
                    i += 4;
                }
                else
                {
                    // BMP character - 2 bytes
                    byte_count += 2;
                    i += 2;
                }

                chars_seen++;
            }

            return byte_count;
        }

        auto char_byte_length(const uint8_t *str) -> uint32_t
        {
            uint16_t unit = static_cast<uint16_t>(str[0]) | (static_cast<uint16_t>(str[1]) << 8);

            // Check if high surrogate (surrogate pair = 4 bytes)
            if (unit >= 0xD800 && unit <= 0xDBFF)
            {
                return 4;
            }

            // Regular BMP character = 2 bytes
            return 2;
        }

    } // namespace utf16

    // UTF-32 utility functions (little-endian)
    namespace utf32
    {
        auto validate(const uint8_t *str, uint32_t byte_len) -> bool
        {
            // UTF-32 must have multiple of 4 bytes
            if (byte_len % 4 != 0)
            {
                return false;
            }

            uint32_t i = 0;
            while (i < byte_len)
            {
                // Read 32-bit code point (little-endian)
                uint32_t codepoint = static_cast<uint32_t>(str[i]) |
                                     (static_cast<uint32_t>(str[i + 1]) << 8) |
                                     (static_cast<uint32_t>(str[i + 2]) << 16) |
                                     (static_cast<uint32_t>(str[i + 3]) << 24);

                // Valid Unicode range: U+0000 to U+10FFFF
                if (codepoint > 0x10FFFF)
                {
                    return false;
                }

                // Surrogate pairs (U+D800 to U+DFFF) are invalid in UTF-32
                if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
                {
                    return false;
                }

                i += 4;
            }

            return true;
        }

        auto char_length(const uint8_t *str, uint32_t byte_len) -> uint32_t
        {
            // UTF-32 is fixed-width: 4 bytes per character
            if (byte_len % 4 != 0)
            {
                return 0; // Invalid UTF-32
            }

            return byte_len / 4;
        }

        auto byte_length(const uint8_t *str, uint32_t char_count) -> uint32_t
        {
            // UTF-32 is fixed-width: 4 bytes per character
            return char_count * 4;
        }

        auto char_byte_length(const uint8_t *str) -> uint32_t
        {
            // UTF-32 is always 4 bytes per character
            return 4;
        }

    } // namespace utf32

} // namespace scratchbird::core
