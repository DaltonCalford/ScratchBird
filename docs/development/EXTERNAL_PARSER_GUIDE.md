# External Parser/Client Implementation Guide

**Version**: 1.0
**Last Updated**: 2025-10-04
**Audience**: External parser and client library developers

## Overview

This guide provides essential information for implementing external parsers and client libraries that connect to the ScratchBird database engine.

## Core Specifications

### Required Reading

1. **[TIMEZONE_SYSTEM_CATALOG.md](../specifications/TIMEZONE_SYSTEM_CATALOG.md)**
   - Timezone system catalog structure (`pg_timezone`)
   - GMT storage architecture
   - Timezone-aware timestamp parsing/formatting
   - DST rule handling
   - `AT TIME ZONE` operator implementation

2. **[character_sets_and_collations.md](../specifications/character_sets_and_collations.md)**
   - Character set support (UTF-8, Latin1, ASCII, etc.)
   - Collation system (15 collations)
   - String comparison and sorting
   - Character set conversion

3. **[POSTGRESQL_PARSER_IMPLEMENTATION.md](../specifications/POSTGRESQL_PARSER_IMPLEMENTATION.md)**
   - PostgreSQL compatibility layer
   - System catalog emulation
   - Type mapping
   - Character set and timezone integration

## Quick Start Checklist

### 1. Connection Initialization

```cpp
// On connection establishment:
□ Query database charset: SELECT pg_catalog.pg_database.encoding
□ Load timezone catalog: SELECT * FROM pg_timezone
□ Get connection timezone: Use Database::getConnectionTimezone()
□ Set client timezone: Database::setConnectionTimezone(tz_id)
□ Cache timezone definitions in-memory
```

### 2. Timestamp Handling

**Parsing** (Client → Database):
```cpp
// Input: "2025-10-04 10:30:00-05:00" (EST)
// Output: int64_t GMT microseconds

auto gmt_ts = timezone_manager.parseTimestamp(
    user_input,
    connection_default_timezone,
    &error_context
);
// Result: 1728055800000000 (GMT)
```

**Formatting** (Database → Client):
```cpp
// Input: int64_t GMT microseconds
// Output: "2025-10-04 10:30:00-05:00"

uint16_t display_tz = column.timezone_hint != 0
    ? column.timezone_hint
    : connection_timezone;

std::string formatted = timezone_manager.formatTimestamp(
    gmt_microseconds,
    display_tz,
    true  // include offset
);
```

### 3. Character Set Handling

**Conversion** (Client → Database):
```cpp
// Convert client encoding to database encoding
Status status = charset_manager.convert(
    client_data, client_data_length,
    client_charset,        // e.g., CharacterSet::LATIN1
    output_buffer,
    database_charset,      // e.g., CharacterSet::UTF8
    &error_context
);
```

**String Operations**:
```cpp
// Get character count (not byte count)
uint32_t char_count = charset_manager.getCharLength(
    string_data, byte_length, CharacterSet::UTF8
);

// Collation-aware comparison
int result = charset_manager.compare(
    str1_data, str1_length,
    str2_data, str2_length,
    collation_id  // e.g., 101 = utf8_general_ci
);
```

## System Catalog Tables

### pg_timezone

**Structure**:
```sql
CREATE TABLE pg_timezone (
    timezone_id SMALLINT PRIMARY KEY,
    name VARCHAR(64),
    abbreviation VARCHAR(16),
    std_offset_minutes INTEGER,
    observes_dst BOOLEAN,
    dst_start_month TINYINT,
    dst_start_week TINYINT,
    dst_start_day TINYINT,
    dst_start_hour TINYINT,
    dst_end_month TINYINT,
    dst_end_week TINYINT,
    dst_end_day TINYINT,
    dst_end_hour TINYINT,
    dst_offset_minutes INTEGER,
    created_time TIMESTAMP,
    last_modified_time TIMESTAMP
);
```

**Query Examples**:
```sql
-- Get all timezones
SELECT * FROM pg_timezone WHERE is_valid = 1;

-- Get timezone by name
SELECT * FROM pg_timezone WHERE name = 'America/New_York';

-- Get timezones with DST
SELECT * FROM pg_timezone WHERE observes_dst = true;
```

### CRUD Operations (C++ API)

```cpp
// Create timezone
CatalogManager::TimezoneInfo tz;
tz.timezone_id = 10;
tz.name = "Europe/London";
tz.abbreviation = "GMT";
tz.std_offset_minutes = 0;
tz.observes_dst = true;
// ... set DST rules
catalog_manager->createTimezone(tz, &ctx);

// Update timezone
tz.std_offset_minutes = 60;  // Political change
catalog_manager->updateTimezone(10, tz, &ctx);

// Get timezone
CatalogManager::TimezoneInfo info;
catalog_manager->getTimezone(10, info, &ctx);
catalog_manager->getTimezoneByName("Europe/London", info, &ctx);

// List all
std::vector<CatalogManager::TimezoneInfo> all_timezones;
catalog_manager->listTimezones(all_timezones, &ctx);

// Delete (soft delete)
catalog_manager->deleteTimezone(10, &ctx);
```

## Type Metadata

### Column Metadata

When querying column information, check for timezone and charset metadata:

```cpp
struct ColumnInfo {
    // ... other fields ...
    uint16_t data_type;          // DataType::TIMESTAMP = 42
    bool with_timezone;          // TIMESTAMP WITH TIME ZONE flag
    uint16_t timezone_hint;      // Display timezone ID (0 = use connection)
    uint16_t charset;            // CharacterSet enum
    uint32_t collation_id;       // Collation ID
};
```

### Wire Protocol Encoding

**Timestamp**:
```
Type: TIMESTAMP (42)
Storage: int64_t (8 bytes, little-endian)
Value: Microseconds since Unix epoch (GMT)
Metadata:
  - with_timezone: bool (1 byte)
  - timezone_hint: uint16_t (2 bytes)
```

**String**:
```
Type: VARCHAR (21)
Storage: Variable-length byte array
Metadata:
  - charset: uint16_t (2 bytes)
  - collation_id: uint32_t (4 bytes)
  - max_length: uint32_t (4 bytes)
```

## SQL Command Support

### Timezone Commands (To Be Implemented)

```sql
-- Create timezone
CREATE TIMEZONE 'Custom/Zone'
    ABBREVIATION 'CZ'
    OFFSET +05:30
    DST START LAST SUNDAY OF MARCH AT 01:00
    DST END LAST SUNDAY OF OCTOBER AT 02:00
    DST OFFSET +01:00;

-- Alter timezone
ALTER TIMEZONE 'America/New_York'
    SET DST START SECOND SUNDAY OF MARCH AT 02:00;

-- Drop timezone
DROP TIMEZONE 'Custom/Zone';

-- Query with timezone conversion
SELECT
    event_time,
    event_time AT TIME ZONE 'America/New_York' AS eastern_time,
    event_time AT TIME ZONE 3 AS pacific_time
FROM events;
```

### Character Set Commands

```sql
-- Set connection charset
SET client_encoding = 'UTF8';

-- Convert string
SELECT CONVERT('café' USING utf8);

-- Apply collation
SELECT name FROM users ORDER BY name COLLATE utf8_general_ci;

-- Column with charset
CREATE TABLE messages (
    id INT,
    content VARCHAR(500) CHARACTER SET utf8 COLLATE utf8_general_ci
);
```

## Common Implementation Patterns

### Pattern 1: Connection Timezone Management

```cpp
class DatabaseConnection {
    Database* db_;
    uint16_t connection_timezone_;
    TimezoneManager tz_manager_;

public:
    void connect(const std::string& connstring) {
        // Open database
        db_->open(path, &ctx);

        // Load timezone catalog
        std::vector<CatalogManager::TimezoneInfo> timezones;
        db_->catalog_manager()->listTimezones(timezones, &ctx);

        // Initialize timezone manager (future: load from catalog)
        // tz_manager_.loadFromCatalog(db_, &ctx);

        // Set default from connection string or database default
        connection_timezone_ = db_->getDatabaseTimezone();
        if (connstring.find("timezone=") != std::string::npos) {
            // Parse and set connection timezone
        }
    }

    void setTimezone(const std::string& tz_name) {
        CatalogManager::TimezoneInfo info;
        auto status = db_->catalog_manager()->getTimezoneByName(tz_name, info, &ctx);
        if (status == Status::OK) {
            connection_timezone_ = info.timezone_id;
            db_->setConnectionTimezone(info.timezone_id);
        }
    }
};
```

### Pattern 2: Timestamp Result Processing

```cpp
class ResultSet {
    void processTimestampColumn(int col_index) {
        auto& col_info = columns_[col_index];

        // Read GMT timestamp from wire
        int64_t gmt_microseconds = readInt64FromWire();

        // Determine display timezone
        uint16_t display_tz = col_info.timezone_hint != 0
            ? col_info.timezone_hint
            : connection_->getConnectionTimezone();

        // Format for client
        std::string formatted = tz_manager_.formatTimestamp(
            gmt_microseconds,
            display_tz,
            col_info.with_timezone  // show offset if WITH TIME ZONE
        );

        // Return to client
        return formatted;
    }
};
```

### Pattern 3: Prepared Statement Parameter Binding

```cpp
void bindTimestampParameter(int param_index, const std::string& timestamp_str) {
    // Parse with connection timezone as default
    auto gmt_ts = tz_manager_.parseTimestamp(
        timestamp_str,
        connection_timezone_,
        &error_ctx
    );

    if (!gmt_ts) {
        throw SQLException("Invalid timestamp format");
    }

    // Send as GMT microseconds to database
    writeInt64ToWire(param_index, *gmt_ts);
}
```

## Testing Requirements

### Timezone Tests

- [ ] Parse timestamp with explicit UTC offset
- [ ] Parse timestamp with named timezone
- [ ] Parse timestamp without timezone (use connection default)
- [ ] Format timestamp in different timezones
- [ ] Handle DST transitions correctly
- [ ] Test across DST boundaries
- [ ] Verify GMT storage (comparisons work correctly)
- [ ] Test microsecond precision preservation
- [ ] Test AT TIME ZONE operator
- [ ] Test timezone cache updates

### Character Set Tests

- [ ] UTF-8 multibyte character handling
- [ ] Character set conversion (UTF-8 ↔ Latin1)
- [ ] Lossy conversion with unmappable characters
- [ ] Character count vs byte count
- [ ] Collation-aware sorting
- [ ] Case-insensitive comparisons
- [ ] VARCHAR length limits (character vs byte)

## Performance Best Practices

### 1. Timezone Caching
```cpp
// Cache all timezones at connection time
std::unordered_map<uint16_t, TimezoneInfo> timezone_cache_;
std::unordered_map<std::string, uint16_t> name_to_id_;

void loadTimezoneCache(Database* db) {
    std::vector<CatalogManager::TimezoneInfo> timezones;
    db->catalog_manager()->listTimezones(timezones, &ctx);

    for (const auto& tz : timezones) {
        timezone_cache_[tz.timezone_id] = tz;
        name_to_id_[tz.name] = tz.timezone_id;
    }
}
```

### 2. Avoid Redundant Conversions
```cpp
// BAD: Convert on every row
for (auto& row : results) {
    auto gmt = toGMT(row.timestamp, tz);  // Repeated conversion
}

// GOOD: Convert once at presentation layer
std::vector<int64_t> gmt_timestamps = fetchAllFromDatabase();
// Timestamps already in GMT, no conversion needed
```

### 3. Batch Timezone Lookups
```cpp
// BAD: One query per timezone
for (auto& event : events) {
    auto tz = catalog->getTimezone(event.tz_id);
}

// GOOD: Load all at once
auto all_timezones = catalog->listTimezones();
std::unordered_map<uint16_t, TimezoneInfo> tz_map;
for (auto& tz : all_timezones) {
    tz_map[tz.timezone_id] = tz;
}
```

## Error Handling

### Common Errors

1. **Invalid timezone offset**: `Status::INVALID_ARGUMENT`
   ```
   Error: Timezone offset out of range (-12:00 to +14:00)
   ```

2. **Timezone not found**: `Status::NOT_FOUND`
   ```
   Error: Timezone 'Invalid/Zone' not found in pg_timezone
   ```

3. **Invalid timestamp format**: `Status::INVALID_ARGUMENT`
   ```
   Error: Cannot parse timestamp '2025-13-01 00:00:00' (invalid month)
   ```

4. **Character set conversion error**: `Status::INVALID_ARGUMENT`
   ```
   Error: Cannot convert character U+1F600 to Latin1 (unmappable)
   ```

### Error Context Usage

```cpp
ErrorContext ctx;
auto result = tz_manager.parseTimestamp(input, default_tz, &ctx);
if (!result) {
    // ctx.status contains error code
    // ctx.message contains error description
    throw DatabaseException(ctx.message);
}
```

## Future Enhancements

### IANA tzdata Integration
- Import timezone data from IANA database
- Support historical timezone changes
- Store complex DST rules in TOAST

### Advanced Collations
- ICU collation support
- Locale-specific sorting
- Custom collation definitions

## Resources

- [ScratchBird SQL Grammar](../specifications/SCRATCHBIRD_SQL_COMPLETE_BNF.md)
- [SBLR Bytecode Specification](../specifications/Appendix_A_SBLR_BYTECODE.md)
- [Wire Protocol Specifications](../specifications/WIRE_PROTOCOL_SPECIFICATIONS.md)
- [PostgreSQL Compatibility](../specifications/postgresql_spec.md)

## Support

For questions or issues with external parser implementation:
- Review the complete specification documents
- Check the reference implementation in `src/parser/`
- See test cases in `tests/unit/test_timezone.cpp` and `tests/unit/test_charset.cpp`
