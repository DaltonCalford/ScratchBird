# MySQL Parser Specification (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the MySQL 8.x emulation parser for ScratchBird. The MySQL parser is a
standalone front-end that parses MySQL SQL and emits SBLR V3 bytecode.

## Baseline

- Compatibility target: MySQL 8.x behavior (SQL + wire semantics)
- Schema root: `/remote/emulation/mysql/<server>/<database>/`
- Parser MUST NOT share grammar or code with the ScratchBird parser.

## Core Rules

1. The parser MUST emit SBLR V3 only; the engine never parses SQL.
2. Tablespace DDL and TABLESPACE clauses are rejected in MySQL emulation.
3. Unsupported MySQL features MUST be rejected with MySQL-compatible errors
   (SQLSTATE `0A000` unless overridden by emulation rules).
4. Reserved words follow MySQL 8.x semantics; reserved keywords must be
   backtick-quoted when used as identifiers.

## Emission Rules

- All statement-to-SBLR rules are defined in:
  `docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md`
- Dialect gap handling and byte examples are defined in:
  `docs/specifications/parser/v3/findings/DIALECT_GAP_EXAMPLES.md`

## Catalog and System Schemas

The MySQL parser MUST emulate:
- `INFORMATION_SCHEMA`
- `mysql.*`
- `performance_schema`

Mappings are defined in:
- `docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md`

## Required Statement Coverage

Minimum required statements in V3:
- DDL: CREATE/ALTER/DROP TABLE/INDEX/VIEW/SEQUENCE/DOMAIN/TYPE
- DML: SELECT/INSERT/UPDATE/DELETE/MERGE
- Utility: SHOW/DESCRIBE/SET/RESET/EXPLAIN
- Transactions: BEGIN/COMMIT/ROLLBACK/SAVEPOINT

## Error Mapping

- Feature not supported: SQLSTATE `0A000`
- Syntax error: SQLSTATE `42000`
- Invalid parameter: SQLSTATE `22023`

## Related Specs

- `docs/specifications/parser/v3/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/v3/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/v3/wire_protocols/MYSQL_EMULATION_BEHAVIOR.md`
