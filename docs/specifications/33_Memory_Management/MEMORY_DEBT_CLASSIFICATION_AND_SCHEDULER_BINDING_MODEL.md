Status: current_authority_beta1

# Memory Debt Classification and Scheduler Binding Model

## Purpose

This document defines the Beta 1 memory-debt classes that must be executed
through the global maintenance-debt ledger owned by Section 38.

It closes the gap between Section 33 memory policy and the existing background
maintenance scheduler by making memory cleanup deterministic and visible.

## Hard invariants

1. Memory debt may not be hidden in ad hoc background threads.
2. Memory debt does not change visibility truth or publication order.
3. Memory debt completion must be proven before a debt row is marked `DONE`.
4. Foreground pressure handling may perform bounded assists, but durable
   background debt still flows through the Section 38 ledger.

## Canonical memory debt classes

Required `subsystem = MEMORY` debt classes are:

| Debt class | Meaning | Priority band |
| --- | --- | --- |
| `FRAGMENTATION_REPAIR` | allocator or arena compaction work | `P2` |
| `CACHE_SHRINK` | derivative cache shrink backlog beyond foreground assists | `P1` |
| `TEMP_SPILL_MERGE` | merge or cleanup of temporary spill pages or workfiles | `P1` |
| `PAGE_RECLAIM_ASSIST` | page-backed arena reclaim or restoration debt | `P1` |
| `RESIDENT_INDEX_DEMOTE` | bounded resident-index working-set reduction | `P1` |
| `JIT_RETIRE_RECLAIM` | retired code-page or handle-table reclaim | `P1` |
| `OBSERVABILITY_SNAPSHOT_CLEANUP` | bounded cleanup of diagnostic spill artifacts | `P3` |

## Ledger binding

Each debt item shall be persisted through
`sb_catalog.maintenance_debt_ledger` with:

- `subsystem = MEMORY`
- `debt_class` from the canonical set above
- `logical_object_uuid` set to the owning cache, arena, tracker, or domain UUID
- `partition_uuid` set to NUMA node, page-group UUID, or schema-root UUID when
  locality matters
- `resume_payload_json` containing only resume state, never authoritative truth

## Foreground assist rules

Foreground assists are allowed only when all are true:

1. estimated work is below `4 MiB` or `5 ms`
2. the work affects the currently executing owner
3. the work cannot cross a publication fence
4. the work will not consume emergency reserve

Otherwise the work must emit debt and return to the scheduler.

## Emit-debt flow

```cpp
void emitMemoryDebt(MemoryDebtClass debtClass,
                    Uuid objectUuid,
                    std::optional<Uuid> partitionUuid,
                    uint64_t estimatedBytes,
                    Json resumePayload) {
  ledger.upsertDebt({
      .subsystem = "MEMORY",
      .debt_class = toIdentifier(debtClass),
      .logical_object_uuid = objectUuid,
      .partition_uuid = partitionUuid,
      .priority_band = classifyMemoryPriority(debtClass, estimatedBytes),
      .estimated_bytes = estimatedBytes,
      .resume_payload_json = std::move(resumePayload),
  });
}
```

## Completion rules

The owning memory subsystem must:

1. perform the repair or reclaim work
2. persist any ownership or state updates
3. verify recovered bytes or retired state
4. only then mark the debt item `DONE`

## Required metrics

The engine shall expose:

- pending memory debt by class
- oldest pending memory debt age
- bytes recoverable by class
- successful reclaimed bytes
- failed-fence counts for memory debt

## Cross-section references

- `../38_Workload_Governance_and_Parallelism/MAINTENANCE_DEBT_LEDGER_AND_SCHEDULING_MODEL.md`
- `MEMORY_PRESSURE_BACKPRESSURE_AND_ADMISSION.md`
- `TEMPORARY_MEMORY_AND_SPILL_BOUNDARY.md`
- `JIT_CODE_MEMORY_RESOURCE_TRACKER_AND_ARTIFACT_LIFECYCLE_MODEL.md`
