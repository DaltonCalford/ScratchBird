# Execution Cache and Invalidation

Status: current_authority_with_reconstructed_expansion

## 1. Purpose

ScratchBird has multiple reuse surfaces that were previously flattened into a
single "cache" story. That is incorrect. The current runtime has separate
prepared-bundle, plan, execution-result, pool-result, and native-artifact reuse
surfaces with different owners, keys, invalidation rules, and observability
lanes.

A limited implementer must treat these surfaces as distinct subsystems.

## 2. Reuse surface taxonomy

| Surface | Primary owner | Primary purpose | Mutability rule | Primary invalidators |
| --- | --- | --- | --- | --- |
| `PreparedFastPathBundle` | planner plus executor | Reuse prepared-query bound identities, key programs, row layouts, and write programs for admitted prepared shapes | replaceable by prepared identity and parameter regime, invalidated on dependency change | schema, security, policy, placement, parameter regime |
| `optimizer::VNextPlanCache` | optimizer | Reuse validated plan payloads and native-ready planning artifacts | immutable after successful insert | catalog epoch, security epoch, capability, translation, ABI, target, payload, object reference |
| `sblr::QueryResultCache` | executor | Reuse top-level statement results inside the engine execution path | replaceable by key, table invalidation supported | table modification, global invalidate, memory pressure |
| `pool::DatabaseResultCache` | pool layer | Reuse result sets across pooled sessions under schema and privilege signatures | mutable entry state machine with deferred invalidation | table/schema modification, DDL, TTL expiry, transaction boundary commit invalidation |
| native artifact reuse | JIT/runtime | Reuse generated native artifacts and fallback SBLR payloads | artifact identity fixed after publish, retirement allowed | ABI mismatch, target mismatch, module/version mismatch, unusable artifact retirement |

## 3. `optimizer::VNextPlanCache`

### 3.1 Role

This cache stores validated plan records for the VNext compiler and planner path. It is not a generic SQL statement cache and it is not a result cache.

### 3.2 Key contract

A valid plan-cache key requires all of the following fields:

- `profile_id`
- `profile_version`
- `taxonomy_contract_id`
- `payload_format`
- `payload_hash`
- `session_option_signature`
- `role_context_signature`
- `canonical_opcode_symbol`
- non-zero `catalog_epoch`
- non-zero `security_epoch`
- `capability_set_hash`
- non-zero `module_version`
- non-zero `translation_rule_version`
- `object_ref_digest`
- `plan_profile_signature`
- `index_family_signature`
- `family_statistics_signature`
- `statistics_snapshot_signature`
- `cost_profile_id`
- `policy_snapshot_id`

If the artifact preference is anything other than `SBLR_ONLY`, the key also requires:

- `host_api_abi_version`
- `target_triples_hash`
- `optimization_level`

The canonical string form is built by `buildPlanCacheKey()` using the stable `pc.v3|...` prefix and a deterministic trailing hash. Implementers must not invent an alternative serialization.

### 3.3 Value contract

A valid cache value requires:

- `native_feature_key`
- `normalized_payload_hash`
- `native_ast_hash`
- `sblr_hash`
- non-empty `sblr_payload`
- non-zero `compile_module_id`

Additional rules:

- `FALLBACK_SBLR_ONLY` values must provide `fallback_reason_code`.
- `GENERATED` values must provide sorted `native_artifacts` in stable `target_triple` order.
- duplicate insert for an already populated key is rejected; overwrite-in-place is not allowed.

### 3.4 Concurrency model

- readers use the cache concurrently through shared access
- inserts and invalidations take exclusive mutation access
- after insertion, a value is treated as immutable
- invalidation removes entries; it does not patch fields inside a live value

### 3.5 Invalidation dimensions

The runtime must support the following invalidation lanes:

- invalidate all entries
- invalidate by payload hash
- invalidate by object reference digest
- invalidate on catalog epoch mismatch
- invalidate on security epoch mismatch
- invalidate on capability set hash mismatch
- invalidate on module version mismatch
- invalidate on translation rule version mismatch
- invalidate on host API ABI version mismatch
- invalidate on target triples hash mismatch

These lanes exist because plan correctness depends on translation rules, catalog identity, security state, target environment, and optimizer statistics identity. A cache hit with any mismatched dimension is non-conforming.

## 3A. `PreparedFastPathBundle`

### 3A.1 Role

This is the prepared-query performance bundle defined by:

- `../36_Query_Rewrite_and_Planner/HIGH_PERFORMANCE_OLTP_PLAN_SHAPES_CONTENTION_AVOIDANCE_AND_PREPARED_EXECUTION_MODEL.md`

It is the reuse surface for admitted prepared shapes such as:

- prepared point select
- prepared point update or delete
- prepared append insert
- prepared micro-batch insert
- prepared bounded lookup join

It is not the same thing as the plan cache and it is not the same thing as the
executor result cache.

### 3A.2 Required contents

A prepared fast-path bundle logically contains:

- prepared statement identity
- parameter-regime identity
- bound relation and index identities
- key-extraction program identity
- row-layout identity
- write-program identity when DML applies
- plan-cache key when the plan is reusable
- result-cache candidacy bit for cacheable top-level selects

### 3A.3 Key rule

A prepared fast-path bundle hit is legal only when all of the following match:

- prepared statement identity
- parameter regime
- schema dependency signature
- security dependency signature
- policy snapshot signature
- placement or routing signature when applicable

### 3A.4 Distinction rule

Implementers shall preserve these distinctions:

1. prepared bundle hit does not imply a plan-cache hit
2. plan-cache hit does not imply a prepared bundle hit
3. prepared bundle hit does not imply a result-cache hit
4. result-cache hit on a prepared select is legal only after prepared-bundle
   validation succeeds

### 3A.5 Invalidation

The runtime shall support prepared-bundle invalidation on:

- schema change
- security change
- policy change
- placement or route change
- parameter-regime mismatch that invalidates the fast path

Prepared-bundle invalidation does not by itself require prepared-handle
retirement when the canonical shape is still valid.

## 4. `sblr::QueryResultCache`

### 4.1 Role

This is the executor-side result cache. It is used for cacheable top-level execution results, not for planning artifacts.

### 4.2 Owner and default shape

Current code-backed defaults are:

- singleton manager via `QueryResultCacheManager::getInstance()`
- default maximum entries: `64`
- default maximum memory: `64 MiB`
- eviction policy: LRU

### 4.3 Key material

The base cache identity is derived from either:

- SHA-256 of SQL text
- SHA-256 of bytecode payload

The executor-side final cache key also binds:

- statement index
- strict-mode flag
- parameter values and null map
- current user id
- active role id
- security policy epoch

A result cache hit is therefore only legal when execution identity, security identity, and statement identity all match.

### 4.4 Admission and scope

The executor-side result cache is currently used on the top-level `SBLR3_SELECT` path when the planner/executor marked the select as cacheable.

It is not authoritative for:

- transactional visibility
- non-select statements
- DDL publication
- plan reuse
- cross-database pooled result reuse

### 4.5 Memory accounting

The cache size estimate includes:

- column names
- column type metadata
- row payloads
- variable-length field payloads
- encrypted data lengths when present
- referenced table UUID list
- fixed object overhead

Implementers must use full result-shape accounting, not row-count-only accounting.

### 4.6 Invalidation

Supported invalidation lanes are:

- invalidate by referenced table
- invalidate all entries
- evict by memory pressure

This cache has no TTL state machine in the current engine path. Freshness is table- and memory-driven.

## 5. `pool::DatabaseResultCache`

### 5.1 Role

This is the connection-pool-side result cache. It is a separate reuse surface from `sblr::QueryResultCache` and must never be described as the same cache.

### 5.2 Default configuration

Current code-backed defaults are:

- `max_memory_bytes = 256 MiB`
- `max_entries = 10000`
- `max_result_size = 16 MiB`
- `max_rows_per_result = 100000`
- default TTL: `300` seconds
- minimum TTL: `10` seconds
- maximum TTL: `3600` seconds
- policy family: `LRU`, `LFU`, `SIZE`, or `TTL`
- `invalidate_on_insert = true`
- `invalidate_on_update = true`
- `invalidate_on_delete = true`
- `invalidate_on_truncate = true`
- `invalidate_on_ddl = true`
- `use_transaction_boundaries = true`

### 5.3 Key and metadata

A legal pool-result-cache key binds:

- SQL text
- serialized parameters
- `schema_version_id`
- `privilege_signature`

Each cached entry also retains metadata including:

- referenced tables
- referenced schemas
- schema version id
- privilege signature
- execution time
- query cost
- determinism marker
- snapshot-safety marker
- volatile-function marker

### 5.4 Entry states

Pool cache entries may be in one of the following states:

- `VALID`
- `LOADING`
- `PARTIAL`
- `STALE`
- `INVALID`

This state machine does not exist on the executor-side cache. Do not copy pool-state rules into the engine result cache.

### 5.5 Invalidation and transaction binding

The pool layer supports:

- per-table invalidation
- schema invalidation through schema version changes
- deferred invalidation via `mark_table_modified()`
- publication of pending invalidations on `commit_invalidations()`
- discard of pending invalidations on `rollback_invalidations()`

Under the ScratchBird always-in-transaction model, this means pool-cache visibility changes are commit-bound. Statement errors do not publish invalidation unless the transaction later commits.

## 6. Native artifact reuse

### 6.1 Role

Native artifacts generated by JIT or compilation are a reuse surface, but they are not a generic cache equivalent to the plan cache or result caches.

### 6.2 Governing rules

- native artifacts are tied to module identity, translation identity, ABI compatibility, and target compatibility
- fallback SBLR payload remains authoritative when native generation is absent or rejected
- unusable artifacts may be retired; retirement is not equivalent to result-cache eviction
- native artifact reuse must respect the same catalog, security, and translation identities that gated plan publication

## 7. Canonical invalidation order

When a statement or transaction changes schema or data, implementers must evaluate invalidation in this order:

1. publish or reject transaction state under MGA rules
2. publish catalog/security epoch changes when applicable
3. invalidate prepared-bundle lanes whose dependency signatures are no longer
   valid
4. invalidate plan-cache lanes whose key dimensions are no longer valid
5. invalidate executor-side result entries by referenced table when required
6. publish pool-side deferred invalidations at commit boundary
7. retire incompatible native artifacts when translation, ABI, or capability identity changed

This order prevents derivative cache state from leading committed truth.

## 8. Observability boundary

These subsystems do not share a single perfect telemetry lane.

Current code-backed observability is split across:

- generic cache telemetry counters
- sys catalog cache rows
- vNext optimizer event counters
- JIT performance snapshots and catalog artifact stats

Operators must interpret those lanes as distinct, not as a single aggregated cache model. Section 20 defines the authoritative observability split.

## 9. Non-authority and rejection rules

The following claims are incorrect:

- there is one universal execution cache for ScratchBird
- prepared bundle hit implies plan-cache hit
- result-cache hit implies plan-cache hit
- pool result cache and executor result cache are interchangeable
- native artifact reuse is the same subsystem as result caching
- plan values may be patched in place after insert
- cache truth may override committed MGA visibility

## 10. Implementation requirements

A conforming implementation must:

- keep all five reuse surfaces distinct
- preserve immutable-after-write plan-cache semantics
- preserve prepared-bundle identity and invalidation independently from the
  plan cache
- bind result reuse to security, parameter, and schema identity
- publish invalidation only after the governing MGA truth changes
- keep derivative reuse surfaces subordinate to transaction and catalog truth
