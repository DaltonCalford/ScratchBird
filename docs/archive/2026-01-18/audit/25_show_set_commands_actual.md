# SHOW and SET Commands (Actual Implementation)

Purpose: Code-verified analysis of SHOW/SET commands, comparing the spec-defined surface with the V2 parser -> bytecode -> executor pipeline and client-side ISQL handling.

Status: static code review snapshot; no runtime execution performed.

## If you only remember 5 things
- The server implements a mix of MySQL-style SHOW (TABLES/DATABASES/COLUMNS/INDEXES) and Firebird-style SHOW (TABLE/INDEX/TRIGGER/...) but omits several spec-listed SHOW targets (PROCESSLIST/VARIABLES/STATUS/WARNINGS/ERRORS).
- V2 emits EXT_SHOW_VARIABLE/EXT_SHOW_ALL/EXT_SHOW_TRANSACTION_LEVEL but the executor has no handlers, so SHOW <var>/SHOW ALL/SHOW TRANSACTION ISOLATION LEVEL fail.
- The executor implements schema-navigation SHOW opcodes (SCHEMA PATH/TREE/SEARCH PATH/LOCATION/RESOLVED/OBJECTS) that the V2 parser never emits.
- SET TRANSACTION/AUTOCOMMIT/SQL DIALECT/NAMES/LOCAL_TIMEOUT work; SET TIME ZONE, SET ROLE, and SET SESSION AUTHORIZATION are parsed but fail due to missing or incorrect bytecode encoding.
- Generic SET only supports SEARCH_PATH; list values and SESSION/LOCAL scope are dropped in semantic and bytecode stages.

## Scope and sources
Specs:
- `ScratchBird/docs/specifications/00_GRAMMAR_BNF.md`
- `ScratchBird/docs/specifications/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `ScratchBird/docs/specifications/07_TRANSACTION_AND_SESSION_CONTROL.md`
- `ScratchBird/docs/specifications/ScratchBird Master Grammar Specification v2.0.md`
- `ScratchBird/docs/specifications/SCHEMA_PATH_RESOLUTION.md`
- `ScratchBird/docs/specifications/01_SQL_DIALECT_OVERVIEW.md`

Code:
- `ScratchBird/include/scratchbird/parser/ast_v2.h`
- `ScratchBird/src/parser/parser_v2.cpp`
- `ScratchBird/src/sblr/semantic_analyzer_v2.cpp`
- `ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- `ScratchBird/include/scratchbird/sblr/opcodes.h`
- `ScratchBird/src/sblr/executor.cpp`
- `ScratchBird/src/cli/sb_isql.cpp`
- `ScratchBird/src/cli/sb_fb_isql.cpp`

## Spec-defined command surface (should exist)

### SHOW (spec)
From the BNF system commands:
- SHOW DATABASES [LIKE pattern]
- SHOW SCHEMAS [LIKE pattern]
- SHOW TABLES [FROM schema_name] [LIKE pattern]
- SHOW COLUMNS FROM table_name
- SHOW INDEX FROM table_name
- SHOW CREATE TABLE table_name
- SHOW CREATE DATABASE database_name
- SHOW PROCESSLIST
- SHOW VARIABLES [LIKE pattern]
- SHOW STATUS [LIKE pattern]
- SHOW WARNINGS
- SHOW ERRORS

From session control:
- SHOW SEARCH_PATH
- SHOW TIME ZONE
- SHOW ALL (all session parameters)

From schema path resolution:
- SHOW SCHEMA name uses path resolution rules.

### SET (spec)
From the BNF SET operations:
- SET [SESSION|LOCAL] <configuration_parameter> TO|= value
- SET [SESSION|LOCAL] TIME ZONE <timezone_value>
- SET [SESSION|LOCAL] ROLE <role_name>
- SET [SESSION|LOCAL] SESSION AUTHORIZATION <user_name>
- SET SCHEMA <schema_name>
- SET SEARCH_PATH TO <schema_list>
- SET CONSTRAINTS {ALL|list} {DEFERRED|IMMEDIATE}
- MySQL user variables: SET @var = / := expr, SELECT expr INTO @var

From Master Grammar v2.0:
- SET CURRENT SCHEMA {UP|DEFAULT|<schema_path>}
- SET SEARCH PATH <path_list>
- SET TRANSACTION <options>
- SET ROLE <identifier>
- SET NAMES <charset>

### Notable spec overlaps or contradictions
- BNF omits SHOW SEARCH_PATH/TIME ZONE/ALL, but session-control spec requires them.
- BNF says SET SCHEMA without TO/=; V2 parser only accepts SET <var> TO/= value, so SET SCHEMA <name> is not accepted.

## Actual server-side implementation (V2 pipeline)

Legend: Y = implemented, P = partial or broken, N = not implemented, U = implemented but unreachable.

### SET command coverage (server)
| Command | Parser | Bytecode | Executor | Notes |
| --- | --- | --- | --- | --- |
| SET TRANSACTION ... | Y | Y | Y | Isolation/access/read-committed variants, wait/no-wait, lock timeout, reserving, autocommit, ON CONFLICT supported |
| SET AUTOCOMMIT [ON|OFF|1|0] [ON CONFLICT ...] | Y | Y | Y | Conflict action applies to current transaction |
| SET SQL DIALECT n | Y | Y | Y | Connection context only |
| SET NAMES <charset> | Y | Y | Y | Connection charset only |
| SET LOCAL_TIMEOUT n | Y | Y | Y | Statement timeout seconds |
| SET ROLE {NONE|DEFAULT|expr} | Y | P | P | Parsed value is ignored; generator emits wrong payload, executor expects flags + role |
| SET SESSION AUTHORIZATION {DEFAULT|expr} | Y | P | P | Same encoding mismatch as SET ROLE |
| SET TIME ZONE {LOCAL|DEFAULT|expr} | Y | N | N | SetType exists but no bytecode encoding |
| SET <variable> TO/= <expr> | Y | P | P | Only SEARCH_PATH accepted at runtime; other names error |
| SET SEARCH_PATH TO a[,b] | Y | P | P | List values dropped in semantic/bytecode; executor supports list if encoded |
| SET SCHEMA <schema_name> | P | N | N | Only works with non-spec syntax `SET SCHEMA TO/=` and still unsupported by executor |
| SET CONSTRAINTS ... | N | N | U | Executor implements EXT_SET_CONSTRAINTS but parser/generator never emit |
| SET SESSION CHARACTERISTICS AS TRANSACTION ... | N | N | N | Spec-only |
| SET @var = / := | N | N | N | Spec-only (MySQL user variables) |

Notes:
- SESSION/LOCAL scope is parsed but not encoded into bytecode; no behavior change.
- SET PARSER VERSION is explicitly rejected.

### SHOW command coverage (server)
| Command | Parser | Bytecode | Executor | Notes |
| --- | --- | --- | --- | --- |
| SHOW TABLES [FROM schema] [LIKE] | Y | Y | Y | FROM is treated as schema; LIKE filters via SQL LIKE |
| SHOW DATABASES [LIKE] | Y | Y | Y | Lists schemas |
| SHOW COLUMNS FROM table [LIKE] | Y | Y | Y | MySQL-style column output |
| SHOW INDEXES FROM table | Y | Y | P | Parser allows no FROM, but executor requires a table |
| SHOW CREATE TABLE table | Y | Y | Y | Emits simplified CREATE TABLE |
| SHOW TABLE table | Y | Y | Y | Firebird-style detailed output |
| SHOW INDEX index | Y | Y | Y | Placeholder output; no catalog lookup |
| SHOW TRIGGER [name] | Y | Y | P | Executor requires name; empty errors |
| SHOW VIEW [name] | Y | Y | P | Executor requires name; empty errors |
| SHOW PROCEDURE [name] | Y | Y | P | Executor requires name; empty errors |
| SHOW FUNCTION [name] | Y | Y | P | Executor requires name; empty errors |
| SHOW DOMAIN [name] | Y | Y | P | Executor requires name; empty errors |
| SHOW GENERATOR/SEQUENCE [name] | Y | Y | P | Executor requires name; empty errors |
| SHOW SCHEMA [name] | Y | Y | Y | Empty lists all schemas |
| SHOW ROLE [name] | Y | Y | P | Executor requires name; empty errors |
| SHOW GRANTS [FOR name] | Y | Y | Y | Empty lists all visible grants |
| SHOW CHECKS [table] | Y | Y | P | Executor expects table name |
| SHOW COLLATIONS [LIKE] | Y | Y | Y | LIKE filter supported |
| SHOW COMMENTS [object] | Y | Y | Y | Empty lists all comments |
| SHOW DEPENDENCIES [object] | Y | Y | Y | Empty lists all dependencies |
| SHOW PACKAGE name | Y | Y | Y | Name required |
| SHOW SQL DIALECT | Y | Y | Y | Connection dialect |
| SHOW VERSION | Y | Y | Y | Server version string |
| SHOW DATABASE | Y | Y | Y | Current database info |
| SHOW SYSTEM | Y | Y | Y | System info tables |
| SHOW ALL | Y | Y | N | EXT_SHOW_ALL has no executor handler |
| SHOW <variable> | Y | Y | N | EXT_SHOW_VARIABLE has no executor handler |
| SHOW TRANSACTION ISOLATION LEVEL | Y | Y | N | EXT_SHOW_TRANSACTION_LEVEL has no executor handler |
| SHOW SCHEMA PATH/TREE/SEARCH PATH/LOCATION/RESOLVED/OBJECTS | N | N | U | Executor implements; parser never emits |
| SHOW CREATE DATABASE | N | N | N | Spec-only |
| SHOW PROCESSLIST / VARIABLES / STATUS / WARNINGS / ERRORS | N | N | N | Spec-only |

Notes:
- SHOW PARSER VERSION is explicitly rejected.
- Executor also has DESCRIBE support (`executeDescribeTable`) but V2 parser does not emit it.

## Output formats (executor)
- SHOW TABLES/DATABASES/COLUMNS/INDEXES/CREATE TABLE use MySQL-style columns.
- SHOW TABLE/INDEX/TRIGGER/VIEW/PROCEDURE/FUNCTION/DOMAIN/GENERATOR/ROLE/SCHEMA return Firebird-style property/value rows.
- Schema navigation SHOW commands (PATH/TREE/SEARCH PATH/LOCATION/RESOLVED/OBJECTS) return dedicated columns but are unreachable from V2 today.

## Client-side ISQL commands (not SQL engine)
`sb_isql.cpp` (Firebird-style CLI) handles these without sending them to the server:
- SET BAIL, TERM, COUNT, HEADING, ECHO, LIST, NULL, WIDTH, STATS, PLAN, PLANONLY, EXPLAIN, NAMES, WARNINGS, AUTODDL, MAXROWS, LOCAL_TIMEOUT, TIME, PROMPT, DEFINE/UNDEFINE.
- SHOW SQL DIALECT (client-only).
- INPUT/OUTPUT/ERROR file redirection (via SET INPUT/OUTPUT/ERROR).

`sb_fb_isql.cpp` implements a smaller subset:
- SET TERM, SQL DIALECT, COUNT, HEADING, STATS.

Notes:
- The CLI forwards SQL SHOW statements (for example, SHOW TABLES/INDEXES/DATABASES) to the server, but most SET commands are client-only and never reach the SQL engine.
