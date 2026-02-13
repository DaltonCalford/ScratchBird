# PostgreSQL Parser Alpha Tracker

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Source audit:** `docs/findings/EMULATED_PARSER_FULL_AUDIT_2026-02-02.md`

## Alpha Blockers

- [x] Align SELECT/INSERT/UPDATE/DELETE bytecode payloads to executor expectations (see `/docs/specifications/parser/v3/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md:381-405`).
- [x] Align CREATE TABLE/CREATE INDEX/CREATE VIEW/ALTER TABLE bytecode payloads to executor expectations (see `/docs/specifications/parser/v3/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md:372-380`).

## Progress Notes

- 2026-02-02: Fixed FROM clause list count encoding, TABLE_REF payload format, and schema/table path emission in `src/parser/postgresql/pg_parser_dml.cpp:273-334`.
- 2026-02-02: Emitted UPDATE FROM / DELETE USING lists using executor-compatible payloads in `src/parser/postgresql/pg_parser_dml.cpp:1172-1254`.
- 2026-02-02: Aligned CREATE INDEX payload structure with executor options/predicate flags in `src/parser/postgresql/pg_parser_ddl.cpp:1046-1108`.
- 2026-02-02: Fixed ON CONFLICT constraint target emission to avoid invalid payloads in `src/parser/postgresql/pg_parser_dml.cpp:1015-1045`.
- 2026-02-02: Aligned MERGE payload shape (target/source strings, length-prefixed expressions) and fixed JOIN TABLE_REF emission in `src/parser/postgresql/pg_parser_dml.cpp:360-1490`.
- 2026-01-28: Verified CREATE TABLE/VIEW/ALTER TABLE payloads align with executor expectations and SELECT/INSERT/UPDATE/DELETE payloads remain compatible (`src/parser/postgresql/pg_parser_ddl.cpp`, `src/parser/postgresql/pg_parser_dml.cpp`).
- 2026-01-28: Explicitly rejected JSONPATH and network types in PostgreSQL type emission (`src/parser/postgresql/pg_parser.cpp`).
- 2026-02-02: Added expression index payloads with INCLUDE and SBLR expression lists in `src/parser/postgresql/pg_parser_ddl.cpp:1206-1325`.
- 2026-02-02: CREATE TYPE RANGE now emits RANGE domain payloads in `src/parser/postgresql/pg_parser_ddl.cpp:2216-2258`.

## High Priority (Alpha)

- [x] Map network types INET/CIDR/MACADDR/MACADDR8 or explicitly reject with errors (see `/docs/specifications/parser/v3/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md:461-480`).
- [x] JSONPATH handling (parse + type mapping or explicit rejection) (`include/scratchbird/parser/postgresql/pg_parser.h:94`).
- [x] Document/implement array domain type encoding (TYPE_DOMAIN array flag) (`src/parser/postgresql/pg_parser.cpp:645-651`).
- [x] GRANT/REVOKE ON ALL bytecode support or explicit error in spec (`src/parser/postgresql/pg_parser_misc.cpp:588, 715`).

- [x] Expression indexes + INCLUDE clause support. (`src/parser/postgresql/pg_parser_ddl.cpp:1206-1325`).
- [x] CREATE TYPE RANGE support. (`src/parser/postgresql/pg_parser_ddl.cpp:2216-2258`).
