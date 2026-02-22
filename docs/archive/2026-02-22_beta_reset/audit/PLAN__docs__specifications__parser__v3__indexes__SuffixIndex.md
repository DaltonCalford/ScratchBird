# Implementation Plan: SuffixIndex.md

**Spec Path:** `docs/specifications/parser/v3/indexes/SuffixIndex.md`

**Category:** indexes

## Scope Summary
- Implement index algorithms, storage integration, and MGA visibility rules.

## Dependencies
- `docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md`
- `docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md`
- `docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md`

## Implementation Steps (Detailed)
- Define suffix array/tree encoding and build algorithm
- Define insert/update behavior or immutability policy
- Define query semantics (prefix, substring search)
- Define storage layout and page types
- Define GC and rebuild policy

## Manual Gap Analysis (Missing/Unclear Details)
- No explicit encoding format
- No update policy (mutable vs rebuild) defined
- No concurrency rules

## Verification
- Index correctness and MVCC visibility tests.
- Index GC/cleanup tests.
