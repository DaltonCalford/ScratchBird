# Implementation Plan: UTILITY_COPY.md

**Spec Path:** `docs/specifications/parser/v3/UTILITY_COPY.md`

**Category:** utility

## Scope Summary
- Implement parsing, SBLR emission, and executor semantics (as applicable).

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`

## Implementation Steps (Detailed)
- Define authoritative COPY grammar for table and query forms
- Define AST schema and SBLR payload mapping for COPY
- Define executor semantics for COPY FROM/TO (streaming, batching, errors)
- Define options schema and validation rules
- Define lock ordering, constraint enforcement, and error handling
- Define COPY format encodings (CSV/TEXT/BINARY) and escaping rules

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only and references v2 parser; no V3 SBLR mapping
- No executor semantics for COPY streaming and error handling
- No formal format encoding rules or option validation schema
- No lock ordering or constraint enforcement rules

## Verification
- Parser tests for statement variants.
- Executor/runtime conformance tests.
