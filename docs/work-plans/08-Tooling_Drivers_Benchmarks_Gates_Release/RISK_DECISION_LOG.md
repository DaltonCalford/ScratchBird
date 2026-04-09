# Risk Decision Log

## Fixed Decisions

- B1-08-001 must close specification sufficiency before any implementation
  ticket begins
- the local reference tree under docs/reference is the primary donor and
  authority intake surface for this lane
- the Beta 1 aggregate gate must run from a clean build tree with the example
  runtime ports free; the benchmark scratchbird runtime now uses the dedicated
  `17092`, `17432`, `17306`, and `17050` listener set and
  `scripts/example_db_manager.sh` fails fast if requested example ports are not
  bindable
- the `ScratchBird-Benchmarks` manual `stress medium` shape is a diagnostic
  workload, not a Beta 1 release gate; the canonical release path remains the
  section `31` aggregate with bounded in-repo stress inputs
- pre-`2026-04-03` cross-engine stress artifacts are not comparable for
  transaction-shape analysis and were intentionally deleted before the
  refreshed matrix was rerun
- the preserved comparable transaction-aware stress authority is now
  `ScratchBird-Benchmarks/results/txmode-matrix-20260403T152011Z`

## Active Risk

Risk: the upstream Firebird `index-comparison` harness still fails outside the
ScratchBird engine with `Table unknown IDXCMP_POINT_LOOKUP`; this remains an
external benchmark-repo issue and prevents a fully green cross-engine matrix.

Risk: PostgreSQL `no_transaction` evidence is only acceptable when the harness
uses real batched multi-row inserts. Raw `psycopg2.executemany()` with
connection autocommit enabled degenerates into row-by-row inserts and turns the
stress load into a driver batching artifact rather than a clean
transaction-shape comparison. The active matrix root above already includes the
corrected `execute_values()` batching path and supersedes the aborted partial
rerun that was deleted before evidence preservation.

Risk: the ScratchBird `stress --scale small` lane in the clean replay
`tests/results/full_gate/20260401T161737Z` no longer crashes after the session
catalog fail-closed hardening, but it still exceeds a practical Beta 1 runtime
envelope on a bounded `10k customers / 5k products / 50k orders / 200k
order_items` dataset. The benchmark-only harness now uses a `7200s` leak
threshold and a ScratchBird load batch cap of `4096` rows, but the throughput
limit is still open and blocks `B1-08-004` and `B1-08-005`.

Risk: direct `sb_isql` import probes now show the throughput issue is not
limited to the Python benchmark harness. A full exported stress load script
(`scratchbird_stress_small_load.sql`) disconnected after `34.98s` to `36.58s`
with repeated `Rows affected: 0` responses and `Failed to receive response`,
and a customers-only `1000`-row script using `256`-row literal `INSERT`
statements was still running after `71s` with `sb_server` pinned near `100%`
CPU and no client response before the probe was cut off. This is current
evidence of a server-side insert-path throughput problem, not only a benchmark
driver artifact.

Risk: direct insert-path tracing now shows the dominant bounded engine cost is
row-by-row free-space discovery, not heap tuple placement itself. With
`SCRATCHBIRD_INSERT_TRACE=1`, a `256`-row heap-only `executemany()` load took
`2673.453 ms`, while the summed per-row engine traces were `2523.284 ms`; of
that traced engine time, `2490.146 ms` was spent in `findFreePage()` and only
`1.015 ms` in `HeapPage::insertTuple()`. The trace also showed roughly
`355` pages scanned and `34` heap pages examined per row on the current
benchmark runtime, matching the current linear heap scan design in
`StorageEngine::findFreePage()`.

Risk: stress-shaped rows add a second large bounded server cost from unique
insert preflight. A `128`-row `customers`-like batch with `PRIMARY KEY` plus
`UNIQUE(email)` took `10006.994 ms`; the traced per-row engine work for the
main table summed to `4228.408 ms`, split as `2859.275 ms` in uniqueness
preflight and `1320.568 ms` in free-page search, with only `0.860 ms` in heap
placement and `14.020 ms` in post-insert index maintenance. The remaining
runtime above the traced storage work is currently attributed to large
multi-row statement overhead above `StorageEngine` (parse/bind/executor-top
layer) and remains open.

Decision: this is a Beta 1 release blocker, not a post-release tuning item.
Active remediation scope and closure criteria are now tracked in
`PERFORMANCE_REMEDIATION_PLAN.md`.

Decision update 2026-04-01:
- The insert-path blocker is no longer in the original failure shape. After the
  writable-page hint, unique-preflight metadata cache, and duplicate
  SQL-layer uniqueness checks were removed from both insert executors, the
  bounded probes improved materially:
  `256` heap-only rows fell from `2673.453 ms` to roughly `183-192 ms`,
  and the `128`-row `customers`-like batch fell from `10006.994 ms` to
  `197.577 ms` execute time.
- The active Beta 1 risk is now narrowed to benchmark replay closure rather
  than a fundamentally broken insert path. Package `08` stays open until the
  fresh bounded `stress --scale small` rerun and the aggregate benchmark gate
  prove the end-to-end lane is practical again.

Risk update 2026-04-01:
- The fresh bounded `stress --scale small` rerun did complete and confirmed the
  insert-throughput blocker is materially reduced, but it exposed a new Beta 1
  correctness fault during `orders` load. After `Batch 10: 40,960 rows loaded`
  the engine returned:
  `Page number 174096499359744 exceeds uint32_t maximum for primary tablespace`.
- That first failure then poisoned the session and benchmark lane with
  follow-on `[25000] Failed to rollback transaction`,
  `[25000] Read timeout: incomplete message`,
  and `[42000] Failed to receive response` errors.
- The active package risk is now the `GPID/page-number` overflow or corruption
  path surfaced by the bounded stress replay, plus the fail-closed recovery of
  the connection after that fault. This replaces the earlier “insert path is
  fundamentally too slow” blocker as the highest-severity open item in
  `B1-08-004`.

Risk update 2026-04-02:
- The `GPID/page-number` load failure is no longer the active blocker on the
  stable branch. The bounded stress lane now loads fully and reaches the
  self-join benchmark consistently.
- Stable retained V3 executor fixes now cover:
  inferred hash-join key extraction from conjunctive predicates,
  string-key admission for inferred hash joins,
  and safe base-side `WHERE` pushdown when the predicate resolves entirely
  against the currently loaded relation set.
- Those retained fixes materially improved the bounded benchmark lane:
  the self-join no longer disconnects on the stable branch and completed in
  `276232.52 ms`, then `260192.82 ms` after the bounded final-join limit
  short-circuit.
- The active Beta 1 blocker is now a narrower performance envelope issue:
  `self_join_same_country` still completes at about `260 s`, which is above the
  scenario’s nominal `180 s` target even though it no longer crashes.
- A further residual-fast-path experiment was tested on `2026-04-02` and was
  rejected because it regressed the bounded lane back to
  `[42000] Failed to receive response` after `309199.67 ms`.
  That branch is not retained; the workspace is back on the last known stable
  executor state.

Decision:
- Keep the stable V3 join improvements in place.
- Record the nominal timeout miss as an explicit Beta 1 release blocker.
- Continue remediation from the stable branch rather than from the rejected
  residual-fast-path experiment.

Risk update 2026-04-03:
- The bounded `self_join_same_country` timeout miss is no longer the active
  Beta 1 blocker. After rebuilding the live `sb_server` with the retained
  stable V3 executor fixes and reordering
  `StorageEngine::findBackVersionPlacementPage()` to consult the hot
  relation-write hint before scanning the primary-page locality window, the
  same-harness stress replay completed in `2446.75 ms` instead of
  `260192.82 ms`.
- The active query risk is now a first-hit competitiveness gap rather than a
  timeout or disconnect. Current same-harness comparison points on the bounded
  `small` dataset are:
  PostgreSQL `13.64 ms`,
  MySQL `112.59 ms`,
  Firebird `137.73 ms`,
  ScratchBird `2446.75 ms`.
- Focused post-load probes show that this remaining gap is not the steady-state
  cost of the join itself. The exact self-join query still takes roughly
  `2220-2295 ms` on its first executions against the bounded benchmark
  database, but once the relevant pages and join path are hot it stabilizes
  around `108-119 ms` for `LIMIT 1` and `241-245 ms` for `LIMIT 10000`.
- This means the open Beta 1 risk has moved again: the largest remaining
  bounded issue is cold-path read/join startup cost and related observability,
  not the former broken DML throughput or the former self-join timeout.
- `EXPLAIN ANALYZE` remains insufficient on this path. The runtime currently
  returns only `Plan unavailable for opcode SBLR3_EXPLAIN_PLAN` even for the
  exact self-join repro, so the package still lacks a reliable Beta 1
  operator-level explain surface for the remaining query-path analysis.

Decision update 2026-04-03:
- Treat the catastrophic self-join blocker as closed.
- Keep the write-hint-first back-version placement change in the stable
  branch; it is directly responsible for collapsing the focused PK-style update
  path from roughly `1866 ms` statement time to `73.891 ms`.
- Continue package `08` on two bounded fronts:
  1. close the remaining first-hit read/join startup penalty so the bounded
  stress query is competitive with Firebird/MySQL/PostgreSQL on the first
  measured run, and
  2. close the Beta 1 explain/plan-surface gap on `SBLR3_EXPLAIN_PLAN` so the
  remaining optimizer and cache-path work is directly observable.

Risk update 2026-04-03 (buffer budget experiment):
- A clean supported-runtime replay proved that the default residency budget was
  itself a major Beta 1 blocker. `BufferPool::Config` still defaults to
  `64` pages, which at the active `16 KiB` page size is only about `1 MiB` of
  durable-page residency unless the runtime overrides it.
- Rerunning the bounded `self_join_same_country` stress lane from scratch with
  `SCRATCHBIRD_MEMORY_BUFFER_POOL_SIZE=256MB` changed the lane materially:
  total load fell from `385.06 s` to `36.57 s`, and the stress query itself
  fell from `2446.75 ms` to `309.07 ms`.
- This means the active Beta 1 risk is no longer “query path still times out”
  and no longer even mainly “query path is catastrophically slow.”
  The dominant newly proven risk is that the engine default buffer-pool budget
  is not suitable for Beta 1 benchmark or production-ready bounded workloads.
- The comparison gap remains open but is now narrow enough to treat as a
  second-order optimization task rather than a fundamental failure:
  PostgreSQL `13.64 ms`,
  MySQL `112.59 ms`,
  Firebird `137.73 ms`,
  ScratchBird `309.07 ms`.

Decision update 2026-04-03 (buffer budget experiment):
- Record the `64`-page default residency budget as an explicit Beta 1
  performance and configuration defect.
- Keep the package focus on two follow-up items:
  1. convert the proven residency-budget workaround into a canonical engine or
  packaged-runtime default that scales with the environment instead of
  relying on an ad hoc benchmark-only override, and
  2. finish the comparison-lane reruns, starting with
  `bulk_update_with_case`, on the corrected runtime budget before declaring
  the remaining gap to be primarily planner-side.

Risk update 2026-04-03 (implicit budget correction proof):
- The bounded `stress --scale small` lane is no longer a live Beta 1 blocker on
  the active default runtime. After correcting `Database::loadBufferPoolConfig`
  so the implicit buffer-pool budget scales from the detected environment
  ceiling instead of silently inheriting the legacy `64`-page struct default,
  clean no-override reruns now match the earlier explicit `256MB` workaround
  closely.
- Preserved filtered proof:
  `evidence/B1-08-004/self_join_defaultbuffer_postpatch_20260403T014200Z/stress_scratchbird_20260403_014046.json`
  with total load `36.82 s` and `self_join_same_country = 294.71 ms`.
- Preserved full-suite proof:
  `evidence/B1-08-004/full_stress_defaultbuffer_postpatch_20260403T014600Z/stress_scratchbird_20260403_014236.json`
  with all `15/15` tests passing, total load `46.90 s`, total measured test
  time `46.41 s`, `self_join_same_country = 350.28 ms`, and
  `bulk_update_with_case = 1324.10 ms`.
- This closes the earlier “bounded small stress lane is operationally
  impractical on the default runtime” risk. The package now has a practical
  default runtime for the bounded Beta 1 stress lane.
- Two residual limits remain and are now explicitly non-blocking:
  1. the cross-engine competitiveness gap is still open on some scenarios
     (`self_join_same_country` remains slower than PostgreSQL, MySQL, and
     Firebird on the first measured run), and
  2. the expected SQL observability views
     `sb_buffer_pool_stats`, `sb_mga_runtime_metrics`, and `sb_engine_health`
     are still not exposed on the live benchmark runtime query surface even
     though the underlying catalog handlers exist in code.

Decision update 2026-04-03 (implicit budget correction proof):
- Treat the bounded `small` stress runtime-shape blocker as closed.
- Preserve the new startup budget log in `database.cpp`; it is useful release
  evidence even though the daemonized example runtime still redirects the
  benchmark `sb_server` stdout and stderr to `/dev/null` after bootstrap.
- Move package `08` back to release-closure work: full clean aggregate rerun,
  archive evidence, and residual competitiveness or observability
  documentation.

Decision update 2026-04-03 (exact-family maintained-write observability):
- Keep the stable same-key exact update suppression path, but no longer allow
  it to bypass maintained-index observability.
- The active runtime now records unchanged-key exact rewrites as
  `IndexDeltaOp::UPDATE` deltas for active online-maintenance rows and bumps
  `index_contention.hot_key_count` on the affected exact index.
- Exact-family unique conflicts now increment
  `index_contention.unique_key_conflict_count` on both insert and update
  preflight failure paths.
- Focused proof is now preserved by the three green contracts in
  `test_index_executor_dispatch_contracts.cpp`.
- This closes a bounded section `18` observability gap, but it does not close
  the remaining phase `1` structural backlog:
  commit-group batch apply, reclaim-driven cleanup debt, cold-page delta
  buffering, and hot-leaf mitigation still remain open Beta 1 work.

Decision update 2026-04-03 (hot-right-edge reserve rung):
- Keep the new exact-family maintained-write observability changes and extend
  phase `1` with the first bounded hot-leaf structural mitigation.
- The active B-tree insert path now presplits monotonic right-edge inserts
  before the rightmost leaf consumes the configured reserved free-space band.
- The implementation is intentionally narrow and low-risk: it reuses the
  existing rightmost-leaf hint and ordinary split path rather than inventing a
  new rebalance regime.
- Focused proof is now preserved by
  `BtreeTextRightmostRegressionTest.SequentialWideKeysPresplitHotRightmostLeafBeforeFull`
  together with the existing three exact-family maintained-write contracts.
- This closes the first required hot-right-edge mitigation rung from section
  `18`, but the remaining phase `1` structural backlog is still open:
  commit-group batch apply, reclaim-driven cleanup debt, narrow cold-page
  delta buffering, and higher-rung hot-leaf mitigation.

Decision update 2026-04-03 (reclaim-driven exact compaction rung):
- Keep the new hot-right-edge mitigation and extend phase `1` with a bounded
  reclaim-driven exact cleanup closure on the B-tree leaf path.
- The active runtime now compacts a leaf immediately during
  `BTree::removeDeadEntries()` after reclaim-proof dead entries are marked
  deleted whenever reclaimable bytes on that page cross the section `18`
  compact threshold.
- The compaction result is verified immediately; destructive cleanup on that
  locality stops if verification fails.
- Focused proof is now preserved by
  `BTreeGCTest.RemoveDeadEntriesCompactsLeafWhenReclaimThresholdExceeded`
  together with the existing hot-right-edge and maintained-write contracts.
- This closes the local reclaim-driven exact compaction rung from section `18`,
  but the remaining phase `1` structural backlog is still open:
  commit-group batch apply, reclaim-driven cleanup debt publication beyond the
  local compaction slice, narrow cold-page delta buffering, and higher-rung
  hot-leaf mitigation.

Decision update 2026-04-03 (bounded exact cleanup debt publication bridge):
- Keep the local reclaim-driven B-tree compaction work and extend phase `1`
  with a bounded publication bridge for residual exact cleanup debt.
- The active B-tree GC path now exposes residual backlog pages, backlog bytes,
  the first affected locality page, and a repair-required bit after
  `removeDeadEntries()`.
- The garbage-collector publication bridge now carries that residual exact
  backlog into `IndexCleanupPublicationRecord` and the cleanup summary without
  breaking the earlier exact-completion contract for fixtures that do not map
  to a real dead B-tree entry.
- Focused proof is now preserved by
  `BTreeGCTest.RemoveDeadEntriesPublishesResidualCleanupDebtBelowCompactionThreshold`
  together with the existing exact compaction, hot-right-edge, maintained-write
  observability, and MGA publication contracts.
- This closes a bounded section `18` visibility gap for exact cleanup debt,
  but the remaining phase `1` structural backlog is still open:
  commit-group batch apply, durable debt ledgers beyond the bounded B-tree
  bridge, narrow cold-page delta buffering, and higher-rung hot-leaf
  mitigation.

Decision update 2026-04-03 (bounded durable exact cleanup debt ledger):
- Keep the new residual exact-cleanup publication bridge and extend it into a
  durable per-index ledger instead of waiting for the later global scheduler
  wave.
- The active runtime now rolls cleanup backlog count, pages, bytes,
  repair-required state, and sweep/checkpoint generations into
  `index_health` whenever `publishIndexCleanupPublication()` updates an exact
  cleanup publication.
- `sb_mga_cleanup_debt` now falls back to that durable ledger when runtime
  cleanup publications are absent, so restart or publication-map loss no
  longer erases the cleanup-debt signal.
- Focused proof is now preserved by
  `CatalogIndexMetricsExtensionContractTest.StorageAndHealthContracts`,
  `MgaObservabilityLiveViewsTest.CleanupPublicationUpdatesDurableIndexHealthLedger`,
  `MgaObservabilityLiveViewsTest.BuildsMgaCleanupDebtRowsFromDurableIndexHealthLedger`,
  `MgaObservabilityLiveViewsTest.BuildsLiveMgaRowsFromRuntimeCatalogAndFragmentationState`,
  and
  `MgaObservabilityLiveViewsTest.BuildsDurabilityRowsFromCatalogHistoryAndRuntimeState`.
- The active runtime now also batches exact online-maintenance deltas through
  the existing group-commit fence: per-xid pending deltas accumulate in
  `StorageEngine`, commit-order batch application happens before final TIP
  publication, and rollback or failed fenced batches discard both queued and
  partially materialized maintenance-delta rows.
- Focused verification for that slice is green with `16/16` passing tests:
  the full `GroupCommitTest.*` suite,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesInsertDelta`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesSameKeyUpdateDelta`,
  `StorageEngineTest.GroupCommitQueuesOnlineMaintenanceDeltaUntilCommit`,
  and
  `StorageEngineTest.GroupCommitRollbackDiscardsQueuedOnlineMaintenanceDelta`.
- This closes the bounded commit-group rung for section `18`. The remaining
  phase `1` structural backlog is now:
  higher-rung hot-leaf mitigation,
  broader scheduler-grade debt routing beyond the bounded per-index ledger,
  and replacing the current synchronous read/write-triggered cold-delta merge
  path with a scheduler-owned merge lane.
- The next bounded prerequisite is now also landed: a durable
  `index_page_delta` catalog family with bootstrap, root-page persistence,
  full-extract visibility, family-matrix registration, and CRUD validation.
  Focused proof was green in `scratchbird_tests` for
  `CatalogDatabaseBootstrapTest.CreatesIndexMetadataExtensionCatalogFamilyPages`,
  `CatalogFamilyMatrixContractTest.*`,
  `CatalogFullExtractTest.*`,
  and
  `CatalogIndexMetadataExtensionContractTest.OptionAndMaintenanceDeltaContracts`
  as a `4/4` pass set. This closes the catalog substrate rung.
- The bounded runtime deferral rung above that substrate is now also landed:
  eligible cold exact-secondary inserts can persist into `index_page_delta`,
  the engine merges those rows synchronously before exact reads and write-side
  exact maintenance, and a reentrancy guard now forces TOAST-backed delta
  metadata publication back onto the normal exact insert path so forced cold
  buffering does not recurse through TOAST.
- Focused verification for that bounded runtime rung is green with `6/6`
  passing tests:
  `CatalogDatabaseBootstrapTest.CreatesIndexMetadataExtensionCatalogFamilyPages`,
  `CatalogIndexMetadataExtensionContractTest.OptionAndMaintenanceDeltaContracts`,
  `StorageEngineTest.GroupCommitQueuesOnlineMaintenanceDeltaUntilCommit`,
  `StorageEngineTest.GroupCommitRollbackDiscardsQueuedOnlineMaintenanceDelta`,
  `StorageEngineTest.ColdExactSecondaryInsertDeltaMergesOnRead`,
  and
  `StorageEngineTest.ColdExactSecondaryDeferralSkipsUniqueIndexes`.
- The remaining phase `1` structural backlog is now:
  higher-rung hot-leaf mitigation,
  broader scheduler-grade debt routing beyond the bounded per-index ledger,
  and replacing the current synchronous read/write-triggered cold-delta merge
  path with a scheduler-owned merge lane.

Decision update 2026-04-03 (bounded scheduler-owned cold exact merge lane):
- Keep the bounded cold exact-secondary delta substrate, but move the next rung
  into background ownership instead of leaving the path foreground-only.
- Background GC now drains deferred exact-secondary `index_page_delta` rows in
  a bounded per-pass merge lane. The engine carries per-index in-flight
  ownership so foreground exact reads and background GC do not double-merge the
  same logical index, and same-thread reentry now short-circuits so TOAST
  cleanup does not self-deadlock through recursive exact seeks.
- The first implementation exposed a concrete scheduler-only visibility limit:
  the background path was reading TOAST-backed delta payloads with
  `ConnectionContext::getCurrentTransactionId()` and therefore fell back to the
  bootstrap XID when no connection context existed. That made committed delta
  payloads invisible to the GC thread and failed
  `StorageEngineTest.BackgroundGcMergesDeferredExactSecondaryDeltasWithoutForegroundRead`
  with `Failed to load expression from TOAST`.
- The active fix now routes that background path through
  `StorageEngine::getCurrentXid()` so the GC merge lane uses the engine's
  runtime XID resolver rather than the connection-context-only fallback.
- Focused verification for the bounded scheduler-owned merge rung is green with
  `5/5` passing tests:
  `StorageEngineTest.GroupCommitQueuesOnlineMaintenanceDeltaUntilCommit`,
  `StorageEngineTest.GroupCommitRollbackDiscardsQueuedOnlineMaintenanceDelta`,
  `StorageEngineTest.ColdExactSecondaryInsertDeltaMergesOnRead`,
  `StorageEngineTest.BackgroundGcMergesDeferredExactSecondaryDeltasWithoutForegroundRead`,
  and
  `StorageEngineTest.ColdExactSecondaryDeferralSkipsUniqueIndexes`.
- This closes the immediate scheduler-owned merge blocker for the cold exact
  path. The remaining phase `1` structural backlog is now:
  higher-rung hot-leaf mitigation,
  and broader scheduler-grade debt routing beyond the bounded per-index ledger
  and the current bounded per-pass exact-delta merge bridge.

Decision update 2026-04-03 (bounded hot-leaf observability counters):
- Keep the existing first-rung hot-right-edge presplit mitigation, but make the
  path measurable before adding the next structural rung.
- `BTree` now records hot right-edge detections, reserved-space presplits, and
  post-split insert retries. The runtime MGA metric surface now exposes the
  counters as
  `sb_storage_hot_leaf_detections_total`,
  `sb_storage_hot_leaf_presplits_total`,
  and
  `sb_storage_hot_leaf_split_retries_total`.
- The first attempt exposed a concrete contract limit: the observability policy
  rejected a new `sb_index_*` subsystem and `index` label. The active fix kept
  the existing canonical contract by publishing the counters under the
  `storage` subsystem with the existing `db` + `relation` label set.
- The first discovery path also proved too narrow when it depended on
  `index_health` rows existing before runtime exposure. The active runtime path
  now enumerates live B-tree handles directly from catalog schemas, tables, and
  active indexes instead of treating the durable health ledger as a
  prerequisite.
- Focused verification for this observability rung is green with `3/3`
  passing tests:
  `BtreeTextRightmostRegressionTest.SequentialWideKeysPresplitHotRightmostLeafBeforeFull`,
  `MgaObservabilityLiveViewsTest.BuildsRuntimeRowsFromHotRightmostBtreeCounters`,
  and
  `MetricContractPolicyTest.MgaContractIsVersionedAndRegistryAuditPasses`.
- This closes the bounded hot-leaf counter-publication rung for phase `1`.
  The remaining hot-leaf backlog is now structural only:
  higher-rung mitigation beyond the existing right-edge reserve rule.
- Decision update 2026-04-03 (direct post-split hot-right-edge insert):
- Keep the existing right-edge reserve and presplit detection path, but stop
  treating the trigger insert itself as a mandatory root-retry case after a
  successful presplit.
- The active B-tree insert path now acquires the freshly created right leaf and
  inserts the pending monotonic key directly there when the split succeeds,
  rather than always yielding and restarting from the root.
- The follow-on cleanup now folds that pending-row placement into
  `split_leaf_page()` itself, so the hot-right-edge trigger insert no longer
  pays the extra repin/relock roundtrip on the new right leaf after a
  successful presplit.
- Focused verification is green with the same `3/3` targeted proof set, and
  `BtreeTextRightmostRegressionTest.SequentialWideKeysPresplitHotRightmostLeafBeforeFull`
  now proves the trigger insert leaves `right_edge_split_retries` unchanged
  while detection and presplit counters still advance.
- This closes the next bounded hot-leaf structural rung for phase `1`.
  The remaining open hot-leaf work is the larger locality-routing step from
  section `18`: commit-group batch apply for hot-right-edge localities and any
  broader scheduler-grade debt routing it requires.

Decision update 2026-04-03 (bounded retail-micro-batch COPY substrate):

- The active `COPY FROM` path now has a reusable `StorageEngine` bulk insert
  handle with pinned writable-page reuse and shared post-insert maintenance,
  and the executor routes the row-insert leg through that handle instead of
  paying the full page-selection reopen path for every row.
- Focused verification is green:
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`
  and
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`.
- This is intentionally recorded as a bounded phase-`2` substrate win, not as
  bulk-ingest closure. The canonical `bulk_load_*` plan/event/progress/guard
  model and explicit lane selection are still open and remain required before
  package `08` can claim section `39` closure.

Decision update 2026-04-03 (bounded retail lane planning and config closure):

- The active `COPY FROM` executor paths no longer depend on a hard-coded
  default batch size. `Executor` now resolves the retail lane batch target from
  `sb.bulk.micro_batch_target_rows`, clamps it to the canonical
  `64..65536` range, and preserves explicit `BATCH_SIZE` overrides when the
  statement supplies them.
- The first bounded phase-`2` planning seam is now explicit in code:
  `Executor::resolveCopyBulkPlan()` returns the chosen lane and batch size, and
  the live executor logs the active lane as `RETAIL_MICRO_BATCH` on both COPY
  handlers instead of silently behaving like an implicit retail path.
- Focused verification is green with `4/4` passing tests:
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`,
  `CopyExecutorTest.ResolveCopyBulkPlanUsesConfiguredRetailMicroBatchDefault`,
  `CopyExecutorTest.ResolveCopyBulkPlanHonorsExplicitBatchSizeOverride`,
  and
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`.
- This closes the bounded active-lane planning rung, not section `39`.
  The remaining Beta 1 bulk-ingest gap is still the durable lane model:
  `bulk_load_*` plan/event/progress/guard rows and real lane selection between
  `RETAIL_MICRO_BATCH`, `SORTED_EXACT_BULK`, and `SHADOW_LOAD_CUTOVER`.

Decision update 2026-04-03 (bounded retail durable bulk-load catalog closure):

- The active V3 `COPY FROM` retail lane now publishes durable canonical
  `bulk_load_*` state instead of only planning locally in executor memory.
  `CatalogManager` persists `bulk_load_plan`, `bulk_load_event`,
  `bulk_load_progress`, and `bulk_load_cutover_guard`, and the retail lane now
  records the active plan, transition events, row-count progress, and
  `NOT_REQUIRED` guard state for the bounded retail path.
- Focused verification is green with `5/5` passing tests:
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`,
  `CopyExecutorTest.ResolveCopyBulkPlanUsesConfiguredRetailMicroBatchDefault`,
  `CopyExecutorTest.ResolveCopyBulkPlanHonorsExplicitBatchSizeOverride`,
  `CopyExecutorTest.CopyFromPublishesRetailBulkLoadCatalogState`,
  and
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`.
- A validation limit was also confirmed during this slice:
  targeted rebuilds can fail at GoogleTest discovery with `text file is busy`
  when a stale `scratchbird_tests` holder from an older shell session still
  has the binary open. Clearing that stale holder is now part of package `08`
  execution hygiene before treating a focused rebuild as authoritative.
- This closes the bounded retail durable-state rung, not section `39`.
  The remaining Beta 1 bulk-ingest gap is still real lane selection and
  matching durable-state publication for `SORTED_EXACT_BULK` and
  `SHADOW_LOAD_CUTOVER`, plus any remaining non-V3 load-path parity work.

Decision update 2026-04-03 (bounded retail durable-state parity across live COPY paths):

- The shared executor now mirrors the same durable retail `bulk_load_*`
  publication path on the remaining live non-V3 `COPY FROM` branch. Both live
  `COPY FROM` surfaces now create a retail bulk-load plan, append
  `COPY_FROM_STARTED` and `COPY_FROM_COMPLETED`, update row-count progress, and
  publish a `NOT_REQUIRED` cutover guard for the bounded retail lane.
- This slice was compile-validated by a clean `scratchbird_tests` rebuild and
  regression-validated by rerunning the existing focused `5/5` copy and
  bulk-handle proof set. There is still no dedicated legacy-copy unit harness,
  so this parity step is recorded as shared-surface validation rather than a
  branch-isolated proof.
- This closes the non-V3 retail durable-state parity gap. The remaining Beta 1
  bulk-ingest gap is now narrower: actual lane selection and matching durable
  execution state for `SORTED_EXACT_BULK` and `SHADOW_LOAD_CUTOVER`.

Decision update 2026-04-03 (bounded sorted-exact lane selection on COPY FROM):

- Both live `COPY FROM` handlers now classify file-backed loads against the
  canonical `sb.bulk.sorted_exact_min_rows` threshold and active exact-key
  B-tree candidates instead of hard-coding the retail lane.
- The active V3 `COPY FROM` path now stages file-backed `TEXT` and `CSV` rows,
  derives canonical exact-key bytes from the selected candidate, sorts the
  staged rows before flush, and publishes `SORTED_EXACT_BULK` when the bounded
  threshold is met.
- Focused verification is green with `7/7` passing tests:
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`,
  `CopyExecutorTest.ResolveCopyBulkPlanUsesConfiguredRetailMicroBatchDefault`,
  `CopyExecutorTest.ResolveCopyBulkPlanHonorsExplicitBatchSizeOverride`,
  `CopyExecutorTest.ClassifyCopyBulkLaneChoosesSortedExactBulkAtThreshold`,
  `CopyExecutorTest.CopyFromPublishesRetailBulkLoadCatalogState`,
  `CopyExecutorTest.CopyFromPublishesSortedExactBulkLoadCatalogState`,
  and
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`.
- The bounded proof also confirmed an intended guardrail:
  `sb.bulk.sorted_exact_min_rows` remains clamped to `1000..10000000`, so the
  proof had to exercise a `1000`-row file-backed load instead of weakening the
  runtime threshold.
- This closes the bounded sorted-exact selection rung, not section `39`.
  The remaining Beta 1 bulk-ingest gap is still real `SHADOW_LOAD_CUTOVER`
  execution and branch-isolated non-V3 proof for sorted/shadow lane behavior.

Decision update 2026-04-03 (reachable shadow-load lane with fail-closed publication):

- The active V3 `COPY FROM` path now accepts `WITH (SHADOW_LOAD true)`,
  routes that request to the canonical `SHADOW_LOAD_CUTOVER` lane, and
  publishes durable bulk-load state for the request instead of leaving the
  lane as an unreachable enum value.
- Because the runtime still lacks object-level shadow target creation and
  commit-bound binding swap, the requested shadow lane now fails closed:
  it writes a `bulk_load_plan` row with ingest lane
  `SHADOW_LOAD_CUTOVER`, a `BLOCKED` `bulk_load_cutover_guard`, and a single
  `PLANNED -> ABORTED_FAIL_CLOSED` event with code
  `SHADOW_LOAD_CUTOVER_UNAVAILABLE`, then returns an explicit runtime error.
- Focused verification is green with `6/6` passing tests:
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`,
  `CopyExecutorTest.CopyFromPublishesRetailBulkLoadCatalogState`,
  `CopyExecutorTest.CopyFromPublishesSortedExactBulkLoadCatalogState`,
  `CopyExecutorTest.ClassifyCopyBulkLaneChoosesShadowCutoverWhenRequested`,
  `CopyExecutorTest.CopyFromShadowLoadRequestPublishesBlockedCutoverStateAndFailsClosed`,
  and
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`.
- This removes the “unreachable lane” ambiguity from section `39`, but it
  does not close section `39`. The remaining Beta 1 gap is still real
  object-level `SHADOW_LOAD_CUTOVER` execution and publication, plus
  branch-isolated non-V3 proof for sorted/shadow lane behavior.

Decision update 2026-04-03 (durable bulk-load plan phase-state truthfulness):

- `bulk_load_plan` rows were still weaker than canon because the durable plan
  record stayed at `PLANNED` even after `COPY FROM` had published
  `RUNNING -> COMPLETED` or `PLANNED -> ABORTED_FAIL_CLOSED` events.
- `CatalogManager` now exposes a narrow `bulk_load_plan.phase_state` updater,
  and both live `COPY FROM` handlers use it so the durable plan row advances
  through `RUNNING`, `COMPLETED`, or `ABORTED_FAIL_CLOSED` in line with the
  already-published event stream.
- Focused verification is green with `9/9` passing tests:
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`,
  `CopyExecutorTest.ResolveCopyBulkPlanUsesConfiguredRetailMicroBatchDefault`,
  `CopyExecutorTest.ResolveCopyBulkPlanHonorsExplicitBatchSizeOverride`,
  `CopyExecutorTest.ClassifyCopyBulkLaneChoosesSortedExactBulkAtThreshold`,
  `CopyExecutorTest.ClassifyCopyBulkLaneChoosesShadowCutoverWhenRequested`,
  `CopyExecutorTest.CopyFromPublishesRetailBulkLoadCatalogState`,
  `CopyExecutorTest.CopyFromPublishesSortedExactBulkLoadCatalogState`,
  `CopyExecutorTest.CopyFromShadowLoadRequestPublishesBlockedCutoverStateAndFailsClosed`,
  and
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`.
- This closes the stale-plan-state observability gap, not section `39`.
  The remaining Beta 1 bulk-ingest gap is still real object-level
  `SHADOW_LOAD_CUTOVER` execution and publication, plus branch-isolated
  non-V3 proof for sorted/shadow lane behavior.

Decision update 2026-04-03 (branch-isolated non-V3 COPY proof closure):

- The PostgreSQL parser-emitted fixed-format `COPY` stream now matches the
  shared executor compatibility contract closely enough for bounded proof:
  it accepts `HEADER true` without forcing `=`, recognizes `SHADOW_LOAD`,
  and emits the trailing `BATCH_SIZE`, `MAX_ERRORS`, and `ON_ERROR` defaults
  that the shared non-V3 `EXT_COPY` path already reads.
- Focused verification is green with `15/15` passing tests across the full
  COPY-focused proof set, including the two new legacy PostgreSQL proofs:
  `CopyExecutorTest.LegacyCopyFromPublishesSortedExactBulkLoadCatalogState`
  and
  `CopyExecutorTest.LegacyCopyFromShadowLoadRequestPublishesBlockedCutoverStateAndFailsClosed`,
  plus the existing retail, sorted, shadow fail-closed, encoding, binary,
  header, and bulk-handle regressions.
- A validation nuance is now explicit in package records:
  raw parser-emitted legacy bytecode still carries `EXT_DEBUG_SPAN` markers,
  while compatibility-mode execution is intentionally narrower than the
  canonical container path. The bounded legacy proof strips that parser-only
  marker before enabling `SCRATCHBIRD_ENABLE_LEGACY_EXECUTE`; this is
  test-harness hygiene, not a new runtime defect.
- This closes the branch-isolated non-V3 proof gap for section `39`.
  The remaining Beta 1 bulk-ingest gap is now only real object-level
  `SHADOW_LOAD_CUTOVER` execution and publication.

Decision update 2026-04-03 (object-level shadow-load cutover execution fixed):

- The remaining bounded section `39` execution gap was real and is now closed
  on both live `COPY FROM` paths. `COPY ... WITH (SHADOW_LOAD true)` now
  completes committed cutover execution instead of only publishing a
  fail-closed placeholder.
- The blocking runtime defect was in heap tuple identity on non-primary pages,
  not in lane planning or durable bulk-load state. The shadow-loaded rows were
  physically present in the target tablespace, but `HeapPage` still stamped
  their `ctid_gpid` values against the primary tablespace. Visible scans then
  rejected those rows because their `ctid` no longer matched the owning target
  page TID.
- The heap substrate now carries the owning page tablespace through tuple
  identity stamping and traversal, and the active non-primary insert, scan,
  and delete paths pass that tablespace explicitly on the shadow-cutover path.
- Focused verification is green with `2/2` on the committed-cutover shadow
  proofs and `9/9` on the broader `COPY`/bulk-handle regression slice.
- This closes the bounded section `39` `COPY` lane closure for package `08`.
  The full expanded workplan remains open on later phases outside this lane
  family: online build/publication, metrics freshness, memory admission,
  planner parity, and analytical/runtime closure.

Decision update 2026-04-03 (durable `index_build_*` shadow-path proof and reopen fix):

- The current shadow-index rebuild path now publishes durable
  `index_build_plan`, `index_build_event`, `index_build_progress`, and
  `index_build_cutover_guard` rows, and the catalog root now persists the
  page ids for those four families.
- The first reopen proof failed, but the failure was not loss of the new page
  ids. Reopen failed with
  `Duplicate object name in scope: type=3 name=users.public.test_table.test_index`
  because generic resolver rebuild still treated every physical index version
  as a user-visible named object.
- The bounded runtime fix keeps generic object-name resolution on the
  published `ACTIVE` index version only. `BUILDING` and `RETIRED` physical
  rows still participate in version-aware index lookup through `index_cache_`
  and `getVisibleIndexVersion()`, but they no longer corrupt generic object
  rebuild on reopen.
- Focused verification is green with `3/3` passing integration tests:
  `ShadowIndexRebuildTest.BasicShadowCreation`,
  `ShadowIndexRebuildTest.ShadowPromotion`,
  and
  `ShadowIndexRebuildTest.PublishesDurableIndexBuildCatalogState`.
- This closes a bounded phase-`3` durability/proof gap on the existing shadow
  rebuild path. The wider expanded workplan remains open on later phase-`3`
  visibility/publication semantics and the remaining later performance phases.

Decision update 2026-04-03 (clean rebuild benchmark rerun and current delta):

- A fresh comparable benchmark rerun now exists at
  `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/clean-rebuild-scratchbird-stress-20260404T023343Z`.
  The engine build tree was deleted and rebuilt from a clean `Release`
  configure before the run, so these numbers are not incremental-build carry
  forward.
- Benchmark runtime bring-up exposed a reproducibility dependency rather than a
  product regression: the local benchmark launcher still assumes a native
  `sb_isql` path from the `ScratchBird-driver` CLI lane. The rerun therefore
  rebuilt `sb_isql` under `ScratchBird-driver/build_cli` and started the
  runtime with
  `SCRATCHBIRD_SB_ISQL=/home/dcalford/CliWork/ScratchBird-driver/build_cli/tracks/p3/drivers/cli/sb_isql`.
- Compared with the preserved comparable baseline in
  `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/txmode-matrix-20260403T152011Z/scratchbird/stress`,
  total load time improved in both supported ScratchBird modes:
  `36.96s -> 34.99s` in `normal_transactional` and
  `46.51s -> 41.63s` in `autocommit`.
- Query-path impact remains mixed. Improvements held on
  `self_join_same_country`, `inner_join_large_result`, and
  `bulk_update_with_join` in both modes, and `bulk_update_with_case` improved
  slightly in `normal_transactional`. Regressions remain visible on
  `bulk_insert_select` in both modes and on `bulk_update_with_case` in
  `autocommit`; `multi_dimensional_agg` regressed in
  `normal_transactional` while slightly improving in `autocommit`.
- Decision: keep using this clean rerun as the new local reference point for
  package `08` work, but do not treat it as later-phase closure. The recent
  section `18/39` work appears to have improved the bounded load surface
  without hiding the remaining analytical/query hot-path gaps, so the next
  engineering priority stays on the later planner/runtime phases rather than
  re-opening the already-landed bulk-lane work without stronger evidence.

Decision update 2026-04-03 (bounded phase `4` metrics freshness publication and planner refusal):

- The first phase-`4` slice is now live on the active optimizer path.
  `StatisticsManager` publishes `metrics_publication_epoch`,
  `metrics_freshness_class`, `metrics_invalidation_state`, and
  `metrics_invalidation_reason` into the canonical family-metrics packet and
  shared metrics envelope, with backward-compatible derivation for older
  payloads that do not yet contain those keys.
- Runtime-plan relations, access-path descriptors, and the optimizer-side
  statistics signature now preserve those fields end-to-end, so later plan
  selection can reason on the published freshness state instead of only on the
  older confidence/queryability pair.
- The first refusal behavior is intentionally bounded: `UNUSABLE` freshness or
  `INVALIDATED_HARD` now refuses that family for winner selection and pushes
  the relation back to `SEQ_SCAN`. This is enough to make hard-invalidated
  family metrics fail closed, but it is not yet the full explicit
  winner-or-refusal bundle required by the wider phase-`4`/phase-`6` work.
- One test expectation had to be corrected to stay honest about the current
  semantics. A direct `refreshIndexFamilyMetrics()` call on the ART path is an
  unsampled/medium-confidence refresh, so the canonical publication is
  `AGED`, not `CURRENT`. The sampled `ANALYZE INDEX ... WITH (sample_rate =
  0.25)` path still publishes `CURRENT` as expected.
- Focused proof is green with `3/3` tests:
  `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
  `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
  and
  `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
- Decision: treat this as bounded phase-`4` progress only. Do not mark phase
  `4` closed until candidate bundles can preserve explicit refusal classes and
  reasons for all implemented families, not just fail closed on hard
  invalidation.

Decision update 2026-04-03 (bounded explicit-refusal bundle preservation):

- `RuntimePlanRelation` now preserves structured
  `candidate_family_refusals` entries with family, candidate label, refusal
  class, refusal cause domain, reason code, and detail, and the
  base-relation candidate bundle now carries those refusals into the chosen
  runtime relation instead of dropping them once the winner falls back to
  `SEQ_SCAN`.
- The first proof attempt exposed a real fidelity bug rather than a bogus
  test assumption: maintenance-state fail-closed refusals already knew when
  the family metrics were hard-invalidated, but the stored refusal detail only
  recorded `maintenance state FAILED incompatible with trust class ...`. That
  detail was too lossy for canonical metrics classification and degraded the
  refusal to a generic policy bucket.
- The bounded runtime fix keeps the existing fail-closed decision but now
  records the full lifecycle snapshot in those maintenance-state refusal
  details, including freshness and invalidation fields. That lets canonical
  bundle classification promote hard-invalidated or `UNUSABLE` cases into the
  expected `unusable metrics` / `METRICS` refusal shape without inventing a
  new decision path.
- Focused verification is green on both the planner/runtime and shadow-index
  guard slices: `3/3` passing tests in `scratchbird_tests` for the
  planner/statistics proof set and `8/8` passing tests in the standalone
  `test_shadow_index_rebuild` integration target.
- Decision: count this as the first structured-refusal preservation rung, not
  phase-`4`/phase-`6` closure. The remaining optimizer parity gap is still
  broader: implemented-family-wide winner-or-refusal coverage, full refusal
  publication across all candidate families, and later replan-trigger work.

Decision update 2026-04-03 (bounded MGA governance refusal preservation):

- MGA churn/governance rejections no longer disappear from the preserved
  candidate bundle. The planner now records those rejections as structured
  `candidate_family_refusals` entries with reason code
  `P08_MGA_GOVERNANCE_REJECTED`, instead of only emitting an
  `MGA_SWITCHOVER` runtime-trace row.
- The runtime trace semantics are preserved on purpose. The helper now lets a
  structured refusal keep its original rejection phase, so the candidate
  bundle and `rejected_paths` stay aligned rather than forcing MGA rejections
  into a generic `ACCESS_PATH` bucket.
- Focused verification is green with `5/5` planner/statistics tests in
  `scratchbird_tests`, including the severe-churn MGA rejection and exact
  probe preservation proofs, and `8/8` passing tests in the standalone
  `test_shadow_index_rebuild` integration target.
- Decision: count this as another bounded phase-`4`/phase-`6` optimizer-parity
  improvement only. Broader implemented-family refusal coverage, replan
  triggers, and the later memory-admission work still remain open.

Decision update 2026-04-03 (bounded cost-domain refusal preservation):

- Implemented-family paths that lose because the current best path is cheaper,
  or because published family metrics mark an exact path structurally
  overweight, no longer disappear from the structured candidate bundle.
- The planner now records those cases as
  `candidate_family_refusals` entries with explicit `COST` cause domain:
  `P08_HIGHER_TOTAL_COST_THAN_CURRENT_BEST` maps to
  `higher estimated cost than current winner`, and
  `P08_STRUCTURALLY_OVERWEIGHT_EXACT_PATH` maps to
  `structurally overweight path under current metrics`.
- The first proof attempt exposed a real execution detail rather than a bad
  test: the expensive exact-family fallback was not hitting the generic
  cost-loser path. It was being refused earlier as a structurally overweight
  exact path, so that earlier rejection also had to move onto the structured
  refusal path.
- Focused verification is green with `6/6` tests in `scratchbird_tests`,
  including the expensive exact-family fallback proof and the existing MGA and
  metrics-freshness regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6` parity step
  only. Wider implemented-family refusal coverage, replan-trigger behavior,
  and later memory-admission work are still open.

Decision update 2026-04-03 (bounded semantic-mismatch refusal preservation):

- Base-relation predicates that do not match any available index no longer
  disappear from the structured candidate bundle when the planner falls back
  to `SEQ_SCAN`.
- The planner now records those cases as
  `candidate_family_refusals` entries with `SEMANTICS` cause domain:
  `P08_NO_MATCHING_INDEX_FOR_PREDICATE` maps to `semantic mismatch`, and the
  refusal detail preserves the original predicate text.
- Focused verification is green with `7/7` tests in `scratchbird_tests`,
  covering the new non-indexed predicate proof plus the current MGA,
  cost-domain, and metrics-freshness regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6` optimizer-parity
  step only. Broader implemented-family refusal coverage, replan-trigger
  behavior, and later memory-admission work are still open.

Decision update 2026-04-04 (bounded native-promotion refusal preservation):

- Enumerated native-family candidates that fail their current promotion
  threshold no longer disappear into raw trace only. The planner now preserves
  those rejections as structured `candidate_family_refusals` entries.
- The current bounded family covers
  `P08_*_NATIVE_PROMOTION_THRESHOLD_NOT_MET`,
  `P08_ANN_HYBRID_FALLBACK_NOT_JUSTIFIED`, and
  `P08_TEXT_SCORE_ROWS_REQUIRED`, all mapped to
  `family-specific promotion threshold not met` with `METRICS` cause domain.
- Focused verification is green with `8/8` tests in `scratchbird_tests`,
  including a BRIN proof where published summary metrics force `SEQ_SCAN`
  while preserving `P08_SUMMARY_NATIVE_PROMOTION_THRESHOLD_NOT_MET` in the
  candidate bundle.
- Decision: count this as another bounded phase-`4`/phase-`6` optimizer-parity
  step only. Broader family-wide refusal coverage, replan-trigger behavior,
  and later memory-admission work are still open.

Decision update 2026-04-04 (bounded generalized lowering refusal preservation):

- Invalid generalized/spatial lowering states no longer collapse into generic
  `P08_FAMILY_NOT_QUERYABLE` when the lowering helper has enough information
  to classify the failure more precisely.
- The current bounded family distinguishes
  `P08_OPERATOR_STRATEGY_UNBOUND`,
  `P08_SUPPORT_FUNCTION_UNVALIDATED`,
  `P08_DISTANCE_SUPPORT_UNVALIDATED`, and
  `P08_NEAREST_ORDER_UNVALIDATED`, and the planner maps those generalized
  invalid shapes to `unsupported operator shape` or
  `missing required runtime capability` instead of a generic policy refusal.
- The first integration proof attempt exposed a real planner boundary rather
  than a runtime bug: before validated opclass support exists, GIST still
  fails closed one stage earlier as
  `P08_NO_MATCHING_INDEX_FOR_PREDICATE`. The proof was updated to reflect
  that actual pre-support semantic gate, then verify that `GIST_SCAN` enters
  the candidate family set once the bound support function is published.
- Focused verification is green with `5/5` tests in `scratchbird_tests`,
  including the new generalized invalid-case lowering proof and the updated
  GIST opclass-support integration proof.
- Decision: count this as another bounded phase-`4`/phase-`6` optimizer-parity
  step only. Broader family-wide refusal coverage, replan-trigger behavior,
  and later memory-admission work are still open.

Decision update 2026-04-04 (bounded ANN lowering refusal preservation):

- ANN-family invalid lowering states no longer collapse into generic
  `P08_FAMILY_NOT_QUERYABLE` when the lowering helper has enough information
  to classify the failure more precisely.
- The current bounded family distinguishes
  `P08_ANN_NEAREST_ORDER_REQUIRED`,
  `P08_ANN_METRIC_INCOMPATIBLE`, and
  `P08_ANN_CANDIDATE_BUDGET_REQUIRED`, and the planner maps those bounded ANN
  invalid shapes to `missing required runtime capability` instead of a
  generic policy refusal.
- Focused verification is green with `6/6` tests in `scratchbird_tests`,
  including the new ANN invalid-case lowering proof plus the current
  generalized, semantic-mismatch, promotion-threshold, and GIST-boundary
  regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6` optimizer-parity
  step only. Broader family-wide refusal coverage, replan-trigger behavior,
  and later memory-admission work are still open.

Decision update 2026-04-04 (bounded ranked-text lowering refusal preservation):

- Ranked-text invalid lowering states no longer collapse into generic
  `P08_FAMILY_NOT_QUERYABLE` when the lowering helper has enough information
  to classify the failure more precisely.
- The current bounded family distinguishes
  `P08_TEXT_CORPUS_STATS_REQUIRED` and
  `P08_TEXT_CANDIDATE_BUDGET_REQUIRED`, and the planner maps those bounded
  ranked-text invalid shapes to `missing required runtime capability`
  instead of a generic policy refusal.
- Focused verification is green with `7/7` tests in `scratchbird_tests`,
  including the new ranked-text invalid-case lowering proof plus the current
  generalized, ANN, semantic-mismatch, promotion-threshold, and GIST-boundary
  regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6` optimizer-parity
  step only. Broader family-wide refusal coverage, replan-trigger behavior,
  and later memory-admission work are still open.

Decision update 2026-04-04 (bounded hash-family lowering refusal preservation):

- Non-equality hash-family predicates no longer collapse into generic
  `P08_FAMILY_NOT_QUERYABLE` when the lowering helper has enough information
  to classify the failure more precisely.
- The current bounded family distinguishes
  `P08_HASH_EQ_PREDICATE_REQUIRED`, and the planner maps that bounded
  hash-family invalid shape to `unsupported operator shape` instead of a
  generic policy refusal.
- Focused verification is green with `8/8` tests in `scratchbird_tests`,
  including the new hash invalid-case lowering proof plus the current
  generalized, ANN, ranked-text, semantic-mismatch, promotion-threshold, and
  GIST-boundary regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6` optimizer-parity
  step only. Broader family-wide refusal coverage, replan-trigger behavior,
  and later memory-admission work are still open.

Decision update 2026-04-04 (bounded family-native metrics provenance closure):

- Runtime plans and `EXPLAIN (FORMAT JSON)` now publish dedicated
  `FAMILY_NATIVE_METRICS` statistics provenance rows for published family
  metrics instead of exposing freshness only indirectly through relation
  fields and family-signature strings.
- The new bounded proof surface carries the family name, freshness class,
  invalidation state, maintenance state, publication epoch, and explicit
  current-compile policy markers:
  `refresh_attempted=false`,
  `refresh_policy=ASYNC_OR_ADMIN`,
  and
  `replan_boundary=FAMILY_STATISTICS_SIGNATURE`.
- Focused verification is green with `10/10` tests in `scratchbird_tests`,
  including typed family metrics runtime-plan proof, `EXPLAIN` JSON proof,
  the fail-closed unusable-metrics case, and the current generalized, ANN,
  ranked-text, hash, semantic-mismatch, promotion-threshold, and GIST-boundary
  regression slice.
- Decision: count this as a bounded phase-`4` freshness/explain closure step
  only. Broader family-wide refusal coverage, explicit replan-trigger
  behavior, and later memory-admission work are still open.

Decision update 2026-04-04 (bounded maintenance/trust refusal mapping closure):

- The planner bundle-refusal classifier is now exposed through reusable
  canonical helpers instead of being trapped inside a query-planner local
  lambda. This keeps the structured refusal vocabulary deterministic and
  directly provable.
- `P08_MAINTENANCE_STATE_INCOMPATIBLE` now has bounded proof as
  `maintenance state incompatible with trust class` with `METRICS` cause
  domain, and `P08_TRUST_LOCATOR_UNDECLARED` now has bounded proof as
  `missing trust or locator classification` with `POLICY` cause domain.
- The attempted ANN integration proof was not retained: the current v3
  compiler surface rejected the nearest-neighbor query shape at parse time, so
  the preserved evidence for this slice intentionally stays at the canonical
  helper and planner-regression level.
- Focused verification is green with `11/11` tests in `scratchbird_tests`,
  including the new
  `QueryPlannerIntegrationTest.AgedPublishedFamilyMetricsPreserveMaintenanceStateRefusal`
  helper-backed proof and the current typed-metrics, unusable-metrics,
  generalized, ANN, ranked-text, hash, semantic-mismatch,
  promotion-threshold, and GIST-boundary regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6`
  refusal-fidelity step only. Broader family-wide refusal coverage, explicit
  replan-trigger behavior, and later memory-admission work are still open.

Decision update 2026-04-04 (bounded MGA governance refusal closure):

- `P08_MGA_GOVERNANCE_REJECTED` no longer inherits the generic
  `fail-closed family-specific safety rule` class. It now preserves a
  dedicated structured refusal:
  `MGA governance rejected candidate under current pressure`.
- The bounded cause domain remains `POLICY`, because this path is still a
  governance switchover decision rather than a cost winner or semantic
  mismatch.
- Focused verification is green with `13/13` tests in `scratchbird_tests`,
  including the severe-churn broad covering-index rejection proof, the
  exact-covering-preservation proof, and the current typed-metrics,
  unusable-metrics, maintenance/trust, generalized, ANN, ranked-text, hash,
  semantic-mismatch, promotion-threshold, and GIST-boundary regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6`
  optimizer-parity step only. Broader family-wide refusal coverage, explicit
  replan-trigger behavior, and later memory-admission work are still open.

Decision update 2026-04-04 (bounded family-statistics replan boundary closure):

- Family-statistics signature churn now publishes an explicit runtime-plan
  replan proof instead of looking like an unexplained plan-cache miss. The
  returned compiled payload now carries `FAMILY_STATISTICS_REPLAN`
  statistics provenance, a `PLAN_CACHE/CACHE_REUSE/REJECTED` trace row for
  `family statistics signature boundary crossed`, and the optimizer control
  `FAMILY_STATISTICS_REPLAN_BOUNDARY=FAMILY_STATISTICS_SIGNATURE`.
- The actual defect was not the annotation layer. The first implementation
  incorrectly required sibling cache entries to have the same
  `cost_profile_id`, but the existing family-metrics refresh path already
  recalibrates cost profile across the same bounded
  `FAMILY_STATISTICS_SIGNATURE` boundary. That caused the second compile to
  miss the cache and replan correctly while still failing to publish the
  explicit proof row.
- Focused verification is green with `11/11` tests in `scratchbird_tests`,
  including the new
  `QueryPlannerIntegrationTest.FamilyStatisticsSignatureBoundaryPublishesExplicitReplanProof`
  plus the current typed-metrics, unusable-metrics, maintenance/trust,
  semantic-mismatch, promotion-threshold, GIST-boundary, severe-churn MGA,
  and statistics-manager regression slice.
- Decision: count this as the first bounded explicit phase-`4`
  replan-trigger closure step only. Broader family-wide refusal coverage,
  wider replan-trigger behavior, and later memory-admission work are still
  open.

Decision update 2026-04-04 (bounded family-legality refusal classification closure):

- The canonical bundle-refusal helper no longer lets family-legality failures
  collapse into the generic fail-closed policy bucket. It now preserves
  `P08_FAMILY_LEGALITY_UNDECLARED` as
  `missing canonical family legality classification`,
  `P08_FAMILY_LEGALITY_TRUST` as
  `trust class violates canonical family legality matrix`,
  `P08_FAMILY_LEGALITY_LOCATOR` as
  `locator granularity violates canonical family legality matrix`,
  and `P08_FAMILY_LEGALITY_VISIBILITY` as
  `visibility enforcement violates canonical family legality matrix`.
- The bounded cause domain remains `POLICY`, which matches the current
  fail-closed family-admission boundary for legality checks.
- Focused verification is green with `16/16` tests in `scratchbird_tests`,
  including the new
  `IndexFamilyLoweringTest.CanonicalPlannerBundleRefusalClassifiesFamilyLegalityFailures`
  proof plus the current family-metrics, replan-boundary, semantic-mismatch,
  promotion-threshold, GIST-boundary, generalized, ANN, ranked-text, hash,
  severe-churn MGA, and statistics-manager regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6`
  refusal-fidelity step only. Broader implemented-family refusal coverage,
  wider replan-trigger behavior, and later memory-admission work are still
  open.

Decision update 2026-04-04 (bounded same-column lowering refusal preservation):

- The unmatched-predicate prepass no longer drops directly to
  `P08_NO_MATCHING_INDEX_FOR_PREDICATE` when the relation already has indexes
  on the same predicate column. It now inspects those same-column indexes and
  preserves more specific refusal semantics first.
- The bounded planner behavior now keeps
  `P08_HASH_EQ_PREDICATE_REQUIRED` for hash range predicates,
  `P08_PARTIAL_INDEX_PREDICATE_MISMATCH` for partial-index implication
  failures, and the stricter generalized-family lowering refusal
  `P08_OPERATOR_STRATEGY_UNBOUND` on the GIST boundary instead of collapsing
  those cases into generic unmatched-predicate fallback.
- Focused verification is green with `13/13` tests in `scratchbird_tests`,
  including the new
  `QueryPlannerIntegrationTest.HashRangePredicateFailsClosedToSequentialScan`
  and
  `QueryPlannerIntegrationTest.PartialIndexRequiresPredicateImplicationBeforeEnumeration`
  proofs plus the current family-metrics, replan-boundary, semantic-mismatch,
  promotion-threshold, GIST-boundary, severe-churn MGA, and
  statistics-manager regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6`
  optimizer-parity step only. Broader implemented-family refusal coverage,
  wider replan-trigger behavior, and later memory-admission work are still
  open.

Decision update 2026-04-04 (bounded bitmap and columnstore promotion-threshold proof closure):

- The planner already emitted
  `P08_BITMAP_NATIVE_PROMOTION_THRESHOLD_NOT_MET` and
  `P08_COLUMNSTORE_NATIVE_PROMOTION_THRESHOLD_NOT_MET`, but those family
  paths did not yet have preserved integration proof at the runtime-plan
  boundary.
- Focused planner proof now shows both families remain in
  `candidate_scan_families`, fall back to `SEQ_SCAN`, and preserve structured
  `candidate_family_refusals` entries with
  `family-specific promotion threshold not met` in the `METRICS` cause
  domain.
- Focused verification is green with `14/14` tests in `scratchbird_tests`,
  including the new
  `QueryPlannerIntegrationTest.BitmapAndColumnstoreFamiliesBelowPromotionThresholdPublishStructuredRefusals`
  proof plus the current summary-threshold, same-column lowering,
  replan-boundary, GIST-boundary, severe-churn MGA, and statistics-manager
  regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6`
  optimizer-parity step only. Broader implemented-family refusal coverage,
  wider replan-trigger behavior, and later memory-admission work are still
  open.

Decision update 2026-04-04 (bounded ranked-text promotion-threshold proof closure):

- The planner already emitted
  `P08_TEXT_NATIVE_PROMOTION_THRESHOLD_NOT_MET`, but that ranked-text path
  did not yet have preserved integration proof at the runtime-plan boundary.
- Focused planner proof now shows the text family remains in
  `candidate_scan_families`, falls back to `SEQ_SCAN`, and preserves a
  structured `candidate_family_refusals` entry for
  `TEXT_BITMAP_SCAN[idx_docs_title_text]` with
  `family-specific promotion threshold not met` in the `METRICS` cause
  domain.
- Focused verification is green with `15/15` tests in `scratchbird_tests`,
  including the new
  `QueryPlannerIntegrationTest.TextFamilyBelowPromotionThresholdPublishesStructuredRefusal`
  proof plus the current summary-threshold, bitmap/columnstore threshold,
  same-column lowering, replan-boundary, GIST-boundary, severe-churn MGA,
  and statistics-manager regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6`
  optimizer-parity step only. Broader implemented-family refusal coverage,
  wider replan-trigger behavior, and later memory-admission work are still
  open.

Decision update 2026-04-04 (bounded promotion-threshold classifier closure):

- The next unproved family slice was originally targeted as a direct ANN
  runtime-plan proof, but the current vector query surface blocked that path:
  ANN candidate enumeration is predicate-driven, the accepted nearest-order
  detector did not recognize the attempted `VECTOR_L2_DISTANCE(...)` form on
  this path, and the compatibility-style `<-> (SELECT ...)` variant still
  failed at parse time.
- Instead of papering over that boundary, this slice closed the planner
  telemetry contract directly. Focused proof now shows
  `P08_ANN_NATIVE_PROMOTION_THRESHOLD_NOT_MET`,
  `P08_ANN_HYBRID_FALLBACK_NOT_JUSTIFIED`,
  `P08_GENERALIZED_NEAREST_NATIVE_PROMOTION_THRESHOLD_NOT_MET`,
  `P08_GENERALIZED_NATIVE_PROMOTION_THRESHOLD_NOT_MET`, and
  `P08_TEXT_SCORE_ROWS_REQUIRED` all normalize to
  `family-specific promotion threshold not met` with `METRICS` cause domain.
- Focused verification is green with `16/16` tests in `scratchbird_tests`,
  including the new
  `QueryPlannerIntegrationTest.CanonicalPlannerBundleRefusalClassifiesAdditionalPromotionThresholdFailures`
  proof plus the current ranked-text, bitmap/columnstore, summary-threshold,
  same-column lowering, replan-boundary, GIST-boundary, severe-churn MGA,
  and statistics-manager regression slice.
- Decision: count this as another bounded phase-`4`/phase-`6`
  optimizer-parity step only, and preserve the ANN query-surface boundary as
  a real remaining gap rather than claiming direct ANN runtime-plan proof.

Decision update 2026-04-04 (remaining bounded phase-`4`/phase-`6` slice closure):

- The last uncovered canonical family-failure mappings are now preserved in
  proof instead of only existing as helper code:
  `P08_SUPPORT_FUNCTION_UNVALIDATED`,
  `P08_BITMAP_COMPOSE_UNAVAILABLE`,
  `P08_SKIP_SCAN_UNAVAILABLE`,
  `P08_EXPRESSION_INDEX_MISMATCH`, and
  `P08_FAMILY_NOT_QUERYABLE`.
- The same focused rerun also carried both explicit replan trigger families
  together, using the already-live adaptive-cardinality and
  family-statistics boundary proofs:
  `QueryPlannerIntegrationTest.CardinalityFeedbackBypassesStaleCacheAndRebuildsPlan`,
  `QueryPlannerIntegrationTest.AdaptiveFeedbackCachedPlanReflectsLatestFeedbackStateAfterRepeatExecution`,
  and
  `QueryPlannerIntegrationTest.FamilyStatisticsSignatureBoundaryPublishesExplicitReplanProof`.
- Focused verification is green with `23/23` tests in `scratchbird_tests`.
- Decision: treat the package-owned phase-`4`/phase-`6`
  refusal-model and replan-trigger slices as closed. Preserve the ANN query
  surface as a separate boundary only where direct nearest-order runtime-plan
  proof is still parser/enumeration-limited; do not keep using it as a reason
  to leave the broader refusal-model slice open. The active frontier now
  moves to phase `5` memory-grant and spill-admission work.

Decision update 2026-04-04 (first bounded phase-`5` durability slice closure):

- The first bounded phase-`5` step is now durable rather than speculative:
  `CatalogManager` persists canonical `memory_grant_feedback` rows through the
  catalog root with keyed upsert/get/list/delete helpers.
- Focused proof shows the new family rejects invalid states, preserves the
  original UUID and `created_time` across same-key updates, survives reopen,
  and deletes cleanly by `grant_key_hash`.
- Focused verification is green after rebuild with `1/1` passing tests for
  `CatalogRoutingAdmissionExtensionContractTest.MemoryGrantFeedbackCatalogContracts`,
  and the widened
  `CatalogRoutingAdmissionExtensionContractTest.*` slice is also green with
  `3/3` passing tests in `scratchbird_tests`.
- Decision: count this as the first bounded phase-`5` catalog durability rung
  only. Do not overstate it as runtime grant admission or spill execution;
  feedback-driven sizing, operator reservation, and runtime spill/workfile
  behavior remain open.

Decision update 2026-04-04 (bounded phase-`5` planner-consumption slice
closure):

- The next phase-`5` blocker was not catalog durability anymore; it was
  key-identity drift between compiler/test feedback keys and planner lookup.
  The planner was hashing `grant_key_hash` with a local FNV variant, so the
  durable row existed but was not found at plan time.
- The compiler/planner handoff now also resolves the effective schema from the
  live connection before issuing the planning request, which removes the
  remaining empty-schema path on this proof surface.
- Focused verification is green after rebuild with `3/3` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowRejectsSpilledHashJoin`,
  and
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin`.
- Decision: count this as the first bounded planner-consumption closure for
  phase `5`. Do not overstate it as full memory admission closure: operator
  reservation, broader resize feedback loops, and runtime spill/workfile
  execution still remain open.

Decision update 2026-04-04 (bounded phase-`5` persisted resize-feedback and
operator-reservation slice closure):

- The next live phase-`5` gaps were planner/runtime mismatches on the memory
  surface, not catalog durability: the runtime-plan annotator was
  recomputing baseline resource metadata and discarding feedback-adjusted
  budgets, and the reusable-plan chooser was still taking statement memory
  from the root node only.
- The runtime-plan boundary now preserves recorded cost evidence, so
  feedback-adjusted `memory_budget_bytes` and `spill_expected` survive for
  merge join, sort, window, aggregate, and distinct operators.
- The executor now persists durable `memory_grant_feedback` samples from
  executed `SORT` and `HASH_AGG` plans, which gives the package its first
  execute-to-recompile feedback loop beyond hash join.
- The chooser now computes statement reservation from the peak operator
  budget across the full runtime plan and publishes
  `PLAN_MEMORY_RESERVATION_BYTES`, which closes the wrapped-operator
  under-admission bug where `Limit` masked a child `Sort` budget.
- Focused verification is green after rebuild with `9/9` passing tests in
  `scratchbird_tests`, including the new wrapped-sort reservation proof and
  the execute-to-recompile `SORT` / `HASH_AGG` persistence proofs.
- Decision: count this as the first bounded upper-stage resize-feedback and
  chooser-visible operator reservation closure for phase `5`. Do not
  overstate it as full runtime admission or spill execution; explicit runtime
  admission, cancellation, and workfile behavior remain open.

Decision update 2026-04-04 (bounded phase-`5` runtime stale-plan admission
closure):

- The next live phase-`5` gap was no longer planner-side: the executor still
  trusted embedded runtime plans when a session tightened `SPILL_POLICY` or
  `WORK_MEM` after compilation.
- The V3 executor now enforces live memory admission against the embedded
  runtime plan and fails closed in two bounded cases:
  when the compiled plan still carries `spill_expected=true` under live
  `SPILL_POLICY=DISALLOW`, and when the plan's recorded reservation exceeds
  the live `WORK_MEM` ceiling under that same no-spill policy.
- Focused verification is green after rebuild with `12/12` passing tests in
  `scratchbird_tests`, including the new stale-bytecode runtime admission
  proofs for spill-expected plans and reservation-overrun plans.
- Decision: count this as the first bounded runtime stale-plan admission
  closure for phase `5`. Do not overstate it as spill/workfile execution;
  actual workfile runtime behavior is still the active frontier.

Decision update 2026-04-04 (bounded phase-`5` executed upper-stage
feedback expansion closure):

- The next live phase-`5` gap was proof coverage, not catalog plumbing:
  executed feedback had only been proven end to end for `SORT` and
  grouped/hash aggregate even though the runtime path also claimed
  `WINDOW` and hash-`DISTINCT`.
- Focused verification is green after rebuild with `6/6` passing tests in
  `scratchbird_tests`, including the new executed `WINDOW` and executed
  hash-`DISTINCT` persistence proofs plus the existing spill-disallow
  recompile checks for both operators.
- Decision: count this as the next bounded execute-to-recompile closure for
  phase `5`. Do not overstate it as real spill/workfile execution for those
  operators; this only proves the durable grant-feedback loop.

Decision update 2026-04-04 (bounded phase-`5` executed hash-join feedback
closure):

- The next live phase-`5` gap after the upper-stage expansion was join-family
  execution proof: hash join had planner-consumption coverage, but not an
  executed feedback loop proving runtime samples seed the later
  spill-disallow compile path.
- Focused verification is green after rebuild with `4/4` passing tests in
  `scratchbird_tests`, including the new
  `ExecutedHashJoinPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`
  proof.
- Decision: count this as the bounded executed hash-join feedback closure for
  phase `5`. Do not overstate it as full join-family closure; merge-join
  execution feedback and actual workfile runtime behavior still remain open.

Decision update 2026-04-04 (bounded phase-`5` merge-join runtime spill/workfile
closure):

- The next live phase-`5` gap was no longer planner-only: forced
  `SORT_TO_MERGE` execution still sorted both inputs fully in memory, so the
  package had sort workfiles but no join-family workfile execution.
- The V3 executor now spills merge-join input runs to `sb_workfile`,
  merges those runs back in key order, and records actual merge-join spill
  observation into durable `memory_grant_feedback`.
- Focused verification is green after rebuild with `3/3` passing tests in
  `scratchbird_tests`, including the new
  `ExecuteBytecodeRunsSpilledMergeJoinThroughWorkfileAndPersistsFeedback`
  proof.
- Decision: count this as the first bounded join-family runtime
  spill/workfile closure for phase `5`. Do not overstate it as full spill
  closure; broader upper-stage runtime spill/workfile behavior still remains
  open.

Decision update 2026-04-04 (bounded phase-`5` window runtime spill/workfile
closure):

- The next live phase-`5` gap after merge join was the stale-window path:
  executed `WINDOW` feedback existed, but the V3 executor still relied on the
  later top-level sort plumbing, so a stale `ROW_NUMBER() OVER (ORDER BY ...)`
  bytecode payload could miss real runtime spill even after row counts grew.
- The V3 executor now extracts the embedded window ordering from the V3
  projection payload, orders the window input stream at the `WINDOW` boundary,
  spills oversized runs to `sb_workfile`, merges them back in key order, and
  records actual window spill observation into durable
  `memory_grant_feedback`.
- Focused verification is green after rebuild with `3/3` passing tests in
  `scratchbird_tests`, including the new
  `ExecutedWindowCapturesActualSortSpillOnStaleBytecode` proof.
- Decision: count this as the bounded stale-window runtime spill/workfile
  closure for phase `5`. Do not overstate it as full spill closure; hash join
  and broader operator-runtime spill behavior still remain open.

Decision update 2026-04-04 (bounded phase-`5` hash-join runtime spill/workfile
closure):

- The next live phase-`5` gap after the window slice was stale hash-join
  execution: planner-side spill intent and durable feedback existed, but a
  previously compiled `HASH_JOIN` bytecode payload still had no bounded
  runtime workfile path once row counts grew past the original in-memory
  budget.
- The V3 executor now partitions oversized build/probe inputs through
  `sb_workfile`, rejoins those partitions at execution time, and records
  actual hash-join spill observation into durable `memory_grant_feedback`.
- Focused verification is green after rebuild with `5/5` passing tests in
  `scratchbird_tests`, including the new
  `ExecutedHashJoinCapturesActualSpillOnStaleBytecode` proof.
- Decision: count this as the bounded stale-hash-join runtime spill/workfile
  closure for phase `5`. Do not overstate it as full spill closure; broader
  operator-runtime spill/workfile behavior still remains open.

Decision update 2026-04-04 (bounded phase-`5` hash-distinct runtime
spill/workfile closure):

- The next live phase-`5` gap after stale hash join was upper-stage
  `HASH_DISTINCT`: execute-to-recompile feedback existed, but stale distinct
  bytecode still deduplicated purely in memory and only inherited planner-side
  spill intent into durable `memory_grant_feedback`.
- The executor now partitions oversized distinct rows through `sb_workfile`,
  deduplicates per partition, and records actual `HASH_AGG` spill observation
  for the hash-distinct runtime path.
- Focused verification is green after rebuild with `3/3` passing tests in
  `scratchbird_tests`, including the new
  `ExecutedHashDistinctCapturesActualSpillOnStaleBytecode` proof.
- Decision: count this as the bounded stale-hash-distinct runtime
  spill/workfile closure for phase `5`. Do not overstate it as full spill
  closure; grouped hash aggregate and broader operator-runtime spill/workfile
  behavior still remain open.

Decision update 2026-04-04 (bounded phase-`5` grouped hash-aggregate runtime
spill/workfile closure):

- The next live phase-`5` gap after stale hash-distinct was grouped
  `HASH_AGGREGATE`: planner-side spill intent and durable feedback existed,
  but stale grouped aggregate bytecode still aggregated purely in memory and
  could miss real runtime spill observation.
- The executor now partitions oversized grouped aggregate state through
  `sb_workfile` and records actual `HASH_AGG` spill observation into durable
  `memory_grant_feedback`.
- Focused verification is green after rebuild with `3/3` passing tests in
  `scratchbird_tests`, including the new
  `ExecutedHashAggregateCapturesActualSpillOnStaleBytecode` proof.
- Decision: count this as the bounded stale-hash-aggregate runtime
  spill/workfile closure for phase `5`. Do not overstate it as full spill
  closure; stale top-level sort execution and wider runtime admission behavior
  still remained open at this boundary.

Decision update 2026-04-04 (bounded phase-`5` stale-sort runtime spill/workfile
closure):

- The next live phase-`5` gap after grouped hash aggregate was top-level
  stale `SORT`: the executor had sort workfiles, but stale sort bytecode still
  entered that path only when planner `spill_expected` was already set, so a
  later runtime row explosion could miss both real spill execution and durable
  `SORT` spill observation.
- The executor now switches stale sort spill on runtime row pressure against
  the embedded plan budget, spills oversized runs to `sb_workfile`, merges
  them back in key order, and records actual `SORT` spill observation into
  durable `memory_grant_feedback`.
- Focused verification is green after rebuild with `3/3` passing tests in
  `scratchbird_tests`, including the repaired
  `ExecutedSortCapturesActualSpillOnStaleBytecode` proof.
- Decision: count this as the bounded stale-sort runtime spill/workfile
  closure for phase `5`. Do not overstate it as full admission closure;
  explicit spill-disallow cancellation accounting and broader runtime
  admission behavior still remained open at this boundary.

Decision update 2026-04-04 (bounded phase-`5` spill-disallow
cancellation-accounting closure):

- The next live phase-`5` gap after stale sort was runtime-admission evidence:
  stale bytecode could now be rejected under live `SPILL_POLICY=DISALLOW`,
  but those runtime cancellations were not incrementing durable
  `memory_grant_feedback.cancel_count`, so the admission path still under-
  reported real operator pressure.
- The executor now increments durable `cancel_count` for the rejected
  operator family when the live session disallows spill-expected operators or
  when the embedded runtime-plan reservation exceeds live `WORK_MEM`.
- Focused verification is green after rebuild with `2/2` passing tests in
  `scratchbird_tests`, covering both stale spill-expected rejection and live
  reservation-overrun rejection.
- Decision: count this as the bounded runtime-admission cancellation-
  accounting closure for phase `5`. Do not overstate it as full memory-
  residency closure; broader operator reservation and remaining spill/workfile
  behavior still remain open.

Decision update 2026-04-04 (bounded phase-`5` right-sizing and oscillation
consumption closure):

- The next live phase-`5` gap after runtime-admission cancellation accounting
  was planner-side right-sizing consumption: durable
  `memory_grant_feedback` already carried stable-underuse and oscillation
  markers, but the planner still only consumed rows that increased the
  reservation, so mature underuse could not shrink oversized operator
  budgets and oscillating packets were not explicitly proven to stay inert.
- The planner now accepts durable right-sizing packets when the feedback row
  reaches the bounded stable-underuse threshold, lowers the runtime working-
  set annotation to the observed feedback packet during shrink, and still
  refuses to shrink when the durable row is marked `OSCILLATING`.
- Focused verification is green after rebuild with `13/13` passing tests in
  `scratchbird_tests`, including the new
  `StableUnderuseFeedbackShrinksSortReservationUnderSpillDisallow` and
  `OscillatingSortFeedbackDoesNotShrinkReservation` proofs.
- Decision: count this as the bounded planner-side right-sizing and
  oscillation-consumption closure for phase `5`. Do not overstate it as full
  memory-residency closure; broader operator reservation and remaining
  spill/workfile behavior still remain open.

Decision update 2026-04-04 (bounded phase-`5` statement-reservation admission
closure):

- The next live phase-`5` gap after planner-side right-sizing was the
  compile-to-runtime statement-reservation bridge: the planner could now
  shrink operator budgets and `PLAN_MEMORY_RESERVATION_BYTES`, but that
  effect was not yet explicitly proven against the live spill-disallow
  admission path.
- The bounded sort path now proves the contract end to end: the baseline
  statement reservation is rejected under a live `WORK_MEM` ceiling, while
  the same SQL recompiled after mature stable-underuse feedback is admitted
  and executes successfully under that same live ceiling.
- Focused verification is green after rebuild with `4/4` passing tests in
  `scratchbird_tests`, including the new
  `FeedbackShrunkSortReservationChangesRuntimeSpillDisallowAdmission` proof.
- Decision: count this as the bounded compile-to-runtime statement-
  reservation admission closure for phase `5`. Do not overstate it as full
  memory-residency closure; broader operator reservation and remaining
  spill/workfile behavior still remain open.

Decision update 2026-04-04 (bounded phase-`5` memory-grant identity hardening
closure):

- The next live phase-`5` gap after statement-reservation admission was grant
  row identity reuse: durable `memory_grant_feedback` rows were already
  keyed more tightly in runtime code, but the package evidence did not yet
  prove that mismatched planner policy snapshots, cache modes, or storage-
  layer shapes were actually excluded on the live compile path, and
  execution-intent separation was still implicit instead of directly proven.
- The focused sort path now proves that a generic grant row is ignored by a
  custom parameter-sensitive compile, that rows seeded under the wrong
  planner policy snapshot, wrong execution-intent class, or wrong storage-
  layer shape are also ignored, and that the fully matching row is consumed.
- Focused verification is green after rebuild with `3/3` passing tests in
  `scratchbird_tests`, covering plan-profile separation, execution-intent
  separation, and policy/storage-shape separation.
- Decision: count this as the bounded memory-grant identity hardening
  closure for phase `5`. Do not overstate it as full memory-residency
  closure; broader operator reservation and remaining spill/workfile
  behavior still remain open.

Decision update 2026-04-04 (phase-`5` package closeout):

- The final live phase-`5` gap was identity agreement between planner lookup
  and executor persistence: reusable-plan policy metadata had widened beyond
  the actual grant-policy identity, so executed feedback rows could land
  under `PLAN_POLICY_SNAPSHOT` while planner consumption still keyed on the
  narrower grant-policy snapshot.
- The compiler now publishes `PLAN_GRANT_POLICY_SNAPSHOT` separately from the
  broader reusable-plan `PLAN_POLICY_SNAPSHOT`, publishes the exact
  `PLAN_MEMORY_FEEDBACK_SNAPSHOT`, and the executor now persists runtime
  `memory_grant_feedback` rows under that same grant-policy identity.
- Verification is green after rebuild with the full bounded phase-`5`
  planner/executor slice at `36/36` passing tests in `scratchbird_tests`.
- Decision: count phase `5` as closed for package `08`. The active frontier
  moves to phase `6` planner parity and refusal-model closure.

Decision update 2026-04-04 (first bounded phase-`6` join-runtime slice):

- The first benchmark-visible phase-`6` hotspot remained the mixed equi-plus-
  residual self-join shape: the planner still routes that family through the
  nested-loop path, and the executor was rebuilding a full combined row and
  re-entering the general expression evaluator for every candidate pair just
  to check direct residual terms like `left.id < right.id`.
- The V3 join executor now recognizes conjunctions of direct left/right
  column comparison terms and evaluates them in place inside the join loops.
  This removes the avoidable combined-row materialization and generic
  expression-evaluation roundtrip from the hot residual path while preserving
  the existing fallback for unsupported predicates.
- Focused verification is green after rebuild with `4/4` passing tests in
  `scratchbird_tests`, including the benchmark-shaped proofs
  `NestedLoopSelfJoinResidualComparisonExecutesExpectedPairs` and
  `DefaultSelfJoinResidualComparisonExecutesExpectedPairs`.
- Decision: count this as the first bounded phase-`6` executor closure for
  package `08`. Do not overstate it as full query-path closure; broader join,
  aggregate, sort, projection, and late-materialization work still remain
  open.

Decision update 2026-04-04 (next bounded phase-`6` hash-join executor slice):

- The next benchmark-visible phase-`6` hotspot after the residual self-join
  path remained the integer-key hash join family: the executor was still
  building and probing single-column hash keys by allocating
  `Value::toString()` strings in both the in-memory branch and the spilled
  partition/workfile branch.
- The V3 hash join executor now normalizes supported scalar integer keys into
  compact structured hash keys for that single-column equality path and uses
  the same fast key shape consistently in spill partition routing and
  per-partition hash tables.
- Focused verification is green after rebuild with `4/4` passing tests in
  `scratchbird_tests`, including the ordinary integer hash-join proof
  `HashJoinPlanExecutesAndReturnsExpectedRows` and the spilled stale-bytecode
  proof `ExecutedHashJoinCapturesActualSpillOnStaleBytecode`.
- Decision: count this as the next bounded phase-`6` executor closure for
  package `08`. Do not overstate it as full query-path closure; broader join,
  aggregate, sort, projection, and late-materialization work still remain
  open.

Decision update 2026-04-04 (phase-`6` projection and aggregate-key closeout):

- The remaining bounded phase-`6` query-path gap after the residual self-join
  and integer hash-join slices was the mixed projection/materialization
  surface: V3 `table.*` had lost its qualifier payload in the emitter and
  lowerer path, relation loads still materialized more columns than the live
  query family needed, and grouped/distinct aggregate keys still allocated
  `Value::toString()` strings on the runtime path.
- The bounded V3 query path now preserves the qualified `table.*` payload,
  collects relation-scoped projection requirements before base and joined
  table loads, prunes those loads through the join path, and uses typed
  binary keys for grouped/distinct aggregate rows instead of string keys.
- Focused verification is green after rebuild with `8/8` passing tests in
  `scratchbird_tests`, including the new proofs
  `QualifiedTableStarPreservesOwningRelationProjection` and
  `CountStarExecutesWithProjectionPrunedRelationLoad`.
- Targeted fresh-runtime stress proof is preserved under
  `ScratchBird-Benchmarks/results/phase6-targeted-20260404T/` with
  `inner_join_large_result = 13934.35ms`,
  `aggregation_daily_sales = 2409.84ms`,
  and
  `multi_dimensional_agg = 4806.16ms`.
- Decision: count this as the bounded phase-`6` closeout for package `08`.
  The active frontier moves to phase `7` full rerun and release evidence
  closure.

## Final Closeout Note

Not yet available.
