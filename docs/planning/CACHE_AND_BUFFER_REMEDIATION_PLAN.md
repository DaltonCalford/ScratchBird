# Cache and Buffer Remediation Plan (Alpha)

## Purpose
Close the cache/buffer gaps identified in:
- `ScratchBird/docs/findings/CACHE_AND_BUFFER_IMPLEMENTATION_REVIEW.md`
- `ScratchBird/docs/specifications/core/CACHE_AND_BUFFER_ARCHITECTURE.md`
- `ScratchBird/docs/specifications/storage/STORAGE_ENGINE_BUFFER_POOL.md`

This plan targets Alpha completeness and aligns all cache tiers with the canonical spec.

## Scope
- Buffer pool (page cache) scan-resistance + read-ahead
- Statement/plan cache integration
- Result cache integration
- Translation cache integration (parser/adapter)
- Monitoring/metrics wiring + SQL views

## Phase 0: Specification Alignment (Docs)
**CB-P0-01** Keep cache/buffer canonical spec as the source of truth.
- Files: `ScratchBird/docs/specifications/core/CACHE_AND_BUFFER_ARCHITECTURE.md`

**CB-P0-02** Ensure buffer pool spec matches code truth + Alpha targets.
- Files: `ScratchBird/docs/specifications/storage/STORAGE_ENGINE_BUFFER_POOL.md`

**CB-P0-03** Normalize cache config keys in config specs and examples.
- Files: `ScratchBird/docs/specifications/configuration/sb_server.conf.md`,
  `ScratchBird/docs/specifications/configuration/scratchbird.yml.md`

Acceptance: docs describe the same cache architecture and reference the canonical config keys.

## Phase 1: Buffer Pool Enhancements
**CB-P1-01** Add scan-resistant ring buffers for sequential scans, sweep/GC, and bulk writes.
- Files: `ScratchBird/src/core/buffer_pool.cpp`, `ScratchBird/include/scratchbird/core/buffer_pool.h`

**CB-P1-02** Implement midpoint insertion or hot/cold segmentation to prevent scan pollution.
- Files: `ScratchBird/src/core/buffer_pool.cpp`

**CB-P1-03** Implement read-ahead policy with sequential detection and range hints.
- Files: `ScratchBird/src/core/buffer_pool.cpp`, `ScratchBird/src/core/storage_engine.cpp`

**CB-P1-04** Expose multi-pool layout option (hot/cold/temp or per-tablespace) behind config.
- Files: `ScratchBird/src/core/buffer_pool.cpp`, `ScratchBird/docs/specifications/configuration/*`

Acceptance: buffer pool avoids scan pollution and shows stable hit ratio under mixed workloads.

## Phase 2: Statement/Plan Cache Integration
**CB-P2-01** Wire statement cache initialization into connection pool.
- Files: `ScratchBird/src/pool/connection_pool.cpp`

**CB-P2-02** Add SQL normalization + parameter signature hashing for cache keys.
- Files: parser layer + `ScratchBird/src/pool/statement_cache.cpp`

**CB-P2-03** Bind cache entries to schema version IDs and privilege bundles for invalidation.
- Files: `ScratchBird/src/core/catalog_manager.cpp`, cache invalidation hooks

**CB-P2-04** Add per-session prepared-statement cache, backed by shared global cache.
- Files: `ScratchBird/src/pool/statement_cache.cpp`, connection context

Acceptance: repeated statements hit cache, invalidation works on DDL/privilege changes.

## Phase 3: Result Cache Integration
**CB-P3-01** Define eligibility rules (deterministic queries only, stable snapshot).
- Files: executor + cache policy module

**CB-P3-02** Cache key must include schema version, privileges, and parameter values.
- Files: `ScratchBird/src/sblr/query_result_cache.cpp`

**CB-P3-03** Wire read/write path for the SBLR query result cache.
- Files: `ScratchBird/src/sblr/query_compiler_v2.cpp`, `ScratchBird/src/sblr/executor.cpp`

Acceptance: result cache serves safe queries and invalidates on DDL/DML.

## Phase 4: Translation Cache (Parser/Adapter)
**CB-P4-01** Implement per-dialect translation cache for SQL -> SBLR.
- Files: parser adapters, translation cache module

**CB-P4-02** Segment translation cache by dialect, schema version, and privileges.
- Files: parser adapters

Acceptance: repeated SQL translation uses cache; invalidation follows schema versioning.

## Phase 5: Monitoring and Observability
**CB-P5-01** Wire buffer pool counters to telemetry registry.
- Files: `ScratchBird/src/core/buffer_pool.cpp`, `ScratchBird/src/core/telemetry.cpp`

**CB-P5-02** Emit cache hit/miss/eviction metrics for statement/result/translation caches.
- Files: cache modules + telemetry

**CB-P5-03** Implement SQL monitoring views: `sys.cache_stats`, `sys.buffer_pool_stats`,
`sys.statement_cache`.
- Files: monitoring view definitions + executor hooks

Acceptance: metrics and SQL views show live, accurate cache stats.

## Phase 6: Tests and Benchmarks
**CB-P6-01** Unit tests for cache keying, invalidation, and eviction policies.
**CB-P6-02** Integration tests for cache hit/miss ratios on repeated workloads.
**CB-P6-03** Performance benchmarks for scan-resistant vs. baseline.

Acceptance: tests cover core cache behaviors; benchmarks show reduced scan pollution.

## Dependencies / Notes
- ScratchBird code is read-only in this workspace; this plan is a roadmap for implementation.
- See `ScratchBird/docs/findings/CACHE_AND_BUFFER_IMPLEMENTATION_REVIEW.md` for code-truth references.
