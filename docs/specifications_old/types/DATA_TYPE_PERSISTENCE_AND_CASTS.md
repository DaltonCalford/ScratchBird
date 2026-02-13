# Data Type Persistence, Parsing, and Casting (Canonical)
Status: Authoritative (V3)

## Goals
- All DataType values must round-trip (write/read) in unencrypted heap tuples using a single canonical encoding.
- Encrypted storage uses the same canonical encoding for plaintext payloads.
- CAST/TRY_CAST must be deterministic and use Firebird 5.x compatible parsing rules for date/time.
- DATE/TIME/TIMESTAMP are stored in UTC; TIMESTAMP WITH TIME ZONE and TIME WITH TIME ZONE also store a per-value display offset.
- CHAR/VARCHAR/BINARY length limits are enforced on INSERT/UPDATE (no silent truncation unless explicitly cast).
- JSONB is stored in PostgreSQL binary JSONB format; conversions to/from CBOR are provided by functions.

## Canonical Encoding Rules (all unencrypted heap + encrypted plaintext)

### General
- Byte order: little-endian for all fixed-width numeric values.
- Variable-length: uint32 length prefix followed by raw bytes, unless a type defines a different internal layout (arrays, JSONB, composite, etc.).
- NULL: represented only in the tuple null bitmap (no payload).

### Integer types
- INT8/INT16/INT32/INT64/UINT8/UINT16/UINT32/UINT64: fixed width, little-endian.
- INT128/UINT128: fixed 16 bytes, little-endian. INT128 uses two's-complement.
- UUID: fixed 16 bytes, raw UUID bytes (little-endian storage).

### Floating point
- FLOAT32: IEEE754 binary32 (4 bytes LE).
- FLOAT64: IEEE754 binary64 (8 bytes LE).

### DECIMAL / NUMERIC (scaled integer)
- Stored as scaled integer only (no prefix, no scale in payload).
- Width is derived from column precision:
  - precision <= 2  -> int8
  - precision <= 4  -> int16
  - precision <= 9  -> int32
  - precision <= 18 -> int64
  - precision <= 38 -> int128
  - precision > 38  -> error (unsupported)
- Scale and precision come from column metadata (TypeInfo/ColumnInfo). No scale bytes are stored.
- NUMERIC and DECIMAL share the same storage rules in Alpha (packed NUMERIC is optional Beta).

### DECFLOAT (decimal floating)
- Stored as IEEE 754-2008 decimal floating formats:
  - DECFLOAT(16) -> Decimal64 (8 bytes)
  - DECFLOAT(34) -> Decimal128 (16 bytes)
- Payload is the raw IEEE decimal encoding in little-endian byte order.
- Values outside the target range raise NUMERIC_VALUE_OUT_OF_RANGE.
- NaN/Infinity are rejected (INVALID_TEXT_REPRESENTATION) unless explicitly enabled by a future session/config flag.

### MONEY
- Stored as int64 (fixed 8 bytes), scale and rounding are defined by the column type modifier.
- DDL: money(precision, rounding)
  - precision: total decimal digits
  - rounding: required rounding mode (see Rounding Modes section)

### Text
- CHAR: uint32 length + raw bytes; stored length equals declared precision. Values shorter are right-padded with spaces (0x20).
- VARCHAR/TEXT: uint32 length + raw bytes (no padding).
- JSON: uint32 length + raw bytes (text JSON).
- XML: uint32 length + raw bytes.

### JSONB (PostgreSQL format)
- Stored as PostgreSQL JSONB binary format.
- JSONB is a varlena container of JEntry headers and value data. The root node is an array or object, with JB_FARRAY/JB_FOBJECT flags and optional JB_FSCALAR for scalar values.
- JEntry layout and container rules follow PostgreSQL's jsonb.h.

### Binary
- BINARY: uint32 length + raw bytes; stored length equals declared precision. Values shorter are right-padded with 0x00 bytes.
- VARBINARY/BLOB/BYTEA: uint32 length + raw bytes (binary safe).
- BLOB_SUB_TYPE_TEXT is stored as VARCHAR + TOAST (charset/collation rules apply).

### BIT
- On disk stored as BINARY/VARBINARY payload, length in bytes.
- PostgreSQL emulation parser maps BIT/VARBIT input/output to binary with bit-length semantics (padding/truncation in parser). Engine storage remains binary.

### Temporal (UTC + per-value display offset)
- Storage uses UTC-normalized values with microsecond resolution.
- All time zone input (offsets or named regions) is resolved to UTC at insert time.
- Per-value display offset is stored as an int32 (seconds). This is a numeric offset only (no zone name).
- If the user does not specify a display preference, offset_seconds MUST be set to INT32_MIN to indicate "no preference".

Encoding:
- DATE: int32 MJD (UTC date) + int32 offset_seconds
- TIME: int64 microseconds since midnight UTC + int32 offset_seconds
- TIMESTAMP: int64 microseconds since Unix epoch UTC + int32 offset_seconds
- TIME WITH TIME ZONE: same as TIME, with per-value offset_seconds
- TIMESTAMP WITH TIME ZONE: same as TIMESTAMP, with per-value offset_seconds

### Spatial
- POINT, LINESTRING, POLYGON, MULTI*, GEOMETRYCOLLECTION: use PostgreSQL-compatible binary layouts (see MULTI_GEOMETRY_TYPES_SPEC.md and PostgreSQL PostGIS-style WKB rules where applicable).

### Arrays
- Stored in PostgreSQL ArrayType layout:
  - varlena header
  - ndim (int32)
  - dataoffset (int32, 0 if no null bitmap)
  - elemtype (OID)
  - dimensions[] (int32)
  - lower bounds[] (int32)
  - optional null bitmap
  - data in row-major order

### Composite (record/struct)
- Stored in PostgreSQL composite binary format:
  - int32 number_of_columns
  - repeated for each column:
    - int32 column_type_oid
    - int32 column_length (-1 for NULL)
    - column binary payload

### Variant
- Stored as a single-element composite following the composite format above, where the column_type_oid is the variant tag.

### Ranges
- Stored in PostgreSQL range binary format for each range type (int4range, int8range, numrange, tsrange, tstzrange, daterange).

### Text search
- TSVECTOR/TSQUERY: stored in PostgreSQL binary formats.

### Network
- INET/CIDR/MACADDR/MACADDR8: stored in PostgreSQL binary formats.

### Vector
- Stored as uint32 length + raw float32[] bytes (little-endian). Parser determines dimension and validates.

## Type Ranges (Canonical)

### Integer ranges
- INT8: -128 to 127
- INT16: -32,768 to 32,767
- INT32: -2,147,483,648 to 2,147,483,647
- INT64: -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
- INT128: full two's-complement 128-bit range
- UINT8: 0 to 255
- UINT16: 0 to 65,535
- UINT32: 0 to 4,294,967,295
- UINT64: 0 to 18,446,744,073,709,551,615
- UINT128: full 128-bit unsigned range

### Floating ranges
- FLOAT32/FLOAT64: IEEE-754 finite values only (NaN/Inf rejected by default)
- DECFLOAT16/34: IEEE-754 decimal ranges for 16/34 digits

### DECIMAL/NUMERIC
- Precision 1..38, scale 0..precision
- Values must fit the target scaled integer width

### MONEY
- Precision is defined per-column. Scale and rounding are defined by the money(precision, rounding) modifier.

### Character/Binary length limits
- Length is specified in characters for CHAR/VARCHAR and bytes for BINARY/VARBINARY.
- Maximum characters depend on charset byte width. The total byte size must not exceed 32,765 bytes (Firebird-compatible cap for CHAR/VARCHAR).
- TEXT and BLOB/BYTEA lengths may exceed 32,765 bytes and are stored via TOAST.

## Dialect Alias Mapping (Parser-Level)
- DATETIME (MySQL) -> TIMESTAMP
- YEAR (MySQL) -> SMALLINT (range enforced by parser)
- MEDIUMINT (MySQL) -> INT32 (range enforced by parser)
- BIT/VARBIT (PostgreSQL) -> BINARY/VARBINARY with bit-length handling in parser
- ENUM/SET (MySQL) -> ENUM/SET type definitions with ordinal storage in engine

## Character Sets and Collations
- Charset-specific length semantics: lengths are in characters, but storage cap is bytes (32,765 for CHAR/VARCHAR).
- CHAR/VARCHAR enforce character counts; byte-length overflow is an error.
- UNICODE_FSS is treated as UTF-8 with a 3-byte ceiling; 4-byte UTF-8 sequences are rejected.
- OCTETS is treated as binary (aliases: BINARY/VARBINARY). No collation or character semantics are applied.

## Parsing Rules

### Firebird-Compatible Date/Time Parsing (Base)
Date/time parsing MUST follow Firebird 5.x rules (CVT_string_to_datetime) as a base:
- Special keywords: NOW, TODAY, TOMORROW, YESTERDAY
- Accepted date formats (examples; separators are flexible):
  - YYYY-MM-DD
  - MM-DD-YY or MM/DD/YY
  - DD.MM.YY (period implies DMY ordering)
  - English month names (JANUARY..DECEMBER) in first or second position
- Time formats:
  - HH:MM
  - HH:MM:SS
  - HH:MM:SS.fraction (fractional seconds up to 4 digits in Firebird; ScratchBird stores microseconds)
- Separators between date fields may be any punctuation; whitespace between components is allowed.
- Two-digit years map to a rolling 50-year window (Firebird behavior).
- Time zone suffix (offset or region name) is allowed for TIMESTAMP/TIME WITH TIME ZONE.

### Time Zone Parsing
- Accepted: numeric offsets (+HH:MM or -HH:MM) and region names (e.g., America/New_York).
- Region names are resolved to offsets at input time; the stored per-value offset is numeric only.
- If no display preference is specified, offset_seconds MUST be set to INT32_MIN.

### Numeric Parsing
- Integer: optional sign, base-10 digits. Hex (`0x` or `0X`) accepted with USING hexadecimal.
- Floating: base-10 with optional exponent, finite only.
- DECIMAL/NUMERIC: optional sign, digits, optional decimal point; scale/precision enforced by target type.
- DECFLOAT: decimal/scientific notation, precision enforced by target type.

### Boolean Parsing
- true/false/t/f/1/0 (case-insensitive, ASCII whitespace allowed).

### Binary Parsing
- Default format: hex (lowercase, no separators).
- Optional prefixes: `0x` or `\\x` for hex.
- USING base64 or escape for alternate formats.

### UUID Parsing
- Canonical 8-4-4-4-12 hex.
- Accepts raw 32-hex, braces, and `urn:uuid:` prefixes.

## CAST/TRY_CAST Rules

### Supported conversions (minimum)
- string -> numeric: INT*, UINT*, FLOAT*, DECIMAL, MONEY, DECFLOAT
- numeric -> string (VARCHAR/TEXT)
- string -> temporal: DATE/TIME/TIMESTAMP (Firebird rules + TZ parsing)
- temporal -> string (uses stored offset if present, else session timezone)
- string -> binary (with USING format)
- binary -> string (with USING format)
- UUID <-> string
- JSON/JSONB/XML <-> string

### Error codes / SQLSTATE
Use core::Status with standard SQLSTATE mapping:
- Invalid numeric text -> Status::INVALID_TEXT_REPRESENTATION (22P02)
- Invalid binary text -> Status::INVALID_TEXT_REPRESENTATION (22P02)
- Invalid datetime text -> Status::INVALID_DATETIME_FORMAT (22007)
- Datetime overflow -> Status::DATETIME_FIELD_OVERFLOW (22008)
- Overlength CHAR/VARCHAR -> Status::STRING_DATA_RIGHT_TRUNCATION (22001)
- Unsupported cast -> Status::DATATYPE_MISMATCH (42804)

## User-Defined Types and Domains (Catalog Requirements)

### Type Kinds
- Base (scalar) types: custom internal storage with input/output functions
- Composite types: record/struct types
- Enum types: ordered set of literals, stored as integer ordinals
- Set types: unordered set of literals, stored as integer bitset or array of ordinals
- Range types: subtype + bounds and inclusivity flags

### Required Functions
- input(text) -> value
- output(value) -> text
- recv(binary) -> value
- send(value) -> binary
- analyze(value) -> stats (optional)
- cast(value, target_type) -> target_value

### Catalog Metadata (Minimum)
- Type name, namespace, owner, kind, storage strategy
- Type modifiers (precision, scale, length)
- Element/subtype info for arrays/ranges
- Enum/set literal table (literal, ordinal)
- Composite field table (field name, type, order)
- Function bindings (input/output/recv/send/analyze/cast)
- Dependency graph (drop cascade rules)

## TOAST Integration (Type-Level)
- Any value larger than TOAST threshold is eligible for TOAST.
- Per-attribute TOAST is the default; tuple-level TOAST is a fallback if the row still exceeds page limits.
- TOAST pointer payload replaces the original column payload.

## Rounding Modes (MONEY)
- HALF_UP
- HALF_EVEN
- HALF_DOWN
- HALF_AWAY
- CEILING
- FLOOR
- TRUNCATE
- ACCOUNTING

## Appendix: Storage Format v2 (Beta Optional)
This appendix applies only to tables with `storage_format_version = 2`.

### Varlen Header v2
- Short header (1 byte): 0b0LLLLLLL for lengths 0..127.
- Long header (5 bytes): 0x80 + uint32 little-endian length for lengths >= 128.
- Reserved markers: 0x81-0xFF reserved for future use.
- NULL remains represented only by the tuple null bitmap.

### Packed NUMERIC (Optional)
- NUMERIC may use a packed base-10000 digit format when configured (`numeric_storage = packed`).
- DECIMAL always remains scaled-integer encoding; NUMERIC with precision <= 38 may remain scaled unless configured to packed.
