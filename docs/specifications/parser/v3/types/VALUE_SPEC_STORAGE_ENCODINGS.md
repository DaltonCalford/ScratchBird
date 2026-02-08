# VALUE_SPEC Storage Encodings (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: define how each VALUE_SPEC literal maps to on-disk storage bytes. This
is the canonical encoder/decoder reference for the executor and storage layers.

Unless stated otherwise, all fixed-width numeric fields are little-endian.
Variable-length values use a `uint32` length prefix + raw bytes.

## 1) Scalar Types

### NULL
`SBLR3_LITERAL_NULL`
- No payload bytes stored.
- Null is represented only by the tuple null bitmap.

### BOOLEAN
`SBLR3_LITERAL_BOOLEAN`
- Stored as `uint8` (0 or 1).

### Signed/Unsigned Integers
- `INT8/INT16/INT32/INT64` -> fixed 1/2/4/8 bytes LE.
- `UINT8/UINT16/UINT32/UINT64` -> fixed 1/2/4/8 bytes LE.
- `INT128/UINT128` -> fixed 16 bytes LE.

### Floating
- `FLOAT32` -> IEEE-754 binary32 (4 bytes LE).
- `DOUBLE` -> IEEE-754 binary64 (8 bytes LE).

### DECIMAL / NUMERIC
Stored as scaled integer only (scale/precision derived from column metadata).

### MONEY
Stored as int64 scaled per column modifier.

### UUID
Stored as 16 raw UUID bytes (v7 canonical unless otherwise specified).

### CHAR / VARCHAR / TEXT / STRING / JSON / XML
Stored as `uint32 length` + UTF-8 bytes.
- CHAR is padded to declared length with spaces.
- VARCHAR/TEXT/JSON/XML are stored without padding.

### BINARY / VARBINARY / BYTEA / BLOB
Stored as `uint32 length` + raw bytes.
- BINARY is padded to declared length with `0x00`.
- Large values are TOASTed per `storage/TOAST_LOB_STORAGE.md`.

### BIT / VARBIT
Stored as packed bits with `int32 nbits` + `bytes[(nbits+7)/8]`.

## 2) Temporal Types

Stored as UTC-normalized values plus per-value display offset (seconds):

- DATE: `int32 mjd` + `int32 offset_seconds`
- TIME: `int64 usec_since_midnight` + `int32 offset_seconds`
- TIMESTAMP: `int64 epoch_usec` + `int32 offset_seconds`
- TIME/TIMESTAMP WITH TIME ZONE: same as above with offset set to input offset

## 3) JSONB

Stored as `uint32 length` + JSONB binary payload (PostgreSQL-compatible).

## 4) Arrays

PostgreSQL-compatible array layout:
```
int32 ndim
int32 flags
uint32 elem_oid
int32 dims[ndim]
int32 lower_bounds[ndim]
optional null bitmap
element data (row-major)
```

## 5) Composite / Row / Variant

```
int32 column_count
repeat column_count:
  int32 column_type_oid
  int32 column_length   // -1 = NULL
  column_data bytes
```

## 6) Range Types

```
uint8 flags
int32 lower_len (if present)
bytes lower_value
int32 upper_len (if present)
bytes upper_value
```

## 7) Network Types

```
INET/CIDR:
  uint8 family
  uint8 bits
  uint8 is_cidr
  uint8 reserved
  uint8 addr[4 or 16]

MACADDR:
  uint8 addr[6]

MACADDR8:
  uint8 addr[8]
```

## 8) TSVECTOR / TSQUERY

Binary formats defined in `types/BINARY_LAYOUT_ANNEX.md`.

## Related Specs

- `docs/specifications/parser/v3/types/BINARY_LAYOUT_ANNEX.md`
- `docs/specifications/parser/v3/storage/TOAST_LOB_STORAGE.md`
