# MySQL Parser Statement Reference (Actual Implementation)

Purpose: Code-verified description of the MySQL parser (MySQL 8.x-like syntax) and the SBLR bytecode it emits, with executor compatibility notes.

Status: static code review snapshot; no runtime execution performed.

## If you only remember 5 things
- The MySQL parser emits SBLR bytecode directly; there is no semantic analyzer stage.
- Core DDL/DML bytecode formats do not match the executor (CREATE TABLE, SELECT/INSERT/UPDATE/DELETE/REPLACE), so most statements fail at execution time.
- DDL coverage is limited: CREATE DATABASE and CREATE TABLE are implemented; CREATE INDEX/VIEW/PROCEDURE/FUNCTION/TRIGGER are TODO; DROP TABLE/INDEX/VIEW and TRUNCATE are not implemented; ALTER TABLE only supports RENAME.
- Admin commands map to executor for SET AUTOCOMMIT, SHOW, DESCRIBE, and USE; generic SET variables fail because the executor only supports search_path.
- LOCK/UNLOCK TABLES are treated as no-ops (emit LITERAL_NULL).

## Scope and sources (code)
- Parser: `ScratchBird/src/parser/mysql/mysql_parser.cpp`
- Lexer: `ScratchBird/src/parser/mysql/mysql_lexer.cpp`
- Executor: `ScratchBird/src/sblr/executor.cpp`

## Statement coverage map
Legend: Y = parser implements, P = partial, N = not implemented, X = executor format mismatch.

| Category | Statement | Parser | Executor compatibility | Notes |
| --- | --- | --- | --- | --- |
| DDL | CREATE DATABASE | Y | Y | EXT_CREATE_DATABASE (flags + path) |
| DDL | CREATE TABLE | Y | X | Emits IF NOT EXISTS byte and column format mismatch |
| DDL | CREATE INDEX / VIEW | N | N | TODO stubs |
| DDL | CREATE PROCEDURE/FUNCTION/TRIGGER | N | N | TODO stubs |
| DDL | ALTER TABLE | P | X | Only RENAME TO; emits EXT_RENAME/EXT_MOVE but other actions error |
| DDL | DROP DATABASE/SCHEMA | Y | Y | EXT_DROP_DATABASE |
| DDL | DROP TABLE/INDEX/VIEW | N | N | Not implemented |
| DDL | TRUNCATE | N | N | Not implemented |
| DML | SELECT | Y | X | DISTINCT flag + alias strings mismatch executor format |
| DML | INSERT | Y | X | Multi-row list, column ref qualifiers, alias handling mismatch |
| DML | UPDATE | Y | X | Table list + alias and column ref qualifiers mismatch |
| DML | DELETE | Y | X | Alias + ORDER/LIMIT mismatch executor expectations |
| DML | REPLACE | Y | X | Encoded as INSERT + EXT_ON_CONFLICT_DO_UPDATE (unsupported) |
| TCL | START/BEGIN, COMMIT, ROLLBACK, SAVEPOINT, RELEASE | Y | Y | Uses executor transaction opcodes |
| Session | SET AUTOCOMMIT | Y | Y | EXT_SET_AUTOCOMMIT |
| Session | SET variable | Y | X | Executor only supports search_path (USE) |
| Session | SHOW TABLES/DATABASES/COLUMNS/INDEXES/CREATE TABLE | Y | Y | EXT_SHOW_* opcodes |
| Session | DESCRIBE | Y | Y | EXT_DESCRIBE_TABLE |
| Session | USE | Y | Y | EXT_SET_VARIABLE search_path |
| Utility | LOCK/UNLOCK TABLES | P | Y | Emits LITERAL_NULL no-op |

## DDL details and lifecycles

### CREATE DATABASE
Parsed syntax (actual):
```sql
CREATE DATABASE [IF NOT EXISTS] name
  [DEFAULT] CHARACTER SET <name>
  [DEFAULT] COLLATE <name>
```
Emission (actual):
- Emits `EXT_CREATE_DATABASE` with flags byte and emulated path.
Executor compatibility: matches executor payload.

### CREATE TABLE
Parsed syntax (actual, partial):
```sql
CREATE [TEMPORARY] TABLE [IF NOT EXISTS] name (
  <column_def> | <table_constraint> [, ...]
)
[table options...]
```
Column constraints (parsed):
- NOT NULL, NULL
- PRIMARY KEY, UNIQUE
- DEFAULT literal
- AUTO_INCREMENT
- REFERENCES ... (parsed but not emitted in executor format)

Table constraints (parsed):
- PRIMARY KEY, UNIQUE, FOREIGN KEY, CHECK
- FULLTEXT/SPATIAL/KEY/INDEX accepted but not emitted

Emission (actual):
- Emits `CREATE_TABLE` then a byte for IF NOT EXISTS, then `TABLE_REF`.
- Column defs emitted as `COLUMN_DEF` + column name, then type opcode and modifiers.
- Table constraints are emitted inside the column list with `PRIMARY_KEY`, `UNIQUE_CONSTRAINT`, and `TABLE_FK` opcodes.
Executor mismatch:
- Executor expects `CREATE_TABLE` immediately followed by `TABLE_REF` and column definitions using `COLUMN_REF` qualifiers and different constraint encoding.

### ALTER TABLE
Parsed syntax (actual):
```sql
ALTER TABLE name RENAME TO new_name
```
Emission (actual):
- Emits EXT_RENAME_OBJECT / EXT_MOVE_OBJECT for table rename or move.
Other ALTER TABLE actions are rejected.

### DROP and TRUNCATE
- DROP DATABASE/SCHEMA is implemented.
- DROP TABLE/INDEX/VIEW and TRUNCATE are not implemented and error at parse time.

## DML details

### SELECT
Parsed features:
- DISTINCT/DISTINCTROW/ALL
- FROM with JOINs (INNER/LEFT/RIGHT/CROSS/NATURAL/STRAIGHT_JOIN, ON/USING)
- WHERE, GROUP BY (WITH ROLLUP), HAVING
- ORDER BY, LIMIT

Emission (actual):
- Emits a DISTINCT flag byte after `SELECT`, then a select list with alias strings.
Executor mismatch:
- Executor expects the select list immediately after `SELECT` and uses a different alias encoding.

### INSERT
Parsed features:
- INSERT INTO <table> [(cols...)] VALUES (...)[, ...]
- INSERT INTO <table> [(cols...)] SELECT ...
- DEFAULT values in VALUES list
- ON DUPLICATE KEY UPDATE

Emission (actual):
- Emits `TABLE_REF`, a column list with `COLUMN_REF` qualifiers (schema/table/column), and a list of rows.
Executor mismatch:
- Executor expects a flat column list with single-name `COLUMN_REF` and a single values list; multi-row and SELECT sources are unsupported.
- ON DUPLICATE KEY UPDATE is encoded as `EXT_ON_CONFLICT_DO_UPDATE`, which executor does not handle.

### UPDATE
Parsed features:
- UPDATE <table> SET col=expr [, ...] [WHERE ...] [ORDER BY ...] [LIMIT ...]
- Multi-table update syntax is parsed as a table list

Emission mismatch:
- Executor expects one table and assignments with unqualified column names; alias/table lists and qualified COLUMN_REF payloads are not handled.

### DELETE
Parsed features:
- DELETE FROM <table> [alias] [WHERE ...] [ORDER BY ...] [LIMIT ...]

Emission mismatch:
- Executor expects a single table and no alias/ORDER/LIMIT payload.

### REPLACE
- Encoded as INSERT plus `EXT_ON_CONFLICT_DO_UPDATE`; executor does not implement this path.

## Bytecode payload layouts (parser emission)

All MySQL parser statements emit:
- VERSION, SBLR_VERSION
- <statement opcode + payload>
- END

### SELECT (MySQL parser)
Order:
1) SELECT
2) <distinct_flag byte> (1 = DISTINCT/DISTINCTROW, 0 = ALL)
3) SELECT list: BEGIN_LIST <count> (<expr> + <alias string>)* END_LIST
4) FROM clause:
   - if present: BEGIN_LIST <count> (TABLE_REF <schema/table>, <alias>) + JOIN_TYPE blocks + END_LIST
   - if absent: emitU32(0)
5) WHERE_CLAUSE + <expr>, else LITERAL_NULL
6) GROUP_BY + BEGIN_LIST <expr>* END_LIST, else emitU32(0)
7) HAVING + <expr>, else LITERAL_NULL
8) ORDER_BY + BEGIN_LIST (SORT_KEY + <expr> + SORT_ASC/DESC [+ NULLS_*])* END_LIST, else emitU32(0)
9) LIMIT + <expr>, else LITERAL_NULL

### INSERT
Order:
1) INSERT
2) TABLE_REF <schema/table>
3) column list: BEGIN_LIST <count> (COLUMN_REF <schema> <table> <column>)* END_LIST
4) VALUES: BEGIN_LIST <row_count> [BEGIN_LIST <val_count> <expr>* END_LIST]* END_LIST
   - or SELECT: nested SELECT bytecode
5) ON DUPLICATE KEY UPDATE: EXT_ON_CONFLICT_DO_UPDATE + assignment list (optional)

### UPDATE
Order:
1) UPDATE
2) BEGIN_LIST <table_count> (TABLE_REF <schema/table> + <alias>)* END_LIST
3) BEGIN_LIST <assignment_count>
   - ASSIGNMENT + COLUMN_REF <schema> <table> <column> + <expr>
4) END_LIST
5) WHERE_CLAUSE + <expr>, else LITERAL_NULL
6) ORDER_BY / LIMIT payloads (optional)

### DELETE
Order:
1) DELETE
2) TABLE_REF <schema/table>
3) <alias string> (optional)
4) WHERE_CLAUSE + <expr>, else LITERAL_NULL
5) ORDER_BY / LIMIT payloads (optional)

### REPLACE
Order:
1) INSERT
2) EXT_ON_CONFLICT_DO_UPDATE
3) TABLE_REF <schema/table>
4) column list + VALUES (same layout as INSERT)

### CREATE TABLE
Order:
1) CREATE_TABLE
2) <if_not_exists byte>
3) TABLE_REF <schema/table>
4) BEGIN_LIST <count> of column defs and table constraints (interleaved)
   - Column: COLUMN_DEF <col_name> <type opcode> [length]
     + NOT_NULL (optional)
     + PRIMARY_KEY/UNIQUE_CONSTRAINT (optional)
     + IDENTITY_COLUMN (optional)
     + DEFAULT_VALUE + LITERAL_NULL or LITERAL_STRING (optional)
   - Table constraint: PRIMARY_KEY/UNIQUE_CONSTRAINT/TABLE_FK/CHECK_CONSTRAINT (using BEGIN_LIST and COLUMN_REF)
5) END_LIST
6) Table options are parsed but not emitted

### Admin/session opcodes
Order:
- SET TRANSACTION: SET_TRANSACTION + <flags int16> + <conflict_action byte> + optional fields (isolation, access mode, autocommit)
- SET AUTOCOMMIT: EXT_SET_AUTOCOMMIT + <mode byte> + <conflict_action byte> [+ <error_code int32> if ERROR]
- SET variable: EXT_SET_VARIABLE + <variable name string> + <expr>
- SHOW TABLES: EXT_SHOW_TABLES + <db string> + <like pattern string>
- SHOW DATABASES: EXT_SHOW_DATABASES + <like pattern string>
- SHOW COLUMNS: EXT_SHOW_COLUMNS + <table string> + <like pattern string>
- SHOW INDEXES: EXT_SHOW_INDEXES + <table string>
- SHOW CREATE TABLE: EXT_SHOW_CREATE_TABLE + <table string>
- SHOW CREATE DATABASE: EXT_SHOW_DATABASE + <db string>
- DESCRIBE: EXT_DESCRIBE_TABLE + <table string> + <column pattern string>
- USE: EXT_SET_VARIABLE + "search_path" + LITERAL_STRING + <schema path string>
- LOCK/UNLOCK TABLES: LITERAL_NULL (no-op)

## Executor decode payload layouts (EXT_* session/admin opcodes)

These payloads are read by the executor (from `Executor::execute*`).

- EXT_SET_AUTOCOMMIT: <mode byte> + <conflict_action byte> + [<conflict_error_code int32> if action=ERROR]
- EXT_SET_VARIABLE (search_path only): <variable name string> then one of:
  - marker byte 0/1 (0 = DEFAULT/RESET, 1 = expression follows)
  - BEGIN_LIST + <int32 count> + <string entries> + END_LIST
  - LITERAL_NULL
  - expression bytecode (string result)
- EXT_SHOW_TABLES: <db string> + <like pattern string>
- EXT_SHOW_DATABASES: <like pattern string>
- EXT_SHOW_COLUMNS: <table string> + <like pattern string>
- EXT_SHOW_INDEXES: <table string>
- EXT_SHOW_CREATE_TABLE: <table string>
- EXT_SHOW_DATABASE: <db string>
- EXT_DESCRIBE_TABLE: <table string> + <column pattern string>

## Session and admin statements

- SET TRANSACTION emits `SET_TRANSACTION` with isolation/access flags (handled by executor).
- SET AUTOCOMMIT emits `EXT_SET_AUTOCOMMIT`.
- Generic SET emits `EXT_SET_VARIABLE`, but the executor only accepts `search_path` (other variables error).
- SHOW TABLES/DATABASES/COLUMNS/INDEXES/CREATE TABLE emit `EXT_SHOW_*` opcodes handled by executor.
- DESCRIBE emits `EXT_DESCRIBE_TABLE` and is handled.
- USE changes the default schema and emits `EXT_SET_VARIABLE` for `search_path`.
- LOCK/UNLOCK TABLES emit `LITERAL_NULL` no-ops.

## Expressions and functions (parser-level)

Expressions:
- Logical AND/OR/NOT
- Comparisons, BETWEEN, IN, LIKE
- Arithmetic and concatenation
- CASE, CAST, EXISTS, subqueries

Function support (special-cased):
- Aggregates: COUNT, SUM, AVG, MIN, MAX
- String: LENGTH, UPPER, LOWER, SUBSTRING, TRIM
- Math: ABS, ROUND, FLOOR, CEIL, SQRT, POWER
- Date: NOW/CURRENT_TIMESTAMP, CURDATE/CURRENT_DATE
- COALESCE, NULLIF

Unrecognized functions are emitted as generic function calls.

## Data types (parser-level)

Parsed MySQL types are mapped to SBLR opcodes, including:
- Integers: TINYINT, SMALLINT, MEDIUMINT, INT/INTEGER, BIGINT
- Numeric: DECIMAL/NUMERIC, FLOAT, DOUBLE
- String: CHAR, VARCHAR, BINARY, VARBINARY, TEXT variants
- Binary: BLOB variants
- Temporal: DATE, TIME, DATETIME, TIMESTAMP, YEAR
- Boolean: BOOL/BOOLEAN
- Other: ENUM, SET, JSON, GEOMETRY/POINT/LINESTRING/POLYGON
