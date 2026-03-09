# Specification: Scalar Types

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | types / catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.3.0 |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/types.h:46`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2641`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:121`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/scratchbird/tests/datatypes/001_numeric_integer_types.sql`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/scratchbird/tests/datatypes/005_binary_types.sql`

## Synopsis

This specification defines the canonical scalar types supported by ScratchBird, including their on-disk binary representations, in-memory layouts, and storage characteristics. Scalar types form the foundation of the type system and are used directly or as building blocks for complex types and domains.

## Scope

### In Scope

- Integer types (INT8, INT16, INT32, INT64, INT128, UINT8, UINT16, UINT32, UINT64, UINT128)
- Floating-point types (FLOAT32, FLOAT64)
- Decimal types (DECIMAL, DECFLOAT16, DECFLOAT34, MONEY)
- String types (CHAR, VARCHAR, TEXT)
- Binary types (BINARY, VARBINARY, BLOB, BYTEA)
- Boolean type (BOOLEAN)
- Temporal types (DATE, TIME, TIMESTAMP, TIMESTAMP_WITH_ZONE, TIME_WITH_ZONE, INTERVAL, YEAR)
- Network types (INET, CIDR, MACADDR, MACADDR8)
- UUID type (UUID)
- Bit string type (BIT)

### Out of Scope

- Complex/container types (ARRAY, COMPOSITE, LIST, MAP) - see [complex_types.md](./complex_types.md)
- JSON/XML types - see [complex_types.md](./complex_types.md)
- Spatial types - see [complex_types.md](./complex_types.md)
- Emulated engine-specific type mappings - see emulation specifications

## Background

ScratchBird uses a unified type system where all data types are defined in a single canonical enum (`DataType`). The type system supports both native storage and emulated engine compatibility. Scalar types are stored with fixed-width or length-prefixed variable-width binary representations, using little-endian byte order for numeric fields.

## Specification

### DataType Enum Definition

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/types.h:46
enum class DataType : uint16_t
{
    UNKNOWN = 0,
    
    // Numeric types (1-19)
    INT8 = 1,        // 1-byte signed integer
    INT16 = 2,       // 2-byte signed integer
    INT32 = 3,       // 4-byte signed integer
    INT64 = 4,       // 8-byte signed integer
    INT128 = 5,      // 16-byte signed integer
    UINT8 = 6,       // 1-byte unsigned integer
    UINT16 = 7,      // 2-byte unsigned integer
    UINT32 = 8,      // 4-byte unsigned integer
    UINT64 = 9,      // 8-byte unsigned integer
    FLOAT32 = 10,    // 4-byte IEEE 754 float
    FLOAT64 = 11,    // 8-byte IEEE 754 double
    DECIMAL = 12,    // Fixed-precision decimal
    MONEY = 13,      // Fixed-precision currency (int64 scaled)
    UINT128 = 14,    // 16-byte unsigned integer
    DECFLOAT16 = 15, // IEEE-754 decimal floating (Decimal64)
    DECFLOAT34 = 16, // IEEE-754 decimal floating (Decimal128)
    
    // String types (20-29)
    CHAR = 20,       // Fixed-length string
    VARCHAR = 21,    // Variable-length string
    TEXT = 22,       // Unlimited variable-length string
    
    // Binary types (30-39)
    BINARY = 30,     // Fixed-length binary data
    VARBINARY = 31,  // Variable-length binary data
    BLOB = 32,       // Binary large object
    BYTEA = 33,      // PostgreSQL-style binary data
    
    // Date/Time types (40-49)
    DATE = 40,       // Date (Modified Julian Day)
    TIME = 41,       // Time of day (microseconds since midnight)
    TIMESTAMP = 42,  // Date + time (microseconds since Unix epoch)
    TIMESTAMP_WITH_ZONE = 43, // Timestamp with timezone
    TIME_WITH_ZONE = 44,      // Time with timezone
    INTERVAL = 45,   // Time interval
    YEAR = 47,       // Year type (INT16 storage)
    
    // Boolean (50-59)
    BOOLEAN = 50,    // True/false
    BIT = 51,        // Bit string type
    
    // Special types (60-69)
    UUID = 60,       // 128-bit UUID (RFC 4122)
    
    // Network types (98-101)
    INET = 98,       // IPv4 or IPv6 address
    CIDR = 99,       // IPv4 or IPv6 network
    MACADDR = 100,   // 6-byte MAC address (EUI-48)
    MACADDR8 = 101,  // 8-byte MAC address (EUI-64)
    
    NULL_TYPE = 255, // SQL NULL
};
```

### Integer Types

#### Binary Representation

| Type | Storage | Range | Byte Order |
|------|---------|-------|------------|
| INT8 | 1 byte | -128 to 127 | N/A |
| INT16 | 2 bytes | -32,768 to 32,767 | Little-endian |
| INT32 | 4 bytes | -2^31 to 2^31-1 | Little-endian |
| INT64 | 8 bytes | -2^63 to 2^63-1 | Little-endian |
| INT128 | 16 bytes | -2^127 to 2^127-1 | Little-endian |
| UINT8 | 1 byte | 0 to 255 | N/A |
| UINT16 | 2 bytes | 0 to 65,535 | Little-endian |
| UINT32 | 4 bytes | 0 to 2^32-1 | Little-endian |
| UINT64 | 8 bytes | 0 to 2^64-1 | Little-endian |
| UINT128 | 16 bytes | 0 to 2^128-1 | Little-endian |

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2641
TypedValue TypedValue::makeInt32(int32_t value)
{
    TypedValue tv(DataType::INT32);
    tv.is_null_ = false;
    tv.data_.int32_val = value;
    return tv;
}
```

### Floating-Point Types

#### Binary Representation

| Type | Storage | Format | Byte Order |
|------|---------|--------|------------|
| FLOAT32 | 4 bytes | IEEE-754 binary32 | Little-endian |
| FLOAT64 | 8 bytes | IEEE-754 binary64 | Little-endian |

**Special Value Handling:**
- NaN and Infinity are rejected unless explicitly enabled via configuration
- Values are stored in native IEEE-754 format

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2713
TypedValue TypedValue::makeFloat32(float value)
{
    TypedValue tv(DataType::FLOAT32);
    tv.is_null_ = false;
    tv.data_.float32_val = value;
    return tv;
}
```

### Decimal Types

#### DECIMAL / NUMERIC

Storage uses scaled integer representation with precision and scale stored in column metadata:

| Precision | Storage Type | Bytes |
|-----------|--------------|-------|
| <= 2 | int8 | 1 |
| <= 4 | int16 | 2 |
| <= 9 | int32 | 4 |
| <= 18 | int64 | 8 |
| <= 38 | int128 | 16 |
| > 38 | Variable-length | 4 + bytes |

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2817
TypedValue TypedValue::makeDecimal(int128_t unscaled_value, uint8_t precision, uint8_t scale)
{
    TypedValue tv(DataType::DECIMAL);
    tv.is_null_ = false;
    tv.decimal_unscaled_ = unscaled_value;
    tv.decimal_precision_ = precision;
    tv.decimal_scale_ = scale;
    return tv;
}
```

#### DECFLOAT16 / DECFLOAT34

| Type | Storage | Format |
|------|---------|--------|
| DECFLOAT16 | 8 bytes | IEEE-754-2008 Decimal64 (BID encoding) |
| DECFLOAT34 | 16 bytes | IEEE-754-2008 Decimal128 (BID encoding) |

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2827
TypedValue TypedValue::makeDecfloat(const DecFloat& value)
{
    TypedValue tv(value.precision == 16 ? DataType::DECFLOAT16 : DataType::DECFLOAT34);
    tv.is_null_ = false;
    std::vector<uint8_t> bytes;
    ErrorContext ctx;
    Status st = encodeDecfloat(value, tv.type_, bytes, &ctx);
    tv.binary_data_ = std::move(bytes);
    return tv;
}
```

#### MONEY

- Stored as int64 scaled by `money_scale` (default: 4, configurable)
- Precision: 19 digits total

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:310
uint8_t defaultMoneyScale()
{
    uint64_t configured = Config::getInstance().getUInt("types", "money_default_scale", 4);
    if (configured > 9) configured = 9;
    return static_cast<uint8_t>(configured);
}
```

### String Types

#### Binary Representation

All string types use UTF-8 encoding with `u32` length prefix:

| Type | Format | Padding |
|------|--------|---------|
| CHAR(n) | `u32 length` + UTF-8 bytes | Space (0x20) padded to n chars |
| VARCHAR(n) | `u32 length` + UTF-8 bytes | No padding |
| TEXT | `u32 length` + UTF-8 bytes | No padding, TOAST eligible |

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2737
TypedValue TypedValue::makeVarchar(const std::string& value)
{
    TypedValue tv(DataType::VARCHAR);
    tv.is_null_ = false;
    tv.string_data_ = value;
    return tv;
}
```

### Binary Types

#### Binary Representation

| Type | Format | Padding |
|------|--------|---------|
| BINARY(n) | `u32 length` + bytes | 0x00 padded to n bytes |
| VARBINARY(n) | `u32 length` + bytes | No padding |
| BLOB / BYTEA | `u32 length` + bytes | TOAST eligible |

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2777
TypedValue TypedValue::makeBinary(const std::vector<uint8_t>& value)
{
    TypedValue tv(DataType::BINARY);
    tv.is_null_ = false;
    tv.binary_data_ = value;
    return tv;
}
```

### Boolean Type

#### Binary Representation

| Value | Storage |
|-------|---------|
| false | 1 byte: 0x00 |
| true | 1 byte: 0x01 |

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2729
TypedValue TypedValue::makeBool(bool value)
{
    TypedValue tv(DataType::BOOLEAN);
    tv.is_null_ = false;
    tv.data_.bool_val = value;
    return tv;
}
```

### Temporal Types

All temporal values are normalized to UTC for storage, with optional display offset.

#### DATE

| Field | Type | Description |
|-------|------|-------------|
| mjd | int32 | Modified Julian Day (days since 1858-11-17) |
| offset_seconds | int32 | Display offset (INT32_MIN = no preference) |

Storage: 8 bytes (4 + 4)

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2981
TypedValue TypedValue::makeDate(int64_t days_since_epoch, int32_t offset_seconds)
{
    TypedValue tv(DataType::DATE);
    tv.is_null_ = false;
    tv.data_.int64_val = days_since_epoch;
    tv.timezone_offset_seconds_ = offset_seconds;
    return tv;
}
```

#### TIME

| Field | Type | Description |
|-------|------|-------------|
| microseconds | int64 | Microseconds since midnight (0 to 86399999999) |
| offset_seconds | int32 | Display offset |

Storage: 12 bytes (8 + 4)

#### TIMESTAMP

| Field | Type | Description |
|-------|------|-------------|
| microseconds | int64 | Microseconds since Unix epoch (1970-01-01) |
| offset_seconds | int32 | Display offset |

Storage: 12 bytes (8 + 4)

#### TIMESTAMP WITH TIME ZONE / TIME WITH TIME ZONE

Same storage as TIMESTAMP/TIME, with offset_seconds always set to a concrete offset value.

#### INTERVAL

| Field | Type | Description |
|-------|------|-------------|
| months | int32 | Number of months |
| days | int32 | Number of days |
| microseconds | int64 | Number of microseconds |

Storage: 16 bytes (4 + 4 + 8)

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/types.h:343
struct Interval
{
    int32_t months;         // Number of months
    int32_t days;           // Number of days
    int64_t microseconds;   // Number of microseconds
};
```

#### YEAR

Stored as INT16 with parser enforcing range constraints (MySQL compatibility: 1901-2155).

### Network Types

#### INET / CIDR

PostgreSQL-compatible format:

| Field | Type | Description |
|-------|------|-------------|
| family | uint8 | 4 for IPv4, 6 for IPv6 |
| bits | uint8 | Prefix length (CIDR) or max bits (INET) |
| is_cidr | uint8 | 0 for INET, 1 for CIDR |
| addr_len | uint8 | 4 for IPv4, 16 for IPv6 |
| address | bytes | addr_len bytes of address |

#### MACADDR / MACADDR8

| Type | Storage |
|------|---------|
| MACADDR | 6 bytes (EUI-48) |
| MACADDR8 | 8 bytes (EUI-64) |

### UUID Type

Stored as 16 raw bytes in RFC 4122 layout:

| Bytes | Field |
|-------|-------|
| 0-3 | time_low |
| 4-5 | time_mid |
| 6-7 | time_high_and_version |
| 8 | clock_seq_hi_and_reserved |
| 9 | clock_seq_low |
| 10-15 | node |

### Bit String Type (BIT)

| Field | Type | Description |
|-------|------|-------------|
| bit_length | uint32 | Number of bits |
| data | bytes | ceil(bit_length/8) bytes, MSB-first |

### Null Semantics

NULL values are tracked in the tuple null bitmap and have no payload bytes. The type system distinguishes between:
- `NULL_TYPE` (type ID 255): The SQL NULL sentinel type
- `is_null_` flag: Indicates a value of a specific type is NULL

## Interface Contracts

### Type Classification Functions

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:361
bool isStringType(DataType type)
{
    return type == DataType::CHAR || type == DataType::VARCHAR || 
           type == DataType::TEXT;
}

bool isIntegerType(DataType type)
{
    return type == DataType::INT8 || type == DataType::INT16 || 
           type == DataType::INT32 || type == DataType::INT64 || 
           type == DataType::INT128 || type == DataType::UINT8 ||
           type == DataType::UINT16 || type == DataType::UINT32 ||
           type == DataType::UINT64 || type == DataType::UINT128;
}

bool isNumericType(DataType type)
{
    return isIntegerType(type) || isFloatType(type) || isDecimalFamily(type);
}

bool isTemporalType(DataType type)
{
    return type == DataType::DATE || type == DataType::TIME ||
           type == DataType::TIMESTAMP || type == DataType::TIMESTAMP_WITH_ZONE ||
           type == DataType::TIME_WITH_ZONE || type == DataType::INTERVAL;
}
```

### TOAST Eligibility

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:758
bool TypeSystem::isToastEligibleType(DataType type)
{
    if (isStringType(type) || isBinaryType(type) || isJsonFamily(type) ||
        isGeometryType(type) || type == DataType::TSVECTOR || type == DataType::TSQUERY)
    {
        return true;
    }
    return type == DataType::ARRAY || type == DataType::LIST ||
           type == DataType::MAP || type == DataType::COMPOSITE ||
           type == DataType::ROW || type == DataType::VARIANT;
}
```

## Invariants

1. **Byte Order Consistency**: All multi-byte numeric fields use little-endian encoding
   - Verification: `appendUint32()` and `readUint32()` in typed_value.cpp

2. **UTF-8 Enforcement**: All string types must contain valid UTF-8
   - Verification: `UTF8Utils::isValidUTF8()` checks on string operations

3. **NULL Bitmap Usage**: NULL values never have payload bytes
   - Verification: Tuple null bitmap checked before payload access

4. **Temporal Normalization**: All temporal values stored in UTC
   - Verification: Offset conversion at storage boundary

5. **Decimal Precision Limits**: DECIMAL precision limited to 38 digits for fixed-width
   - Verification: `decimalStorageSize()` in typed_value.cpp

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `INVALID_TEXT_REPRESENTATION` | String-to-numeric parse failure | Return error to caller |
| `NUMERIC_VALUE_OUT_OF_RANGE` | Value exceeds type range | Return error to caller |
| `DATETIME_VALUE_OUT_OF_RANGE` | Invalid date/time value | Return error to caller |
| `DATA_CORRUPTED` | Invalid binary payload | Return error, may trigger repair |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/compatibility/scratchbird/tests/datatypes/001_numeric_integer_types.sql` | Integer type round-trip |
| `tests/compatibility/scratchbird/tests/datatypes/005_binary_types.sql` | Binary type storage |
| `tests/compatibility/firebird/converted/functional/datatypes/decfloat-binding-to-other-types.sql` | DECFLOAT operations |
| `tests/compatibility/mysql/converted/main/type_*.sql` | MySQL type compatibility |
| `tests/compatibility/postgresql/converted/core/type_sanity.sql` | PostgreSQL type sanity |

## Related Specifications

- [complex_types.md](./complex_types.md) - Array, JSON, VECTOR, and other complex types
- [type_coercion_rules.md](./type_coercion_rules.md) - Type casting and conversion rules

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| MJD | Modified Julian Day - days since 1858-11-17 |
| BID | Binary Integer Decimal - IEEE 754 decimal encoding |
| TOAST | The Oversized-Attribute Storage Technique for large values |
| SRID | Spatial Reference ID for geometry types |

### Configuration Keys

| Key | Default | Description |
|-----|---------|-------------|
| `types.money_default_scale` | 4 | Default scale for MONEY type |
| `types.default_character_set` | UTF8 | Default character encoding |
| `types.default_collation` | default | Default string collation |
| `types.json.validation` | strict | JSON validation mode |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial scalar types specification | Dalton Calford |
