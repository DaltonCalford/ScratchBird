Status: current_authority_beta1

# Memory Budget Tree Breaker and Schema Quota Model

## Purpose

This document defines the Beta 1 canonical memory-governance tree for
ScratchBird. It makes the Section 33 domain model enforceable by defining:

- the ownership tree used for every material allocation
- soft and hard breaker behavior
- schema-root quota inheritance
- the reservation and commit protocol
- the emergency-reserve contract

This file is normative for implementation.

## Hard invariants

1. Every non-trivial allocation must be charged into one leaf of the memory
   budget tree.
2. A successful allocation may not bypass parent charging.
3. Schema-root quota enforcement is first-class. Database-only quotas are
   insufficient for Beta 1.
4. The memory tree governs memory. It does not redefine MGA visibility,
   catalog truth, or parser ownership boundaries.
5. Emergency-reserve bytes exist only for rollback, error materialization,
   observability, and restart-safe cleanup. Foreground feature work may not
   consume them.

## Canonical node hierarchy

The runtime shall maintain the following ownership tree:

1. `PROCESS_ROOT`
2. `DOMAIN_ROOT`
3. `DATABASE_ROOT`
4. `SCHEMA_ROOT`
5. `CONNECTION_ROOT`
6. `TRANSACTION_ROOT`
7. `STATEMENT_ROOT`
8. `OPERATOR_ROOT`
9. `TASK_ROOT`
10. `RESOURCE_TRACKER_ROOT`

`DOMAIN_ROOT` is required for each Section 33 memory domain. `RESOURCE_TRACKER_ROOT`
is used for long-lived objects whose ownership is not naturally statement-local,
including JIT code units, resident-index warm groups, and spill-page groups.

## Required domain roots

The process tree shall contain one root for each domain:

- `buffer_pool_domain`
- `storage_sidecar_cache_domain`
- `transaction_and_visibility_domain`
- `catalog_and_metadata_domain`
- `parser_and_front_door_domain`
- `connection_and_statement_cache_domain`
- `executor_runtime_domain`
- `result_cache_domain`
- `temp_and_spill_domain`
- `resident_index_domain`
- `jit_metadata_domain`
- `jit_code_domain`
- `accelerator_domain`

## Canonical node record

Every in-memory budget node shall carry at least:

| Field | Meaning |
| --- | --- |
| `node_uuid` | stable row or runtime identity |
| `parent_uuid` | parent node identity |
| `node_kind` | one of the canonical hierarchy kinds |
| `memory_domain` | owning domain |
| `owner_uuid` | database, schema, connection, statement, task, or object UUID |
| `soft_limit_bytes` | pressure threshold |
| `hard_limit_bytes` | admission threshold |
| `reserved_bytes` | bytes granted but not yet committed |
| `committed_bytes` | bytes currently charged |
| `peak_bytes` | high-water mark |
| `reclaimable_bytes` | bytes reclaimable without semantic loss |
| `spillable_bytes` | bytes that may move to spill |
| `nonspillable_bytes` | bytes that must stay resident |
| `breaker_state` | `NORMAL`, `SOFT_PRESSURE`, `HARD_PRESSURE`, `EMERGENCY_ONLY`, `BLOCKED` |
| `last_pressure_reason` | last transition reason code |
| `pressure_generation` | monotonic transition counter |

## Quota inheritance

Quotas are inherited top-down.

The required inheritance chain is:

1. process defaults
2. domain defaults
3. database policy overrides
4. schema-root policy overrides
5. workload-class or session policy clamps
6. statement or task explicit reservations

Rules:

1. A child may narrow a parent. A child may not widen a parent.
2. `SCHEMA_ROOT` quotas are inherited recursively through the schema tree unless
   a child schema root defines a tighter override.
3. If a `SCHEMA_ROOT` quota is absent, the database default applies.
4. If both database and schema quotas are absent, the domain default applies.

## Default process policy

Beta 1 defaults are:

| Tunable | Default | Meaning |
| --- | --- | --- |
| `sb.mem.process_soft_pct` | `90` | soft pressure at 90% of process target |
| `sb.mem.process_hard_pct` | `97` | hard pressure at 97% of process target |
| `sb.mem.emergency_reserve_pct` | `3` | process reserve held outside normal admission |
| `sb.mem.emergency_reserve_min_mb` | `128` | minimum reserve |
| `sb.mem.emergency_reserve_max_mb` | `1024` | maximum reserve |
| `sb.mem.schema_default_soft_pct_of_db` | `10` | default schema soft quota |
| `sb.mem.schema_default_hard_pct_of_db` | `15` | default schema hard quota |

The process reserve is:

```text
min(max(process_target_bytes * 0.03, 128 MiB), 1 GiB)
```

## Reservation protocol

All substantial allocation must follow the reservation protocol:

1. identify the owning leaf node
2. compute the parent chain up to `PROCESS_ROOT`
3. attempt `reserve(bytes, class)`
4. if any node enters `HARD_PRESSURE`, invoke the pressure protocol
5. if reservation succeeds, perform allocation
6. convert reserved bytes to committed bytes
7. release unused reservation immediately

The allocation is non-conforming if it commits bytes without a reservation
decision when the allocation class is not explicitly marked `small_untracked`.

`small_untracked` is permitted only for leaf helper allocations smaller than
`512` bytes that are subsequently rolled into a tracked parent arena.

## Breaker transitions

The required state machine is:

1. `NORMAL` to `SOFT_PRESSURE` when committed plus reserved exceeds
   `soft_limit_bytes`
2. `SOFT_PRESSURE` to `HARD_PRESSURE` when committed plus reserved exceeds
   `hard_limit_bytes`
3. `HARD_PRESSURE` to `EMERGENCY_ONLY` when the request would consume the
   emergency reserve
4. `EMERGENCY_ONLY` to `BLOCKED` when the request is not emergency-eligible
5. states move downward only after bytes fall below the configured hysteresis
   boundary

Hysteresis is required. Beta 1 default:

```text
re-enter lower state only after usage falls below threshold - 5% of hard_limit
```

## Emergency-eligible operations

Only these operations may allocate against emergency reserve:

- rollback materialization
- error reporting and diagnostic capture
- forced spill metadata finalization
- pressure-event metrics emission
- checkpoint-safe memory cleanup

JIT compile, cache refill, warm-load, optional acceleration, and speculative
work are not emergency-eligible.

## Reservation flow

```cpp
ReserveResult reserveBytes(MemoryNode& leaf, uint64_t bytes, MemoryClass cls) {
  auto chain = leaf.pathToRoot();
  for (auto* node : chain) {
    if (node->wouldEnterEmergencyOnly(bytes, cls) && !cls.emergencyEligible()) {
      return ReserveResult::blocked("EMERGENCY_ONLY");
    }
    if (node->wouldExceedHard(bytes)) {
      if (!runPressureProtocol(*node, leaf, bytes, cls)) {
        return ReserveResult::blocked("HARD_PRESSURE");
      }
    }
  }
  for (auto* node : chain) {
    node->reserved_bytes += bytes;
    node->peak_bytes = std::max(node->peak_bytes, node->reserved_bytes + node->committed_bytes);
  }
  return ReserveResult::granted();
}
```

## Schema-root admission flow

```cpp
MemoryNode& bindSchemaLeaf(const ExecutionBinding& binding, MemoryDomain domain) {
  auto& db = memoryTree.databaseNode(binding.database_uuid, domain);
  auto& schema = memoryTree.schemaNode(db, binding.schema_root_uuid);
  auto& conn = memoryTree.connectionNode(schema, binding.connection_uuid);
  auto& txn = memoryTree.transactionNode(conn, binding.transaction_uuid);
  auto& stmt = memoryTree.statementNode(txn, binding.statement_uuid);
  return stmt;
}
```

## Required operator behavior

1. Every statement shall create a `STATEMENT_ROOT` for each domain it uses.
2. Every spillable operator shall create an `OPERATOR_ROOT` beneath the
   statement node.
3. Every long-lived published JIT artifact shall create a `RESOURCE_TRACKER_ROOT`
   under `jit_code_domain`.
4. Every background maintenance worker shall allocate beneath a `TASK_ROOT`.

## Non-conforming behavior

The following are forbidden:

1. charging resident-index warm state to generic statement scratch
2. charging published code pages to the parser or executor runtime domain
3. allowing one schema root to overrun another schema root's quota
4. bypassing the breaker tree for spill, result-cache, or JIT compile work

## Cross-section references

- `MEMORY_CONTEXT_HIERARCHY_ARENA_AND_LIFETIME_MODEL.md`
- `MEMORY_PRESSURE_BACKPRESSURE_AND_ADMISSION.md`
- `MEMORY_GRANT_FEEDBACK_AND_OPERATOR_RESERVATION_MODEL.md`
- `../38_Workload_Governance_and_Parallelism/WORKLOAD_CLASS_RESOLUTION_AND_ADMISSION_BINDING_MODEL.md`
