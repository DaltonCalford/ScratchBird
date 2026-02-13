# PostgreSQL Parser Specification (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the PostgreSQL 16+ emulation parser for ScratchBird. The PostgreSQL
parser is a standalone front-end that parses PostgreSQL SQL and emits SBLR V3.

## Baseline

- Compatibility target: PostgreSQL 16+ behavior
- Schema root: `/remote/emulation/postgresql/<server>/<database>/`
- Parser MUST NOT share grammar or code with the ScratchBird parser.

## Core Rules

1. The parser MUST emit SBLR V3 only; the engine never parses SQL.
2. Tablespace DDL and TABLESPACE clauses are rejected in PostgreSQL emulation.
3. Unsupported PostgreSQL features MUST be rejected with SQLSTATE `0A000`
   unless a different code is explicitly required by emulation rules.
4. Reserved keywords must be double-quoted when used as identifiers.

## Emission Rules

- All statement-to-SBLR rules are defined in:
  `docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md`
- Dialect gap handling and byte examples are defined in:
  `docs/specifications/parser/v3/findings/DIALECT_GAP_EXAMPLES.md`

## Catalog and System Schemas

The PostgreSQL parser MUST emulate:
- `pg_catalog`
- `information_schema`

Mappings are defined in:
- `docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md`
- `docs/specifications/parser/v3/operations/OID_MAPPING_STRATEGY.md`

## Required Statement Coverage

Minimum required statements in V3:
- DDL: CREATE/ALTER/DROP TABLE/INDEX/VIEW/SEQUENCE/DOMAIN/TYPE
- DML: SELECT/INSERT/UPDATE/DELETE/MERGE
- Utility: SHOW/DESCRIBE/SET/RESET/EXPLAIN
- Transactions: BEGIN/COMMIT/ROLLBACK/SAVEPOINT

## Error Mapping

- Feature not supported: SQLSTATE `0A000`
- Syntax error: SQLSTATE `42601`
- Invalid parameter: SQLSTATE `22023`

## Related Specs

- `docs/specifications/parser/v3/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/v3/parser/MYSQL_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/v3/wire_protocols/POSTGRESQL_EMULATION_BEHAVIOR.md`
