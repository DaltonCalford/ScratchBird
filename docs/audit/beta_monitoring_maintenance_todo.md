# TODO: Beta Monitoring & Maintenance (Background Workers, Stats, Diagnostics)

Goal: Define Beta-stage monitoring/maintenance best practices with minimal operational impact, covering all supported index types (btree, hash, gist, gin, rtree/spatial, bitmap, columnstore, lsm/fulltext if applicable), storage, and resource usage. Provide control surfaces to enable/disable or throttle components.

## Metrics/Stats (queryable by trusted cluster members)
- Table/index statistics: row counts, dead tuples, page counts, bloat estimates, fragmentation, fillfactor, per-index health (fanout, depth, splits), vacuum/sweep/autoanalyze timestamps.
- Index-type specifics:
  - B-tree: height, leaf density, split/merge rates, rightmost growth.
  - Hash: bucket count/load, overflow pages.
  - GiST/GiN: tree depth, pending list size (GiN), dedup/cleanup stats.
  - R-tree/spatial: node utilization, splits, bounding box stats.
  - Bitmap: dictionary size, roaring container stats.
  - Columnstore: segment counts, encoding ratios, predicate pushdown hit rates.
  - Fulltext/LSM (if present): memtable/flush/compaction stats, segment counts.
- Buffer/cache: hit/miss per tablespace/object; pinned pages; dirty/clean ratios.
- IO/disk: per-tablespace IO ops/latency/throughput; free/used space; temp usage.
- Memory: per-background-worker usage; allocator stats; spill events.
- Background tasks: sweep/vacuum runs, index analyze/rebalance runs, duration, rows/pages processed.
- Locks/contention: lock waits, deadlock detections (if available).
- Config state: which monitors/workers are enabled, sampling intervals, thresholds.

## Background Workers (child threads)
- Sweep/vacuum: clean dead versions, reclaim space; scheduled/threshold-based; per-table/index targeting.
- Analyze/stats: collect/update stats; adaptive scheduling based on changes.
- Index maintenance: rebalance/defrag where supported (btree/rtree/gist/gin/hash/bitmap/columnstore as applicable).
- Resource monitors: memory/disk/IO watchdog; temp space monitor.
- Optional: replication/logging hooks (for WAL-based ETL/replication when added in Beta).

## Controls/Configurability
- Enable/disable per component (sweep, analyze, index maintenance, resource monitors).
- Sampling intervals, work limits (pages/rows per run), pause/resume.
- Thresholds to trigger work (dead tuples %, bloat %, depth growth, pending list size, disk free %).
- Priority/niceness to reduce impact; backoff under load.
- Expose state via catalog views/functions to trusted cluster members; RBAC enforcement.

## Access & Security
- Stats/diagnostics visible only to trusted roles/cluster members.
- No sensitive data leakage in metrics/logs; avoid value sampling unless explicitly enabled.

## Diagnostics/Logging
- Structured logs for maintenance runs (start/stop/duration/work done/reason).
- Error reporting and counters for failed runs; retry/backoff strategy.
- Optional verbose mode per component for troubleshooting.

## Deliverables
- Spec for metrics schema (catalog views/functions) and background worker behavior.
- Config options (ini/env) for enabling/disabling and tuning each component.
- Tests: verify collection, control toggles, access control, and throttling under load.
