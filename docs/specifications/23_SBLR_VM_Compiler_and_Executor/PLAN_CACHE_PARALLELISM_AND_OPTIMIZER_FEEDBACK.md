# Plan Cache Parallelism and Optimizer Feedback

Status: current_authority_with_reconstructed_expansion

## 1. Scope

This file governs the VNext plan-cache concurrency model, cache-key and value validation, and the optimizer feedback surfaces that are already wired into current code.

It does not govern result caching. Result-cache behavior is specified separately.

## 2. Plan-cache ownership model

The authoritative plan-cache implementation is `optimizer::VNextPlanCache`.

It is responsible for:

- stable plan-key construction
- insert-time validation
- immutable value retention
- multi-reader access
- explicit invalidation
- lightweight operational statistics

It is not responsible for:

- statement result reuse
- transaction visibility
- schema publication
- adaptive self-modifying plan rewrites after insert

## 3. Concurrency model

The current concurrency design is read-many, mutate-by-exclusive-operation:

- cache lookups run under shared access
- insert and invalidation run under exclusive access
- a successfully stored entry becomes immutable
- duplicate insert for an existing live key is rejected instead of patched or merged

This model exists to keep a plan value stable once published. Readers must not observe partially rewritten plan records.

## 4. Canonical key-validation procedure

A plan-cache insert is legal only if the key validates all required identity dimensions.

### 4.1 Mandatory dimensions

- planning profile identity
- translation taxonomy identity
- payload format and payload hash
- session option signature
- role-context signature
- canonical opcode symbol
- catalog epoch
- security epoch
- capability-set identity
- module version
- translation-rule version
- object-reference digest
- plan-profile signature
- index-family signature
- family-statistics signature
- statistics-snapshot signature
- cost-profile id
- policy-snapshot id

### 4.2 Mandatory native dimensions when native generation is allowed

If the artifact preference allows native artifacts, the key must also validate:

- host API ABI version
- target-triples hash
- optimization level

### 4.3 Stability rule

The cache key must be derived only from canonicalized identities. No ambient pointer address, temporary object identity, or non-deterministic serialization may participate in the key.

## 5. Canonical value-validation procedure

A cache value is admissible only when it provides the normalized planning identity and the authoritative fallback payload.

Mandatory value fields:

- `native_feature_key`
- `normalized_payload_hash`
- `native_ast_hash`
- `sblr_hash`
- non-empty `sblr_payload`
- non-zero `compile_module_id`

Additional requirements:

- `FALLBACK_SBLR_ONLY` requires `fallback_reason_code`
- `GENERATED` requires at least one native artifact
- native artifacts must be deterministically sorted by `target_triple`

## 6. Invalidation axes

The plan cache must invalidate when any correctness dimension changes.

Required invalidation axes are:

- all entries
- payload hash
- object-reference digest
- catalog epoch
- security epoch
- capability-set hash
- module version
- translation-rule version
- host API ABI version
- target-triples hash

A plan hit that survives any of these mismatches is non-conforming.

## 7. Parallel compilation and publication

The plan cache is allowed to receive outputs from concurrent planners or compilers, but publication must remain deterministic.

Publication rules:

1. compile or plan work may proceed concurrently
2. each candidate must validate the full key and value contracts before publication
3. the first successful insert for a key wins
4. later equivalent publications for the same live key are rejected instead of merged
5. invalidation removes entries; it does not reopen a live value for mutation

This preserves deterministic read behavior even under parallel planning or native compilation.

## 8. Optimizer feedback and metrics identity

### 8.1 Current code-backed plan-cache stats

`VNextPlanCache` keeps internal counts for:

- hits
- misses
- inserts
- invalidations
- errors
- entries

### 8.2 Public observability lane

Current public plan-cache observability is not exposed through the generic statement/result cache counters.

The public lane is the VNext optimizer event counter recorded through `core::VNextMetricsEventModel::recordOptimizerEvent` with metric name:

- `scratchbird_vnext_optimizer_events_total`

Required labels are:

- `event`
- `outcome`
- `code`

The current cache-violation code already used in this path is:

- `UDR_1511`

### 8.3 Required interpretation rule

Operators and implementers must not infer plan-cache health from generic result-cache telemetry. Plan-cache events, generic cache counters, and JIT artifact telemetry are separate lanes.

## 9. Feedback relationship to section 18 metrics

The plan key already incorporates index and statistics identity through:

- `index_family_signature`
- `family_statistics_signature`
- `statistics_snapshot_signature`
- `cost_profile_id`
- `policy_snapshot_id`

This means plan reuse is already bound to the optimizer-facing index metrics contract. If family-native statistics change, plan reuse must re-evaluate through invalidation or key mismatch.

## 10. Non-authority and rejection rules

The following claims are incorrect:

- the plan cache is a mutable optimization memo that may be edited after insert
- result-cache counters are sufficient to diagnose plan-cache correctness
- duplicate insert should overwrite the older plan in place
- a native artifact preference may omit ABI or target identity
- adaptive feedback may silently patch a published plan without invalidation

## 11. Implementation requirements

A conforming implementation must:

- preserve immutable-after-write plan values
- use the canonical multi-dimensional key
- reject malformed or under-specified values
- invalidate on all correctness-relevant dimension changes
- expose plan-cache events through the optimizer event path
- keep result-cache telemetry and plan-cache telemetry distinct in operator guidance
