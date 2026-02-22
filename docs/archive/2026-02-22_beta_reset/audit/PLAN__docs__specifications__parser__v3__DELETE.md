# Implementation Plan: DELETE.md

**Spec Path:** `docs/specifications/parser/v3/DELETE.md`

**Category:** dml

## Scope Summary
- Implement DELETE parsing, SBLR emission, and executor semantics.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`
- `docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`

## Implementation Steps (Detailed)
- Define authoritative DELETE grammar for ScratchBird + dialect variants (USING, RETURNING, ONLY, WHERE CURRENT OF)
- Define DELETE AST schema (target, alias, using joins, where, returning, triggers)
- Map DELETE to SBLR opcodes and payload schemas (row source, qualifiers, returning)
- Define executor semantics for delete: row visibility, tombstone creation, index maintenance
- Define lock ordering for DELETE (table/index/row/foreign key locks)
- Define FK constraint handling and ON DELETE actions
- Define trigger firing order (BEFORE/AFTER, row/statement) and transition tables
- Define error codes/SQLSTATE for delete conflicts or constraint violations
- Define returning clause evaluation and result set behavior

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 grammar, AST, or SBLR mapping
- No executor semantics for visibility, GC tombstones, or index maintenance
- No lock ordering rules for DELETE
- No FK constraint enforcement details or trigger ordering
- WHERE CURRENT OF and ONLY are not specified

## Verification
- Parser tests for DELETE variants and dialect modifiers.
- MVCC visibility tests for delete conflicts.
- FK/trigger and RETURNING tests.
