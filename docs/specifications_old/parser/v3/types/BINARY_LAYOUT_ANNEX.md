# Binary Layout Annex (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: consolidate all binary layouts referenced by VALUE_SPEC and storage
encoding rules into a single authoritative annex under V3.

## 1) JSONB Binary Layout

```
uint32 length
bytes[length] jsonb_payload
```

JSONB payload is PostgreSQL-compatible and follows `jsonb.h` layout rules.

## 2) TSVECTOR / TSQUERY

TSVECTOR:
```
uint8  version
uint8  flags
uint16 num_lexemes
repeat num_lexemes:
  uint8 lexeme_len
  bytes lexeme
  uint8 weight (0..4)
  uint16 num_positions
  uint16 positions[num_positions]
```

TSQUERY:
```
uint8  version
uint8  flags
uint16 num_nodes
preorder traversal:
  uint8 node_type
  if LEXEME:
    uint8 lexeme_len
    bytes lexeme
    uint8 weight
    uint8 prefix_flag
  if PHRASE/PROXIMITY:
    uint16 distance
```

## 3) INET / CIDR / MACADDR / MACADDR8

```
INET/CIDR:
  uint8 family (AF_INET=2, AF_INET6=10)
  uint8 bits
  uint8 is_cidr
  uint8 reserved
  uint8 addr[4 or 16]

MACADDR:
  uint8 addr[6]

MACADDR8:
  uint8 addr[8]
```

## 4) Arrays

```
int32 ndim
int32 flags
uint32 elem_oid
int32 dims[ndim]
int32 lower_bounds[ndim]
optional null bitmap
element data (row-major)
```

Elements use canonical encodings. Variable-length elements are prefixed with
`int32 len` (-1 = NULL).

## 5) Ranges

```
uint8 flags
if lower bound present:
  int32 lower_len
  lower_value bytes
if upper bound present:
  int32 upper_len
  upper_value bytes
```

Flags:
```
RANGE_EMPTY   = 0x01
RANGE_LB_INC  = 0x02
RANGE_UB_INC  = 0x04
RANGE_LB_INF  = 0x08
RANGE_UB_INF  = 0x10
```

## 6) Composite / Row / Variant

```
int32 column_count
repeat column_count:
  int32 column_type_oid
  int32 column_length   // -1 = NULL
  column_data bytes
```

Variant is a single-element composite.

## 7) Geometry

```
uint8 format   // 0 = canonical, 1 = WKB
uint32 srid
uint32 len
bytes[len]
```

## 8) BIT / VARBIT

```
int32 nbits
bytes[(nbits+7)/8] packed bits
```
