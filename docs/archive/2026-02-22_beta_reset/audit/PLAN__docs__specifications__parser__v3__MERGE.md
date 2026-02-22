# Implementation Plan: MERGE.md

**Spec Path:** `docs/specifications/parser/v3/MERGE.md`

**Category:** dml

## Scope Summary
- Implement MERGE parsing, SBLR emission, and executor semantics.

## Dependencies
- `docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`

## Implementation Steps (Detailed)
- Define authoritative MERGE grammar including all WHEN clause variants
- Define MERGE AST schema including target/source, match conditions, and actions
- Map MERGE to SBLR opcodes and payload schemas (matched/unmatched branches)
- Define executor semantics for MERGE (row matching, action ordering, determinism)
- Define lock ordering and modes for MERGE (target/source/index/row)
- Define constraint enforcement and trigger behavior per branch
- Define error/SQLSTATE mapping for conflicts and invalid MERGE clauses
- Define RETURNING semantics if supported

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 grammar, AST, or SBLR mapping
- No executor semantics for MERGE matching and action ordering
- No lock ordering rules for MERGE
- No constraint/trigger ordering or error mappings
- No definition of branch determinism when multiple WHEN clauses match

## Verification
- Parser tests for all MERGE forms.
- Branch determinism and constraint tests.
