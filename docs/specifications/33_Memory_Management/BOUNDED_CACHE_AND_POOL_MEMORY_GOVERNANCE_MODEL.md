# BOUNDED_CACHE_AND_POOL_MEMORY_GOVERNANCE_MODEL

## Status

Current code-backed authority with current test-backed cache bounds.

## Purpose

This document defines the non-buffer-pool memory caches and bounded pool surfaces that a limited implementer must preserve.

## Governing rule

All current cache and pool surfaces are bounded and advisory.

They are not transaction truth, durability truth, or privilege truth.

## Query result cache

The current query result cache is a bounded LRU cache for `SELECT` result sets.

Current code-backed properties:

1. cache key is SHA-256 of SQL text or bytecode
2. entries carry column metadata, row data, referenced table IDs, timestamps, access count, and estimated size
3. default limits are `64` entries and `64MB`
4. the cache is thread-safe via `shared_mutex`
5. table-based invalidation exists
6. global invalidation exists
7. single oversized entries are refused rather than cached
8. statistics track hits, misses, evictions, invalidations, insertions, current entries, and current memory

## Statement cache

The current statement cache is a bounded prepared-statement reuse surface with both database and connection limits.

Current code-backed properties:

1. per-database statement limit
2. max-memory limit
3. per-connection limit
4. selectable eviction policies:
   - `LRU`
   - `LFU`
   - `ARC`
   - `FIFO`
5. TTL windows
6. statement fingerprinting
7. schema-version and privilege-signature participation in cache identity
8. schema-change invalidation support

The cache key is not raw SQL text alone. Fingerprint, parameter signature, schema version, and privilege signature all participate in cache identity.

## Transaction cache

Current test-backed authority states that the transaction cache is bounded by `MAX_CACHE_SIZE`, sourced from `DEFAULT_TRANSACTION_CACHE_SIZE`, with an LRU discipline.

The authoritative bounded-transaction-cache rules are:

1. least-recently-used entries are evicted when capacity is reached
2. reads and updates touch the LRU position
3. commit, rollback, begin, and transaction-state lookup paths all respect the bounded cache discipline
4. cache structures are mutex-protected

This transaction cache exists to accelerate transaction-state lookup. It must not be misread as the authoritative transaction inventory.

## Cache invalidation rules

Current canonical rules are:

1. result-cache entries referencing modified tables must be invalidated
2. DDL and other schema changes may invalidate statement and result caches
3. permission and security changes invalidate security-sensitive caches rather than trusting stale entries

## Memory-governance rule

The current implementation direction is consistent across these caches:

1. every cache is bounded by entry count, memory, TTL, or all three
2. oversized or stale entries are evicted or refused
3. caches improve latency only; they do not change correctness semantics

## Relationship to always-in-transaction model

Because ScratchBird is always in a transaction:

1. cached results must not bypass transaction visibility rules
2. locking or visibility-sensitive reads must disable inappropriate result-cache reuse
3. cache identity and invalidation must remain subordinate to committed schema and privilege state

## Required implementer interpretation

Another agent shall preserve:

1. bounded size or memory for every cache
2. explicit invalidation on data, schema, or privilege changes
3. transaction cache as accelerator only
4. no use of cache state as authority for MGA correctness
