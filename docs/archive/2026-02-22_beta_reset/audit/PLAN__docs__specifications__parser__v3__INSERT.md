# Implementation Plan: INSERT.md

**Spec Path:** `docs/specifications/parser/v3/INSERT.md`

**Category:** dml

## Scope Summary
- Implement INSERT parsing, SBLR emission, and executor semantics.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`
- `docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`

## Implementation Steps (Detailed)
- Define authoritative INSERT grammar including VALUES, SELECT, DEFAULT VALUES, ON CONFLICT, RETURNING
- Define INSERT AST schema including target, column list, source, conflict action, and returning
- Map INSERT variants to SBLR opcodes and payload schemas
- Define executor semantics for insert: default evaluation, identity/sequence handling, generated columns
- Define ON CONFLICT semantics: constraint inference, index selection, DO UPDATE rules
- Define lock ordering and modes for INSERT and conflict resolution
- Define constraint enforcement order and trigger firing order
- Define error/SQLSTATE mapping for conflicts, constraint violations, and type errors
- Define returning clause evaluation and result set rules

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 grammar, AST, or SBLR mapping
- No executor semantics for default/identity/sequence handling
- No ON CONFLICT conflict target resolution or index selection rules
- No lock ordering rules for insert + conflict update
- No error/SQLSTATE mapping or trigger/constraint ordering

## Verification
- Parser tests for INSERT variants and conflict clauses.
- Constraint/trigger tests for insert behavior.
- RETURNING tests.
