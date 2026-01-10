# MySQL Emulation Parity Audit

Date: 2025-12-20

This audit focuses on MySQL protocol parity with native MySQL clients, including
parser coverage, wire protocol behavior, and metadata/catalog API surfaces.

## Scope
- Parser: `src/parser/mysql/` -> SBLR bytecode
- Wire protocol adapter: `src/protocol/adapters/mysql_adapter.cpp`
- Catalog/metadata: `include/scratchbird/catalog/mysql_catalog.h`,
  `include/scratchbird/catalog/information_schema.h`,
  `include/scratchbird/catalog/emulation_view_generator.h`
- Executor output used by MySQL SHOW/metadata opcodes: `src/sblr/executor.cpp`

## Reference Specs
- `docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `docs/specifications/mysql_mariadb_spec.md`
- `docs/specifications/wire_protocols/mysql_wire_protocol.md`
- `docs/specifications/WIRE_PROTOCOL_SPECIFICATIONS.md`
- `docs/archive/2026-01-09/findings/mysql_wire_protocol_gaps.md` (existing gap list)

## Parser Gaps (Missing or Stubbed Features)

### DDL coverage is largely stubbed
- `parseAlterStmt` always errors (no ALTER support) at
  `src/parser/mysql/mysql_parser.cpp:1927`.
- `parseDropStmt` always errors (no DROP support) at
  `src/parser/mysql/mysql_parser.cpp:1933`.
- `parseTruncateStmt` always errors (no TRUNCATE support) at
  `src/parser/mysql/mysql_parser.cpp:1939`.
- `parseCreateIndex` is TODO (no CREATE INDEX) at
  `src/parser/mysql/mysql_parser.cpp:2306`.
- `parseCreateView` is TODO (no CREATE VIEW) at
  `src/parser/mysql/mysql_parser.cpp:2310`.
- `parseCreateDatabase` is TODO (no CREATE DATABASE / SCHEMA) at
  `src/parser/mysql/mysql_parser.cpp:2314`.
- `parseCreateProcedure` is TODO (no CREATE PROCEDURE) at
  `src/parser/mysql/mysql_parser.cpp:2318`.
- `parseCreateFunction` is TODO (no CREATE FUNCTION) at
  `src/parser/mysql/mysql_parser.cpp:2322`.
- `parseCreateTrigger` is TODO (no CREATE TRIGGER) at
  `src/parser/mysql/mysql_parser.cpp:2326`.

### Table constraints are skipped
- In CREATE TABLE, table-level constraints are detected then skipped (ignored),
  so PRIMARY/UNIQUE/FOREIGN/CHECK/INDEX/FULLTEXT/etc are not emitted.
  `src/parser/mysql/mysql_parser.cpp:1982` to `:1994`.
- `parseIndexDef` and `parseForeignKeyDef` are TODO and never called with
  functional output. `src/parser/mysql/mysql_parser.cpp:2330` and `:2335`.

### RENAME TABLE not implemented
- `RENAME` is lexed (`src/parser/mysql/mysql_lexer.cpp:163`) but no parser
  support exists, so RENAME TABLE is unsupported.

### Expression/semantic gaps
- NULL-safe equality `<=>` now emits `EXT_NULL_SAFE_EQ` with NULL-safe semantics
  (fixed in `src/parser/mysql/mysql_parser.cpp` + executor).
- LIKE ESCAPE clause emits `EXT_LIKE_ESCAPE` and is enforced in executor.
- `?` placeholders emit `LITERAL_NULL`, so prepared statements do not bind
  parameters. `src/parser/mysql/mysql_parser.cpp:1095`.

### Data type mapping gaps
- Geometry types emit `EXTENDED_OPCODE` placeholder instead of a concrete type.
  `src/parser/mysql/mysql_parser.cpp:2299`.

### Qualified name parsing allows feature bleed
- `parseQualifiedName` accepts unlimited dotted segments, which allows
  multi-level schema paths not supported by MySQL. `src/parser/mysql/mysql_parser.cpp:388`.
  For parity, restrict to `db.table` or `table` for table references and
  `db.table.column` for column references only.

## Wire Protocol and Session API Gaps
- Authentication is trust-mode; password validation is TODO.
  `src/protocol/adapters/mysql_adapter.cpp:616`.
- `COM_INIT_DB` does not validate database existence and always OKs the request.
  `src/protocol/adapters/mysql_adapter.cpp:714`.
- Prepared statement placeholders are not wired to parser parameter binding
  (parser emits NULLs for `?`), so COM_STMT_PREPARE/EXECUTE is incomplete.
  `src/parser/mysql/mysql_parser.cpp:1095`.

## Catalog and Metadata API Gaps

### mysql.* system schema is stubbed
- `MySQLCatalogHandler` returns empty results and placeholder columns for
  all mysql.* tables. `include/scratchbird/catalog/mysql_catalog.h`.
  This breaks clients/tools that query `mysql.user`, `mysql.db`, etc.

### information_schema is stubbed and incomplete
- `InformationSchemaHandler` returns empty results for standard tables.
  `include/scratchbird/catalog/information_schema.h`.
- `EmulationViewGenerator` provides only INFORMATION_SCHEMA.TABLES for MySQL,
  no COLUMNS/STATISTICS/ROUTINES/etc. `include/scratchbird/catalog/emulation_view_generator.h:571`.
- `bootstrapInformationSchema` creates local tables but populates them from
  ScratchBird `information_schema.*`, which is a stub, so results are empty.
  `src/protocol/adapters/mysql_adapter.cpp:1551`.

### SHOW / DESCRIBE output does not match MySQL
- MySQL parser emits `EXT_SHOW_*` opcodes. Executor implementation uses
  ScratchBird schema `PUBLIC` and ScratchBird column formats, not MySQL's
  expected result shape or database scoping. `src/sblr/executor.cpp:22637`.
  This breaks clients expecting MySQL metadata columns (e.g. `Tables_in_db`,
  `Field`, `Type`, `Null`, `Key`, `Default`, `Extra`).

## Feature Bleed / Non-MySQL Constructs
- Multi-segment qualified names (see parser gap above) allow ScratchBird-style
  schema paths.
- SHOW commands are executed using ScratchBird semantics; for strict MySQL
  parity, SHOW should be implemented as MySQL result sets or rewritten to
  information_schema queries in the adapter.

## Test Coverage Gaps
- Parser tests exist but do not cover DDL (ALTER/DROP/CREATE DATABASE/VIEW/
  PROCEDURE/FUNCTION/TRIGGER/INDEX) or constraint parsing.
  `tests/unit/test_mysql_parser.cpp`.
- No tests validate MySQL protocol metadata (mysql.* tables,
  information_schema tables, SHOW outputs).
- No integration tests with native MySQL clients (mysql CLI, JDBC, ODBC)
  are present in `tests/`.

## Summary (MySQL Parity Risk)
Current MySQL emulation is limited to a subset of DML and SELECT parsing.
DDL, constraints, prepared statements, authentication, and metadata queries
are incomplete or stubbed. A native MySQL client will not see expected
catalogs, SHOW results, or authentication behavior. These gaps must be
resolved for 1:1 parity with MySQL 8.0 clients.
