# MGA Index Path Audit (Insert/Update/Delete/Cleanup) – Snapshot

Scope: Assess per-index-type handling of MGA visibility (non-blocking readers/writers), back versions, and dead-entry cleanup. This is a high-level audit; detailed code verification still needed.

## Summary Table
| Index Type | Insert/Update Handling | Delete Handling | Scan Visibility | Cleanup Strategy | Risk/Action |
|------------|------------------------|-----------------|-----------------|------------------|-------------|
| B-tree | Likely standard insert with page splits; HOT-style reuse when non-indexed cols change is unclear. | Deletes leave dead entries until sweep; merge policy not confirmed. | Needs tuple visibility check per fetch; risk if relying on snapshot pruning. | Sweep/vacuum expected; not verified. | Verify HOT reuse rules, MGA visibility filters on scans, and sweep removing dead entries. |
| Hash | Bucket insert/overflow; update path unclear for non-key changes. | Dead tuples may linger in buckets/overflow. | Must recheck tuple visibility; risk of stale hits. | Cleanup not confirmed. | Review bucket cleanup and visibility filtering; add sweep coverage. |
| GiST | Tree insert/split; updates may reinsert. | Deletes should mark/remove entries; not confirmed. | Must recheck tuples after bounding-box match. | Pending recheck; no confirmed cleanup path. | Ensure recheck + visibility; verify delete/cleanup. |
| GiN | Pending list then merge; updates may add postings. | Deletions should drop postings; not confirmed. | Posting fetch must apply MGA visibility. | Cleanup of pending/postings not verified. | Audit posting removal and visibility filters; ensure cleanup worker runs. |
| R-tree/Spatial | Insert with splits; updates may reinsert. | Deletes should prune entries; not confirmed. | Needs recheck with visibility. | Cleanup not confirmed. | Verify delete/prune and recheck logic. |
| Bitmap | Sets bits on insert; updates may toggle bits. | Bits for dead tuples must be cleared; unclear. | Scans need visibility recheck to mask dead bits. | Cleanup not confirmed. | Audit bit cleanup and scan masking; tie to sweep. |
| Columnstore | Segment append on insert; updates likely rewrite segments. | Deletes may mark rowids; need purge/compaction. | Scans must visibility-check rowids/versions. | Compaction/purge path not confirmed. | Verify segment rewrite/cleanup and visibility filtering. |
| Fulltext/LSM (if present) | Memtable/LSM insert; updates may append new terms. | Deletes should tombstone/trim postings; unclear. | Queries must recheck visibility on postings. | Compaction/tombstone handling not verified. | Audit deletion/tombstone and visibility recheck; ensure compaction purges dead postings. |

## Observations
- No explicit MGA-specific safeguards were identified in code during this quick pass; rely on executor visibility checks and background sweep/vacuum. Need confirmation per index type.
- Dead-entry cleanup policies are largely unverified; risk of bloat/false hits if visibility filters are missing.
- Updates that do not change indexed columns should reuse existing index entries (HOT-style); unclear across index types.

## Actions (per index type)
- **All:** Confirm scans always apply MGA tuple visibility; add tests with concurrent update/delete + scan.  
- **B-tree:** Validate HOT/no-indexed-column change path; confirm split/merge under concurrent MGA; ensure sweep removes dead entries.  
- **Hash:** Audit overflow cleanup and visibility masking; ensure sweep handles buckets.  
- **GiST/R-tree:** Ensure delete/prune and recheck paths; verify split logic under MGA.  
- **GiN:** Confirm posting removal and pending-list cleanup; add visibility filters on posting fetch.  
- **Bitmap:** Ensure dead-row bits are cleared or masked; tie to sweep/cleanup.  
- **Columnstore:** Verify segment purge/compaction removes dead rowids and scans filter by visibility.  
- **Fulltext/LSM:** Confirm tombstones and compaction purge dead postings; enforce visibility checks in query path.  
