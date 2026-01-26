# EXTRACT and ALTER_ELEMENT (Component Access and Update)

Status: Draft (Target). This specification defines the component model used by EXTRACT and ALTER_ELEMENT so every data type has a clear list of extractable elements and alterable elements.

## Goals
- Provide a single, consistent component vocabulary across all data types.
- Define EXTRACT elements and ALTER_ELEMENT targets in one place.
- Allow element access for complex types (arrays, composite, JSON, network, spatial, ranges).
- Ensure altered values are validated and normalized consistently (especially for temporal types).

## Scope
This spec covers the EXTRACT and ALTER_ELEMENT expressions only. It does not define SQL dot notation, JSON operators, or dialect-specific function aliases, though it may reference them as synonyms.

## Syntax

### Canonical forms
```
EXTRACT ( <element-selector> FROM <value-expr> )
ALTER_ELEMENT ( <element-selector> IN <value-expr> TO <new-value-expr> )
```

### Element selector
```
<element-selector> ::= <identifier> [ '(' <element-arg-list> ')' ]
                     | <string-literal>
                     | <integer-expr>

<element-arg-list> ::= <expr> ( ',' <expr> )*
```

Notes:
- ALTER_ELEMENT is a single token keyword (not `ALTER ELEMENT`).
- `<identifier>` is case-insensitive. Canonical element names are uppercase.
- `<string-literal>` is interpreted by type (for example, JSON/XML paths).
- `<integer-expr>` is a shorthand for ELEMENT(index) when the value is an ARRAY.

### Shorthand equivalences
- ARRAY: `EXTRACT(3 FROM arr)` == `EXTRACT(ELEMENT(3) FROM arr)`.
- COMPOSITE: `EXTRACT(city FROM addr)` == `EXTRACT(FIELD('city') FROM addr)`.
- JSON/XML: `EXTRACT('$.a[0]' FROM doc)` == `EXTRACT(PATH('$.a[0]') FROM doc)`.

## Evaluation rules
- EXTRACT is strict: if `<value-expr>` is NULL, the result is NULL.
- ALTER_ELEMENT is semi-strict:
  - If `<value-expr>` is NULL, the result is NULL.
  - If `<new-value-expr>` is NULL, behavior is element-specific; see element definitions.
- Element names not defined for a type raise an error.

## Type conversion rules
- For EXTRACT, the element type determines the return type.
- For ALTER_ELEMENT, `<new-value-expr>` is cast to the element type before assignment.
- If cast or validation fails, an error is raised (INVALID_TEXT_REPRESENTATION,
  DATETIME_FIELD_OVERFLOW, STRING_DATA_RIGHT_TRUNCATION, NUMERIC_VALUE_OUT_OF_RANGE,
  or DATATYPE_MISMATCH as appropriate).

## Temporal semantics
- DATE, TIME, TIMESTAMP values are stored as UTC plus a stored timezone offset.
- ALTER_ELEMENT for temporal types operates on local time:
  1) Convert stored UTC to local by applying the stored offset.
  2) Replace the targeted element.
  3) Validate ranges (calendar-aware for DATE/TIMESTAMP).
  4) Convert back to UTC and store the same offset (unless the offset was altered).
- TIMEZONE_HOUR, TIMEZONE_MINUTE, and TZ_OFFSET are alterable. When altered, the
  local wall time is preserved and the stored UTC value is adjusted accordingly.
- DATE uses `server.time.date_default_time` (default 00:00:00) as the local
  time-of-day for normalization.

## Element catalog (canonical names)

Legend:
- [R] read-only (EXTRACT only)
- [RW] read/write (EXTRACT + ALTER_ELEMENT)

### Matrix: Extractable Elements by Type
This matrix summarizes extractable element names per type family. Use the
sections below for full semantics and validation rules.

| Type(s) | Extractable elements (R/RW) |
| --- | --- |
| UNKNOWN | VALUE [R] |
| NULL_TYPE | VALUE [R] |
| INT8/16/32/64/128, UINT8/16/32/64/128 | VALUE [RW], SIGN [R], ABS [R], BYTES [R], BITS [R], HI64 [R]*, LO64 [R]* |
| FLOAT32/FLOAT64 | VALUE [RW], SIGN [R], EXPONENT [R], MANTISSA [R], IS_NAN [R], IS_INF [R] |
| DECIMAL | VALUE [RW], PRECISION [R], SCALE [R], UNSCALED [RW], SIGN [R] |
| MONEY | VALUE [RW], SCALE [R], MAJOR [R], MINOR [R], SIGN [R] |
| BOOLEAN | VALUE [RW] |
| CHAR/VARCHAR/TEXT | VALUE [RW], CHAR_LENGTH [R], OCTET_LENGTH [R], CODEPOINT_LENGTH [R], TRIMMED_LENGTH [R]* |
| JSON/JSONB | VALUE [RW], PATH(path) [RW], TYPE [R], KEYS [R] |
| XML | VALUE [RW], PATH(xpath) [RW], ATTRIBUTES [R] |
| BINARY/VARBINARY/BLOB/BYTEA | VALUE [RW], LENGTH [R], BYTE(index) [RW], BIT(index) [RW], SLICE(start,length) [RW], DIGEST(algorithm) [R] |
| VECTOR | VALUE [RW], DIMENSION [R], ELEMENT(index) [RW], NORM_L2 [R], DOT(other_vector) [R] |
| DATE | VALUE [RW], YEAR [RW], MONTH [RW], DAY [RW], DAYOFMONTH [R], DOW [R], WEEKDAY [R], DOY [R], DAYOFYEAR [R], QUARTER [R], WEEK [R], ISO_WEEK [R], ISO_YEAR [R], ISO_DOW [R], CENTURY [R], DECADE [R], MILLENNIUM [R], EPOCH [R], TIMEZONE [RW], TIMEZONE_HOUR [RW], TIMEZONE_MINUTE [RW], TZ_OFFSET [RW] |
| TIME | VALUE [RW], HOUR/HOUR24 [RW], HOUR12 [R], MINUTE [RW], SECOND [RW], MILLISECOND [RW], MICROSECOND [RW], EPOCH [R], TIMEZONE [RW], TIMEZONE_HOUR [RW], TIMEZONE_MINUTE [RW], TZ_OFFSET [RW] |
| TIMESTAMP | VALUE [RW], YEAR [RW], MONTH [RW], DAY [RW], HOUR [RW], MINUTE [RW], SECOND [RW], MILLISECOND [RW], MICROSECOND [RW], DOW [R], DOY [R], QUARTER [R], WEEK [R], ISO_WEEK [R], ISO_YEAR [R], ISO_DOW [R], CENTURY [R], DECADE [R], MILLENNIUM [R], EPOCH [R], TIMEZONE [RW], TIMEZONE_HOUR [RW], TIMEZONE_MINUTE [RW], TZ_OFFSET [RW] |
| INTERVAL | VALUE [RW], YEAR [RW], MONTH [RW], DAY [RW], HOUR [RW], MINUTE [RW], SECOND [RW], MILLISECOND [RW], MICROSECOND [RW], TOTAL_MONTHS [R], TOTAL_DAYS [R], TOTAL_SECONDS [R], EPOCH [R], SIGN [R] |
| UUID | VALUE [RW], BYTES [RW], VERSION [R], VARIANT [R], TIMESTAMP [R], CLOCK_SEQ [R], NODE [R], TIME_LOW [R], TIME_MID [R], TIME_HIGH [R], RAND_A [R], RAND_B [R] |
| Spatial (POINT/LINESTRING/POLYGON/MULTI*/GEOMETRYCOLLECTION) | VALUE [RW], SRID [RW], BBOX [R]; POINT: X [RW], Y [RW]; LINESTRING: POINTS [R], NUM_POINTS [R], LENGTH [R]; POLYGON: RINGS [R], NUM_RINGS [R], AREA [R]; MULTI*/GEOMETRYCOLLECTION: GEOMETRIES [R], NUM_GEOMETRIES [R] |
| ARRAY | VALUE [RW], ELEMENT(index) [RW], CARDINALITY [R], NDIMS [R], LOWER(dim) [R], UPPER(dim) [R], DIMS [R] |
| COMPOSITE | VALUE [RW], FIELD(name_or_index) [RW], FIELD_NAMES [R], CARDINALITY [R] |
| VARIANT | VALUE [RW], DATATYPE [RW] |
| TSVECTOR | VALUE [RW], LEXEMES [R], POSITIONS [R], WEIGHTS [R], SIZE [R], HAS_LEXEME(term) [R] |
| TSQUERY | VALUE [RW], ROOT_OP [R], TERMS [R], OPERATORS [R], PHRASE_DISTANCE [R], NODES [R] |
| RANGE (INT4/INT8/NUM/TS/TSTZ/DATE) | VALUE [RW], LOWER [RW], UPPER [RW], LOWER_INC [RW], UPPER_INC [RW], IS_EMPTY [RW], IS_LOWER_INFINITE [RW], IS_UPPER_INFINITE [RW] |
| INET | VALUE [RW], FAMILY [R], NETMASK [RW], ADDRESS [RW], NETWORK [R], BROADCAST [R], NETMASK_ADDR [R], HOSTMASK [R], IS_IPV4 [R], IS_IPV6 [R] |
| CIDR | VALUE [RW], FAMILY [R], NETMASK [RW], ADDRESS [RW], NETWORK [R], NETMASK_ADDR [R], HOSTMASK [R], IS_IPV4 [R], IS_IPV6 [R] |
| MACADDR/MACADDR8 | VALUE [RW], BYTES [RW], OUI [RW], NIC [RW], IS_MULTICAST [R], IS_LOCAL [R], TRUNC [R] |

Notes:
- * HI64/LO64 are for 128-bit integers only.
- * TRIMMED_LENGTH applies to CHAR only.

### Meta and NULL
#### UNKNOWN
- VALUE [R] -> UNKNOWN. (Reserved; not persisted.)

#### NULL_TYPE
- VALUE [R] -> NULL.

### Numeric types
Applies to INT8/INT16/INT32/INT64/INT128/UINT8/UINT16/UINT32/UINT64/UINT128.
- VALUE [RW] -> same type.
- SIGN [R] -> INT8 (-1, 0, 1).
- ABS [R] -> same type.
- BYTES [R] -> INT16 (storage width).
- BITS [R] -> INT16 (storage width * 8).
- HI64 [R] -> UINT64 (INT128/UINT128 only, high 64 bits).
- LO64 [R] -> UINT64 (INT128/UINT128 only, low 64 bits).

#### FLOAT32 / FLOAT64
- VALUE [RW] -> same type.
- SIGN [R] -> INT8 (-1, 0, 1).
- EXPONENT [R] -> INT32.
- MANTISSA [R] -> INT64.
- IS_NAN [R] -> BOOLEAN.
- IS_INF [R] -> BOOLEAN.

#### DECIMAL
- VALUE [RW] -> DECIMAL.
- PRECISION [R] -> INT16.
- SCALE [R] -> INT16.
- UNSCALED [RW] -> INT128 (sets the stored unscaled value; precision/scale unchanged).
- SIGN [R] -> INT8 (-1, 0, 1).

#### MONEY
- VALUE [RW] -> MONEY.
- SCALE [R] -> INT8 (always 4).
- MAJOR [R] -> INT64 (value / 10000).
- MINOR [R] -> INT32 (abs(value) % 10000).
- SIGN [R] -> INT8 (-1, 0, 1).

#### BOOLEAN
- VALUE [RW] -> BOOLEAN.

### String and document types
#### CHAR / VARCHAR / TEXT
- VALUE [RW] -> same type.
- CHAR_LENGTH [R] -> INT32 (character count).
- OCTET_LENGTH [R] -> INT32 (byte length).
- CODEPOINT_LENGTH [R] -> INT32 (Unicode codepoints; ASCII == CHAR_LENGTH).
- TRIMMED_LENGTH [R] -> INT32 (CHAR only, right-trim spaces then count).

#### JSON / JSONB
- VALUE [RW] -> same type.
- PATH(path) [RW] -> JSON/JSONB (extract or replace at JSON path).
- TYPE [R] -> TEXT (object, array, string, number, boolean, null).
- KEYS [R] -> ARRAY(TEXT) (root object keys).

Notes:
- PATH uses a JSONPath-like syntax (e.g., $.a[0].b).
- If PATH is not found, EXTRACT returns NULL; ALTER_ELEMENT creates the path
  if possible, otherwise raises an error.

#### XML
- VALUE [RW] -> XML.
- PATH(xpath) [RW] -> XML (extract or replace nodes at XPath).
- ATTRIBUTES [R] -> ARRAY(TEXT) (root element attributes).

### Binary and vector types
#### BINARY / VARBINARY / BLOB / BYTEA
- VALUE [RW] -> same type.
- LENGTH [R] -> INT32 (byte length).
- BYTE(index) [RW] -> UINT8 (0-based byte index).
- BIT(index) [RW] -> UINT8 (0 or 1; 0-based bit index).
- SLICE(start, length) [RW] -> same type (replace the slice).
- DIGEST(algorithm) [R] -> BINARY (algorithm-dependent length).

#### VECTOR
- VALUE [RW] -> VECTOR.
- DIMENSION [R] -> INT32 (number of float32 elements).
- ELEMENT(index) [RW] -> FLOAT32 (1-based index).
- NORM_L2 [R] -> FLOAT64.
- DOT(other_vector) [R] -> FLOAT64.

### Temporal types
#### DATE
- VALUE [RW] -> DATE.
- YEAR [RW] -> INT32.
- MONTH [RW] -> INT32 (1-12).
- DAY [RW] -> INT32 (1-31, calendar-validated).
- DAYOFMONTH [R] -> INT32 (alias of DAY).
- DOW [R] -> INT32 (0=Sunday .. 6=Saturday).
- WEEKDAY [R] -> INT32 (alias of DOW).
- DOY [R] -> INT32 (1-366).
- DAYOFYEAR [R] -> INT32 (alias of DOY).
- QUARTER [R] -> INT32 (1-4).
- WEEK [R] -> INT32 (week of year, Sunday-based).
- ISO_WEEK [R] -> INT32 (ISO week number, Monday-based).
- ISO_YEAR [R] -> INT32 (ISO week-based year).
- ISO_DOW [R] -> INT32 (1=Mon .. 7=Sun).
- CENTURY [R] -> INT32.
- DECADE [R] -> INT32.
- MILLENNIUM [R] -> INT32.
- EPOCH [R] -> INT64 (seconds since Unix epoch at local midnight).
- TIMEZONE [RW] -> INT32 (alias of TZ_OFFSET).
- TIMEZONE_HOUR [RW] -> INT32.
- TIMEZONE_MINUTE [RW] -> INT32.
- TZ_OFFSET [RW] -> INT32 (offset seconds).

#### TIME
- VALUE [RW] -> TIME.
- HOUR [RW] -> INT32 (0-23).
- HOUR24 [RW] -> INT32 (alias of HOUR).
- HOUR12 [R] -> INT32 (1-12).
- MINUTE [RW] -> INT32 (0-59).
- SECOND [RW] -> INT32 (0-59).
- MILLISECOND [RW] -> INT32 (0-999).
- MICROSECOND [RW] -> INT32 (0-999999).
- EPOCH [R] -> INT64 (seconds since local midnight).
- TIMEZONE [RW] -> INT32 (alias of TZ_OFFSET).
- TIMEZONE_HOUR [RW] -> INT32.
- TIMEZONE_MINUTE [RW] -> INT32.
- TZ_OFFSET [RW] -> INT32 (offset seconds).

#### TIMESTAMP
- VALUE [RW] -> TIMESTAMP.
- YEAR [RW] -> INT32.
- MONTH [RW] -> INT32 (1-12).
- DAY [RW] -> INT32 (1-31, calendar-validated).
- HOUR [RW] -> INT32 (0-23).
- MINUTE [RW] -> INT32 (0-59).
- SECOND [RW] -> INT32 (0-59).
- MILLISECOND [RW] -> INT32 (0-999).
- MICROSECOND [RW] -> INT32 (0-999999).
- DOW [R] -> INT32 (0=Sunday .. 6=Saturday).
- DOY [R] -> INT32 (1-366).
- QUARTER [R] -> INT32 (1-4).
- WEEK [R] -> INT32 (week of year, Sunday-based).
- ISO_WEEK [R] -> INT32 (ISO week number, Monday-based).
- ISO_YEAR [R] -> INT32 (ISO week-based year).
- ISO_DOW [R] -> INT32 (1=Mon .. 7=Sun).
- CENTURY [R] -> INT32.
- DECADE [R] -> INT32.
- MILLENNIUM [R] -> INT32.
- EPOCH [R] -> INT64 (seconds since Unix epoch, local time).
- TIMEZONE [RW] -> INT32 (alias of TZ_OFFSET).
- TIMEZONE_HOUR [RW] -> INT32.
- TIMEZONE_MINUTE [RW] -> INT32.
- TZ_OFFSET [RW] -> INT32 (offset seconds).

#### INTERVAL
- VALUE [RW] -> INTERVAL.
- YEAR [RW] -> INT32 (sets months = year*12 + current_month).
- MONTH [RW] -> INT32 (0-11, updates months remainder).
- DAY [RW] -> INT32.
- HOUR [RW] -> INT32 (updates microseconds).
- MINUTE [RW] -> INT32 (updates microseconds).
- SECOND [RW] -> INT32 (updates microseconds).
- MILLISECOND [RW] -> INT32 (updates microseconds).
- MICROSECOND [RW] -> INT32 (updates microseconds).
- TOTAL_MONTHS [R] -> INT32.
- TOTAL_DAYS [R] -> INT64.
- TOTAL_SECONDS [R] -> FLOAT64.
- EPOCH [R] -> FLOAT64 (months*30 days + days + microseconds).
- SIGN [R] -> INT8 (-1, 0, 1).

### UUID
- VALUE [RW] -> UUID.
- BYTES [RW] -> BINARY(16).
- VERSION [R] -> INT32.
- VARIANT [R] -> INT32.
- TIMESTAMP [R] -> INT64 (microseconds since Unix epoch for v1/v7).
- CLOCK_SEQ [R] -> INT32 (v1 only).
- NODE [R] -> TEXT (v1 only, MAC string).
- TIME_LOW [R] -> UINT32 (v1 only).
- TIME_MID [R] -> UINT16 (v1 only).
- TIME_HIGH [R] -> UINT16 (v1 only).
- RAND_A [R] -> UINT32 (v7 only).
- RAND_B [R] -> BINARY(8) (v7 only).

### Spatial types
Applies to POINT/LINESTRING/POLYGON/MULTIPOINT/MULTILINESTRING/MULTIPOLYGON/GEOMETRYCOLLECTION.
- VALUE [RW] -> same type.
- SRID [RW] -> INT32 (0 = undefined).
- BBOX [R] -> POLYGON (axis-aligned bounding box).

POINT only:
- X [RW] -> FLOAT64.
- Y [RW] -> FLOAT64.

LINESTRING only:
- POINTS [R] -> ARRAY(POINT).
- NUM_POINTS [R] -> INT32.
- LENGTH [R] -> FLOAT64.

POLYGON only:
- RINGS [R] -> ARRAY(ARRAY(POINT)).
- NUM_RINGS [R] -> INT32.
- AREA [R] -> FLOAT64.

MULTI* and GEOMETRYCOLLECTION:
- GEOMETRIES [R] -> ARRAY(GEOMETRY).
- NUM_GEOMETRIES [R] -> INT32.

### Array / Composite / Variant
#### ARRAY
- VALUE [RW] -> ARRAY.
- ELEMENT(index) [RW] -> element type (1-based index).
- CARDINALITY [R] -> INT32.
- NDIMS [R] -> INT32.
- LOWER(dim) [R] -> INT32 (default 1).
- UPPER(dim) [R] -> INT32 (size for that dimension).
- DIMS [R] -> ARRAY(INT32).

#### COMPOSITE
- VALUE [RW] -> COMPOSITE.
- FIELD(name_or_index) [RW] -> field type.
- FIELD_NAMES [R] -> ARRAY(TEXT).
- CARDINALITY [R] -> INT32.

#### VARIANT
- VALUE [RW] -> any type.
- DATATYPE [RW] -> INT32 (DataType enum; casts value to that type and stores explicit tag).

### Text search
#### TSVECTOR
- VALUE [RW] -> TSVECTOR.
- LEXEMES [R] -> ARRAY(TEXT).
- POSITIONS [R] -> ARRAY(ARRAY(INT32)).
- WEIGHTS [R] -> ARRAY(ARRAY(TEXT)).
- SIZE [R] -> INT32 (lexeme count).
- HAS_LEXEME(term) [R] -> BOOLEAN.

#### TSQUERY
- VALUE [RW] -> TSQUERY.
- ROOT_OP [R] -> TEXT (LEXEME, AND, OR, NOT, PHRASE).
- TERMS [R] -> ARRAY(TEXT).
- OPERATORS [R] -> ARRAY(TEXT).
- PHRASE_DISTANCE [R] -> INT32.
- NODES [R] -> INT32.

### Range types
Applies to INT4RANGE/INT8RANGE/NUMRANGE/TSRANGE/TSTZRANGE/DATERANGE.
- VALUE [RW] -> same range type.
- LOWER [RW] -> element type (NULL means unbounded lower).
- UPPER [RW] -> element type (NULL means unbounded upper).
- LOWER_INC [RW] -> BOOLEAN.
- UPPER_INC [RW] -> BOOLEAN.
- IS_EMPTY [RW] -> BOOLEAN (true makes range empty).
- IS_LOWER_INFINITE [RW] -> BOOLEAN (true makes lower unbounded).
- IS_UPPER_INFINITE [RW] -> BOOLEAN (true makes upper unbounded).

Notes:
- LOWER_VALUE and UPPER_VALUE are accepted aliases for LOWER and UPPER.

### Network types
#### INET
- VALUE [RW] -> INET.
- FAMILY [R] -> INT32 (4 or 6).
- NETMASK [RW] -> INT32 (0-32 or 0-128).
- ADDRESS [RW] -> TEXT (address without netmask).
- NETWORK [R] -> INET (host bits zeroed).
- BROADCAST [R] -> INET (IPv4 only; NULL for IPv6).
- NETMASK_ADDR [R] -> INET.
- HOSTMASK [R] -> INET.
- IS_IPV4 [R] -> BOOLEAN.
- IS_IPV6 [R] -> BOOLEAN.

#### CIDR
- VALUE [RW] -> CIDR.
- FAMILY [R] -> INT32 (4 or 6).
- NETMASK [RW] -> INT32 (0-32 or 0-128).
- ADDRESS [RW] -> TEXT (must be canonical network address; host bits must be zero).
- NETWORK [R] -> CIDR.
- NETMASK_ADDR [R] -> CIDR.
- HOSTMASK [R] -> CIDR.
- IS_IPV4 [R] -> BOOLEAN.
- IS_IPV6 [R] -> BOOLEAN.

#### MACADDR / MACADDR8
- VALUE [RW] -> MACADDR / MACADDR8.
- BYTES [RW] -> BINARY(6 or 8).
- OUI [RW] -> BINARY(3) (first 3 bytes).
- NIC [RW] -> BINARY(3 or 5) (remaining bytes).
- IS_MULTICAST [R] -> BOOLEAN.
- IS_LOCAL [R] -> BOOLEAN.
- TRUNC [R] -> BINARY(3) (manufacturer ID).

## Examples

### Temporal
```
SELECT ALTER_ELEMENT(HOUR IN order_time TO 9) FROM orders;
SELECT ALTER_ELEMENT(TIMEZONE_HOUR IN order_time TO -5) FROM orders;
SELECT EXTRACT(ISO_WEEK FROM order_date) FROM orders;
```

### Array and composite
```
SELECT EXTRACT(2 FROM tags) FROM items; -- second element
SELECT ALTER_ELEMENT(ELEMENT(2) IN tags TO 'hot') FROM items;
SELECT EXTRACT(city FROM address) FROM customers;
SELECT ALTER_ELEMENT(FIELD('city') IN address TO 'Boston') FROM customers;
```

### JSON
```
SELECT EXTRACT(PATH('$.a[0].b') FROM doc) FROM t;
SELECT ALTER_ELEMENT(PATH('$.a[0].b') IN doc TO 7) FROM t;
```

## Implementation notes
- ALTER_ELEMENT should compile to a dedicated SBLR opcode with the element
  selector and a value-cast rule per type.
- EXTRACT/ALTER_ELEMENT for derived fields (DOW, DOY, QUARTER, ISO_*) are
  read-only and must error if used in ALTER_ELEMENT.
- For CIDR ADDRESS updates, reject non-canonical host addresses.
- For arrays, index is 1-based. Out-of-range indexes raise an error.
- For composite fields, unknown field names raise an error.
