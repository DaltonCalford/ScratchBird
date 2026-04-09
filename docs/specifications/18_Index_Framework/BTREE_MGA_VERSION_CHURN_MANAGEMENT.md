# B-tree MGA Version-Churn Management

## Purpose
Define visibility-aware page classification, dead-version density accounting,
and compact-before-split policy for MGA-heavy B-tree workloads.

## Scope
- dead entry density and reclaimability scoring
- split-before-GC versus compact-before-split decisions
- page classes for MGA churn pressure
- cost and metric hooks

## Hard Invariants
1. Visibility truth still comes from record and transaction state, not from page
   class labels.
2. Compact-before-split is advisory only when reclaimability cannot be proven.
3. Dead-version density must be observable per page and per index.

## Page Classes
Required B-tree page classes:
- `LIVE_HOT`
- `DUP_PRESSURE`
- `GC_CANDIDATE`
- `SPLIT_RISK`

## Reclaimability Score
Each leaf page computes:
- `dead_version_density`
- `reclaimable_bytes`
- `duplicate_pressure`
- `split_risk`
- `scan_pressure`

These values combine into a page-local reclaimability score used before split.

## Compact-Before-Split Policy
Before splitting a leaf page, the engine must check:
1. whether reclaimable bytes exceed the incoming entry need plus reserve
2. whether no blocking snapshot or duplicate continuation constraint prevents
   cleanup
3. whether compaction cost is lower than split cost

If all conditions hold, compact before split and count the event.

## Metrics and Optimizer Hooks
Expose:
- dead-version density
- compact-before-split count
- MGA churn bytes
- split avoided by cleanup

Planner costing may consume these metrics only through canonical section-18 and
section-20 surfaces.

## Acceptance Criteria
- MGA-heavy workloads reduce unnecessary split rate
- bloat from dead versions is visible and bounded
- page-class decisions are deterministic for fixed inputs

## Cross-Section References
- `BTREE_DUPLICATE_KEY_AND_POSTING_LIST_MANAGEMENT.md`
- `INDEX_METRICS_AND_COSTING.md`
- `../10_GC_and_Sweep/GC_SWEEP_ALGORITHM.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/INDEX_GC_PROTOCOL.md` | dead-entry lifecycle extended into page-local split/compact policy |

## Gap Closure Mapping
- `SB-BTR-007`
