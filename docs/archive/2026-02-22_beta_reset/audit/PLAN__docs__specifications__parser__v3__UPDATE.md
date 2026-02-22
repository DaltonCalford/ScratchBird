# Implementation Plan: UPDATE.md

**Spec Path:** `docs/specifications/parser/v3/UPDATE.md`

**Category:** dml

## Scope Summary
- Implement parsing, SBLR emission, and executor semantics.

## Dependencies
- `docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`

## Implementation Steps (Detailed)
- Define authoritative UPDATE grammar including FROM/RETURNING
- Define UPDATE AST schema and SBLR mapping
- Define executor semantics for update, visibility, and index maintenance
- Define lock ordering and modes for UPDATE
- Define constraint/trigger order and error/SQLSTATE mapping
- Define WHERE CURRENT OF support or rejection rules

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 SBLR mapping
- No executor semantics for update, index maintenance, or triggers
- No lock ordering rules for UPDATE
- WHERE CURRENT OF and ONLY are not specified

## Verification
- Parser tests for statement variants.
- Executor/runtime conformance tests.
