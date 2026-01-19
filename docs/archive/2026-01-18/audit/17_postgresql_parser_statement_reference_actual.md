# PostgreSQL Parser Statement Reference (Actual Implementation)

Purpose: Code-verified description of the PostgreSQL parser (PostgreSQL 16 syntax) and the SBLR bytecode it emits, with executor compatibility notes.

Status: static code review snapshot; no runtime execution performed.

## If you only remember 5 things
- The PostgreSQL parser emits SBLR bytecode directly; there is no semantic analyzer stage.
- Most core DDL/DML bytecode formats do not match the executor (CREATE TABLE/INDEX/VIEW, SELECT/INSERT/UPDATE/DELETE, DROP/TRUNCATE), so many statements fail at execution time.
- Some extended statements map to executor formats (CREATE DATABASE/SCHEMA/DOMAIN, transaction control, ANALYZE, CREATE ROLE, CREATE FUNCTION/PROCEDURE/TRIGGER), but CREATE USER and several SET variants (ROLE/SESSION AUTH/CONSTRAINTS) emit mismatched payloads.
- WITH (CTE) bytecode layout does not match the executor; recursive and non-recursive CTEs are parsed but not executed.
- SHOW/GRANT/REVOKE and EXPLAIN use opcodes not handled by the executor and are effectively unsupported.

## Scope and sources (code)
- Parser core: `ScratchBird/src/parser/postgresql/pg_parser.cpp`
- DDL: `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp`
- DML: `ScratchBird/src/parser/postgresql/pg_parser_dml.cpp`
- Expressions: `ScratchBird/src/parser/postgresql/pg_parser_expr.cpp`
- Misc (SET/SHOW/TCL/GRANT/ANALYZE/EXPLAIN/COPY): `ScratchBird/src/parser/postgresql/pg_parser_misc.cpp`
- Executor: `ScratchBird/src/sblr/executor.cpp`

## Statement coverage map
Legend: Y = parser implements, P = partial, N = not implemented, X = executor format mismatch.

| Category | Statement | Parser | Executor compatibility | Notes |
| --- | --- | --- | --- | --- |
| DDL | CREATE TABLE | Y | X | Emits extra IF NOT EXISTS byte, column/constraint format mismatch |
| DDL | CREATE INDEX | Y | X | Layout differs from executor (name/table/flags ordering) |
| DDL | CREATE VIEW | Y | X | Emits select bytecode, executor expects SQL string and flags |
| DDL | CREATE MATERIALIZED VIEW | Y | X | Uses CREATE_VIEW opcode with materialized flag; format mismatch |
| DDL | CREATE SEQUENCE | Y | X | Executor expects different payload |
| DDL | CREATE DATABASE | Y | Y | EXT_CREATE_DATABASE, flags + path |
| DDL | CREATE SCHEMA | Y | Y | EXT_CREATE_SCHEMA |
| DDL | CREATE DOMAIN | Y | Y | EXT_CREATE_DOMAIN (BASIC) |
| DDL | CREATE TYPE (ENUM/composite) | Y | Y | Mapped to domain payloads |
| DDL | CREATE FUNCTION/PROCEDURE/TRIGGER | Y | Y | EXT_CREATE_* opcodes with stored body |
| DDL | CREATE ROLE | Y | Y | EXT_CREATE_ROLE (executor expects rolename string only) |
| DDL | CREATE USER | Y | X | Executor expects flags byte before optional password |
| DDL | ALTER TABLE | P | X | Emits legacy ALTER_TABLE format not used by executor |
| DDL | ALTER DOMAIN | Y | Y | EXT_ALTER_DOMAIN / EXT_MOVE_OBJECT |
| DDL | ALTER SCHEMA/SEQUENCE/DATABASE | Y | Y | EXT_ALTER_SCHEMA/EXT_ALTER_DATABASE/ALTER_SEQUENCE |
| DDL | DROP TABLE/INDEX/VIEW/SEQUENCE | Y | X | Executor expects name + flags; parser emits TABLE_REF lists and extra flags |
| DDL | DROP DOMAIN/SCHEMA/DATABASE | Y | Y | EXT_DROP_* opcodes |
| DDL | TRUNCATE TABLE | Y | X | Emits TABLE_REF list + flags not read by executor |
| DML | SELECT | Y | X | Emits DISTINCT flag and list layout not expected by executor |
| DML | INSERT | Y | X | Emits alias, multi-row lists, DEFAULT VALUES/SELECT not supported |
| DML | UPDATE | Y | X | Emits alias and FROM list; executor expects single table and assignments |
| DML | DELETE | Y | X | Emits alias and USING clause; executor expects single table |
| DML | MERGE | Y | X | Uses EXT_MERGE_* opcodes not executed by executor |
| DML | WITH (CTE) | Y | X | EXT_WITH_CLAUSE layout mismatched (executor expects count + flag) |
| TCL | BEGIN/SET TRANSACTION | Y | Y | START_TRANSACTION/SET_TRANSACTION payload matches executor |
| TCL | COMMIT/ROLLBACK/SAVEPOINT/RELEASE | Y | Y | Extended opcodes are handled |
| Session | SET ROLE | Y | X | Executor expects flags byte + optional role name |
| Session | SET SESSION AUTHORIZATION | Y | X | Executor expects flags byte + optional user name |
| Session | SET CONSTRAINTS | Y | X | Parser emits list + mode; executor expects flags + name_count |
| Session | SET search_path | Y | Y | EXT_SET_VARIABLE; executor supports search_path list/expr |
| Session | SET TIME ZONE / other variables | Y | X | Executor only supports search_path |
| Session | SHOW (ALL/VARIABLE/TRANSACTION LEVEL) | Y | X | EXT_SHOW_ALL/EXT_SHOW_VARIABLE not handled |
| DCL | GRANT/REVOKE | Y | X | EXT_GRANT/EXT_REVOKE not handled (executor expects EXT_GRANT_PRIVILEGE) |
| Utility | ANALYZE | Y | Y | EXT_ANALYZE |
| Utility | EXPLAIN | Y | X | EXPLAIN_PLAN opcode not handled |
| Utility | COPY | P | X | Emits no top-level opcode; executor has no COPY support |

## DDL details and lifecycles

### CREATE TABLE
Parsed syntax (actual, partial):
```sql
CREATE [OR REPLACE] [TEMP|TEMPORARY] [UNLOGGED] TABLE [IF NOT EXISTS] name (
  <column_def> | <table_constraint> [, ...]
)
[WITH (...)] [TABLESPACE name]
```
Emission (actual):
- Emits `CREATE_TABLE` then a byte for IF NOT EXISTS, then `TABLE_REF`, then a column/constraint list.
- Column definitions are emitted as `COLUMN_DEF` + column name (no `COLUMN_REF` qualifier), then type opcode and optional modifiers.
- Table constraints are emitted as `PRIMARY_KEY`/`UNIQUE_CONSTRAINT`/`TABLE_FK` opcodes inside the column list.
Executor mismatch:
- Executor expects `CREATE_TABLE` followed immediately by `TABLE_REF`, then each column as `COLUMN_DEF` + `COLUMN_REF` + type and modifier opcodes.
- Extra IF NOT EXISTS byte and the constraint opcodes do not match executor layout.

### CREATE INDEX
Parsed syntax (actual, partial):
```sql
CREATE [UNIQUE] INDEX [CONCURRENTLY] [IF NOT EXISTS] [name] ON table
  [USING method] (col_or_expr [ASC|DESC] [NULLS FIRST|LAST], ...)
  [INCLUDE (col, ...)] [WHERE expr]
```
Emission (actual):
- Emits `CREATE_INDEX`, two flags (concurrently, if_not_exists), index name, `TABLE_REF`, method, and a list with `COLUMN_REF` plus sort flags.
Executor mismatch:
- Executor expects: index name string, table name string, is_unique, column count, column names, tablespace name, index type, expression/predicate flags.

### CREATE VIEW / MATERIALIZED VIEW
Parsed syntax (actual, partial):
```sql
CREATE [OR REPLACE] VIEW [IF NOT EXISTS] name [(cols...)] AS SELECT ...
CREATE MATERIALIZED VIEW name AS SELECT ... [WITH [NO] DATA]
```
Emission (actual):
- Emits `CREATE_VIEW`, a flag byte, view name, optional column list, then emits SELECT bytecode.
Executor mismatch:
- Executor expects view name, flags, column list (byte count), and a SQL definition string, not SELECT bytecode.

### CREATE SEQUENCE
Parsed syntax (actual, partial):
```sql
CREATE SEQUENCE [IF NOT EXISTS] name [START WITH n] [INCREMENT BY n] ...
```
Emission (actual):
- Emits `CREATE_SEQUENCE` + flags + name, but sequence options are parsed and not emitted.
Executor mismatch:
- Executor expects a specific payload for sequence options and name ordering.

### CREATE DOMAIN / CREATE TYPE
- `CREATE DOMAIN` emits `EXT_CREATE_DOMAIN` with BASIC domain payload and checks.
- `CREATE TYPE ENUM` and composite types are mapped to domain payloads (ENUM/RECORD kinds).
Executor compatibility: expected to work, as executor uses domain manager for these payloads.

### CREATE FUNCTION / PROCEDURE / TRIGGER
- Emits `EXT_CREATE_FUNCTION_STMT`, `EXT_CREATE_PROCEDURE_STMT`, `EXT_CREATE_TRIGGER` with stored body and optional dependency list.
- Dependencies are extracted by compiling the body through QueryCompilerV2 when possible.
Executor compatibility: executor handles these extended opcodes.

### ALTER / DROP / TRUNCATE
- ALTER TABLE uses a legacy `ALTER_TABLE` emission with `TABLE_REF` and inline column data, which does not match executor format.
- DROP TABLE/INDEX/VIEW/SEQUENCE and TRUNCATE emit lists of `TABLE_REF` and extra flag bytes; executor expects a single name string and a flags byte.
- ALTER DOMAIN/SCHEMA/DATABASE and DROP DOMAIN/SCHEMA/DATABASE use extended opcodes and match executor formats.

## DML details

### SELECT
Parsed features:
- DISTINCT and DISTINCT ON
- FROM with JOINs (INNER/LEFT/RIGHT/FULL/CROSS/NATURAL, ON/USING)
- WHERE, GROUP BY (ROLLUP/CUBE/GROUPING SETS), HAVING
- WINDOW clause and OVER specifications
- ORDER BY, LIMIT/OFFSET/FETCH, FOR UPDATE/SHARE
- Subqueries and CTEs

Emission (actual):
- Emits a DISTINCT flag byte after `SELECT`, select list with alias strings, then table lists and join markers.
Executor mismatch:
- Executor expects select list immediately after `SELECT` (no DISTINCT flag) and uses a different alias encoding (alias markers as `COLUMN_REF` opcodes).

### INSERT
Parsed features:
- Column list, multi-row VALUES, DEFAULT VALUES, INSERT ... SELECT
- ON CONFLICT and RETURNING

Emission (actual):
- Emits alias string after `TABLE_REF`, then column list and a row list structure.
Executor mismatch:
- Executor expects `TABLE_REF` followed by column list and a single value list; aliases, multi-row values, DEFAULT VALUES, SELECT sources, and ON CONFLICT are not supported.

### UPDATE / DELETE
Parsed features:
- UPDATE SET with multi-column assignment, optional FROM, WHERE, RETURNING
- DELETE FROM with USING, WHERE, RETURNING

Emission (actual):
- Emits alias strings and optional FROM/USING table lists.
Executor mismatch:
- Executor expects a single table name and a list of assignments, without alias or FROM/USING data.

### MERGE
Parsed and emitted via EXT_MERGE_* opcodes, but executor does not implement MERGE execution.

### WITH (CTE)
- Parser emits `EXT_WITH_CLAUSE` then a recursive flag and a BEGIN_LIST of CTE definitions.
- Executor expects a 16-bit count followed by a recursive flag; the list layout does not match.

## Bytecode payload layouts (parser emission)

All PostgreSQL parser statements emit:
- VERSION, SBLR_VERSION
- <statement opcode + payload>
- END

### SELECT (PostgreSQL parser)
Order:
1) SELECT
2) DISTINCT ON list (optional): BEGIN_LIST <count> <expr>* END_LIST
3) <distinct_flag byte> (1 = DISTINCT, 0 = ALL)
4) SELECT list: BEGIN_LIST <count> (<item bytecode> + <alias string>)* END_LIST
5) FROM clause:
   - if present: BEGIN_LIST <count> entries (TABLE_REF <schema/table>, <alias>), then JOIN_TYPE + TABLE_REF + alias + JOIN_CONDITION (as encountered), END_LIST
   - if absent: emitU32(0)
6) WHERE_CLAUSE + <expr>, else LITERAL_NULL
7) GROUP_BY + BEGIN_LIST <expr>* END_LIST, else emitU32(0)
8) HAVING + <expr>, else LITERAL_NULL
9) WINDOW clause (optional)
10) ORDER_BY + BEGIN_LIST (SORT_KEY + <expr> + SORT_ASC/DESC [+ NULLS_*])* END_LIST, else emitU32(0)
11) LIMIT + <expr>, else LITERAL_NULL
12) OFFSET + <expr> (optional)
13) FETCH clause (optional)
14) FOR clause (optional)

### INSERT
Order:
1) INSERT
2) TABLE_REF <schema/table>
3) <alias string> (possibly empty)
4) column list:
   - BEGIN_LIST <count> COLUMN_REF <col_name>* END_LIST, or emitU32(0)
5) source discriminator byte:
   - 1 = DEFAULT VALUES
   - 0 = VALUES (BEGIN_LIST <row_count> [BEGIN_LIST <val_count> <expr>* END_LIST]* END_LIST)
   - 2 = SELECT (nested SELECT bytecode)
6) EXT_ON_CONFLICT payload (optional)
7) EXT_RETURNING payload (optional)

### UPDATE
Order:
1) UPDATE
2) TABLE_REF <schema/table>
3) <alias string> (possibly empty)
4) BEGIN_LIST <assignment_count>
   - ASSIGNMENT + <column_name string> + <expr>
5) END_LIST
6) FROM clause payload (optional)
7) WHERE_CLAUSE + <expr>, else LITERAL_NULL
8) EXT_RETURNING payload (optional)

### DELETE
Order:
1) DELETE
2) TABLE_REF <schema/table>
3) <alias string> (possibly empty)
4) USING clause payload (optional)
5) WHERE_CLAUSE + <expr>, else LITERAL_NULL
6) EXT_RETURNING payload (optional)

### CREATE TABLE
Order:
1) CREATE_TABLE
2) <if_not_exists byte>
3) TABLE_REF <schema/table>
4) BEGIN_LIST <count> of column defs and table constraints (interleaved)
   - Column: COLUMN_DEF <col_name> <type opcode> [length/precision/scale]
     + NOT_NULL (optional)
     + PRIMARY_KEY/UNIQUE_CONSTRAINT (optional)
     + DEFAULT_VALUE (literal or string)
     + IDENTITY_COLUMN <always_flag byte>
     + GENERATED_COLUMN <expr string> <stored_flag byte>
   - Table constraint: PRIMARY_KEY/UNIQUE_CONSTRAINT/TABLE_FK with BEGIN_LIST of COLUMN_REF
5) END_LIST
6) Table options and TABLESPACE are parsed but not emitted

### CREATE INDEX
Order:
1) CREATE_INDEX
2) <concurrently byte>
3) <if_not_exists byte>
4) <index_name string>
5) TABLE_REF <schema/table>
6) <method byte>
7) BEGIN_LIST <count> (COLUMN_REF <col_name> + SORT_ASC/DESC + NULLS_* )* END_LIST
8) INCLUDE list (optional): BEGIN_LIST <count> COLUMN_REF <col_name>* END_LIST
9) WHERE_CLAUSE + <expr> (optional)

### CREATE VIEW
Order:
1) CREATE_VIEW
2) <if_not_exists byte>
3) <schema/table string>
4) optional column list: BEGIN_LIST <count> <col_name string>* END_LIST
5) SELECT bytecode for view definition
6) WITH CHECK OPTION byte (optional; only emitted when present)

### DROP TABLE
Order:
1) DROP_TABLE
2) <if_exists byte>
3) TABLE_REF <schema/table> (repeated)
4) CASCADE/RESTRICT byte (optional)

### TRUNCATE TABLE
Order:
1) TRUNCATE_TABLE
2) TABLE_REF <schema/table> (repeated)
3) RESTART/CONTINUE IDENTITY byte (optional)
4) CASCADE/RESTRICT byte (optional)

## Additional payload layouts (GRANT/REVOKE, SET/SHOW, EXT_*)

### GRANT (PostgreSQL parser)
Order:
1) EXT_GRANT
2) BEGIN_LIST <priv_count> <priv strings>* END_LIST
3) <object_type byte> (1=table,2=sequence,3=function,4=procedure,5=database,6=schema,11=all tables,12=all sequences,13=all functions)
4) BEGIN_LIST <obj_count> <object name strings>* END_LIST
5) BEGIN_LIST <grantee_count> <grantee strings>* END_LIST
6) EXT_GRANT_OPTION (optional, emitted only if WITH GRANT OPTION)

### REVOKE (PostgreSQL parser)
Order:
1) EXT_REVOKE
2) <revoke_grant_option byte> (1 if GRANT OPTION FOR)
3) BEGIN_LIST <priv_count> <priv strings>* END_LIST
4) <object_type byte>
5) BEGIN_LIST <obj_count> <object name strings>* END_LIST
6) BEGIN_LIST <grantee_count> <grantee strings>* END_LIST
7) <cascade/restrict byte> (1=CASCADE,2=RESTRICT)

### SET (PostgreSQL parser)
- SET TRANSACTION:
  1) SET_TRANSACTION
  2) <flags int16> + <conflict_action byte> + optional fields (conflict code, autocommit, isolation, access_mode, deferrable)
- SET CONSTRAINTS:
  1) EXT_SET_CONSTRAINTS
  2) [BEGIN_LIST <count> <constraint name>* END_LIST] or byte 0 for ALL
  3) <mode byte> (1=DEFERRED,2=IMMEDIATE)
- SET ROLE:
  1) EXT_SET_ROLE
  2) <role name string> (empty for NONE)
- SET SESSION AUTHORIZATION:
  1) EXT_SET_SESSION_AUTH
  2) <user name string> (empty for DEFAULT)
- SET TIME ZONE / SET search_path / SET variable:
  1) EXT_SET_VARIABLE
  2) <variable name string>
  3) <value expr or LITERAL_NULL>

### SHOW (PostgreSQL parser)
- EXT_SHOW_ALL
- EXT_SHOW_TRANSACTION_LEVEL
- EXT_SHOW_SEARCH_PATH
- EXT_SHOW_VARIABLE + <variable name string>

### ANALYZE (PostgreSQL parser)
Order:
1) EXT_ANALYZE
2) <verbose byte>
3) [BEGIN_LIST <table_count> <table name> [BEGIN_LIST <col_count> <col name>* END_LIST]* END_LIST] (optional)

### EXPLAIN (PostgreSQL parser)
Order:
1) EXPLAIN_PLAN
2) <options byte>
3) <statement bytecode> (nested statement)

### COPY (PostgreSQL parser)
Order (emitted fragment):
- BEGIN_LIST <col_count> COLUMN_REF <col_name>* END_LIST (optional)
- TABLE_REF <table>
- <direction byte> (1=FROM,2=TO)
- <source string> (STDIN/STDOUT or filename)
- Option parsing emits no opcode payload (parsed only)

### CREATE ROLE/USER (PostgreSQL parser)
- EXT_CREATE_ROLE + <role name string>
- EXT_CREATE_USER + <user name string> [+ optional password string]
- Mismatch: executor expects a flags byte (has_password/is_superuser) before optional password.

### CREATE FUNCTION/PROCEDURE/TRIGGER (PostgreSQL parser)
- EXT_CREATE_FUNCTION_STMT:
  - <flags byte> + <function name>
  - <param_count byte> + (mode byte + name string + type byte + precision int32 + scale int32)*
  - <return type byte> + <return precision int32> + <return scale int32>
  - <body string>
  - [dependencies] <dep_count> + (UUID + object_type byte)*
- EXT_CREATE_PROCEDURE_STMT:
  - <flags byte> + <procedure name>
  - <param_count byte> + (mode byte + name string + type byte + precision int32 + scale int32)*
  - <body string>
  - [dependencies] <dep_count> + (UUID + object_type byte)*
- EXT_CREATE_TRIGGER:
  - <trigger name> + <timing byte> + <event bytes> + <table name>
  - <row/statement byte> + optional WHEN expr + <function name>

### Executor decode payload layouts (EXT_* security/session opcodes)

These payloads are read by the executor (from `Executor::execute*`); several do not match PostgreSQL parser emission.

#### EXT_CREATE_USER (executor)
Order:
1) <user name string>
2) <flags byte> (bit0 = has_password, bit1 = is_superuser)
3) <password string> if has_password

#### EXT_CREATE_ROLE (executor)
Order:
1) <role name string>

#### EXT_DROP_USER (executor)
Order:
1) <user name string>
2) <flags byte> (bit0 = IF EXISTS, bit1 = CASCADE)

#### EXT_DROP_ROLE (executor)
Order:
1) <role name string>
2) <flags byte> (bit0 = IF EXISTS, bit1 = CASCADE)

#### EXT_SET_ROLE (executor)
Order:
1) <flags byte> (bit0 = RESET)
2) <role name string> if not RESET

#### EXT_SET_SESSION_AUTH (executor)
Order:
1) <flags byte> (bit0 = RESET)
2) <user name string> if not RESET

#### EXT_SET_CONSTRAINTS (executor)
Order:
1) <flags byte> (bit0 = ALL, bit1 = DEFERRED)
2) if not ALL: <name_count byte> + <constraint name string>*

#### EXT_GRANT_PRIVILEGE (executor)
Order:
1) <privileges int32>
2) <object_type byte>
3) <object name string>
4) <grantee_type byte>
5) <grantee name string>
6) <flags byte> (bit0 = WITH GRANT OPTION, bit1 = HAS COLUMN LIST)
7) if HAS COLUMN LIST: <column_count int32> + <column name string>*

#### EXT_REVOKE_PRIVILEGE (executor)
Order:
1) <privileges int32>
2) <object_type byte>
3) <object name string>
4) <grantee_type byte>
5) <grantee name string>
6) <flags byte> (bit0 = CASCADE, bit1 = HAS COLUMN LIST)
7) if HAS COLUMN LIST: <column_count int32> + <column name string>*

#### EXT_GRANT_ROLE (executor)
Order:
1) <role name string>
2) <grantee_type byte>
3) <grantee name string>
4) <with_admin_option byte> (0/1)

#### EXT_REVOKE_ROLE (executor)
Order:
1) <role name string>
2) <grantee_type byte>
3) <grantee name string>
4) <flags byte> (bit0 = CASCADE)

#### EXT_SET_VARIABLE (executor; search_path only)
After <variable name string>, payload can be:
- marker byte 0/1 (0 = DEFAULT/RESET, 1 = expression follows)
- or BEGIN_LIST + <int32 count> + <string entries> + END_LIST
- or LITERAL_NULL
- or expression bytecode (string result)

## Session and utility statements

- SET TRANSACTION, BEGIN, COMMIT, ROLLBACK, SAVEPOINT, RELEASE emit payloads matching executor expectations.
- SET search_path is handled by executor; SET ROLE/SET SESSION AUTHORIZATION/SET CONSTRAINTS payloads do not match executor decode, and non-search_path variables (e.g., TIME ZONE) are rejected.
- SHOW ALL / SHOW variable / SHOW TRANSACTION LEVEL emit opcodes not implemented in executor.
- GRANT/REVOKE emit `EXT_GRANT`/`EXT_REVOKE`, while executor expects `EXT_GRANT_PRIVILEGE`/`EXT_REVOKE_PRIVILEGE`.
- ANALYZE emits `EXT_ANALYZE` and is executed.
- EXPLAIN emits `EXPLAIN_PLAN`, which is not handled by executor.
- COPY parses syntax and emits fragments but no top-level opcode; executor has no COPY support.

## Expressions and functions (parser-level)

Expressions:
- Boolean logic, comparisons, IS [NOT] NULL/TRUE/FALSE/UNKNOWN, IS [NOT] DISTINCT FROM
- IN (list or subquery), BETWEEN, LIKE/ILIKE/SIMILAR TO, arithmetic and concatenation
- CASE, CAST, EXISTS, array literals, subqueries, window functions

Function support (special-cased):
- Aggregates: COUNT, SUM, AVG, MIN, MAX
- String: upper, lower, length/char_length, substring, trim
- Date/time: now/current_timestamp, current_date
- Math: abs, sqrt, round
- COALESCE, NULLIF
- JSON: json_object/jsonb_build_object, json_array/jsonb_build_array, json_extract_path

Unrecognized functions are emitted as generic function calls.
