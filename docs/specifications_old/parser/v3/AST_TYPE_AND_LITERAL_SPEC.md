# AST Type and Literal Specification (V3)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

Date: 2026-02-07
Status: Authoritative (V3)

This document defines the **AST structures** required to emit all V3 SBLR type markers and literal opcodes. It is the single source of truth for parser and analyzer implementations that produce V3 SBLR.

## 1. General Rules

1. **AST nodes must carry resolved catalog IDs** for all catalog-backed types (domain, enum, set, row/composite). Names may be kept for diagnostics but **SBLR emission uses IDs only**.
2. **UUID v7** is mandatory for all catalog identifiers.
3. **Type nodes are schema-neutral**, but their payloads must be sufficient to encode `TYPE_SPEC` in `SBLR_V3_OPCODE_PAYLOADS.md`.
4. **Literal nodes must be fully typed** before emission (no untyped literal payloads in SBLR).
5. Where a literal allows both **ordinal** and **label** (ENUM/SET), the emitter **must prefer ordinal** if resolved.

## 2. Type AST Nodes

### 2.1 Core Type Node

```text
TypeSpec
  - kind: TypeKind (enum)
  - precision: u32 (optional; 0 if not applicable)
  - scale: u32 (optional; 0 if not applicable)
  - element_type: TypeSpec? (required for ARRAY, SET)
  - with_timezone: bool (for TIME/TIMESTAMP/DATETIME)
  - timezone_hint: u16 (display TZ hint for TIME/TIMESTAMP TZ)
  - catalog_id: u128 (UUID v7; required for DOMAIN/ENUM/SET/ROW/COMPOSITE)
  - flags: u16 (type-specific)
  - srid: u32 (geometry)
  - format: u8 (geometry/jsonpath/year)
  - storage_mode: u8 (set/bit)
  - bit_length: u16 (bit/varbit)
  - fields: [FieldSpec] (for inline ROW/COMPOSITE)
```

**TypeKind** (subset for V3 additions):
- `UNKNOWN`
- `ENUM`, `SET`, `ROW`, `COMPOSITE`
- `GEOMETRY` (generic)
- `BIT`
- `YEAR`
- `DATETIME`
- `BLOB_TEXT`
- `JSONPATH`

### 2.2 ENUM Type

```text
TypeEnum
  - kind = ENUM
  - catalog_id: u128 (UUID v7)
  - storage_width: u8 (1/2/4)
  - flags: u16
      0x0001 ORDERED
      0x0002 WRAP_ALLOWED
```

**SBLR mapping**: `SBLR3_TYPE_ENUM` (0x0B0A) with payload per `TYPE_SPEC` rules.

### 2.3 SET Type

```text
TypeSet
  - kind = SET
  - catalog_id: u128 (UUID v7)
  - storage_mode: u8
      0 = ORDINAL_LIST
      1 = BITSET
  - element_type: TypeSpec (required)
```

**SBLR mapping**: `SBLR3_TYPE_SET` (0x0B0C) with payload per `TYPE_SPEC` rules.

### 2.4 ROW / COMPOSITE Type

```text
TypeRow
  - kind = ROW or COMPOSITE
  - catalog_id: u128 (UUID v7)
  - fields: [FieldSpec] (optional inline definition if catalog is not yet bound)

FieldSpec
  - name: string
  - type: TypeSpec
  - flags: u16
      0x0001 NULLABLE
      0x0002 HAS_DEFAULT
```

**SBLR mapping**: `SBLR3_TYPE_ROW` (0x0B11) or `SBLR3_TYPE_COMPOSITE` (0x0B02).

### 2.5 GEOMETRY Types

```text
TypeGeometry
  - kind = GEOMETRY (generic) or specific geometry kind (POINT, LINESTRING, POLYGON, MULTI*, GEOMETRYCOLLECTION)
  - srid: u32 (0 = unspecified)
  - format: u8
      0 = CANONICAL (ScratchBird canonical encoding)
      1 = WKB_COMPAT (PostGIS-style WKB)
```

**SBLR mapping**: `SBLR3_TYPE_GEOMETRY` (0x0B03) or specific geometry opcode.

### 2.6 BIT Type

```text
TypeBit
  - kind = BIT
  - bit_length: u16 (0 = unspecified)
  - storage_mode: u8
      0 = BITSTRING
      1 = VARBIT
```

**SBLR mapping**: `SBLR3_TYPE_BIT` (0x0B19).

### 2.7 YEAR Type

```text
TypeYear
  - kind = YEAR
  - format: u8
      0 = YEAR_2DIGIT
      1 = YEAR_4DIGIT
```

**SBLR mapping**: `SBLR3_TYPE_YEAR` (0x0B1B).

### 2.8 DATETIME Type

```text
TypeDateTime
  - kind = DATETIME
  - with_timezone: bool
  - precision: u8 (0-6 fractional seconds)
```

**SBLR mapping**: `SBLR3_TYPE_DATETIME` (0x0B1D).

### 2.9 BLOB_TEXT Type

```text
TypeBlobText
  - kind = BLOB_TEXT
  - charset_id: u16
  - collation_id: u16
  - flags: u16
      0x0001 TOAST_BACKED
```

**SBLR mapping**: `SBLR3_TYPE_BLOB_TEXT` (0x0B25).

### 2.10 JSONPATH Type

```text
TypeJsonPath
  - kind = JSONPATH
  - dialect: u8
      0 = PostgreSQL JSONPath
      1 = SQL/JSON
```

**SBLR mapping**: `SBLR3_TYPE_JSONPATH` (0x0B10).

## 3. Literal AST Nodes

### 3.1 ENUM Literal

```text
LiteralEnum
  - kind = ENUM
  - enum_catalog_id: u128
  - ordinal: u32 (preferred if resolved)
  - label: string (optional if ordinal present)
```

**SBLR mapping**: `SBLR3_LITERAL_ENUM` (0x0C1E) using `SCHEMA_LITERAL_ENUM`.

### 3.2 SET Literal

```text
LiteralSet
  - kind = SET
  - set_catalog_id: u128
  - elements: [LiteralEnum]
```

**SBLR mapping**: `SBLR3_LITERAL_SET` (0x0C1F) using `SCHEMA_LITERAL_SET`.

### 3.3 ROW Literal

```text
LiteralRow
  - kind = ROW
  - row_catalog_id: u128
  - fields: [RowFieldLiteral]

RowFieldLiteral
  - name: string
  - value: ValueSpec
```

**SBLR mapping**: `SBLR3_LITERAL_ROW` (0x0C20) using `SCHEMA_LITERAL_ROW`.

### 3.4 COMPOSITE Literal

```text
LiteralComposite
  - kind = COMPOSITE
  - composite_catalog_id: u128
  - fields: [RowFieldLiteral]
```

**SBLR mapping**: `SBLR3_LITERAL_COMPOSITE` (0x0C21) using `SCHEMA_LITERAL_COMPOSITE`.

### 3.5 DOMAIN Literal

```text
LiteralDomain
  - kind = DOMAIN
  - domain_id: u128
  - value: ValueSpec (encoded in base type)
```

**SBLR mapping**: `SBLR3_LITERAL_DOMAIN` (0x0C22) using `SCHEMA_LITERAL_DOMAIN`.

### 3.6 BIT Literal

```text
LiteralBit
  - bit_length: u16
  - bytes: byte[] (big-endian bitstring)
```

**SBLR mapping**: `SBLR3_LITERAL_BIT` (0x0C18) using `SCHEMA_LITERAL_BITS`.

### 3.7 YEAR Literal

```text
LiteralYear
  - value: i32
  - format: u8 (YEAR_2DIGIT or YEAR_4DIGIT)
```

**SBLR mapping**: `SBLR3_LITERAL_YEAR` (0x0C19).

### 3.8 DATETIME Literal

```text
LiteralDateTime
  - value: timestamp (microseconds since epoch)
  - with_timezone: bool
  - precision: u8
```

**SBLR mapping**: `SBLR3_LITERAL_DATETIME` (0x0C1A).

### 3.9 MEDIUMINT Literal

```text
LiteralMediumInt
  - value: i32 (range constrained to -8,388,608..8,388,607)
```

**SBLR mapping**: `SBLR3_LITERAL_MEDIUMINT` (0x0C1B).

### 3.10 GEOMETRY Literal

```text
LiteralGeometry
  - format: u8 (CANONICAL or WKB_COMPAT)
  - srid: u32
  - bytes: byte[]
```

**SBLR mapping**: `SBLR3_LITERAL_GEOMETRY` (0x0C1C) using `SCHEMA_LITERAL_GEOMETRY`.

### 3.11 JSONPATH Literal

```text
LiteralJsonPath
  - dialect: u8
  - text: string
```

**SBLR mapping**: `SBLR3_LITERAL_JSONPATH` (0x0C1D).

### 3.12 INT8 Literal

```text
LiteralInt8
  - value: i8
```

**SBLR mapping**: `SBLR3_LITERAL_INT8` (0x0C23).

### 3.13 INT16 Literal

```text
LiteralInt16
  - value: i16
```

**SBLR mapping**: `SBLR3_LITERAL_INT16` (0x0C24).

### 3.14 UINT8 Literal

```text
LiteralUInt8
  - value: u8
```

**SBLR mapping**: `SBLR3_LITERAL_UINT8` (0x0C25).

### 3.15 UINT16 Literal

```text
LiteralUInt16
  - value: u16
```

**SBLR mapping**: `SBLR3_LITERAL_UINT16` (0x0C26).

### 3.16 UINT32 Literal

```text
LiteralUInt32
  - value: u32
```

**SBLR mapping**: `SBLR3_LITERAL_UINT32` (0x0C27).

### 3.17 UINT64 Literal

```text
LiteralUInt64
  - value: u64
```

**SBLR mapping**: `SBLR3_LITERAL_UINT64` (0x0C28).

### 3.18 UINT128 Literal

```text
LiteralUInt128
  - value: u128
```

**SBLR mapping**: `SBLR3_LITERAL_UINT128` (0x0C29).

### 3.19 INT128 Literal

```text
LiteralInt128
  - value: i128
```

**SBLR mapping**: `SBLR3_LITERAL_INT128` (0x0C2A).

### 3.20 FLOAT32 Literal

```text
LiteralFloat32
  - value: f32
```

**SBLR mapping**: `SBLR3_LITERAL_FLOAT32` (0x0C2B).

### 3.21 TIME WITH TIME ZONE Literal

```text
LiteralTimeTz
  - time_usec: i64 (microseconds since midnight)
  - tz_offset_minutes: i16
  - tz_name: string (optional; canonical per `types/CANONICALIZATION_RULES.md`)
```

**SBLR mapping**: `SBLR3_LITERAL_TIME_TZ` (0x0C2C).

### 3.22 TIMESTAMP WITH TIME ZONE Literal

```text
LiteralTimestampTz
  - epoch_usec: i64 (UTC, microseconds since epoch)
  - tz_offset_minutes: i16
  - tz_name: string (optional; canonical per `types/CANONICALIZATION_RULES.md`)
```

**SBLR mapping**: `SBLR3_LITERAL_TIMESTAMP_TZ` (0x0C2D).

### 3.23 RANGE Literal

```text
LiteralRange
  - range_base_type: TypeSpec
  - flags: u8 (lower_inc, upper_inc, lower_inf, upper_inf, empty)
  - lower_present: bool
  - upper_present: bool
  - lower: ValueSpec (if lower_present)
  - upper: ValueSpec (if upper_present)
```

**SBLR mapping**: `SBLR3_LITERAL_RANGE` (0x0C2E).

### 3.24 ARRAY Literal

```text
LiteralArray
  - element_type: TypeSpec
  - dimensions: u8
  - dim_lengths: [u32]
  - elements: [ValueSpec] (row-major order)
```

**SBLR mapping**: `SBLR3_LITERAL_ARRAY` (0x0C2F).

### 3.25 VARIANT Literal

```text
LiteralVariant
  - variant_type_id: u128
  - tag_name: string
  - value: ValueSpec
```

**SBLR mapping**: `SBLR3_LITERAL_VARIANT` (0x0C30).

### 3.26 TSVECTOR Literal

```text
LiteralTsVector
  - text: string (canonical tsvector; see `types/CANONICALIZATION_RULES.md`)
```

**SBLR mapping**: `SBLR3_LITERAL_TSVECTOR` (0x0C31).

### 3.27 TSQUERY Literal

```text
LiteralTsQuery
  - text: string (canonical tsquery; see `types/CANONICALIZATION_RULES.md`)
```

**SBLR mapping**: `SBLR3_LITERAL_TSQUERY` (0x0C32).

### 3.28 BLOB LOCATOR Literal

```text
LiteralBlobLocator
  - blob_id: u128
  - blob_subtype: i16
  - blob_length: u64
  - compression: u8
```

**SBLR mapping**: `SBLR3_LITERAL_BLOB_LOCATOR` (0x0C33).

## 4. ValueSpec Extensions

ValueSpec must be able to carry **typed literals** for:
- ENUM, SET, ROW, COMPOSITE, DOMAIN
- BIT, YEAR, DATETIME, MEDIUMINT, GEOMETRY, JSONPATH
- INT8/INT16/INT128, UINT8/UINT16/UINT32/UINT64/UINT128, FLOAT32
- TIME_TZ, TIMESTAMP_TZ, RANGE, ARRAY, VARIANT, TSVECTOR, TSQUERY, BLOB_LOCATOR

```text
ValueSpec
  - type: TypeSpec
  - literal: (one of the Literal* nodes)
```

## 5. Examples

### ENUM Literal Example
```sql
DECLARE @status order_status = 'SUBMITTED';
```
AST:
```text
LiteralEnum
  enum_catalog_id = <uuid-v7>
  ordinal = 1
  label = "SUBMITTED"
```

### SET Literal Example
```sql
DECLARE @tags tag_set = SET['sql','design'];
```
AST:
```text
LiteralSet
  set_catalog_id = <uuid-v7>
  elements = [LiteralEnum('sql'), LiteralEnum('design')]
```

### ROW Literal Example
```sql
INSERT INTO t (address) VALUES (ROW('123 Main', 'Apt 1', 'CA'));
```
AST:
```text
LiteralRow
  row_catalog_id = <uuid-v7>
  fields = [
    {name:'street', value:'123 Main'},
    {name:'unit', value:'Apt 1'},
    {name:'state', value:'CA'}
  ]
```

### DOMAIN Literal Example
```sql
INSERT INTO t (email) VALUES (DOMAIN email_address 'a@b.com');
```
AST:
```text
LiteralDomain
  domain_id = <uuid-v7>
  value = 'a@b.com'
```

### GEOMETRY Literal Example
```sql
INSERT INTO geo (shape) VALUES (ST_GeomFromText('POINT(1 2)', 4326));
```
AST:
```text
LiteralGeometry
  format = WKB_COMPAT
  srid = 4326
  bytes = <WKB bytes>
```

## 6. Required Validations

1. **Catalog-backed literals** must resolve to valid IDs before SBLR emission.
2. **ENUM/SET literals** must validate ordinals against catalog length.
3. **ROW/COMPOSITE literals** must match field count and type constraints.
4. **DOMAIN literals** must run domain constraint validation (pre-exec check or runtime policy).
5. **BIT literals** must validate bit_length against payload.
6. **YEAR/DATETIME literals** must validate format and precision.
7. **GEOMETRY literals** must validate format and SRID bounds (if enforced).
8. **JSONPATH** must validate dialect before emission.
