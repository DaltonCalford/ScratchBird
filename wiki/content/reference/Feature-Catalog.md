# ScratchBird Feature Catalog

**Last Updated:** 2026-02-03

---

This catalog summarizes the major feature surfaces in ScratchBird. It is designed
as a single, self‑contained reference for release readiness and learning.

---

## Dialects and Protocols

- Native ScratchBird (V2) SQL parser + SBLR bytecode.
- Emulated SQL parsers: Firebird 5, PostgreSQL 16, MySQL 8.0.
- Wire protocols: ScratchBird native, Firebird, PostgreSQL, MySQL.
- IPC contract: parser <-> server via SBWP IPC protocol.

---

## Schema Tree (Recursive/Emulated Layout)

ScratchBird separates **database** (physical storage root) from **schema**
(logical namespace). Schemas are recursive and can nest under other schemas.

- **Database**: physical storage root (files/tablespaces and catalog).
- **Schema**: logical namespace; schemas can contain subschemas and objects.
- **Authoritative catalog**: `sys.*` (native ScratchBird system catalog).
- **SQL standard views**: `information_schema.*`.
- **Emulated catalog surfaces** (views mapped to `sys.*`):
  - PostgreSQL: `pg_catalog.*` and `pg_*`.
  - MySQL: `mysql.*` and `information_schema.*`.
  - Firebird: `rdb$*` system tables + `mon$*` views.
- **Emulated database tree** (per server + database name):
  - `/<dialect>/<server>/<db>/...` mapped to ScratchBird storage objects.
  - Emulated parsers never create physical files; only the native engine creates
    on‑disk objects.

---

## SQL Objects

- Database, schema, and tablespace.
- Table (temporary, unlogged).
- View and materialized view.
- Indexes (core and advanced types; see Indexes section).
- Sequences and identity columns.
- Domain and type (enum, composite/record, set, variant).
- Function, procedure, trigger (row/statement), package.
- Role, user, privileges, policies (RLS/CLS).
- Job scheduler objects (job, job run, dependencies).
- Foreign data wrappers and UDR connectors.
- Partitioned tables (native V2; emulated parsers follow source engine rules).
- Security objects: certificate, key, auth provider registrations.

---

## Data Types

### Numeric
- Integers: SMALLINT, INTEGER, BIGINT, INT128.
- Unsigned integers: UINT8, UINT16, UINT32, UINT64.
- Fixed‑point: DECIMAL(p,s), NUMERIC(p,s), MONEY.
- IEEE decimal: DECFLOAT(16), DECFLOAT(34).
- Floating‑point: REAL, DOUBLE PRECISION.

### Character and Binary
- CHAR(n), VARCHAR(n), TEXT.
- BYTEA, BINARY(n), BLOB.
- Character sets and collations (I18N catalogs).

### Date/Time
- DATE, TIME, TIMESTAMP.
- TIME WITH TIME ZONE, TIMESTAMP WITH TIME ZONE.
- INTERVAL.

### Boolean and Special
- BOOLEAN.
- UUID (v4, v7, v8).
- JSON, JSONB (canonical text storage in Alpha).
- XML.

### Arrays, Composite, and Advanced
- Arrays: `datatype[]` (multi‑dimensional).
- Composite/record types (row types).
- Enum domains, set domains, variant domains.
- Range types (PostgreSQL‑compatible ranges).
- Network types: INET, CIDR, MACADDR, MACADDR8.
- Geometric types: POINT, LINE, LINESTRING, POLYGON, CIRCLE, MULTI*.
- Vector type for AI/ML search (HNSW/IVF indexes).

---

## Operators (Core Surface)

### Arithmetic
`+`, `-`, `*`, `/`, `%`, `^`, `**`, `DIV`

### Comparison and Predicates
`=`, `<>`, `!=`, `<`, `<=`, `>`, `>=`
`IS NULL`, `IS NOT NULL`, `IS DISTINCT FROM`
`IS TRUE`, `IS FALSE`, `IS UNKNOWN` (with optional `NOT`)
`BETWEEN`, `NOT BETWEEN`
`IN (...)`, `NOT IN (...)`
`EXISTS (...)`, `NOT EXISTS (...)`
`LIKE`, `ILIKE`, `SIMILAR TO`
`STARTING WITH`, `CONTAINING`
`REGEXP`
Quantified: `<op> ALL/ANY/SOME (subquery)`

### Logical
`AND`, `OR`, `NOT`

### String
`||`, `CONCAT`

### Bitwise
`&`, `|`, `#`, `~`, `<<`, `>>`

### Array/Range/JSON/Text Search
- Array subscripts: `[]`
- Array/range containment and overlap: `@>`, `<@`, `&&`
- JSON path: `->`, `->>`, `#>`, `#>>`
- JSON key existence: `?`, `?|`, `?&`
- Full‑text match: `@@`

---

## Functions (Native V2 Core)

### Aggregates
`COUNT`, `SUM`, `AVG`, `MIN`, `MAX`, `ARRAY_AGG`, `STRING_AGG`,
`STDDEV`, `STDDEV_POP`, `STDDEV_SAMP`, `VAR_POP`, `VAR_SAMP`, `VARIANCE`,
`CORR`, `COVAR_POP`, `COVAR_SAMP`, `REGR_AVGX`, `REGR_AVGY`, `REGR_COUNT`,
`REGR_INTERCEPT`, `REGR_R2`, `REGR_SLOPE`, `REGR_SXX`, `REGR_SXY`, `REGR_SYY`

### Window
`ROW_NUMBER`, `RANK`, `DENSE_RANK`, `LAG`, `LEAD`, `FIRST_VALUE`,
`LAST_VALUE`, `NTH_VALUE`, `NTILE`, `PERCENT_RANK`, `CUME_DIST`

### String
`LTRIM`, `RTRIM`, `TRIM`, `CONCAT`, `CONCAT_WS`, `SUBSTRING`, `POSITION`,
`LENGTH`, `CHAR_LENGTH`, `LOWER`, `UPPER`, `REPLACE`, `STARTS_WITH`, `ENDS_WITH`,
`CONVERT`

### Numeric
`ABS`, `CEIL`, `FLOOR`, `ROUND`, `POWER`, `SQRT`, `LOG`, `EXP`, `MOD`,
`SIGN`, `LEAST`, `GREATEST`

### Date/Time
`NOW`, `CURRENT_DATE`, `CURRENT_TIME`, `CURRENT_TIMESTAMP`, `DATE_ADD`,
`DATE_SUB`, `DATE_DIFF`, `DATE_TRUNC`, `EXTRACT`, `TO_CHAR`, `TO_DATE`,
`TO_TIMESTAMP`, `AT TIME ZONE`

### JSON/JSONB
`JSON_EXTRACT`, `JSON_ARRAY`, `JSON_OBJECT`, `JSON_TABLE`, `JSON_QUERY`,
`JSON_VALUE`, `JSON_EXISTS`, `JSON_HAS_KEY`, `JSONB_*`

### Array
`ARRAY_AGG`, `ARRAY_APPEND`, `ARRAY_PREPEND`, `ARRAY_CAT`, `ARRAY_DIMS`,
`ARRAY_FILL`, `ARRAY_LENGTH`, `ARRAY_LOWER`, `ARRAY_UPPER`, `ARRAY_NDIMS`,
`ARRAY_POSITION`, `ARRAY_POSITIONS`, `ARRAY_REMOVE`, `ARRAY_REPLACE`,
`ARRAY_SLICE`, `ARRAY_TO_STRING`, `CARDINALITY`, `STRING_TO_ARRAY`,
`TRIM_ARRAY`, `UNNEST`

### Spatial
`ST_POINT`, `ST_MAKELINE`, `ST_MAKEPOLYGON`, `ST_ASTEXT`, `ST_ASBINARY`,
`ST_GEOMETRYTYPE`, `ST_ISVALID`, `ST_Multi`, `ST_Collect`, `ST_NumGeometries`,
`ST_GeometryN`, `ST_IsEmpty`, `ST_IsSimple`, `ST_IsClosed`, `ST_Dump`,
`ST_CollectionExtract`, `ST_Union`

### System
`CURRENT_USER`, `CURRENT_ROLE`, `CURRENT_SCHEMA`, `SESSION_USER`, `VERSION`

### Control Flow
`CASE`, `COALESCE`, `NULLIF`

### UUID
`GEN_UUID_V1`, `GEN_UUID_V4`, `GEN_UUID_V7`, `UUID_TO_STRING`

### Text Search
`TO_TSVECTOR`, `TO_TSQUERY`, `PLAINTO_TSQUERY`, `PHRASETO_TSQUERY`

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
