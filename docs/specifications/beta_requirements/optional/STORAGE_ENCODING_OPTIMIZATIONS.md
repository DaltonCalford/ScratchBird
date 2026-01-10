# Beta Optional: Storage Encoding Optimizations (Varlen, Numeric, TOAST)

Status: Draft (Beta Optional)
Owner: Storage/Engine

## Motivation
ScratchBird v1 uses a fixed 4-byte length prefix for all variable-length values and currently TOASTs whole tuples. Firebird uses a 2-byte length for VARCHAR, and PostgreSQL uses a 1-byte header for short varlena values plus TOAST for large attributes. This optional beta feature reduces per-value overhead for short strings/binary, avoids toasting entire rows, and optionally adds a packed NUMERIC encoding for arbitrary precision.

## Goals
- Reduce storage overhead for short variable-length values without changing SQL semantics.
- Allow attribute-level TOAST so only large values are externalized.
- Provide an optional packed NUMERIC encoding for precision beyond scaled-int limits.
- Preserve backward compatibility with v1 on-disk format.
- Keep wire protocols and query semantics unchanged.

## Non-Goals
- No changes to SQL semantics or client-visible behavior.
- No forced migration of existing tables.
- No change to v1 canonical encoding rules outside of opt-in v2 tables.
- No requirement to implement page-level compression here (handled by compression framework).

## References
- `ScratchBird/docs/specifications/storage/ON_DISK_FORMAT.md`
- `ScratchBird/docs/specifications/storage/TOAST_LOB_STORAGE.md`
- `ScratchBird/docs/specifications/storage/HEAP_TOAST_INTEGRATION.md`
- `ScratchBird/docs/specifications/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md`
- `ScratchBird/docs/specifications/compression/COMPRESSION_FRAMEWORK.md`
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`
- https://www.postgresql.org/docs/current/storage-toast.html
- https://www.postgresql.org/docs/current/datatype-numeric.html
- https://raw.githubusercontent.com/postgres/postgres/master/src/backend/utils/adt/numeric.c

## Baseline (v1)
- Varlen values (VARCHAR/TEXT/JSON/BINARY/BLOB/etc.) are stored as: `uint32 length + bytes`.
- DECIMAL is stored as scaled integer with width based on precision (1/2/4/8/16 bytes).
- TOAST is applied at the tuple level; the entire tuple payload can be replaced by a single ToastPointer.
- NULLs are represented only in the tuple null bitmap.

## Proposed v2 Changes (Optional)

### 1) Storage Format Versioning
Introduce a per-table storage format version (v1 or v2). v2 enables the optimizations below.

- Catalog: add `storage_format_version` to table metadata.
- Database default: `storage.default_format_version` (1 or 2).
- DDL: `CREATE TABLE ... WITH (storage_format = 2)`; `ALTER TABLE ... SET (storage_format = 2)` rewrites the table.
- Readers select decoding path based on the table format version.

### 1.1) DDL Surface (Exact Syntax)
Storage format and column storage strategies are configured through table and column storage parameters.

#### CREATE TABLE (table-level options)
```
CREATE TABLE <table_name> (
    <column_definition> [ , ... ]
)
[ TABLESPACE <tablespace_name> ]
[ WITH ( <table_storage_parameter> = <value> [ , ... ] ) ];
```

`<table_storage_parameter>`:
- `storage_format`: `1` or `2` (default from `storage.default_format_version`)
- `toast_strategy`: `plain|extended|external|compressed`
- `toast_threshold`: integer bytes (defaults to ToastSettings threshold)
- `toast_target`: integer bytes (defaults to ToastSettings target)
- `toast_compression`: `none|lz4|zstd`
- `numeric_storage`: `scaled|packed`

#### Column definition (column-level overrides)
```
<column_definition> ::=
    <column_name> <data_type> [ <column_constraint> ... ]
    [ WITH ( <column_storage_parameter> = <value> [ , ... ] ) ]
```

`<column_storage_parameter>`:
- `toast_strategy`: `plain|extended|external|compressed`
- `toast_threshold`: integer bytes
- `toast_target`: integer bytes
- `toast_compression`: `none|lz4|zstd`
- `numeric_storage`: `scaled|packed` (NUMERIC only; DECIMAL is always scaled)

Column parameters override table defaults. Unspecified parameters inherit from table defaults.

#### ALTER TABLE (table-level)
```
ALTER TABLE <table_name>
    SET ( <table_storage_parameter> = <value> [ , ... ] );
```

Changing `storage_format` triggers a table rewrite. Other parameters update defaults for new rows; existing rows are unaffected unless rewritten.

#### ALTER TABLE (column-level)
```
ALTER TABLE <table_name>
    ALTER COLUMN <column_name>
    SET ( <column_storage_parameter> = <value> [ , ... ] );
```

Column-level changes affect new values and updates of the column. Existing values remain in their prior encoding until rewritten.

#### Examples
```
CREATE TABLE documents (
    id BIGINT PRIMARY KEY,
    title VARCHAR(200) WITH (toast_strategy = plain),
    body TEXT WITH (toast_strategy = external, toast_compression = lz4),
    price NUMERIC(50, 4) WITH (numeric_storage = packed)
)
WITH (storage_format = 2, toast_threshold = 2048);

ALTER TABLE documents SET (storage_format = 2);
ALTER TABLE documents ALTER COLUMN body
    SET (toast_strategy = external, toast_compression = zstd);
```

### 2) Varlen Header v2 (Short + Long)
Replace the fixed 4-byte length prefix with a compact header in v2 tables.

#### Encoding
- **Short header (1 byte)**: if length <= 127
  - Byte: `0b0LLLLLLL` (MSB 0), where L = length in bytes
  - Payload immediately follows
- **Long header (5 bytes)**: if length >= 128
  - Byte 0: `0x80` (long marker)
  - Bytes 1-4: `uint32` little-endian length
  - Payload immediately follows
- Reserved markers: `0x81-0xFF` are reserved for future (compressed inline, external pointer tags).

#### Notes
- Length is the byte length of the payload only (not including header bytes).
- Zero-length values are encoded as a single `0x00` byte.
- NULL remains controlled by the tuple null bitmap; no payload is stored for NULL.
- This encoding applies to all varlen types: CHAR/VARCHAR/TEXT/JSON/XML/JSONB/BINARY/VARBINARY/BLOB/BYTEA, arrays/composites/ranges that use varlen serialization.
- Alignment rule from `ON_DISK_FORMAT.md` still applies at tuple layout level; no padding is added inside the varlen payload.

#### Pseudocode
```c
// Encode
if (len <= 127) {
    out[0] = (uint8_t)len;
    memcpy(out + 1, payload, len);
} else {
    out[0] = 0x80;
    write_u32_le(out + 1, len);
    memcpy(out + 5, payload, len);
}

// Decode
if ((in[0] & 0x80) == 0) {
    len = in[0];
    payload = in + 1;
} else if (in[0] == 0x80) {
    len = read_u32_le(in + 1);
    payload = in + 5;
} else {
    error("reserved varlen marker");
}
```

#### Hex Examples
- TEXT "cat" (3 bytes): `03 63 61 74`
- VARBINARY length 200 (0xC8): `80 C8 00 00 00 <200 bytes>`
- ToastPointer length 18 (0x12): `12 <18 bytes>`

### 3) Attribute-Level TOAST (Per-Column)
Apply TOAST on a per-column basis instead of toasting the entire tuple.

#### Storage Rule
- When a varlen value is toasted, store a `ToastPointer` as the column payload.
- The varlen header length equals `sizeof(ToastPointer)` (18 bytes) so the decoder can read and check the pointer.
- `ToastManager::isToastPointer` is used to detect and detoast values on read.

#### Strategy Selection
- Add per-column storage strategy and thresholds:
  - `PLAIN`: never toast
  - `EXTENDED`: externalize without compression
  - `COMPRESSED`: compress inline (optional)
  - `EXTERNAL`: externalize and compress
- Default strategy is based on data type and column definition; configurable per column.

#### Algorithm (v2 tables)
1. Build tuple header and null bitmap.
2. For each varlen column:
   - If value length <= threshold: store inline (varlen header v2).
   - Else: use `ToastManager::toastValue` and store `ToastPointer` as column payload.
3. If the resulting row still exceeds page limits, apply tuple-level TOAST as a fallback (rare).

### 4) Packed NUMERIC Encoding (Optional)
Provide an optional packed NUMERIC encoding for arbitrary precision values, based on PostgreSQL's base-10000 digit layout.

#### When Used
- `NUMERIC(p, s)` with precision > 38, or
- `NUMERIC` column configured with `storage_mode = packed`, or
- database config `server.numeric.prefer_packed = true`.

#### Packed Layout (payload)
The varlen payload uses one of two headers followed by base-10000 digits.

```c
// All fields are little-endian.
struct NumericPackedShort {
    uint16 header;    // sign + scale + weight (short form)
    int16  digits[];  // base-10000 digits
};

struct NumericPackedLong {
    uint16 sign_scale; // sign + scale (long form)
    int16  weight;     // weight of first digit
    int16  digits[];   // base-10000 digits
};
```

Header format follows PostgreSQL conventions:
- `NUMERIC_POS = 0x0000`
- `NUMERIC_NEG = 0x4000`
- `NUMERIC_SHORT = 0x8000`
- `NUMERIC_SPECIAL = 0xC000` (NaN/Inf not supported in ScratchBird; reserved)
- Short form packs sign, scale, and weight into the 14 low bits.
- Long form stores scale in the header and weight separately.

Notes:
- Digits are base-10000, stored as int16 (0..9999), most significant digit first.
- Values are normalized (no leading or trailing zero digits).
- Zero is encoded with no digits; weight can be 0.

#### Interaction With DECIMAL
- DECIMAL retains scaled-integer encoding for fixed precision.
- NUMERIC with precision <= 38 may still use scaled-int unless configured to packed.

### 5) Compression (Optional)
Compression is optional and may be applied inline or in TOAST storage.

- Inline compression: varlen payload begins with `ToastCompressHeader` followed by compressed bytes.
- External compression: TOAST chunks store compressed bytes; `ToastPointer.va_tag` indicates compression.
- Compression algorithms are defined in `COMPRESSION_FRAMEWORK.md` (LZ4 baseline, Zstd optional).

## Compatibility and Migration
- v1 tables use existing `uint32 length + bytes` encoding and tuple-level TOAST rules.
- v2 tables use varlen header v2 and attribute-level TOAST.
- Mixed format is allowed per table; data readers must check table format.
- Migration uses table rewrite:
  - `ALTER TABLE ... SET (storage_format = 2)` rewrites all tuples.
  - `ALTER TABLE ... SET (storage_format = 1)` rewrites back to v1.
- Backup/restore must preserve the table storage format.

## Implementation Checklist (Beta Optional)
- Add `storage_format_version` to system catalog and DDL surface.
- Add v2 varlen encoding helpers (encode/decode) with tests.
- Update tuple builder and reader to support per-column TOAST in v2.
- Add per-column storage strategy metadata and defaults.
- Implement packed NUMERIC encoder/decoder (optional behind config).
- Update `TypedValue` serialization to accept format version.
- Add compatibility gates in backup/restore and verification tools.

## Testing
- Round-trip tests for varlen headers: lengths 0, 1, 127, 128, 255, 256, 4096, 2^32-1.
- Mixed tuples with small varlen inline and large varlen toasted.
- Packed NUMERIC round-trip tests for small and large precisions.
- Regression tests to ensure v1 tables remain unchanged.

## Open Questions
- Whether to allow inline compression for non-TOAST values in v2.
- Upper bound for packed NUMERIC precision in ScratchBird.
