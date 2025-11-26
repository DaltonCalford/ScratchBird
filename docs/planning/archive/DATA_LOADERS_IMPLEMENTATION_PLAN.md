# Data Loaders Implementation Plan - Timezone & Character Set Files

**Created:** November 23, 2025
**Priority:** P1 - HIGH (Required for full temporal and text functionality)
**Estimated Effort:** 40-50 hours
**Target:** Alpha 1 completion
**Dependencies:** Catalog system must be operational

---

## OVERVIEW

This plan implements file-based data loaders for:
1. **Timezone data** - Load IANA timezone database into `pg_timezone` catalog
2. **Character set definitions** - Load character set mappings into `pg_charsets` catalog

Both loaders should:
- Parse standard file formats
- Populate catalog tables
- Support incremental updates
- Validate data integrity
- Provide CLI tools for loading

**Data File Locations:**
- **Timezone data:** `resources/timezones/` (IANA tzdata 2024b)
- **Character sets:** `resources/charsets/charsets.json`
- **Collations:** `resources/collations/collations.json`
- **Configuration:** `resources/config/sb_config.ini`

**Execution Strategy:** Can be split into 2 parallel agents:
- **Agent A:** Timezone Data Loader (20-25 hours)
- **Agent B:** Character Set Loader (20-25 hours)

---

## AGENT A: TIMEZONE DATA LOADER

**Estimated Effort:** 20-25 hours
**Component:** Temporal / Catalog System

---

### Background: IANA Timezone Database

The IANA timezone database (tzdata/tzdb) is the authoritative source for timezone information worldwide.

**Standard Format:** Binary TZif format (Time Zone Information Format)

**File Locations:**
- **Project resources:** `resources/timezones/` (IANA tzdata 2024b - included in repository)
- **System location (Linux):** `/usr/share/zoneinfo/` (if available)
- **Fallback:** `resources/timezones/` should always be used as the primary source

**File Structure:**
- Header (magic number, version)
- Transition times (when rules change)
- Transition types (UTC offset, DST flag, abbreviation)
- Leap second records
- POSIX TZ string (for future dates)

**Included Files in resources/timezones/:**
- Regional data files: `africa`, `asia`, `europe`, `northamerica`, `southamerica`, `australasia`, `antarctica`
- Additional files: `etcetera`, `backward`, `backzone`, `factory`
- Leap seconds: `leapseconds`, `leap-seconds.list`
- Coordinate data: `zone.tab`, `zone1970.tab`, `zonenow.tab`
- Compressed archives: `tzdata2024b.tar.gz`, `tzcode2024b.tar.gz`

---

### Phase 1: TZif File Parser (8-10 hours)

**File:** `/home/dcalford/CliWork/ScratchBird/src/core/tzfile_parser.cpp`

#### TZif Format Structure

```cpp
// TZif version 2/3 format
struct TZifHeader {
    char magic[4];        // "TZif"
    uint8_t version;      // '2' or '3'
    char reserved[15];
    uint32_t ttisgmtcnt;  // # of UTC/local indicators
    uint32_t ttisstdcnt;  // # of standard/wall indicators
    uint32_t leapcnt;     // # of leap second records
    uint32_t timecnt;     // # of transition times
    uint32_t typecnt;     // # of transition types
    uint32_t charcnt;     // # of timezone abbrev chars
};

struct TransitionTime {
    int64_t time;         // Unix timestamp of transition
    uint8_t type_index;   // Index into transition types
};

struct TransitionType {
    int32_t utoff;        // UTC offset in seconds
    uint8_t isdst;        // Is DST in effect (0 or 1)
    uint8_t abbrind;      // Index into abbreviation string
};

struct LeapSecond {
    int64_t time;         // When leap second occurs
    int32_t total;        // Total leap seconds after this
};
```

#### Parser Implementation

```cpp
#ifndef SCRATCHBIRD_CORE_TZFILE_PARSER_H
#define SCRATCHBIRD_CORE_TZFILE_PARSER_H

#include <string>
#include <vector>
#include "status.h"
#include "error_context.h"

namespace scratchbird {
namespace core {

struct TimezoneTransition {
    int64_t timestamp;        // Unix timestamp
    int32_t utc_offset;       // Seconds from UTC
    bool is_dst;              // Daylight saving time?
    std::string abbreviation; // e.g., "EST", "EDT"
};

struct TimezoneInfo {
    std::string name;                          // e.g., "America/New_York"
    std::vector<TimezoneTransition> transitions;
    std::string posix_tz_string;               // For future dates
    std::vector<LeapSecond> leap_seconds;
};

class TZFileParser {
public:
    // Parse a single TZif file
    Status parseFile(const std::string& filepath,
                    TimezoneInfo& tz_info,
                    ErrorContext* ctx);

    // Parse entire timezone directory tree
    Status parseDirectory(const std::string& zoneinfo_dir,
                         std::vector<TimezoneInfo>& timezones,
                         ErrorContext* ctx);

private:
    Status readHeader(FILE* fp, TZifHeader& header, ErrorContext* ctx);
    Status readTransitions(FILE* fp, const TZifHeader& header,
                          std::vector<TransitionTime>& transitions,
                          ErrorContext* ctx);
    Status readTypes(FILE* fp, const TZifHeader& header,
                    std::vector<TransitionType>& types,
                    ErrorContext* ctx);
    Status readAbbreviations(FILE* fp, const TZifHeader& header,
                            std::string& abbrevs,
                            ErrorContext* ctx);
    Status readLeapSeconds(FILE* fp, const TZifHeader& header,
                          std::vector<LeapSecond>& leaps,
                          ErrorContext* ctx);
    Status readPosixTZ(FILE* fp, std::string& posix_tz, ErrorContext* ctx);

    uint32_t read32(FILE* fp);
    uint64_t read64(FILE* fp);
    void byteSwap32(uint32_t& val);
    void byteSwap64(uint64_t& val);
};

}}  // namespace scratchbird::core

#endif  // SCRATCHBIRD_CORE_TZFILE_PARSER_H
```

#### Implementation Example

```cpp
#include "scratchbird/core/tzfile_parser.h"
#include <cstdio>
#include <cstring>
#include <arpa/inet.h>  // for ntohl, ntohll

namespace scratchbird {
namespace core {

Status TZFileParser::parseFile(const std::string& filepath,
                                TimezoneInfo& tz_info,
                                ErrorContext* ctx) {
    FILE* fp = fopen(filepath.c_str(), "rb");
    if (!fp) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
            "Cannot open timezone file: " + filepath);
        return Status::NOT_FOUND;
    }

    // Read header
    TZifHeader header;
    Status status = readHeader(fp, header, ctx);
    if (status != Status::OK) {
        fclose(fp);
        return status;
    }

    // Validate magic number
    if (memcmp(header.magic, "TZif", 4) != 0) {
        fclose(fp);
        SET_ERROR_CONTEXT(ctx, Status::INVALID_FORMAT,
            "Invalid TZif magic number");
        return Status::INVALID_FORMAT;
    }

    // Read version 1 data (skip, we'll use version 2/3)
    if (header.version == '2' || header.version == '3') {
        // Skip version 1 block
        size_t v1_size = header.timecnt * 5 + header.typecnt * 6 +
                        header.charcnt + header.leapcnt * 8 +
                        header.ttisstdcnt + header.ttisgmtcnt;
        fseek(fp, v1_size, SEEK_CUR);

        // Read version 2/3 header
        status = readHeader(fp, header, ctx);
        if (status != Status::OK) {
            fclose(fp);
            return status;
        }
    }

    // Read transition times
    std::vector<TransitionTime> raw_transitions;
    status = readTransitions(fp, header, raw_transitions, ctx);
    if (status != Status::OK) {
        fclose(fp);
        return status;
    }

    // Read transition types
    std::vector<TransitionType> types;
    status = readTypes(fp, header, types, ctx);
    if (status != Status::OK) {
        fclose(fp);
        return status;
    }

    // Read abbreviation strings
    std::string abbrevs;
    status = readAbbreviations(fp, header, abbrevs, ctx);
    if (status != Status::OK) {
        fclose(fp);
        return status;
    }

    // Read leap seconds
    status = readLeapSeconds(fp, header, tz_info.leap_seconds, ctx);
    if (status != Status::OK) {
        fclose(fp);
        return status;
    }

    // Read POSIX TZ string
    status = readPosixTZ(fp, tz_info.posix_tz_string, ctx);
    if (status != Status::OK) {
        fclose(fp);
        return status;
    }

    // Combine transitions and types
    for (const auto& trans : raw_transitions) {
        const TransitionType& type = types[trans.type_index];

        TimezoneTransition tz_trans;
        tz_trans.timestamp = trans.time;
        tz_trans.utc_offset = type.utoff;
        tz_trans.is_dst = (type.isdst != 0);

        // Extract abbreviation
        size_t abbr_start = type.abbrind;
        size_t abbr_end = abbrevs.find('\0', abbr_start);
        tz_trans.abbreviation = abbrevs.substr(abbr_start, abbr_end - abbr_start);

        tz_info.transitions.push_back(tz_trans);
    }

    // Extract timezone name from filepath
    // resources/timezones/america -> america
    // /usr/share/zoneinfo/America/New_York -> America/New_York
    size_t zoneinfo_pos = filepath.find("/zoneinfo/");
    if (zoneinfo_pos != std::string::npos) {
        tz_info.name = filepath.substr(zoneinfo_pos + 10);
    } else {
        // Check for resources/timezones/ prefix
        size_t resources_pos = filepath.find("resources/timezones/");
        if (resources_pos != std::string::npos) {
            tz_info.name = filepath.substr(resources_pos + 20);
        } else {
            tz_info.name = filepath;
        }
    }

    fclose(fp);
    return Status::OK;
}

Status TZFileParser::parseDirectory(const std::string& zoneinfo_dir,
                                    std::vector<TimezoneInfo>& timezones,
                                    ErrorContext* ctx) {
    // Parse regional timezone data files from resources/timezones/
    // These are the IANA source files (text format with zone definitions)

    const std::vector<std::string> regional_files = {
        "africa", "antarctica", "asia", "australasia",
        "europe", "northamerica", "southamerica",
        "etcetera", "backward"
    };

    // For the implementation plan, we'll parse these source files
    // OR if zoneinfo_dir points to a compiled zoneinfo directory,
    // parse binary TZif files for common timezones:

    const std::vector<std::string> common_binary_zones = {
        "UTC", "GMT", "EST", "PST", "MST", "CST",
        "America/New_York", "America/Los_Angeles", "America/Chicago",
        "America/Denver", "America/Phoenix", "America/Anchorage",
        "America/Toronto", "America/Mexico_City", "America/Sao_Paulo",
        "Europe/London", "Europe/Paris", "Europe/Berlin", "Europe/Moscow",
        "Asia/Tokyo", "Asia/Shanghai", "Asia/Dubai", "Asia/Kolkata",
        "Australia/Sydney", "Pacific/Auckland",
        // ... (594 total in full implementation)
    };

    // Check if directory contains source files or compiled binaries
    std::string test_file = zoneinfo_dir + "/africa";
    FILE* test_fp = fopen(test_file.c_str(), "rb");
    bool is_source_dir = (test_fp != nullptr);
    if (test_fp) fclose(test_fp);

    if (is_source_dir) {
        // Parse source files (requires zic compiler or custom parser)
        for (const auto& file : regional_files) {
            std::string filepath = zoneinfo_dir + "/" + file;
            // TODO: Parse IANA source format (text-based zone definitions)
            // This is complex - consider using system 'zic' compiler
            LOG_INFO("Found timezone source file: " + file);
        }
    } else {
        // Parse compiled binary TZif files
        for (const auto& zone : common_binary_zones) {
            std::string filepath = zoneinfo_dir + "/" + zone;
            TimezoneInfo tz_info;
            Status status = parseFile(filepath, tz_info, ctx);
            if (status == Status::OK) {
                timezones.push_back(tz_info);
            } else {
                // Log warning but continue
                LOG_WARNING("Failed to parse timezone: " + zone);
            }
        }
    }

    return Status::OK;
}

uint32_t TZFileParser::read32(FILE* fp) {
    uint32_t val;
    fread(&val, 4, 1, fp);
    return ntohl(val);  // Convert big-endian to host
}

uint64_t TZFileParser::read64(FILE* fp) {
    uint64_t val;
    fread(&val, 8, 1, fp);
    // Manual byte swap for 64-bit (ntohll may not be available)
    byteSwap64(val);
    return val;
}

}}  // namespace scratchbird::core
```

---

### Phase 2: Catalog Integration (4-6 hours)

**File:** `/home/dcalford/CliWork/ScratchBird/src/core/timezone_loader.cpp`

#### Catalog Table Schema

```sql
CREATE TABLE pg_timezone (
    tz_id UUID PRIMARY KEY,
    tz_name VARCHAR(128) UNIQUE NOT NULL,  -- e.g., "America/New_York"
    posix_string TEXT,                      -- POSIX TZ string for future dates
    is_dst_observing BOOLEAN,               -- Does this zone observe DST?
    created_at BIGINT NOT NULL
);

CREATE TABLE pg_timezone_transitions (
    transition_id UUID PRIMARY KEY,
    tz_id UUID NOT NULL REFERENCES pg_timezone(tz_id),
    transition_time BIGINT NOT NULL,        -- Unix timestamp
    utc_offset INTEGER NOT NULL,            -- Seconds from UTC
    is_dst BOOLEAN NOT NULL,
    abbreviation VARCHAR(16) NOT NULL,      -- "EST", "EDT", etc.
    UNIQUE(tz_id, transition_time)
);

CREATE INDEX idx_tz_transitions_time ON pg_timezone_transitions(tz_id, transition_time);
```

#### Loader Implementation

```cpp
class TimezoneLoader {
public:
    TimezoneLoader(CatalogManager* catalog, Database* db)
        : catalog_(catalog), db_(db) {}

    // Load single timezone into catalog
    Status loadTimezone(const TimezoneInfo& tz_info, ErrorContext* ctx);

    // Load all timezones from directory
    Status loadFromDirectory(const std::string& zoneinfo_dir, ErrorContext* ctx);

    // Update existing timezone data
    Status updateTimezone(const std::string& tz_name,
                         const TimezoneInfo& tz_info,
                         ErrorContext* ctx);

    // Clear all timezone data
    Status clearAllTimezones(ErrorContext* ctx);

private:
    CatalogManager* catalog_;
    Database* db_;

    ID generateTimezoneID(const std::string& tz_name);
};

Status TimezoneLoader::loadTimezone(const TimezoneInfo& tz_info,
                                    ErrorContext* ctx) {
    // Generate UUID for timezone
    ID tz_id = generateTimezoneID(tz_info.name);

    // Insert into pg_timezone
    Status status = catalog_->insertTimezone(
        tz_id,
        tz_info.name,
        tz_info.posix_tz_string,
        !tz_info.transitions.empty(),  // has DST if has transitions
        getCurrentTimestamp(),
        ctx
    );

    if (status != Status::OK) {
        return status;
    }

    // Insert transitions
    for (const auto& trans : tz_info.transitions) {
        ID trans_id = generateUUID();

        status = catalog_->insertTimezoneTransition(
            trans_id,
            tz_id,
            trans.timestamp,
            trans.utc_offset,
            trans.is_dst,
            trans.abbreviation,
            ctx
        );

        if (status != Status::OK) {
            // Rollback?
            return status;
        }
    }

    return Status::OK;
}

Status TimezoneLoader::loadFromDirectory(const std::string& zoneinfo_dir,
                                         ErrorContext* ctx) {
    // Parse all timezone files
    TZFileParser parser;
    std::vector<TimezoneInfo> timezones;
    Status status = parser.parseDirectory(zoneinfo_dir, timezones, ctx);

    if (status != Status::OK) {
        return status;
    }

    LOG_INFO("Parsed " + std::to_string(timezones.size()) + " timezones");

    // Begin transaction
    TransactionId xid = db_->beginTransaction(IsolationLevel::READ_COMMITTED, false);

    // Load each timezone
    size_t loaded = 0;
    for (const auto& tz : timezones) {
        status = loadTimezone(tz, ctx);
        if (status == Status::OK) {
            loaded++;
        } else {
            LOG_WARNING("Failed to load timezone: " + tz.name);
        }
    }

    // Commit transaction
    db_->commitTransaction(xid);

    LOG_INFO("Loaded " + std::to_string(loaded) + " timezones into catalog");
    return Status::OK;
}
```

---

### Phase 3: CLI Tool (4-5 hours)

**File:** `/home/dcalford/CliWork/ScratchBird/tools/sb_timezone_loader.cpp`

```cpp
// Command-line tool to load timezone data

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: sb_timezone_loader <database_path> [zoneinfo_dir]\n";
        std::cerr << "Examples:\n";
        std::cerr << "  sb_timezone_loader /data/mydb.sb resources/timezones\n";
        std::cerr << "  sb_timezone_loader /data/mydb.sb /usr/share/zoneinfo\n";
        std::cerr << "  sb_timezone_loader /data/mydb.sb  # Uses default: resources/timezones\n";
        return 1;
    }

    std::string db_path = argv[1];
    std::string zoneinfo_dir = (argc >= 3) ? argv[2] : "resources/timezones";

    // Open database
    ErrorContext ctx;
    Database* db = Database::open(db_path, &ctx);
    if (!db) {
        std::cerr << "Failed to open database: " << ctx.message << "\n";
        return 1;
    }

    // Create loader
    TimezoneLoader loader(db->getCatalog(), db);

    // Load timezones
    std::cout << "Loading timezones from " << zoneinfo_dir << "...\n";
    Status status = loader.loadFromDirectory(zoneinfo_dir, &ctx);

    if (status == Status::OK) {
        std::cout << "Timezone data loaded successfully!\n";
        return 0;
    } else {
        std::cerr << "Failed to load timezone data: " << ctx.message << "\n";
        return 1;
    }
}
```

**CMakeLists.txt entry:**
```cmake
add_executable(sb_timezone_loader tools/sb_timezone_loader.cpp)
target_link_libraries(sb_timezone_loader scratchbird_core)
```

---

### Phase 4: Testing (4-5 hours)

**Test Coverage:**

```cpp
TEST(TimezoneLoader, ParseTZifFile) {
    TZFileParser parser;
    TimezoneInfo tz_info;
    ErrorContext ctx;

    Status status = parser.parseFile("/usr/share/zoneinfo/America/New_York",
                                     tz_info, &ctx);
    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ("America/New_York", tz_info.name);
    EXPECT_GT(tz_info.transitions.size(), 0);
}

TEST(TimezoneLoader, LoadIntoDatabase) {
    Database* db = createTestDatabase();
    TimezoneLoader loader(db->getCatalog(), db);

    TimezoneInfo tz_info;
    tz_info.name = "America/New_York";
    tz_info.posix_tz_string = "EST5EDT,M3.2.0,M11.1.0";

    TimezoneTransition trans;
    trans.timestamp = 1615708800;  // March 14, 2021 2am
    trans.utc_offset = -14400;     // EDT = UTC-4
    trans.is_dst = true;
    trans.abbreviation = "EDT";
    tz_info.transitions.push_back(trans);

    ErrorContext ctx;
    Status status = loader.loadTimezone(tz_info, &ctx);
    EXPECT_EQ(Status::OK, status);

    // Verify data in catalog
    auto tz_opt = db->getCatalog()->getTimezone("America/New_York", &ctx);
    EXPECT_TRUE(tz_opt.has_value());
}

TEST(TimezoneLoader, LoadEntireDirectory) {
    Database* db = createTestDatabase();
    TimezoneLoader loader(db->getCatalog(), db);

    ErrorContext ctx;
    Status status = loader.loadFromDirectory("/usr/share/zoneinfo", &ctx);
    EXPECT_EQ(Status::OK, status);

    // Should have loaded common zones
    EXPECT_TRUE(db->getCatalog()->getTimezone("UTC", &ctx).has_value());
    EXPECT_TRUE(db->getCatalog()->getTimezone("America/New_York", &ctx).has_value());
    EXPECT_TRUE(db->getCatalog()->getTimezone("Europe/London", &ctx).has_value());
}
```

---

## AGENT B: CHARACTER SET LOADER

**Estimated Effort:** 20-25 hours
**Component:** Text Processing / Catalog System

---

### Background: Character Set Files

Character sets define mappings between byte sequences and Unicode code points.

**File Locations:**
- **Project resources:** `resources/charsets/charsets.json` (39 character sets - included in repository)
- **Collations:** `resources/collations/collations.json` (30+ collations - included in repository)

**File Format:** JSON format with character set definitions

**Included Character Sets (39 total):**
- **Unicode:** UTF-8, UTF-16, UTF-32
- **Western European:** ASCII, ISO-8859-1/15, Windows-1252, MacRoman
- **Central/Eastern European:** ISO-8859-2/3/4, Windows-1250/1251
- **Asian:** Shift_JIS, EUC-JP, GB2312, GBK, GB18030, Big5, EUC-KR
- **Other:** Cyrillic (KOI8-R/U), Arabic, Hebrew, Greek, Turkish, Thai

**Included Collations (30+ total):**
- MySQL: utf8mb4_general_ci, utf8mb4_unicode_ci, utf8mb4_0900_ai_ci
- PostgreSQL: en_US.UTF-8, C, POSIX
- SQL Server: Latin1_General_CI_AS, SQL_Latin1_General_CP1_CI_AS
- Firebird: UTF8_UNICODE, UTF8_UNICODE_CI
- Oracle: AL32UTF8, WE8MSWIN1252

---

### Phase 1: Character Set File Parser (8-10 hours)

**File:** `/home/dcalford/CliWork/ScratchBird/src/core/charset_parser.cpp`

#### Character Set Data Structure

```cpp
struct CharacterMapping {
    uint32_t byte_sequence;     // 1-4 bytes (stored as uint32)
    uint32_t unicode_codepoint; // Unicode code point
    uint8_t byte_length;        // 1, 2, 3, or 4
};

struct CharacterSet {
    std::string name;           // e.g., "UTF-8", "ISO-8859-1"
    std::string description;    // Human-readable description
    uint8_t max_bytes;          // Maximum bytes per character
    uint8_t min_bytes;          // Minimum bytes per character
    bool is_variable_width;     // Variable vs fixed width
    std::vector<CharacterMapping> mappings;
    std::string aliases;        // Comma-separated aliases
};

struct Collation {
    std::string name;           // e.g., "utf8_general_ci"
    std::string charset_name;   // Parent character set
    bool case_insensitive;      // Case-insensitive comparison
    bool accent_insensitive;    // Accent-insensitive comparison
    std::string language;       // Language code (e.g., "en", "fr")
};
```

#### Parser for JSON Character Set Definitions

```cpp
class CharsetParser {
public:
    // Parse JSON character set file (resources/charsets/charsets.json)
    Status parseJSONFile(const std::string& filepath,
                        std::vector<CharacterSet>& charsets,
                        ErrorContext* ctx);

    // Parse collations JSON file (resources/collations/collations.json)
    Status parseCollationsFile(const std::string& filepath,
                              std::vector<Collation>& collations,
                              ErrorContext* ctx);

    // Generate built-in character sets (UTF-8, ASCII, etc.)
    // These don't require external files
    Status generateBuiltinCharsets(std::vector<CharacterSet>& charsets);

private:
    Status parseCharsetJSON(const nlohmann::json& j, CharacterSet& charset);
    Status parseCollationJSON(const nlohmann::json& j, Collation& collation);
};
```

#### Built-in Character Sets (No File Required)

```cpp
Status CharsetParser::generateBuiltinCharsets(std::vector<CharacterSet>& charsets) {
    // UTF-8 (built-in, no file needed)
    CharacterSet utf8;
    utf8.name = "UTF-8";
    utf8.description = "Unicode Transformation Format, 8-bit";
    utf8.max_bytes = 4;
    utf8.min_bytes = 1;
    utf8.is_variable_width = true;
    utf8.aliases = "utf8,UTF8";
    // UTF-8 encoding is algorithmic, no explicit mappings needed
    charsets.push_back(utf8);

    // ASCII (subset of UTF-8)
    CharacterSet ascii;
    ascii.name = "ASCII";
    ascii.description = "American Standard Code for Information Interchange";
    ascii.max_bytes = 1;
    ascii.min_bytes = 1;
    ascii.is_variable_width = false;
    ascii.aliases = "US-ASCII,ANSI_X3.4-1968";
    // ASCII is 0x00-0x7F direct mapping
    for (uint32_t i = 0; i < 128; i++) {
        CharacterMapping mapping;
        mapping.byte_sequence = i;
        mapping.unicode_codepoint = i;
        mapping.byte_length = 1;
        ascii.mappings.push_back(mapping);
    }
    charsets.push_back(ascii);

    // ISO-8859-1 (Latin-1)
    CharacterSet latin1;
    latin1.name = "ISO-8859-1";
    latin1.description = "Latin-1 Western European";
    latin1.max_bytes = 1;
    latin1.min_bytes = 1;
    latin1.is_variable_width = false;
    latin1.aliases = "latin1,ISO_8859-1,CP819";
    // ISO-8859-1 is direct mapping to Unicode (0x00-0xFF)
    for (uint32_t i = 0; i < 256; i++) {
        CharacterMapping mapping;
        mapping.byte_sequence = i;
        mapping.unicode_codepoint = i;
        mapping.byte_length = 1;
        latin1.mappings.push_back(mapping);
    }
    charsets.push_back(latin1);

    // UTF-16
    CharacterSet utf16;
    utf16.name = "UTF-16";
    utf16.description = "Unicode Transformation Format, 16-bit";
    utf16.max_bytes = 4;
    utf16.min_bytes = 2;
    utf16.is_variable_width = true;
    utf16.aliases = "utf16,UTF16";
    charsets.push_back(utf16);

    // UTF-32
    CharacterSet utf32;
    utf32.name = "UTF-32";
    utf32.description = "Unicode Transformation Format, 32-bit";
    utf32.max_bytes = 4;
    utf32.min_bytes = 4;
    utf32.is_variable_width = false;
    utf32.aliases = "utf32,UTF32";
    charsets.push_back(utf32);

    return Status::OK;
}
```

---

### Phase 2: Catalog Integration (4-6 hours)

**File:** `/home/dcalford/CliWork/ScratchBird/src/core/charset_loader.cpp`

#### Catalog Table Schema

```sql
CREATE TABLE pg_charsets (
    charset_id UUID PRIMARY KEY,
    charset_name VARCHAR(64) UNIQUE NOT NULL,
    description VARCHAR(256),
    max_bytes INTEGER NOT NULL,
    min_bytes INTEGER NOT NULL,
    is_variable_width BOOLEAN NOT NULL,
    aliases TEXT,
    created_at BIGINT NOT NULL
);

CREATE TABLE pg_collations (
    collation_id UUID PRIMARY KEY,
    collation_name VARCHAR(128) UNIQUE NOT NULL,
    charset_id UUID NOT NULL REFERENCES pg_charsets(charset_id),
    case_insensitive BOOLEAN NOT NULL,
    accent_insensitive BOOLEAN NOT NULL,
    language_code VARCHAR(8),
    sort_rules TEXT,  -- JSON format for complex rules
    created_at BIGINT NOT NULL
);
```

#### Loader Implementation

```cpp
class CharsetLoader {
public:
    CharsetLoader(CatalogManager* catalog, Database* db)
        : catalog_(catalog), db_(db) {}

    // Load single character set
    Status loadCharset(const CharacterSet& charset, ErrorContext* ctx);

    // Load collation
    Status loadCollation(const Collation& collation, ErrorContext* ctx);

    // Load built-in character sets (UTF-8, ASCII, etc.)
    Status loadBuiltinCharsets(ErrorContext* ctx);

    // Load from directory
    Status loadFromDirectory(const std::string& charset_dir, ErrorContext* ctx);

private:
    CatalogManager* catalog_;
    Database* db_;
};

Status CharsetLoader::loadBuiltinCharsets(ErrorContext* ctx) {
    CharsetParser parser;
    std::vector<CharacterSet> charsets;

    Status status = parser.generateBuiltinCharsets(charsets);
    if (status != Status::OK) {
        return status;
    }

    // Begin transaction
    TransactionId xid = db_->beginTransaction(IsolationLevel::READ_COMMITTED, false);

    // Load each charset
    for (const auto& charset : charsets) {
        status = loadCharset(charset, ctx);
        if (status != Status::OK) {
            db_->rollbackTransaction(xid);
            return status;
        }
    }

    // Load default collations
    std::vector<Collation> default_collations = {
        {"utf8_general_ci", "UTF-8", true, false, ""},
        {"utf8_bin", "UTF-8", false, false, ""},
        {"ascii_general_ci", "ASCII", true, false, ""},
        {"latin1_general_ci", "ISO-8859-1", true, false, ""},
    };

    for (const auto& collation : default_collations) {
        status = loadCollation(collation, ctx);
        if (status != Status::OK) {
            db_->rollbackTransaction(xid);
            return status;
        }
    }

    db_->commitTransaction(xid);
    return Status::OK;
}
```

---

### Phase 3: CLI Tool (4-5 hours)

**File:** `/home/dcalford/CliWork/ScratchBird/tools/sb_charset_loader.cpp`

```cpp
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: sb_charset_loader <database_path> [options]\n";
        std::cerr << "Options:\n";
        std::cerr << "  --builtin              Load built-in charsets only (UTF-8, ASCII, etc.)\n";
        std::cerr << "  --all                  Load all charsets from resources/charsets/\n";
        std::cerr << "  --json <file>          Load from specific JSON file\n";
        std::cerr << "\nExamples:\n";
        std::cerr << "  sb_charset_loader /data/mydb.sb --builtin\n";
        std::cerr << "  sb_charset_loader /data/mydb.sb --all\n";
        std::cerr << "  sb_charset_loader /data/mydb.sb --json resources/charsets/charsets.json\n";
        return 1;
    }

    std::string db_path = argv[1];
    std::string mode = (argc >= 3) ? argv[2] : "--builtin";

    // Open database
    ErrorContext ctx;
    Database* db = Database::open(db_path, &ctx);
    if (!db) {
        std::cerr << "Failed to open database: " << ctx.message << "\n";
        return 1;
    }

    CharsetLoader loader(db->getCatalog(), db);
    Status status;

    if (mode == "--builtin") {
        std::cout << "Loading built-in character sets (UTF-8, ASCII, ISO-8859-1, UTF-16, UTF-32)...\n";
        status = loader.loadBuiltinCharsets(&ctx);
    } else if (mode == "--all") {
        std::cout << "Loading all character sets from resources/charsets/charsets.json...\n";
        status = loader.loadFromJSONFile("resources/charsets/charsets.json", &ctx);

        if (status == Status::OK) {
            std::cout << "Loading collations from resources/collations/collations.json...\n";
            status = loader.loadCollationsFromJSONFile("resources/collations/collations.json", &ctx);
        }
    } else if (mode == "--json" && argc >= 4) {
        std::string json_file = argv[3];
        std::cout << "Loading character sets from " << json_file << "...\n";
        status = loader.loadFromJSONFile(json_file, &ctx);
    } else {
        std::cerr << "Invalid option: " << mode << "\n";
        return 1;
    }

    if (status == Status::OK) {
        std::cout << "Character sets loaded successfully!\n";
        return 0;
    } else {
        std::cerr << "Failed: " << ctx.message << "\n";
        return 1;
    }
}
```

---

### Phase 4: Testing (4-5 hours)

```cpp
TEST(CharsetLoader, LoadBuiltinCharsets) {
    Database* db = createTestDatabase();
    CharsetLoader loader(db->getCatalog(), db);

    ErrorContext ctx;
    Status status = loader.loadBuiltinCharsets(&ctx);
    EXPECT_EQ(Status::OK, status);

    // Verify charsets exist
    auto utf8 = db->getCatalog()->getCharset("UTF-8", &ctx);
    EXPECT_TRUE(utf8.has_value());
    EXPECT_EQ(4, utf8->max_bytes);
    EXPECT_EQ(1, utf8->min_bytes);
    EXPECT_TRUE(utf8->is_variable_width);

    auto ascii = db->getCatalog()->getCharset("ASCII", &ctx);
    EXPECT_TRUE(ascii.has_value());
    EXPECT_EQ(1, ascii->max_bytes);
    EXPECT_FALSE(ascii->is_variable_width);
}

TEST(CharsetLoader, LoadCollations) {
    Database* db = createTestDatabase();
    CharsetLoader loader(db->getCatalog(), db);

    ErrorContext ctx;
    loader.loadBuiltinCharsets(&ctx);

    // Verify collations
    auto collation = db->getCatalog()->getCollation("utf8_general_ci", &ctx);
    EXPECT_TRUE(collation.has_value());
    EXPECT_TRUE(collation->case_insensitive);
}
```

---

## EXECUTION TIMELINE

### Week 1 (Agent A - Timezone Loader)
- Days 1-2: TZif parser implementation
- Days 3-4: Catalog integration
- Day 5: CLI tool

### Week 1 (Agent B - Charset Loader)
- Days 1-2: Character set parser
- Days 3-4: Built-in charsets + catalog integration
- Day 5: CLI tool

### Week 2 (Both Agents)
- Days 1-2: Testing and validation
- Day 3: Integration testing
- Days 4-5: Documentation and bug fixes

**Total: 2 weeks with 2 agents in parallel**

---

## COMPLETION CRITERIA

1. ✅ TZif parser can read standard timezone files
2. ✅ Timezone data loads into pg_timezone catalog tables
3. ✅ Character sets load into pg_charsets catalog tables
4. ✅ CLI tools functional (sb_timezone_loader, sb_charset_loader)
5. ✅ Built-in character sets available (UTF-8, ASCII, Latin-1, UTF-16, UTF-32)
6. ✅ Default collations loaded (utf8_general_ci, utf8_bin, etc.)
7. ✅ All unit tests passing
8. ✅ Integration tests verify catalog queries work
9. ✅ Documentation updated with usage examples

---

## USAGE EXAMPLES

### Loading Timezone Data

```bash
# Load timezones from project resources (default - RECOMMENDED)
./sb_timezone_loader /data/mydb.sb

# OR explicitly specify resources directory
./sb_timezone_loader /data/mydb.sb resources/timezones

# OR load from system zoneinfo directory (if available)
./sb_timezone_loader /data/mydb.sb /usr/share/zoneinfo

# Verify in SQL
SELECT tz_name, posix_string FROM pg_timezone ORDER BY tz_name LIMIT 10;
SELECT tz_name, COUNT(*) as transitions
FROM pg_timezone_transitions
GROUP BY tz_name
ORDER BY transitions DESC
LIMIT 10;
```

### Loading Character Sets

```bash
# Load built-in character sets only (UTF-8, ASCII, ISO-8859-1, UTF-16, UTF-32)
./sb_charset_loader /data/mydb.sb --builtin

# Load all character sets from project resources (RECOMMENDED)
./sb_charset_loader /data/mydb.sb --all

# Load from specific JSON file
./sb_charset_loader /data/mydb.sb --json resources/charsets/charsets.json

# Verify in SQL
SELECT charset_name, description, max_bytes, is_variable_width
FROM pg_charsets
ORDER BY charset_name;

SELECT collation_name, charset_name, case_insensitive, accent_insensitive
FROM pg_collations
ORDER BY collation_name;
```

---

## FUTURE ENHANCEMENTS

### Additional Features (Post-Alpha 1)
- Automatic timezone updates (download from IANA)
- Custom collation definitions
- Character set conversion functions
- Locale-specific sorting rules
- Timezone abbreviation resolution

---

**Document Status:** READY FOR IMPLEMENTATION
**Last Updated:** November 23, 2025
