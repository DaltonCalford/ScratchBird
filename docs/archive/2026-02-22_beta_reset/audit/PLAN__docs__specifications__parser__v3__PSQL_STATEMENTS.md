# Implementation Plan: PSQL_STATEMENTS.md

**Spec Path:** `docs/specifications/parser/v3/PSQL_STATEMENTS.md`

**Category:** psql

## Scope Summary
- Implement PSQL statement parsing, SBLR emission, and executor semantics.

## Dependencies
- `docs/specifications/parser/v3/PSQL_RUNTIME_V3.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`

## Implementation Steps (Detailed)
- Define authoritative grammar for all PSQL statements and variants
- Define AST node schemas for each PSQL statement type
- Map PSQL statements to SBLR opcodes and payload schemas
- Define executor semantics for each PSQL statement (IF/CASE/LOOP, exceptions, cursors)
- Define scoping and symbol table construction rules
- Define error/SQLSTATE mapping for runtime and parse errors
- Define dynamic SQL execution rules and parameter binding
- Define interaction with transaction control and savepoints

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 grammar, AST, or SBLR mapping
- No executor semantics for PSQL statements or error handling
- No dynamic SQL parameter binding rules
- No formal mapping of cursor states to opcodes
- No scoping/symbol table rules beyond dispatch order

## Verification
- Parser tests for all PSQL statement forms.
- Runtime tests for control flow, cursors, and exceptions.
