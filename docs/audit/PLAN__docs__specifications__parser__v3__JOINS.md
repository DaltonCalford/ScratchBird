# Implementation Plan: JOINS.md

**Spec Path:** `docs/specifications/parser/v3/JOINS.md`

**Category:** query

## Scope Summary
- Implement join parsing, SBLR emission, and executor semantics.

## Dependencies
- `docs/specifications/parser/v3/SELECT_AND_QUERY.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`

## Implementation Steps (Detailed)
- Define authoritative join grammar for all supported join types and modifiers
- Define join AST schema including join type, join condition, lateral, and aliasing
- Map join constructs to SBLR opcodes and payload schemas
- Define executor join semantics (inner/outer, null extension, join filters)
- Define join order rules and optimizer hints
- Define error rules for invalid join conditions
- Define USING/NATURAL expansion and column resolution rules
- Define lateral and function table reference semantics
- Define row locking interaction for SELECT FOR UPDATE with joins

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 grammar, AST, or SBLR mapping
- No executor semantics for join processing or null extension rules
- No column resolution rules for NATURAL/USING joins
- No optimizer or join order semantics
- No locking behavior defined for joined queries

## Verification
- Parser tests for all join forms and aliasing.
- Execution tests for inner/outer join semantics.
- NATURAL/USING column resolution tests.
