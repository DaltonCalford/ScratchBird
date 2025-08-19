ScratchBird Index Build and Maintenance (Phase H)

Scope: Parser-first acceptance and executor scaffolding for index lifecycle; design for offline bulk build and online concurrent build (backfill + WAL catch-up fence); REINDEX, VALIDATE, DROP, ALTER SET STATISTICS; catalog metadata; bootstrap/migration safety.

1) CREATE INDEX
- Offline bulk build: scan base table, sort keys, write B-Tree via builder; emit WAL for root updates; register stats.
- Online concurrent build: phases
  - Phase A: take snapshot T_s, register build task in catalog; no blocking readers.
  - Phase B: backfill from heap snapshot into build index, record high-water txn.
  - Phase C: catch-up: apply WAL delta (inserts/deletes/updates) since T_s; repeat until lag < threshold.
  - Phase D: fence: briefly block conflicting writers; drain remaining WAL; atomically swap index to ACTIVE.
- Partial indexes: evaluator applies predicate on candidate rows before insert.
- INCLUDE columns: payload appended to leaf records; used by planner for index-only scans.

2) REINDEX
- Validate current structure (walk leaves/branches, key order, sibling links, root metadata).
- If valid and method unchanged: optional compact in-place; else rebuild offline, then atomic swap.
- Preserve OID, name; version bump in catalog.

3) VALIDATE INDEX
- Non-blocking validation pass: sample pages + invariants; full pass optional under maintenance window.
- Emit diagnostics into `SDB$STATS` and `SDB$INDEX_VALIDATE_LOG` (future).

4) DROP INDEX
- Mark INACTIVE; ensure no active planners depend on it; unlink from catalog; schedule background purge of pages.

5) ALTER INDEX ... SET STATISTICS
- Update planner hints (target stats density) without rebuild; persisted in `SDB$INDEX` row.

6) Catalog Integration
- `SDB$INDEX` (oid, relation_oid, name, method, expr, columns, include_cols, predicate, tablespace, active, valid, created_at, version, stats_target).
- `SDB$STATS` keyed by (object_oid, kind) stores height, ndistinct, histogram, mcv, correlation, leaf/branch pages, key_count.
- Bootstrap: core system indexes inserted with fixed OIDs; migrations append columns conservatively.

7) Concurrency
- Readers never blocked: concurrent build uses fence only at final swap; long readers continue with old visibility.
- Writers during backfill generate WAL captured to delta queue; applied in catch-up.

8) Telemetry
- Progress rows in `sys.monitoring.index_builds` with phases, scanned rows, lag, ETA.

9) Wiring to WAL
- `WalManager` exposes global listeners. Online builds register a listener to capture `Insert/Delete` logical records and push them into the build's delta queue. Root updates are ignored for deltas. Listener is best-effort; final fence applies remaining deltas.

10) Validation (real)
- Validator walks B-Tree leaves in key order, verifies sorted invariant, and sibling `prev/next` links are consistent. Also ensures promoted separators in branches are >= max of left and < min of right. Diagnostics are exposed with the first offending page and condition.

Executor status (parser-first):
- CREATE/REINDEX/VALIDATE/DROP/ALTER SET STATISTICS accepted and return acknowledgement without side effects.
- ANALYZE/VACUUM update in-memory stats; planner uses them for EXPLAIN.
