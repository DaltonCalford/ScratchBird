# ScratchBird SQL Language Specification (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the ScratchBird native SQL language surface with authoritative pointers
for syntax and semantics.

## Canonical Grammar

The authoritative grammar is defined in:
- `docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `docs/specifications/parser/v3/parser/05_PSQL_PROCEDURAL_LANGUAGE.md`

## Statement Families

- DDL: CREATE/ALTER/DROP/TRUNCATE/COMMENT
- DML: SELECT/INSERT/UPDATE/DELETE/MERGE/COPY
- DCL: GRANT/REVOKE
- TCL: BEGIN/COMMIT/ROLLBACK/SAVEPOINT
- PSQL: procedures, functions, triggers, execute block
- Utility: SET/RESET/SHOW/DESCRIBE/EXPLAIN

## Execution Model

1. Parse to AST (ScratchBird parser only).
2. Validate and type-check.
3. Emit SBLR V3 bytecode (canonicalized).
4. Execute by SBLR executor.

## Related Specs

- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md`
