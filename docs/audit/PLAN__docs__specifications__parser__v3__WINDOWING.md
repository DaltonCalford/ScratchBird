# Implementation Plan: WINDOWING.md

**Spec Path:** `docs/specifications/parser/v3/WINDOWING.md`

**Category:** query

## Scope Summary
- Implement parsing, SBLR emission, and executor semantics (as applicable).

## Dependencies
- `docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`

## Implementation Steps (Detailed)
- Define authoritative window function grammar and AST schema
- Map window functions and frames to SBLR opcodes and payloads
- Define executor semantics for window partitioning, ordering, and frames
- Define frame boundary evaluation rules and null handling
- Define error/SQLSTATE mapping for invalid window definitions

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 SBLR mapping
- No executor semantics for window evaluation
- No frame boundary evaluation rules or error mappings

## Verification
- Parser tests for statement variants.
- Executor/runtime conformance tests.
