# PostgreSQL Parser Specification for ScratchBird Emulation

**Version:** 1.0
**Status:** Planning
**Target:** Full PostgreSQL 16 SQL Compatibility for Emulated Clients

---

## Executive Summary

This specification covers creating a complete PostgreSQL 16 SQL parser separate from the ScratchBird V2 parser, plus the virtual catalog handler for PostgreSQL system catalogs (pg_catalog, information_schema). PostgreSQL clients connecting to ScratchBird will be able to use native PostgreSQL SQL syntax and see familiar system schemas.

**Reference:** This document follows the architecture defined in `/docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`

---

## Architecture Overview

```
PostgreSQL Client (psql, pgAdmin, DBeaver, etc.)
      |
      v
+---------------------------------------------------------------------+
|                   PostgreSQL Emulation Layer                         |
|  +---------------+  +---------------+  +------------------------+   |
|  | PostgreSQL    |  | PostgreSQL    |  | PgCatalogHandler       |   |
|  | Lexer         |-->| Parser       |  | (pg_catalog,           |   |
|  +---------------+  +---------------+  |  information_schema)   |   |
|         |                   |          +------------------------+   |
|         v                   v                       |               |
|    PG Tokens        SBLR Bytecode           Virtual Tables          |
+---------------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------------+
|                    Shared ScratchBird Pipeline                       |
|  +-------------------+                  +------------------------+   |
|  | SBLR Executor     |----------------->| ScratchBird Storage    |   |
|  | (Single impl)     |                  | Engine                 |   |
|  +-------------------+                  +------------------------+   |
+---------------------------------------------------------------------+
```

**Schema Path:** `/remote/emulation/postgresql/{server}/{database}/`

---

## PostgreSQL 16 Reserved Keywords

PostgreSQL 16 has approximately **130 keywords**, with **~96 reserved** (~74%). Reserved keywords require double-quote quoting when used as identifiers.

### Reserved Keywords (Must Be Quoted as Identifiers)

```
ALL, ANALYSE, ANALYZE, AND, ANY, ARRAY, AS, ASC, ASYMMETRIC,
AUTHORIZATION, BINARY, BOTH, CASE, CAST, CHECK, COLLATE, COLLATION,
COLUMN, CONCURRENTLY, CONSTRAINT, CREATE, CROSS, CURRENT_CATALOG,
CURRENT_DATE, CURRENT_ROLE, CURRENT_SCHEMA, CURRENT_TIME,
CURRENT_TIMESTAMP, CURRENT_USER, DEFAULT, DEFERRABLE, DESC, DISTINCT,
DO, ELSE, END, EXCEPT, FALSE, FETCH, FOR, FOREIGN, FREEZE, FROM, FULL,
GRANT, GROUP, HAVING, ILIKE, IN, INITIALLY, INNER, INTERSECT, INTO, IS,
ISNULL, JOIN, LATERAL, LEADING, LEFT, LIKE, LIMIT, LOCALTIME,
LOCALTIMESTAMP, NATURAL, NOT, NOTNULL, NULL, OFFSET, ON, ONLY, OR,
ORDER, OUTER, OVERLAPS, PLACING, PRIMARY, REFERENCES, RETURNING, RIGHT,
SELECT, SESSION_USER, SIMILAR, SOME, SYMMETRIC, SYSTEM_USER, TABLE,
TABLESAMPLE, THEN, TO, TRAILING, TRUE, UNION, UNIQUE, USER, USING,
VARIADIC, VERBOSE, WHEN, WHERE, WINDOW, WITH
```

### Non-Reserved Keywords (Context-Sensitive)

PostgreSQL has many non-reserved keywords that can be used as identifiers without quoting in most contexts. Key non-reserved keywords include:

```
ABORT, ABSOLUTE, ACCESS, ACTION, ADD, ADMIN, AFTER, AGGREGATE, ALSO,
ALTER, ALWAYS, ASSERTION, ASSIGNMENT, AT, ATTACH, ATTRIBUTE, BACKWARD,
BEFORE, BEGIN, BIGINT, BOOLEAN, BY, CACHE, CALL, CALLED, CASCADE,
CASCADED, CATALOG, CHAIN, CHARACTERISTICS, CHECKPOINT, CLASS, CLOSE,
CLUSTER, COALESCE, COLUMNS, COMMENT, COMMENTS, COMMIT, COMMITTED,
CONFIGURATION, CONFLICT, CONNECTION, CONSTRAINTS, CONTENT, CONTINUE,
CONVERSION, COPY, COST, CSV, CUBE, CURRENT, CURSOR, CYCLE, DATA,
DATABASE, DAY, DEALLOCATE, DEC, DECIMAL, DECLARE, DEFAULTS, DEFERRED,
DEFINER, DELETE, DELIMITER, DELIMITERS, DEPENDS, DETACH, DICTIONARY,
DISABLE, DISCARD, DOCUMENT, DOMAIN, DOUBLE, DROP, EACH, ENABLE,
ENCODING, ENCRYPTED, ENUM, ESCAPE, EVENT, EXCLUDE, EXCLUDING,
EXCLUSIVE, EXECUTE, EXISTS, EXPLAIN, EXPRESSION, EXTENSION, EXTERNAL,
EXTRACT, FAMILY, FILTER, FINALIZE, FIRST, FLOAT, FOLLOWING, FORCE,
FORMAT, FORWARD, FUNCTION, FUNCTIONS, GENERATED, GLOBAL, GRANTED,
GREATEST, GROUPING, GROUPS, HANDLER, HEADER, HOLD, HOUR, IDENTITY, IF,
IMMEDIATE, IMMUTABLE, IMPLICIT, IMPORT, INCLUDE, INCLUDING, INCREMENT,
INDEX, INDEXES, INHERIT, INHERITS, INITIALLY, INLINE, INPUT, INSENSITIVE,
INSERT, INSTEAD, INT, INTEGER, INTERVAL, INVOKER, ISOLATION, JSON,
KEY, LABEL, LANGUAGE, LARGE, LAST, LATERAL, LC_COLLATE, LC_CTYPE, LEAKPROOF,
LEAST, LEVEL, LISTEN, LOAD, LOCAL, LOCATION, LOCK, LOCKED, LOGGED,
MAPPING, MATCH, MATCHED, MATERIALIZED, MAXVALUE, MERGE, METHOD,
MINUTE, MINVALUE, MODE, MONTH, MOVE, NAME, NAMES, NATIONAL, NCHAR,
NEW, NEXT, NFC, NFD, NFKC, NFKD, NO, NONE, NORMALIZE, NORMALIZED,
NOTHING, NOTIFY, NOWAIT, NULLIF, NULLS, NUMERIC, OBJECT, OF, OFF,
OIDS, OLD, OPERATOR, OPTION, OPTIONS, ORDINALITY, OTHERS, OUT, OVER,
OWNED, OWNER, PARALLEL, PARAMETER, PARSER, PARTIAL, PARTITION, PASSING,
PASSWORD, PLANS, POLICY, POSITION, PRECEDING, PRECISION, PREPARE,
PREPARED, PRESERVE, PRIMARY, PRIOR, PRIVILEGES, PROCEDURAL, PROCEDURE,
PROCEDURES, PROGRAM, PUBLICATION, QUOTE, RANGE, READ, REAL, REASSIGN,
RECHECK, RECURSIVE, REF, REFERENCING, REFRESH, REINDEX, RELATIVE,
RELEASE, RENAME, REPEATABLE, REPLACE, REPLICA, RESET, RESTART,
RESTRICT, RETURN, RETURNS, REVOKE, ROLE, ROLLBACK, ROLLUP, ROUTINE,
ROUTINES, ROW, ROWS, RULE, SAVEPOINT, SCALAR, SCHEMA, SCHEMAS, SCROLL,
SEARCH, SECOND, SECURITY, SEQUENCE, SEQUENCES, SERIALIZABLE, SERVER,
SESSION, SET, SETOF, SETS, SHARE, SHOW, SIMPLE, SKIP, SMALLINT,
SNAPSHOT, SQL, STABLE, STANDALONE, START, STATEMENT, STATISTICS,
STDIN, STDOUT, STORAGE, STORED, STRICT, STRIP, SUBSCRIPTION,
SUBSTRING, SUPPORT, SYSID, SYSTEM, TABLES, TABLESPACE, TEMP, TEMPLATE,
TEMPORARY, TEXT, TIES, TIME, TIMESTAMP, TRANSACTION, TRANSFORM, TREAT,
TRIGGER, TRIM, TRUNCATE, TRUSTED, TYPE, TYPES, UESCAPE, UNBOUNDED,
UNCOMMITTED, UNENCRYPTED, UNKNOWN, UNLISTEN, UNLOGGED, UNTIL, UPDATE,
VACUUM, VALID, VALIDATE, VALIDATOR, VALUE, VALUES, VARCHAR, VARYING,
VERSION, VIEW, VIEWS, VOLATILE, WHITESPACE, WITHIN, WITHOUT, WORK,
WRAPPER, WRITE, XML, XMLATTRIBUTES, XMLCONCAT, XMLELEMENT, XMLEXISTS,
XMLFOREST, XMLNAMESPACES, XMLPARSE, XMLPI, XMLROOT, XMLSERIALIZE,
XMLTABLE, YEAR, YES, ZONE
```

### Querying Keywords Dynamically

PostgreSQL clients can query keywords via:
```sql
SELECT * FROM pg_get_keywords();
SELECT word, catcode, catdesc FROM pg_get_keywords() WHERE catcode = 'R';
```

---

## PostgreSQL 16 Data Types

### Numeric Types

| Type | Storage | Description |
|------|---------|-------------|
| SMALLINT (INT2) | 2 bytes | -32,768 to 32,767 |
| INTEGER (INT, INT4) | 4 bytes | -2,147,483,648 to 2,147,483,647 |
| BIGINT (INT8) | 8 bytes | -9.2×10^18 to 9.2×10^18 |
| DECIMAL/NUMERIC(p,s) | Variable | Exact precision up to 131,072 digits |
| REAL (FLOAT4) | 4 bytes | ~6 decimal digits precision |
| DOUBLE PRECISION (FLOAT8) | 8 bytes | ~15 decimal digits precision |
| SMALLSERIAL (SERIAL2) | 2 bytes | Auto-incrementing smallint |
| SERIAL (SERIAL4) | 4 bytes | Auto-incrementing integer |
| BIGSERIAL (SERIAL8) | 8 bytes | Auto-incrementing bigint |
| MONEY | 8 bytes | Currency amount |

**PostgreSQL-Specific:**
- `SERIAL`/`BIGSERIAL`/`SMALLSERIAL` are pseudo-types that create sequences
- `GENERATED AS IDENTITY` is the SQL-standard alternative to SERIAL
- No UNSIGNED modifier (unlike MySQL)

### Character Types

| Type | Description |
|------|-------------|
| CHAR(n) / CHARACTER(n) | Fixed-length, blank-padded |
| VARCHAR(n) / CHARACTER VARYING(n) | Variable-length with limit |
| TEXT | Variable unlimited length |
| "char" | Single-byte internal type |
| NAME | Internal type for identifiers (64 bytes) |

### Binary Data Types

| Type | Description |
|------|-------------|
| BYTEA | Variable-length binary data |

### Date/Time Types

| Type | Storage | Description |
|------|---------|-------------|
| DATE | 4 bytes | Date only (4713 BC to 5874897 AD) |
| TIME | 8 bytes | Time of day (no time zone) |
| TIME WITH TIME ZONE | 12 bytes | Time with time zone |
| TIMESTAMP | 8 bytes | Date and time (no time zone) |
| TIMESTAMP WITH TIME ZONE | 8 bytes | Date and time with time zone |
| INTERVAL | 16 bytes | Time span |

**PostgreSQL-Specific:**
- Fractional seconds precision: `TIMESTAMP(p)`, `TIME(p)` (0-6)
- `INTERVAL` fields: YEAR, MONTH, DAY, HOUR, MINUTE, SECOND
- Can specify: `INTERVAL 'value' field` or `INTERVAL 'value' field TO field`

### Boolean Type

| Type | Storage | Description |
|------|---------|-------------|
| BOOLEAN | 1 byte | true/false/null |

**Accepted literals:** TRUE, FALSE, 't', 'f', 'true', 'false', 'y', 'n', 'yes', 'no', 'on', 'off', '1', '0'

### Enumerated Type

```sql
CREATE TYPE mood AS ENUM ('sad', 'ok', 'happy');
```

### UUID Type

| Type | Storage | Description |
|------|---------|-------------|
| UUID | 16 bytes | Universally Unique Identifier |

### JSON Types

| Type | Description |
|------|-------------|
| JSON | Textual JSON data (preserves formatting) |
| JSONB | Binary JSON (decomposed, indexable) |
| JSONPATH | JSON path expressions |

### Array Types

Any data type can be made into an array:
- `INTEGER[]` - One-dimensional array
- `INTEGER[][]` - Multi-dimensional array
- `INTEGER ARRAY` - Alternative syntax
- `INTEGER ARRAY[n]` - With size (informational only)

### Composite Types

```sql
CREATE TYPE inventory_item AS (
    name TEXT,
    supplier_id INTEGER,
    price NUMERIC
);
```

### Range Types

| Type | Description |
|------|-------------|
| INT4RANGE | Range of integer |
| INT8RANGE | Range of bigint |
| NUMRANGE | Range of numeric |
| TSRANGE | Range of timestamp without time zone |
| TSTZRANGE | Range of timestamp with time zone |
| DATERANGE | Range of date |

**Constructor syntax:** `'[1,10)'::int4range` or `int4range(1, 10)`

### Geometric Types

| Type | Description |
|------|-------------|
| POINT | Point on a plane (x,y) |
| LINE | Infinite line {A,B,C} |
| LSEG | Line segment ((x1,y1),(x2,y2)) |
| BOX | Rectangular box ((x1,y1),(x2,y2)) |
| PATH | Geometric path [(x1,y1),...] |
| POLYGON | Polygon ((x1,y1),...) |
| CIRCLE | Circle <(x,y),r> |

### Network Address Types

| Type | Description |
|------|-------------|
| CIDR | IPv4 or IPv6 network |
| INET | IPv4 or IPv6 host address |
| MACADDR | MAC address |
| MACADDR8 | MAC address (EUI-64 format) |

### Text Search Types

| Type | Description |
|------|-------------|
| TSVECTOR | Text search document |
| TSQUERY | Text search query |

### Other Types

| Type | Description |
|------|-------------|
| XML | XML data |
| BIT(n) | Fixed-length bit string |
| BIT VARYING(n) | Variable-length bit string |
| OID | Object identifier |
| REGCLASS, REGTYPE, etc. | Object identifier types |
| PG_LSN | Log sequence number |

---

## PostgreSQL 16 Operators

### Arithmetic Operators

| Operator | Description |
|----------|-------------|
| `+` | Addition |
| `-` | Subtraction |
| `*` | Multiplication |
| `/` | Division |
| `%` | Modulo |
| `^` | Exponentiation |
| `\|/` | Square root |
| `\|\|/` | Cube root |
| `!` | Factorial (postfix) |
| `!!` | Factorial (prefix) |
| `@` | Absolute value |
| `&` | Bitwise AND |
| `\|` | Bitwise OR |
| `#` | Bitwise XOR |
| `~` | Bitwise NOT |
| `<<` | Bitwise shift left |
| `>>` | Bitwise shift right |

### Comparison Operators

| Operator | Description |
|----------|-------------|
| `=` | Equal |
| `<>`, `!=` | Not equal |
| `<` | Less than |
| `<=` | Less than or equal |
| `>` | Greater than |
| `>=` | Greater than or equal |
| `IS DISTINCT FROM` | Not equal, treating null as comparable |
| `IS NOT DISTINCT FROM` | Equal, treating null as comparable |
| `IS NULL` | Is null |
| `IS NOT NULL` | Is not null |
| `BETWEEN ... AND ...` | Range test |
| `NOT BETWEEN ... AND ...` | Not in range |
| `BETWEEN SYMMETRIC ... AND ...` | Range test (bounds can be in any order) |
| `IN (...)` | Set membership |
| `NOT IN (...)` | Not in set |
| `LIKE` | Pattern matching |
| `NOT LIKE` | Pattern not matching |
| `ILIKE` | Case-insensitive LIKE (PostgreSQL-specific) |
| `NOT ILIKE` | Case-insensitive NOT LIKE |
| `SIMILAR TO` | SQL regular expression |
| `~` | POSIX regex match (case sensitive) |
| `~*` | POSIX regex match (case insensitive) |
| `!~` | POSIX regex not match (case sensitive) |
| `!~*` | POSIX regex not match (case insensitive) |

### Logical Operators

| Operator | Description |
|----------|-------------|
| `AND` | Logical AND |
| `OR` | Logical OR |
| `NOT` | Logical NOT |

### Type Cast Operator

| Operator | Description |
|----------|-------------|
| `::` | Type cast (PostgreSQL-specific) |
| `CAST(x AS type)` | SQL standard type cast |

### String Operators

| Operator | Description |
|----------|-------------|
| `\|\|` | String concatenation |
| `COLLATE` | Collation specification |

### Array Operators

| Operator | Description |
|----------|-------------|
| `@>` | Contains |
| `<@` | Is contained by |
| `&&` | Overlap (have elements in common) |
| `\|\|` | Array concatenation |
| `[n]` | Array subscript |
| `[n:m]` | Array slice |

### JSON/JSONB Operators

| Operator | Description |
|----------|-------------|
| `->` | Get JSON object field (returns JSON) |
| `->>` | Get JSON object field (returns text) |
| `#>` | Get JSON object at path (returns JSON) |
| `#>>` | Get JSON object at path (returns text) |
| `@>` | Contains (JSONB only) |
| `<@` | Is contained by (JSONB only) |
| `?` | Key exists (JSONB only) |
| `?\|` | Any key exists (JSONB only) |
| `?&` | All keys exist (JSONB only) |
| `\|\|` | Concatenate (JSONB only) |
| `-` | Delete key/element (JSONB only) |
| `#-` | Delete at path (JSONB only) |
| `@?` | JSON path exists (JSONB only) |
| `@@` | JSON path match (JSONB only) |

### Range Operators

| Operator | Description |
|----------|-------------|
| `@>` | Contains range/element |
| `<@` | Is contained by |
| `&&` | Overlap |
| `<<` | Strictly left of |
| `>>` | Strictly right of |
| `&<` | Does not extend to the right of |
| `&>` | Does not extend to the left of |
| `-\|-` | Is adjacent to |
| `+` | Union |
| `*` | Intersection |
| `-` | Difference |

### Text Search Operators

| Operator | Description |
|----------|-------------|
| `@@` | Text search match |
| `@@@` | Text search match (deprecated) |
| `\|\|` | Concatenate tsvectors |
| `&&` | AND tsqueries |
| `\|\|` | OR tsqueries |
| `!!` | Negate tsquery |
| `<->` | Followed by (tsquery) |

### Geometric Operators

| Operator | Description |
|----------|-------------|
| `+` | Translation |
| `-` | Translation |
| `*` | Scaling/rotation |
| `/` | Scaling/rotation |
| `#` | Point of intersection |
| `##` | Closest point |
| `<->` | Distance |
| `@>` | Contains |
| `<@` | Is contained by |
| `&&` | Overlaps |
| `<<` | Is strictly left of |
| `>>` | Is strictly right of |
| `&<` | Does not extend to right of |
| `&>` | Does not extend to left of |
| `<<\|` | Is strictly below |
| `\|>>` | Is strictly above |
| `&<\|` | Does not extend above |
| `\|&>` | Does not extend below |
| `?#` | Intersects |
| `?-` | Is horizontal |
| `?\|` | Is vertical |
| `?-\|` | Is perpendicular |
| `?\|\|` | Is parallel |
| `@` | Center |
| `~=` | Same as |

### Network Address Operators

| Operator | Description |
|----------|-------------|
| `<<` | Is contained by |
| `<<=` | Is contained by or equal |
| `>>` | Contains |
| `>>=` | Contains or equals |
| `&&` | Contains or is contained by |
| `~` | Bitwise NOT |
| `&` | Bitwise AND |
| `\|` | Bitwise OR |
| `+` | Addition |
| `-` | Subtraction |

---

## PostgreSQL 16 Built-in Functions

### Aggregate Functions

```
AVG(), BIT_AND(), BIT_OR(), BIT_XOR(), BOOL_AND(), BOOL_OR(), COUNT(),
COUNT(DISTINCT), EVERY(), JSON_AGG(), JSON_OBJECT_AGG(), JSONB_AGG(),
JSONB_OBJECT_AGG(), MAX(), MIN(), STRING_AGG(), SUM(), XMLAGG(),
ARRAY_AGG()
```

**Statistical Aggregates:**
```
CORR(), COVAR_POP(), COVAR_SAMP(), REGR_AVGX(), REGR_AVGY(), REGR_COUNT(),
REGR_INTERCEPT(), REGR_R2(), REGR_SLOPE(), REGR_SXX(), REGR_SXY(),
REGR_SYY(), STDDEV(), STDDEV_POP(), STDDEV_SAMP(), VAR_POP(), VAR_SAMP(),
VARIANCE()
```

**Ordered-Set Aggregates:**
```
MODE() WITHIN GROUP (ORDER BY ...)
PERCENTILE_CONT(fraction) WITHIN GROUP (ORDER BY ...)
PERCENTILE_DISC(fraction) WITHIN GROUP (ORDER BY ...)
```

### Window Functions

```
CUME_DIST(), DENSE_RANK(), FIRST_VALUE(), LAG(), LAST_VALUE(), LEAD(),
NTH_VALUE(), NTILE(), PERCENT_RANK(), RANK(), ROW_NUMBER()
```

**OVER Clause Syntax:**
```sql
function_name() OVER (
    [PARTITION BY expr [, expr ...]]
    [ORDER BY expr [ASC|DESC] [NULLS {FIRST|LAST}] [, ...]]
    [frame_clause]
)

function_name() OVER window_name

frame_clause:
    {RANGE | ROWS | GROUPS} frame_start [frame_exclusion]
  | {RANGE | ROWS | GROUPS} BETWEEN frame_start AND frame_end [frame_exclusion]

frame_start/frame_end:
    UNBOUNDED PRECEDING | offset PRECEDING | CURRENT ROW |
    offset FOLLOWING | UNBOUNDED FOLLOWING

frame_exclusion:
    EXCLUDE CURRENT ROW | EXCLUDE GROUP | EXCLUDE TIES | EXCLUDE NO OTHERS
```

### String Functions

```
ASCII(), BIT_LENGTH(), BTRIM(), CHAR_LENGTH(), CHARACTER_LENGTH(),
CHR(), CONCAT(), CONCAT_WS(), CONVERT(), CONVERT_FROM(), CONVERT_TO(),
DECODE(), ENCODE(), FORMAT(), INITCAP(), LEFT(), LENGTH(), LOWER(),
LPAD(), LTRIM(), MD5(), NORMALIZE(), OCTET_LENGTH(), OVERLAY(),
PARSE_IDENT(), PG_CLIENT_ENCODING(), POSITION(), QUOTE_IDENT(),
QUOTE_LITERAL(), QUOTE_NULLABLE(), REGEXP_COUNT(), REGEXP_INSTR(),
REGEXP_LIKE(), REGEXP_MATCH(), REGEXP_MATCHES(), REGEXP_REPLACE(),
REGEXP_SPLIT_TO_ARRAY(), REGEXP_SPLIT_TO_TABLE(), REGEXP_SUBSTR(),
REPEAT(), REPLACE(), REVERSE(), RIGHT(), RPAD(), RTRIM(), SPLIT_PART(),
STARTS_WITH(), STRING_TO_ARRAY(), STRING_TO_TABLE(), STRPOS(), SUBSTR(),
SUBSTRING(), TO_ASCII(), TO_HEX(), TRANSLATE(), TRIM(), UNISTR(), UPPER()
```

### Numeric Functions

```
ABS(), ACOS(), ACOSD(), ACOSH(), ASIN(), ASIND(), ASINH(), ATAN(),
ATAN2(), ATAN2D(), ATAND(), ATANH(), CBRT(), CEIL(), CEILING(), COS(),
COSD(), COSH(), COT(), COTD(), DEGREES(), DIV(), EXP(), FACTORIAL(),
FLOOR(), GCD(), LCM(), LN(), LOG(), LOG10(), MIN_SCALE(), MOD(), PI(),
POWER(), RADIANS(), RANDOM(), ROUND(), SCALE(), SETSEED(), SIGN(), SIN(),
SIND(), SINH(), SQRT(), TAN(), TAND(), TANH(), TRIM_SCALE(), TRUNC(),
WIDTH_BUCKET()
```

### Date/Time Functions

```
AGE(), CLOCK_TIMESTAMP(), CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP,
DATE_BIN(), DATE_PART(), DATE_TRUNC(), EXTRACT(), ALTER_ELEMENT(), ISFINITE(), JUSTIFY_DAYS(),
JUSTIFY_HOURS(), JUSTIFY_INTERVAL(), LOCALTIME, LOCALTIMESTAMP,
MAKE_DATE(), MAKE_INTERVAL(), MAKE_TIME(), MAKE_TIMESTAMP(),
MAKE_TIMESTAMPTZ(), NOW(), PG_SLEEP(), PG_SLEEP_FOR(), PG_SLEEP_UNTIL(),
STATEMENT_TIMESTAMP(), TIMEOFDAY(), TIMEZONE(), TO_TIMESTAMP(),
TRANSACTION_TIMESTAMP()
```

### JSON Functions

```
ARRAY_TO_JSON(), JSON_AGG(), JSON_ARRAY(), JSON_ARRAY_ELEMENTS(),
JSON_ARRAY_ELEMENTS_TEXT(), JSON_ARRAY_LENGTH(), JSON_BUILD_ARRAY(),
JSON_BUILD_OBJECT(), JSON_EACH(), JSON_EACH_TEXT(), JSON_EXTRACT_PATH(),
JSON_EXTRACT_PATH_TEXT(), JSON_OBJECT(), JSON_OBJECT_AGG(),
JSON_OBJECT_KEYS(), JSON_POPULATE_RECORD(), JSON_POPULATE_RECORDSET(),
JSON_QUERY(), JSON_SCALAR(), JSON_SERIALIZE(), JSON_STRIP_NULLS(),
JSON_TABLE(), JSON_TO_RECORD(), JSON_TO_RECORDSET(), JSON_TYPEOF(),
JSON_VALUE(), ROW_TO_JSON(), TO_JSON()
```

**JSONB-specific:**
```
JSONB_AGG(), JSONB_ARRAY_ELEMENTS(), JSONB_ARRAY_ELEMENTS_TEXT(),
JSONB_ARRAY_LENGTH(), JSONB_BUILD_ARRAY(), JSONB_BUILD_OBJECT(),
JSONB_EACH(), JSONB_EACH_TEXT(), JSONB_EXTRACT_PATH(),
JSONB_EXTRACT_PATH_TEXT(), JSONB_INSERT(), JSONB_OBJECT(),
JSONB_OBJECT_AGG(), JSONB_OBJECT_KEYS(), JSONB_PATH_EXISTS(),
JSONB_PATH_EXISTS_TZ(), JSONB_PATH_MATCH(), JSONB_PATH_MATCH_TZ(),
JSONB_PATH_QUERY(), JSONB_PATH_QUERY_ARRAY(), JSONB_PATH_QUERY_ARRAY_TZ(),
JSONB_PATH_QUERY_FIRST(), JSONB_PATH_QUERY_FIRST_TZ(), JSONB_PATH_QUERY_TZ(),
JSONB_POPULATE_RECORD(), JSONB_POPULATE_RECORDSET(), JSONB_PRETTY(),
JSONB_SET(), JSONB_SET_LAX(), JSONB_STRIP_NULLS(), JSONB_TO_RECORD(),
JSONB_TO_RECORDSET(), JSONB_TYPEOF(), TO_JSONB()
```

### Array Functions

```
ARRAY_AGG(), ARRAY_APPEND(), ARRAY_CAT(), ARRAY_DIMS(), ARRAY_FILL(),
ARRAY_LENGTH(), ARRAY_LOWER(), ARRAY_NDIMS(), ARRAY_POSITION(),
ARRAY_POSITIONS(), ARRAY_PREPEND(), ARRAY_REMOVE(), ARRAY_REPLACE(),
ARRAY_SAMPLE(), ARRAY_SHUFFLE(), ARRAY_TO_STRING(), ARRAY_UPPER(),
CARDINALITY(), STRING_TO_ARRAY(), TRIM_ARRAY(), UNNEST()
```

### Range Functions

```
ISEMPTY(), LOWER(), LOWER_INC(), LOWER_INF(), RANGE_MERGE(),
UPPER(), UPPER_INC(), UPPER_INF()
```

### Control Flow Functions

```
CASE ... WHEN ... THEN ... ELSE ... END
COALESCE(expr1, expr2, ...)
GREATEST(value1, value2, ...)
LEAST(value1, value2, ...)
NULLIF(expr1, expr2)
```

### Type Cast Functions

```
CAST(expr AS type)
expr::type
```

### System Information Functions

```
CURRENT_CATALOG, CURRENT_DATABASE(), CURRENT_QUERY(), CURRENT_ROLE,
CURRENT_SCHEMA, CURRENT_SCHEMAS(), CURRENT_USER, INET_CLIENT_ADDR(),
INET_CLIENT_PORT(), INET_SERVER_ADDR(), INET_SERVER_PORT(),
PG_BACKEND_PID(), PG_BLOCKING_PIDS(), PG_CONF_LOAD_TIME(),
PG_CURRENT_LOGFILE(), PG_IS_OTHER_TEMP_SCHEMA(), PG_JIT_AVAILABLE(),
PG_LISTENING_CHANNELS(), PG_MY_TEMP_SCHEMA(), PG_NOTIFICATION_QUEUE_USAGE(),
PG_POSTMASTER_START_TIME(), PG_SAFE_SNAPSHOT_BLOCKING_PIDS(),
PG_TRIGGER_DEPTH(), SESSION_USER, SYSTEM_USER, USER, VERSION()
```

### Object Information Functions

```
COL_DESCRIPTION(), FORMAT_TYPE(), HAS_ANY_COLUMN_PRIVILEGE(),
HAS_COLUMN_PRIVILEGE(), HAS_DATABASE_PRIVILEGE(), HAS_FOREIGN_DATA_WRAPPER_PRIVILEGE(),
HAS_FUNCTION_PRIVILEGE(), HAS_LANGUAGE_PRIVILEGE(), HAS_PARAMETER_PRIVILEGE(),
HAS_SCHEMA_PRIVILEGE(), HAS_SEQUENCE_PRIVILEGE(), HAS_SERVER_PRIVILEGE(),
HAS_TABLE_PRIVILEGE(), HAS_TABLESPACE_PRIVILEGE(), HAS_TYPE_PRIVILEGE(),
OBJ_DESCRIPTION(), PG_DESCRIBE_OBJECT(), PG_GET_CATALOG_FOREIGN_KEYS(),
PG_GET_CONSTRAINTDEF(), PG_GET_EXPR(), PG_GET_FUNCTION_ARGUMENTS(),
PG_GET_FUNCTION_IDENTITY_ARGUMENTS(), PG_GET_FUNCTION_RESULT(),
PG_GET_FUNCTIONDEF(), PG_GET_INDEXDEF(), PG_GET_KEYWORDS(),
PG_GET_OBJECT_ADDRESS(), PG_GET_PARTKEYDEF(), PG_GET_RULEDEF(),
PG_GET_SERIAL_SEQUENCE(), PG_GET_STATISTICSOBJDEF(), PG_GET_TRIGGERDEF(),
PG_GET_USERBYID(), PG_GET_VIEWDEF(), PG_IDENTIFY_OBJECT(),
PG_IDENTIFY_OBJECT_AS_ADDRESS(), PG_INDEX_COLUMN_HAS_PROPERTY(),
PG_INDEX_HAS_PROPERTY(), PG_INDEXAM_HAS_PROPERTY(), PG_OPTIONS_TO_TABLE(),
PG_SETTINGS_GET_FLAGS(), PG_TABLESPACE_DATABASES(), PG_TABLESPACE_LOCATION(),
PG_TYPEOF(), SHOBJ_DESCRIPTION(), TO_REGCLASS(), TO_REGCOLLATION(),
TO_REGNAMESPACE(), TO_REGOPER(), TO_REGOPERATOR(), TO_REGPROC(),
TO_REGPROCEDURE(), TO_REGROLE(), TO_REGTYPE()
```

---

## PostgreSQL 16 DML Syntax

### SELECT Statement

```sql
[ WITH [ RECURSIVE ] with_query [, ...] ]
SELECT [ ALL | DISTINCT [ ON ( expression [, ...] ) ] ]
    [ * | expression [ [ AS ] output_name ] [, ...] ]
    [ FROM from_item [, ...] ]
    [ WHERE condition ]
    [ GROUP BY [ ALL | DISTINCT ] grouping_element [, ...] ]
    [ HAVING condition ]
    [ WINDOW window_name AS ( window_definition ) [, ...] ]
    [ { UNION | INTERSECT | EXCEPT } [ ALL | DISTINCT ] select ]
    [ ORDER BY expression [ ASC | DESC | USING operator ] [ NULLS { FIRST | LAST } ] [, ...] ]
    [ LIMIT { count | ALL } ]
    [ OFFSET start [ ROW | ROWS ] ]
    [ FETCH { FIRST | NEXT } [ count ] { ROW | ROWS } { ONLY | WITH TIES } ]
    [ FOR { UPDATE | NO KEY UPDATE | SHARE | KEY SHARE } [ OF table_name [, ...] ] [ NOWAIT | SKIP LOCKED ] [...] ]

from_item:
    [ ONLY ] table_name [ * ] [ [ AS ] alias [ ( column_alias [, ...] ) ] ]
        [ TABLESAMPLE sampling_method ( argument [, ...] ) [ REPEATABLE ( seed ) ] ]
  | [ LATERAL ] ( select ) [ AS ] alias [ ( column_alias [, ...] ) ]
  | with_query_name [ [ AS ] alias [ ( column_alias [, ...] ) ] ]
  | [ LATERAL ] function_name ( [ argument [, ...] ] )
        [ WITH ORDINALITY ] [ [ AS ] alias [ ( column_alias [, ...] ) ] ]
  | [ LATERAL ] function_name ( [ argument [, ...] ] ) [ AS ] alias ( column_definition [, ...] )
  | [ LATERAL ] function_name ( [ argument [, ...] ] ) AS ( column_definition [, ...] )
  | [ LATERAL ] ROWS FROM( function_name ( [ argument [, ...] ] ) [ AS ( column_definition [, ...] ) ] [, ...] )
        [ WITH ORDINALITY ] [ [ AS ] alias [ ( column_alias [, ...] ) ] ]
  | from_item join_type from_item { ON join_condition | USING ( join_column [, ...] ) [ AS join_using_alias ] }
  | from_item NATURAL join_type from_item
  | from_item CROSS JOIN from_item

join_type:
    [ INNER ] JOIN
  | LEFT [ OUTER ] JOIN
  | RIGHT [ OUTER ] JOIN
  | FULL [ OUTER ] JOIN

grouping_element:
    ( )
  | expression
  | ( expression [, ...] )
  | ROLLUP ( { expression | ( expression [, ...] ) } [, ...] )
  | CUBE ( { expression | ( expression [, ...] ) } [, ...] )
  | GROUPING SETS ( grouping_element [, ...] )
```

### DISTINCT ON (PostgreSQL-Specific)

```sql
SELECT DISTINCT ON (column1, column2) column1, column2, column3
FROM table_name
ORDER BY column1, column2, column3;
```

### LATERAL Join (PostgreSQL-Specific)

```sql
SELECT *
FROM table1 t1
LEFT JOIN LATERAL (
    SELECT * FROM table2 t2
    WHERE t2.ref_id = t1.id
    ORDER BY t2.created_at DESC
    LIMIT 1
) sub ON true;
```

### Common Table Expressions (WITH Clause)

```sql
WITH [ RECURSIVE ] cte_name [ ( column_name [, ...] ) ] AS [ [ NOT ] MATERIALIZED ] ( query )
    [, ...]
SELECT ...

-- Recursive CTE example
WITH RECURSIVE cte AS (
    -- Non-recursive term
    SELECT 1 AS n
    UNION ALL
    -- Recursive term
    SELECT n + 1 FROM cte WHERE n < 10
)
SELECT * FROM cte;
```

### INSERT Statement

```sql
[ WITH [ RECURSIVE ] with_query [, ...] ]
INSERT INTO table_name [ AS alias ] [ ( column_name [, ...] ) ]
    [ OVERRIDING { SYSTEM | USER } VALUE ]
    { DEFAULT VALUES | VALUES ( { expression | DEFAULT } [, ...] ) [, ...] | query }
    [ ON CONFLICT [ conflict_target ] conflict_action ]
    [ RETURNING * | output_expression [ [ AS ] output_name ] [, ...] ]

conflict_target:
    ( { index_column_name | ( index_expression ) } [ COLLATE collation ] [ opclass ] [, ...] )
        [ WHERE index_predicate ]
  | ON CONSTRAINT constraint_name

conflict_action:
    DO NOTHING
  | DO UPDATE SET { column_name = { expression | DEFAULT } |
                    ( column_name [, ...] ) = [ ROW ] ( { expression | DEFAULT } [, ...] ) |
                    ( column_name [, ...] ) = ( sub-SELECT )
                  } [, ...]
        [ WHERE condition ]
```

### UPDATE Statement

```sql
[ WITH [ RECURSIVE ] with_query [, ...] ]
UPDATE [ ONLY ] table_name [ * ] [ [ AS ] alias ]
    SET { column_name = { expression | DEFAULT } |
          ( column_name [, ...] ) = [ ROW ] ( { expression | DEFAULT } [, ...] ) |
          ( column_name [, ...] ) = ( sub-SELECT )
        } [, ...]
    [ FROM from_item [, ...] ]
    [ WHERE condition | WHERE CURRENT OF cursor_name ]
    [ RETURNING * | output_expression [ [ AS ] output_name ] [, ...] ]
```

### DELETE Statement

```sql
[ WITH [ RECURSIVE ] with_query [, ...] ]
DELETE FROM [ ONLY ] table_name [ * ] [ [ AS ] alias ]
    [ USING from_item [, ...] ]
    [ WHERE condition | WHERE CURRENT OF cursor_name ]
    [ RETURNING * | output_expression [ [ AS ] output_name ] [, ...] ]
```

### MERGE Statement (PostgreSQL 15+)

```sql
[ WITH [ RECURSIVE ] with_query [, ...] ]
MERGE INTO [ ONLY ] target_table_name [ * ] [ [ AS ] target_alias ]
USING data_source ON join_condition
when_clause [...]
[ RETURNING * | output_expression [ [ AS ] output_name ] [, ...] ]

when_clause:
    WHEN MATCHED [ AND condition ] THEN { UPDATE SET ... | DELETE | DO NOTHING }
  | WHEN NOT MATCHED [ AND condition ] THEN { INSERT ... | DO NOTHING }
```

---

## PostgreSQL 16 DDL Syntax

**Emulation Note:** ScratchBird emulated PostgreSQL parsing rejects tablespace DDL and
TABLESPACE clauses. Tablespace creation/attachment is only supported by ScratchBird SQL.

### CREATE DATABASE

```sql
CREATE DATABASE name
    [ WITH ] [ OWNER [=] user_name ]
           [ TEMPLATE [=] template ]
           [ ENCODING [=] encoding ]
           [ STRATEGY [=] strategy ]
           [ LOCALE [=] locale ]
           [ LC_COLLATE [=] lc_collate ]
           [ LC_CTYPE [=] lc_ctype ]
           [ ICU_LOCALE [=] icu_locale ]
           [ ICU_RULES [=] icu_rules ]
           [ LOCALE_PROVIDER [=] locale_provider ]
           [ COLLATION_VERSION [=] collation_version ]
           [ TABLESPACE [=] tablespace_name ]
           [ ALLOW_CONNECTIONS [=] allowconn ]
           [ CONNECTION LIMIT [=] connlimit ]
           [ IS_TEMPLATE [=] istemplate ]
           [ OID [=] oid ]
```

**Emulation Note:** In ScratchBird emulation, `CREATE DATABASE` registers emulated schema metadata only and does not create physical database files.

### CREATE SCHEMA

```sql
CREATE SCHEMA schema_name [ AUTHORIZATION role_specification ] [ schema_element [ ... ] ]
CREATE SCHEMA AUTHORIZATION role_specification [ schema_element [ ... ] ]
CREATE SCHEMA IF NOT EXISTS schema_name [ AUTHORIZATION role_specification ]
CREATE SCHEMA IF NOT EXISTS AUTHORIZATION role_specification
```

### CREATE TABLE

```sql
CREATE [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } | UNLOGGED ] TABLE [ IF NOT EXISTS ] table_name
    ( [ { column_name data_type [ STORAGE { PLAIN | EXTERNAL | EXTENDED | MAIN | DEFAULT } ]
            [ COMPRESSION compression_method ] [ COLLATE collation ]
            [ column_constraint [ ... ] ] |
          table_constraint |
          LIKE source_table [ like_option ... ] }
        [, ... ]
    ] )
    [ INHERITS ( parent_table [, ... ] ) ]
    [ PARTITION BY { RANGE | LIST | HASH } ( { column_name | ( expression ) } [ COLLATE collation ] [ opclass ] [, ... ] ) ]
    [ USING method ]
    [ WITH ( storage_parameter [= value] [, ... ] ) | WITHOUT OIDS ]
    [ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
    [ TABLESPACE tablespace_name ]

column_constraint:
    [ CONSTRAINT constraint_name ]
    { NOT NULL |
      NULL |
      CHECK ( expression ) [ NO INHERIT ] |
      DEFAULT default_expr |
      GENERATED ALWAYS AS ( generation_expr ) STORED |
      GENERATED { ALWAYS | BY DEFAULT } AS IDENTITY [ ( sequence_options ) ] |
      UNIQUE [ NULLS [ NOT ] DISTINCT ] index_parameters |
      PRIMARY KEY index_parameters |
      REFERENCES reftable [ ( refcolumn ) ] [ MATCH FULL | MATCH PARTIAL | MATCH SIMPLE ]
        [ ON DELETE referential_action ] [ ON UPDATE referential_action ] }
    [ DEFERRABLE | NOT DEFERRABLE ] [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]

table_constraint:
    [ CONSTRAINT constraint_name ]
    { CHECK ( expression ) [ NO INHERIT ] |
      UNIQUE [ NULLS [ NOT ] DISTINCT ] ( column_name [, ... ] ) index_parameters |
      PRIMARY KEY ( column_name [, ... ] ) index_parameters |
      EXCLUDE [ USING index_method ] ( exclude_element WITH operator [, ... ] ) index_parameters [ WHERE ( predicate ) ] |
      FOREIGN KEY ( column_name [, ... ] ) REFERENCES reftable [ ( refcolumn [, ... ] ) ]
        [ MATCH FULL | MATCH PARTIAL | MATCH SIMPLE ] [ ON DELETE referential_action ] [ ON UPDATE referential_action ] }
    [ DEFERRABLE | NOT DEFERRABLE ] [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]

referential_action:
    NO ACTION | RESTRICT | CASCADE | SET NULL [ ( column_name [, ... ] ) ] | SET DEFAULT [ ( column_name [, ... ] ) ]
```

### CREATE TABLE AS

```sql
CREATE [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } | UNLOGGED ] TABLE [ IF NOT EXISTS ] table_name
    [ (column_name [, ...] ) ]
    [ USING method ]
    [ WITH ( storage_parameter [= value] [, ... ] ) | WITHOUT OIDS ]
    [ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
    [ TABLESPACE tablespace_name ]
    AS query
    [ WITH [ NO ] DATA ]
```

### Table Inheritance (PostgreSQL-Specific)

```sql
-- Create child table inheriting from parent
CREATE TABLE child_table (
    extra_column INTEGER
) INHERITS (parent_table);

-- Query only parent, not children
SELECT * FROM ONLY parent_table;

-- Remove inheritance
ALTER TABLE child_table NO INHERIT parent_table;
```

### Table Partitioning

```sql
-- Range partitioning
CREATE TABLE measurement (
    logdate DATE NOT NULL,
    peaktemp INT
) PARTITION BY RANGE (logdate);

CREATE TABLE measurement_y2020 PARTITION OF measurement
    FOR VALUES FROM ('2020-01-01') TO ('2021-01-01');

-- List partitioning
CREATE TABLE cities (
    city_id INT NOT NULL,
    name TEXT,
    region TEXT
) PARTITION BY LIST (region);

CREATE TABLE cities_east PARTITION OF cities
    FOR VALUES IN ('east');

-- Hash partitioning
CREATE TABLE orders (
    order_id INT NOT NULL,
    customer_id INT
) PARTITION BY HASH (order_id);

CREATE TABLE orders_p0 PARTITION OF orders
    FOR VALUES WITH (MODULUS 4, REMAINDER 0);
```

### CREATE INDEX

```sql
CREATE [ UNIQUE ] INDEX [ CONCURRENTLY ] [ [ IF NOT EXISTS ] name ] ON [ ONLY ] table_name [ USING method ]
    ( { column_name | ( expression ) } [ COLLATE collation ] [ opclass [ ( opclass_parameter = value [, ... ] ) ] ] [ ASC | DESC ] [ NULLS { FIRST | LAST } ] [, ...] )
    [ INCLUDE ( column_name [, ...] ) ]
    [ NULLS [ NOT ] DISTINCT ]
    [ WITH ( storage_parameter [= value] [, ... ] ) ]
    [ TABLESPACE tablespace_name ]
    [ WHERE predicate ]

-- Index methods: btree (default), hash, gist, spgist, gin, brin
```

### CREATE VIEW

```sql
CREATE [ OR REPLACE ] [ TEMP | TEMPORARY ] [ RECURSIVE ] VIEW name [ ( column_name [, ...] ) ]
    [ WITH ( view_option_name [= view_option_value] [, ... ] ) ]
    AS query
    [ WITH [ CASCADED | LOCAL ] CHECK OPTION ]
```

### CREATE MATERIALIZED VIEW

```sql
CREATE MATERIALIZED VIEW [ IF NOT EXISTS ] table_name
    [ (column_name [, ...] ) ]
    [ USING method ]
    [ WITH ( storage_parameter [= value] [, ... ] ) ]
    [ TABLESPACE tablespace_name ]
    AS query
    [ WITH [ NO ] DATA ]
```

### CREATE FUNCTION

```sql
CREATE [ OR REPLACE ] FUNCTION
    name ( [ [ argmode ] [ argname ] argtype [ { DEFAULT | = } default_expr ] [, ...] ] )
    [ RETURNS rettype
      | RETURNS TABLE ( column_name column_type [, ...] ) ]
  { LANGUAGE lang_name
    | TRANSFORM { FOR TYPE type_name } [, ... ]
    | WINDOW
    | { IMMUTABLE | STABLE | VOLATILE }
    | [ NOT ] LEAKPROOF
    | { CALLED ON NULL INPUT | RETURNS NULL ON NULL INPUT | STRICT }
    | { [ EXTERNAL ] SECURITY INVOKER | [ EXTERNAL ] SECURITY DEFINER }
    | PARALLEL { UNSAFE | RESTRICTED | SAFE }
    | COST execution_cost
    | ROWS result_rows
    | SUPPORT support_function
    | SET configuration_parameter { TO value | = value | FROM CURRENT }
    | AS 'definition'
    | AS 'obj_file', 'link_symbol'
    | sql_body
  } ...
```

### CREATE PROCEDURE

```sql
CREATE [ OR REPLACE ] PROCEDURE
    name ( [ [ argmode ] [ argname ] argtype [ { DEFAULT | = } default_expr ] [, ...] ] )
  { LANGUAGE lang_name
    | TRANSFORM { FOR TYPE type_name } [, ... ]
    | [ EXTERNAL ] SECURITY INVOKER | [ EXTERNAL ] SECURITY DEFINER
    | SET configuration_parameter { TO value | = value | FROM CURRENT }
    | AS 'definition'
    | AS 'obj_file', 'link_symbol'
    | sql_body
  } ...
```

### CREATE TRIGGER

```sql
CREATE [ OR REPLACE ] [ CONSTRAINT ] TRIGGER name { BEFORE | AFTER | INSTEAD OF } { event [ OR ... ] }
    ON table_name
    [ FROM referenced_table_name ]
    [ NOT DEFERRABLE | [ DEFERRABLE ] [ INITIALLY IMMEDIATE | INITIALLY DEFERRED ] ]
    [ REFERENCING { { OLD | NEW } TABLE [ AS ] transition_relation_name } [ ... ] ]
    [ FOR [ EACH ] { ROW | STATEMENT } ]
    [ WHEN ( condition ) ]
    EXECUTE { FUNCTION | PROCEDURE } function_name ( arguments )

event:
    INSERT | UPDATE [ OF column_name [, ... ] ] | DELETE | TRUNCATE
```

### CREATE TYPE

```sql
-- Composite type
CREATE TYPE name AS ( [ attribute_name data_type [ COLLATE collation ] [, ... ] ] )

-- Enum type
CREATE TYPE name AS ENUM ( [ 'label' [, ... ] ] )

-- Range type
CREATE TYPE name AS RANGE ( SUBTYPE = subtype [, option [, ... ] ] )

-- Base type (requires C functions)
CREATE TYPE name ( INPUT = input_function, OUTPUT = output_function [, ...] )
```

### CREATE DOMAIN

```sql
CREATE DOMAIN name [ AS ] data_type
    [ COLLATE collation ]
    [ DEFAULT expression ]
    [ constraint [ ... ] ]

constraint:
    [ CONSTRAINT constraint_name ]
    { NOT NULL | NULL | CHECK (expression) }
```

---

## PL/pgSQL Syntax

### Block Structure

```sql
[ <<label>> ]
[ DECLARE
    declarations ]
BEGIN
    statements
[ EXCEPTION
    WHEN condition [ OR condition ... ] THEN
        handler_statements
    [ WHEN condition [ OR condition ... ] THEN
          handler_statements
      ... ]
]
END [ label ];
```

### Variable Declaration

```sql
name [ CONSTANT ] type [ COLLATE collation_name ] [ NOT NULL ] [ { DEFAULT | := | = } expression ];

-- Examples
counter INTEGER := 0;
user_id users.id%TYPE;
row_data users%ROWTYPE;
```

### Control Flow

```sql
-- IF
IF condition THEN
    statements
[ ELSIF condition THEN
    statements ]
[ ELSE
    statements ]
END IF;

-- CASE (simple)
CASE expression
    WHEN value THEN statements
    [ WHEN value THEN statements ... ]
    [ ELSE statements ]
END CASE;

-- CASE (searched)
CASE
    WHEN condition THEN statements
    [ WHEN condition THEN statements ... ]
    [ ELSE statements ]
END CASE;

-- LOOP
[ <<label>> ]
LOOP
    statements
END LOOP [ label ];

-- EXIT
EXIT [ label ] [ WHEN condition ];

-- CONTINUE
CONTINUE [ label ] [ WHEN condition ];

-- WHILE
[ <<label>> ]
WHILE condition LOOP
    statements
END LOOP [ label ];

-- FOR (integer range)
[ <<label>> ]
FOR name IN [ REVERSE ] expression .. expression [ BY expression ] LOOP
    statements
END LOOP [ label ];

-- FOR (query)
[ <<label>> ]
FOR target IN query LOOP
    statements
END LOOP [ label ];

-- FOREACH (array)
[ <<label>> ]
FOREACH target [ SLICE number ] IN ARRAY expression LOOP
    statements
END LOOP [ label ];
```

### Returning Data

```sql
-- Return single value
RETURN expression;

-- Return query results
RETURN QUERY query;
RETURN QUERY EXECUTE command-string [ USING expression [, ... ] ];

-- Return next row (in SETOF function)
RETURN NEXT expression;
```

### Cursors

```sql
-- Declare cursor
DECLARE
    curs CURSOR [ ( arguments ) ] FOR query;
    curs refcursor;

-- Open cursor
OPEN curs [ ( argument_values ) ];
OPEN curs FOR query;
OPEN curs FOR EXECUTE query_string [ USING expression [, ... ] ];

-- Fetch from cursor
FETCH [ direction FROM ] cursor INTO target;
MOVE [ direction FROM ] cursor;

-- Close cursor
CLOSE cursor;

direction:
    NEXT | PRIOR | FIRST | LAST | ABSOLUTE count | RELATIVE count | FORWARD | BACKWARD
```

### Exception Handling

```sql
EXCEPTION
    WHEN division_by_zero THEN
        -- handle division by zero
    WHEN OTHERS THEN
        -- handle all other exceptions
        -- SQLERRM contains the error message
        -- SQLSTATE contains the error code
        RAISE NOTICE 'Error: %, State: %', SQLERRM, SQLSTATE;
```

### Raising Errors

```sql
RAISE [ level ] 'format' [, expression [, ... ]] [ USING option = expression [, ... ] ];
RAISE [ level ] condition_name [ USING option = expression [, ... ] ];
RAISE [ level ] SQLSTATE 'sqlstate' [ USING option = expression [, ... ] ];
RAISE [ level ] USING option = expression [, ... ];
RAISE ;

level: DEBUG | LOG | INFO | NOTICE | WARNING | EXCEPTION
option: MESSAGE | DETAIL | HINT | ERRCODE | COLUMN | CONSTRAINT | DATATYPE | TABLE | SCHEMA
```

---

## PostgreSQL 16 String Literals

### Standard Strings

```sql
'This is a string'
'It''s doubled apostrophe'  -- escaped single quote
```

### Escape Strings (E'...')

```sql
E'This is an escape string\nwith a newline'
E'Tab\there and backslash\\'
E'Unicode: \u0041 is A'
```

**Escape sequences:**
- `\b` - backspace
- `\f` - form feed
- `\n` - newline
- `\r` - carriage return
- `\t` - tab
- `\ooo` - octal byte value
- `\xhh` - hex byte value
- `\uxxxx` - 16-bit Unicode
- `\Uxxxxxxxx` - 32-bit Unicode

### Dollar-Quoted Strings

```sql
$$This is a dollar-quoted string$$
$tag$This allows $$ inside$tag$
$function$
BEGIN
    RETURN 'Hello';
END;
$function$
```

### Unicode Escape Strings (U&'...')

```sql
U&'d\0061t\+000061'  -- 'data'
U&'d!0061t!+000061' UESCAPE '!'  -- custom escape character
```

### Bit-String Constants

```sql
B'1001'      -- binary bit string
X'1FF'       -- hexadecimal bit string
```

---

## PostgreSQL System Catalogs

### pg_catalog Schema Tables (~70+ tables)

| Table | Description |
|-------|-------------|
| pg_class | Tables, indexes, sequences, views, etc. |
| pg_namespace | Schemas |
| pg_attribute | Table columns |
| pg_type | Data types |
| pg_index | Index information |
| pg_constraint | Constraints |
| pg_proc | Functions and procedures |
| pg_trigger | Triggers |
| pg_rewrite | Query rewrite rules |
| pg_depend | Object dependencies |
| pg_description | Object comments |
| pg_attrdef | Column defaults |
| pg_cast | Type casts |
| pg_operator | Operators |
| pg_opfamily | Operator families |
| pg_opclass | Operator classes |
| pg_am | Index access methods |
| pg_amop | Access method operators |
| pg_amproc | Access method procedures |
| pg_language | Procedural languages |
| pg_aggregate | Aggregate functions |
| pg_statistic | Planner statistics |
| pg_statistic_ext | Extended statistics |
| pg_inherits | Table inheritance |
| pg_sequence | Sequence parameters |
| pg_publication | Logical replication publications |
| pg_subscription | Logical replication subscriptions |
| pg_enum | Enum labels |
| pg_range | Range types |
| pg_collation | Collations |
| pg_conversion | Encoding conversions |
| pg_database | Databases |
| pg_tablespace | Tablespaces |
| pg_authid | Authorization identifiers (roles) |
| pg_auth_members | Role memberships |
| pg_shdescription | Shared object comments |
| pg_shdepend | Shared dependencies |
| pg_extension | Extensions |
| pg_foreign_data_wrapper | Foreign data wrappers |
| pg_foreign_server | Foreign servers |
| pg_user_mapping | User mappings for foreign servers |
| pg_foreign_table | Foreign tables |
| pg_policy | Row security policies |
| pg_replication_origin | Replication origins |
| pg_default_acl | Default access privileges |
| pg_init_privs | Initial privileges |
| pg_seclabel | Security labels |
| pg_shseclabel | Shared security labels |
| pg_event_trigger | Event triggers |
| pg_transform | Type transforms |
| pg_publication_rel | Publication-table mapping |
| pg_subscription_rel | Subscription-table mapping |
| pg_partitioned_table | Partitioned tables |

### pg_catalog System Views

| View | Description |
|------|-------------|
| pg_tables | All tables |
| pg_indexes | All indexes |
| pg_views | All views |
| pg_matviews | Materialized views |
| pg_sequences | Sequences |
| pg_stats | Planner statistics |
| pg_user | Users (wrapper around pg_authid) |
| pg_roles | Roles (wrapper around pg_authid) |
| pg_shadow | Password hashes (limited access) |
| pg_group | Groups (legacy) |
| pg_rules | Rewrite rules |
| pg_settings | Runtime configuration |
| pg_file_settings | Config file settings |
| pg_hba_file_rules | pg_hba.conf rules |
| pg_ident_file_mappings | pg_ident.conf mappings |
| pg_locks | Lock information |
| pg_cursors | Open cursors |
| pg_prepared_statements | Prepared statements |
| pg_prepared_xacts | Prepared transactions |
| pg_stat_activity | Session activity |
| pg_stat_replication | Replication statistics |
| pg_stat_wal_receiver | Write-after log (WAL) receiver statistics (optional post-gold) |
| pg_stat_database | Database statistics |
| pg_stat_all_tables | Table statistics |
| pg_stat_all_indexes | Index statistics |
| pg_statio_all_tables | Table I/O statistics |
| pg_statio_all_indexes | Index I/O statistics |
| pg_stat_user_tables | User table statistics |
| pg_stat_user_indexes | User index statistics |
| pg_stat_sys_tables | System table statistics |
| pg_stat_sys_indexes | System index statistics |
| pg_stat_bgwriter | Background writer statistics |
| pg_stat_progress_* | Progress monitoring views |

Note: pg_stat_* and pg_locks views MUST map to ScratchBird native monitoring views
defined in `operations/MONITORING_SQL_VIEWS.md`. Column-level mappings follow
`operations/MONITORING_DIALECT_MAPPINGS.md` to ensure consistent runtime data.

### information_schema Views (~60+ views)

| View | Description |
|------|-------------|
| schemata | Schemas |
| tables | Tables |
| columns | Columns |
| views | Views |
| sequences | Sequences |
| table_constraints | Constraints |
| referential_constraints | Foreign keys |
| check_constraints | Check constraints |
| key_column_usage | Key columns |
| constraint_column_usage | Constraint columns |
| constraint_table_usage | Constraint tables |
| table_privileges | Table privileges |
| column_privileges | Column privileges |
| routine_privileges | Routine privileges |
| usage_privileges | Usage privileges |
| role_table_grants | Role table grants |
| role_column_grants | Role column grants |
| routines | Functions/procedures |
| parameters | Routine parameters |
| triggers | Triggers |
| domains | Domains |
| domain_constraints | Domain constraints |
| user_defined_types | User types |
| attributes | Composite type attributes |
| element_types | Array element types |
| character_sets | Character sets |
| collations | Collations |
| collation_character_set_applicability | Collation-charset mapping |
| sql_features | SQL standard features |
| sql_implementation_info | Implementation info |
| sql_parts | SQL standard parts |
| sql_sizing | SQL sizing info |
| enabled_roles | Current user's roles |
| applicable_roles | Available roles |
| administrable_role_authorizations | Admin role grants |

---

## Implementation Phases

### Phase 1: PostgreSQL Lexer (Est. 25-35 hours)
1. Define all PostgreSQL token types (~90 types)
2. Implement keyword recognition (~130 keywords, ~96 reserved)
3. Implement PostgreSQL-specific literals:
   - Dollar-quoted strings ($...$, $tag$...$tag$)
   - Escape strings (E'...')
   - Unicode strings (U&'...')
   - Bit strings (B'...', X'...')
4. Implement PostgreSQL operators including `::`, `@>`, `<@`, `?`, `?|`, `?&`
5. Handle double-quoted identifiers
6. Unit tests for lexer

### Phase 2: PostgreSQL Parser Core (Est. 45-55 hours)
1. Statement dispatch (DDL, DML, DCL, Transaction)
2. Expression parsing with PostgreSQL operator precedence
3. Type parsing (PostgreSQL type names, arrays, domains)
4. Type cast operator (`::`) integration
5. Column/table reference parsing with double-quote quoting
6. Unit tests for each statement type

### Phase 3: PostgreSQL Parser DDL (Est. 40-50 hours)
1. CREATE/ALTER/DROP DATABASE/SCHEMA
2. CREATE/ALTER/DROP TABLE with:
   - INHERITS
   - PARTITION BY (RANGE, LIST, HASH)
   - Generated columns (GENERATED ALWAYS AS)
   - Identity columns (GENERATED AS IDENTITY)
3. CREATE/DROP INDEX (CONCURRENTLY, INCLUDE, partial indexes)
4. CREATE/ALTER/DROP VIEW and MATERIALIZED VIEW
5. CREATE/ALTER/DROP FUNCTION/PROCEDURE
6. CREATE/DROP TRIGGER (BEFORE, AFTER, INSTEAD OF, per-row, per-statement)
7. CREATE TYPE (composite, enum, range)
8. CREATE DOMAIN
9. CREATE SEQUENCE
10. COMMENT ON statements

### Phase 4: PostgreSQL Parser DML (Est. 40-50 hours)
1. SELECT with all PostgreSQL extensions:
   - DISTINCT ON
   - LATERAL joins
   - TABLESAMPLE
   - FETCH FIRST ... WITH TIES
   - FOR UPDATE/SHARE variants
2. INSERT with ON CONFLICT (DO NOTHING, DO UPDATE)
3. UPDATE with FROM clause
4. DELETE with USING clause
5. MERGE statement
6. RETURNING clause for all DML
7. WITH clause (CTEs) including RECURSIVE and MATERIALIZED
8. Window functions with frame clauses (ROWS, RANGE, GROUPS)
9. Array constructors and subscripts
10. Range constructors

### Phase 5: PostgreSQL Parser PL/pgSQL (Est. 45-55 hours)
1. Block structure (DECLARE, BEGIN, END, EXCEPTION)
2. Variable declaration (%TYPE, %ROWTYPE)
3. Control flow (IF, CASE, LOOP, WHILE, FOR, FOREACH)
4. EXIT and CONTINUE statements
5. RETURN, RETURN NEXT, RETURN QUERY
6. Cursor operations (DECLARE, OPEN, FETCH, MOVE, CLOSE)
7. RAISE statement with all options
8. EXCEPTION handling with condition names
9. EXECUTE for dynamic SQL
10. GET DIAGNOSTICS

### Phase 6: PostgreSQL Catalog Handler (Est. 40-50 hours)
1. Create `PgCatalogHandler` class
2. Implement pg_catalog tables (~70 tables)
3. Implement pg_catalog views (~35 views)
4. Implement information_schema views (~60 views)
5. Implement pg_stat_* views (key monitoring views)
6. Register handler in virtual_catalog.cpp
7. Unit tests for each system table

### Phase 7: Integration & Testing (Est. 25-35 hours)
1. Connect parser to SBLR bytecode generator
2. Verify bytecode generation for all statement types
3. End-to-end tests with PostgreSQL syntax
4. Test system catalog queries
5. Test with PostgreSQL clients (psql, pgAdmin)
6. Performance benchmarking

---

## File Organization

### New Files to Create

```
include/scratchbird/parser/postgresql/
├── pg_lexer.h          # PostgreSQL lexer interface
├── pg_parser.h         # PostgreSQL parser interface
├── pg_token.h          # PostgreSQL token types
└── pg_codegen.h        # PostgreSQL -> SBLR generator

src/parser/postgresql/
├── pg_lexer.cpp
├── pg_parser.cpp
├── pg_parser_ddl.cpp   # DDL parsing
├── pg_parser_dml.cpp   # DML parsing
├── pg_parser_plpgsql.cpp # PL/pgSQL parsing
└── pg_codegen.cpp      # SBLR generation

include/scratchbird/catalog/
└── pg_catalog.h

src/catalog/
└── pg_catalog.cpp

tests/unit/
├── test_pg_lexer.cpp
├── test_pg_parser_ddl.cpp
├── test_pg_parser_dml.cpp
├── test_pg_parser_plpgsql.cpp
└── test_pg_catalog.cpp
```

### Files to Modify

```
src/catalog/virtual_catalog.cpp     # Register PgCatalogHandler
include/scratchbird/catalog/virtual_catalog.h
tests/CMakeLists.txt                # Add new test files
src/CMakeLists.txt                  # Add new source files
```

---

## Total Estimated Effort

| Phase | Hours |
|-------|-------|
| Phase 1: Lexer | 25-35 |
| Phase 2: Parser Core | 45-55 |
| Phase 3: Parser DDL | 40-50 |
| Phase 4: Parser DML | 40-50 |
| Phase 5: PL/pgSQL | 45-55 |
| Phase 6: Catalog Handler | 40-50 |
| Phase 7: Integration | 25-35 |
| **Total** | **260-330 hours** |

---

## Key Differences from MySQL Parser

| Feature | PostgreSQL | MySQL |
|---------|------------|-------|
| Reserved keywords | ~96 (~74%) | ~262 (~40%) |
| Identifier quoting | Double quotes ("") | Backticks (``) |
| Type cast | `::` operator | CAST() only |
| Case sensitivity | Lowercase by default | Configurable |
| Boolean literals | TRUE/FALSE/t/f/on/off | TRUE/FALSE/1/0 |
| String escapes | E'...' prefix | Backslash in strings |
| Dollar quoting | Yes ($...$) | No |
| Table inheritance | Yes (INHERITS) | No |
| DISTINCT ON | Yes | No |
| LATERAL joins | Yes | Limited (8.0.14+) |
| RETURNING clause | All DML | No (use triggers) |
| ON CONFLICT | Yes | ON DUPLICATE KEY |
| Range types | Built-in | No |
| Array types | Built-in | JSON arrays only |
| SERIAL types | Pseudo-type for sequences | AUTO_INCREMENT |
| JSON operators | Extensive (`@>`, `?`, etc.) | `->`, `->>` only |
| Full text search | tsvector/tsquery | FULLTEXT indexes |

---

## Success Criteria

1. PostgreSQL SQL syntax parses correctly (DDL, DML, PL/pgSQL)
2. All pg_catalog tables return correct data
3. All information_schema views return correct data
4. Key pg_stat_* views return useful data
5. Existing ScratchBird tests continue to pass
6. psql can connect and query
7. pgAdmin can connect and browse
8. Performance comparable to ScratchBird V2 parser

---

## References

- [PostgreSQL 16 Documentation](https://www.postgresql.org/docs/16/)
- [PostgreSQL Keywords](https://www.postgresql.org/docs/current/sql-keywords-appendix.html)
- [PostgreSQL Data Types](https://www.postgresql.org/docs/current/datatype.html)
- [PostgreSQL Functions](https://www.postgresql.org/docs/current/functions.html)
- [PostgreSQL SQL Commands](https://www.postgresql.org/docs/current/sql-commands.html)
- [PostgreSQL System Catalogs](https://www.postgresql.org/docs/current/catalogs.html)
- [PL/pgSQL](https://www.postgresql.org/docs/current/plpgsql.html)
- [PostgreSQL Lexical Structure](https://www.postgresql.org/docs/current/sql-syntax-lexical.html)
- PostgreSQL Emulation Protocol Behavior: `docs/specifications/wire_protocols/POSTGRESQL_EMULATION_BEHAVIOR.md`
- SBLR Opcodes: `include/scratchbird/sblr/opcodes.h`
- Schema Architecture: `docs/specifications/SCHEMA_ARCHITECTURE.md`
- Emulated Parser Spec: `docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
