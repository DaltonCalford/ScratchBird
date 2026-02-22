# Implementation Plan: InvertedIndex.md

**Spec Path:** `docs/specifications/parser/v3/indexes/InvertedIndex.md`

**Category:** indexes

## Scope Summary
- Implement index algorithms, storage integration, and MGA visibility rules.

## Dependencies
- `docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md`
- `docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md`
- `docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md`

## Implementation Steps (Detailed)
- Define term dictionary and posting list encoding
- Define index build and update algorithms
- Define query processing and ranking
- Define storage layout and page type usage
- Define GC/merge policies and concurrency

## Manual Gap Analysis (Missing/Unclear Details)
- No explicit term dictionary encoding format
- No defined scoring/ranking formula
- No concurrency/lock ordering rules

## Verification
- Index correctness and MVCC visibility tests.
- Index GC/cleanup tests.
