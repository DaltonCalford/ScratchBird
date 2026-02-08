# PSQL Statements (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the PSQL statement grammar and dispatch rules for ScratchBird. PSQL is
compiled to SBLR; the engine never parses SQL.

## Statement Categories

- Control flow: IF/ELSIF/ELSE, CASE, LOOP, WHILE, FOR
- Cursor control: DECLARE/OPEN/FETCH/CLOSE
- Exception handling: EXCEPTION, TRY/EXCEPT
- Assignment: `SET` / `:=`
- Calls: `CALL`, `EXECUTE`
- DML statements inside PSQL: SELECT/INSERT/UPDATE/DELETE

## Dispatch Rules

PSQL statement dispatch is deterministic:
1. Control flow statements (IF/CASE/LOOP/WHILE/FOR)
2. Cursor statements (DECLARE/OPEN/FETCH/CLOSE)
3. Exception handling (EXCEPTION/TRY)
4. Assignment or CALL
5. Embedded DML

## SBLR Emission

All PSQL statements emit opcodes and schemas defined in:
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`
- `docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md`

## Related Specs

- `docs/specifications/parser/v3/05_PSQL_PROCEDURAL_LANGUAGE.md`
- `docs/specifications/parser/v3/PSQL_RUNTIME_V3.md`
