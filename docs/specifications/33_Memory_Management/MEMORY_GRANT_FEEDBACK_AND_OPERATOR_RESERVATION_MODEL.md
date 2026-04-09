Status: current_authority_beta1

# Memory Grant Feedback and Operator Reservation Model

## Purpose

This document defines the Beta 1 operator and statement memory-grant system.
It prevents ScratchBird from relying on static guesses for work that can spill,
partition, or resize itself.

## Scope

This file owns:

- initial reservation estimation
- statement and operator grant binding
- persisted feedback rows
- percentile-based right-sizing
- oscillation detection
- spill and cancellation interaction

## Hard invariants

1. A grant is an admission decision, not a hint.
2. Operators may not consume more than their grant without either:
   - expanding through the breaker tree
   - spilling
   - being canceled
3. Feedback may never widen identity reuse across schema, privilege, or plan
   shape mismatch.
4. Grant feedback may improve sizing. It may not change semantics.

## Canonical grant identity

Feedback is keyed by:

- `database_uuid`
- `schema_root_uuid`
- `engine_family`
- normalized plan signature
- canonical bytecode hash
- plan epoch compatibility
- workload class
- operator kind

## Persisted feedback row

The engine shall persist feedback in
`sb_catalog.memory_grant_feedback` with at least:

| Column | Meaning |
| --- | --- |
| `grant_feedback_uuid` | row identity |
| `grant_key_hash` | stable identity hash |
| `database_uuid` | database owner |
| `schema_root_uuid` | schema-root owner |
| `operator_kind` | `HASH_AGG`, `SORT`, `HASH_JOIN`, `VECTOR_SCAN`, and so on |
| `sample_count` | number of completed samples |
| `last_grant_bytes` | most recent grant |
| `p50_bytes` | median observed use |
| `p90_bytes` | 90th percentile observed use |
| `peak_bytes` | maximum observed use |
| `spill_count` | spill events |
| `cancel_count` | over-limit cancellations |
| `oscillation_count` | unstable adjustment counter |
| `state` | `STABLE`, `WARMING`, `OSCILLATING`, `DISABLED` |
| `updated_at` | last update |

## Initial estimation

Before feedback exists, the engine shall estimate grants from:

1. operator kind
2. estimated rows
3. estimated groups or partitions
4. key widths and payload widths
5. algorithm shape
6. spill support
7. vector or ANN lane enablement

## Required grant flow

1. bind the statement node
2. compute the operator grant request
3. attempt reservation beneath the statement node
4. if reservation fails, run pressure protocol
5. if still blocked:
   - spillable operator: start in spill-enabled mode
   - non-spillable operator: cancel before execution
6. track `current_bytes`, `peak_bytes`, `spill_bytes`, and `expand_count`
7. update feedback on operator completion

## Adjustment algorithm

Beta 1 shall use percentile-based right-sizing:

```text
next_grant =
  clamp(
    max(minimum_operator_floor, p90_bytes * 1.10),
    operator_floor,
    operator_cap
  )
```

Additional rules:

1. if `spill_count > 0`, the next grant may rise up to `min(peak_bytes * 1.10, operator_cap)`
2. if `peak_bytes < last_grant_bytes / 2` for `8` consecutive stable executions,
   the next grant may shrink
3. if alternating widen/shrink occurs `5` times in `16` samples, mark the row
   `OSCILLATING`
4. `OSCILLATING` rows use the greater of `p90_bytes` and the last stable grant
5. after `3` more unstable windows, mark the row `DISABLED`

## Benchmark-parity closure requirements

For benchmark-governed operators, grant policy is a speed contract as well as
an admission contract.

The engine shall:

1. prefer fit-in-memory execution when warm feedback proves that the operator's
   `p90_bytes` fits within the legal statement and domain budget
2. treat repeated avoidable spill under stable warm feedback as
   non-conforming behavior
3. preserve the observed memory identity boundary so grant reuse never crosses
   cache mode, planner policy snapshot, execution intent, storage shape, or
   operator kind
4. bind leader and worker reservations explicitly when a parallel candidate is
   chosen, rather than hiding worker scratch in untracked process heap
5. publish the combined memory posture of serial versus parallel alternatives
   so a lower-cost path is not chosen by undercharging worker memory

The parity package may not close while a benchmark-governed sort, hash join,
merge join, hash aggregate, distinct, or window path spills repeatedly despite
stable evidence that it can fit legally in memory.

## Operator floors and caps

Beta 1 default floors:

| Operator kind | Floor |
| --- | --- |
| `SORT` | `8 MiB` |
| `HASH_AGG` | `16 MiB` |
| `HASH_JOIN` | `16 MiB` |
| `VECTOR_SCAN` | `32 MiB` |
| `ANN_SEARCH` | `64 MiB` |

Caps are determined by:

1. schema-root quota
2. statement hard limit
3. domain hard limit
4. process reserve constraints

## Sample execution flow

```cpp
OperatorGrant bindOperatorGrant(const PlanNode& node, StatementContext& stmt) {
  auto key = GrantKey::fromPlan(node, stmt);
  auto fb = catalog.loadGrantFeedback(key);
  uint64_t requested = estimateInitialGrant(node, fb);
  auto lease = reserveBytes(stmt.memoryNode(node.domain), requested, node.memoryClass());
  if (!lease.ok() && node.supportsSpill()) {
    return OperatorGrant::spillFirst(requested);
  }
  if (!lease.ok()) {
    throw MemoryLimitExceeded("operator grant denied");
  }
  return OperatorGrant::reserved(requested, lease);
}
```

## Required metrics

The engine shall emit:

- grant requests
- grant denials
- grant expansions
- spill-triggered grant failures
- per-operator peak bytes
- feedback updates
- oscillation disables

## Non-conforming behavior

The following are forbidden:

1. reusing feedback across schema or privilege drift
2. allowing a spillable operator to consume unlimited memory because it "can spill later"
3. widening grants from a single pathological execution without percentile protection

## Cross-section references

- `MEMORY_BUDGET_TREE_BREAKER_AND_SCHEMA_QUOTA_MODEL.md`
- `TEMPORARY_MEMORY_AND_SPILL_BOUNDARY.md`
- `MEMORY_PRESSURE_BACKPRESSURE_AND_ADMISSION.md`
