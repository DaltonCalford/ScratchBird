# Data Type Persistence and Casting (Canonical Encoding)

Status: Draft (Alpha). This document defines the canonical on-disk encoding for all DataType values, the conversion rules for CAST/TRY_CAST, and the error code/SQLSTATE mapping. It is the source of truth for parser/SBLR/executor changes.

## Goals
- All DataType values must round-trip (write/read) in unencrypted heap tuples using a single canonical encoding.
- Encrypted storage uses the same canonical encoding for plaintext payloads.
- CAST/TRY_CAST must support string <-> numeric and string <-> temporal conversions with consistent errors.
- DATE/TIME/TIMESTAMP must be normalized to UTC for storage, while preserving the original timezone offset.
- CHAR/VARCHAR length limits are enforced on INSERT/UPDATE (no silent truncation).

## Canonical Encoding Rules (all unencrypted heap + encrypted plaintext)

### General
- Byte order: little-endian for all fixed-width numeric values.
- Variable-length: uint32 length prefix followed by raw bytes.
- NULL: represented only in the tuple null bitmap (no payload).

### Integer types
- INT8/INT16/INT32/INT64/UINT8/UINT16/UINT32/UINT64: fixed width, little-endian.
- INT128/UUID: fixed 16 bytes (two's-complement for INT128, raw UUID bytes for UUID).

### Floating point
- FLOAT32: IEEE754 binary32 (4 bytes LE).
- FLOAT64: IEEE754 binary64 (8 bytes LE).

### DECIMAL (scaled integer)
- Stored as scaled integer only (no prefix, no scale in payload).
- Width is derived from column precision:
  - precision <= 2  -> int8
  - precision <= 4  -> int16
  - precision <= 9  -> int32
  - precision <= 18 -> int64
  - precision <= 38 -> int128
  - precision > 38  -> error (unsupported)
- Scale and precision come from column metadata (TypeInfo/ColumnInfo). No scale bytes are stored.

### MONEY
- Stored as int64 (fixed 8 bytes), scale is implied by MONEY semantics.

### Text
- CHAR/VARCHAR/TEXT: uint32 length + raw bytes (no padding/truncation on storage).
- JSON/JSONB/XML: uint32 length + raw bytes (text JSON/XML; JSONB is text in Alpha).

### Binary
- BINARY/VARBINARY/BLOB/BYTEA: uint32 length + raw bytes (binary safe).

### Temporal (UTC + offset seconds)
- Storage uses Firebird-compatible date/time formats, plus a signed offset seconds field.
- DATE: int32 MJD (date in UTC) + int32 offset_seconds.
- TIME: int32 deci_ms (100 us units since midnight UTC) + int32 offset_seconds.
- TIMESTAMP: int32 MJD (UTC date) + int32 deci_ms (UTC time) + int32 offset_seconds.
- All date/time inputs are normalized to UTC before storing.
- Offset seconds is the original timezone offset at input time (can be 0 for UTC).

#### DATE handling
- DATE is treated as a local date at `server.time.date_default_time` (default 00:00:00) in the input timezone.
- The local date/time is converted to UTC, then stored as MJD + offset_seconds.
- This preserves the original offset for later display or conversion.

### Spatial
- POINT, LINESTRING, POLYGON, MULTI*, GEOMETRYCOLLECTION: use TypedValue::serializePlainValue encodings (see TypedValue for byte layout).

### Array/Composite/Variant
- Stored as TypedValue::serializePlainValue encodings (element list with type tags and per-element payloads).

### Text search and ranges
- TSVECTOR/TSQUERY/RANGE types use TypedValue::serializePlainValue encodings (see TypedValue).
- TSVECTOR/TSQUERY binary layout follows their `toBinary()` implementations in `core/tsvector` and `core/tsquery`.

## CAST/TRY_CAST Rules

### Supported conversions (minimum)
- string -> numeric: INT*, UINT*, FLOAT*, DECIMAL, MONEY
- numeric -> string (VARCHAR/TEXT)
- string -> temporal: DATE/TIME/TIMESTAMP (with optional timezone offset)
- temporal -> string (uses stored offset or UTC)
- string -> binary (with USING format)
- binary -> string (with USING format)
- UUID <-> string
- JSON/JSONB/XML <-> string

### Binary USING formats
- `USING hex` (default when not specified)
- `USING base64`
- `USING escape` (bytea-style \x and octal escapes)

### Error codes / SQLSTATE
Use core::Status with standard SQLSTATE mapping:
- Invalid numeric text -> Status::INVALID_TEXT_REPRESENTATION (22P02)
- Invalid binary text -> Status::INVALID_TEXT_REPRESENTATION (22P02)
- Invalid datetime text -> Status::INVALID_DATETIME_FORMAT (22007)
- Datetime overflow -> Status::DATETIME_FIELD_OVERFLOW (22008)
- Overlength CHAR/VARCHAR -> Status::STRING_DATA_RIGHT_TRUNCATION (22001)
- Unsupported cast -> Status::DATATYPE_MISMATCH (42804)

Error messages must be structured and include value + target type when possible.

## Parser/SBLR Requirements
- CAST supports optional `USING <format>` clause: `CAST(expr AS VARCHAR(50) USING hex)`.
- SBLR CAST payload must include the USING format (enum or string) and type modifiers.
- All CAST operations must reference this document.

## Configuration
- `server.time.date_default_time = 00:00:00` in `sb_config.ini` (section `[server.time]`).
- Default is 00:00:00 when not configured.
