# Implementation Plan: DDL_ALTER.md

**Spec Path:** `docs/specifications/parser/v3/DDL_ALTER.md`

**Category:** ddl

## Scope Summary
- Implement ALTER parsing, SBLR emission, and executor/catalog semantics for all supported objects.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`
- `docs/specifications/parser/v3/catalog/*`
- `docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`

## Implementation Steps (Detailed)
- Define authoritative ALTER grammar for each object type (table, schema, database, tablespace, type, domain, index, etc.)
- Define AST node schemas for every ALTER action with explicit payload fields
- Map each ALTER action to SBLR opcodes and payload schemas
- Define executor semantics for each ALTER action (catalog updates, storage changes, validation)
- Define lock ordering and lock modes for each ALTER action (table/index/schema/catalog)
- Define constraint validation rules and deferred constraint behavior during ALTER
- Define error codes and SQLSTATE mappings per ALTER action
- Define transactional behavior and rollback semantics for DDL
- Define cross‑dialect parsing rules and normalization for PostgreSQL/MySQL/Firebird variants

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser files; no V3 grammar, AST, or SBLR mapping
- No executor/catalog semantics for ALTER actions (what changes, how it persists)
- No lock ordering rules per ALTER action (critical for DDL correctness)
- No error/SQLSTATE mapping for invalid ALTER operations
- No validation rules for ALTER TABLE subcommands (e.g., SET DATA TYPE compatibility, constraint validation)

## Verification
- Parser tests for all ALTER actions and dialect variants.
- DDL transaction rollback tests.
- Catalog consistency checks after ALTER.
