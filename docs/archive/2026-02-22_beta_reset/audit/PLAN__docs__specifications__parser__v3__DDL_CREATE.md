# Implementation Plan: DDL_CREATE.md

**Spec Path:** `docs/specifications/parser/v3/DDL_CREATE.md`

**Category:** ddl

## Scope Summary
- Implement CREATE parsing, SBLR emission, and executor/catalog semantics for all supported objects.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`
- `docs/specifications/parser/v3/catalog/*`
- `docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md`

## Implementation Steps (Detailed)
- Define authoritative CREATE grammar for each object type and modifiers
- Define AST node schemas for every CREATE variant with explicit payload fields
- Map CREATE variants to SBLR opcodes and payload schemas
- Define executor semantics for catalog creation, storage allocation, and dependency tracking
- Define lock ordering and lock modes for each CREATE operation
- Define validation rules (naming, schema path, type constraints, dependency checks)
- Define error codes and SQLSTATE mappings per CREATE operation
- Define transactional behavior (rollback, visibility, DDL within transactions)
- Define cross‑dialect normalization (Postgres/MySQL/Firebird)

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser files; no V3 grammar, AST, or SBLR mapping
- No executor/catalog semantics for creating objects (catalog rows, storage pages, UUIDs)
- No lock ordering rules for DDL creates
- No explicit validation rules for modifiers (TEMP/UNLOGGED/MATERIALIZED/OR REPLACE)
- No error/SQLSTATE mapping for rejected or invalid CREATE paths

## Verification
- Parser tests for all CREATE variants and dialect modifiers.
- DDL transaction rollback tests.
- Catalog consistency checks after CREATE.
