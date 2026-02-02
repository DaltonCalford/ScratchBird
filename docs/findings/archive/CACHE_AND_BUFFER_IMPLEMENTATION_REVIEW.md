# Cache and Buffer Implementation Review (Alpha/Beta Prep)
Status: Superseded (implementation verified)
Last Updated: 2026-02-02

Note: All gaps called out here are closed. Track any remaining work in
`docs/planning/TRACKER_OUTSTANDING_MASTER.md`.


**Date:** 2026-01-20
**Scope:** Buffer pool + storage caches + parser/query caches + monitoring
**Source of truth:** Codebase (headers/specs may be stale)

---

## 1) Executive Summary

ScratchBird has a working core buffer pool with clock-sweep eviction, partitioned page table, and a background writer. Several higher-level caches (statement/result/query result/index caches) exist in code but are **not wired** into the execution path. Monitoring scaffolding exists (metrics registry, IO stats, query profiler), but buffer/cache counters are not consistently fed or exposed to SQL views.

Key gaps vs. proven models (PostgreSQL / SQL Server / Oracle / InnoDB):

- **Scan-resistance and multi-pool design** are not implemented (spec calls for ring buffers, adaptive hash, read-ahead, multiple pools).
- **Plan/statement cache integration** is absent (statement cache exists but connection pool TODOs and no parser use).
- **Query result cache** exists but is only invalidated; no read/write path uses it.
- **Monitoring** lacks end-to-end wiring for cache hit/miss, eviction and writeback counters.

---

## 2) Current Implementation Inventory (Code Truth)

### 2.1 Buffer Pool (Core)

**Implemented:**
- Partitioned page table with per-partition locks; cache hit fast path. (`src/core/buffer_pool.cpp:145-219`)
- Clock-sweep eviction with LRU fallback and dirty-page preference. (`src/core/buffer_pool.cpp:680-824`)
- Background writer with adaptive dirty-ratio thresholds (low/high/checkpoint). (`src/core/buffer_pool.cpp:1000-1202`)
- Dirty page counter (O(1) dirty ratio). (`src/core/buffer_pool.cpp:1204-1224`)
- Per-page pin/unpin, flush, and GPID support. (`src/core/buffer_pool.cpp:106-520`)

**Config/Stats:**
- Config includes background writer settings and dirty ratio thresholds. (`include/scratchbird/core/buffer_pool.h:13-60`)
- Stats snapshot includes hits/misses/evictions/flushes + bgwriter metrics. (`include/scratchbird/core/buffer_pool.h:240-291`)

**Spec mismatch:**
- Storage buffer pool spec still states “simple 32-page LRU, no bgwriter” and lists missing features that are partially present now. (`docs/specifications/storage/STORAGE_ENGINE_BUFFER_POOL.md:6-16`)

### 2.2 LSM Block Cache

- LRU cache for SSTable blocks, mutex-protected, size-bounded. (`src/core/lsm_block_cache.cpp:1-200`)
- No sharding/admission control/compression or read-ahead integration.

### 2.3 Statement Cache (Parser/Pool)

- Statement cache supports LRU/LFU/ARC/FIFO, TTL, fingerprinting, table/schema invalidation. (`src/pool/statement_cache.cpp:1-940`)
- **Not wired** into connection pool or parser/executor.
  - Connection pool cache init is TODO. (`src/pool/connection_pool.cpp:310-312`)
  - Cache clearing / invalidation are TODO. (`src/pool/connection_pool.cpp:608-617`)
- No call sites outside pool module (no runtime use found).

### 2.4 Result Cache (Pool)

- Result cache supports LRU/LFU/SIZE/TTL eviction, TTL, invalidation tracking, memory limits. (`src/pool/result_cache.cpp:1-1200`)
- **Not wired** into connection pool or query execution path (connection pool TODOs, no usage in executor).

### 2.5 Query Result Cache (SBLR)

- SHA-256 hash keys, LRU by entry + memory cap, per-table invalidation. (`src/sblr/query_result_cache.cpp:115-220`)
- **Only invalidated**, not used for reads/writes by executor or compiler.
  - Invalidations on DDL/DML in executor. (`src/sblr/executor.cpp:7285-7287`)
  - Compiler “compileWithCache” computes hash but does not read cache. (`src/sblr/query_compiler_v2.cpp:41-46`)

### 2.6 Index Cache (SBLR)

- LRU cache of index instances keyed by UUID. (`src/sblr/index_cache.cpp:50-210`)
- **No call sites** outside the cache implementation (not in executor or catalog).

### 2.7 Catalog/Metadata Caches

- Extensive object caches in catalog manager (tables, columns, triggers, constraints, etc.). (multiple caches in `src/core/catalog_manager.cpp`)
- These are used directly but are not tied to a unified cache policy or monitoring surface.

### 2.8 Monitoring/Telemetry

- Metrics registry defines buffer pool counters and gauges. (`src/core/telemetry.cpp:496-521`)
- **No code paths update these metrics** from `BufferPool` (no increment call sites found).
- Query profiler records I/O and buffer hits/misses, but no usage sites found for `addBufferHit/addBufferMiss`. (`src/optimizer/query_profiler.cpp:71-88`)
- ConnectionContext tracks page read/write/fetch/mark per connection/transaction/statement. (`src/core/connection_context.cpp:1257-1287`)

---

## 3) Comparison to Proven Models

### 3.1 Buffer Pool / Page Cache

**What ScratchBird already aligns with:**
- **Clock-sweep eviction** (PostgreSQL-like), with dirty-page preference.
- **Background writer with dirty ratio thresholds** (PostgreSQL/InnoDB-style adaptive flushing).
- **Partitioned page table** to reduce contention (InnoDB-like sharded hash).

**Missing vs. common high-performance models:**
- **Scan-resistance / ring buffers** (PostgreSQL): dedicated ring buffers for sequential scans, vacuum/GC, and bulk writes to avoid polluting the main cache.
- **Young/old LRU split** (InnoDB): midpoint insertion to avoid scan thrash.
- **Multiple buffer pools** for workload types (hot/cold, OLTP/OLAP, temp).
- **Read-ahead orchestration** (SQL Server/InnoDB) for large scans.
- **Adaptive hash index** in buffer pool (InnoDB feature), if desired.

### 3.2 Statement / Plan Cache

**Best practice patterns:**
- Plan cache keyed by normalized SQL + parameter types, with schema versioning and invalidation.
- Per-database and per-session policies with LRU aging.
- Prepared statement reuse for client sessions, with TTL or size caps.

**ScratchBird state:**
- Statement cache exists with eviction policies + fingerprinting.
- **No integration** with parser or execution pipeline.
- Schema-change invalidation is declared but not wired to DDL execution or dependency tracking.

### 3.3 Query Result Cache

**Best practice patterns:**
- Result cache only for deterministic queries, with invalidation by table version and snapshot.
- Must respect transaction isolation and user permissions.
- TTL and size caps.

**ScratchBird state:**
- Query result cache exists (SBLR) and includes invalidation hooks.
- **No read/write path uses it** (only invalidation on DDL/DML).

### 3.4 LSM/Block Cache

**Best practice patterns (RocksDB/LevelDB class):**
- Sharded LRU or CLOCK for concurrency.
- Admission control (e.g., 2Q, TinyLFU) to avoid caching one-time reads.
- Optional compression in cache.
- Read-ahead for range scans.

**ScratchBird state:**
- Basic LRU + size bound, single mutex; no admission control or prefetch.

### 3.5 Monitoring / Performance Instrumentation

**Best practice patterns:**
- Buffer pool hit ratio, dirty ratio, eviction rate, flush rate, background writer activity.
- Plan cache hits/misses, evictions, invalidations, memory usage.
- Per-query I/O stats and cache hits/misses.

**ScratchBird state:**
- Metrics are defined but **not wired** to live counters.
- Query profiler has I/O fields but no evident updates for buffer hit/miss.

---

## 4) Recommended Beta Work (Performance + Monitoring)

### 4.1 Buffer Pool Enhancements

1. Add **scan-resistant ring buffers** (PostgreSQL-style) for sequential scans and maintenance passes.
2. Introduce **young/old LRU split** or similar midpoint insertion to avoid cache pollution.
3. Add **read-ahead** policy for large scans (size-based + sequential detection).
4. Partition buffer pools by workload (OLTP/OLAP/temp) or tablespace tier.

### 4.2 Parser/Plan Cache Integration

1. Wire `StatementCacheManager` into the parser/executor pipeline with SQL→SBLR cache and dependency tracking.
2. Bind cache entries to **schema version IDs** so DDL invalidation is exact and fast.
3. Ensure **per-session cache** for prepared statements plus a shared global cache gated by permissions.

### 4.3 Result Cache Integration

1. Define a **safe cache eligibility policy** (deterministic queries only).
2. Bind cache entries to **transaction snapshot or commit version**.
3. Use table-version invalidation and per-user privilege checks.

### 4.4 Monitoring & Observability

1. Connect `BufferPool` stats to Prometheus counters/gauges in `ScratchBirdMetrics`.
2. Expose cache stats via SQL monitoring views (hit/miss/eviction/size).
3. Track per-query cache usage: buffer hits/misses, cache wait time, eviction attribution.

---

## 5) Spec Drift and Documentation Updates

- `docs/specifications/storage/STORAGE_ENGINE_BUFFER_POOL.md` is outdated relative to the code (it claims no bgwriter, only LRU). Update to reflect the current clock-sweep and background writer implementation and mark missing features as TODO rather than “not implemented.”
- Network/parser cache specs reference statement/result/translation caches, but the runtime does not wire these yet. Ensure those specs are tagged “pending integration” so feature parity checks don’t misreport progress.

---

## 6) Reference Touchpoints

**Core Buffer Pool**
- `src/core/buffer_pool.cpp:145-219` (page table partitioning, hit path)
- `src/core/buffer_pool.cpp:680-824` (clock-sweep eviction)
- `src/core/buffer_pool.cpp:1000-1202` (adaptive background writer)
- `include/scratchbird/core/buffer_pool.h:240-291` (stats snapshot)

**Statement/Result Cache**
- `src/pool/statement_cache.cpp:1-940` (statement cache implementation)
- `src/pool/result_cache.cpp:1-1200` (result cache implementation)
- `src/pool/connection_pool.cpp:310-312` (cache init TODO)
- `src/pool/connection_pool.cpp:608-617` (cache invalidation TODO)

**SBLR Query Result Cache**
- `src/sblr/query_result_cache.cpp:115-220` (LRU + memory cap)
- `src/sblr/query_compiler_v2.cpp:41-46` (cache hash but no lookup)
- `src/sblr/executor.cpp:7285-7287` (cache invalidation)

**LSM Block Cache**
- `src/core/lsm_block_cache.cpp:1-200`

**Monitoring/Telemetry**
- `src/core/telemetry.cpp:496-521` (buffer pool metrics definitions)
- `src/optimizer/query_profiler.cpp:71-88` (buffer hit/miss stats in profiler)
- `src/core/connection_context.cpp:1257-1287` (page I/O counters)

