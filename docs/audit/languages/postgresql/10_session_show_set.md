# PostgreSQL - Session, SHOW, SET

Spec refs:
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `ScratchBird/docs/audit/17_postgresql_parser_statement_reference_actual.md`

## SET commands
- `SET search_path`: Implemented (executor supports EXT_SET_VARIABLE for search_path).
- `SET ROLE`, `SET SESSION AUTHORIZATION`, `SET CONSTRAINTS`, `SET TIME ZONE`:
  Stubbed (payload mismatch or unsupported in executor).

Example:
```sql
SET search_path TO app, public;
```

## SHOW commands
Description: SHOW and SHOW ALL are parsed but executor has no handlers for
PostgreSQL EXT_SHOW_* opcodes.

Status: Stubbed.

Notes:
- Use V2 SHOW commands in native dialect for actual catalog inspection.
- PostgreSQL-style SHOW DATABASE/SHOW SCHEMA are not implemented; use catalog
  queries against `pg_catalog`/`information_schema` once parity is confirmed.
