# Emulated Parser Full Audit (PostgreSQL/MySQL/Firebird)
Status: Superseded (implementation verified)
Last Updated: 2026-02-02

Note: All gaps called out here are closed. Track any remaining work in
`docs/planning/TRACKER_OUTSTANDING_MASTER.md`.


**Date:** 2026-02-02
**Scope:** PostgreSQL, MySQL, and Firebird emulated parsers (lexer + parser + bytecode emission + catalog emulation) against their specs.

## Sources Reviewed

- `docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md`
- `docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md`
- `docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md`
- `docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`
- `docs/specifications/V2_PARSER_FIREBIRD_ALIGNMENT_SPECIFICATION.md` (Firebird-style alignment)
- Parser implementations:
  - `src/parser/postgresql/pg_parser*.cpp`
  - `src/parser/mysql/mysql_parser.cpp`
  - `src/parser/firebird/firebird_parser.cpp`
- Virtual catalogs:
  - `include/scratchbird/catalog/pg_catalog.h`
  - `include/scratchbird/catalog/mysql_catalog.h`
  - `include/scratchbird/catalog/firebird_catalog.h`
  - `src/catalog/firebird_catalog.cpp`

## Summary (High-Level)

- **PostgreSQL parser**: broad syntax coverage, but still has **bytecode format mismatches** vs executor and several PostgreSQL-specific features are parsed but emitted in a format the executor rejects. Some DDL/DML features are explicitly rejected or stubbed.
- **MySQL parser**: DDL/DML surface is large but many MySQL-specific features are **parsed but ignored** or **explicitly rejected**. ALTER TABLE coverage is limited. Several important DML/DDL modifiers are not wired to bytecode.
- **Firebird parser**: core DDL/DML/PSQL entry points exist and many Firebird constructs parse, but **ALTER/DROP/RECREATE coverage is incomplete** and multiple features remain unimplemented or partially implemented. Firebird parity spec lists required items that are still missing.

---

# PostgreSQL Parser Audit

## Lexer/Keywords

- Lexer recognizes PG keywords and types, but the full reserved/non‑reserved keyword list in `docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md` is **not exhaustively mapped**. No automated verification exists.
- Action: add keyword coverage checks or tests to ensure reserved keyword lists match the spec.

## Type System Coverage

**Implemented:** core scalar types (int, float, numeric, text, bytea, date/time, UUID, JSON/JSONB).
- Type mapping in `src/parser/postgresql/pg_parser.cpp:594-715`.

**Gaps/Partial:**
- **Network types** (INET/CIDR/MACADDR/MACADDR8) are parsed in `src/parser/postgresql/pg_parser_ddl.cpp:803-811` but fall through to default handling (historically mapped to VARCHAR). See `docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md:461-480`.
- **JSONPATH** is listed in the enum in `include/scratchbird/parser/postgresql/pg_parser.h:96` but no opcode mapping exists in `src/parser/postgresql/pg_parser.cpp:594-631`.
- **Array domain types** are not supported in the domain payload (`src/parser/postgresql/pg_parser.cpp:645-651`).

**Recently fixed (still needs spec update):**
- ARRAY type mapping is now emitted as `TYPE_ARRAY` with element metadata and size. (Implemented in `src/parser/postgresql/pg_parser.cpp:650-705` and `src/parser/postgresql/pg_parser_ddl.cpp:830-861`.)

## DDL Coverage

**Implemented/Partial:**
- CREATE TABLE, CREATE INDEX, CREATE VIEW, CREATE MATERIALIZED VIEW, CREATE SEQUENCE; basic ALTER TABLE, DROP TABLE/VIEW/INDEX/SEQUENCE.
- See `src/parser/postgresql/pg_parser_ddl.cpp:158-3165`.

**Explicitly not supported or stubbed:**
- TABLESPACE clauses (emulated parsers reject). `src/parser/postgresql/pg_parser_ddl.cpp:496, 1062, 1460`.
- Expression indexes are rejected. `src/parser/postgresql/pg_parser_ddl.cpp:1067`.
- ALTER TABLE DROP CONSTRAINT and ALTER COLUMN SET/DROP DEFAULT, SET/DROP NOT NULL, USING are rejected. `src/parser/postgresql/pg_parser_ddl.cpp:2627-2662`.
- CREATE TYPE RANGE explicitly rejected. `src/parser/postgresql/pg_parser_ddl.cpp:1973`.
- CREATE DOMAIN base type rejected (requires specific mapping). `src/parser/postgresql/pg_parser_ddl.cpp:2112`.
- ALTER object types beyond TABLE/DOMAIN/etc are stubbed. `src/parser/postgresql/pg_parser_ddl.cpp:2803`.

## DML Coverage

**Implemented/Partial:**
- SELECT/INSERT/UPDATE/DELETE/UPSERT, RETURNING, JOINs, array expressions, COPY (parser surface).
- See `src/parser/postgresql/pg_parser_dml.cpp` and `src/parser/postgresql/pg_parser_expr.cpp`.

**Explicitly not supported or partial:**
- DEFAULT values in multi-row INSERT rejected. `src/parser/postgresql/pg_parser_dml.cpp:905`.

## Utility/Session/DCL

**Implemented/Partial:**
- SET/SHOW (PG-style) plus transactional controls. `src/parser/postgresql/pg_parser_misc.cpp`.

**Explicitly rejected:**
- SHOW TABLES / DATABASES / COLUMNS / INDEXES in PG dialect. `src/parser/postgresql/pg_parser_misc.cpp:265-277`.
- GRANT/REVOKE ON ALL rejected for current bytecode. `src/parser/postgresql/pg_parser_misc.cpp:588, 715`.
- TRUNCATE options rejected. `src/parser/postgresql/pg_parser_ddl.cpp:3165`.

## Bytecode Compatibility Gaps (Critical)

Per `docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md:372-405`, **current bytecode output does not match executor expectations** for several key statements:

- CREATE TABLE / CREATE INDEX / CREATE VIEW / ALTER TABLE
- SELECT / INSERT / UPDATE / DELETE / MERGE

These are listed as **CRITICAL** executor-rejection risks in the spec gap doc and must be reconciled (parser vs executor format). Until corrected, the PG parser is not “full fidelity”.

## Catalog Emulation Coverage

- Pg catalog handler exists but is likely minimal: `include/scratchbird/catalog/pg_catalog.h`.
- PostgreSQL adapter currently inserts minimal pg_catalog placeholders (`src/protocol/adapters/postgresql_adapter.cpp:1551`).
- `information_schema` is provided by `InformationSchemaHandler` but completeness vs PG spec is unverified.

**Audit gap:** No systematic coverage test against `pg_catalog`/`information_schema` tables in the PG spec; this is required for 1:1 compatibility (psql/pgAdmin rely on these).

---

# MySQL Parser Audit

## Lexer/Keywords

- Lexer includes a subset of the MySQL 8.0 keyword list but no validation exists against the full list in `docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md`.

## Type System Coverage

**Implemented/Partial:**
- Numeric, string, binary, date/time, JSON types mapped in `src/parser/mysql/mysql_parser.cpp:119-181` and `src/parser/mysql/mysql_parser.cpp:4307-4312`.

**Gaps/Partial:**
- UNSIGNED / ZEROFILL are parsed but require enforcement. See `docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:332-376`.
- ENUM/SET types are parsed but mapping/enforcement is not fully defined.

## DDL Coverage

**Implemented/Partial:**
- CREATE DATABASE/SCHEMA, CREATE TABLE, CREATE VIEW, CREATE INDEX, CREATE PROCEDURE (stub), TRUNCATE. `src/parser/mysql/mysql_parser.cpp:2887-4900`.
- Basic ALTER TABLE: rename/add/drop/modify/change columns. `src/parser/mysql/mysql_parser.cpp:3000-3560`.

**Explicitly not supported or stubbed:**
- CREATE statements beyond table/view/index: error path in `src/parser/mysql/mysql_parser.cpp:2887`.
- ALTER TABLE ADD/DROP INDEX rejected. `src/parser/mysql/mysql_parser.cpp:3282-3298`.
- ALTER TABLE ALTER COLUMN rejected. `src/parser/mysql/mysql_parser.cpp:3349`.
- Partition options rejected. `src/parser/mysql/mysql_parser.cpp:3781`.
- DROP statements beyond table/view/index are stubbed. `src/parser/mysql/mysql_parser.cpp:3458`.

**Critical gaps (from spec gap doc):**
- TEMPORARY tables: parsed but silently treated as permanent. `docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:38-78`.
- Table options coverage incomplete (ROW_FORMAT, KEY_BLOCK_SIZE, ENGINE, etc.). `docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:431-557`.

## DML Coverage

**Implemented/Partial:**
- INSERT/REPLACE/UPDATE/DELETE with MySQL syntax forms. `src/parser/mysql/mysql_parser.cpp:1860-2790`.

**Explicitly not supported or partial:**
- DEFAULT values in multi-row INSERT/REPLACE rejected. `src/parser/mysql/mysql_parser.cpp:2279, 2709`.
- INSERT modifiers (LOW_PRIORITY/DELAYED/HIGH_PRIORITY/IGNORE) parsed but ignored. `src/parser/mysql/mysql_parser.cpp:1982-1985`.
- INSERT IGNORE does not map to ON CONFLICT DO NOTHING. `docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:246-266`.
- ON DUPLICATE KEY UPDATE parsing exists but **bytecode emission is disabled** (silent failure). `docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:90-150`.

## Utility/Session/DCL

- LOCK/UNLOCK TABLES explicitly unsupported. `src/parser/mysql/mysql_parser.cpp:5509-5517`.
- SHOW variants are limited; parity vs MySQL spec is unverified.

## Bytecode Compatibility Gaps (Critical)

- MySQL gap spec notes broad mismatches (40-50% executor compatibility). See `docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`.
- Several MySQL-specific constructs are parsed but either emit no bytecode or emit incompatible payloads, leading to silent failures.

## Catalog Emulation Coverage

- MySQL catalog handler exists: `include/scratchbird/catalog/mysql_catalog.h`.
- The MySQL protocol adapter bootstraps `information_schema` entries in `src/protocol/adapters/mysql_adapter.cpp:1704-1930`.
- Completeness of `mysql.*` and `performance_schema` is not verified against MySQL 8.0 spec.

---

# Firebird Parser Audit

## Lexer/Keywords

- Firebird lexer/token set is substantial. However, specific keyword handling gaps are called out in `docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md`, notably:
  - `RDB$GET_CONTEXT` / `RDB$SET_CONTEXT` defined in lexer but not in `isNonReservedKeyword` (parser may reject function usage). See spec section “Context Variables”.

## Type System Coverage

- Firebird types and array handling are parsed (see `src/parser/firebird/firebird_parser.cpp:1470-1535`).
- Domain, array, and charset/collation handling are present, but full enforcement depends on executor.

## DDL Coverage

**Implemented/Partial:**
- CREATE DATABASE/TABLE/INDEX/VIEW/SEQUENCE/PROCEDURE/FUNCTION/TRIGGER/DOMAIN/EXCEPTION/ROLE/PACKAGE. `src/parser/firebird/firebird_parser.cpp:1720-1935`.
- ALTER TABLE/DOMAIN/INDEX/VIEW/PROCEDURE/FUNCTION/TRIGGER/PACKAGE/EXCEPTION partial. `src/parser/firebird/firebird_parser.cpp:1940-2068`.
- DROP DATABASE/TABLE/INDEX/VIEW/DOMAIN/SEQUENCE/PROCEDURE/FUNCTION/TRIGGER/PACKAGE/ROLE/EXCEPTION partial. `src/parser/firebird/firebird_parser.cpp:2068-2167`.

**Explicitly not supported or stubbed:**
- ALTER/DROP/RECREATE for various object types fall into generic “not yet implemented” errors. `src/parser/firebird/firebird_parser.cpp:1982, 2068, 2223`.
- ALTER DATABASE options rejected. `src/parser/firebird/firebird_parser.cpp:2201`.
- ALTER TABLE SET rejected. `src/parser/firebird/firebird_parser.cpp:2423`.

## DML Coverage

**Implemented/Partial:**
- SELECT/INSERT/UPDATE/DELETE, MERGE, RETURNING, and Firebird-specific syntax forms are parsed. `src/parser/firebird/firebird_parser.cpp:3000-3700`.

**Spec‑driven gaps (from Firebird parity spec):**
- UPDATE OR INSERT (Firebird upsert) needs verification; V2 parser lacks it per spec. `docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md:329-357`.
- MERGE executor support for `EXT_MERGE_*` opcodes remains a broader engine gap (spec notes executor support missing).

## PSQL Coverage

- Parser includes PSQL statement parsing in `src/parser/firebird/firebird_parser.cpp:4263-4768`.
- However, parity spec still lists PSQL completeness gaps and additional test coverage requirements (control structures, exceptions, and runtime support). `docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md:378-1009`.

## Utility/Session/DCL

- Firebird transaction syntax (RECORD_VERSION/NO_RECORD_VERSION etc.) is parsed per spec, but V2 alignment is flagged as needing verification.
- GRANT/REVOKE/COMMENT/SHOW paths exist but not fully validated against Firebird reference.

## Catalog Emulation Coverage

- Firebird catalog handler is comprehensive in breadth: `src/catalog/firebird_catalog.cpp` implements RDB$, MON$, and SEC$ tables.
- However, recent WS‑7 gaps were enumerated for MON$ parity; those must be checked against the monitoring spec and Firebird reference.

---

# Cross‑Parser Gaps & Requirements to Finish Alpha

## Critical (Alpha‑Blocking)

- **Bytecode compatibility fixes for PostgreSQL parser** (DDL/DML payload alignment). See `docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md:372-405`.
- **MySQL ON DUPLICATE KEY UPDATE semantics** (currently parsed but no bytecode). `docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:90-150`.
- **MySQL TEMPORARY tables semantics** (currently parsed but treated as permanent). `docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:38-78`.
- **Firebird ALTER/DROP/RECREATE completeness** (explicit errors in parser). `src/parser/firebird/firebird_parser.cpp:1982, 2068, 2223`.

## High Priority (Alpha)

- **PG network types** and **JSONPATH** handling (mapped or rejected explicitly).
- **MySQL INSERT IGNORE** -> ON CONFLICT DO NOTHING remap.
- **MySQL ALTER TABLE ADD/DROP INDEX** support or explicit rejection in spec/tests.
- **Firebird context functions** (`RDB$GET_CONTEXT` / `RDB$SET_CONTEXT`) keyword handling in parser.
- **Catalog completeness verification** for pg_catalog/mysql.*/information_schema/performance_schema.

## Medium/Low Priority (Post‑Alpha)

- PG UNLOGGED tables (WAL semantics).
- PG expression indexes / INCLUDE indexes.
- MySQL table options (ROW_FORMAT, KEY_BLOCK_SIZE, ENGINE, etc.)
- Full Firebird PSQL test suite and executor conformance audit.

---

# Recommended Next Steps

1. **Update gap specs to reflect current fixes** (PG ARRAY now implemented; ensure `docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md` is updated).
2. **Create a per‑parser checklist** from this audit with explicit acceptance tests (DDL, DML, utility, catalog queries).
3. **Resolve parser/executor bytecode mismatches** in PG and MySQL as the top Alpha blockers.
4. **Add catalog parity tests** for pg_catalog, information_schema, mysql.* and Firebird MON$ tables.
