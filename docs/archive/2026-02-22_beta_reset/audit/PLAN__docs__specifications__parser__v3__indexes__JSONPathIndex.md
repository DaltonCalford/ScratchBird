# Implementation Plan: JSONPathIndex.md

**Spec Path:** `docs/specifications/parser/v3/indexes/JSONPathIndex.md`

**Category:** indexes

## Scope Summary
- Implement index algorithms, storage integration, and MGA visibility rules.

## Dependencies
- `docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md`
- `docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md`
- `docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md`

## Implementation Steps (Detailed)
- Define JSONPath key extraction and encoding
- Define index build and query evaluation
- Define storage layout and page type usage
- Define update/delete handling and GC
- Define concurrency rules

## Manual Gap Analysis (Missing/Unclear Details)
- No explicit JSONPath extraction algorithm
- No concurrency rules or update semantics
- No storage encoding details

## Verification
- Index correctness and MVCC visibility tests.
- Index GC/cleanup tests.
