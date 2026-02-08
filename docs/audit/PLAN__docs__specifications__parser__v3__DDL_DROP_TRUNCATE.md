# Implementation Plan: DDL_DROP_TRUNCATE.md

**Spec Path:** `docs/specifications/parser/v3/DDL_DROP_TRUNCATE.md`

**Category:** ddl

## Scope Summary
- Implement DROP/TRUNCATE parsing, SBLR emission, and executor/catalog semantics for all supported objects.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`
- `docs/specifications/parser/v3/catalog/*`
- `docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`

## Implementation Steps (Detailed)
- Define authoritative DROP/TRUNCATE grammar for each object type and modifiers
- Define AST node schemas for DROP/TRUNCATE actions
- Map each DROP/TRUNCATE variant to SBLR opcodes and payload schemas
- Define executor semantics for catalog deletion, dependency traversal, and storage cleanup
- Define CASCADE/RESTRICT rules and dependency ordering
- Define lock ordering and lock modes for DROP/TRUNCATE
- Define error codes and SQLSTATE mappings (IF EXISTS, missing objects, dependency violations)
- Define transactional behavior and rollback semantics for DDL drops/truncate

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser files; no V3 grammar, AST, or SBLR mapping
- No executor/catalog semantics for drop/truncate or storage reclamation
- No dependency traversal/cascade rules defined
- No lock ordering or DDL transactional rules
- No error/SQLSTATE mapping for dependency or privilege failures

## Verification
- Parser tests for all DROP/TRUNCATE variants and dialect modifiers.
- DDL transaction rollback tests.
- Catalog consistency checks after DROP/TRUNCATE.
