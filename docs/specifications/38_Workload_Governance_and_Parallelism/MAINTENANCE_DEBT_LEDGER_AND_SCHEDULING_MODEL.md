# Maintenance Debt Ledger and Scheduling Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define the Beta 1 global maintenance-debt ledger and scheduler used by index,
checkpoint, and schema-evolution work. This file turns maintenance from an
implicit background activity into a deterministic engine-owned service.

## Scope

This file owns:

- the durable maintenance-debt ledger
- worker leasing and retry rules
- priority bands and admission budgets
- scheduler defaults and operator controls
- observability and fail-closed rules

## Hard invariants

1. The scheduler does not own visibility truth.
2. The scheduler may defer work only within the bounds admitted by the owning
   subsystem files.
3. A debt item may not be silently dropped.
4. A failed worker lease must return the debt item to a visible pending state.
5. Operator controls may throttle or pause work, but they may not declare debt
   resolved without proof from the owning subsystem.

## Durable maintenance-debt ledger

The engine shall persist maintenance work in `sb_catalog.maintenance_debt_ledger`
with this shape:

| Column | Type | Meaning |
| --- | --- | --- |
| `maintenance_debt_uuid` | `cat_uuid` | row identity |
| `subsystem` | `cat_identifier` | `INDEX`, `CHECKPOINT`, `DDL`, `STORAGE`, `MEMORY` |
| `debt_class` | `cat_identifier` | class from owning subsystem |
| `logical_object_uuid` | `cat_uuid` | target object |
| `partition_uuid` | `cat_uuid` nullable | locality or shard |
| `priority_band` | `cat_identifier` | `P0`, `P1`, `P2`, `P3` |
| `estimated_bytes` | `cat_uint64` | work size |
| `estimated_io_cost` | `cat_uint64` | coarse IO estimate |
| `oldest_pending_at` | `cat_timestamp` | age anchor |
| `resume_payload_json` | `cat_json` nullable | subsystem-owned resume data |
| `debt_state` | `cat_identifier` | `PENDING`, `LEASED`, `RUNNING`, `BLOCKED`, `DONE`, `FAILED_FENCE` |
| `attempt_count` | `cat_uint32` | retries |
| `lease_owner_uuid` | `cat_uuid` nullable | current worker |
| `lease_expires_at` | `cat_timestamp` nullable | lease timeout |
| `last_error_code` | `cat_identifier` nullable | last failure |
| `is_valid` | `cat_bool` | row validity |

## Worker-lease rules

1. one worker may own a debt row at a time
2. lease acquisition changes `debt_state` from `PENDING` to `LEASED`
3. execution changes `debt_state` from `LEASED` to `RUNNING`
4. successful completion changes `debt_state` to `DONE`
5. timeout or worker death changes `debt_state` back to `PENDING`
6. ambiguity or structural contradiction changes `debt_state` to
   `FAILED_FENCE`

## Priority model

Required priority mapping:

| Priority | Meaning | Examples |
| --- | --- | --- |
| `P0` | correctness-adjacent debt that threatens latency or restart safety if ignored | final side-log drain, exact cleanup backlog near reclaim fence, schema cutover guard completion |
| `P1` | performance debt that materially affects foreground work | cold-page delta merge, ranked segment merge, hot-leaf reshape, temp spill merge, JIT retire reclaim |
| `P2` | efficiency debt with bounded operational impact | bloom rebuild, summary merge, compaction, fragmentation repair |
| `P3` | advisory or opportunistic work | density refresh, low-value compaction |

Scheduler ordering shall sort by:

1. `priority_band`
2. oldest pending age
3. owning subsystem urgency
4. estimated IO cost within the same priority

## Admission budgets

The scheduler shall enforce per-class concurrency caps.

Defaults:

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.maint.p0_workers` | `4` | `1..32` | reloadable |
| `sb.maint.p1_workers` | `4` | `1..32` | reloadable |
| `sb.maint.p2_workers` | `2` | `1..16` | reloadable |
| `sb.maint.global_io_budget_mb_per_sec` | `256` | `32..4096` | reloadable |
| `sb.maint.lease_timeout_ms` | `30000` | `1000..600000` | reloadable |
| `sb.maint.failed_fence_retry_limit` | `3` | `0..32` | reloadable |

## Scheduler loop

```cpp
void maintenanceSchedulerTick() {
  auto ready = ledger.fetchReadyDebtOrdered();
  for (auto& debt : ready) {
    if (!budget.allows(debt)) {
      continue;
    }
    if (!ledger.tryLease(debt, self.worker_uuid)) {
      continue;
    }
    runDebt(debt);
  }
}
```

## Work-completion contract

The scheduler does not define completion itself. The owning subsystem must:

1. execute the work
2. persist any subsystem-local completion markers
3. confirm the debt item is satisfied
4. only then mark the debt ledger row `DONE`

## Operator controls

Required operator controls:

- pause or resume by subsystem
- pause or resume by debt class
- set per-priority worker limits
- raise or lower IO budget
- fence a specific object into `FAILED_FENCE`
- force one debt item back to `PENDING` after operator review

## Observability

The engine shall expose:

- pending debt rows by class and priority
- leased and running worker counts
- oldest pending age by class
- bytes and IO estimate backlog
- retry count and `FAILED_FENCE` counts
- scheduler budget saturation

## Explicit non-guarantees

This file does not claim:

1. distributed cluster-wide scheduling
2. tenant-grade fairness certification
3. cost-based query scheduling

## Required tests

1. debt rows survive restart
2. lease expiry returns work to `PENDING`
3. one worker cannot steal a live lease
4. failed-fence debt stops retry after the configured limit
5. scheduler priority order is deterministic for fixed inputs

## Cross-section references

- `PRIORITY_ADMISSION_AND_SCHEDULING_BOUNDARY.md`
- `OPERATOR_CONTROLS_AND_OBSERVABILITY_BOUNDARY.md`
- `../18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md`
- `../35_Durability_Crash_Recovery_and_Checkpoint_Model/CHECKPOINT_BOUND_DELTA_RECONCILIATION_AND_MAINTENANCE_MARKERS.md`
- `../37_Statistics_Metadata_and_Schema_DDL/ONLINE_SCHEMA_CHANGE_AND_BACKFILL_MODEL.md`
