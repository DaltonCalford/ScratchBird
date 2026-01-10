# MySQL Parser Correction Plan Checklist (Actual)

Purpose: Extracted mismatch list from `18_mysql_parser_statement_reference_actual.md` for implementation planning.

Status: derived from static code review; no runtime execution performed.

## Bytecode format mismatches (execution blockers)
- [x] CREATE TABLE: remove extra IF NOT EXISTS byte or update executor to consume it.
- [x] CREATE TABLE: emit COLUMN_DEF + COLUMN_REF qualifier + name (executor expects qualifier + name).
- [x] CREATE TABLE: encode DEFAULT as bytecode length + expression (executor expects serialized expression, not LITERAL_STRING).
- [x] CREATE TABLE: emit IDENTITY_COLUMN with ALWAYS/BY DEFAULT byte (executor expects a byte).
- [x] CREATE TABLE: move TABLE_FK constraints after columns in executor format (byte counts + strings), not BEGIN_LIST with COLUMN_REF opcodes.
- [x] SELECT: emit flags byte + BEGIN_LIST select list with alias strings to match executor.
- [x] SELECT: emit BEGIN_LIST for FROM with TABLE_REF entries + JOIN_TYPE sequences (aligned with executor).
- [x] INSERT: emit TABLE_REF payload with ref_kind + alias, column list as COLUMN_REF + name, and row_count/row lists.
- [x] INSERT: handle DEFAULT VALUES (row_count=0) and INSERT ... SELECT (emit SELECT bytecode) for executor.
- [x] INSERT: ON DUPLICATE KEY UPDATE emits EXT_ON_CONFLICT + DO UPDATE payloads that executor can read (no explicit target).
- [x] UPDATE: emit TABLE_REF payload with ref_kind + alias and assignments as ASSIGNMENT + COLUMN_REF + expression; ignore extra table list in bytecode.
- [x] DELETE: emit TABLE_REF payload with ref_kind + alias; ORDER BY/LIMIT parsed but not emitted.
- [ ] REPLACE: encoded with legacy payload; still needs ON CONFLICT alignment and semantics.
- [x] LOCK/UNLOCK TABLES: emit explicit errors (avoid invalid top-level opcodes).

## Missing DDL implementations
- [x] CREATE INDEX / CREATE VIEW: TODO stubs.
- [x] CREATE PROCEDURE / FUNCTION / TRIGGER: TODO stubs.
- [x] DROP TABLE / DROP INDEX / DROP VIEW: not implemented.
- [x] TRUNCATE: not implemented.

## Partial implementations / silent drops
- [ ] ALTER TABLE supports only RENAME; other ALTER actions are rejected.
- [x] CREATE TABLE: FULLTEXT/SPATIAL/KEY/INDEX constraints are parsed and emitted as CREATE INDEX.
- [ ] CREATE TABLE: many table options are parsed for syntax but not emitted.

## Cross-cutting
- [ ] Decide whether MySQL parser should target executor (current) format or update executor to accept current parser payloads.
- [ ] Add dialect-specific bytecode versioning if multiple formats will coexist.
