# Cache and Buffer Architecture Specification

**Status:** Alpha specification (current state documented; target state defined)
**Last Updated:** 2026-02-02
**Scope:** Buffer pool, storage caches, statement/plan caches, query result caches, and cache observability

---

## 1) Purpose

Define a single, authoritative cache/buffer architecture for ScratchBird that:

- Captures **current implementation behavior** (Alpha) for verification.
- Defines the **target architecture** (Alpha) using proven, well-tested patterns.
- Establishes **invalidation, safety, and monitoring requirements** for every cache tier.

This specification is the canonical reference for cache correctness, performance tuning, and monitoring.

---

## 2) Scope

This document covers:

- **Buffer pool** (page cache)
- **Storage-level caches** (LSM block cache)
- **Parser/executor caches** (statement/plan cache, query result cache)
- **Catalog/metadata caches**
- **Monitoring and observability**

Out of scope:

- Cluster-global cache coherence (covered in cluster specs)
- Application driver caches (covered in driver specs)

---

## 3) Terminology

- **Buffer Pool**: In-memory page cache for database pages.
- **Statement Cache**: Prepared/compiled statements keyed by normalized SQL.
- **Plan Cache**: Execution plans derived from statements, keyed by SQL + parameter types.
- **Result Cache**: Cached query results (rows/columns) for deterministic queries.
- **Query Result Cache (SBLR)**: Engine-level cache keyed by SBLR bytecode or SQL hash.
- **Translation Cache**: Parser/adapter cache for dialect -> SBLR translation.
- **Granular Invalidation**: Cache invalidation by object ID or version rather than global flush.

---

## 4) Architecture Overview

```
+------------------------------+-------------------------------+
| Parser/Protocol Layer        | Engine Layer                  |
+------------------------------+-------------------------------+
| Translation cache            | Statement/Plan cache          |
| Prepared statement cache     | Query result cache (SBLR)     |
+------------------------------+-------------------------------+
| Catalog/Metadata caches      | Buffer pool (page cache)      |
|                              | LSM block cache (if LSM)       |
+------------------------------+-------------------------------+
| OS page cache (optional)     | Disk                           |
+------------------------------+-------------------------------+
```

**Key rule:** all caches are **node-local**; for cluster consistency, see distributed query specs.

---

## 5) Current Implementation (Alpha, Code-Truth)

### 5.1 Buffer Pool

**Implemented:**
- Partitioned page table with per-partition locks (cache hit fast path).
- Clock-sweep eviction with dirty-page preference and LRU fallback.
- Adaptive background writer (low/high/checkpoint dirty thresholds).
- GPID-based page addressing and dirty page counter.

**Code references:**
- `src/core/buffer_pool.cpp:145-219` (partitioned lookup + hit path)
- `src/core/buffer_pool.cpp:680-824` (clock-sweep eviction)
- `src/core/buffer_pool.cpp:1000-1202` (adaptive background writer)
- `include/scratchbird/core/buffer_pool.h:240-291` (stats snapshot)

### 5.2 LSM Block Cache

- LRU cache for SSTable blocks with size bound and single mutex.
- No admission control or sharding.

**Code references:**
- `src/core/lsm_block_cache.cpp:1-200`

### 5.3 Statement Cache (Pool)

- Statement cache supports LRU/LFU/ARC/FIFO, TTL, fingerprinting, table/schema invalidation.
- **Not wired** into parser/executor or connection pool.

**Code references:**
- `src/pool/statement_cache.cpp:1-940`
- `src/pool/connection_pool.cpp:310-312` (cache init TODO)
- `src/pool/connection_pool.cpp:608-617` (clear/invalidation TODO)

### 5.4 Result Cache (Pool)

- Result cache supports eviction policies, TTL, memory bounds.
- **Not wired** into query execution path.

**Code references:**
- `src/pool/result_cache.cpp:1-1200`

### 5.5 Query Result Cache (SBLR)

- LRU + memory cap keyed by SQL or bytecode hash.
- **Invalidation only** (DDL/DML triggers). No read/write usage in executor.

**Code references:**
- `src/sblr/query_result_cache.cpp:115-220`
- `src/sblr/query_compiler_v2.cpp:41-46` (hash computed, no lookup)
- `src/sblr/executor.cpp:7285-7287` (invalidation)

### 5.6 Index Cache (SBLR)

- LRU cache of index instances keyed by UUID.
- **No call sites** outside cache implementation.

**Code references:**
- `src/sblr/index_cache.cpp:50-210`

### 5.7 Monitoring

- Metrics registry defines buffer pool counters/gauges.
- **Buffer pool metrics wired** for hits/misses/reads/writes and dirty/pages gauges.
- Query profiler supports buffer hit/miss fields, but there are no call sites.
- Statement/result/translation cache metrics remain unwired.

**Code references:**
- `src/core/telemetry.cpp:496-521`
- `src/core/buffer_pool.cpp:190-330`
- `src/core/buffer_pool.cpp:1040-1100`
- `src/sblr/executor.cpp:52853`
- `src/optimizer/query_profiler.cpp:71-88`

---

## 6) Target Architecture (Alpha)

### 6.0 Alpha Scope vs. Beta Extensions

**Alpha scope (must be implemented):**
- All node-local cache layers described in this spec (buffer pool, statement/plan cache, result cache, translation cache).
- Scan-resistance and read-ahead mechanisms for the buffer pool.
- Deterministic invalidation tied to schema versioning and table version IDs.
- Full cache and buffer monitoring exposure (SQL views + Prometheus counters).

**Beta extensions (cluster-only):**
- Cross-node cache coherence and distributed invalidation.
- Global result cache or plan cache sharing across cluster nodes.
- Cluster-wide cache admission control or distributed cache eviction policies.

### 6.1 Buffer Pool (Page Cache)

**Required:**
1. **Clock-sweep** eviction (retain current implementation).
2. **Scan-resistant rings** for sequential scan, sweep/GC, and bulk write operations.
3. **Hot/Cold segmentation** (midpoint insertion) to avoid scan pollution.
4. **Read-ahead policy** for large scans and range access.
5. **Optional multi-pool** layout (OLTP/OLAP/temp or per-tablespace pools).
6. **Adaptive flushing** (retain current writer, add checkpoint integration hooks).

**Model sources:** PostgreSQL ring buffers and bgwriter; InnoDB midpoint insertion; SQL Server read-ahead patterns.

### 6.2 Storage Block Cache (LSM)

**Required:**
- Sharded LRU or CLOCK for concurrency.
- Optional admission control (e.g., TinyLFU/2Q) to prevent one-hit pollution.
- Optional compression for cached blocks.

### 6.3 Statement/Plan Cache

**Required:**
- **SQL -> SBLR cache** keyed by normalized SQL + parameter signature.
- **Plan cache** tied to schema version IDs and privilege bundles.
- **Deterministic invalidation** on DDL, permissions, and statistics version changes.
- Per-connection cache for prepared statements with global shared cache behind security gating.

### 6.4 Result Cache

**Required:**
- Enabled only for **deterministic** queries and stable snapshots.
- Cache key includes schema version, user privileges, and parameter values.
- Table-level invalidation using table version IDs.
- Size bounds and TTL enforced by policy.

### 6.5 Translation Cache (Parser/Adapter)

**Required:**
- Per-dialect translation cache for SQL->SBLR mappings.
- Segmented by dialect, schema version, and privileges.
- Shared across parser pool processes (when safe) or per-connection in strict mode.

---

## 7) Cache Coherence and Invalidation

### 7.1 Schema and Permission Versioning

Every cache entry must be tied to:
- **Schema version** (increment on DDL)
- **Privilege bundle hash** (change invalidates cached statements/plans)
- **Statistics version** (optional, for plan cache)

### 7.2 Table Version IDs

- Table modifications increment a **table version**.
- Result caches and plan caches using that table must validate against the current version.

### 7.3 MGA Snapshot Safety

- Result cache entries are valid only for the transaction snapshot that produced them.
- A shared result cache must use **snapshot-safe** invalidation or be disabled for strict isolation.

---

## 8) Monitoring and Observability

### 8.1 Required Metrics (Prometheus)

Buffer pool:
- `scratchbird_buffer_pool_hits_total`
- `scratchbird_buffer_pool_misses_total`
- `scratchbird_buffer_pool_reads_total`
- `scratchbird_buffer_pool_writes_total`
- `scratchbird_buffer_pool_pages_dirty`
- `scratchbird_buffer_pool_hit_ratio`

Statement/plan cache:
- `scratchbird_stmt_cache_hits_total`
- `scratchbird_stmt_cache_misses_total`
- `scratchbird_stmt_cache_evictions_total`
- `scratchbird_stmt_cache_memory_bytes`

Result cache:
- `scratchbird_result_cache_hits_total`
- `scratchbird_result_cache_misses_total`
- `scratchbird_result_cache_evictions_total`
- `scratchbird_result_cache_memory_bytes`

### 8.2 Required SQL Views

Expose cache stats in monitoring views (see `docs/specifications/operations/`):
- `sys.cache_stats` (per cache type)
- `sys.buffer_pool_stats` (hits/misses/dirty/flushes)
- `sys.statement_cache` (top cached statements)

---

## 9) Configuration Surface

Configuration keys follow existing naming conventions (systemd spec / config files)
and are documented in `docs/user-documentation/configuration/sb_server.conf.md`:

- `buffer_pool_size`
- `buffer_pool_page_size`
- `buffer_pool_bgwriter_enabled`
- `buffer_pool_bgwriter_max_pages`
- `buffer_pool_dirty_ratio_low`
- `buffer_pool_dirty_ratio_high`
- `buffer_pool_dirty_ratio_checkpoint`
- `statement_cache` (bool)
- `statement_cache_size`
- `result_cache` (bool)
- `result_cache_size`
- `result_cache_ttl`

These keys must map into runtime configuration with per-database overrides where supported.

---

## 10) Acceptance Criteria (Alpha)

A cache subsystem is considered complete when:

1. **Buffer pool** reports stable hit ratio under mixed workloads and does not exhibit scan pollution.
2. **Statement/plan caches** show measurable hit ratios on repeated query workloads.
3. **Result cache** only serves deterministic queries and invalidates correctly on DDL/DML.
4. **Metrics** expose accurate counters and can be traced back to actual cache behavior.
5. **All cache types** are observable through SQL views and Prometheus endpoints.

---

## 11) Related Specifications

- Buffer Pool Design: `docs/specifications/storage/STORAGE_ENGINE_BUFFER_POOL.md`
- Memory Management: `docs/specifications/MEMORY_MANAGEMENT.md`
- Network Layer Caches: `docs/specifications/network/NETWORK_LAYER_SPEC.md`
- Query Optimizer: `docs/specifications/query/QUERY_OPTIMIZER_SPEC.md`
- Monitoring Views: `docs/specifications/operations/MONITORING_SQL_VIEWS.md`
