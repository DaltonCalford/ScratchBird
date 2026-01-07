# MySQL Parser Correction Plan Checklist (Actual)

Purpose: Extracted mismatch list from `18_mysql_parser_statement_reference_actual.md` for implementation planning.

Status: derived from static code review; no runtime execution performed.

## Bytecode format mismatches (execution blockers)
- [ ] CREATE TABLE: remove extra IF NOT EXISTS byte or update executor to consume it.
- [ ] CREATE TABLE: emit COLUMN_DEF + COLUMN_REF qualifier + name (executor expects qualifier + name).
- [ ] CREATE TABLE: encode DEFAULT as bytecode length + expression (executor expects serialized expression, not LITERAL_STRING).
- [ ] CREATE TABLE: emit IDENTITY_COLUMN with ALWAYS/BY DEFAULT byte (executor expects a byte).
- [ ] CREATE TABLE: move TABLE_FK constraints after columns in executor format (byte counts + strings), not BEGIN_LIST with COLUMN_REF opcodes.
- [ ] SELECT: remove DISTINCT flag byte or update executor to consume it; align alias encoding (executor expects alias markers as COLUMN_REF + empty qualifier).
- [ ] SELECT: align FROM clause layout (executor expects single TABLE_REF for legacy format); current parser emits BEGIN_LIST and JOIN_TYPE sequences.
- [ ] INSERT: emit column list as COLUMN_REF + column name only (no schema/table qualifiers); emit a single values list (no row_count/row nesting).
- [ ] INSERT: handle DEFAULT VALUES and INSERT SELECT in executor or drop from parser.
- [ ] INSERT: ON DUPLICATE KEY UPDATE is emitted as EXT_ON_CONFLICT_DO_UPDATE, which executor does not handle.
- [ ] UPDATE: remove table list + alias emission; executor expects TABLE_REF then assignment list with COLUMN_REF names.
- [ ] DELETE: remove alias string after TABLE_REF and drop ORDER BY/LIMIT payload unless executor adds support.
- [ ] REPLACE: encoded as INSERT + EXT_ON_CONFLICT_DO_UPDATE; executor does not support.
- [ ] LOCK/UNLOCK TABLES: parser emits LITERAL_NULL at top-level, which executor does not handle as a statement opcode.

## Missing DDL implementations
- [ ] CREATE INDEX / CREATE VIEW: TODO stubs.
- [ ] CREATE PROCEDURE / FUNCTION / TRIGGER: TODO stubs.
- [ ] DROP TABLE / DROP INDEX / DROP VIEW: not implemented.
- [ ] TRUNCATE: not implemented.

## Partial implementations / silent drops
- [ ] ALTER TABLE supports only RENAME; other ALTER actions are rejected.
- [ ] CREATE TABLE: FULLTEXT/SPATIAL/KEY/INDEX constraints are parsed and dropped.
- [ ] CREATE TABLE: many table options are parsed for syntax but not emitted.

## Cross-cutting
- [ ] Decide whether MySQL parser should target executor (current) format or update executor to accept current parser payloads.
- [ ] Add dialect-specific bytecode versioning if multiple formats will coexist.
