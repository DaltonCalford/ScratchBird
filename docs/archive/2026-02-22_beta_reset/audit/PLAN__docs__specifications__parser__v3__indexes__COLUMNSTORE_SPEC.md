# Implementation Plan: COLUMNSTORE_SPEC.md

**Spec Path:** `docs/specifications/parser/v3/indexes/COLUMNSTORE_SPEC.md`

**Category:** indexes

## Scope Summary
- Implement index algorithms, storage integration, and MGA visibility rules.

## Dependencies
- `docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md`
- `docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md`
- `docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md`

## Implementation Steps (Detailed)
- Define columnstore segment layout and encoding choices
- Define insert/update/delete path and row group management
- Define scan/aggregation semantics and predicate pushdown
- Define page type usage and TOAST integration
- Define GC and compaction rules

## Manual Gap Analysis (Missing/Unclear Details)
- No explicit column encoding format or segment metadata schema
- No update/delete handling and compaction policy details
- No lock ordering or concurrency rules

## Verification
- Index correctness and MVCC visibility tests.
- Index GC/cleanup tests.
