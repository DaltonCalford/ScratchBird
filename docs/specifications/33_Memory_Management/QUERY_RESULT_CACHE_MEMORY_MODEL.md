# Query Result Cache Memory Model

Status: current_authority_with_reconstructed_expansion

## 1. Scope

ScratchBird currently has two distinct result-cache memory domains:

- engine-executor result cache
- pool-layer result cache

They must be documented and implemented separately. Treating them as one cache produces incorrect admission, invalidation, and observability behavior.

## 2. Cache-family summary

| Cache family | Owner | Default memory cap | Default entry cap | Primary state model |
| --- | --- | --- | --- | --- |
| executor result cache | `sblr::QueryResultCache` | `64 MiB` | `64` | LRU, table/global invalidation |
| pool result cache | `pool::DatabaseResultCache` | `256 MiB` | `10000` | TTL plus entry-state machine |

## 3. Executor result-cache memory domain

### 3.1 Purpose

This cache keeps reusable top-level `SELECT` results inside the engine execution path.

### 3.2 Memory accounting

Admission and eviction must account for:

- column names
- column type descriptors
- row count
- per-row payload bytes
- variable-length payload bytes
- encrypted payload byte contribution when present
- referenced table UUID list
- structural object overhead

A conforming memory model must reject row-count-only accounting.

### 3.3 Admission and eviction

Current code-backed behavior:

- singleton cache manager
- default `64 MiB` total cap
- default `64` entry cap
- LRU eviction
- no TTL state machine

### 3.4 Key identity

Executor result-cache identity is derived from:

- SQL or bytecode digest
- statement index
- strict-mode flag
- bound parameter values and null map
- current user id
- active role id
- security policy epoch

### 3.5 Invalidation

Executor result-cache invalidation is driven by:

- referenced-table invalidation
- invalidate-all
- memory-pressure eviction

This cache is not driven by a pool-style TTL or a pool-style deferred invalidation queue.

## 4. Pool result-cache memory domain

### 4.1 Purpose

This cache keeps reusable result sets at the connection-pool layer and therefore carries stronger schema, privilege, and freshness metadata.

### 4.2 Default memory and object limits

Current code-backed defaults:

- `max_memory_bytes = 256 MiB`
- `max_entries = 10000`
- `max_result_size = 16 MiB`
- `max_rows_per_result = 100000`

### 4.3 Freshness and eviction controls

Current code-backed defaults and controls:

- default TTL `300` seconds
- minimum TTL `10` seconds
- maximum TTL `3600` seconds
- selectable policy: `LRU`, `LFU`, `SIZE`, `TTL`
- optional partial caching
- optional empty-result caching
- deterministic-query requirement gate
- snapshot-safe requirement gate
- parameterized-only gate

### 4.4 Key identity

Pool cache identity binds:

- SQL text
- serialized parameters
- `schema_version_id`
- `privilege_signature`

### 4.5 Entry-state machine

Pool result entries may be:

- `VALID`
- `LOADING`
- `PARTIAL`
- `STALE`
- `INVALID`

### 4.6 Transaction-bound invalidation

Pool caches are transaction-aware:

- table modifications can be recorded in a pending invalidation set
- pending invalidations publish at commit through `commit_invalidations()`
- pending invalidations are discarded at rollback through `rollback_invalidations()`

Because ScratchBird is always in a transaction, this means pool-result freshness is commit-bound, not statement-bound.

## 5. Separation rules

The following separations are mandatory:

- executor result cache and pool result cache must not share admission logic by default
- executor cache does not inherit TTL semantics from the pool layer
- pool cache does not inherit executor-only top-level-select assumptions
- a hit in one cache does not imply a hit in the other
- operators must be able to reason about the two memory domains separately even when generic telemetry is currently aggregated

## 6. Observability limitation currently present in code

Current generic telemetry counters for result-cache hits, misses, and evictions are incremented by both the executor-side and pool-side caches.

That means generic result-cache telemetry currently conflates two different memory domains.

Until separate public counters exist, operator guidance must treat generic result-cache counters as combined activity, not as proof of one specific cache family.

## 7. Memory-governance requirements

A conforming implementation must:

- keep executor and pool result-cache memory budgets distinct
- account for full result payload size, not row count only
- bind cache reuse to schema, privilege, and security identity appropriate to the cache family
- publish pool invalidation at transaction commit boundary
- keep executor result invalidation subordinate to referenced-table truth and memory pressure
- never let result-cache residency override MGA visibility or committed catalog truth
