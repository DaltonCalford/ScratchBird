# Execution Memory Contexts and Budgets

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the current code-backed execution-memory surfaces that materially affect planning, execution, caching, and JIT behavior, and it separates those real domains from still-unfinished universal budget-governor claims.

## Current code-backed memory domains

### Buffer-backed execution domain

Current authority already includes a real buffer-backed storage domain with:
- page frames
- dirty-state tracking
- writeback state
- residency tiers
- thrash-detector state

This domain is durability-adjacent and must not be conflated with transient operator scratch.

### Executor and spill budgeting domain

Current planning and runtime surfaces already carry:
- estimated memory fields
- spill expectation
- spill policy
- worker-count planning fields

These are real current authority for operator planning and execution reporting, even though they do not yet form a universal hard-cap governor for every operator family.

### Query-result cache domain

Current code-backed query-result cache behavior is:
- enabled by default
- maximum `64` cached entries by default
- maximum `64 MiB` cached memory by default
- cache key from SHA-256 of SQL text or compiled bytecode
- LRU eviction
- table-based invalidation
- full invalidation path for broad schema or DDL changes
- hit, miss, eviction, invalidation, insertion, current-entry, and current-memory statistics

The cache estimates entry size from:
- column names
- column types
- row values
- variable-length payloads
- encrypted payload sizes
- referenced table ids
- per-entry structural overhead

### JIT and native-artifact memory domain

Current code-backed JIT memory surfaces include:
- compile queue entries
- dedupe-key registry
- hotness counters
- per-object performance maps
- in-memory native blobs during verification and materialization
- optional signature blobs during verification

Persistent native blobs and signatures are catalog-plus-TOAST backed and must not be treated as anonymous process heap policy.

### Resident index and accelerator domain

Resident-by-default index and accelerator working sets remain a separate domain and must not be silently charged to generic executor scratch memory.
That rule is already required elsewhere in section `33` and remains binding here.

## Current authority rules

1. Buffer-backed page residency and transient execution scratch are distinct domains.
2. Query-result cache memory is bounded independently from the buffer pool.
3. JIT queue growth is bounded by queue capacity, not by unbounded compile-request accumulation.
4. Native artifact blobs are persistent catalog objects first and transient loaded buffers second.
5. Result-cache invalidation must follow table modification or broad invalidation signals; cached rows are not independent truth.

## Current budget and limit surfaces

Current code-backed limit surfaces include:
- result cache entry-count limit
- result cache memory limit
- JIT compile queue capacity
- hotness threshold gate before deferred JIT promotion
- plan payload estimated memory and spill-policy fields

## Current non-claims

Current code does not yet prove:
- a single universal executor memory-context tree for all operators
- a closed per-operator hard budget governor across the entire engine
- a globally unified admission controller that charges every transient byte to one mature scheduler

## Reconstructed commercial-grade requirements

The mature recovered specification requires:
- explicit memory-domain ownership for every executor-adjacent subsystem
- explicit charging of result cache, resident index, accelerator, and JIT working sets to the correct domain
- refusal or spill when a domain cannot legally grow
- observability that lets operators distinguish:
  - buffer pressure
  - result-cache pressure
  - JIT queue or artifact pressure
  - resident-index pressure

## Fail-closed rule

No execution subsystem may hide material memory growth inside an unrelated domain simply because a universal governor is not yet closed. Missing governor closure is not permission for silent cross-domain borrowing.
