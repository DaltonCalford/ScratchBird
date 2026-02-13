# V3 Parser: Session and Utility Statements

Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: define parsing, SBLR emission, and executor semantics for session and
utility statements (SET/SHOW/RESET/EXPLAIN/ANALYZE/CONNECT/DISCONNECT/COMMENT).

This document consolidates parsing behavior for session and utility statements.
It is implementation-first and aligned with current parser code.

## Statement Dispatch

The following statements are dispatched from the top-level statement parser:
- `SET`, `SHOW`, `RESET`, `DESCRIBE`/`DESC`
- `EXPLAIN`, `ANALYZE`, `SWEEP`
- `CONNECT`, `DISCONNECT`
- `COMMENT`
- `EXECUTE JOB`, `CANCEL JOB RUN`

## SET

### Supported Forms

- `SET [SESSION|LOCAL] <name> = <value>`
- `SET [SESSION|LOCAL] <name> TO <value>`
- `SET [SESSION|LOCAL] <name> TO DEFAULT`
- `SET TIME ZONE <value|LOCAL|DEFAULT>`
- `SET AUTOCOMMIT <ON|OFF|1|0> [ON CONFLICT <action>]`
- `SET TRANSACTION <characteristics>`
- `SET SESSION AUTHORIZATION <user|DEFAULT>`
- `SET ROLE <role|NONE|DEFAULT>`
- `SET SQL DIALECT <1|2|3>`
- `SET NAMES <charset>`
- `SET LOCAL_TIMEOUT <seconds>`
- `SET PARSER VERSION ...` (explicitly rejected)

### Transaction Characteristics (SET TRANSACTION)

Same parsing rules as `START TRANSACTION`:
- Isolation level (READ UNCOMMITTED/COMMITTED, REPEATABLE READ, SERIALIZABLE,
  SNAPSHOT, SNAPSHOT TABLE STABILITY)
- Read committed variants (READ CONSISTENCY, RECORD VERSION, NO RECORD VERSION)
- Access mode (READ ONLY/READ WRITE)
- ON CONFLICT (COMMIT/ROLLBACK/ERROR/KEEP, optional error code)
- DEFERRABLE / NOT DEFERRABLE
- WAIT / NO WAIT
- LOCK TIMEOUT <int>
- RESERVING <table> FOR (SHARED|PROTECTED) (READ|WRITE)
- AUTOCOMMIT <mode>

### Parsing Algorithm (Current Implementation)

1. Parse optional scope: `SESSION` or `LOCAL`.
2. If `SESSION AUTHORIZATION`, parse name or DEFAULT and return.
3. Recognize special SET variants in order: TIME ZONE, AUTOCOMMIT, TRANSACTION,
   SQL DIALECT, NAMES, LOCAL_TIMEOUT, ROLE, PARSER VERSION.
4. Otherwise parse as variable assignment (`name = value` or `name TO value`).
   - `DEFAULT` is supported as the value.
   - Comma-separated values are parsed into a list.

### Autocommit Semantics

When autocommit is enabled, each successful non-transaction-control statement is
committed immediately after execution, and the next statement runs in a new
transaction. This emulates engines that do not expose explicit transaction
isolation while remaining compatible with MGA. When disabled, statements
participate in the current transaction until an explicit COMMIT/ROLLBACK.

## RESET

### Supported Forms

- `RESET ALL`
- `RESET SESSION AUTHORIZATION`
- `RESET ROLE`
- `RESET TIME ZONE`
- `RESET <name>`

## SHOW

### Supported Forms

- `SHOW ALL`
- `SHOW TRANSACTION ISOLATION LEVEL`
- `SHOW TABLES [FROM db] [LIKE pattern]`
- `SHOW DATABASES [LIKE pattern]`
- `SHOW COLUMNS FROM table [LIKE pattern]`
- `SHOW INDEXES FROM table` or `SHOW INDEX name` (Firebird style)
- `SHOW CREATE TABLE name`
- `SHOW TABLE [name]`
- `SHOW TRIGGER[S] [name]`
- `SHOW VIEW[S] [name]`
- `SHOW PROCEDURE[S] [name]`
- `SHOW FUNCTION[S] [name]`
- `SHOW DOMAIN[S] [name]`
- `SHOW GENERATOR[S] | SEQUENCE[S] [name]`
- `SHOW SCHEMA[S] [name]`
- `SHOW ROLE[S] [name]`
- `SHOW GRANTS [FOR name]`
- `SHOW JOBS [LIKE pattern]`
- `SHOW JOB name` or `SHOW JOB RUNS [FOR] job_name`
- `SHOW CHECKS table`
- `SHOW COLLATION[S] [LIKE pattern]`
- `SHOW COMMENTS [name]`
- `SHOW DEPENDENCIES [name]`
- `SHOW PACKAGE[S] name`
- `SHOW SQL DIALECT`
- `SHOW TIME ZONE`
- `SHOW VERSION`
- `SHOW PARSER VERSION` (alias of `SHOW VERSION`)
- `SHOW DATABASE`
- `SHOW SYSTEM`
- `SHOW METRICS`
- `SHOW <variable>` (default)

## DESCRIBE / DESC

- Parsed as `SHOW COLUMNS` for a table with optional column/like pattern.

## EXPLAIN

- Supports `EXPLAIN [ANALYZE] [VERBOSE]` and parenthesized options.
- Options: ANALYZE, VERBOSE, COSTS (ON/OFF), BUFFERS, TIMING (ON/OFF), FORMAT
  (JSON|XML|YAML|TEXT).
- Followed by one of `SELECT|INSERT|UPDATE|DELETE`.

## ANALYZE

- optional `VERBOSE`.
- Requires a table name.
- optional column (only one allowed) using either `(col)` or `COLUMN col`.
- optional `SAMPLE <float|int>` (only one allowed).

## SWEEP

- `SWEEP DATABASE` only.

## CONNECT / DISCONNECT

### CONNECT

- `CONNECT [TO] database`.
- optional clauses (any order): `USER`, `PASSWORD`, `ROLE`, `CHARSET`/`CHARACTER SET`.

### DISCONNECT

- `DISCONNECT ALL | CURRENT | <connection_name>`.

## COMMENT

- `COMMENT ON <object> IS <string|NULL>`
- Supported object types: TABLE, COLUMN, INDEX, VIEW, SEQUENCE, FUNCTION,
  PROCEDURE, TRIGGER, SCHEMA, DATABASE, ROLE, CONSTRAINT.
- For FUNCTION/PROCEDURE, any argument signature in parentheses is skipped.

## JOB Utility

- `EXECUTE JOB <job_name>`
- `CANCEL JOB RUN <run_uuid>`

## Implementation References (Current Code)

- Dispatch: `src/parser/parser_v2.cpp:250`
- `parseSet`: `src/parser/parser_v2.cpp:8789`
- `parseReset`: `src/parser/parser_v2.cpp:9214`
- `parseShow`: `src/parser/parser_v2.cpp:9238`
- `parseDescribe`: `src/parser/parser_v2.cpp:9486`
- `parseExplain`: `src/parser/parser_v2.cpp:9517`
- `parseAnalyze`: `src/parser/parser_v2.cpp:9595`
- `parseSweep`: `src/parser/parser_v2.cpp:9668`
- `parseConnect`: `src/parser/parser_v2.cpp:9885`
- `parseDisconnect`: `src/parser/parser_v2.cpp:9921`
- `parseComment`: `src/parser/parser_v2.cpp:9942`
- `parseExecuteJob`: `src/parser/parser_v2.cpp:10646`
- `parseCancelJobRun`: `src/parser/parser_v2.cpp:10659`

## Notes and Constraints

- `SET PARSER VERSION` is explicitly rejected.
- `SHOW PARSER VERSION` is accepted and maps to `SHOW VERSION`.
- SHOW INDEX supports both MySQL-style and Firebird-style syntaxes.

---

## SBLR Emission (Normative)

### SET / RESET

- `SET TIME ZONE` → `SBLR3_SET_TIME_ZONE`
- `SET AUTOCOMMIT` → `SBLR3_SET_AUTOCOMMIT`
- `SET TRANSACTION` → `SBLR3_SET_TRANSACTION`
- `SET ROLE` → `SBLR3_SET_ROLE`
- `SET SESSION AUTHORIZATION` → `SBLR3_SET_SESSION_AUTH`
- `SET NAMES` → `SBLR3_SET_NAMES`
- `SET SQL DIALECT` → `SBLR3_SET_SQL_DIALECT`
- `SET LOCAL_TIMEOUT` → `SBLR3_SET_LOCAL_TIMEOUT`
- Generic `SET name=value` → `SBLR3_SET_VARIABLE`
- `RESET ALL` → `SBLR3_RESET_ALL`
- `RESET ROLE` → `SBLR3_RESET_ROLE`
- `RESET SESSION AUTHORIZATION` → `SBLR3_RESET_SESSION_AUTH`
- `RESET TIME ZONE` → `SBLR3_RESET_TIME_ZONE`
- `RESET <name>` → `SBLR3_RESET`

### SHOW / DESCRIBE

Each SHOW form emits a specific opcode:
- `SHOW ALL` → `SBLR3_SHOW_ALL`
- `SHOW TABLES` → `SBLR3_SHOW_TABLES`
- `SHOW TABLE` → `SBLR3_SHOW_TABLE`
- `SHOW COLUMNS` / `DESCRIBE` → `SBLR3_SHOW_COLUMNS`
- `SHOW INDEX/INDEXES` → `SBLR3_SHOW_INDEX` / `SBLR3_SHOW_INDEXES`
- `SHOW SCHEMA` → `SBLR3_SHOW_SCHEMA`
- `SHOW ROLE` → `SBLR3_SHOW_ROLE`
- `SHOW GRANTS` → `SBLR3_SHOW_GRANTS`
- `SHOW VERSION` → `SBLR3_SHOW_VERSION`
- `SHOW SYSTEM` → `SBLR3_SHOW_SYSTEM`
- `SHOW METRICS` → `SBLR3_SHOW_METRICS`
- `SHOW SQL DIALECT` → `SBLR3_SHOW_SQL_DIALECT`
- Generic `SHOW <variable>` → `SBLR3_SHOW_VARIABLE`

### EXPLAIN / ANALYZE / COMMENT / CONNECT / DISCONNECT

- `EXPLAIN` → `SBLR3_EXPLAIN_PLAN`
- `ANALYZE` → `SBLR3_ANALYZE`
- `COMMENT ON` → `SBLR3_COMMENT`
- `CONNECT` → `SBLR3_CONNECT`
- `DISCONNECT` → `SBLR3_DISCONNECT`

All identifiers in payloads MUST use `string_id`.

---

## Executor Semantics (Normative)

- `SET SESSION` changes persist for the session until reset.
- `SET LOCAL` applies only to the current transaction; it MUST be rejected
  when no transaction is active (`SQLSTATE 25P01`).
- `RESET` restores default configuration values.
- `SHOW` statements are read‑only and MUST NOT acquire write locks.
- `CONNECT/DISCONNECT` are session control commands and must not emit DDL/DML.

---

## Errors / SQLSTATE (Required)

- `42601` syntax_error
- `42704` undefined_object (unknown SHOW target)
- `0A000` feature_not_supported (explicitly rejected options)
- `25P01` no_active_sql_transaction (SET LOCAL without transaction)
