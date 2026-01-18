# Data Types

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

ScratchBird provides a rich type system including domains, arrays, spatial, JSON,
vector, and temporal types. The canonical on-disk encoding for heap storage is
defined by TypedValue serialization and the persistence spec.

## Numeric types

### Integers

| Type | Bytes | Range (signed) | Notes |
| --- | --- | --- | --- |
| INT8 | 1 | -128 to 127 | Two's complement |
| INT16 | 2 | -32768 to 32767 | Two's complement |
| INT32 | 4 | -2^31 to (2^31 - 1) | Two's complement |
| INT64 | 8 | -2^63 to (2^63 - 1) | Two's complement |
| INT128 | 16 | -2^127 to (2^127 - 1) | Two's complement |

Unsigned variants are available (UINT8 to UINT128) with ranges 0 to (2^N - 1).

### Floating point

| Type | Bytes | Notes |
| --- | --- | --- |
| REAL (FLOAT32) | 4 | IEEE-754 binary32 |
| DOUBLE PRECISION (FLOAT64) | 8 | IEEE-754 binary64 |

### DECIMAL / NUMERIC

- Stored as a scaled integer with no per-row scale bytes.
- Storage width is determined by column precision:
  - precision <= 2  -> 1 byte
  - precision <= 4  -> 2 bytes
  - precision <= 9  -> 4 bytes
  - precision <= 18 -> 8 bytes
  - precision <= 38 -> 16 bytes

### MONEY

- Stored as INT64 (8 bytes), scaled by 10^-4.

## Character and binary types

| Type | Storage | Notes |
| --- | --- | --- |
| CHAR(n) | uint32 length + bytes | Padded with spaces to n; no truncation |
| VARCHAR(n) | uint32 length + bytes | No padding; length limited to n |
| TEXT | uint32 length + bytes | No length limit enforced by type |
| BINARY(n) | uint32 length + bytes | Padded with 0x00 to n |
| VARBINARY | uint32 length + bytes | No padding |
| BYTEA / BLOB | uint32 length + bytes | Binary-safe |
| JSON / JSONB / XML | uint32 length + bytes | JSONB is text in Alpha |

NULL values are represented only in the tuple null bitmap (no payload bytes).

## Temporal types

ScratchBird stores temporal values normalized to UTC plus the original offset.

| Type | Payload | Notes |
| --- | --- | --- |
| DATE | int32 MJD + int32 offset_seconds | MJD is UTC date; offset preserved |
| TIME | int64 micros + int32 offset_seconds | Micros since midnight UTC |
| TIMESTAMP | int64 micros + int32 offset_seconds | Micros since Unix epoch UTC |

The default time-of-day for DATE is configured by
`server.time.date_default_time` (default 00:00:00).

## UUID and 128-bit

| Type | Bytes | Notes |
| --- | --- | --- |
| UUID | 16 | Raw UUID bytes |
| INT128 / UINT128 | 16 | Two's complement / unsigned |

## Arrays, domains, and variants

- Arrays are stored as typed element lists with per-element payloads.
- Domains are first-class types with constraints, security, and validation.
- VARIANT holds a dynamically typed value at runtime.

## Spatial and vector types

- Spatial types (POINT, LINESTRING, POLYGON, MULTI*) follow TypedValue layouts.
- VECTOR stores float32 elements as a binary payload with length prefix.

## EXTRACT and ALTER_ELEMENT

ScratchBird defines a shared element vocabulary for EXTRACT and ALTER_ELEMENT.
Selected examples:

- Numeric: VALUE, SIGN, ABS, BYTES, BITS, HI64, LO64
- String: VALUE, CHAR_LENGTH, OCTET_LENGTH, CODEPOINT_LENGTH
- Binary: VALUE, LENGTH, BYTE(index), BIT(index), SLICE(start, length)
- JSON/XML: PATH(path), TYPE, KEYS, ATTRIBUTES
- Temporal: YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, TZ_OFFSET, ISO_WEEK
- Vector: DIMENSION, ELEMENT(index), NORM_L2

See the EXTRACT specification for the full catalog of elements and rules.

## Temporal context values and literals

Core temporal context functions:

- NOW(), CURRENT_TIMESTAMP
- CURRENT_DATE
- CURRENT_TIME

Firebird emulation also recognizes shorthand datetime literals when using the
Firebird parser (for example, 'NOW', 'TODAY', 'TOMORROW', 'YESTERDAY') via CAST.

## References

- `docs/specifications/types/03_TYPES_AND_DOMAINS.md`
- `docs/specifications/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md`
- `docs/specifications/ddl/EXTRACT_AND_ALTER_ELEMENT.md`
- `docs/specifications/core/INTERNAL_FUNCTIONS.md`
- `docs/specifications/types/UUID_IDENTITY_COLUMNS.md`
- `docs/specifications/types/MULTI_GEOMETRY_TYPES_SPEC.md`
- `docs/specifications/types/TIMEZONE_SYSTEM_CATALOG.md`
