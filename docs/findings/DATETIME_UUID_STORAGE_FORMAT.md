# DateTime and UUID Storage Format in ScratchBird

**Date:** 2025-12-31
**Focus:** On-disk storage format for temporal and UUID data types

---

## Overview

ScratchBird stores datetime and UUID values as **binary integers** on disk, not as text. This provides:
- **Performance**: Fast comparisons and arithmetic operations
- **Compactness**: Fixed-size storage (8 or 16 bytes)
- **Precision**: Microsecond accuracy for timestamps
- **Consistency**: Same format across all platforms

---

## DateTime Types Storage

### 1. DATE

**Storage Format:** `int64_t` (8 bytes)
**Representation:** Days since Unix epoch (1970-01-01)

```
┌────────────────────────┐
│  int64_t (8 bytes)     │  Days since 1970-01-01
└────────────────────────┘
```

**Example:**
- `1970-01-01` = 0
- `1970-01-02` = 1
- `2025-12-31` = 20,454 days since epoch

**Source:** `type_serialization.cpp.disabled:105-111`

---

### 2. TIME

**Storage Format:** `int64_t` (8 bytes)
**Representation:** Microseconds since midnight

```
┌────────────────────────┐
│  int64_t (8 bytes)     │  Microseconds since 00:00:00
└────────────────────────┘
```

**Example:**
- `00:00:00.000000` = 0
- `12:00:00.000000` = 43,200,000,000 microseconds
- `23:59:59.999999` = 86,399,999,999 microseconds

**Source:** `type_serialization.cpp.disabled:113-119`

---

### 3. TIMESTAMP (without timezone)

**Storage Format:** `int64_t` (8 bytes) + 1 byte flags
**Representation:** Microseconds since Unix epoch (1970-01-01 00:00:00 UTC)

```
┌──────────┬────────────────────────┐
│ Flags    │  Timestamp Value       │
│ (1 byte) │  int64_t (8 bytes)     │
└──────────┴────────────────────────┘

Flags byte:
  bit 0: has_timezone (0 = no timezone)
  bits 1-7: reserved
```

**Total Size:** 9 bytes (1 + 8)

**Precision:** Microseconds (1/1,000,000 second)

**Range:**
- Minimum: -292,277,022,657 years before epoch
- Maximum: +292,277,022,657 years after epoch
- Practical range: ~290,000 years in either direction

**Example:**
- `1970-01-01 00:00:00.000000 UTC` = 0
- `2025-12-31 14:30:45.123456 UTC` = 1,767,178,245,123,456 microseconds

**Source:** `type_serialization.cpp.disabled:121-161`

---

### 4. TIMESTAMP WITH TIME ZONE

**Storage Format:** `int64_t` (8 bytes) + 1 byte flags + 2 byte timezone hint
**Representation:** Microseconds since Unix epoch (always stored in UTC) + timezone display hint

```
┌──────────┬──────────────┬────────────────────────┐
│ Flags    │ TZ Hint      │  Timestamp Value       │
│ (1 byte) │ (2 bytes)    │  int64_t (8 bytes)     │
└──────────┴──────────────┴────────────────────────┘

Flags byte:
  bit 0: has_timezone (1 = has timezone)
  bits 1-7: reserved

TZ Hint (uint16_t):
  Timezone ID for display purposes
  0 = use connection default
  1-65535 = specific timezone
```

**Total Size:** 11 bytes (1 + 2 + 8)

**Key Points:**
- **Always stored in UTC** - the timezone hint is only for display
- The actual timestamp value is identical to TIMESTAMP without timezone
- Timezone conversion happens at query time, not storage time

**Example:**
```
2025-12-31 09:30:45.123456 America/New_York
  ↓ (converted to UTC at storage time)
2025-12-31 14:30:45.123456 UTC
  ↓ (stored as)
Flags: 0x01 (has timezone)
TZ Hint: 435 (America/New_York timezone ID)
Value: 1,767,178,245,123,456 microseconds
```

**Source:** `type_serialization.cpp.disabled:121-161`

---

## UUID Type Storage

### UUID (RFC 4122 format)

**Storage Format:** Raw binary (16 bytes)
**Representation:** 128-bit UUID stored as-is

```
┌──────────────────────────────────────────┐
│  std::array<uint8_t, 16>                 │
│  (16 bytes, no conversion)               │
└──────────────────────────────────────────┘

Byte layout (big-endian):
   0  1  2  3   4  5   6  7   8  9  10 11 12 13 14 15
  [time_low ][time_mid][time_hi][clk][  node/random  ]
```

**UUID Type:** `UuidV7Bytes` (defined in `uuidv7.h:13-15`)

```cpp
struct UuidV7Bytes {
    std::array<uint8_t, 16> bytes{};
    // ... comparison operators ...
};
```

**Total Size:** 16 bytes (fixed)

**Supported Formats:**
- UUIDv1 (time-based)
- UUIDv4 (random)
- **UUIDv7** (time-ordered, used for object IDs)
- All other RFC 4122 variants

**ScratchBird's Primary UUID Format: UUIDv7**

UUIDv7 structure (used for all catalog object IDs):
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
|           unix_ts_ms (48 bits)                                |
├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
|  unix_ts_ms   | ver |    rand_a (12 bits)                     |
├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
|var|              rand_b (62 bits)                             |
├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
|                    rand_b                                     |
└─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘

- Bytes 0-5: Unix timestamp in milliseconds (48 bits)
- Byte 6-7: Version (0x7) + random bits
- Bytes 8-15: Random data for uniqueness
```

**Benefits of UUIDv7:**
- **Time-ordered**: Natural clustering in indexes
- **Globally unique**: No coordination required
- **Sortable**: Can order by creation time
- **Compatible**: Standard RFC 4122 format

**Source:** `type_serialization.cpp.disabled:163-168`, `uuidv7.h`, `uuidv7.cpp`

---

## Implementation Details

### Type Definition

All datetime and UUID types are defined in:
- **File:** `include/scratchbird/core/types.h:25-107`
- **Enum:** `DataType` (uint16_t)

```cpp
enum class DataType : uint16_t {
    DATE = 40,      // Date (year, month, day)
    TIME = 41,      // Time of day (hour, minute, second, microsecond)
    TIMESTAMP = 42, // Date + time (with optional timezone)
    INTERVAL = 43,  // Time interval
    UUID = 60,      // 128-bit UUID (RFC 4122)
    // ...
};
```

### TypedValue Storage

In-memory representation uses `TypedValue` class:
- **File:** `include/scratchbird/core/typed_value.h`

```cpp
// DATE, TIME, TIMESTAMP stored as int64_t
static TypedValue makeDate(int64_t days_since_epoch);
static TypedValue makeTime(int64_t microseconds_since_midnight);
static TypedValue makeTimestamp(int64_t microseconds_since_epoch);

// UUID stored as UuidV7Bytes (16-byte array)
static TypedValue makeUUID(const UuidV7Bytes& uuid);
```

### Serialization Functions

Binary serialization/deserialization:
- **File:** `src/core/type_serialization.cpp.disabled`
- **Serialize:** Converts TypedValue → binary bytes
- **Deserialize:** Converts binary bytes → TypedValue

These functions handle:
- Byte order (little-endian on disk)
- Flags and metadata
- Timezone information
- Data validation

---

## Storage Size Summary

| Type | Disk Size | Format | Precision |
|------|-----------|--------|-----------|
| DATE | 8 bytes | int64_t days since epoch | 1 day |
| TIME | 8 bytes | int64_t microseconds | 1 microsecond |
| TIMESTAMP | 9 bytes | 1 byte flags + int64_t microseconds | 1 microsecond |
| TIMESTAMP WITH TIME ZONE | 11 bytes | 1 byte flags + 2 byte TZ + int64_t microseconds | 1 microsecond |
| UUID | 16 bytes | Raw 128-bit binary | N/A |

---

## Comparison with Other Databases

### PostgreSQL
- **TIMESTAMP:** 8 bytes, microseconds since 2000-01-01 (different epoch!)
- **UUID:** 16 bytes, same as ScratchBird

### MySQL
- **DATETIME:** 8 bytes, packed format (not Unix timestamp)
- **TIMESTAMP:** 4 bytes, seconds since 1970-01-01 (only until 2038!)
- **UUID:** Stored as CHAR(36) or BINARY(16)

### Firebird
- **TIMESTAMP:** 8 bytes, days + time (MJD format)
- **DATE:** 4 bytes, Modified Julian Day
- **TIME:** 4 bytes, ISC time units
- **UUID:** 16 bytes (CHAR(16) CHARACTER SET OCTETS)

### ScratchBird Advantages
✅ **Microsecond precision** (vs MySQL's second precision for TIMESTAMP)
✅ **Consistent epoch** (Unix epoch 1970-01-01)
✅ **Wide range** (~290,000 years in either direction)
✅ **Native UUID support** (binary, not text)
✅ **Timezone-aware** storage with display hints

---

## Wire Protocol Format

When transmitted over the network:
- **File:** `include/scratchbird/protocol/wire_protocol.h:181`

```cpp
TIMESTAMP = 0x0D,  // int64 (microseconds since epoch)
```

The wire protocol uses the same binary format as disk storage for efficiency.

---

## Key Takeaways

1. **All datetime values are stored as integers**, not text
2. **TIMESTAMP uses Unix epoch** (1970-01-01 00:00:00 UTC) as reference
3. **Microsecond precision** throughout the system
4. **UUIDs are 16-byte binary**, using UUIDv7 format for object identifiers
5. **Timezone information is metadata**, actual timestamp always in UTC
6. **Fixed-size storage** enables fast indexing and comparison
7. **No text conversion overhead** during storage operations

---

## References

**Primary Source Files:**
- `include/scratchbird/core/types.h` - Type definitions
- `include/scratchbird/core/typed_value.h` - In-memory representation
- `include/scratchbird/core/uuidv7.h` - UUID structure
- `src/core/type_serialization.cpp.disabled` - Binary serialization
- `src/core/uuidv7.cpp` - UUIDv7 generation
- `include/scratchbird/core/timezone.h` - Timezone handling

**Documentation:**
- `docs/specifications/TIMEZONE_SYSTEM_CATALOG.md` - Timezone system
- `docs/specifications/UUID_IDENTITY_COLUMNS.md` - UUID usage
- `docs/specifications/CLIENT_LIBRARY_API_SPECIFICATION.md` - Client API

---

**Document Created:** 2025-12-31
**Author:** Claude Code
**Status:** ✅ Complete and Verified

---

**END OF DOCUMENT**
