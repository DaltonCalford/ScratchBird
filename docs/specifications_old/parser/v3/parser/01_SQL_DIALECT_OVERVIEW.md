# SQL Dialect Overview (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the ScratchBird dialect and its compatibility baselines. This document
is an overview; detailed syntax and semantics are defined in the V3 parser,
DDL/DML/PSQL, SBLR, and executor specifications.

## Core Principles

- Parsers are dialect-specific and MUST NOT share SQL grammars.
- The engine MUST NOT parse SQL. Parsers emit SBLR; the engine executes SBLR only.
- Each dialect maps to ScratchBird core semantics with explicit emission rules.

## Compatibility Targets (Normative)

- ScratchBird native dialect (authoritative grammar)
- PostgreSQL 16+ behavior (as specified in V3 emulation specs)
- MySQL 8.x behavior (as specified in V3 emulation specs)
- Firebird 5.x behavior (as specified in V3 emulation specs)
- TDS/MSSQL is not supported and MUST be rejected

## Statement Categories (High-Level)

- DDL: CREATE/ALTER/DROP for database, schema, table, index, view, sequence, domain, trigger, function, procedure
- DML: SELECT/INSERT/UPDATE/DELETE/MERGE/COPY
- DCL: GRANT/REVOKE
- TCL: BEGIN/COMMIT/ROLLBACK/SAVEPOINT
- PSQL: stored procedures, functions, triggers, control flow
- Utility: SET/SHOW/EXPLAIN/DESCRIBE

## Compilation Pipeline (Normative)

1. Parse SQL to AST (dialect-specific parser).
2. Validate and type-check AST.
3. Emit SBLR V3 bytecode (deterministic and canonical).
4. Execute via the SBLR executor (no SQL parsing in engine).

## Related Specifications

- `docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_CORE_LANGUAGE.md`
- `docs/specifications/parser/v3/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/v3/parser/MYSQL_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/v3/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
