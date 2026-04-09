# Master Tracker

| Ticket | Title | Status | Depends On | Primary Evidence |
| --- | --- | --- | --- | --- |
| B1-08-001 | Specification sufficiency closure | completed | - | updated assigned specs, gap closure notes, audit matrix |
| B1-08-002 | Ownership and audit anchor normalization | completed | B1-08-001 | current audit matrix, canonical spec audit anchors |
| B1-08-003 | Tooling drivers and admin-surface closure | completed | B1-08-002 | code, spec, and evidence updates for lane A |
| B1-08-004 | Beta 1 optimization, benchmarks, gates, and release closure | active | B1-08-003 | ordered optimization backlog, clean gate replay logs, crash regression, benchmark throughput findings, performance remediation plan |
| B1-08-005 | Gates, benchmarks, and evidence closure | queued | B1-08-003, B1-08-004 | gate rerun artifacts, benchmark rerun artifacts, section 31 closure |
| B1-08-006 | Final closeout | queued | B1-08-005 | move-ready package |

## Expanded B1-08-004 Phase Order

| Order | Phase | Scope | Rationale |
| --- | --- | --- | --- |
| 1 | Exact-family write-path closure | close same-key suppression, cleanup debt, delta buffering, commit-grouping, and hot-leaf behavior on the active maintained write path | ingest, build publication, and metrics invalidation all depend on a stable write substrate |
| 2 | Bulk-ingest lane closure | close `RETAIL_MICRO_BATCH`, `SORTED_EXACT_BULK`, and `SHADOW_LOAD_CUTOVER` plus durable `bulk_load_*` state | the release benchmark load path depends directly on canonical ingest lanes |
| 3 | Online build and heavy-family publication | close `index_build_*`, cutover guards, visibility states, and immutable heavy-family publication | metrics freshness depends on explicit build, publish, retire, and invalidate events |
| 4 | Family-native metrics and freshness | close native packet publication, freshness classes, invalidation reasons, publication epochs, and refresh triggers | planner parity must consume explicit metrics state rather than implicit assumptions |
| 5 | Memory residency and operator grants | close buffer-pool domain budgeting, operator grant feedback, and bounded spill admission or refusal behavior | final operator tuning should not target a missing admission layer |
| 6 | Planner parity and refusal-model closure | close complete candidate enumeration, no-secondary-family behavior, winner-or-refusal coverage, access-path ordering, and join enumeration | this phase depends on explicit metrics and memory state from the earlier phases |
| 7 | Query-runtime and analytical closure | close remaining benchmark-visible runtime gaps, including analytical and late-materialization closure where canon requires it | planner-admitted winners must map to real runtime execution |
| 8 | Rerun gates and evidence | rerun the package gate and benchmark matrix and preserve updated Beta 1 proof | package `08` cannot close without regenerated end-to-end evidence |
