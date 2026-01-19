# Firebird SQL Parser Statement Reference (Actual Implementation)

Purpose: Code-verified description of the Firebird parser coverage and how it flows through the V2 pipeline (semantic analysis, bytecode generation, executor).

Status: static code review snapshot; no runtime execution performed.

## If you only remember 5 things
- Firebird parser emits AST v2 nodes, so all V2 pipeline limitations apply.
- PSQL blocks and EXECUTE BLOCK are parsed but not supported by SemanticAnalyzerV2 or BytecodeGeneratorV2.
- Many Firebird DDL statements are stubbed (CREATE PROCEDURE/FUNCTION/TRIGGER/PACKAGE/ROLE/EXCEPTION, ALTER INDEX, DROP SEQUENCE) and error at parse time.
- Firebird-specific syntax is accepted (FIRST/SKIP, UPDATE OR INSERT, RETURNING, RECREATE), but some features are not implemented after parsing (UPDATE OR INSERT behaves like INSERT).
- Schema-qualified names are rejected; dotted identifiers emit errors and are flattened into a single path.

## Scope and sources (code)
- Firebird parser: `ScratchBird/src/parser/firebird/firebird_parser.cpp`
- Firebird lexer: `ScratchBird/src/parser/firebird/firebird_lexer.cpp`
- AST v2: `ScratchBird/include/scratchbird/parser/ast_v2.h`
- V2 pipeline: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `ScratchBird/src/sblr/bytecode_generator_v2.cpp`, `ScratchBird/src/sblr/executor.cpp`

## Statement coverage map
Legend: Y = implemented, P = partial/limited, N = not implemented, X = incompatible with executor format.

| Category | Statement | Parser | Semantic | Bytecode | Executor | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| DDL | CREATE DATABASE | Y | Y | Y | Y | Path is emulated from db filename |
| DDL | CREATE TABLE | Y | Y | P | P | V2 format limits (constraints/options ignored) |
| DDL | CREATE INDEX | Y | Y | Y | X | V2 bytecode vs executor mismatch |
| DDL | CREATE VIEW | Y | Y | Y | X | V2 bytecode vs executor mismatch |
| DDL | CREATE SEQUENCE/GENERATOR | Y | N | N | N | Semantic analyzer rejects |
| DDL | CREATE DOMAIN | Y | Y | Y | Y | BASIC domains only |
| DDL | CREATE PROCEDURE/FUNCTION/TRIGGER/PACKAGE/ROLE/EXCEPTION | N | N | N | N | Parser emits errors |
| DDL | ALTER TABLE | P | P | P | P | Only add/drop/alter column parsed |
| DDL | ALTER DOMAIN | Y | Y | Y | Y | Limited actions |
| DDL | ALTER DATABASE | N | N | N | N | Parser errors "not supported" |
| DDL | DROP DATABASE | Y | Y | Y | Y | Emulated path |
| DDL | DROP TABLE/INDEX/VIEW | Y | Y | Y | X | V2 bytecode vs executor mismatch |
| DDL | DROP DOMAIN | Y | Y | Y | Y | Works |
| DDL | DROP SEQUENCE/GENERATOR | N | N | N | N | Not implemented |
| DDL | RECREATE TABLE/VIEW/INDEX | P | P | P | P | Parsed; same limits as CREATE |
| DML | SELECT | Y | Y | P | P | Joins/multi-table formats limited |
| DML | INSERT | Y | Y | P | P | Multi-row/INSERT SELECT dropped |
| DML | UPDATE | Y | Y | P | P | Limited |
| DML | DELETE | Y | Y | P | P | Limited |
| DML | UPDATE OR INSERT | Y | P | P | P | Compiled as INSERT only |
| DML | MERGE | N | N | N | N | Not implemented |
| TCL | SET TRANSACTION | Y | Y | Y | Y | Firebird-specific options parsed |
| TCL | COMMIT/ROLLBACK | Y | Y | Y | Y | RETAINING mapped to AND CHAIN |
| TCL | SAVEPOINT/RELEASE | Y | Y | Y | Y | |
| Session | SET/SHOW | N | N | N | N | Not implemented |
| DCL | GRANT/REVOKE | N | N | N | N | Not implemented |
| Meta | COMMENT | N | N | N | N | Not implemented |
| PSQL | BEGIN...END, IF/WHILE/FOR, etc. | Y | N | N | N | AST nodes unsupported by V2 pipeline |

## DDL details and lifecycles

### CREATE DATABASE
Parsed syntax (actual):
```sql
CREATE DATABASE <string|identifier>
  [USER <id|string>] [PASSWORD <id|string>]
  [PAGE [SIZE] <int>]
  [DEFAULT CHARACTER SET <id|string>]
  [DEFAULT COLLATION <id|string>]
```
Lifecycle (actual):
- Parse: converts database path to emulated schema path `remote.emulated.firebird.localhost.<db>`.
- Semantic/bytecode/executor: handled by V2 pipeline (same as native CREATE DATABASE).

### CREATE TABLE
Parsed syntax (actual):
```sql
CREATE [OR REPLACE|OR ALTER] [GLOBAL] [TEMPORARY] TABLE <name> (
  <column_def> | <table_constraint> [, ...]
)
[ON COMMIT DELETE ROWS | ON COMMIT PRESERVE ROWS]
```
Column constraints (parsed):
- [CONSTRAINT name] NOT NULL | NULL
- PRIMARY KEY | UNIQUE
- REFERENCES <table> [(cols...)] [ON DELETE/UPDATE ...]
- CHECK (expr)
- DEFAULT expr
- GENERATED [ALWAYS|BY DEFAULT] AS IDENTITY
- COMPUTED BY (expr)

Table constraints (parsed):
- [CONSTRAINT name] PRIMARY KEY (cols...)
- [CONSTRAINT name] UNIQUE (cols...)
- [CONSTRAINT name] FOREIGN KEY (cols...) REFERENCES <table> [(cols...)]
- [CONSTRAINT name] CHECK (expr)

Lifecycle (actual):
- Parse: builds `CreateTableStmt` with columns, constraints, temp/or_replace flags.
- Semantic/bytecode/executor: same limitations as V2 CREATE TABLE (options/constraints largely ignored).

### CREATE INDEX
Parsed syntax (actual):
```sql
CREATE [UNIQUE] [ASCENDING|DESCENDING] INDEX [name] ON <table> (
  <column> | <expr> [ASC|DESC] [, ...]
)
[WHERE <expr>]
```
Lifecycle (actual):
- Parse: supports expression indexes and partial indexes.
- Bytecode/executor: V2 CREATE INDEX format is incompatible with executor.

### CREATE VIEW
Parsed syntax (actual):
```sql
CREATE [OR REPLACE] VIEW <name> [(col, ...)] AS <select>
[WITH CHECK OPTION]
```
Lifecycle (actual):
- Parse: builds view AST with query and optional column list.
- Bytecode/executor: V2 CREATE VIEW format is incompatible with executor.

### CREATE SEQUENCE / GENERATOR
Parsed syntax (actual):
```sql
CREATE SEQUENCE <name> [START [WITH] n] [INCREMENT [BY] n]
```
Lifecycle (actual):
- Parse: captures start/increment.
- Semantic: not implemented (statement rejected).

### CREATE DOMAIN
Parsed syntax (actual):
```sql
CREATE DOMAIN <name> [AS] <type>
  [COLLATE <name>]
  [CONSTRAINT name] NOT NULL | NULL | DEFAULT <expr> | CHECK (<expr>)
```
Lifecycle (actual):
- Parse: builds `CreateDomainStmt` with base type and constraints.
- Semantic/bytecode/executor: handled by domain pipeline.

### ALTER TABLE
Parsed syntax (actual):
```sql
ALTER TABLE <name>
  ADD [COLUMN] <column_def>
  | ADD CONSTRAINT <constraint>   (stub)
  | DROP COLUMN <name>
  | DROP CONSTRAINT <name>
  | ALTER [COLUMN] <name> TO <new>   (rename)
  | ALTER [COLUMN] <name>            (type/constraints not parsed)
```
Notes:
- RENAME and SET variants error as unsupported.

### DROP TABLE / DROP INDEX / DROP VIEW / DROP DOMAIN / DROP DATABASE
Parsed with optional IF EXISTS and CASCADE/RESTRICT where shown.
DROP SEQUENCE/GENERATOR is not implemented.

### RECREATE TABLE/VIEW/INDEX
Parsed as CREATE with `or_replace` semantics; same V2 pipeline limitations apply.

## DML details

### SELECT
Parsed features:
- FIRST/SKIP at start; ROWS clause at end
- DISTINCT/ALL
- FROM with subqueries and JOINs (INNER/LEFT/RIGHT/FULL/CROSS/NATURAL, ON/USING)
- WHERE, GROUP BY, HAVING, UNION
- ORDER BY with ASC/DESC and NULLS FIRST/LAST
- FOR UPDATE [WITH LOCK]
Notes:
- Schema-qualified names are rejected in identifiers and table paths.

### INSERT
Parsed features:
- INSERT INTO <table> [(cols...)] VALUES (...)[, ...]
- INSERT INTO <table> [(cols...)] SELECT ...
- INSERT INTO <table> DEFAULT VALUES
- RETURNING clause
Notes:
- V2 pipeline only emits first VALUES row; INSERT SELECT is not compiled.

### UPDATE
Parsed features:
- UPDATE <table> [alias] SET col = expr [, ...]
- WHERE clause
- RETURNING clause

### DELETE
Parsed features:
- DELETE FROM <table> [alias]
- WHERE clause
- RETURNING clause

### UPDATE OR INSERT
Parsed as Firebird-specific UPSERT syntax, but compiled as a normal INSERT; UPDATE semantics are not implemented.

### MERGE
Not implemented.

## Bytecode payload layouts (V2 generator)

All Firebird statements compiled through the V2 pipeline emit:
- VERSION, SBLR_VERSION
- <statement opcode + payload>
- END

### SELECT (V2)
Order:
1) SELECT
2) BEGIN_LIST, <int32 select_count>
3) Select items:
   - SELECT_STAR
   - EXT_SELECT_TABLE_STAR + <UUID>
   - <expression bytecode> + optional COLUMN_REF "" <alias>
4) END_LIST
5) FROM:
   - TABLE_REF "" (no FROM), or
   - TABLE_REF <table name or alias> (single table), or
   - full FROM list (BEGIN_LIST ... JOIN_TYPE/TABLE_REF/JOIN_CONDITION) for multi-table/joins
6) WHERE_CLAUSE + <expr> (if present)
7) GROUP_BY + BEGIN_LIST + <expr>* + END_LIST (if present)
8) HAVING + <expr> (if present)
9) ORDER_BY + BEGIN_LIST + (SORT_KEY + <expr> + SORT_ASC/DESC [+ NULLS_FIRST/LAST])* + END_LIST (if present)
10) LIMIT + <expr> (if present)
11) OFFSET + <expr> (if present)
12) Set operations: EXT_UNION/EXT_UNION_ALL/EXT_INTERSECT/EXT_EXCEPT + nested SELECT (if present)

### INSERT (V2)
Order:
1) INSERT
2) TABLE_REF <table name>
3) BEGIN_LIST <col_count> [COLUMN_REF <col_name>]* END_LIST
4) BEGIN_LIST <value_count> [<expr>]* END_LIST (first VALUES row only)
5) EXT_RETURNING + <int32 count> + <column names> (optional)

### UPDATE (V2)
Order:
1) UPDATE
2) TABLE_REF <table name>
3) BEGIN_LIST <assignment_count>
   - ASSIGNMENT, COLUMN_REF <col_name>, <expr>
4) END_LIST
5) WHERE_CLAUSE + <expr> (optional)
6) EXT_RETURNING + SELECT_LIST (optional; full v2 list format)

### DELETE (V2)
Order:
1) DELETE
2) TABLE_REF <table name>
3) WHERE_CLAUSE + <expr> (optional)
4) EXT_RETURNING + SELECT_LIST (optional; full v2 list format)

### CREATE TABLE (V2)
Order:
1) CREATE_TABLE
2) TABLE_REF <table name>
3) BEGIN_LIST <col_count>
   - COLUMN_DEF
   - COLUMN_REF "" <col_name>
   - <type opcode> [length/precision/scale]
   - NOT_NULL (optional)
   - DEFAULT_VALUE, <int32 bytecode_len>, <expr bytecode> (optional)
   - CHECK_CONSTRAINT, <int32 bytecode_len>, <expr bytecode>, <constraint_name> (optional)
4) END_LIST
5) <tablespace_name string> (currently empty)
6) TABLE_FK entries (for each FK table constraint):
   - TABLE_FK
   - <byte child_col_count> + child column names
   - <parent table name string>
   - <byte parent_col_count> + parent column index ints
   - <on_delete string> + <on_update string>
   - <constraint_name string>
   - <deferrable_flags byte>

### CREATE INDEX (V2)
Order:
1) CREATE_INDEX
2) <flags byte> (unique/if_not_exists/concurrent)
3) <index_name string>
4) <table UUID>
5) <index_method string>
6) <int32 column_count> + (column_index int32 + desc byte)*
7) <byte has_where> + <expr bytecode> (if present)
8) <int16 tablespace_id>

### CREATE VIEW (V2)
Order:
1) CREATE_VIEW or REFRESH_MATERIALIZED_VIEW
2) <flags byte> (or_replace/materialized/check_option/has_deps)
3) <schema UUID>
4) <view name>
5) <int32 column_name_count> + column names
6) <SELECT bytecode for view query>
7) [deps] <int32 dep_count> + (UUID + object_type byte)* (optional)

### DROP TABLE/INDEX/VIEW/SEQUENCE (V2)
Order:
1) DROP_* opcode
2) <flags byte> (if_exists/cascade)
3) <int32 object_count> + UUID list

### TRUNCATE TABLE (V2)
Order:
1) TRUNCATE_TABLE
2) <flags byte> (cascade/restart_identity/async)
3) <int32 table_count> + UUID list

## Additional payload layouts (session and SHOW opcodes)

These layouts are emitted by the V2 bytecode generator (not by the Firebird parser itself, which does not parse SET/SHOW).

### SET TRANSACTION (V2)
Order:
1) SET_TRANSACTION
2) <int16 flags>
3) <conflict_action byte>
4) [conflict_error_code int32] if flags include HAS_CONFLICT_ERROR_CODE
5) [autocommit_mode byte] if HAS_AUTOCOMMIT
6) [isolation byte] if HAS_ISOLATION
7) [read_committed_mode byte] if HAS_READ_COMMITTED_MODE
8) [access_mode byte] if HAS_ACCESS_MODE
9) [deferrable byte] if HAS_DEFERRABLE
10) [wait_mode byte] if HAS_WAIT_MODE
11) [lock_timeout int32] if HAS_LOCK_TIMEOUT
12) [BEGIN_LIST + reservations] if HAS_RESERVATIONS:
    - BEGIN_LIST <int32 count>
    - for each: TABLE_REF <table name> + <lock_mode byte> + <for_write byte>
    - END_LIST

### SET VARIABLE (V2 generic)
Order:
1) EXT_SET_VARIABLE
2) <variable name string>
3) <has_value byte> (0 = DEFAULT/RESET, 1 = value follows)
4) <expr bytecode> if has_value

### SET SQL DIALECT (V2)
Order:
1) EXT_SET_SQL_DIALECT
2) <dialect byte> (1..3)

### SET NAMES (V2)
Order:
1) EXT_SET_NAMES
2) <charset name string>

### SET LOCAL_TIMEOUT (V2)
Order:
1) EXT_SET_LOCAL_TIMEOUT
2) <timeout seconds int32>

### SHOW (V2)
Order:
- EXT_SHOW_VARIABLE + <variable name>
- EXT_SHOW_ALL
- EXT_SHOW_TRANSACTION_LEVEL
- EXT_SHOW_TABLES + <from database> + <like pattern>
- EXT_SHOW_DATABASES + <like pattern>
- EXT_SHOW_COLUMNS + <from table> + <like pattern>
- EXT_SHOW_INDEXES + <from table>
- EXT_SHOW_CREATE_TABLE + <table name>
- Firebird ISQL-style EXT_SHOW_* (executor decode):
  - EXT_SHOW_TABLE + <table name string> (required)
  - EXT_SHOW_INDEX + <index name string> (not validated; empty echoes name)
  - EXT_SHOW_TRIGGER + <trigger name string> (required)
  - EXT_SHOW_PROCEDURE + <procedure name string> (required)
  - EXT_SHOW_FUNCTION + <function name string> (required)
  - EXT_SHOW_VIEW + <view name string> (required)
  - EXT_SHOW_DOMAIN + <domain name string> (required)
  - EXT_SHOW_GENERATOR + <generator/sequence name string> (required)
  - EXT_SHOW_SCHEMA + <schema name string> (empty lists all schemas)
  - EXT_SHOW_ROLE + <role name string> (required)
  - EXT_SHOW_GRANTS + <object name string> (empty lists all grants)
  - EXT_SHOW_CHECKS + <table name string> (empty lists all checks)
  - EXT_SHOW_COLLATIONS + <like pattern string> (empty lists all)
  - EXT_SHOW_COMMENTS + <object name string> (empty lists all comments)
  - EXT_SHOW_DEPENDENCIES + <object name string> (empty lists all dependencies)
  - EXT_SHOW_PACKAGE + <package name string> (required)
  - EXT_SHOW_SYSTEM (no payload)
  - EXT_SHOW_SQL_DIALECT (no payload)
  - EXT_SHOW_VERSION (no payload)
  - EXT_SHOW_DATABASE (no payload)

## Transaction and session statements

- SET TRANSACTION supports READ ONLY/WRITE, isolation (READ COMMITTED/SNAPSHOT), WAIT/NO WAIT, LOCK TIMEOUT, RESERVING.
- COMMIT and ROLLBACK support RETAINING and ROLLBACK TO SAVEPOINT.
- SAVEPOINT and RELEASE SAVEPOINT supported.
- SET/SHOW/GRANT/REVOKE/COMMENT are not implemented in this parser.

## PSQL (procedural SQL)

Parsed PSQL statements include:
- BEGIN...END blocks with exception handlers (WHEN ...)
- DECLARE VARIABLE
- Assignment (var := expr)
- IF/ELSE, WHILE, FOR SELECT ... INTO ... DO
- LEAVE, CONTINUE, EXIT, SUSPEND, RETURN
- EXCEPTION, POST_EVENT
- OPEN/FETCH/CLOSE cursor (DECLARE CURSOR exists but is not wired into parse flow)
- EXECUTE BLOCK

Unsupported/unimplemented:
- EXECUTE PROCEDURE, EXECUTE STATEMENT, FOR EXECUTE STATEMENT, LOOP

All PSQL AST nodes are currently ignored by SemanticAnalyzerV2/BytecodeGeneratorV2, so they do not execute.

## Expressions and types (parser-level)

Expressions:
- Logical: AND/OR/NOT
- Comparisons: =, <>, <, <=, >, >= and Firebird-specific !<, !>, ~=, ^=
- BETWEEN, IN (list or subquery), LIKE / CONTAINING / STARTING [WITH] / SIMILAR TO
- Arithmetic: +, -, *, /, concatenation (||)
- CASE, CAST, EXISTS, subqueries
- Function calls (with DISTINCT/ALL and COUNT(*)), OVER clause with PARTITION/ORDER and ROWS/RANGE frames
- Context variables: CURRENT_DATE/TIME/TIMESTAMP, CURRENT_USER/ROLE/CONNECTION/TRANSACTION
- Arrays: [expr, ...]

Types parsed:
- INTEGER/INT, SMALLINT, BIGINT, INT128
- FLOAT, DOUBLE PRECISION, REAL, DECIMAL/NUMERIC, DECFLOAT
- CHAR/CHARACTER (including VARYING), VARCHAR, NCHAR, VARBINARY
- BOOLEAN, DATE, TIME, TIMESTAMP
- BLOB with SUB_TYPE n/TEXT/BINARY
- Optional length/precision/scale, array size, and WITH TIME ZONE
