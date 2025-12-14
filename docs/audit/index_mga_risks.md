# Index Implementations: MGA-Specific Risks/Design Checks (Snapshot)

Scope: MGA/Firebird-style visibility (non-blocking readers/writers, back versions). Potential logical/design risks in index handling.

## Common MGA Risks (all index types)
- **Version visibility:** Index entries may point to newer back versions; need visibility checks on fetch and HOT-like updates. Ensure executor filters tuples by MGA rules, not snapshot-style MVCC.
- **Index cleanup:** If vacuum/sweep is deferred, dead index entries can bloat; confirm background sweep cleans index entries for fully dead tuples.
- **HOT/back-version pointers:** Ensure in-page back-version chains don’t leave stale index pointers; rewrites should create new index entries when indexed columns change, reuse when not.
- **Concurrent page splits/updates:** Need MGA-safe split logic; readers should tolerate concurrent splits without locking conflicts.
- **Right-link/routing:** For append-heavy workloads, confirm right-link traversal is safe under MGA without latch deadlocks.

## B-tree
- Verify dedup/reuse of entries when only non-indexed columns change (HOT-style); otherwise extra entries may remain visible until sweep.
- Check page split/merge logic under concurrent MGA updates (no writer/reader blocking).
- Ensure index scans apply MGA visibility per tuple and ignore dead versions without relying on VACUUM.

## Hash
- Hash buckets must tolerate deleted tuples until sweep; check overflow bucket cleanup.
- Verify bucket splits don’t block readers; ensure consistent masking of dead entries.

## GiST / R-tree / Spatial
- Bounding boxes derived from stale versions can misroute searches; need recheck/filter with MGA visibility.
- Page splits must keep tree balanced without locking out readers; confirm concurrent insert/delete path correctness.

## GiN
- Pending list + cleanup: ensure cleanup processes remove postings for dead tuples; MGA visibility filters must be applied on posting fetch.
- Concurrent updates to posting lists should not assume snapshot pruning.

## Bitmap
- Dictionary/roaring containers must drop bits for dead tuples after sweep; confirm no snapshot-only pruning assumptions.
- Ensure compression doesn’t hide dead-bit removal requirements.

## Columnstore
- Segment-level metadata may reference rows that are dead; scans must check tuple visibility and handle back versions if stored.
- Rebuild/compaction needs MGA-aware filtering of dead rows.

## Fulltext/LSM (if present)
- Memtable flush/compaction must purge dead tuples; queries must visibility-filter postings.
- Segment merges should recheck visibility to avoid resurrecting deleted rows.

## Actions
- Audit each index type’s insert/update/delete path for MGA visibility handling and dead-entry cleanup strategy.
- Ensure sweeps/vacuum cover index cleanup; add tests for concurrent update/delete + scan across all index types.
- Add MGA-specific scan filters where missing; avoid reliance on snapshot-based pruning.  
