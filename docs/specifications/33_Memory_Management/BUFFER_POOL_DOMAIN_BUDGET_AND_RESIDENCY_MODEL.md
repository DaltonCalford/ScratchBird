# Buffer Pool Domain Budget and Residency Model

Status: current_authority_beta1

## Purpose

This file defines the Beta 1 residency controller used for shared page
residency. It covers buffer-pool policy domains, workload classes, residency
tiers, dirty-state handling, and the binding between persistent pages and
spillable temporary pages.

## Current code-backed authority

The buffer pool already exposes explicit policy vocabulary for:
- buffer profiles
- pool layouts
- policy domains
- workload classes
- MGA page classes
- writeback queue states
- residency tiers
- lifecycle states
- dirty states
- thrash detector states
- per-domain budget configuration

## Policy domains

Current policy domains are:
- `CriticalSystem`
- `HotOltp`
- `ReadMostly`
- `ScanBulkRing`
- `VersionUndo`
- `TemporaryWork`

These domains are budget and admission concepts over the shared frame array. They do not change MGA truth.

## Unified residency controller

The buffer pool is the shared residency controller for:

1. persistent data pages
2. temporary spill pages
3. temporary table pages
4. resident-index page groups that use buffer-managed residency

Rules:

1. persistent and temporary pages share one residency controller
2. policy domains remain distinct even when frames come from the same array
3. temporary spill pages use `TemporaryWork` workload and page classes unless a
   more specific owning subsystem defines a narrower page class
4. cache eviction is separate from spill write lifecycle even when both use the
   same frame controller

## Workload classes

Current workload classes include:
- `PointLookup`
- `IndexProbe`
- `RangeScan`
- `SequentialScan`
- `NestedLoopReread`
- `SweepGc`
- `BulkWrite`
- `CheckpointCleaner`
- `RecoveryReplay`
- `PrefetchSpeculative`
- `TemporaryWork`

## MGA page classes

Current MGA page classes include:
- `TX_STATE`
- `SYSTEM_META`
- `INDEX_ROOT_INTERNAL`
- `VERSION_ROOT`
- `CHAIN_HEAVY`
- `GC_CANDIDATE`
- `SCAN_PROBATION`
- `INDEX_CHURN`
- `TEMP_WORK`

## Residency and lifecycle

Current residency tiers include:
- `LegacyShared`
- `RingOnly`
- `Probationary`
- `Protected`
- `PinBiased`

Current lifecycle states include:
- `Free`
- `Loading`
- `Valid`
- `Flushing`
- `Evicting`
- `Error`

## Dirty-state model

Current dirty states include:
- `Clean`
- `DirtyUnscheduled`
- `DirtyQueued`
- `DirtyInFlight`
- `DirtyFlushedPendingFsync`
- `DirtyFailed`

`DirtyFlushedPendingFsync` is explicitly a forced-write durability stage, not WAL authority.

## Domain budgeting

The current configuration model carries per-domain min, target, and max percentages and frame counts. Commercial-grade memory canon must preserve those budgets when adding resident index or accelerator domains rather than collapsing everything into generic cache.

## Residency-class binding

Beta 1 residency classes are:

| Residency class | Default policy domain | Notes |
| --- | --- | --- |
| persistent table or index page | domain-specific persistent policy | MGA-visible durable state |
| temp operator page | `TemporaryWork` | spillable and reclaimable |
| temp table page | `TemporaryWork` | statement or session scoped |
| resident-index page group | domain-specific resident-index policy | optional warm state unless owning spec says otherwise |

## Sample admission flow

```cpp
FrameLease admitFrame(PageResidencyRequest req) {
  auto domainBudget = policy.resolveBudget(req.policy_domain);
  if (!domainBudget.canAdmit(req.frame_count)) {
    runPressureProtocol(*req.memory_node, *req.memory_node, req.bytes, req.memory_class);
  }
  return frameArray.lease(req);
}
```
