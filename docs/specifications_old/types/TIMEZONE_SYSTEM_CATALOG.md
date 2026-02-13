# Timezone System Catalog Specification

**Version**: 1.0
**Status**: Implemented
**Last Updated**: 2025-10-04

## Overview

ScratchBird implements a comprehensive timezone system with a persistent, updatable system catalog (`pg_timezone`). All timestamp values are stored internally in GMT/UTC, with timezone information used only for input parsing and output formatting.

## Architecture Principles

### 1. **GMT Storage**
- All `TIMESTAMP` values are stored as **microseconds since Unix epoch in GMT**
- No timezone conversion happens during storage
- Comparisons are always in GMT (no conversion overhead)
- Original input offset seconds is preserved in the payload for round-trip formatting

### 2. **Display Hints**
- Column-level `timezone_hint` specifies display timezone
- Connection-level timezone provides default
- Explicit `AT TIME ZONE` operator for ad-hoc conversion

### 3. **Persistent Configuration**
- Timezone definitions stored in system catalog
- DST rules are updatable without recompilation
- Supports political timezone changes

## System Catalog Structure

### Table: `pg_timezone`

Located at `CatalogRootPage.timezones_page`

#### On-Disk Structure (`TimezoneRecord`)

```cpp
struct TimezoneRecord
{
    uint16_t timezone_id;       // Unique timezone ID (1-65535)
    char name[64];              // Full name (e.g., "America/New_York")
    char abbreviation[16];      // Short code (e.g., "EST", "PST")
    int32_t std_offset_minutes; // Standard offset from GMT (-720 to +840 minutes)
    uint8_t observes_dst;       // 1 = observes DST, 0 = no DST
    uint8_t reserved1;
    uint16_t reserved2;

    // DST Transition Rules (simplified)
    uint8_t dst_start_month;    // Month DST starts (1-12, 0 = no DST)
    uint8_t dst_start_week;     // Week of month (1-5, 0 = last week)
    uint8_t dst_start_day;      // Day of week (0-6, 0 = Sunday)
    uint8_t dst_start_hour;     // Hour DST starts (0-23)
    uint8_t dst_end_month;      // Month DST ends (1-12)
    uint8_t dst_end_week;       // Week of month (1-5, 0 = last)
    uint8_t dst_end_day;        // Day of week (0-6, 0 = Sunday)
    uint8_t dst_end_hour;       // Hour DST ends (0-23)
    int32_t dst_offset_minutes; // Additional offset during DST (typically 60)

    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;          // 1 = active, 0 = deleted
    uint32_t padding;           // Alignment
};
```

**Total Size**: ~144 bytes per record

#### Logical Structure (`CatalogManager::TimezoneInfo`)

```cpp
struct TimezoneInfo
{
    uint16_t timezone_id;
    std::string name;
    std::string abbreviation;
    int32_t std_offset_minutes;
    bool observes_dst;
    uint8_t dst_start_month;
    uint8_t dst_start_week;
    uint8_t dst_start_day;
    uint8_t dst_start_hour;
    uint8_t dst_end_month;
    uint8_t dst_end_week;
    uint8_t dst_end_day;
    uint8_t dst_end_hour;
    int32_t dst_offset_minutes;
    uint64_t created_time;
    uint64_t last_modified_time;
};
```

## CRUD Operations

### CatalogManager API

#### Create Timezone
```cpp
auto createTimezone(const TimezoneInfo &tz_info, ErrorContext *ctx = nullptr) -> Status;
```

#### Update Timezone
```cpp
auto updateTimezone(uint16_t timezone_id, const TimezoneInfo &tz_info,
                   ErrorContext *ctx = nullptr) -> Status;
```

#### Get Timezone by ID
```cpp
auto getTimezone(uint16_t timezone_id, TimezoneInfo &info,
                ErrorContext *ctx = nullptr) -> Status;
```

#### Get Timezone by Name
```cpp
auto getTimezoneByName(const std::string &name, TimezoneInfo &info,
                      ErrorContext *ctx = nullptr) -> Status;
```

#### List All Timezones
```cpp
auto listTimezones(std::vector<TimezoneInfo> &timezones,
                  ErrorContext *ctx = nullptr) -> Status;
```

#### Delete Timezone (Soft Delete)
```cpp
auto deleteTimezone(uint16_t timezone_id, ErrorContext *ctx = nullptr) -> Status;
```

## Predefined Timezone IDs

| ID | Name | Abbreviation | Offset | DST |
|----|------|--------------|--------|-----|
| 1 | UTC/GMT | UTC | +00:00 | No |
| 2 | America/New_York | EST/EDT | -05:00 | Yes |
| 3 | America/Los_Angeles | PST/PDT | -08:00 | Yes |
| 4 | America/Chicago | CST/CDT | -06:00 | Yes |
| 5 | America/Denver | MST/MDT | -07:00 | Yes |

## SQL Interface (Future Implementation)

### DDL Commands

#### CREATE TIMEZONE
```sql
CREATE TIMEZONE 'Europe/London'
    ABBREVIATION 'GMT'
    OFFSET 0
    DST START LAST SUNDAY OF MARCH AT 01:00
    DST END LAST SUNDAY OF OCTOBER AT 02:00
    DST OFFSET +01:00;
```

#### ALTER TIMEZONE
```sql
ALTER TIMEZONE 'Europe/London'
    SET OFFSET +00:00;  -- Political change

ALTER TIMEZONE 'America/New_York'
    SET DST START SECOND SUNDAY OF MARCH AT 02:00
    SET DST END FIRST SUNDAY OF NOVEMBER AT 02:00;
```

#### DROP TIMEZONE
```sql
DROP TIMEZONE 'Custom/Zone';
```

### Query Syntax

#### List Timezones
```sql
SELECT * FROM pg_timezone;
SELECT * FROM pg_timezone WHERE observes_dst = true;
```

#### Use in Column Definitions
```sql
CREATE TABLE events (
    id INT,
    event_time TIMESTAMP WITH TIME ZONE,
    timezone_hint SMALLINT DEFAULT 1  -- 1 = UTC
);
```

#### AT TIME ZONE Operator
```sql
-- Convert to specific timezone
SELECT event_time AT TIME ZONE 'America/New_York' FROM events;
SELECT event_time AT TIME ZONE 3 FROM events;  -- By ID

-- Display with different timezone
SELECT
    event_time,
    event_time AT TIME ZONE 2 AS eastern_time,
    event_time AT TIME ZONE 3 AS pacific_time
FROM events;
```

## Type System Integration

### TypeInfo Structure
```cpp
struct TypeInfo
{
    DataType type;
    uint32_t precision;
    uint32_t scale;
    DataType element_type;
    bool with_timezone;      // For TIMESTAMP WITH TIME ZONE
    uint16_t timezone_hint;  // Display timezone ID (0 = use connection default)
};
```

### Column Metadata
```cpp
struct ColumnInfo
{
    // ... other fields ...
    bool with_timezone;      // TIMESTAMP WITH TIME ZONE flag
    uint16_t timezone_hint;  // Display timezone ID
};
```

### Database Connection Context
```cpp
class Database
{
    uint16_t getConnectionTimezone() const;
    void setConnectionTimezone(uint16_t tz_id);
    uint16_t getDatabaseTimezone() const;
};
```

## TimezoneManager Integration

### Loading from Catalog

**Current Implementation**: Hardcoded timezones in `TimezoneManager`

**Future Enhancement**:
```cpp
class TimezoneManager
{
public:
    // Load timezones from database catalog
    auto loadFromCatalog(Database *db, ErrorContext *ctx = nullptr) -> Status;

    // Reload to pick up changes
    auto reloadFromCatalog(Database *db, ErrorContext *ctx = nullptr) -> Status;

    // Check if timezone exists
    auto hasTimezone(uint16_t timezone_id) const -> bool;
};
```

### Timezone Data Ingestion and Updates
ScratchBird uses IANA tzdata as the canonical timezone source. Timezone records
must be loaded into `pg_timezone` using the loader tool and the catalog should
record the tzdata version used for traceability.

**Loader tool (required):**
- `sb_timezone_loader` loads TZif files from a zoneinfo directory or a single file.
- Tool options: `--from <dir>`, `--file <path>`, `--stats` (per
  `ScratchBird/tools/sb_timezone_loader.cpp`).

**Version tracking (required):**
- Store the IANA tzdata version (e.g., `2024b`) in a small metadata record
  (current: dedicated catalog row in `pg_timezone` with `timezone_id=0`,
  `name='TZDATA_VERSION'`, version stored in `abbreviation`).
- Store the i18n resource bundle version in a separate metadata record
  (`timezone_id=65535`, `name='I18N_RESOURCE_VERSION'`), sourced from
  `resources/i18n/version`.

**Update workflow (expected):**
1. Replace tzdata in `resources/timezones/` (or use system `/usr/share/zoneinfo`).
2. Run `sb_timezone_loader` to refresh `pg_timezone`.
3. Update the recorded tzdata version in the catalog metadata.

### Implementation Pattern
```cpp
// During database open
TimezoneManager tz_mgr;
tz_mgr.loadFromCatalog(db, &ctx);

// During query execution
uint16_t conn_tz = db->getConnectionTimezone();
std::string formatted = tz_mgr.formatTimestamp(gmt_ts, conn_tz, true);
```

## DST Calculation Rules

### Transition Format
- **Week**: 1-5 (1=first, 2=second, ... 5=fifth/last), 0=last week of month
- **Day**: 0-6 (0=Sunday, 1=Monday, ..., 6=Saturday)
- **Hour**: 0-23 (local time)

### Examples

#### US Eastern Time (2007+ rules)
```
DST Start: Second Sunday of March at 02:00
  - dst_start_month = 3
  - dst_start_week = 2
  - dst_start_day = 0 (Sunday)
  - dst_start_hour = 2

DST End: First Sunday of November at 02:00
  - dst_end_month = 11
  - dst_end_week = 1
  - dst_end_day = 0 (Sunday)
  - dst_end_hour = 2
  - dst_offset_minutes = 60
```

#### European Summer Time
```
DST Start: Last Sunday of March at 01:00
  - dst_start_month = 3
  - dst_start_week = 0 (last)
  - dst_start_day = 0 (Sunday)
  - dst_start_hour = 1

DST End: Last Sunday of October at 02:00
  - dst_end_month = 10
  - dst_end_week = 0 (last)
  - dst_end_day = 0 (Sunday)
  - dst_end_hour = 2
  - dst_offset_minutes = 60
```

## Client/Parser Implementation Notes

### For External Parser/Client Developers

#### 1. **Initialization**
- Query `pg_timezone` system catalog on connection
- Cache timezone definitions locally
- Respect connection's default timezone

#### 2. **Timestamp Parsing**
```
Input: "2025-10-04 15:30:00-05:00"
Steps:
  1. Parse datetime components
  2. Parse timezone offset or name
  3. Convert to GMT: local_time - offset
  4. Store as int64_t microseconds since epoch (UTC)
  5. Store original offset seconds for round-trip display
```

#### 3. **Timestamp Formatting**
```
Input: GMT microseconds (e.g., 1728054600000000) + stored offset seconds
Steps:
  1. Get display timezone from column hint or connection
  2. Look up timezone offset from catalog
  3. Apply offset: gmt_time + offset
  4. Format with timezone indicator
Output: "2025-10-04 10:30:00-05:00"
```

#### 4. **AT TIME ZONE Implementation**
```sql
SELECT timestamp_col AT TIME ZONE 'America/New_York';
```

**Execution**:
1. Fetch GMT timestamp from storage
2. Look up target timezone from catalog
3. Calculate offset (including DST if applicable)
4. Return formatted string

#### 5. **Timezone Cache Invalidation**
- Watch for `ALTER TIMEZONE` / `CREATE TIMEZONE` commands
- Reload timezone cache when catalog changes
- Consider TTL-based cache refresh

### Wire Protocol Considerations

#### Timestamp Encoding
```
Type: TIMESTAMP
Encoding: int64_t (8 bytes, little-endian) + int32_t offset_seconds (4 bytes)
Value: Microseconds since Unix epoch (UTC), plus original input offset seconds
Metadata: with_timezone flag, timezone_hint (uint16_t)
```

#### Timezone Metadata
```
Message Type: ColumnMetadata
Fields:
  - column_type: TIMESTAMP (42)
  - with_timezone: bool (1 byte)
  - timezone_hint: uint16_t (2 bytes)
  - ... other fields
```

## Migration and Compatibility

### Database Initialization
```cpp
// During CREATE DATABASE
void initializeTimezones(CatalogManager *catalog)
{
    // Create UTC (required)
    TimezoneInfo utc;
    utc.timezone_id = 1;
    utc.name = "UTC";
    utc.abbreviation = "UTC";
    utc.std_offset_minutes = 0;
    utc.observes_dst = false;
    catalog->createTimezone(utc, &ctx);

    // Create common timezones
    // ... (EST, PST, CST, MST, etc.)
}
```

### Upgrading Existing Databases
```sql
-- Check if timezones table exists
SELECT COUNT(*) FROM pg_catalog WHERE table_name = 'pg_timezone';

-- If not, run migration
INSERT INTO pg_timezone VALUES (1, 'UTC', 'UTC', 0, 0, ...);
-- ... populate standard timezones
```

## Performance Considerations

### Indexing
- Primary key on `timezone_id`
- Index on `name` for lookup by name
- Small table size (~100-500 records typical)

### Caching Strategy
- Load all timezones at connection time
- In-memory hash map: `timezone_id -> TimezoneInfo`
- Refresh on `ALTER TIMEZONE` notification

### Comparison Optimization
- All comparisons happen in GMT
- No conversion needed for WHERE clauses
- Conversion only at presentation layer

## IANA tzdata Integration (Future)

For production systems, integrate with IANA timezone database:

### Data Import
```cpp
// Import from tzdata
auto importIANATimezone(const std::string &iana_name,
                       CatalogManager *catalog,
                       ErrorContext *ctx) -> Status;
```

### Rule Storage
- Store complex DST rules in TOAST
- Reference from `TimezoneRecord.index_params_oid`
- Support historical timezone changes

### Example Structure
```cpp
struct IANATimezoneRule
{
    int32_t year_from;
    int32_t year_to;
    // Detailed transition rules per year range
    std::vector<DSTTransition> transitions;
};
```

## Testing Checklist

For external parser/client implementers:

- [ ] Parse timestamps with explicit UTC offset
- [ ] Parse timestamps with named timezone
- [ ] Parse timestamps without timezone (use connection default)
- [ ] Format timestamps in different timezones
- [ ] Handle DST transitions correctly
- [ ] Support AT TIME ZONE operator
- [ ] Cache timezone definitions
- [ ] Handle timezone catalog updates
- [ ] Test across DST boundaries
- [ ] Test with historical dates (pre-2007 US DST rules)
- [ ] Verify GMT storage (all comparisons in GMT)
- [ ] Test microsecond precision preservation

## Summary

The ScratchBird timezone system provides:

✅ **Persistent Configuration**: Timezone rules stored in system catalog
✅ **Updatable**: DST rules can change without code recompilation
✅ **Standard Compliance**: Follows SQL standard for TIMESTAMP WITH TIME ZONE
✅ **Performance**: All comparisons in GMT (no conversion overhead)
✅ **Extensibility**: Easy to add new timezones or update rules
✅ **Client-Friendly**: Clear API and wire protocol integration points

For external parser/client development, follow the patterns documented in this specification to ensure correct timezone handling and compatibility with the ScratchBird core engine.
