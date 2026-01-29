# Data Type Persistence and Casting (Canonical Encoding)

Status: Draft (Alpha). This document defines the canonical on-disk encoding for all DataType values, the conversion rules for CAST/TRY_CAST, and the error code/SQLSTATE mapping. It is the source of truth for parser/SBLR/executor changes.

## Goals
- All DataType values must round-trip (write/read) in unencrypted heap tuples using a single canonical encoding.
- Encrypted storage uses the same canonical encoding for plaintext payloads.
- CAST/TRY_CAST must support string <-> numeric and string <-> temporal conversions with consistent errors.
- DATE/TIME/TIMESTAMP must be normalized to UTC for storage, while preserving the original timezone offset.
- CHAR/VARCHAR/BINARY length limits are enforced on INSERT/UPDATE (no silent truncation).

## Canonical Encoding Rules (all unencrypted heap + encrypted plaintext)

### General
- Byte order: little-endian for all fixed-width numeric values.
- Variable-length: uint32 length prefix followed by raw bytes.
- NULL: represented only in the tuple null bitmap (no payload).

### Integer types
- INT8/INT16/INT32/INT64/UINT8/UINT16/UINT32/UINT64/UINT128: fixed width, little-endian.
- INT128/UINT128/UUID: fixed 16 bytes (two's-complement for INT128, unsigned for UINT128, raw UUID bytes for UUID).
- INT128/UINT128 casts and comparisons use the full 128-bit range (no 38-digit limit).

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

### DECFLOAT (decimal floating)
- Stored as IEEE 754-2008 decimal floating formats:
  - DECFLOAT(16) -> Decimal64 (8 bytes)
  - DECFLOAT(34) -> Decimal128 (16 bytes)
- Payload is the raw IEEE decimal encoding in little-endian byte order.
- Values outside the target range raise NUMERIC_VALUE_OUT_OF_RANGE.
- NaN/Infinity are rejected (INVALID_TEXT_REPRESENTATION) unless explicitly
  enabled by a future session/config flag.
- Rounding and trap behavior follow session settings (see SET DECFLOAT in the
  Firebird compatibility spec).

### MONEY
- Stored as int64 (fixed 8 bytes), scale is implied by MONEY semantics.

### Text
- CHAR: uint32 length + raw bytes; stored length equals declared precision. Values shorter are right-padded with spaces (0x20). Values longer raise STRING_DATA_RIGHT_TRUNCATION.
- VARCHAR/TEXT: uint32 length + raw bytes (no padding; length <= declared precision for VARCHAR).
- JSON/JSONB/XML: uint32 length + raw bytes (text JSON/XML; JSONB is text in Alpha).

### Binary
- BINARY: uint32 length + raw bytes; stored length equals declared precision. Values shorter are right-padded with 0x00 bytes. Values longer raise STRING_DATA_RIGHT_TRUNCATION.
- VARBINARY/BLOB/BYTEA: uint32 length + raw bytes (binary safe).

### Temporal (UTC + offset seconds)
- Storage uses UTC-normalized values with microsecond resolution, plus a signed offset seconds field.
- DATE: int32 MJD (date in UTC) + int32 offset_seconds.
- TIME: int64 microseconds since midnight UTC + int32 offset_seconds.
- TIMESTAMP: int64 microseconds since Unix epoch UTC + int32 offset_seconds.
- All date/time inputs are normalized to UTC before storing.
- Offset seconds is the original timezone offset at input time (can be 0 for UTC).
- `with_timezone` and `timezone_hint` are metadata for parsing/formatting and are not stored in the payload.

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
- string -> numeric: INT*, UINT*, FLOAT*, DECIMAL, MONEY, DECFLOAT
- numeric -> string (VARCHAR/TEXT)
- string -> temporal: DATE/TIME/TIMESTAMP (with optional timezone offset)
- temporal -> string (uses stored offset or UTC)
- string -> binary (with USING format)
- binary -> string (with USING format)
- UUID <-> string
- JSON/JSONB/XML <-> string

### Canonical text formats (round-trip guarantee)
For each type below, any string that matches the canonical format MUST be accepted by CAST/TRY_CAST back to the original type. In other words, `CAST(x AS TEXT)` MUST produce a string that can be `CAST(... AS <type>)` without loss of meaning (subject to precision/scale limits).

- Integer types: optional sign (`+`/`-`) followed by base-10 digits. Leading/trailing ASCII whitespace is allowed.
- Integer types also accept hexadecimal literal form (`0x` or `0X` + hex digits) for parsing; this is not the default formatting unless `USING hexadecimal` is specified.
- Floating types: base-10 decimal or exponent form (per `strtod`), finite only. NaN/Inf are rejected.
- DECIMAL: optional sign, digits, optional decimal point; scale/precision enforced by target type.
- DECFLOAT: decimal or scientific notation; precision 16 or 34 is enforced by the target type.
- MONEY: decimal string with scale 4 (extra fractional digits are rounded per DECIMAL rules).
- BOOLEAN: `true`/`false`/`t`/`f`/`1`/`0` (case-insensitive, ASCII whitespace allowed).
- UUID: canonical `8-4-4-4-12` hex is the default; parser must also accept raw 32-hex (no separators), braces, and `urn:uuid:` prefixes.
- Binary types: default format is HEX (lowercase, no separators). Parser must accept optional `0x` or `\\x` prefix for HEX. BASE64 and ESCAPE are supported when USING is specified.
- DATE: `YYYY-MM-DD` with optional offset suffix (`Z` or `+/-HH:MM` or `+/-HHMM`).
- TIME: `HH:MM[:SS[.ffffff]]` with optional offset suffix; fractional seconds are 1-6 digits (microsecond precision).
- TIMESTAMP: `YYYY-MM-DD[ T]HH:MM:SS[.ffffff]` with optional offset suffix; fractional seconds are 1-6 digits.
- JSON/JSONB/XML: treated as raw text; no validation beyond length.

### Binary USING formats
- `USING hex` (default when not specified)
- `USING base64`
- `USING escape` (bytea-style \x and octal escapes)

### Numeric USING formats
- `USING hexadecimal` (alias `hex`) formats integer values as lowercase hex with a `0x` prefix and parses hex strings back to integers.

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
- CAST supports optional `USING <format>` clause: `CAST(expr AS VARCHAR(50) USING hex)` or `CAST(expr AS VARCHAR(50) USING hexadecimal)`.
- SBLR CAST payload must include the USING format (enum or string) and type modifiers.
- All CAST operations must reference this document.

## Configuration
- `server.time.date_default_time = 00:00:00` in `sb_config.ini` (section `[server.time]`).
- Default is 00:00:00 when not configured.

## Appendix: Storage Format v2 (Beta Optional)
This appendix applies only to tables with `storage_format_version = 2`. See `ScratchBird/docs/specifications/beta_requirements/optional/STORAGE_ENCODING_OPTIMIZATIONS.md`.

### Varlen Header v2
- Short header (1 byte): `0b0LLLLLLL` for lengths 0..127.
- Long header (5 bytes): `0x80` + `uint32` little-endian length for lengths >= 128.
- Reserved markers: `0x81-0xFF` are reserved for future use.
- NULL remains represented only by the tuple null bitmap.

### Per-Column TOAST
- Varlen columns may store a `ToastPointer` payload in place of the original value.
- The varlen length equals `sizeof(ToastPointer)` (18 bytes); values are detoasted on read if the payload is a ToastPointer.
- TOAST thresholds and strategy are configured per column or inherited from table defaults.

### Packed NUMERIC (Optional)
- NUMERIC may use a packed base-10000 digit format when configured (`numeric_storage = packed`).
- DECIMAL always remains scaled-integer encoding; NUMERIC with precision <= 38 may remain scaled unless configured to packed.
