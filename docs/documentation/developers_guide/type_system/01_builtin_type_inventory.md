# Built-in Type System

[Type System README](../README.md)

## Synopsis

Complete reference of ScratchBird data types.

## Numeric Types

### Integer Types

| Type | Size | Range | Description |
|------|------|-------|-------------|
| `SMALLINT` | 2 bytes | -32,768 to 32,767 | Small integer |
| `INTEGER` / `INT` | 4 bytes | -2^31 to 2^31-1 | Standard integer |
| `BIGINT` | 8 bytes | -2^63 to 2^63-1 | Large integer |

### Arbitrary Precision

| Type | Description | Example |
|------|-------------|---------|
| `NUMERIC(p, s)` | Exact decimal | `NUMERIC(10, 2)` - 10 digits, 2 decimal |
| `DECIMAL(p, s)` | Same as NUMERIC | - |

### Floating Point

| Type | Size | Precision | Description |
|------|------|-----------|-------------|
| `REAL` | 4 bytes | ~6 digits | Single precision |
| `DOUBLE PRECISION` | 8 bytes | ~15 digits | Double precision |
| `FLOAT(n)` | variable | n digits | Floating point |

### Serial Types

| Type | Description |
|------|-------------|
| `SMALLSERIAL` | Auto-incrementing smallint |
| `SERIAL` | Auto-incrementing integer |
| `BIGSERIAL` | Auto-incrementing bigint |

## Character Types

| Type | Description | Max Size |
|------|-------------|----------|
| `CHAR(n)` | Fixed-length, padded | 1 GB |
| `VARCHAR(n)` | Variable-length with limit | 1 GB |
| `TEXT` | Variable-length unlimited | 1 GB |
| `CHARACTER(n)` | Same as CHAR(n) | - |
| `CHARACTER VARYING(n)` | Same as VARCHAR(n) | - |

Storage: All use same underlying storage (TOAST for large values).

## Binary Data

| Type | Description |
|------|-------------|
| `BYTEA` | Variable-length binary |

## Boolean

| Type | Values |
|------|--------|
| `BOOLEAN` / `BOOL` | `TRUE`, `FALSE`, `NULL` |

## Temporal Types

### Date/Time

| Type | Description | Range |
|------|-------------|-------|
| `DATE` | Date only | 4713 BC - 294276 AD |
| `TIME` | Time without zone | 00:00:00 - 24:00:00 |
| `TIMETZ` | Time with time zone | - |
| `TIMESTAMP` | Date and time | 4713 BC - 294276 AD |
| `TIMESTAMPTZ` | Timestamp with zone | - |
| `INTERVAL` | Time span | -178M - 178M years |

## UUID

| Type | Description |
|------|-------------|
| `UUID` | 128-bit UUID v7 |

## JSON Types

| Type | Description | Storage |
|------|-------------|---------|
| `JSON` | Text JSON | Raw text |
| `JSONB` | Binary JSON | Parsed + indexed |

Prefer JSONB for:
- Indexing
- Containment queries
- Processing

## Geometric Types

| Type | Description | Representation |
|------|-------------|----------------|
| `POINT` | Point | (x, y) |
| `LINE` | Infinite line | {A, B, C} |
| `LSEG` | Line segment | ((x1,y1), (x2,y2)) |
| `BOX` | Rectangle | ((x1,y1), (x2,y2)) |
| `PATH` | Path | ((x1,y1), ...) |
| `POLYGON` | Polygon | ((x1,y1), ...) |
| `CIRCLE` | Circle | <(x,y), r> |

## Network Types

| Type | Description |
|------|-------------|
| `CIDR` | IP network |
| `INET` | IP host + network |
| `MACADDR` | MAC address |
| `MACADDR8` | MAC address (EUI-64) |

## Text Search

| Type | Description |
|------|-------------|
| `TSVECTOR` | Optimized text search doc |
| `TSQUERY` | Text search query |

## Range Types

| Type | Description |
|------|-------------|
| `INT4RANGE` | Range of integers |
| `INT8RANGE` | Range of bigints |
| `NUMRANGE` | Range of numerics |
| `TSRANGE` | Range of timestamps |
| `TSTZRANGE` | Range of timestamptz |
| `DATERANGE` | Range of dates |

## Arrays

Any type can be an array:
- `INTEGER[]` - Array of integers
- `TEXT[][]` - 2D array of text
- `INTEGER[3]` - Fixed-length array

## Special Types

| Type | Description |
|------|-------------|
| `OID` | Object identifier |
| `REGPROC` | Procedure name |
| `REGCLASS` | Table name |
| `REGTYPE` | Type name |
| `PG_LSN` | Log sequence number |
| `XID` | Transaction ID |
| `CID` | Command ID |

## See Also

- [Numeric Types](02_numeric_types.md)
- [Character Types](03_character_and_text_types.md)
- [Temporal Types](05_temporal_types.md)
- [JSON Types](07_json_document_and_semistructured_types.md)
