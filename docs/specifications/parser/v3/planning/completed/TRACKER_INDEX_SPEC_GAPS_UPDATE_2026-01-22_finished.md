# Index Spec Gap Tracker - Remediation Update (2026-01-22)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


This file contains the remediation updates intended for `docs/planning/TRACKER_INDEX_SPEC_GAPS.md`, written separately due to current write permissions on that file.

## Advanced Indexes (Not Implemented)

- [x] **Inverted Index:** core structures + DML + search implemented per spec (`/docs/specifications/parser/v3/indexes/InvertedIndex.md`).
  - [x] Implement GC purge via `InvertedIndex::removeDeadEntries` (see remediation section below).

## Remediation Items (Alpha)

- [x] **FULLTEXT GC stub:** implement `InvertedIndex::removeDeadEntries` (`src/core/inverted_index.cpp:3795`).
- [x] **Index GC wiring:** extend `src/core/garbage_collector.cpp:870-946` to open/sweep GIST, SPGIST, RTREE, BITMAP, COLUMNSTORE, FULLTEXT, LSM.
- [x] **GiST cache cleanup:** remove the temporary leak in `src/sblr/index_cache.cpp:270-290` by enabling GiST deletion.
- [x] **V2 FULLTEXT parser gap:** add `USING FULLTEXT` and optional `USING INVERTED` alias (see `/docs/specifications/parser/v3/V2_PARSER_INDEX_TYPE_COMPLETENESS.md`).
