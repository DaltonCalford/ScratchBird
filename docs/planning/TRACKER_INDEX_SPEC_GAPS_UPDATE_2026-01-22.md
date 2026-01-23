# Index Spec Gap Tracker - Remediation Update (2026-01-22)

This file contains the remediation updates intended for `docs/planning/TRACKER_INDEX_SPEC_GAPS.md`, written separately due to current write permissions on that file.

## Advanced Indexes (Not Implemented)

- [x] **Inverted Index:** core structures + DML + search implemented per spec (`docs/specifications/indexes/InvertedIndex.md`).
  - [ ] Implement GC purge via `InvertedIndex::removeDeadEntries` (see remediation section below).

## Remediation Items (Alpha)

- [ ] **FULLTEXT GC stub:** implement `InvertedIndex::removeDeadEntries` (`src/core/inverted_index.cpp:3795`).
- [ ] **Index GC wiring:** extend `src/core/garbage_collector.cpp:870-946` to open/sweep GIST, SPGIST, RTREE, BITMAP, COLUMNSTORE, FULLTEXT, LSM.
- [ ] **GiST cache cleanup:** remove the temporary leak in `src/sblr/index_cache.cpp:270-290` by enabling GiST deletion.
- [ ] **V2 FULLTEXT parser gap:** add `USING FULLTEXT` and optional `USING INVERTED` alias (see `docs/specifications/V2_PARSER_INDEX_TYPE_COMPLETENESS.md`).
