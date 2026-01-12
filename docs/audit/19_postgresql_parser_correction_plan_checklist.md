# PostgreSQL Parser Correction Plan Checklist (Actual)

Purpose: Extracted mismatch list from `17_postgresql_parser_statement_reference_actual.md` for implementation planning.

Status: derived from static code review; no runtime execution performed.

## Bytecode format mismatches (execution blockers)
- [x] CREATE TABLE: remove extra IF NOT EXISTS byte or update executor to consume it; align column definition format (executor expects COLUMN_DEF + COLUMN_REF qualifier + name).
- [ ] CREATE TABLE: move table-level constraints out of column list or teach executor to accept PRIMARY_KEY/UNIQUE/TABLE_FK opcodes inside column list.
- [x] CREATE INDEX: reorder payload to match executor (index_name, table_name, is_unique, column count, column names, tablespace, index_type, expression flags).
- [x] CREATE VIEW / MATERIALIZED VIEW: emit SQL definition string + flags instead of SELECT bytecode, or extend executor to accept SELECT bytecode.
- [x] CREATE SEQUENCE: emit payload expected by executor (options + name ordering).
- [x] CREATE USER: emit flags byte (has_password/is_superuser) before optional password to match executor.
- [x] SET ROLE / SET SESSION AUTHORIZATION: emit flags byte (RESET) + optional name to match executor expectations.
- [x] SET CONSTRAINTS: emit flags byte (ALL/DEFERRED) and name_count byte + names to match executor layout.
- [x] ALTER TABLE: replace legacy ALTER_TABLE emission with executor’s expected payload (table name string + action code + fields), or add executor support for legacy format.
- [x] DROP TABLE/INDEX/VIEW/SEQUENCE: switch from TABLE_REF lists to single name string + flags byte; align CASCADE/RESTRICT encoding.
- [x] TRUNCATE: emit single table name + flags, not TABLE_REF lists and optional extra bytes.
- [x] SELECT: emit flags byte + BEGIN_LIST select list with alias strings to match executor.
- [x] SELECT: emit BEGIN_LIST for FROM with TABLE_REF entries + JOIN_TYPE sequences (aligned with executor).
- [x] INSERT: emit TABLE_REF payload with ref_kind + alias, column list as COLUMN_REF + name, and row_count/row lists; support DEFAULT VALUES and INSERT ... SELECT.
- [x] UPDATE: emit TABLE_REF payload with ref_kind + alias and assignments as ASSIGNMENT + COLUMN_REF + expression.
- [x] DELETE: emit TABLE_REF payload with ref_kind + alias; ignore USING clause in bytecode (executor has no support).
- [ ] MERGE: add executor handling for EXT_MERGE_* or drop support in parser.
- [x] WITH/CTE: align EXT_WITH_CLAUSE payload layout with executor (executor expects count + recursive flag, not list).

## Missing executor handlers (opcode gaps)
- [ ] SHOW ALL / SHOW VARIABLE / SHOW TRANSACTION LEVEL: executor has no EXT_SHOW_* handlers for these PostgreSQL opcodes.
- [x] GRANT / REVOKE: parser emits EXT_GRANT/EXT_REVOKE, executor expects EXT_GRANT_PRIVILEGE/EXT_REVOKE_PRIVILEGE.
- [x] EXPLAIN: EXPLAIN_PLAN opcode is not handled by executor.
- [x] COPY: no top-level opcode emitted; executor has no COPY support.

## Parser partials / silent drops
- [ ] CREATE TABLE: WITH options and TABLESPACE are parsed but not emitted.
- [ ] CREATE INDEX: expression indexes are parsed but not emitted; INCLUDE and WHERE are parsed but only partially emitted.
- [x] CREATE VIEW: WITH CHECK OPTION emits only when present; no default byte, so payload is ambiguous.
- [ ] INSERT/UPDATE/DELETE: RETURNING and ON CONFLICT are emitted but executor has no support.
- [x] CREATE SEQUENCE: options parsed but dropped.

## Cross-cutting
- [ ] Decide whether PostgreSQL parser should target executor (current) format or move executor to accept parser’s richer format.
- [ ] Add bytecode versioning or dialect flags if multiple payload formats must coexist.
