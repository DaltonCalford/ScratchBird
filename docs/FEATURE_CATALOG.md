# ScratchBird Feature Catalog

This catalog is a consolidated, high-level feature inventory for ScratchBird.
It points to authoritative specs for the exhaustive definitions, but enumerates
all major feature surfaces in one place for release readiness reviews.

## Source of Truth

- SQL grammar and statement inventory: `docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- Core SQL language: `docs/specifications/parser/ScratchBird SQL Language Specification - Master Document.md`
- Type system: `docs/specifications/types/README.md` and `docs/specifications/types/03_TYPES_AND_DOMAINS.md`
- Index system: `docs/specifications/indexes/README.md`
- SBLR opcode coverage: `docs/specifications/sblr/SBLR_OPCODE_REGISTRY.md`
- Emulated dialects: `docs/specifications/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- Emulated protocol behavior: `docs/specifications/wire_protocols/README.md`

## Dialects and Protocols

- Native ScratchBird (V2) SQL parser + SBLR bytecode.
- Emulated SQL parsers: Firebird 5, PostgreSQL 16, MySQL 8.0.
- Wire protocols: ScratchBird native, Firebird, PostgreSQL, MySQL.
- IPC contract: parser <-> server via SBWP IPC protocol.

## Schema Tree (Recursive/Emulated Layout)

ScratchBird separates **database** (physical storage root) from **schema**
(logical namespace). Schemas are recursive and can nest under other schemas.

- **Database**: the physical storage root (files/tablespaces and catalog).
- **Schema**: a logical namespace within a database; schemas can contain
  subschemas and objects (recursive tree).
- **Authoritative catalog**: `sys.*` (native ScratchBird system catalog).
- **SQL standard views**: `information_schema.*`.
- **Emulated catalog surfaces** (views mapped to `sys.*`):
  - PostgreSQL: `pg_catalog.*` and `pg_*`.
  - MySQL: `mysql.*` and `information_schema.*`.
  - Firebird: `rdb$*` system tables + `mon$*` views.
- **Emulated database tree** (per server + database name):
  - `/<dialect>/<server>/<db>/...` mapped to ScratchBird storage objects.
  - Emulated parsers never create physical files; only the native engine
    creates on-disk objects. Emulated DDL maps into the scratchbird tree.

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

## Data Types

Types are implemented via the core type system and domain system. Full details
are in `docs/specifications/types/README.md`.

### Numeric
- Integers: SMALLINT, INTEGER, BIGINT, INT128.
- Unsigned integers: UINT8, UINT16, UINT32, UINT64.
- Fixed-point: DECIMAL(p,s), NUMERIC(p,s), MONEY.
- IEEE decimal: DECFLOAT(16), DECFLOAT(34).
- Floating-point: REAL, DOUBLE PRECISION.

### Character and Binary
- CHAR(n), VARCHAR(n), TEXT.
- BYTEA, BINARY(n), BLOB.
- Character sets and collations (I18N catalogs; see `types/character_sets_and_collations.md`).

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
- Arrays: `datatype[]` (multi-dimensional).
- Composite/record types (row types).
- Enum domains, set domains, variant domains.
- Range types (PostgreSQL-compatible ranges).
- Network types: INET, CIDR, MACADDR, MACADDR8.
- Geometric types: POINT, LINE, LINESTRING, POLYGON, CIRCLE, MULTI* (see `types/MULTI_GEOMETRY_TYPES_SPEC.md`).
- Vector type for AI/ML search (HNSW/IVF indexes).

## Operators (Core Surface)

Operator definitions are authoritative in
`docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`.

### Native V2 Operators (from BNF)

Arithmetic:
`+`, `-`, `*`, `/`, `%`, `^`, `**`, `DIV`

Comparison and predicates:
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

Logical:
`AND`, `OR`, `NOT`

String:
`||`, `CONCAT`

Bitwise:
`&`, `|`, `#`, `~`, `<<`, `>>`

### Array/Range/JSON/Text Search Operators

- Array subscripts: `[]`
- Array/range containment and overlap: `@>`, `<@`, `&&`
- JSON path: `->`, `->>`, `#>`, `#>>`
- JSON key existence: `?`, `?|`, `?&`
- Full-text match: `@@`

Spatial operator sets are defined in `docs/specifications/types/MULTI_GEOMETRY_TYPES_SPEC.md`.

## Functions (Detailed Catalog)

### ScratchBird Native Built-ins (V2 Core)

This list is the core (native) ScratchBird function surface. It is derived
from `docs/specifications/core/INTERNAL_FUNCTIONS.md` and the native SQL
grammar. Emulated dialect lists appear later and are not substitutes for the
native core.

Aggregates:
`COUNT`, `SUM`, `AVG`, `MIN`, `MAX`, `ARRAY_AGG`, `STRING_AGG`,
`STDDEV`, `STDDEV_POP`, `STDDEV_SAMP`, `VAR_POP`, `VAR_SAMP`, `VARIANCE`,
`CORR`, `COVAR_POP`, `COVAR_SAMP`, `REGR_AVGX`, `REGR_AVGY`, `REGR_COUNT`,
`REGR_INTERCEPT`, `REGR_R2`, `REGR_SLOPE`, `REGR_SXX`, `REGR_SXY`, `REGR_SYY`

Window:
`ROW_NUMBER`, `RANK`, `DENSE_RANK`, `LAG`, `LEAD`, `FIRST_VALUE`,
`LAST_VALUE`, `NTH_VALUE`, `NTILE`, `PERCENT_RANK`, `CUME_DIST`

String:
`LTRIM`, `RTRIM`, `TRIM`, `CONCAT`, `CONCAT_WS`, `SUBSTRING`, `POSITION`,
`LENGTH`, `CHAR_LENGTH`, `LOWER`, `UPPER`, `REPLACE`, `STARTS_WITH`, `ENDS_WITH`,
`CONVERT` (character set conversion)

Numeric:
`ABS`, `CEIL`, `FLOOR`, `ROUND`, `POWER`, `SQRT`, `LOG`, `EXP`, `MOD`,
`SIGN`, `LEAST`, `GREATEST`

Date/Time:
`NOW`, `CURRENT_DATE`, `CURRENT_TIME`, `CURRENT_TIMESTAMP`, `DATE_ADD`,
`DATE_SUB`, `DATE_DIFF`, `DATE_TRUNC`, `EXTRACT`, `TO_CHAR`, `TO_DATE`,
`TO_TIMESTAMP`, `AT TIME ZONE`

JSON/JSONB:
`JSON_EXTRACT`, `JSON_ARRAY`, `JSON_OBJECT`, `JSON_TABLE`, `JSON_QUERY`,
`JSON_VALUE`, `JSON_EXISTS`, `JSON_HAS_KEY`, `JSONB_*` (JSONB variants)

Array (core list from `POSTGRESQL_ARRAY_TYPE_SPEC.md`):
`ARRAY_AGG`, `ARRAY_APPEND`, `ARRAY_PREPEND`, `ARRAY_CAT`, `ARRAY_DIMS`,
`ARRAY_FILL`, `ARRAY_LENGTH`, `ARRAY_LOWER`, `ARRAY_UPPER`, `ARRAY_NDIMS`,
`ARRAY_POSITION`, `ARRAY_POSITIONS`, `ARRAY_REMOVE`, `ARRAY_REPLACE`,
`ARRAY_SLICE`, `ARRAY_TO_STRING`, `CARDINALITY`, `STRING_TO_ARRAY`,
`TRIM_ARRAY`, `UNNEST`

Spatial (core list from `MULTI_GEOMETRY_TYPES_SPEC.md`):
`ST_POINT`, `ST_MAKELINE`, `ST_MAKEPOLYGON`, `ST_ASTEXT`, `ST_ASBINARY`,
`ST_GEOMETRYTYPE`, `ST_ISVALID`, `ST_Multi`, `ST_Collect`, `ST_NumGeometries`,
`ST_GeometryN`, `ST_IsEmpty`, `ST_IsSimple`, `ST_IsClosed`, `ST_Dump`,
`ST_CollectionExtract`, `ST_Union`

System:
`CURRENT_USER`, `CURRENT_ROLE`, `CURRENT_SCHEMA`, `SESSION_USER`, `VERSION`

Control Flow:
`CASE`, `COALESCE`, `NULLIF`

UUID:
`GEN_UUID_V1`, `GEN_UUID_V4`, `GEN_UUID_V7`, `UUID_TO_STRING`

Text Search:
`TO_TSVECTOR`, `TO_TSQUERY`, `PLAINTO_TSQUERY`, `PHRASETO_TSQUERY`

### Emulated PostgreSQL 16 Built-in Functions

Aggregate Functions:
`AVG`, `BIT_AND`, `BIT_OR`, `BIT_XOR`, `BOOL_AND`, `BOOL_OR`, `COUNT`,
`EVERY`, `JSON_AGG`, `JSON_OBJECT_AGG`, `JSONB_AGG`, `JSONB_OBJECT_AGG`,
`MAX`, `MIN`, `STRING_AGG`, `SUM`, `XMLAGG`, `ARRAY_AGG`

Statistical Aggregates:
`CORR`, `COVAR_POP`, `COVAR_SAMP`, `REGR_AVGX`, `REGR_AVGY`, `REGR_COUNT`,
`REGR_INTERCEPT`, `REGR_R2`, `REGR_SLOPE`, `REGR_SXX`, `REGR_SXY`,
`REGR_SYY`, `STDDEV`, `STDDEV_POP`, `STDDEV_SAMP`, `VAR_POP`, `VAR_SAMP`,
`VARIANCE`

Ordered-Set Aggregates:
`MODE`, `PERCENTILE_CONT`, `PERCENTILE_DISC`

Window Functions:
`CUME_DIST`, `DENSE_RANK`, `FIRST_VALUE`, `LAG`, `LAST_VALUE`, `LEAD`,
`NTH_VALUE`, `NTILE`, `PERCENT_RANK`, `RANK`, `ROW_NUMBER`

String Functions:
`ASCII`, `BIT_LENGTH`, `BTRIM`, `CHAR_LENGTH`, `CHARACTER_LENGTH`, `CHR`,
`CONCAT`, `CONCAT_WS`, `CONVERT`, `CONVERT_FROM`, `CONVERT_TO`, `DECODE`,
`ENCODE`, `FORMAT`, `INITCAP`, `LEFT`, `LENGTH`, `LOWER`, `LPAD`, `LTRIM`,
`MD5`, `NORMALIZE`, `OCTET_LENGTH`, `OVERLAY`, `PARSE_IDENT`,
`PG_CLIENT_ENCODING`, `POSITION`, `QUOTE_IDENT`, `QUOTE_LITERAL`,
`QUOTE_NULLABLE`, `REGEXP_COUNT`, `REGEXP_INSTR`, `REGEXP_LIKE`,
`REGEXP_MATCH`, `REGEXP_MATCHES`, `REGEXP_REPLACE`, `REGEXP_SPLIT_TO_ARRAY`,
`REGEXP_SPLIT_TO_TABLE`, `REGEXP_SUBSTR`, `REPEAT`, `REPLACE`, `REVERSE`,
`RIGHT`, `RPAD`, `RTRIM`, `SPLIT_PART`, `STARTS_WITH`, `STRING_TO_ARRAY`,
`STRING_TO_TABLE`, `STRPOS`, `SUBSTR`, `SUBSTRING`, `TO_ASCII`, `TO_HEX`,
`TRANSLATE`, `TRIM`, `UNISTR`, `UPPER`

Numeric Functions:
`ABS`, `ACOS`, `ACOSD`, `ACOSH`, `ASIN`, `ASIND`, `ASINH`, `ATAN`, `ATAN2`,
`ATAN2D`, `ATAND`, `ATANH`, `CBRT`, `CEIL`, `CEILING`, `COS`, `COSD`,
`COSH`, `COT`, `COTD`, `DEGREES`, `DIV`, `EXP`, `FACTORIAL`, `FLOOR`,
`GCD`, `LCM`, `LN`, `LOG`, `LOG10`, `MIN_SCALE`, `MOD`, `PI`, `POWER`,
`RADIANS`, `RANDOM`, `ROUND`, `SCALE`, `SETSEED`, `SIGN`, `SIN`, `SIND`,
`SINH`, `SQRT`, `TAN`, `TAND`, `TANH`, `TRIM_SCALE`, `TRUNC`,
`WIDTH_BUCKET`

Date/Time Functions:
`AGE`, `CLOCK_TIMESTAMP`, `CURRENT_DATE`, `CURRENT_TIME`,
`CURRENT_TIMESTAMP`, `DATE_BIN`, `DATE_PART`, `DATE_TRUNC`, `EXTRACT`,
`ALTER_ELEMENT`, `ISFINITE`, `JUSTIFY_DAYS`, `JUSTIFY_HOURS`,
`JUSTIFY_INTERVAL`, `LOCALTIME`, `LOCALTIMESTAMP`, `MAKE_DATE`,
`MAKE_INTERVAL`, `MAKE_TIME`, `MAKE_TIMESTAMP`, `MAKE_TIMESTAMPTZ`, `NOW`,
`PG_SLEEP`, `PG_SLEEP_FOR`, `PG_SLEEP_UNTIL`, `STATEMENT_TIMESTAMP`,
`TIMEOFDAY`, `TIMEZONE`, `TO_TIMESTAMP`, `TRANSACTION_TIMESTAMP`

JSON Functions:
`ARRAY_TO_JSON`, `JSON_AGG`, `JSON_ARRAY`, `JSON_ARRAY_ELEMENTS`,
`JSON_ARRAY_ELEMENTS_TEXT`, `JSON_ARRAY_LENGTH`, `JSON_BUILD_ARRAY`,
`JSON_BUILD_OBJECT`, `JSON_EACH`, `JSON_EACH_TEXT`, `JSON_EXTRACT_PATH`,
`JSON_EXTRACT_PATH_TEXT`, `JSON_OBJECT`, `JSON_OBJECT_AGG`,
`JSON_OBJECT_KEYS`, `JSON_POPULATE_RECORD`, `JSON_POPULATE_RECORDSET`,
`JSON_QUERY`, `JSON_SCALAR`, `JSON_SERIALIZE`, `JSON_STRIP_NULLS`,
`JSON_TABLE`, `JSON_TO_RECORD`, `JSON_TO_RECORDSET`, `JSON_TYPEOF`,
`JSON_VALUE`, `ROW_TO_JSON`, `TO_JSON`

JSONB Functions:
`JSONB_AGG`, `JSONB_ARRAY_ELEMENTS`, `JSONB_ARRAY_ELEMENTS_TEXT`,
`JSONB_ARRAY_LENGTH`, `JSONB_BUILD_ARRAY`, `JSONB_BUILD_OBJECT`,
`JSONB_EACH`, `JSONB_EACH_TEXT`, `JSONB_EXTRACT_PATH`,
`JSONB_EXTRACT_PATH_TEXT`, `JSONB_INSERT`, `JSONB_OBJECT`,
`JSONB_OBJECT_AGG`, `JSONB_OBJECT_KEYS`, `JSONB_PATH_EXISTS`,
`JSONB_PATH_EXISTS_TZ`, `JSONB_PATH_MATCH`, `JSONB_PATH_MATCH_TZ`,
`JSONB_PATH_QUERY`, `JSONB_PATH_QUERY_ARRAY`, `JSONB_PATH_QUERY_ARRAY_TZ`,
`JSONB_PATH_QUERY_FIRST`, `JSONB_PATH_QUERY_FIRST_TZ`,
`JSONB_PATH_QUERY_TZ`, `JSONB_POPULATE_RECORD`, `JSONB_POPULATE_RECORDSET`,
`JSONB_PRETTY`, `JSONB_SET`, `JSONB_SET_LAX`, `JSONB_STRIP_NULLS`,
`JSONB_TO_RECORD`, `JSONB_TO_RECORDSET`, `JSONB_TYPEOF`, `TO_JSONB`

Array Functions:
`ARRAY_AGG`, `ARRAY_APPEND`, `ARRAY_CAT`, `ARRAY_DIMS`, `ARRAY_FILL`,
`ARRAY_LENGTH`, `ARRAY_LOWER`, `ARRAY_NDIMS`, `ARRAY_POSITION`,
`ARRAY_POSITIONS`, `ARRAY_PREPEND`, `ARRAY_REMOVE`, `ARRAY_REPLACE`,
`ARRAY_SAMPLE`, `ARRAY_SHUFFLE`, `ARRAY_TO_STRING`, `ARRAY_UPPER`,
`CARDINALITY`, `STRING_TO_ARRAY`, `TRIM_ARRAY`, `UNNEST`

Range Functions:
`ISEMPTY`, `LOWER`, `LOWER_INC`, `LOWER_INF`, `RANGE_MERGE`, `UPPER`,
`UPPER_INC`, `UPPER_INF`

Control Flow:
`CASE`, `COALESCE`, `GREATEST`, `LEAST`, `NULLIF`

System/Object Information:
`CURRENT_CATALOG`, `CURRENT_DATABASE`, `CURRENT_QUERY`, `CURRENT_ROLE`,
`CURRENT_SCHEMA`, `CURRENT_SCHEMAS`, `CURRENT_USER`, `INET_CLIENT_ADDR`,
`INET_CLIENT_PORT`, `INET_SERVER_ADDR`, `INET_SERVER_PORT`, `PG_BACKEND_PID`,
`PG_BLOCKING_PIDS`, `PG_CONF_LOAD_TIME`, `PG_CURRENT_LOGFILE`,
`PG_IS_OTHER_TEMP_SCHEMA`, `PG_JIT_AVAILABLE`, `PG_LISTENING_CHANNELS`,
`PG_MY_TEMP_SCHEMA`, `PG_NOTIFICATION_QUEUE_USAGE`, `PG_POSTMASTER_START_TIME`,
`PG_SAFE_SNAPSHOT_BLOCKING_PIDS`, `PG_TRIGGER_DEPTH`, `SESSION_USER`,
`SYSTEM_USER`, `USER`, `VERSION`, `COL_DESCRIPTION`, `FORMAT_TYPE`,
`HAS_ANY_COLUMN_PRIVILEGE`, `HAS_COLUMN_PRIVILEGE`, `HAS_DATABASE_PRIVILEGE`,
`HAS_FOREIGN_DATA_WRAPPER_PRIVILEGE`, `HAS_FUNCTION_PRIVILEGE`,
`HAS_LANGUAGE_PRIVILEGE`, `HAS_PARAMETER_PRIVILEGE`, `HAS_SCHEMA_PRIVILEGE`,
`HAS_SEQUENCE_PRIVILEGE`, `HAS_SERVER_PRIVILEGE`, `HAS_TABLE_PRIVILEGE`,
`HAS_TABLESPACE_PRIVILEGE`, `HAS_TYPE_PRIVILEGE`, `OBJ_DESCRIPTION`,
`PG_DESCRIBE_OBJECT`, `PG_GET_CATALOG_FOREIGN_KEYS`, `PG_GET_CONSTRAINTDEF`,
`PG_GET_EXPR`, `PG_GET_FUNCTION_ARGUMENTS`, `PG_GET_FUNCTION_IDENTITY_ARGUMENTS`,
`PG_GET_FUNCTION_RESULT`, `PG_GET_FUNCTIONDEF`, `PG_GET_INDEXDEF`,
`PG_GET_KEYWORDS`, `PG_GET_OBJECT_ADDRESS`, `PG_GET_PARTKEYDEF`,
`PG_GET_RULEDEF`, `PG_GET_SERIAL_SEQUENCE`, `PG_GET_STATISTICSOBJDEF`,
`PG_GET_TRIGGERDEF`, `PG_GET_USERBYID`, `PG_GET_VIEWDEF`,
`PG_IDENTIFY_OBJECT`, `PG_IDENTIFY_OBJECT_AS_ADDRESS`,
`PG_INDEX_COLUMN_HAS_PROPERTY`, `PG_INDEX_HAS_PROPERTY`,
`PG_INDEXAM_HAS_PROPERTY`, `PG_OPTIONS_TO_TABLE`, `PG_SETTINGS_GET_FLAGS`,
`PG_TABLESPACE_DATABASES`, `PG_TABLESPACE_LOCATION`, `PG_TYPEOF`,
`SHOBJ_DESCRIPTION`, `TO_REGCLASS`, `TO_REGCOLLATION`, `TO_REGNAMESPACE`,
`TO_REGOPER`, `TO_REGOPERATOR`, `TO_REGPROC`, `TO_REGPROCEDURE`,
`TO_REGROLE`, `TO_REGTYPE`

### Emulated MySQL 8.0 Built-in Functions

Aggregate Functions:
`AVG`, `BIT_AND`, `BIT_OR`, `BIT_XOR`, `COUNT`, `GROUP_CONCAT`,
`JSON_ARRAYAGG`, `JSON_OBJECTAGG`, `MAX`, `MIN`, `STD`, `STDDEV`,
`STDDEV_POP`, `STDDEV_SAMP`, `SUM`, `VAR_POP`, `VAR_SAMP`, `VARIANCE`

Window Functions:
`CUME_DIST`, `DENSE_RANK`, `FIRST_VALUE`, `LAG`, `LAST_VALUE`, `LEAD`,
`NTH_VALUE`, `NTILE`, `PERCENT_RANK`, `RANK`, `ROW_NUMBER`

String Functions:
`ASCII`, `BIN`, `BIT_LENGTH`, `CHAR`, `CHAR_LENGTH`, `CHARACTER_LENGTH`,
`CONCAT`, `CONCAT_WS`, `ELT`, `EXPORT_SET`, `FIELD`, `FIND_IN_SET`,
`FORMAT`, `FROM_BASE64`, `HEX`, `INSERT`, `INSTR`, `LCASE`, `LEFT`,
`LENGTH`, `LOAD_FILE`, `LOCATE`, `LOWER`, `LPAD`, `LTRIM`, `MAKE_SET`,
`MATCH`, `MID`, `OCT`, `OCTET_LENGTH`, `ORD`, `POSITION`, `QUOTE`,
`REPEAT`, `REPLACE`, `REVERSE`, `RIGHT`, `RPAD`, `RTRIM`, `SOUNDEX`,
`SPACE`, `STRCMP`, `SUBSTR`, `SUBSTRING`, `SUBSTRING_INDEX`,
`TO_BASE64`, `TRIM`, `UCASE`, `UNHEX`, `UPPER`, `WEIGHT_STRING`

Numeric Functions:
`ABS`, `ACOS`, `ASIN`, `ATAN`, `ATAN2`, `CEIL`, `CEILING`, `CONV`, `COS`,
`COT`, `CRC32`, `DEGREES`, `EXP`, `FLOOR`, `LN`, `LOG`, `LOG2`, `LOG10`,
`MOD`, `PI`, `POW`, `POWER`, `RADIANS`, `RAND`, `ROUND`, `SIGN`, `SIN`,
`SQRT`, `TAN`, `TRUNCATE`

Date/Time Functions:
`ADDDATE`, `ADDTIME`, `CONVERT_TZ`, `CURDATE`, `CURRENT_DATE`,
`CURRENT_TIME`, `CURRENT_TIMESTAMP`, `CURTIME`, `DATE`, `DATE_ADD`,
`DATE_FORMAT`, `DATE_SUB`, `DATEDIFF`, `DAY`, `DAYNAME`, `DAYOFMONTH`,
`DAYOFWEEK`, `DAYOFYEAR`, `EXTRACT`, `ALTER_ELEMENT`, `FROM_DAYS`,
`FROM_UNIXTIME`, `GET_FORMAT`, `HOUR`, `LAST_DAY`, `LOCALTIME`,
`LOCALTIMESTAMP`, `MAKEDATE`, `MAKETIME`, `MICROSECOND`, `MINUTE`,
`MONTH`, `MONTHNAME`, `NOW`, `PERIOD_ADD`, `PERIOD_DIFF`, `QUARTER`,
`SEC_TO_TIME`, `SECOND`, `STR_TO_DATE`, `SUBDATE`, `SUBTIME`, `SYSDATE`,
`TIME`, `TIME_FORMAT`, `TIME_TO_SEC`, `TIMEDIFF`, `TIMESTAMP`,
`TIMESTAMPADD`, `TIMESTAMPDIFF`, `TO_DAYS`, `TO_SECONDS`,
`UNIX_TIMESTAMP`, `UTC_DATE`, `UTC_TIME`, `UTC_TIMESTAMP`, `WEEK`,
`WEEKDAY`, `WEEKOFYEAR`, `YEAR`, `YEARWEEK`

JSON Functions:
`JSON_ARRAY`, `JSON_ARRAYAGG`, `JSON_ARRAY_APPEND`, `JSON_ARRAY_INSERT`,
`JSON_CONTAINS`, `JSON_CONTAINS_PATH`, `JSON_DEPTH`, `JSON_EXTRACT`,
`JSON_INSERT`, `JSON_KEYS`, `JSON_LENGTH`, `JSON_MERGE_PATCH`,
`JSON_MERGE_PRESERVE`, `JSON_OBJECT`, `JSON_OBJECTAGG`, `JSON_OVERLAPS`,
`JSON_PRETTY`, `JSON_QUOTE`, `JSON_REMOVE`, `JSON_REPLACE`,
`JSON_SCHEMA_VALID`, `JSON_SCHEMA_VALIDATION_REPORT`, `JSON_SEARCH`,
`JSON_SET`, `JSON_STORAGE_FREE`, `JSON_STORAGE_SIZE`, `JSON_TABLE`,
`JSON_TYPE`, `JSON_UNQUOTE`, `JSON_VALID`, `JSON_VALUE`, `MEMBER OF`

Control Flow:
`CASE`, `IF`, `IFNULL`, `NULLIF`, `COALESCE`

Cast/Convert:
`BINARY`, `CAST`, `CONVERT`

Information:
`BENCHMARK`, `CHARSET`, `COERCIBILITY`, `COLLATION`, `CONNECTION_ID`,
`CURRENT_ROLE`, `CURRENT_USER`, `DATABASE`, `FOUND_ROWS`, `ICU_VERSION`,
`LAST_INSERT_ID`, `ROW_COUNT`, `SCHEMA`, `SESSION_USER`, `SYSTEM_USER`,
`USER`, `VERSION`

Encryption:
`AES_DECRYPT`, `AES_ENCRYPT`, `MD5`, `RANDOM_BYTES`, `SHA1`, `SHA2`,
`STATEMENT_DIGEST`, `STATEMENT_DIGEST_TEXT`

### Emulated Firebird 5.0 Built-in Functions

Firebird function sets are implemented to match the Firebird 5.0 reference.
The list below is extracted from the Firebird 5.0 reference split docs:
`docs/specifications/reference/firebird/firebird_docs_split/`.

Scalar Functions:
`RDB$GET_CONTEXT`, `RDB$SET_CONTEXT`, `ABS`, `ACOS`, `ACOSH`, `ASIN`, `ASINH`,
`ATAN`, `ATAN2`, `ATANH`, `CEIL`, `COS`, `COSH`, `COT`, `EXP`, `FLOOR`, `LN`,
`LOG`, `LOG10`, `MOD`, `PI`, `POWER`, `RAND`, `ROUND`, `SIGN`, `SIN`, `SINH`,
`SQRT`, `TAN`, `TANH`, `TRUNC`, `ASCII_CHAR`, `ASCII_VAL`, `BASE64_DECODE`,
`BASE64_ENCODE`, `BIT_LENGTH`, `BLOB_APPEND`, `CHAR_LENGTH`, `CRYPT_HASH`,
`HASH`, `HEX_DECODE`, `HEX_ENCODE`, `LEFT`, `LOWER`, `LPAD`, `OCTET_LENGTH`,
`OVERLAY`, `POSITION`, `REPLACE`, `REVERSE`, `RIGHT`, `RPAD`, `SUBSTRING`,
`TRIM`, `UNICODE_CHAR`, `UNICODE_VAL`, `UPPER`, `DATEADD`, `DATEDIFF`,
`EXTRACT`, `FIRST_DAY`, `LAST_DAY`, `CAST`, `BIN_AND`, `BIN_NOT`, `BIN_OR`,
`BIN_SHL`, `BIN_SHR`, `BIN_XOR`, `CHAR_TO_UUID`, `GEN_UUID`, `UUID_TO_CHAR`,
`GEN_ID`, `COALESCE`, `DECODE`, `IIF`, `MAXVALUE`, `MINVALUE`, `NULLIF`,
`COMPARE_DECFLOAT`, `NORMALIZE_DECFLOAT`, `QUANTIZE`, `TOTALORDER`, `DECRYPT`,
`ENCRYPT`, `RSA_DECRYPT`, `RSA_ENCRYPT`, `RSA_PRIVATE`, `RSA_PUBLIC`,
`RSA_SIGN_HASH`, `RSA_VERIFY_HASH`, `MAKE_DBKEY`, `RDB$ERROR`,
`RDB$GET_TRANSACTION_CN`, `RDB$ROLE_IN_USE`

Aggregate Functions:
`RDB$SYSTEM_PRIVILEGE`, `AVG`, `COUNT`, `LIST`, `MAX`, `MIN`, `SUM`, `CORR`,
`COVAR_POP`, `COVAR_SAMP`, `STDDEV_POP`, `STDDEV_SAMP`, `VAR_POP`, `VAR_SAMP`,
`REGR_AVGX`, `REGR_AVGY`, `REGR_COUNT`, `REGR_INTERCEPT`, `REGR_R2`,
`REGR_SLOPE`, `REGR_SXX`, `REGR_SXY`, `REGR_SYY`

Window Functions:
`CUME_DIST`, `DENSE_RANK`, `NTILE`, `PERCENT_RANK`, `RANK`, `ROW_NUMBER`,
`FIRST_VALUE`, `LAG`, `LAST_VALUE`, `LEAD`, `NTH_VALUE`

## Index Types

Core index types (implemented in V2 and the engine):

- BTREE, HASH, GIN, GIST, SPGIST, BRIN, RTREE.
- HNSW, IVF (vector search).
- BITMAP, COLUMNSTORE, LSM.
- FULLTEXT (inverted index), ZONEMAP.

Advanced index specifications exist in `docs/specifications/indexes/` for
optional/extended types (e.g., ZORDER, JSON_PATH).

## SQL Statement Coverage

### DDL
- CREATE/ALTER/DROP for databases, schemas, tables, views, indexes, sequences,
  functions, procedures, triggers, domains, types, roles, policies, jobs.

### DML
- SELECT, INSERT, UPDATE, DELETE, MERGE, COPY, TRUNCATE.

### Transaction Control
- BEGIN, COMMIT, ROLLBACK, SAVEPOINT, RELEASE SAVEPOINT, SET TRANSACTION.

### Utility
- ANALYZE, EXPLAIN, SHOW/SET, DESCRIBE, COMMENT.

### Security (DCL)
- GRANT, REVOKE, role management, security policies.

### Procedural SQL (PSQL)
- Blocks, variables, control flow, exception handling, cursors.

## Monitoring and Observability

- `sys.performance` metrics surface (engine-level).
- Emulated monitoring views (e.g., Firebird `mon$*`) mapped to sys tables.
- Structured logging + audit trail.

## Scheduler and Jobs

- Catalog-backed job definitions and run history.
- Cron and dependency gating.
- Runtime settings via `ALTER SYSTEM SET scheduler.*`.

## Backup and Recovery

- Full, incremental, differential backup.
- PITR support (spec-level).
- Tablespace-aware restore paths.

## Security

- SCRAM authentication, TLS 1.3.
- Row-level and column-level security.
- Data masking, encryption at rest, audit logging.

## Compatibility Suites

- SQL compatibility harnesses for PostgreSQL, MySQL, Firebird, and native V2.
- Compatibility scripts in `tests/compatibility/` (13,303 SQL files).
