# Native (V2) - Session, SHOW, SET

Spec refs:
- `ScratchBird/docs/specifications/07_TRANSACTION_AND_SESSION_CONTROL.md`
- `ScratchBird/docs/specifications/SCHEMA_PATH_RESOLUTION.md`
- `ScratchBird/docs/specifications/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `ScratchBird/docs/audit/25_show_set_commands_actual.md`

## SET commands
### Implemented
- `SET TRANSACTION ...` (see `08_transactions.md`)
- `SET AUTOCOMMIT {ON|OFF|1|0}`
- `SET SQL DIALECT n`
- `SET NAMES <charset>`
- `SET LOCAL_TIMEOUT <seconds>`

### Partial / stubbed
- `SET ROLE <role>`: parsed but bytecode payload mismatch.
- `SET SESSION AUTHORIZATION <user>`: parsed but bytecode payload mismatch.
- `SET TIME ZONE <value>`: parsed; no bytecode emitted.
- `SET <var> TO/= <expr>`: only SEARCH_PATH is accepted at runtime.
- `SET SEARCH_PATH TO a, b`: list values are dropped in semantic/bytecode.
- `SET SCHEMA <schema>`: only works as `SET SCHEMA TO/=` and still fails in
  executor.
- `SET CONSTRAINTS`: executor supports, parser does not emit.

Example:
```sql
SET AUTOCOMMIT OFF;
SET SEARCH_PATH TO app, public;
```

## SHOW commands
### Implemented (server)
- `SHOW TABLES [FROM schema] [LIKE pattern]`
- `SHOW DATABASES [LIKE pattern]`
- `SHOW COLUMNS FROM table [LIKE pattern]`
- `SHOW INDEXES FROM table`
- `SHOW CREATE TABLE table`
- Firebird-style: `SHOW TABLE/INDEX/TRIGGER/VIEW/PROCEDURE/FUNCTION/DOMAIN/
  GENERATOR/ROLE/GRANTS/CHECKS/COLLATIONS/COMMENTS/DEPENDENCIES/PACKAGE`
- `SHOW SQL DIALECT`, `SHOW VERSION`, `SHOW DATABASE`, `SHOW SYSTEM`
- `SHOW SCHEMA [schema]` (empty lists schemas)

### Stubbed / missing
- `SHOW ALL`, `SHOW <variable>`, `SHOW TRANSACTION ISOLATION LEVEL`:
  parser emits, executor lacks handlers.
- Schema navigation SHOW commands (`SHOW SCHEMA PATH/TREE/SEARCH PATH/...`) are
  implemented in executor but parser does not emit.
- `SHOW CREATE DATABASE` is spec-only.

## SHOW DATABASE vs SHOW SCHEMA (clarification)
- `SHOW DATABASES` lists schemas (MySQL-style list); this is effectively a list
  of schema names, not physical databases.
- `SHOW SCHEMA` with no argument also lists schemas; with a schema name, it
  returns Firebird-style property/value details for that schema.
- `SHOW DATABASE` (singular) returns current database info, not a list.

Notes:
- Some SET/SHOW commands are client-only in `sb_isql` and never reach the
  server (see `docs/audit/25_show_set_commands_actual.md`).
