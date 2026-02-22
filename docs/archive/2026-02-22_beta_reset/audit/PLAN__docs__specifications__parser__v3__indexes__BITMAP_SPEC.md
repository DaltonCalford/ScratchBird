# Implementation Plan: BITMAP_SPEC.md

**Spec Path:** `docs/specifications/parser/v3/indexes/BITMAP_SPEC.md`

**Category:** indexes

## Scope Summary
- Implement index algorithms, storage integration, and MGA visibility rules.

## Dependencies
- `docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md`
- `docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md`
- `docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md`

## Implementation Steps (Detailed)
- Define bitmap index layout (logical fields) and bitmap compression
- Define insert/update/delete operations with MGA visibility
- Define scan semantics for predicates and bitmap combination
- Define page type usage and overflow handling
- Define GC cleanup for bitmap entries

## Manual Gap Analysis (Missing/Unclear Details)
- No detailed bitmap compression format or bitmap chunk layout
- No explicit concurrency/lock ordering rules
- No defined page split/merge behavior

## Verification
- Index correctness and MVCC visibility tests.
- Index GC/cleanup tests.
