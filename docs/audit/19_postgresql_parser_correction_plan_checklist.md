# PostgreSQL Parser Correction Plan Checklist (Actual)

Purpose: Extracted mismatch list from `17_postgresql_parser_statement_reference_actual.md` for implementation planning.

Status: derived from static code review; no runtime execution performed.

## Bytecode format mismatches (execution blockers)
- [ ] CREATE TABLE: remove extra IF NOT EXISTS byte or update executor to consume it; align column definition format (executor expects COLUMN_DEF + COLUMN_REF qualifier + name).
- [ ] CREATE TABLE: move table-level constraints out of column list or teach executor to accept PRIMARY_KEY/UNIQUE/TABLE_FK opcodes inside column list.
- [ ] CREATE INDEX: reorder payload to match executor (index_name, table_name, is_unique, column count, column names, tablespace, index_type, expression flags).
- [ ] CREATE VIEW / MATERIALIZED VIEW: emit SQL definition string + flags instead of SELECT bytecode, or extend executor to accept SELECT bytecode.
- [ ] CREATE SEQUENCE: emit payload expected by executor (options + name ordering).
- [ ] CREATE USER: emit flags byte (has_password/is_superuser) before optional password to match executor.
- [ ] SET ROLE / SET SESSION AUTHORIZATION: emit flags byte (RESET) + optional name to match executor expectations.
- [ ] SET CONSTRAINTS: emit flags byte (ALL/DEFERRED) and name_count byte + names to match executor layout.
- [ ] ALTER TABLE: replace legacy ALTER_TABLE emission with executor’s expected payload (table name string + action code + fields), or add executor support for legacy format.
- [ ] DROP TABLE/INDEX/VIEW/SEQUENCE: switch from TABLE_REF lists to single name string + flags byte; align CASCADE/RESTRICT encoding.
- [ ] TRUNCATE: emit single table name + flags, not TABLE_REF lists and optional extra bytes.
- [ ] SELECT: remove DISTINCT flag byte or update executor to read it; align SELECT list alias encoding (executor expects alias markers as COLUMN_REF + empty qualifier).
- [ ] SELECT: align FROM clause layout (executor expects single TABLE_REF for legacy format); current parser emits BEGIN_LIST and join opcodes.
- [ ] INSERT: remove alias string after TABLE_REF; align values layout (executor expects a single row list without row_count); handle DEFAULT VALUES / SELECT sources explicitly.
- [ ] UPDATE: emit ASSIGNMENT + COLUMN_REF + column name (executor expects COLUMN_REF opcode); remove alias string after TABLE_REF.
- [ ] DELETE: remove alias string after TABLE_REF; remove USING clause or add executor support.
- [ ] MERGE: add executor handling for EXT_MERGE_* or drop support in parser.
- [ ] WITH/CTE: align EXT_WITH_CLAUSE payload layout with executor (executor expects count + recursive flag, not list).

## Missing executor handlers (opcode gaps)
- [ ] SHOW ALL / SHOW VARIABLE / SHOW TRANSACTION LEVEL: executor has no EXT_SHOW_* handlers for these PostgreSQL opcodes.
- [ ] GRANT / REVOKE: parser emits EXT_GRANT/EXT_REVOKE, executor expects EXT_GRANT_PRIVILEGE/EXT_REVOKE_PRIVILEGE.
- [ ] EXPLAIN: EXPLAIN_PLAN opcode is not handled by executor.
- [ ] COPY: no top-level opcode emitted; executor has no COPY support.

## Parser partials / silent drops
- [ ] CREATE TABLE: WITH options and TABLESPACE are parsed but not emitted.
- [ ] CREATE INDEX: expression indexes are parsed but not emitted; INCLUDE and WHERE are parsed but only partially emitted.
- [ ] CREATE VIEW: WITH CHECK OPTION emits only when present; no default byte, so payload is ambiguous.
- [ ] INSERT/UPDATE/DELETE: RETURNING and ON CONFLICT are emitted but executor has no support.
- [ ] CREATE SEQUENCE: options parsed but dropped.

## Cross-cutting
- [ ] Decide whether PostgreSQL parser should target executor (current) format or move executor to accept parser’s richer format.
- [ ] Add bytecode versioning or dialect flags if multiple payload formats must coexist.
