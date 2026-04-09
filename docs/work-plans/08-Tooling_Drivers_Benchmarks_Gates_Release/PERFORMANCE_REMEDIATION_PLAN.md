# Performance Remediation Plan

Status: active_package_expansion

Owning tickets:
- B1-08-004
- B1-08-005

Detailed expansion:
- NON_BETA2_PERFORMANCE_CLOSURE_WORKPLAN.md

## Purpose

This plan converts the current benchmark-throughput failure into an explicit
Beta 1 remediation program. The original blocker was insert throughput, but the
required closure is now broader: package `08` owns the release benchmark
surface, and that surface is still blocked by a combined write, ingest,
optimizer, and query-path performance backlog. Every major user-visible action
must therefore have a repeatable measurement path, a bounded diagnosis
workflow, and an evidence-based improvement cycle before package `08` can
close.

## Beta 1 Blocking Statement

Package `08` may not close while the bounded ScratchBird release benchmark
surface lacks a clean preserved proof set on the current Beta 1 hardware and
gate shape. The original benchmark blocker was operational impracticality on
the default runtime. That bounded blocker is now closed on the active branch,
but package `08` still owns the release aggregate rerun plus any residual
competitiveness and observability limits that must be documented before Beta 1
closeout.

## Current Measured Findings

### Confirmed now

- Direct `sb_isql` imports reproduce the same slowdown as the Python benchmark
  harness, so the issue is server-side.
- Heap tuple placement itself is cheap; the dominant bounded storage cost is
  repeated row-by-row free-space discovery in `StorageEngine::findFreePage()`.
- Stress-shaped rows add a second large bounded cost from per-row unique insert
  preflight.
- Large multi-row statements still carry additional runtime above traced
  storage work, so the full problem is split across storage-path cost and
  higher-layer statement overhead.
- Stable bounded stress replays now make it past the original raw insert
  throughput defect and into later benchmark stages.
- The corrected implicit buffer-pool budget now makes the default runtime
  practical again on the bounded `small` stress shape.
- Clean no-override proof on the active branch:
  total load `36.82 s` and `self_join_same_country = 294.71 ms` on the focused
  rerun; full-suite rerun with all `15/15` tests passing, total load
  `46.90 s`, total measured test time `46.41 s`,
  `self_join_same_country = 350.28 ms`, and
  `bulk_update_with_case = 1324.10 ms`.
- The remaining gap is no longer a timeout blocker. It is now bounded to
  first-run competitiveness against donor engines plus the missing live SQL
  observability views on the benchmark runtime path.
- The first exact-family section `18` maintained-write closure slice is now
  landed on the active branch:
  same-key stable-TID updates no longer bypass maintained-index observability,
  active online-maintenance rows now receive `UPDATE` deltas for those
  unchanged-key rewrites, and unique-exact conflicts now increment durable
  `index_contention.unique_key_conflict_count`.

### Measured examples

- Heap-only `256`-row batch:
  `2673.453 ms` end-to-end; `2523.284 ms` traced storage time;
  `2490.146 ms` in `findFreePage()`.
- `customers`-like `128`-row batch with `PRIMARY KEY` and `UNIQUE(email)`:
  `10006.994 ms` end-to-end; `4228.408 ms` traced storage time;
  `2859.275 ms` uniqueness preflight; `1320.568 ms` free-page search.

## Success Standard

This blocker is not closed by one isolated fix. It closes only when all of the
following are true:

1. The expanded Beta 1 optimization authorities consumed by this package are
   closed in dependency order, not bypassed by local benchmark-only patches.
2. The primary action families below have repeatable baselines and phase-level
   instrumentation.
3. Each major action family has a bounded optimization backlog ordered by user
   impact and Beta 1 gate relevance.
4. The bounded package `08` gate and benchmark surface rerun green with
   preserved evidence.
5. Residual limits, if any, are explicitly documented and no longer block Beta
   1 release-readiness.

## Expanded Canonical Authorities

The package now explicitly consumes these cross-section optimization
authorities:

- section `12` temporary-work and spill boundary
- section `18` DML and index-maintenance optimization
- section `23` access-path ordering and join search
- section `33` residency, grants, and pressure policy
- section `34` columnstore analytical and predicate-pruning boundary
- section `36` optimizer parity, metrics freshness, and refusal explanation
- section `37` metrics publication and online change cutover
- section `39` bulk ingest and shadow-load cutover

## Ordered Beta 1 Execution Program

Package `08` shall execute the remaining optimization backlog in this order.
The sequence is dependency ordered, not merely severity ordered.

### Phase 0: Measurement and correctness guardrails

Deliverables:
- keep focused phase-level timing for client, statement, executor, and storage
  layers
- preserve bounded reproductions for load, join, and explain paths
- convert any newly discovered correctness faults under sustained benchmark
  load into focused reproductions before broader optimization work proceeds

Exit criteria:
- the active hot paths are measurable without relying on the full benchmark
  harness
- correctness failures are isolated before performance tuning continues

### Phase 1: Exact-family write path and primary-tablespace correctness

Deliverables:
- close the remaining exact-family Beta 1 write-path model from section `18`
- land the still-missing same-key suppression, commit-group, hot-leaf, and
  cleanup-debt behavior needed by the active runtime families
- eliminate any sustained-load primary-tablespace correctness faults that still
  appear after the earlier insert-throughput fixes

Rationale:
- bulk ingest, online build, and metrics invalidation all depend on a stable
  exact-family write substrate

Exit criteria:
- write-path correctness and throughput are stable under the bounded benchmark
  loads
- the exact-family Beta 1 write-path backlog is materially reduced to
  non-blocking residuals

Current phase status:
- phases `1` through `5` are now closed on bounded package proof.
- active frontier: phase `6` planner parity and refusal-model closure.
- landed:
  exact same-key update observability for maintained indexes
  and durable unique-conflict / hot-key contention accounting on the active
  write path, with focused green contracts;
  bounded hot-right-edge reserve and presplit mitigation on monotonic
  rightmost B-tree inserts, with focused regression proof;
  reclaim-driven exact cleanup compaction on B-tree leaf pages when reclaimable
  bytes cross the section `18` threshold, with focused regression proof;
  residual exact-cleanup backlog publication on the B-tree GC path so the
  engine now exposes backlog pages and bytes after exact cleanup runs even when
  the local reclaim proof does not cross the compaction threshold;
  bounded durable cleanup-debt ledger publication through
  `index_health`, with runtime publication sync and SQL cleanup-debt fallback
  proof after restart or runtime publication loss;
  commit-group batch apply for the active exact online-maintenance delta path,
  with per-xid queueing under `GROUP_COMMIT`, ordered materialization before
  terminal TIP publication, rollback discard for aborted xids, and fail-closed
  cleanup of prepared delta rows when the fenced commit batch does not complete,
  verified by the focused `16/16` green proof set covering
  `GroupCommitTest.*`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesInsertDelta`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesSameKeyUpdateDelta`,
  `StorageEngineTest.GroupCommitQueuesOnlineMaintenanceDeltaUntilCommit`,
  and
  `StorageEngineTest.GroupCommitRollbackDiscardsQueuedOnlineMaintenanceDelta`;
  durable `index_page_delta` catalog substrate for the next cold-page buffering
  rung, with catalog root allocation, CRUD/list/delete contracts, full extract
  and family-matrix wiring, and focused `4/4` proof in
  `CatalogDatabaseBootstrapTest.CreatesIndexMetadataExtensionCatalogFamilyPages`,
  `CatalogFamilyMatrixContractTest.*`,
  `CatalogFullExtractTest.*`,
  and
  `CatalogIndexMetadataExtensionContractTest.OptionAndMaintenanceDeltaContracts`;
  bounded runtime narrow cold-page exact-secondary buffering on the active
  insert path, with durable `index_page_delta` persistence, synchronous merge
  before exact reads and write-side exact maintenance, and a reentrancy guard
  so TOAST-backed delta metadata publication falls back to the normal exact
  insert path instead of recursively deferring itself, verified by focused
  `6/6` green proof in
  `CatalogDatabaseBootstrapTest.CreatesIndexMetadataExtensionCatalogFamilyPages`,
  `CatalogIndexMetadataExtensionContractTest.OptionAndMaintenanceDeltaContracts`,
  `StorageEngineTest.GroupCommitQueuesOnlineMaintenanceDeltaUntilCommit`,
  `StorageEngineTest.GroupCommitRollbackDiscardsQueuedOnlineMaintenanceDelta`,
  `StorageEngineTest.ColdExactSecondaryInsertDeltaMergesOnRead`,
  and
  `StorageEngineTest.ColdExactSecondaryDeferralSkipsUniqueIndexes`;
  bounded scheduler-owned merge for that same cold exact-secondary path, with
  background GC draining deferred `index_page_delta` rows in a per-pass lane,
  per-index in-flight ownership to prevent duplicate foreground/background
  merge work, same-thread reentry bypass so TOAST cleanup does not self-deadlock
  through recursive index seeks, and background-path XID resolution through
  `StorageEngine::getCurrentXid()` so committed delta payloads remain visible
  when no connection context exists, verified by focused `5/5` green proof in
  `StorageEngineTest.GroupCommitQueuesOnlineMaintenanceDeltaUntilCommit`,
  `StorageEngineTest.GroupCommitRollbackDiscardsQueuedOnlineMaintenanceDelta`,
  `StorageEngineTest.ColdExactSecondaryInsertDeltaMergesOnRead`,
  `StorageEngineTest.BackgroundGcMergesDeferredExactSecondaryDeltasWithoutForegroundRead`,
  and
  `StorageEngineTest.ColdExactSecondaryDeferralSkipsUniqueIndexes`
- still open:
  hot-leaf mitigation beyond the first right-edge reserve rung,
  and broader scheduler-grade debt routing beyond the bounded per-index ledger
  and the current bounded per-pass background exact-delta merge bridge

### Phase 2: Bulk ingest lanes and load-path closure

Deliverables:
- close `RETAIL_MICRO_BATCH` for benchmark-driven additive loads
- implement `SORTED_EXACT_BULK` where the lane-selection rules require it
- implement `SHADOW_LOAD_CUTOVER` and its durable `bulk_load_*` plan, event,
  progress, and cutover-guard records

Current bounded substrate status:
- landed:
  reusable `StorageEngine` bulk insert handle with pinned writable-page reuse
  and shared post-insert maintenance on the active load path, with the
  executor `COPY FROM` row-insert leg routed through that handle and focused
  `2/2` green proof in
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`
  and
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`
- still open:
  canonical bulk lane selection and all durable `bulk_load_*` records, plus
  explicit sorted-bulk and shadow-cutover execution paths

Rationale:
- the release benchmark load path depends directly on this lane, and it builds
  on the exact-family substrate from phase `1`

Exit criteria:
- large bounded benchmark loads no longer run exclusively through the ordinary
  retail path when canon requires a bulk lane
- load-path correctness and throughput are practical for the package benchmark
  shape

### Phase 3: Online build, heavy-family publication, and visibility-state closure

Deliverables:
- close the durable `index_build_*` rows and cutover-guard semantics
- close admitted index visibility states and their publish or retirement rules
- close heavy-family pending-lane, immutable-generation, and manifest-based
  publication where Beta 1 canon now requires them

Rationale:
- bulk load and online DDL share the same cutover, resume, and retirement
  semantics, and later metrics freshness depends on these events being explicit

Exit criteria:
- online build and heavy-family publication no longer rely on partial or
  implicit state
- metrics invalidation has concrete build and publication events to observe

### Phase 4: Metrics freshness, invalidation, and optimizer parity

Deliverables:
- close the published family-metrics freshness and invalidation model
- close the no-secondary-family and explicit-refusal obligations
- close planner consumption of family metrics, publication epochs, and staleness
  penalties using the canonical packet model

Rationale:
- optimizer parity depends on real maintenance, build, and invalidation events
  from phases `1` through `3`

Exit criteria:
- legal implemented families no longer disappear silently
- candidate bundles preserve explicit refusal classes and metrics freshness
  state

### Phase 5: Memory grant, residency, and spill admission

Deliverables:
- close the Beta 1 operator-grant feedback and reservation model from section
  `33`
- bind query execution to real admission and spill-first policy rather than
  planner-only estimates
- preserve section `12` boundaries by refusing unsupported spill semantics
  instead of inventing them

Rationale:
- broader join, aggregate, and sort optimization should not be tuned against a
  nonexistent runtime admission layer

Exit criteria:
- the engine has bounded operator admission behavior and feedback-driven sizing
- query-path refusals can name memory or spill admission explicitly when it is
  the real reason

### Phase 6: Query-path operator optimization and benchmark hot-path closure

Deliverables:
- close the remaining bounded self-join benchmark blocker
- extend the current stable V3 work into broader join, aggregate, sort,
  projection, covering, and late-materialization closures where canon requires
  them
- close winner-or-refusal explanation coverage for the benchmark-visible query
  families

Rationale:
- this is the current bounded read-phase blocker, but it should be tuned only
  after the write, ingest, metrics, and memory substrates above are explicit

Exit criteria:
- the active bounded query benchmark scenarios run inside a practical Beta 1
  envelope
- explain or trace outputs can account for winner selection and refusal

### Phase 7: Full rerun and release evidence closure

Deliverables:
- rerun the bounded package gate and benchmark matrix from clean state
- preserve focused before or after evidence for every major optimization phase
- update section `31` and package evidence so the expanded backlog is visibly
  closed

Exit criteria:
- package `08` owns a clean Beta 1 proof set for the expanded optimization lane
- B1-08-005 can close without unresolved optimization ambiguity

## Current Closure State

- The original bounded default-runtime stress blocker is closed.
- That closure did not close the broader non-Beta2 performance canon consumed
  by package `08`.
- The package is therefore expanded again under
  NON_BETA2_PERFORMANCE_CLOSURE_WORKPLAN.md:
  write-path, ingest, publication, metrics, memory, planner, and analytical
  runtime work must all be closed before the final section `31` rerun can be
  treated as authoritative Beta 1 proof.

## Action Inventory

The analysis and optimization program must cover these action families:

1. Session lifecycle:
   connect, authenticate, attach database, disconnect, shutdown.
2. Transaction lifecycle:
   begin, commit, rollback, savepoint, reopen after failure.
3. Write path:
   singleton insert, batched insert, SQL-script import, indexed insert,
   update, delete, mixed OLTP write bursts.
4. Read path:
   point lookup, range scan, join, aggregate, sort, temp/spill, explain/plan.
5. DDL and maintenance:
   create table, alter table, create index, rebuild index, statistics refresh.
6. Recovery and operational paths:
   startup, open database, checkpoint, crash recovery, backup, restore.
7. Front-door and tooling surfaces:
   native protocol, `sb_isql`, driver `executemany()`, admin SQL, benchmark
   harness orchestration.

## Required Measurement Model

Every action family above needs both end-to-end timing and phase-level timing.
The mandatory measurement layers are:

1. Client-visible wall clock:
   connect, execute, fetch, commit, disconnect.
2. Statement/executor phases:
   parse, bind, plan selection, executor entry, row production, result encode.
3. Storage phases:
   catalog lookup, constraint preflight, free-space search, page pin/unpin,
   heap placement, index maintenance, durable publication.
4. Runtime counters:
   pages scanned, heap pages examined, pins/unpins, page allocations, reads,
   writes, temp spills, lock waits, retries, protocol round trips.
5. Resource sampling:
   CPU time, memory growth, file size growth, I/O amplification, syscall hot
   spots where relevant.

## Analysis Workflow

Apply the same workflow to each action family:

1. Reproduce the slow path with the smallest stable workload that still shows
   the issue.
2. Split client-visible time into statement/executor/storage phases.
3. Confirm whether the cost is per statement, per row, per page, per index, or
   per transaction.
4. Remove benchmark-wrapper ambiguity by replaying the same action through at
   least two front doors when possible:
   driver path and `sb_isql`, or benchmark harness and focused native test.
5. Produce before/after evidence for every fix:
   micro probe, bounded gate replay, and package-level benchmark replay.

## Immediate Write-Path Remediation

### Phase 0A: Stabilize measurement

Deliverables:
- Keep guarded insert tracing available in the engine.
- Add statement-level timing above `StorageEngine` so the remaining
  multi-row-statement overhead is measured directly.
- Capture commit publication timing separately from statement execution timing.
- Preserve small reproducible probes for:
  heap-only rows, indexed rows, stress-shaped rows, and SQL-script import.

Exit criteria:
- each insert probe reports stable end-to-end and phase-level numbers
- the remaining non-storage overhead is bounded to a concrete layer

### Phase 1A: Remove linear free-space search from the hot path

Deliverables:
- add a per-table append/free-page hint so repeated inserts do not scan from
  `heap_scan_start_page` on every row
- keep the last successful writable page hot for the current table and current
  write burst until the page is actually full
- avoid re-walking unrelated heap pages after every successful insert

Expected impact:
- collapse the current `O(total_heap_pages)` per-row search into an amortized
  near-constant append/write path for steady inserts

Exit criteria:
- heap-only insert traces show `findFreePage()` is no longer the dominant cost
- warm per-row insert time falls materially in both singleton and batched probes

Progress 2026-04-01:
- implemented: per-table writable-page hint in `StorageEngine`
- implemented: unique-preflight metadata cache so repeated rows do not rebuild
  the same index lookup plan
- result: the heap-only `256`-row probe dropped from `2673.453 ms` to
  roughly `183-192 ms`

### Phase 1B: Fix uniqueness preflight cost

Deliverables:
- measure the exact work inside `preflightUniqueInsert()`
- ensure uniqueness checks are targeted index lookups, not broader scans
- add batch-aware duplicate detection so one multi-row statement does not redo
  avoidable cross-check work row by row
- distinguish intra-batch duplicate checks from persistent-index duplicate checks

Expected impact:
- indexed insert cost scales with actual unique-key work, not repeated generic
  preflight across the same batch

Exit criteria:
- stress-shaped indexed insert probes show uniqueness preflight no longer
  dominates the row cost

Progress 2026-04-01:
- implemented: executor-side duplicate checks now skip ordinary storage-backed
  `BTREE` and `HASH` unique indexes in both insert execution paths
- implemented: the active V3/native insert path now reuses cached index,
  constraint, foreign-key, and trigger catalog rows instead of reloading them
  row by row
- result: the `128`-row `customers`-like probe fell from `10006.994 ms`
  to `197.577 ms` execute time, with traced storage work down to
  `61.712 ms` total across the batch

### Phase 1C: Reduce multi-row statement overhead above storage

Deliverables:
- instrument parse, bind, execution setup, and result handling for large
  multi-row `INSERT ... VALUES (...)` statements
- compare multi-row insert, repeated single-row prepared insert, and a dedicated
  internal bulk-insert path
- decide whether Beta 1 needs a dedicated bulk-import or copy-like path for
  release benchmarks

Exit criteria:
- the remaining gap between traced storage work and end-to-end batch runtime is
  explained and either fixed or bounded by an intentional Beta 1 policy

Progress 2026-04-01:
- still open, but materially reduced. The post-fix `customers`-like batch now
  spends about `197.577 ms` in execute time with only `61.712 ms` traced in
  storage, which is a bounded residual rather than the former multi-second
  engine defect.
- The fresh `stress --scale small` replay is no longer blocked by raw insert
  speed, but it now fails during `orders` load with
  `Page number 174096499359744 exceeds uint32_t maximum for primary tablespace`.
  Phase `3` therefore now includes isolating whether the residual large-load
  failure is caused by `GPID` packing, page-number corruption, or a later
  protocol/result decode path after the first storage error.

### Phase 1D: Structural write-path improvements

Deliverables:
- design and land a real free-space tracking model for heap relations
- evaluate whether table-local extents or relation page inventories are needed
- cache relation and TOAST lookup state at the statement or transaction scope
  where safe

Exit criteria:
- write throughput no longer depends on file age or unrelated heap growth

## Broader Performance Program For All Actions

### Write-path family after inserts

- update:
  version-chain creation, same-page update cost, index maintenance, reclaim debt
- delete:
  tombstone publication, index cleanup cost, sweep interaction
- mixed OLTP:
  contention, lock waits, pin churn, commit publication

### Read/query family

- point lookups:
  heap versus index path, visibility checks, plan quality
- range scans:
  index traversal, heap fetch amplification, readahead effectiveness
- joins and aggregates:
  operator timing, temp space, sort/hash memory, spill thresholds
- explain and plan capture:
  plan generation cost and plan introspection reliability for benchmarks

### Session and transaction family

- attach/auth latency by method
- transaction begin/commit/rollback/savepoint timing
- reconnect and fail-closed recovery latency

### DDL and maintenance family

- create or alter table cost
- create or rebuild index cost by family
- statistics refresh, metadata publication, and cache invalidation timing

### Recovery and operational family

- startup and open-database latency
- checkpoint cost and cadence
- crash recovery replay time
- backup and restore throughput

## Required Evidence Per Action Family

Each action family needs:

1. a focused micro-benchmark or regression probe
2. one preserved log or metrics artifact with phase-level timing
3. one bounded gate or benchmark replay showing the fix survives a realistic run
4. an updated package note if a residual limit remains

## Ticket Integration

### B1-08-004

Must produce:
- insert-path diagnosis closure
- first corrective engine changes
- before/after focused evidence
- updated blocker notes and package risk log

### B1-08-005

Must produce:
- rerun of the bounded package gate and benchmark surface
- broader action-family baselines where this package owns the release evidence
- updated section `31` evidence and Beta 1 release-readiness artifacts

## Immediate Next Engineering Steps

1. Finish phase `1` by isolating and correcting any remaining sustained-load
   primary-tablespace fault or exact-family write defect under the bounded load
   replay.
2. Move directly into phase `2` so the package benchmark load lane no longer
   depends only on the retail path when canon requires bulk behavior.
3. Preserve before or after evidence after each phase transition rather than
   waiting for one giant end-of-ticket rerun.
4. Keep the current bounded self-join hot path under focused measurement, but do
   not declare the broader query-path lane closed until phases `4` through `6`
   are complete.
5. Use the new bounded hot-leaf counters as the phase-`1` observability anchor:
   `sb_storage_hot_leaf_detections_total`,
   `sb_storage_hot_leaf_presplits_total`,
   and
   `sb_storage_hot_leaf_split_retries_total`
   now expose right-edge B-tree pressure at the canonical `db` and `relation`
   label set.
6. Keep the new direct post-split insert path on the monotonic hot-right-edge
   trigger insert:
   the bounded phase-`1` B-tree path now inserts straight into the freshly
   created right leaf after a successful presplit instead of always yielding and
   retrying from the root. The follow-on cleanup now lets
   `split_leaf_page()` consume that pending row while both split pages are still
   pinned, which removes the extra repin/relock roundtrip on the trigger
   insert, and the focused proof keeps
   `right_edge_split_retries` flat on that trigger insert while preserving the
   existing right-edge detection and presplit counters.
7. Keep the active `COPY FROM` path on explicit canonical
   `RETAIL_MICRO_BATCH` planning and durable retail-lane state:
   `Executor` now resolves the default batch size from
   `sb.bulk.micro_batch_target_rows` with the canonical `64..65536` clamp,
   preserves explicit `BATCH_SIZE` overrides when present, logs the chosen lane
   as `RETAIL_MICRO_BATCH` on both live `COPY` executor paths, and publishes
   durable `bulk_load_plan`, `bulk_load_event`, `bulk_load_progress`, and
   `bulk_load_cutover_guard` rows on both live `COPY FROM` retail paths.
8. Treat the newly landed retail durable-state path as the bounded proof seam
   for phase `2` lane closure:
   focused verification now proves the config-driven default path, the explicit
   override path, and the active retail-lane publication path before the
   package moves on to real selection between `RETAIL_MICRO_BATCH`,
   `SORTED_EXACT_BULK`, and `SHADOW_LOAD_CUTOVER`.
9. Preserve the validation-build process limit in package records:
   targeted rebuilds can fail at GoogleTest discovery with `text file is busy`
   when a stale `scratchbird_tests` holder from an earlier session keeps the
   binary open. This is an execution-hygiene limit, not a product defect, and
   it must be cleared before targeted proof runs are treated as authoritative.
10. Land bounded `SORTED_EXACT_BULK` selection on the active `COPY FROM`
    surfaces:
    both live `COPY FROM` handlers now classify file-backed loads against the
    canonical `sb.bulk.sorted_exact_min_rows` threshold and active exact-key
    candidates instead of hard-coding `RETAIL_MICRO_BATCH`.
11. Keep sorted execution claims honest while advancing the active path:
    the V3 `COPY FROM` lane now stages file-backed `TEXT` and `CSV` rows,
    derives canonical exact-key bytes from the admitted B-tree candidate, sorts
    them before bulk flush, and publishes `SORTED_EXACT_BULK` when the bounded
    threshold is met. The shared non-V3 path now classifies and publishes the
    selected lane, but the remaining section `39` gap is still real
    `SHADOW_LOAD_CUTOVER` execution and branch-isolated proof for non-V3
    sorted/shadow behavior.
12. Make the remaining section `39` gap explicit and user-reachable:
    `COPY ... WITH (SHADOW_LOAD true)` now reaches the canonical
    `SHADOW_LOAD_CUTOVER` lane on the active V3 path, publishes a durable
    `bulk_load_plan` row with that lane, writes a `BLOCKED` cutover guard, and
    fails closed with an `ABORTED_FAIL_CLOSED` bulk-load event instead of
    silently falling back to retail or sorted bulk behavior. This closes the
    unreachable-lane gap, but the real object-level shadow target creation and
    commit-bound cutover execution are still open.
13. Keep the durable bulk-load plan row truthful on the live COPY paths:
    `CatalogManager` now has a narrow `bulk_load_plan.phase_state` update path,
    and both live `COPY FROM` handlers advance that durable row through
    `RUNNING`, `COMPLETED`, or `ABORTED_FAIL_CLOSED` instead of leaving the
    plan record stuck at `PLANNED` after execution. Focused proof is green on
    retail, sorted exact, shadow fail-closed, and the reusable bulk-handle
    storage path.
14. Keep the branch-isolated non-V3 COPY proof green and honest:
    the PostgreSQL parser-emitted fixed-format `COPY` stream now accepts the
    same bounded boolean option syntax used by the active path, carries
    `SHADOW_LOAD`, and emits the trailing `BATCH_SIZE`, `MAX_ERRORS`, and
    `ON_ERROR` defaults that the shared executor compatibility path already
    expects. Focused proof is now green across both new legacy PostgreSQL COPY
    parity tests plus the existing retail, sorted, shadow fail-closed, and
    bulk-handle regressions. The remaining section `39` gap is now real
    object-level `SHADOW_LOAD_CUTOVER` execution and publication, not legacy
    branch proof coverage.
15. Close the bounded object-level shadow-load cutover defect honestly:
    real `SHADOW_LOAD_CUTOVER` execution is now green on both live `COPY FROM`
    paths, and the blocking runtime defect was not lane planning but heap
    tuple identity on non-primary pages. `HeapPage` had been stamping tuple
    `ctid_gpid` values against the primary tablespace even when the owning page
    lived in a shadow tablespace, which left shadow-loaded rows physically
    present but invisible to ordinary scans after cutover. The heap substrate
    now carries the page tablespace through tuple identity stamping and
    traversal, the active non-primary insert/scan/delete paths pass that
    tablespace explicitly, and focused proof is green on the committed-cutover
    shadow tests plus the broader `COPY`/bulk-handle regression slice.
    This closes the bounded section `39` `COPY` lane closure owned here. The
    full expanded package `08` workplan is still open on later phases:
    online build/publication, metrics freshness, memory admission, planner
    parity, and analytical/runtime closure.
16. Land bounded phase `3` durable `index_build_*` publication on the existing
    shadow-index path and prove it survives reopen:
    `CatalogManager` now persists `index_build_plan`, `index_build_event`,
    `index_build_progress`, and `index_build_cutover_guard` rows for the
    current shadow rebuild flow, stores their root page ids in the catalog
    root, and publishes `DRAFTED -> BUILDING -> CUTOVER_PENDING -> PUBLISHED`
    state on create/promote. Focused integration proof is green across
    shadow creation, promotion, and reopen-backed durable state.
17. Keep the shadow-index publication proof honest about the actual blocker:
    the first reopen attempt did not fail on the new `index_build_*` catalog
    pages. It failed because generic resolver rebuild treated all physical
    index versions as user-visible named objects and tripped
    `Duplicate object name in scope` once a `BUILDING` shadow existed beside
    its active predecessor. Resolver rebuild now admits only the published
    `ACTIVE` index version into generic object-name resolution while
    version-aware index visibility remains on `index_cache_`. This bounded
    fix closes the reopen regression without overstating later phase-`3`
    visibility-state work that is still open.
18. Re-run the comparable transaction-aware stress lane from a clean rebuild
    and keep the outcome explicit:
    the fresh artifact root is
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/clean-rebuild-scratchbird-stress-20260404T023343Z`,
    produced after a clean `Release` reconfigure/rebuild of the engine tree on
    April 3, 2026 and a fresh `sb_isql` CLI rebuild in the
    `ScratchBird-driver` lane for benchmark runtime bring-up. Compared with the
    preserved comparable baseline in
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/txmode-matrix-20260403T152011Z/scratchbird/stress`,
    load time improved in both supported ScratchBird modes
    (`36.96s -> 34.99s` normal transactional, `46.51s -> 41.63s`
    autocommit). Query-path impact remained mixed: `self_join_same_country`,
    `inner_join_large_result`, and `bulk_update_with_join` improved in both
    modes, `bulk_update_with_case` improved slightly in normal transactional
    but regressed in autocommit, and `bulk_insert_select` regressed in both
    modes (`14.61s -> 15.29s` normal transactional, `19.26s -> 21.26s`
    autocommit). This clean rerun shows the recent write-path and bulk-lane
    work did not regress the bounded load surface overall, but it does not yet
    close the later query/runtime phases that still dominate benchmark
    competitiveness.
19. Land the first bounded phase `4` metrics-freshness slice and keep the
    semantics explicit:
    `IndexFamilyMetricsPacket`, runtime-plan relations, access-path
    descriptors, and optimizer signatures now carry
    `metrics_publication_epoch`, `metrics_freshness_class`,
    `metrics_invalidation_state`, and `metrics_invalidation_reason`, and
    `StatisticsManager` now publishes those fields from the canonical shared
    family-metrics envelope with backward-compatible derivation for older
    payloads. The planner now consumes those fields directly: `UNUSABLE` or
    hard-invalidated family metrics force the path to an invalid/refused state,
    `STALE_DEGRADED` carries a stronger penalty than `AGED`, and the chosen
    runtime relation preserves the published freshness/invalidation state.
    Focused proof is green with `3/3` tests:
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    This is not full phase-`4` closure yet: explicit refusal bundles and
    wider winner-or-refusal parity across all implemented families are still
    open.
20. Land the first bounded explicit-refusal bundle slice and keep the
    classifications honest:
    `RuntimePlanRelation` now preserves structured
    `candidate_family_refusals` entries with family, candidate label,
    refusal class, cause domain, reason code, and detail, and the
    base-relation candidate bundle now carries those structured refusals
    forward into the chosen runtime relation instead of dropping them once the
    winner falls back to `SEQ_SCAN`. The maintenance-state fail-closed path
    now records the full lifecycle snapshot in refusal detail, so
    hard-invalidated or `UNUSABLE` family metrics classify canonically as
    `unusable metrics` instead of as an opaque policy fallback. Focused proof
    is green on the planner/statistics slice with `3/3` passing tests:
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    The standalone shadow-index integration binary is also still green with
    `8/8` passing tests, including
    `ShadowIndexRebuildTest.BasicShadowCreation`,
    `ShadowIndexRebuildTest.ShadowPromotion`,
    and
    `ShadowIndexRebuildTest.PublishesDurableIndexBuildCatalogState`.
    This closes only the first structured-refusal preservation rung. Full
    phase-`4`/phase-`6` closure still requires implemented-family-wide
    winner-or-refusal parity, explicit refusal publication across all family
    paths, and the later replan-trigger work.
21. Close the next bounded optimizer-parity gap by preserving MGA governance
    refusals in the structured candidate bundle:
    MGA churn/governance rejections no longer exist only as
    `MGA_SWITCHOVER` trace rows. The base-relation candidate bundle now
    records those rejections as structured `candidate_family_refusals`
    with reason code `P08_MGA_GOVERNANCE_REJECTED`, while preserving the
    original `MGA_SWITCHOVER` rejection phase in runtime trace. Focused proof
    is green with `5/5` tests in `scratchbird_tests`:
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    The standalone shadow-index integration target also remains green with
    `8/8` passing tests. This closes one more winner-or-refusal gap, but not
    full phase-`4`/phase-`6` parity: broader implemented-family-wide refusal
    coverage, replan-trigger closure, and memory admission still remain open.
22. Close the next bounded optimizer-parity gap by preserving cost-domain
    family refusals instead of only raw rejected traces:
    implemented-family paths that lose before winner selection because the
    current best path is cheaper, or because family metrics mark an exact path
    structurally overweight, now publish structured
    `candidate_family_refusals` entries instead of only emitting
    `rejected_paths` trace rows. The planner now records
    `P08_HIGHER_TOTAL_COST_THAN_CURRENT_BEST` as
    `higher estimated cost than current winner` with `COST` cause domain, and
    `P08_STRUCTURALLY_OVERWEIGHT_EXACT_PATH` as
    `structurally overweight path under current metrics` with `COST` cause
    domain. Focused proof is green with `6/6` tests in `scratchbird_tests`:
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    This closes another winner-or-refusal rung, but phase-`4`/phase-`6`
    closure still needs broader implemented-family refusal coverage, replan
    triggers, and the later memory-admission work.
23. Close the next bounded optimizer-parity gap by preserving semantic
    mismatch refusals for predicates that do not match any available index:
    when a base relation has indexes but a predicate still has no matching
    index path, the planner now publishes a structured
    `candidate_family_refusals` entry instead of only emitting a raw
    `rejected_paths` trace row. The planner records
    `P08_NO_MATCHING_INDEX_FOR_PREDICATE` as `semantic mismatch` with
    `SEMANTICS` cause domain, and preserves the original predicate text in the
    refusal detail. Focused proof is green with `7/7` tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    This closes another winner-or-refusal rung, but phase-`4`/phase-`6`
    closure still needs broader implemented-family refusal coverage, replan
    triggers, and the later memory-admission work.
24. Close the next bounded optimizer-parity gap by preserving native-family
    promotion-threshold refusals in the structured candidate bundle:
    native-family access paths that are enumerated but rejected because their
    current family metrics do not justify promotion now publish structured
    `candidate_family_refusals` entries instead of only raw `rejected_paths`
    trace rows. The planner records the current threshold family with
    `P08_*_NATIVE_PROMOTION_THRESHOLD_NOT_MET`,
    `P08_ANN_HYBRID_FALLBACK_NOT_JUSTIFIED`, and
    `P08_TEXT_SCORE_ROWS_REQUIRED`, and maps them to
    `family-specific promotion threshold not met` with `METRICS` cause
    domain. Focused proof is green with `8/8` tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    This closes another bounded implemented-family refusal gap, but broader
    family-wide refusal coverage, replan-trigger closure, and the later
    memory-admission work still remain open.
25. Close the next bounded optimizer-parity gap by preserving specific
    generalized/spatial lowering rejection codes instead of collapsing them
    into generic family-not-queryable failures: the planner-family lowering
    helper now distinguishes `P08_OPERATOR_STRATEGY_UNBOUND`,
    `P08_SUPPORT_FUNCTION_UNVALIDATED`,
    `P08_DISTANCE_SUPPORT_UNVALIDATED`, and
    `P08_NEAREST_ORDER_UNVALIDATED`, and the planner maps the generalized
    invalid shapes that reach the bundle to `unsupported operator shape` or
    `missing required runtime capability` instead of a generic policy reject.
    Focused proof is green with `5/5` tests in `scratchbird_tests`:
    `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    and
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`.
    The GIST integration proof now stays honest about the actual planner
    boundary: before validated opclass support is present, the planner still
    fails closed one stage earlier as
    `P08_NO_MATCHING_INDEX_FOR_PREDICATE`; once the bound support function is
    published, `GIST_SCAN` enters the candidate family set. This closes
    another bounded implemented-family refusal gap, but broader family-wide
    refusal coverage, replan-trigger closure, and the later memory-admission
    work still remain open.
26. Close the next bounded optimizer-parity gap on the ANN-family side by
    preserving explicit invalid-lowering reasons instead of collapsing ANN
    invalid states into generic family-not-queryable failures. The
    planner-family lowering helper now distinguishes
    `P08_ANN_NEAREST_ORDER_REQUIRED`,
    `P08_ANN_METRIC_INCOMPATIBLE`, and
    `P08_ANN_CANDIDATE_BUDGET_REQUIRED`, and the planner maps those bounded
    ANN invalid shapes to `missing required runtime capability` instead of a
    generic policy refusal. Focused proof is green with `6/6` tests in
    `scratchbird_tests`:
    `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.AnnInvalidCasesPublishSpecificRejectionCodes`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    and
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`.
    This closes another bounded implemented-family refusal gap, but broader
    family-wide refusal coverage, replan-trigger closure, and the later
    memory-admission work still remain open.
27. Close the next bounded optimizer-parity gap on the ranked-text side by
    preserving explicit invalid-lowering reasons instead of collapsing ranked
    text invalid states into generic family-not-queryable failures. The
    planner-family lowering helper now distinguishes
    `P08_TEXT_CORPUS_STATS_REQUIRED` and
    `P08_TEXT_CANDIDATE_BUDGET_REQUIRED`, and the planner maps those bounded
    ranked-text invalid shapes to `missing required runtime capability`
    instead of a generic policy refusal. Focused proof is green with `7/7`
    tests in `scratchbird_tests`:
    `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.AnnInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.TextInvalidCasesPublishSpecificRejectionCodes`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    and
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`.
    This closes another bounded implemented-family refusal gap, but broader
    family-wide refusal coverage, replan-trigger closure, and the later
    memory-admission work still remain open.
28. Close the next bounded optimizer-parity gap on the hash-family side by
    preserving explicit invalid-lowering reasons instead of collapsing
    non-equality hash predicates into generic family-not-queryable failures.
    The planner-family lowering helper now distinguishes
    `P08_HASH_EQ_PREDICATE_REQUIRED`, and the planner maps that bounded
    hash-family invalid shape to `unsupported operator shape` instead of a
    generic policy refusal. Focused proof is green with `8/8` tests in
    `scratchbird_tests`:
    `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.AnnInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.TextInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.HashInvalidCasesPublishSpecificRejectionCodes`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    and
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`.
    This closes another bounded implemented-family refusal gap, but broader
    family-wide refusal coverage, replan-trigger closure, and the later
    memory-admission work still remain open.
29. Close the next bounded phase-`4` freshness/explain gap by publishing
    dedicated family-native metrics provenance into the runtime plan and
    `EXPLAIN` JSON proof surface. The planner now emits
    `FAMILY_NATIVE_METRICS` statistics provenance rows for published family
    metrics, carrying the selected or rejected family name, freshness class,
    invalidation state, maintenance state, publication epoch, and the bounded
    current-compile policy markers
    `refresh_attempted=false refresh_policy=ASYNC_OR_ADMIN replan_boundary=FAMILY_STATISTICS_SIGNATURE`.
    Focused proof is green with `10/10` tests in `scratchbird_tests`:
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.AnnInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.TextInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.HashInvalidCasesPublishSpecificRejectionCodes`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    and
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`.
    This closes a bounded freshness-provenance rung, but broader family-wide
    refusal coverage, explicit replan-trigger closure, and the later
    memory-admission work still remain open.

30. Close the next bounded phase-`4`/phase-`6` refusal-fidelity rung by
    extracting the planner bundle-refusal classifier into reusable canonical
    helpers and proving the new maintenance/trust mappings directly instead of
    depending on the currently unsupported nearest-neighbor SQL surface inside
    the v3 integration harness. The canonical helper now preserves
    `P08_MAINTENANCE_STATE_INCOMPATIBLE` as
    `maintenance state incompatible with trust class` with `METRICS` cause
    domain, and `P08_TRUST_LOCATOR_UNDECLARED` as
    `missing trust or locator classification` with `POLICY` cause domain.
    Focused proof is green with `11/11` passing tests in `scratchbird_tests`:
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `QueryPlannerIntegrationTest.AgedPublishedFamilyMetricsPreserveMaintenanceStateRefusal`,
    `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.AnnInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.TextInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.HashInvalidCasesPublishSpecificRejectionCodes`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    and
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`.
    During proofing, the attempted ANN integration route failed at parse time
    on the current v3 compiler surface, so the evidence was intentionally kept
    at the canonical helper layer for this bounded slice. Broader family-wide
    refusal coverage, explicit replan-trigger closure, and the later
    memory-admission work still remain open.

31. Close the next bounded phase-`4`/phase-`6` optimizer-parity rung by
    preserving `P08_MGA_GOVERNANCE_REJECTED` as its own structured refusal
    instead of classifying it as generic fail-closed policy. The canonical
    bundle-refusal helpers now classify this path as
    `MGA governance rejected candidate under current pressure` with `POLICY`
    cause domain, and the severe-churn broad covering-index proof now asserts
    that specific class instead of the old generic fallback.
    Focused proof is green with `13/13` passing tests in `scratchbird_tests`:
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `QueryPlannerIntegrationTest.AgedPublishedFamilyMetricsPreserveMaintenanceStateRefusal`,
    `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.AnnInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.TextInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.HashInvalidCasesPublishSpecificRejectionCodes`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    and
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`.
    This closes another bounded refusal-fidelity rung, but broader
    family-wide refusal coverage, explicit replan-trigger closure, and later
    memory-admission work still remain open.

32. Close the first bounded explicit phase-`4` replan-trigger rung by making
    family-statistics signature cache-boundary replans visible in the returned
    runtime plan instead of only surfacing them as a silent cache miss. The
    compiler now counts cached sibling variants that match the same reuse
    context across family-statistics churn, intentionally allowing cost-profile
    recalibration to differ because the published family-metrics refresh path
    already changes `cost_profile_id` while still representing the same
    family-statistics boundary. The returned compiled payload now carries
    `FAMILY_STATISTICS_REPLAN` statistics provenance,
    a `PLAN_CACHE/CACHE_REUSE/REJECTED` trace row for
    `family statistics signature boundary crossed`, and the optimizer control
    `FAMILY_STATISTICS_REPLAN_BOUNDARY=FAMILY_STATISTICS_SIGNATURE`.
    Focused proof is green with `11/11` passing tests in `scratchbird_tests`:
    `QueryPlannerIntegrationTest.FamilyStatisticsSignatureBoundaryPublishesExplicitReplanProof`,
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `QueryPlannerIntegrationTest.AgedPublishedFamilyMetricsPreserveMaintenanceStateRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    This closes the first explicit family-statistics replan-boundary proof, but
    broader family-wide refusal coverage, wider replan-trigger behavior, and
    later memory-admission work still remain open.

33. Close the next bounded phase-`4`/phase-`6` refusal-fidelity rung by
    preserving canonical family-legality failures as distinct structured
    refusals instead of letting them fall through the generic fail-closed
    bucket. The canonical bundle-refusal helper in
    `include/scratchbird/optimizer/path.h` now preserves
    `P08_FAMILY_LEGALITY_UNDECLARED` as
    `missing canonical family legality classification`,
    `P08_FAMILY_LEGALITY_TRUST` as
    `trust class violates canonical family legality matrix`,
    `P08_FAMILY_LEGALITY_LOCATOR` as
    `locator granularity violates canonical family legality matrix`,
    and `P08_FAMILY_LEGALITY_VISIBILITY` as
    `visibility enforcement violates canonical family legality matrix`.
    These bounded legality paths currently remain in the `POLICY` cause domain,
    which matches the current fail-closed family-admission boundary.
    Focused proof is green with `16/16` passing tests in `scratchbird_tests`:
    `IndexFamilyLoweringTest.CanonicalPlannerBundleRefusalClassifiesFamilyLegalityFailures`,
    `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.AnnInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.TextInvalidCasesPublishSpecificRejectionCodes`,
    `IndexFamilyLoweringTest.HashInvalidCasesPublishSpecificRejectionCodes`,
    `QueryPlannerIntegrationTest.FamilyStatisticsSignatureBoundaryPublishesExplicitReplanProof`,
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `QueryPlannerIntegrationTest.AgedPublishedFamilyMetricsPreserveMaintenanceStateRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    This closes the bounded legality-classification gap only. Broader
    implemented-family refusal coverage, wider replan-trigger behavior, and the
    later memory-admission work still remain open.

34. Close the next bounded optimizer-parity rung by preserving same-column
    lowering failures before the planner falls back to generic
    unmatched-predicate refusal:
    the unmatched-predicate prepass now inspects indexes on the predicate
    column and publishes more specific structured refusals before emitting
    `P08_NO_MATCHING_INDEX_FOR_PREDICATE`. That now preserves
    `P08_HASH_EQ_PREDICATE_REQUIRED` for range predicates against hash indexes,
    keeps partial-index implication failures as
    `P08_PARTIAL_INDEX_PREDICATE_MISMATCH`, and allows generalized-family
    boundary proofs to surface the stricter lowering refusal
    `P08_OPERATOR_STRATEGY_UNBOUND` instead of collapsing back to generic
    semantic mismatch. Focused proof is green with `13/13` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.HashRangePredicateFailsClosedToSequentialScan`,
    `QueryPlannerIntegrationTest.PartialIndexRequiresPredicateImplicationBeforeEnumeration`,
    `QueryPlannerIntegrationTest.FamilyStatisticsSignatureBoundaryPublishesExplicitReplanProof`,
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `QueryPlannerIntegrationTest.AgedPublishedFamilyMetricsPreserveMaintenanceStateRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    This closes another bounded implemented-family refusal gap, but broader
    family-wide refusal coverage, wider replan-trigger behavior, and the later
    memory-admission work still remain open.

35. Close the next bounded optimizer-parity rung by proving native-promotion
    threshold refusals for bitmap and columnstore families at the runtime-plan
    boundary:
    the planner was already emitting
    `P08_BITMAP_NATIVE_PROMOTION_THRESHOLD_NOT_MET` and
    `P08_COLUMNSTORE_NATIVE_PROMOTION_THRESHOLD_NOT_MET`, but those paths had
    no preserved integration proof. The focused planner slice now proves both
    families stay in `candidate_scan_families`, fall back to `SEQ_SCAN`, and
    preserve structured `candidate_family_refusals` entries with
    `family-specific promotion threshold not met` in the `METRICS` cause
    domain. Focused proof is green with `14/14` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.BitmapAndColumnstoreFamiliesBelowPromotionThresholdPublishStructuredRefusals`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.HashRangePredicateFailsClosedToSequentialScan`,
    `QueryPlannerIntegrationTest.PartialIndexRequiresPredicateImplicationBeforeEnumeration`,
    `QueryPlannerIntegrationTest.FamilyStatisticsSignatureBoundaryPublishesExplicitReplanProof`,
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `QueryPlannerIntegrationTest.AgedPublishedFamilyMetricsPreserveMaintenanceStateRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    This closes another bounded implemented-family refusal gap, but broader
    family-wide refusal coverage, wider replan-trigger behavior, and the later
    memory-admission work still remain open.

36. Close the next bounded optimizer-parity rung by proving ranked-text native
    promotion-threshold refusal at the runtime-plan boundary:
    the planner was already emitting
    `P08_TEXT_NATIVE_PROMOTION_THRESHOLD_NOT_MET`, but that ranked-text path
    had no preserved integration proof. The focused planner slice now proves
    the text family stays in `candidate_scan_families`, falls back to
    `SEQ_SCAN`, and preserves a structured `candidate_family_refusals` entry
    for `TEXT_BITMAP_SCAN[idx_docs_title_text]` with
    `family-specific promotion threshold not met` in the `METRICS` cause
    domain. Focused proof is green with `15/15` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.TextFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.BitmapAndColumnstoreFamiliesBelowPromotionThresholdPublishStructuredRefusals`,
    `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
    `QueryPlannerIntegrationTest.HashRangePredicateFailsClosedToSequentialScan`,
    `QueryPlannerIntegrationTest.PartialIndexRequiresPredicateImplicationBeforeEnumeration`,
    `QueryPlannerIntegrationTest.FamilyStatisticsSignatureBoundaryPublishesExplicitReplanProof`,
    `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
    `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
    `QueryPlannerIntegrationTest.AgedPublishedFamilyMetricsPreserveMaintenanceStateRefusal`,
    `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
    `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
    `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
    `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
    and
    `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
    This closes another bounded implemented-family refusal gap, but broader
    family-wide refusal coverage, wider replan-trigger behavior, and the later
    memory-admission work still remain open.

37. Close the next bounded optimizer-parity rung by proving the canonical
    classifier for the remaining promotion-threshold reason codes:
    the direct runtime-plan ANN proof was blocked by the current vector query
    surface, but the planner-side canonical bundle classifier still needed
    preserved proof for `P08_ANN_NATIVE_PROMOTION_THRESHOLD_NOT_MET`,
    `P08_ANN_HYBRID_FALLBACK_NOT_JUSTIFIED`,
    `P08_GENERALIZED_NEAREST_NATIVE_PROMOTION_THRESHOLD_NOT_MET`,
    `P08_GENERALIZED_NATIVE_PROMOTION_THRESHOLD_NOT_MET`, and
    `P08_TEXT_SCORE_ROWS_REQUIRED`. The focused planner slice now proves those
    codes all normalize to
    `family-specific promotion threshold not met` with `METRICS` cause domain
    through
    `QueryPlannerIntegrationTest.CanonicalPlannerBundleRefusalClassifiesAdditionalPromotionThresholdFailures`.
    Focused proof is green with `16/16` passing tests in `scratchbird_tests`,
    adding that new classifier proof to the current summary-threshold,
    bitmap/columnstore threshold, ranked-text threshold, same-column lowering,
    replan-boundary, GIST-boundary, severe-churn MGA, and
    statistics-manager regression slice. This closes another bounded
    implemented-family refusal gap, but broader family-wide refusal coverage,
    wider replan-trigger behavior, and the later memory-admission work still
    remain open.

38. Close the remaining bounded phase-`4`/phase-`6` refusal and replan slice
    by proving the last uncovered canonical family-failure mappings and
    rerunning the adaptive-cardinality replan path as part of the same package
    proof surface:
    `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`
    now also proves `P08_SUPPORT_FUNCTION_UNVALIDATED`, and the new
    `IndexFamilyLoweringTest.CanonicalPlannerBundleRefusalClassifiesRemainingFamilyFailureCodes`
    proves the canonical classifier and cause-domain mapping for
    `P08_SUPPORT_FUNCTION_UNVALIDATED`,
    `P08_BITMAP_COMPOSE_UNAVAILABLE`,
    `P08_SKIP_SCAN_UNAVAILABLE`,
    `P08_EXPRESSION_INDEX_MISMATCH`, and
    `P08_FAMILY_NOT_QUERYABLE`. The focused verification slice also reruns the
    live adaptive and family-statistics replan boundary proofs together:
    `QueryPlannerIntegrationTest.CardinalityFeedbackBypassesStaleCacheAndRebuildsPlan`,
    `QueryPlannerIntegrationTest.AdaptiveFeedbackCachedPlanReflectsLatestFeedbackStateAfterRepeatExecution`,
    and
    `QueryPlannerIntegrationTest.FamilyStatisticsSignatureBoundaryPublishesExplicitReplanProof`.
    Focused verification is green with `23/23` passing tests in
    `scratchbird_tests`, spanning the remaining family-lowering classifier
    surface, the bounded promotion-threshold/refusal suite, and both explicit
    replan trigger families. This closes the remaining package-owned
    phase-`4`/phase-`6` refusal-model and replan-trigger slices; the next live
    frontier is phase `5` memory-grant and spill-admission work.

39. Land the first bounded phase-`5` durability slice by persisting canonical
    `memory_grant_feedback` rows in the catalog instead of leaving grant
    telemetry runtime-only:
    `CatalogManager` now carries a durable `memory_grant_feedback` family with
    root-page persistence, keyed upsert/get/list/delete helpers, and bounded
    inline storage for `operator_kind` and feedback `state`. The focused proof
    surface in
    `CatalogRoutingAdmissionExtensionContractTest.MemoryGrantFeedbackCatalogContracts`
    now covers invalid-state rejection, keyed upsert/update semantics,
    UUID/created-time preservation across update, reopen persistence, and
    delete-by-key. Focused verification is green after rebuild:
    `1/1` passed for the new contract, and the widened
    `CatalogRoutingAdmissionExtensionContractTest.*` slice is also green with
    `3/3` passing tests in `scratchbird_tests`. This closes the first bounded
    phase-`5` catalog durability rung only; grant admission, feedback-driven
    resizing, and runtime spill/workfile execution remain open.

40. Land the next bounded phase-`5` planner-admission slice by consuming
    durable `memory_grant_feedback` rows during hash-join planning and
    proving the feedback path at the runtime-plan boundary:
    the compiler/planner handoff now resolves the effective schema from the
    live connection, the planner computes `grant_key_hash` with the same
    canonical `sblr::v3::stableHash64` contract used by the compiler-side
    feedback snapshot, and hash-join grant feedback now preserves a raised
    catalog-backed operator budget without reintroducing the prior
    spill-policy rejection path.
    Focused verification is green after rebuild with `3/3` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowRejectsSpilledHashJoin`,
    and
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin`.
    This closes the first bounded phase-`5` planner-consumption rung only;
    wider operator reservation, persisted resize feedback loops beyond the
    hash-join proof surface, and runtime spill/workfile execution still
    remain open.

41. Land the next bounded phase-`5` resize-feedback slice by preserving
    feedback-adjusted operator budgets at the runtime-plan boundary and
    persisting executed upper-stage grant observations back into the durable
    catalog feedback family:
    the runtime-plan resource annotator now keeps recorded cost evidence
    instead of recomputing baseline resource metadata, so feedback-adjusted
    `memory_budget_bytes` and `spill_expected` state survive for merge join,
    aggregate, sort, window, and distinct operators. The executor now also
    persists canonical `memory_grant_feedback` samples for executed `SORT`
    and `HASH_AGG` plans, which closes the first end-to-end
    execute-to-recompile feedback loop beyond hash join.
    Focused verification is green after rebuild with `9/9` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.AutoPlanProfileUsesChooserAndPublishesReuseMetadata`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForMergeJoin`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashAggregate`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForSort`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForWindow`,
    `QueryPlannerIntegrationTest.ExecutedSortPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    `QueryPlannerIntegrationTest.ExecutedHashAggregatePersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    and
    `QueryPlannerIntegrationTest.AutoPlanProfilePublishesPeakOperatorMemoryReservationForWrappedSort`.
    This closes the first bounded persisted-resize loop for upper-stage
    operators only; broader operator reservation, admission behavior, and
    runtime spill/workfile execution still remain open.

42. Land the next bounded phase-`5` operator-reservation slice by deriving a
    chooser-visible statement reservation from the full runtime-plan tree
    instead of only the root node:
    reusable-plan candidate construction now computes
    `memory_budget_bytes` from the peak operator budget across the runtime
    plan and join steps, and the compiler publishes that bound as
    `PLAN_MEMORY_RESERVATION_BYTES`. This closes the concrete wrapped-operator
    under-admission bug where `Limit` or similar root wrappers previously
    published `0` while a child `Sort` carried the real budget.
    Focused verification is green after rebuild with the same `9/9` passing
    Phase-`5` slice listed above, including
    `QueryPlannerIntegrationTest.AutoPlanProfilePublishesPeakOperatorMemoryReservationForWrappedSort`.
    This closes the first bounded chooser-visible operator reservation rung
    only; explicit runtime admission behavior and spill/workfile execution
    still remain open.

43. Land the next bounded phase-`5` runtime-admission slice by enforcing live
    spill policy and `WORK_MEM` against previously compiled bytecode at
    execution time:
    the V3 executor now validates embedded runtime plans before execution and
    fails closed when the live session has `SPILL_POLICY=DISALLOW` but the
    compiled plan still carries `spill_expected=true`, or when the plan's
    recorded memory reservation exceeds the live `WORK_MEM` ceiling. This
    closes the stale-bytecode admission gap where a plan compiled under a
    looser memory policy could still execute after the session tightened its
    runtime admission rules.
    Focused verification is green after rebuild with `12/12` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.AutoPlanProfilePublishesPeakOperatorMemoryReservationForWrappedSort`,
    `QueryPlannerIntegrationTest.HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowRejectsSpilledHashJoin`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForMergeJoin`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashAggregate`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForSort`,
    `QueryPlannerIntegrationTest.ExecutedSortPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForWindow`,
    `QueryPlannerIntegrationTest.ExecutedHashAggregatePersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    `QueryPlannerIntegrationTest.ExecuteBytecodeRejectsPreviouslyCompiledSpillPlanWhenLiveSpillPolicyDisallows`,
    and
    `QueryPlannerIntegrationTest.ExecuteBytecodeRejectsPlanWhoseReservationExceedsLiveWorkMemUnderSpillDisallow`.
    This closes the first bounded runtime stale-plan admission rung only;
    runtime spill/workfile execution itself still remains open.

44. Land the next bounded phase-`5` execute-to-recompile feedback slice by
    extending executed upper-stage proof coverage beyond `SORT` and
    grouped/hash aggregate:
    the durable `memory_grant_feedback` path is now proven end to end for
    executed `WINDOW` and hash-`DISTINCT` plans as well, with the runtime
    recompile path consuming those catalog samples under later
    `SPILL_POLICY=DISALLOW` planning.
    Focused verification is green after rebuild with `6/6` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.ExecutedSortPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    `QueryPlannerIntegrationTest.ExecutedWindowPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    `QueryPlannerIntegrationTest.ExecutedHashAggregatePersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    `QueryPlannerIntegrationTest.ExecutedHashDistinctPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForWindow`,
    and
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashDistinct`.
    This closes another bounded execute-to-recompile feedback gap, but
    join-family execution feedback beyond hash join, wider runtime admission,
    and runtime spill/workfile execution still remain open.

45. Land the next bounded phase-`5` join-family execute-to-recompile feedback
    slice by proving executed `HASH_JOIN` plans seed durable feedback that the
    later spill-disallow compile path actually consumes:
    the package now has end-to-end executed proof for hash join, not just
    catalog-seeded planner-consumption proof.
    Focused verification is green after rebuild with `4/4` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowRejectsSpilledHashJoin`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin`,
    and
    `QueryPlannerIntegrationTest.ExecutedHashJoinPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`.
    This closes the executed hash-join feedback proof gap, but merge-join
    execution feedback, wider runtime admission, and runtime spill/workfile
    execution still remain open.

46. Land the next bounded phase-`5` runtime spill/workfile join-family slice
    by giving `MERGE_JOIN` a real external-sort path for sort-to-merge inputs
    instead of always sorting both sides in memory:
    the V3 executor now spills merge-join input runs to `sb_workfile` when the
    effective join budget cannot hold the full sort-to-merge input, merges
    those runs back in key order, and records actual merge-join spill
    observation into durable `memory_grant_feedback` instead of relying only on
    planner-side `spill_expected`.
    Focused verification is green after rebuild with `3/3` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForMergeJoin`,
    `QueryPlannerIntegrationTest.ExecutedMergeJoinPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    and
    `QueryPlannerIntegrationTest.ExecuteBytecodeRunsSpilledMergeJoinThroughWorkfileAndPersistsFeedback`.
    This closes the first bounded join-family runtime spill/workfile execution
    gap, but broader upper-stage runtime spill/workfile behavior still remains
    open.

47. Land the next bounded phase-`5` upper-stage runtime spill/workfile slice by
    making the V3 `WINDOW` operator perform bounded external ordering under
    live memory pressure instead of only publishing planner-side spill intent:
    the executor now extracts the embedded window ordering from the V3
    projection payload, sorts the window input stream on that boundary, spills
    oversized runs to `sb_workfile`, merges those runs back in key order, and
    records actual `WINDOW` spill observation into durable
    `memory_grant_feedback`.
    Focused verification is green after rebuild with `3/3` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.ExecutedWindowCapturesActualSortSpillOnStaleBytecode`,
    `QueryPlannerIntegrationTest.ExecutedWindowPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    and
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForWindow`.
    This closes the bounded stale-window runtime spill/workfile gap, but hash
    join and broader operator-runtime spill behavior still remain open.

48. Land the next bounded phase-`5` hash-join runtime spill/workfile slice by
    making the V3 executor spill oversized hash-join partitions to
    `sb_workfile` under live memory pressure instead of relying only on
    planner-side `spill_expected` and compile-time feedback:
    stale bytecode that was originally compiled below spill thresholds now
    partitions build/probe inputs through bounded workfiles, rejoins those
    partitions at execution time, and records actual `HASH_JOIN` spill
    observation into durable `memory_grant_feedback`.
    Focused verification is green after rebuild with `5/5` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowRejectsSpilledHashJoin`,
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin`,
    `QueryPlannerIntegrationTest.ExecutedHashJoinPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    and
    `QueryPlannerIntegrationTest.ExecutedHashJoinCapturesActualSpillOnStaleBytecode`.
    This closes the bounded stale-hash-join runtime spill/workfile gap, but
    broader operator-runtime spill behavior still remains open.

49. Land the next bounded phase-`5` upper-stage hash-distinct runtime
    spill/workfile slice by making stale `HASH_DISTINCT` bytecode partition
    and deduplicate through `sb_workfile` once runtime cardinality outgrows the
    original in-memory budget:
    the executor now records actual `HASH_AGG` spill observation for distinct
    plans instead of only inheriting planner-side spill intent.
    Focused verification is green after rebuild with `3/3` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashDistinct`,
    `QueryPlannerIntegrationTest.ExecutedHashDistinctPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    and
    `QueryPlannerIntegrationTest.ExecutedHashDistinctCapturesActualSpillOnStaleBytecode`.
    This closes the bounded stale-hash-distinct runtime spill/workfile gap,
    but grouped hash aggregate and broader operator-runtime spill behavior
    still remain open.

50. Land the next bounded phase-`5` grouped hash-aggregate runtime
    spill/workfile slice by making stale grouped `HASH_AGGREGATE` bytecode
    partition and aggregate through `sb_workfile` once runtime cardinality
    outgrows the original in-memory budget:
    the executor now records actual `HASH_AGG` spill observation for grouped
    aggregate plans instead of only inheriting planner-side spill intent.
    Focused verification is green after rebuild with `3/3` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashAggregate`,
    `QueryPlannerIntegrationTest.ExecutedHashAggregatePersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
    and
    `QueryPlannerIntegrationTest.ExecutedHashAggregateCapturesActualSpillOnStaleBytecode`.
    This closes the bounded stale-hash-aggregate runtime spill/workfile gap,
    but stale sort execution and wider runtime admission behavior still remain
    open.

51. Land the next bounded phase-`5` top-level sort runtime spill/workfile
    slice by making stale `SORT` bytecode switch on runtime row pressure
    against the embedded plan budget instead of only on planner-side
    `spill_expected`:
    the executor now spills oversized sort runs to `sb_workfile`, merges them
    back in key order, preserves the ordered result stream, and records actual
    `SORT` spill observation into durable `memory_grant_feedback`.
    Focused verification is green after rebuild with `3/3` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForSort`,
    `QueryPlannerIntegrationTest.ExecuteBytecodeRunsSpilledSortThroughWorkfileAndPreservesOrdering`,
    and
    `QueryPlannerIntegrationTest.ExecutedSortCapturesActualSpillOnStaleBytecode`.
    This closes the bounded stale-sort runtime spill/workfile gap, but
    explicit spill-disallow cancellation accounting and broader runtime
    admission behavior still remain open.

52. Land the next bounded phase-`5` spill-disallow cancellation-accounting
    slice by making runtime admission publish durable cancellation evidence
    into `memory_grant_feedback` when stale bytecode is rejected under live
    `SPILL_POLICY=DISALLOW`:
    the executor now increments durable `cancel_count` for the rejected
    operator family when the live session disallows spill-expected operators or
    when the embedded runtime-plan reservation exceeds live `WORK_MEM`.
    Focused verification is green after rebuild with `2/2` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.ExecuteBytecodeRejectsPreviouslyCompiledSpillPlanWhenLiveSpillPolicyDisallows`
    and
    `QueryPlannerIntegrationTest.ExecuteBytecodeRejectsPlanWhoseReservationExceedsLiveWorkMemUnderSpillDisallow`.
    This closes the bounded cancellation-accounting gap for runtime admission,
    but broader operator reservation and remaining spill/workfile behavior
    still remain open.

53. Land the next bounded phase-`5` right-sizing and oscillation-consumption
    slice by making the planner consume mature stable-underuse feedback to
    shrink oversized operator reservations without letting oscillating rows
    thrash the compile-time budget:
    the planner now accepts durable `memory_grant_feedback` right-sizing
    packets when `underuse_streak` reaches the bounded stable threshold,
    lowers the runtime working-set annotation to the observed packet when
    shrinking, and still refuses to shrink when the feedback state is
    `OSCILLATING`.
    Focused verification is green after rebuild with `13/13` passing tests in
    `scratchbird_tests`, including
    `QueryPlannerIntegrationTest.StableUnderuseFeedbackShrinksSortReservationUnderSpillDisallow`
    and
    `QueryPlannerIntegrationTest.OscillatingSortFeedbackDoesNotShrinkReservation`.
    This closes the bounded planner-side right-sizing branch for phase `5`,
    but broader operator reservation and remaining spill/workfile behavior
    still remain open.

54. Land the next bounded phase-`5` statement-reservation admission slice by
    proving the feedback-shrunk `PLAN_MEMORY_RESERVATION_BYTES` control
    actually changes runtime spill-disallow admission instead of only changing
    compile-time metadata:
    the focused sort path now proves that the old statement reservation is
    rejected under a live `WORK_MEM` ceiling while the same SQL compiled after
    mature stable-underuse feedback is admitted and executed successfully
    under that same live ceiling.
    Focused verification is green after rebuild with `4/4` passing tests in
    `scratchbird_tests`, including
    `QueryPlannerIntegrationTest.FeedbackShrunkSortReservationChangesRuntimeSpillDisallowAdmission`.
    This closes the bounded compile-to-runtime statement-reservation bridge
    for phase `5`, but broader operator reservation and remaining
    spill/workfile behavior still remain open.

55. Land the next bounded phase-`5` memory-grant identity-hardening slice by
    proving durable `memory_grant_feedback` rows do not bleed across planner
    policy, cache-mode, execution-intent, or storage-shape mismatches:
    the focused sort path now proves that a generic grant row is ignored by a
    custom parameter-sensitive compile, that rows seeded under the wrong
    planner policy snapshot, wrong execution-intent class, or wrong
    storage-layer shape are also ignored, and that the fully matching row is
    consumed.
    Focused verification is green after rebuild with `3/3` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.MemoryGrantFeedbackDoesNotCrossPlanProfileIdentity`,
    `QueryPlannerIntegrationTest.MemoryGrantFeedbackRequiresExecutionIntentIdentity`,
    and
    `QueryPlannerIntegrationTest.MemoryGrantFeedbackRequiresPolicySnapshotAndStorageShapeIdentity`.
    This closes the bounded grant-key identity hardening branch for phase `5`,
    but broader operator reservation and remaining spill/workfile behavior
    still remain open.

56. Land the bounded phase-`5` closeout slice by separating grant-policy
    identity from the broader reusable-plan policy snapshot, publishing the
    exact memory-feedback snapshot on the runtime plan, and replaying the full
    phase-`5` proof surface:
    the compiler now publishes `PLAN_GRANT_POLICY_SNAPSHOT` and
    `PLAN_MEMORY_FEEDBACK_SNAPSHOT`, the executor persists runtime
    `memory_grant_feedback` rows under the same grant-policy identity the
    planner consumes, and the full bounded memory-admission suite is green.
    Verification is green after rebuild with `36/36` passing tests in
    `scratchbird_tests` for the bounded phase-`5` filter covering grant
    feedback, spill admission, reservations, underuse, oscillation, and the
    reusable-plan metadata surface.
    This closes phase `5` for package `08`; the active frontier moves to
    phase `6` planner parity and refusal-model closure.

57. Land the first bounded phase-`6` join-runtime hot-path slice by removing
    avoidable full-row materialization from direct residual join predicates:
    the V3 join executor now recognizes conjunctions of direct left/right
    column comparison terms and evaluates them in place inside the join loops
    instead of rebuilding a combined row and re-entering the general
    expression evaluator for every candidate pair.
    This directly covers the benchmark-shaped self-join residual pattern
    (`left.eq_key = right.eq_key AND left.id < right.id`) on the nested-loop
    path the planner currently chooses for that query family.
    Focused verification is green after rebuild with `4/4` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.HashJoinPlanExecutesAndReturnsExpectedRows`,
    `QueryPlannerIntegrationTest.NestedLoopSelfJoinResidualComparisonExecutesExpectedPairs`,
    `QueryPlannerIntegrationTest.DefaultSelfJoinResidualComparisonExecutesExpectedPairs`,
    and
    `QueryPlannerIntegrationTest.MergeJoinPlanExecutesAndPreservesRuntimeMetadata`.
    This closes the first bounded phase-`6` executor slice, but broader
    benchmark-visible join, aggregate, sort, projection, and late-
    materialization work still remain open.

58. Land the next bounded phase-`6` hash-join executor slice by removing
    avoidable string key materialization from the supported single-column
    integer equality path:
    the V3 hash join executor now normalizes supported scalar integer keys
    into compact structured hash keys in both the in-memory build/probe
    branch and the spilled partition/workfile branch instead of allocating
    `Value::toString()` keys for every row on that join family.
    This directly targets the benchmark-visible integer hash-join path behind
    `inner_join_large_result` while preserving the prior string-key fallback
    for unsupported key shapes.
    Focused verification is green after rebuild with `4/4` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.HashJoinPlanExecutesAndReturnsExpectedRows`,
    `QueryPlannerIntegrationTest.ExecutedHashJoinCapturesActualSpillOnStaleBytecode`,
    `QueryPlannerIntegrationTest.NestedLoopSelfJoinResidualComparisonExecutesExpectedPairs`,
    and
    `QueryPlannerIntegrationTest.DefaultSelfJoinResidualComparisonExecutesExpectedPairs`.
    This closes the next bounded phase-`6` executor slice, but broader
    benchmark-visible join, aggregate, sort, projection, and late-
    materialization work still remain open.

59. Land the bounded phase-`6` projection and aggregate-key closeout slice by
    removing the remaining avoidable query-path overheads on the live V3
    benchmark surface:
    qualified `table.*` payloads are now preserved end to end, relation-scoped
    projection requirements now drive pruned table loads through the join path,
    and the grouped or distinct key builders on the V3 aggregate path now use
    typed binary keys instead of `Value::toString()` string keys.
    Focused verification is green after rebuild with `8/8` passing tests in
    `scratchbird_tests`:
    `QueryPlannerIntegrationTest.HashJoinPlanExecutesAndReturnsExpectedRows`,
    `QueryPlannerIntegrationTest.QualifiedTableStarPreservesOwningRelationProjection`,
    `QueryPlannerIntegrationTest.CountStarExecutesWithProjectionPrunedRelationLoad`,
    `QueryPlannerIntegrationTest.NestedLoopSelfJoinResidualComparisonExecutesExpectedPairs`,
    `QueryPlannerIntegrationTest.DefaultSelfJoinResidualComparisonExecutesExpectedPairs`,
    `QueryPlannerIntegrationTest.ExecuteBytecodeRunsSpilledMergeJoinThroughWorkfileAndPersistsFeedback`,
    `QueryPlannerIntegrationTest.MergeJoinPlanExecutesAndPreservesRuntimeMetadata`,
    and
    `QueryPlannerIntegrationTest.ParallelAggregateWrapsPlanInGatherAndPublishesStageMetadata`.
    Targeted fresh-runtime stress proof is preserved in
    `ScratchBird-Benchmarks/results/phase6-targeted-20260404T/` with
    `inner_join_large_result = 13934.35ms`,
    `aggregation_daily_sales = 2409.84ms`,
    and
    `multi_dimensional_agg = 4806.16ms`.
    This closes phase `6` for package `08`; the active frontier moves to
    phase `7` full rerun and release evidence closure.

## Beta 1 Decision Rule

Beta 1 release closure is blocked until the action families owned by this
package have practical bounded performance and preserved evidence. If a limit is
still materially user-visible or still breaks the package benchmark surface, it
remains a release blocker and cannot be deferred as cosmetic optimization.
