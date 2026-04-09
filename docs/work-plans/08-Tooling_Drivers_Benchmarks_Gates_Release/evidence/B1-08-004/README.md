# B1-08-004 Evidence Notes

## Transaction-Mode Matrix Refresh

- On `2026-04-03`, the earlier cross-engine stress artifacts were explicitly
  discarded because they mixed incompatible transaction shapes across engines.
  The preserved comparable root is now:
  `ScratchBird-Benchmarks/results/txmode-matrix-20260403T152011Z`.
- The active bounded stress matrix is now:
  `ScratchBird` and `Firebird` in `normal_transactional` and `autocommit`,
  `MySQL` and `PostgreSQL` in `no_transaction`,
  `normal_transactional`, and `autocommit`.
- During the first PostgreSQL rerun for that matrix, the `no_transaction` leg
  proved that raw `psycopg2.executemany()` with connection autocommit enabled
  degenerates into row-by-row inserts. That behavior measures a Python-driver
  batching artifact more than the intended transaction-shape difference.
- The benchmark harness was then corrected so PostgreSQL `executemany()`
  inserts are emitted as real multi-row `INSERT` statements via
  `psycopg2.extras.execute_values()`. Only after that correction was the clean
  `txmode-matrix-20260403T152011Z` PostgreSQL evidence preserved.
- Fresh bounded matrix highlights from that preserved root:
  ScratchBird `normal_transactional` completed the full `15/15` lane with
  `36.96 s` load time and `44.52 s` measured test time;
  ScratchBird `autocommit` completed `15/15` with `46.51 s` load and
  `52.97 s` measured test time.
  Firebird `normal_transactional` completed `15/15` with `110.90 s` load and
  `22.72 s` measured test time;
  Firebird `autocommit` completed `15/15` with `113.30 s` load and
  `23.40 s` measured test time.
  MySQL completed all three transaction shapes in about `5.70-5.75 s` load and
  `6.57-8.92 s` measured test time.
  PostgreSQL completed all three transaction shapes in about `4.50-4.78 s`
  load and `2.81-3.09 s` measured test time.

## Runtime Collision Guard

- `scripts/example_db_manager.sh` now checks that the example runtime listener
  ports are bindable before startup.
- This closes the observed false-positive startup path where a stale
  `ScratchBird-Benchmarks` scratchbird runtime held ports that overlapped the
  example setup, causing the example setup to see live ports from the wrong
  process and then stall waiting for control sockets that would never be
  created under `/tmp/scratchbird-example-dynamic/control`.
- Direct repro after the fix fails fast with:
  `example runtime ports already in use on 127.0.0.1: 16432 16306 16050; stop the conflicting runtime or override SCRATCHBIRD_EXAMPLE_DYNAMIC_*_PORT`
- After stopping the benchmark scratchbird runtime, direct repro of
  `tests/compatibility/scripts/manage_example_db.sh dynamic-setup` completed
  successfully and the targeted CTest rerun
  `CompatibilityExampleDbSetup` passed in `90.44 sec`.

## Session Close Fail-Closed Hardening

- The clean benchmark replay exposed a real server bug at
  `2026-04-01 15:10:37 EDT`: `sb_server` PID `690183` crashed in
  `CatalogManager::closeSession(...)` while deleting a session row from the
  sessions catalog page.
- Core analysis showed the disconnect path trusted corrupt catalog heap page
  header and record layout state inside `deleteRecordFromHeapPage`, allowing the
  raw scan to walk off-page and crash the server.
- The fix hardens the catalog heap page mutators and preserves the underlying
  corruption detail through `closeSession()` instead of overwriting it with a
  generic wrapper message.
- Focused verification now passes in `scratchbird_tests` with
  `CatalogRuntimeContextExtensionContractTest.CloseSessionRejectsCorruptSessionCatalogPage`
  and
  `CatalogRuntimeContextExtensionContractTest.LiveTransactionCommitSurvivesClosedSessionBinding`.
- Post-fix benchmark replays stayed alive past the old crash window and did not
  create a new `sb_server` coredump during the monitored reruns.

## Benchmark Limit Notes

- Manual `ScratchBird-Benchmarks` `stress medium` runs are slower than the Beta
  1 release gate envelope and were treated as diagnostic-only during this lane.
- The canonical Beta 1 gate remains
  `scripts/run_full_build_test_with_metrics.sh --run-public-beta --run-benchmarks --benchmark-engines firebird,mysql,postgresql,scratchbird`
  with the bounded in-repo section `31` stress shape, not the manual external
  `medium` stress load.
- The upstream Firebird `index-comparison` lane still fails in the external
  harness with `Table unknown IDXCMP_POINT_LOOKUP`; this is preserved as
  external failure evidence, not a new ScratchBird engine regression.
- The bounded ScratchBird `stress --scale small` lane remains the open Beta 1
  benchmark blocker. The dataset is only `10k customers`, `5k products`,
  `50k orders`, and `200k order_items`, but the clean replay
  `tests/results/full_gate/20260401T161737Z/benchmark.log` still pushed beyond
  one hour before I stopped it after proving the crash was fixed.
- Benchmark-only harness tuning now raises the ScratchBird leak-detector
  threshold to `7200s` and increases the ScratchBird load batch cap from `512`
  to `4096` rows in
  `ScratchBird-Benchmarks/stress-tests/runners/dialect_stress_runner.py`. This
  removed the false-positive 30-minute leak spam, but it did not yet close the
  small-scale stress throughput gap.
- Direct `sb_isql` insert probes now show the bottleneck is not confined to the
  Python benchmark harness. A plain-SQL export of the full small stress load
  disconnected after `34.98s` to `36.58s` with repeated `Rows affected: 0`
  output followed by `Failed to receive response`.
- A narrower customers-only import probe was then generated to separate total
  workload size from insert-path cost. The `1000`-row script with `256`-row
  literal `INSERT` statements on a fresh benchmark runtime was still executing
  after `71s` with `sb_server` pinned near `100%` CPU and no `sb_isql` output
  before the probe was manually stopped.
- Insert-path tracing was then enabled with `SCRATCHBIRD_INSERT_TRACE=1` and a
  dedicated append log at `/tmp/scratchbird_insert_trace.log`. On a fresh
  heap-only probe table, a `256`-row `executemany()` batch completed in
  `2673.453 ms`; the summed engine traces for those `256` rows were
  `2523.284 ms`, with `2490.146 ms` spent in `findFreePage()`, `1.015 ms` in
  `HeapPage::insertTuple()`, and about `355` scanned pages per row. This
  isolates the primary bounded engine cost to repeated linear free-space
  discovery.
- A second probe against a `customers`-like schema matching the stress lane
  (`PRIMARY KEY` plus `UNIQUE(email)`) inserted `128` rows in `10006.994 ms`.
  The summed engine traces for the main table were `4228.408 ms`, of which
  `2859.275 ms` was uniqueness preflight and `1320.568 ms` was free-page
  search; heap placement itself was only `0.860 ms` total and post-insert
  index maintenance `14.020 ms`. This shows that stress-shaped rows pay both
  the linear free-page scan cost and substantial per-row uniqueness preflight.
- The same `128`-row customers-like batch still had several seconds of runtime
  above the traced storage work, so the remaining open portion is now bounded
  to large multi-row statement overhead above `StorageEngine` rather than to
  Python-only harness behavior.
- This blocker is now being driven by the package-level
  `PERFORMANCE_REMEDIATION_PLAN.md`, which covers the insert-path fix first and
  then the wider Beta 1 action-family performance program required for package
  closeout.
- Current conclusion: the open benchmark blocker is a real ScratchBird
  server-side insert-throughput problem. It reproduces both through the Python
  driver benchmark path and through standalone `sb_isql` SQL-script execution.

## Insert-Path Remediation Progress

- Phase `1` and the bounded Phase `2` fixes are now landed in the engine.
  `StorageEngine` now keeps a hot writable-page hint per relation and caches
  unique-preflight metadata instead of rediscovering it row by row.
- The executor-side duplicate work was also removed in both insert execution
  paths. Ordinary storage-backed `BTREE` and `HASH` unique indexes are now
  treated as `StorageEngine` authority instead of being revalidated again in
  the SQL-layer insert loops, and the active V3/native insert path now reuses
  cached index, constraint, foreign-key, and trigger catalog surfaces.
- Post-fix heap-only probe:
  `256` rows now execute in `183.289 ms` to `192.016 ms` with commit in
  `75.400 ms` to `84.425 ms`, down from the earlier `2673.453 ms` execute
  baseline.
- Post-fix `customers`-like probe (`PRIMARY KEY` plus `UNIQUE(email)`):
  `128` rows now execute in `197.577 ms` with commit in `75.878 ms`, down from
  the earlier `10006.994 ms` execute baseline and the intermediate
  `5929.432 ms` / `5515.540 ms` measurements before the active V3 path was
  patched.
- The matching storage trace for that post-fix `customers`-like probe summed to
  only `61.712 ms` across `128` rows:
  `20.492 ms` uniqueness preflight,
  `18.506 ms` free-page search,
  and `2.400 ms` post-insert index maintenance.
  This confirms the original Beta blocker was the broken insert path, not the
  benchmark wrapper itself.
- A fresh `ScratchBird-Benchmarks` `stress --scale small` rerun is now active
  against the remediated runtime. That rerun completed on `2026-04-01` and
  materially changed the failure shape:
  `customers` loaded in `17.05 s`,
  `products` loaded in `10.04 s`,
  and the run advanced into `orders` where the prior Beta blocker had been
  raw insert throughput.
- The new bounded blocker is correctness, not gross insert speed. During
  `orders` load, the stress replay failed after
  `Batch 10: 40,960 rows loaded...` with:
  `Failed to insert row: Page number 174096499359744 exceeds uint32_t maximum for primary tablespace`.
- After that first fault, the same stress replay cascaded into rollback and
  response-path failures including:
  `[25000] Failed to rollback transaction`,
  `[25000] Read timeout: incomplete message`,
  and `[42000] Failed to receive response`.
- The preserved replay artifacts for this post-fix run are:
  `stress_scratchbird_small_postfix_20260401_223127.json` in this evidence
  directory and the live runner log at `/tmp/sb_stress_small_postfix.log`.
- Package `08` therefore remains open, but the open issue is now narrowed to
  the new `GPID/page-number` correctness failure during larger stress loads and
  its follow-on protocol recovery behavior, not the original insert-throughput
  defect.
- Follow-up remediation on `2026-04-02` changed that failure shape again.
  The `GPID/page-number` load fault was eliminated on the stable branch after
  the writable-page hint work landed, and the remaining bounded blocker moved
  into V3 SELECT self-join execution.
- Stable retained fixes now include:
  V3 inferred hash-join key discovery for conjunctive join predicates,
  string-key admission for inferred hash joins,
  and safe base-side `WHERE` pushdown when the predicate resolves entirely
  against the currently loaded relation set.
- The focused customer-shaped repro improved materially on that stable branch:
  a `10,000`-row self-join with
  `c1.country_code = c2.country_code AND c1.customer_id < c2.customer_id`
  plus `WHERE c1.registration_date >= '2024-01-01' LIMIT 1`
  dropped from “did not complete in tens of seconds” to about `15.92 ms`
  execute time on the patched runtime.
- The real bounded stress replay also improved materially on the same stable
  branch. Preserved artifacts:
  `self_join_fix_rerun_20260402T222500Z/stress_scratchbird_20260402_181713.json`
  and
  `self_join_limit_pushdown_rerun_20260402T224000Z/stress_scratchbird_20260402_183526.json`.
  Those runs no longer disconnected during the self-join and completed with
  `10,000` rows returned in `276232.52 ms` and then `260192.82 ms`.
- The active Beta 1 limit is now narrower:
  the stable branch completes the bounded self-join lane but still exceeds the
  scenario’s nominal `180 s` budget by about `80 s`.
- An additional experimental residual-fast-path change was tested and rejected
  on `2026-04-02` because it regressed the bounded stress lane back to a
  protocol failure. Preserved artifact:
  `self_join_residual_fastpath_rerun_20260402T230900Z/stress_scratchbird_20260402_190224.json`.
  That branch is not retained in the current workspace.
- Follow-up remediation on `2026-04-03` materially changed the bounded
  self-join picture again. After rebuilding the active `sb_server` with the
  retained V3 executor fixes and reordering
  `StorageEngine::findBackVersionPlacementPage()` to try the relation write
  hint before the primary-page locality sweep, the same bounded stress lane
  completed `self_join_same_country` in `2446.75 ms` instead of
  `260192.82 ms`.
- Preserved replay artifact:
  `self_join_post_dml_fix_20260403T045600Z/stress_scratchbird_20260403_005904.json`.
  That replay also closed the earlier catastrophic small-scale insert blocker
  end to end: `customers` loaded in `7.20 s`, `products` in `3.93 s`,
  `orders` in `46.53 s`, `order_items` in `327.40 s`, workload indexes built
  in `10.61 s`, and the stress query itself passed with `10,000` rows returned
  in `2446.75 ms`.

## Exact-Family Maintained-Write Closure Slice

- On `2026-04-03`, the active phase `1` exact-family work gained a bounded
  maintained-index observability closure on the write path.
- Same-key stable-TID updates had already been avoiding delete-plus-insert
  churn inside `StorageEngine`, but they were still bypassing the maintained
  online-index metadata path entirely whenever the caller classified the update
  as `indexed_keys_unchanged`.
- The active runtime now records those unchanged-key exact rewrites after the
  successful heap mutation by:
  incrementing `sb_catalog.index_contention.hot_key_count` for each affected
  exact-family maintained index and
  publishing an `IndexDeltaOp::UPDATE` maintenance delta for active online
  maintenance rows.
- Unique conflict pressure is also now durable on the exact preflight path.
  Both insert and update uniqueness conflicts increment
  `sb_catalog.index_contention.unique_key_conflict_count` for the conflicting
  exact index.
- Focused proof now passes in `scratchbird_tests` with:
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesInsertDelta`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesSameKeyUpdateDelta`,
  and
  `IndexExecutorDispatchContractsTest.UniqueConflictIncrementsIndexContentionCounter`.
- This closes the first maintained-write observability slice under section
  `18`; the remaining phase `1` exact-family backlog is the larger structural
  work: commit-group batch apply, reclaim-driven cleanup debt, cold-page delta
  buffering, and hot-leaf mitigation beyond counter publication.
- On `2026-04-03`, the first bounded hot-leaf structural rung also landed for
  exact B-tree inserts.
- The active runtime now preserves a reserved free-space band on the hot
  rightmost leaf by presplitting monotonic right-edge inserts before the leaf
  is packed into the reserved range. This is deliberately narrow: it uses the
  existing rightmost-leaf hint and normal split path rather than introducing a
  broader rebalance mechanism.
- Focused proof now also passes in `scratchbird_tests` with
  `BtreeTextRightmostRegressionTest.SequentialWideKeysPresplitHotRightmostLeafBeforeFull`.
- The current bounded proof set for this section is now:
  `BtreeTextRightmostRegressionTest.SequentialWideKeysPresplitHotRightmostLeafBeforeFull`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesInsertDelta`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesSameKeyUpdateDelta`,
  and
  `IndexExecutorDispatchContractsTest.UniqueConflictIncrementsIndexContentionCounter`.
- This closes the first required hot-right-edge mitigation rung from section
  `18`; the remaining phase `1` structural backlog is still
  commit-group batch apply,
  reclaim-driven cleanup debt,
  narrow cold-page delta buffering,
  and broader hot-leaf mitigation beyond the initial right-edge reserve rule.
- On `2026-04-03`, the exact-cleanup path gained a second bounded section `18`
  closure slice for exact B-tree indexes.
- The active runtime now compacts a leaf immediately during
  `BTree::removeDeadEntries()` after reclaim-proof dead entries have been marked
  deleted whenever the page-local reclaimable bytes cross the canonical compact
  threshold. The implementation verifies the compacted page immediately and
  stops destructive cleanup on that locality if verification fails.
- Focused proof now also passes in `scratchbird_tests` with
  `BTreeGCTest.RemoveDeadEntriesCompactsLeafWhenReclaimThresholdExceeded`.
- The current bounded proof set for this section is now:
  `BTreeGCTest.RemoveDeadEntriesCompactsLeafWhenReclaimThresholdExceeded`,
  `BtreeTextRightmostRegressionTest.SequentialWideKeysPresplitHotRightmostLeafBeforeFull`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesInsertDelta`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesSameKeyUpdateDelta`,
  and
  `IndexExecutorDispatchContractsTest.UniqueConflictIncrementsIndexContentionCounter`.
- This closes the local reclaim-driven exact compaction rung from section `18`,
  but the broader phase `1` backlog is still open:
  commit-group batch apply,
  reclaim-driven cleanup debt publication beyond the local B-tree compaction
  slice,
  narrow cold-page delta buffering,
  and broader hot-leaf mitigation beyond the initial right-edge reserve rule.
- On `2026-04-03`, the exact-cleanup path gained a second bounded debt-bridge
  closure on top of that local compaction work.
- The active B-tree runtime now records residual exact-cleanup debt after
  `removeDeadEntries()` by exposing backlog pages, backlog bytes, first
  affected locality page, and a repair-required bit through the
  `IndexGCInterface` cleanup snapshot path.
- The garbage-collector publication bridge now carries those residual exact
  backlog fields into `IndexCleanupPublicationRecord` and the publication
  summary, while preserving the earlier completion contract when the heap proof
  fixture does not actually map to a matching exact dead entry.
- Focused proof now passes in `scratchbird_tests` with:
  `BTreeGCTest.RemoveDeadEntriesPublishesResidualCleanupDebtBelowCompactionThreshold`,
  `BTreeGCTest.RemoveDeadEntriesCompactsLeafWhenReclaimThresholdExceeded`,
  `MgaFragmentationPolicyTest.GcPagePublishesExactCleanupCompletionWithProofContext`,
  `BtreeTextRightmostRegressionTest.SequentialWideKeysPresplitHotRightmostLeafBeforeFull`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesInsertDelta`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesSameKeyUpdateDelta`,
  and
  `IndexExecutorDispatchContractsTest.UniqueConflictIncrementsIndexContentionCounter`.
- This closes the bounded B-tree exact cleanup debt publication bridge from
  section `18`, but it does not yet create the broader durable debt ledger or
  cross-family maintenance scheduler canon from later phases.
- On `2026-04-03`, the next bounded durability rung landed: exact cleanup debt
  now persists into the per-index `index_health` ledger so backlog count,
  backlog pages, backlog bytes, repair-required state, and sweep/checkpoint
  generations survive beyond the in-memory publication map.
- `StorageEngine::publishIndexCleanupPublication()` now refreshes that durable
  ledger automatically after every publication update, and
  `sb_mga_cleanup_debt` now falls back to the durable `index_health` aggregate
  when a runtime publication is no longer present.
- Focused proof for the durable ledger slice is now green in
  `scratchbird_tests` with
  `CatalogIndexMetricsExtensionContractTest.StorageAndHealthContracts`,
  `MgaObservabilityLiveViewsTest.BuildsLiveMgaRowsFromRuntimeCatalogAndFragmentationState`,
  `MgaObservabilityLiveViewsTest.CleanupPublicationUpdatesDurableIndexHealthLedger`,
  `MgaObservabilityLiveViewsTest.BuildsMgaCleanupDebtRowsFromDurableIndexHealthLedger`,
  and
  `MgaObservabilityLiveViewsTest.BuildsDurabilityRowsFromCatalogHistoryAndRuntimeState`.
- This reduces the remaining phase `1` exact-family structural backlog to
  narrow cold-page delta buffering,
  hot-leaf mitigation beyond the first right-edge reserve rung,
  and broader scheduler-grade debt routing beyond the bounded per-index ledger.
- The next bounded section `18` slice is now also green: active
  online-maintenance exact deltas queue by xid while `GROUP_COMMIT` is active,
  materialize in commit order under the fenced commit batch before final TIP
  publication, discard cleanly on rollback, and delete any prepared delta rows
  if the fenced batch fails before completion.
- Focused proof for that commit-group maintenance-batching slice is green in
  `scratchbird_tests` with the full `GroupCommitTest.*` suite,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesInsertDelta`,
  `IndexExecutorDispatchContractsTest.OnlineMaintenanceCapturesSameKeyUpdateDelta`,
  `StorageEngineTest.GroupCommitQueuesOnlineMaintenanceDeltaUntilCommit`,
  and
  `StorageEngineTest.GroupCommitRollbackDiscardsQueuedOnlineMaintenanceDelta`
  for a `16/16` pass set.
- The next bounded section `18` substrate slice is also green: the engine now
  materializes a durable `index_page_delta` catalog family with root-page
  allocation, restart-stable bootstrap, extract visibility, family-matrix
  coverage, and CRUD/list/delete validation for the canonical
  `INSERT`/`DELETE`/`UPDATE_SAME_KEY`/`UPDATE_KEY_CHANGE` delta shapes.
- Focused proof for that `index_page_delta` substrate slice is green in
  `scratchbird_tests` with
  `CatalogDatabaseBootstrapTest.CreatesIndexMetadataExtensionCatalogFamilyPages`,
  `CatalogFamilyMatrixContractTest.*`,
  `CatalogFullExtractTest.*`,
  and
  `CatalogIndexMetadataExtensionContractTest.OptionAndMaintenanceDeltaContracts`
  for a `4/4` pass set.
- The next bounded runtime slice is now green too: the engine can defer
  eligible cold exact-secondary inserts into durable `index_page_delta`
  records, merge those deltas synchronously before exact reads and write-side
  exact maintenance, and it now carries a reentrancy guard so TOAST-backed
  delta metadata publication does not recursively defer itself during catalog
  or TOAST writes.
- Focused proof for that bounded runtime cold-page slice is green in
  `scratchbird_tests` with
  `CatalogDatabaseBootstrapTest.CreatesIndexMetadataExtensionCatalogFamilyPages`,
  `CatalogIndexMetadataExtensionContractTest.OptionAndMaintenanceDeltaContracts`,
  `StorageEngineTest.GroupCommitQueuesOnlineMaintenanceDeltaUntilCommit`,
  `StorageEngineTest.GroupCommitRollbackDiscardsQueuedOnlineMaintenanceDelta`,
  `StorageEngineTest.ColdExactSecondaryInsertDeltaMergesOnRead`,
  and
  `StorageEngineTest.ColdExactSecondaryDeferralSkipsUniqueIndexes`
  for a `6/6` pass set.
- The next bounded scheduler-owned merge slice is now green too: background GC
  can drain deferred exact-secondary `index_page_delta` rows without waiting
  for a foreground exact read, per-index in-flight ownership now prevents
  duplicate merge work while still allowing same-thread TOAST cleanup to reenter
  exact seeks without self-deadlocking, and the background path now resolves its
  runtime XID through `StorageEngine::getCurrentXid()` so committed TOAST-backed
  delta payloads stay visible even when no connection context exists.
- Focused proof for that scheduler-owned merge slice is green in
  `scratchbird_tests` with
  `StorageEngineTest.GroupCommitQueuesOnlineMaintenanceDeltaUntilCommit`,
  `StorageEngineTest.GroupCommitRollbackDiscardsQueuedOnlineMaintenanceDelta`,
  `StorageEngineTest.ColdExactSecondaryInsertDeltaMergesOnRead`,
  `StorageEngineTest.BackgroundGcMergesDeferredExactSecondaryDeltasWithoutForegroundRead`,
  and
  `StorageEngineTest.ColdExactSecondaryDeferralSkipsUniqueIndexes`
  for a `5/5` pass set.
- This reduces the remaining phase `1` exact-family structural backlog to
  hot-leaf mitigation beyond the first right-edge reserve rung,
  broader scheduler-grade debt routing beyond the bounded per-index ledger,
  and widening the current bounded per-pass scheduler merge bridge into the
  broader debt-routing lane required by canon.
- Focused direct update probes explain why the DML lane moved so sharply.
  Against a `1000`-row wide `PRIMARY KEY` table, the first post-rebuild update
  still spent `1837.616 ms` of `1866.135 ms` statement time in storage, with
  `1820.574 ms` isolated to back-version placement lookup. After the
  write-hint-first reorder, the same update path dropped to
  `73.891 ms` statement time with only `52.337 ms` summed storage time and
  `42.470 ms` in back-version placement lookup.
- The self-join lane is no longer a timeout blocker, but the comparison-engine
  gap remains real on the first bounded run. Current same-harness reference
  results on the `small` dataset remain:
  PostgreSQL `13.64 ms`,
  MySQL `112.59 ms`,
  Firebird `137.73 ms`,
  ScratchBird `2446.75 ms`.
- Focused post-load reruns show the remaining self-join penalty is largely a
  first-hit cost rather than a steady-state join cost. On the already loaded
  benchmark database, the first executions of the exact self-join query
  remained around `2220-2295 ms`, but once the relevant pages and join path
  were hot the same query stabilized near `108-119 ms` for `LIMIT 1` and
  `241-245 ms` for `LIMIT 10000`.
- `EXPLAIN ANALYZE` and `EXPLAIN (ANALYZE, FORMAT JSON)` still expose a Beta 1
  observability gap on this path. Both currently return only
  `Plan unavailable for opcode SBLR3_EXPLAIN_PLAN` plus actual row count for
  the self-join query, even though the executor-side runtime-plan formatting
  surfaces exist in code. This remains an instrumentation limit, not a query
  correctness failure.
- A clean `2026-04-03` runtime-budget experiment then isolated a much larger
  bounded cause: the benchmark runtime had been running on the engine default
  buffer pool of only `64` pages. At the current `16 KiB` page size, that is
  about `1 MiB` of durable-page residency.
- The same bounded self-join stress lane was rerun from scratch through the
  supported example-manager bootstrap path with
  `SCRATCHBIRD_MEMORY_BUFFER_POOL_SIZE=256MB` preserved in the launch
  environment. Preserved artifact:
  `self_join_bufferpool_256mb_20260403T054500Z/stress_scratchbird_20260403_011455.json`.
- That single runtime-budget correction changed both the load lane and the
  read lane materially:
  `customers` improved from `7.20 s` to `2.68 s`,
  `products` from `3.93 s` to `1.06 s`,
  `orders` from `46.53 s` to `9.12 s`,
  `order_items` from `327.40 s` to `23.71 s`,
  total load from `385.06 s` to `36.57 s`,
  and `self_join_same_country` from `2446.75 ms` to `309.07 ms`.
- This proves the larger package-08 benchmark gap was not only a query-plan
  issue. The tiny default durability-residency budget was materially starving
  both sustained inserts and first-hit join execution on the bounded stress
  workload.
- The comparison gap is now much narrower but still present on the first
  bounded run:
  PostgreSQL `13.64 ms`,
  MySQL `112.59 ms`,
  Firebird `137.73 ms`,
  ScratchBird `309.07 ms`.
- The current active comparison lane is the matching
  `bulk_update_with_case` rerun under the same `256MB` residency budget:
  `bulk_update_bufferpool_256mb_20260403T055300Z`.

## Implicit Budget Correction Proof

- The corrected implicit runtime budget is now preserved on the active branch.
  `Database::loadBufferPoolConfig()` no longer inherits the legacy
  `64`-page struct default on the implicit path; it now derives the
  environment-scaled default budget directly from the detected memory ceiling.
- Preserved focused no-override proof:
  `self_join_defaultbuffer_postpatch_20260403T014200Z/stress_scratchbird_20260403_014046.json`.
  That clean runtime loaded the full bounded dataset in `36.82 s` and then ran
  `self_join_same_country` in `294.71 ms` without any
  `SCRATCHBIRD_MEMORY_BUFFER_POOL_SIZE` override.
- Preserved full no-override proof:
  `full_stress_defaultbuffer_postpatch_20260403T014600Z/stress_scratchbird_20260403_014236.json`.
  That clean runtime loaded the full bounded dataset in `46.90 s`, built the
  workload indexes in `0.30 s`, passed all verification queries, and then
  passed all `15/15` stress scenarios with total measured test time
  `46.41 s`.
- Key same-harness scenario results on that full no-override rerun:
  `self_join_same_country = 350.28 ms`,
  `bulk_update_with_case = 1324.10 ms`,
  `bulk_insert_select = 15974.63 ms`,
  `inner_join_large_result = 14255.57 ms`.
- This closes the earlier bounded blocker that the default runtime was still
  behaving like a `64`-page / `1 MiB` residency budget. The active default
  runtime now behaves in the same practical performance band as the earlier
  explicit `256MB` workaround.
- Remaining non-blocking observability limit:
  the expected live SQL views `sb_buffer_pool_stats`,
  `sb_mga_runtime_metrics`, and `sb_engine_health` are still not exposed on
  the benchmark runtime query surface even though the catalog handlers exist in
  code. Runtime-budget diagnosis therefore still relied on preserved benchmark
  evidence plus process-map inspection instead of canonical SQL introspection.
- On `2026-04-03`, the next bounded phase-`1` hot-leaf observability slice
  landed in the live engine. `BTree` now publishes right-edge detection,
  presplit, and split-retry counters, and the MGA runtime view builder exposes
  them through canonical storage metrics:
  `sb_storage_hot_leaf_detections_total`,
  `sb_storage_hot_leaf_presplits_total`,
  and
  `sb_storage_hot_leaf_split_retries_total`.
- The first implementation attempt widened the label surface to per-index
  metrics, but the existing metric contract rejected that shape because the
  canonical policy currently allows the `storage` subsystem and `relation`
  label, not a new `index` label. The active implementation therefore records
  the hot-leaf counters at canonical `db` + `relation` granularity and
  enumerates live B-tree handles directly from the catalog instead of depending
  on `index_health` rows to exist first.
- Focused proof for this slice is green:
  `BtreeTextRightmostRegressionTest.SequentialWideKeysPresplitHotRightmostLeafBeforeFull`,
  `MgaObservabilityLiveViewsTest.BuildsRuntimeRowsFromHotRightmostBtreeCounters`,
  and
  `MetricContractPolicyTest.MgaContractIsVersionedAndRegistryAuditPasses`.
- On `2026-04-03`, the next bounded hot-leaf structural slice landed on top of
  that observability baseline: after a successful monotonic hot-right-edge
  presplit, the trigger insert now takes a direct post-split insert path into
  the new right leaf instead of always yielding and retrying from the root.
- The follow-on cleanup for that same slice is also landed: the split helper
  now consumes the pending row while both split pages are still pinned, so the
  trigger insert no longer needs the extra repin/relock roundtrip on the new
  right leaf after the split succeeds.
- Focused proof for that follow-on slice is also green with the same `3/3`
  targeted set, and the hot-right-edge regression now proves that
  `right_edge_split_retries` stays flat on the trigger insert while
  `right_edge_detections` and `right_edge_presplits` still advance.
- On `2026-04-03`, the first bounded phase-`2` retail-micro-batch substrate
  also landed on the active load path: `StorageEngine` now exposes a reusable
  bulk insert handle with pinned writable-page reuse and shared post-insert
  maintenance, and the active `COPY FROM` executor path now routes row inserts
  through that handle instead of reopening the full storage insert path for
  every row.
- Focused proof for that slice is green:
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`
  and
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`.
- This does not close phase `2`. It closes a bounded substrate rung for
  `RETAIL_MICRO_BATCH` and lowers the next implementation step to the
  canonical lane model itself: durable `bulk_load_*` plan/progress/guard state
  and explicit lane selection between retail, sorted-bulk, and shadow-cutover.
- On `2026-04-03`, the next bounded phase-`2` lane-planning slice landed on
  top of that substrate: `Executor` now resolves the active `COPY FROM`
  default batch size from canonical config key
  `sb.bulk.micro_batch_target_rows` instead of a hard-coded `10000`, clamps it
  to the spec range `64..65536`, preserves explicit `BATCH_SIZE` overrides,
  and records the chosen lane explicitly as `RETAIL_MICRO_BATCH` on both live
  `COPY` executor paths.
- Focused proof for this lane-planning slice is green with `4/4` passing
  tests:
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`,
  `CopyExecutorTest.ResolveCopyBulkPlanUsesConfiguredRetailMicroBatchDefault`,
  `CopyExecutorTest.ResolveCopyBulkPlanHonorsExplicitBatchSizeOverride`,
  and
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`.
- This still does not close section `39`. It closes the active retail-lane
  planning rung and leaves the next required step unchanged:
  durable `bulk_load_*` plan/event/progress/guard rows plus real lane
  selection between `RETAIL_MICRO_BATCH`, `SORTED_EXACT_BULK`, and
  `SHADOW_LOAD_CUTOVER`.
- On `2026-04-03`, the next bounded phase-`2` retail durable-state slice
  landed on the `COPY FROM` retail path: `CatalogManager` now persists
  canonical `bulk_load_plan`, `bulk_load_event`, `bulk_load_progress`, and
  `bulk_load_cutover_guard` families, and the retail lane publishes
  plan creation, `PLANNED -> RUNNING` and `RUNNING -> COMPLETED` events,
  row-count progress, and a `NOT_REQUIRED` cutover guard for this bounded
  retail path.
- Focused proof for this durable-state slice is green with `5/5` passing
  tests:
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`,
  `CopyExecutorTest.ResolveCopyBulkPlanUsesConfiguredRetailMicroBatchDefault`,
  `CopyExecutorTest.ResolveCopyBulkPlanHonorsExplicitBatchSizeOverride`,
  `CopyExecutorTest.CopyFromPublishesRetailBulkLoadCatalogState`,
  and
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`.
- A validation-only execution limit also surfaced during this slice:
  the first targeted rebuild failed at GoogleTest discovery with
  `text file is busy` because a stale `scratchbird_tests` holder from an older
  executor-focused shell session still had the binary open. Clearing that stale
  holder allowed the targeted rebuild and proof run to complete cleanly. This
  is recorded as package evidence hygiene, not as a product failure.
- On `2026-04-03`, the next bounded parity slice extended that same durable
  retail publication model to the remaining live non-V3 `COPY FROM` branch in
  the shared executor. This slice was compile-validated through a clean
  `scratchbird_tests` rebuild and regression-validated by rerunning the same
  focused `5/5` proof set on the shared executor surface.
- This still does not close section `39`. It closes durable retail-lane
  publication parity across both live `COPY FROM` paths and leaves the next
  required step as real lane selection and matching durable state for
  `SORTED_EXACT_BULK` and `SHADOW_LOAD_CUTOVER`.
- On `2026-04-03`, the next bounded section-`39` slice landed:
  both live `COPY FROM` handlers now classify file-backed loads against
  `sb.bulk.sorted_exact_min_rows` and admitted exact-key candidates, and the
  active V3 `COPY FROM` path now stages `TEXT` and `CSV` rows in memory,
  sorts them by canonical exact-key bytes, and then flushes them through the
  existing bulk insert handle.
- Focused proof for this sorted-lane slice is green with `7/7` passing tests:
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`,
  `CopyExecutorTest.ResolveCopyBulkPlanUsesConfiguredRetailMicroBatchDefault`,
  `CopyExecutorTest.ResolveCopyBulkPlanHonorsExplicitBatchSizeOverride`,
  `CopyExecutorTest.ClassifyCopyBulkLaneChoosesSortedExactBulkAtThreshold`,
  `CopyExecutorTest.CopyFromPublishesRetailBulkLoadCatalogState`,
  `CopyExecutorTest.CopyFromPublishesSortedExactBulkLoadCatalogState`,
  and
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`.
- A validation nuance also surfaced and was kept explicit in proof:
  `sb.bulk.sorted_exact_min_rows` is intentionally clamped to the canonical
  `1000..10000000` range, so the bounded proof had to exercise a `1000`-row
  file-backed load rather than weakening the runtime threshold.
- This still does not close section `39`. It closes bounded
  `SORTED_EXACT_BULK` selection and the active V3 sorted-staging execution
  rung. The remaining Beta 1 gap is still real `SHADOW_LOAD_CUTOVER`
  execution, plus branch-isolated non-V3 proof for sorted/shadow lane
  behavior.
- On `2026-04-03`, the next bounded section-`39` slice made the remaining
  shadow lane reachable and fail-closed on the active V3 path:
  `COPY ... WITH (SHADOW_LOAD true)` now classifies to
  `SHADOW_LOAD_CUTOVER`, publishes that lane in `bulk_load_plan`, writes a
  `BLOCKED` `bulk_load_cutover_guard`, and records a single
  `PLANNED -> ABORTED_FAIL_CLOSED` event with code
  `SHADOW_LOAD_CUTOVER_UNAVAILABLE` before returning a clear runtime error.
- Focused proof for this shadow-request slice is green with `6/6` passing
  tests:
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`,
  `CopyExecutorTest.CopyFromPublishesRetailBulkLoadCatalogState`,
  `CopyExecutorTest.CopyFromPublishesSortedExactBulkLoadCatalogState`,
  `CopyExecutorTest.ClassifyCopyBulkLaneChoosesShadowCutoverWhenRequested`,
  `CopyExecutorTest.CopyFromShadowLoadRequestPublishesBlockedCutoverStateAndFailsClosed`,
  and
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`.
- This closes the unreachable-shadow-lane gap, not section `39`.
  The remaining Beta 1 bulk-ingest gap is still real object-level
  `SHADOW_LOAD_CUTOVER` execution and publication, plus branch-isolated
  non-V3 proof for sorted/shadow lane behavior.
- On `2026-04-03`, the next bounded section-`39` observability slice landed:
  `bulk_load_plan` rows no longer remain stranded at `PLANNED` after the live
  `COPY FROM` paths execute. `CatalogManager` now supports a narrow
  `phase_state` update for bulk-load plans, and both live `COPY FROM`
  handlers advance that durable row to `RUNNING`, `COMPLETED`, or
  `ABORTED_FAIL_CLOSED` in step with the already-published event stream.
- Focused proof for this phase-state slice is green with `9/9` passing tests:
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
- This closes a real durable-state truthfulness gap, not section `39`.
  The remaining Beta 1 bulk-ingest gap is still real object-level
  `SHADOW_LOAD_CUTOVER` execution and publication, plus branch-isolated
  non-V3 proof for sorted/shadow lane behavior.
- On `2026-04-03`, the next bounded section-`39` parity slice closed that
  remaining branch-isolated non-V3 proof gap:
  the PostgreSQL parser-emitted fixed-format `COPY` stream now accepts
  `HEADER true` in the same bounded form as the active path, recognizes
  `SHADOW_LOAD`, and emits the trailing `BATCH_SIZE`, `MAX_ERRORS`, and
  `ON_ERROR` defaults that the shared executor compatibility branch already
  expects.
- Focused proof for this legacy PostgreSQL COPY slice is green with `15/15`
  passing tests:
  `CopyExecutorTest.CopyCsvFromWithHeaderAndDelimiter`,
  `CopyExecutorTest.ResolveCopyBulkPlanUsesConfiguredRetailMicroBatchDefault`,
  `CopyExecutorTest.ResolveCopyBulkPlanHonorsExplicitBatchSizeOverride`,
  `CopyExecutorTest.ClassifyCopyBulkLaneChoosesSortedExactBulkAtThreshold`,
  `CopyExecutorTest.ClassifyCopyBulkLaneChoosesShadowCutoverWhenRequested`,
  `CopyExecutorTest.CopyFromPublishesRetailBulkLoadCatalogState`,
  `CopyExecutorTest.CopyFromPublishesSortedExactBulkLoadCatalogState`,
  `CopyExecutorTest.CopyFromShadowLoadRequestPublishesBlockedCutoverStateAndFailsClosed`,
  `CopyExecutorTest.LegacyCopyFromPublishesSortedExactBulkLoadCatalogState`,
  `CopyExecutorTest.LegacyCopyFromShadowLoadRequestPublishesBlockedCutoverStateAndFailsClosed`,
  `CopyExecutorTest.CopyEncodingUtf8Accepted`,
  `CopyExecutorTest.CopyEncodingUnsupportedRejected`,
  `CopyExecutorTest.CopyBinaryRoundTrip`,
  `CopyExecutorTest.CopyToSupportsHeader`,
  and
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`.
- A proof-harness nuance also became explicit in this slice:
  raw parser-emitted legacy bytecode still carries `EXT_DEBUG_SPAN` markers,
  while the compatibility executor intentionally remains narrower than the
  canonical container path. The focused legacy proof therefore strips that
  parser-only marker before invoking `SCRATCHBIRD_ENABLE_LEGACY_EXECUTE`; this
  is bounded test-harness hygiene, not a new product regression.
- This closes the remaining branch-isolated non-V3 proof gap for section `39`.
  The remaining Beta 1 bulk-ingest gap is now only real object-level
  `SHADOW_LOAD_CUTOVER` execution and publication.
- On `2026-04-03`, that remaining bounded section-`39` execution gap was
  closed on the active and legacy `COPY FROM` paths:
  object-level `SHADOW_LOAD_CUTOVER` now completes and publishes committed
  cutover state instead of failing closed, and the blocking runtime defect was
  traced to non-primary heap tuple identity rather than lane planning.
- The root cause was concrete: shadow-loaded rows were physically present in
  the target tablespace after cutover, but `HeapPage` was still stamping
  `ctid_gpid` against the primary tablespace on insert and same-page MGA
  maintenance. That left the new rows with source-style `ctid`s such as
  `0:2:1` while the owning target heap page was `2:2:*`, so ordinary visible
  scans rejected them even though raw heap scans found all rows.
- The fix now carries the page tablespace through `HeapPage`, stamps tuple
  `ctid_gpid` values against the real owning tablespace, and passes that
  tablespace explicitly on the non-primary insert, scan, and delete paths that
  the shadow-cutover execution path exercises.
- Focused proof after this fix is green with `2/2` on the committed-cutover
  shadow tests:
  `CopyExecutorTest.CopyFromShadowLoadRequestPublishesCommittedCutoverState`
  and
  `CopyExecutorTest.LegacyCopyFromShadowLoadRequestPublishesCommittedCutoverState`.
- On April 3, 2026, a fresh clean-build benchmark rerun was preserved at
  `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/clean-rebuild-scratchbird-stress-20260404T023343Z`.
  This was not an incremental rebuild: the engine build tree was deleted,
  reconfigured in `Release`, rebuilt far enough to produce the production
  runtime binaries, and the benchmark ScratchBird runtime was restarted from
  that fresh tree before the stress run.
- The benchmark runtime startup exposed one operational dependency that is now
  explicit in the evidence chain: `start-engine.sh scratchbird` depends on the
  native `sb_isql` CLI from `ScratchBird-driver`. The local default lookup did
  not include the freshly built `build_cli` lane, so the run used
  `SCRATCHBIRD_SB_ISQL=/home/dcalford/CliWork/ScratchBird-driver/build_cli/tracks/p3/drivers/cli/sb_isql`
  during runtime bring-up. No driver code changed in this step; this was a
  reproducibility/wiring requirement only.
- Compared with the preserved comparable transaction-aware baseline in
  `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/txmode-matrix-20260403T152011Z/scratchbird/stress`,
  total load time improved in both ScratchBird-supported modes:
  `36.96s -> 34.99s` in `normal_transactional` and
  `46.51s -> 41.63s` in `autocommit`.
- Query/test time was mixed rather than uniformly better. In
  `normal_transactional`, `self_join_same_country` improved
  `316.77ms -> 285.61ms`, `inner_join_large_result` improved
  `14.44s -> 13.92s`, and `bulk_update_with_join` improved
  `143.66ms -> 118.06ms`, but `multi_dimensional_agg` regressed
  `4.23s -> 5.05s` and `bulk_insert_select` regressed
  `14.61s -> 15.29s`. In `autocommit`, `self_join_same_country` improved
  `406.04ms -> 368.52ms`, `inner_join_large_result` improved
  `16.20s -> 15.85s`, and `multi_dimensional_agg` improved
  `5.23s -> 5.09s`, but `bulk_insert_select` regressed
  `19.26s -> 21.26s` and `bulk_update_with_case` regressed
  `1.29s -> 1.54s`.
- The clean rerun therefore shows real progress on bounded load-path and some
  join/update surfaces, but it also keeps the later-phase query/runtime
  benchmark gaps visible instead of masking them behind incremental-build
  noise.
- The broader `COPY` regression slice is also green with `9/9` passing tests,
  including the two committed-cutover shadow proofs plus the bounded retail,
  sorted-exact, and bulk-handle regressions.
- This closes the bounded section `39` `COPY` lane closure for package `08`.
  The full expanded package workplan remains open on later phases outside this
  bounded bulk-ingest lane set.
- On `2026-04-03`, the next bounded phase-`3` slice landed durable
  `index_build_*` publication on the existing shadow-index rebuild path:
  `CatalogManager` now persists `index_build_plan`, `index_build_event`,
  `index_build_progress`, and `index_build_cutover_guard` rows, stores their
  table page ids in the catalog root, and publishes the current shadow flow as
  `DRAFTED -> BUILDING -> CUTOVER_PENDING -> PUBLISHED`.
- The first proof attempt exposed the real reopen blocker: the new
  `index_build_*` pages were present before close, but reopen failed with
  `Duplicate object name in scope: type=3 name=users.public.test_table.test_index`.
  The failure was not catalog-root persistence; generic resolver rebuild was
  still admitting all physical index versions as user-visible named objects.
- The bounded fix now keeps generic object-name resolution on the published
  `ACTIVE` index version only. `BUILDING` and `RETIRED` physical index rows
  remain version-visible through `index_cache_` and `getVisibleIndexVersion()`,
  but they no longer trip duplicate-name rebuild failures on reopen.
- Focused integration proof for this slice is green with `3/3` passing tests:
  `ShadowIndexRebuildTest.BasicShadowCreation`,
  `ShadowIndexRebuildTest.ShadowPromotion`,
  and
  `ShadowIndexRebuildTest.PublishesDurableIndexBuildCatalogState`.
- This closes a bounded phase-`3` durability/proof gap on the existing shadow
  rebuild path. The wider phase-`3` work remains open: richer visibility-state
  semantics, online build parity beyond shadow rebuild, and the remaining
  later-phase performance surfaces outside section `39`.
- On `2026-04-03`, the first bounded phase-`4` metrics-freshness slice landed
  on the active optimizer path. Published family metrics now carry
  `metrics_publication_epoch`, `metrics_freshness_class`,
  `metrics_invalidation_state`, and `metrics_invalidation_reason` in both the
  canonical metrics packet and the shared envelope payload, and runtime-plan
  relations plus access-path descriptors now preserve those fields.
- Planner consumption now uses those published fields directly instead of
  inferring everything from older confidence/queryability signals. In this
  bounded slice, `UNUSABLE` or hard-invalidated metrics force the candidate
  family into refusal for winner selection, while `AGED` and
  `STALE_DEGRADED` carry distinct penalties instead of collapsing into the
  older maintenance-state heuristics alone.
- Focused proof after this slice is green with `3/3` passing tests:
  `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
  `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
  and
  `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
- One proof expectation had to be corrected rather than forcing the runtime
  into a false `CURRENT` state. Direct `refreshIndexFamilyMetrics()` on the
  ART index path is an unsampled/medium-confidence refresh, so the canonical
  published freshness is `AGED`, not `CURRENT`; `CURRENT` still holds for the
  sampled high-confidence `ANALYZE INDEX ... WITH (sample_rate = 0.25)` path.
- This does not close full phase `4`. The remaining gap is broader optimizer
  parity: explicit refusal bundles for every implemented family, preserved
  refusal reasons in candidate bundles, and the later replan-trigger work.
- On `2026-04-03`, the first bounded explicit-refusal bundle slice landed on
  top of that metrics-freshness work. `RuntimePlanRelation` now preserves
  structured `candidate_family_refusals` entries with family,
  `candidate_label`, `refusal_class`, `refusal_cause_domain`,
  `refusal_reason_code`, and `refusal_detail`, and the base-relation candidate
  bundle now carries those structured refusals into the chosen runtime
  relation instead of losing them when winner selection falls back to
  `SEQ_SCAN`.
- The main correction in this slice was not a new planner branch but refusal
  detail fidelity. The maintenance-state fail-closed path already knew when a
  family had `UNUSABLE` or hard-invalidated metrics, but its stored refusal
  detail dropped the lifecycle freshness/invalidation fields before canonical
  classification. That path now records the full lifecycle snapshot in the
  refusal detail, so those refusals classify as `unusable metrics` with
  `METRICS` cause domain instead of degrading into an opaque policy fallback.
- Focused proof after this slice is green with `3/3` on the planner/runtime
  unit slice:
  `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
  `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
  and
  `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
- The standalone shadow-index integration target is also still green after the
  rebuild with `8/8` passing tests, including
  `ShadowIndexRebuildTest.BasicShadowCreation`,
  `ShadowIndexRebuildTest.ShadowPromotion`,
  and
  `ShadowIndexRebuildTest.PublishesDurableIndexBuildCatalogState`.
- This closes only the first preserved-refusal rung. Full phase `4` / phase
  `6` optimizer parity still needs implemented-family-wide winner-or-refusal
  coverage, preserved refusal publication across all candidate families, and
  the later replan-trigger work.
- On `2026-04-03`, the next bounded optimizer-parity slice landed on top of
  that refusal substrate: MGA churn/governance rejections are now preserved in
  `candidate_family_refusals` instead of only existing as `MGA_SWITCHOVER`
  trace rows.
- The planner helper now records those rejections with reason code
  `P08_MGA_GOVERNANCE_REJECTED` while keeping the original
  `MGA_SWITCHOVER` runtime-trace phase, so the preserved candidate bundle and
  the live trace now agree on why the family was refused under severe MGA
  churn.
- Focused proof after this slice is green with `5/5` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
  `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
  `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
  `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
  and
  `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
- The standalone shadow-index integration target also remains green after the
  rebuild with `8/8` passing tests.
- This closes another bounded winner-or-refusal gap, but it is still not full
  phase `4` / phase `6` closure. Wider implemented-family refusal coverage,
  replan-trigger semantics, and memory-admission work are still open.
- On `2026-04-03`, the next bounded optimizer-parity slice landed on the
  cost-domain side of family refusal preservation. Implemented-family paths
  that lose because the current best path is cheaper, or because family
  metrics mark an exact path structurally overweight, now publish structured
  `candidate_family_refusals` entries instead of only raw `rejected_paths`
  trace rows.
- The planner now classifies `P08_HIGHER_TOTAL_COST_THAN_CURRENT_BEST` as
  `higher estimated cost than current winner` with `COST` cause domain, and
  `P08_STRUCTURALLY_OVERWEIGHT_EXACT_PATH` as
  `structurally overweight path under current metrics` with `COST` cause
  domain.
- Focused proof after this slice is green with `6/6` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
  `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
  `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
  `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
  `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`,
  and
  `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
- The expensive exact-family fallback proof now stays honest about the real
  runtime cause: the path was not merely a generic cost loser, it was being
  refused earlier as a structurally overweight exact path under the published
  family metrics. That refusal is now preserved in the candidate bundle.
- This closes another bounded phase `4` / phase `6` gap, but broader
  implemented-family refusal coverage, replan-trigger work, and memory
  admission still remain open.
- On `2026-04-03`, the next bounded optimizer-parity slice landed on the
  semantics side of family refusal preservation. When a base relation has
  indexes but a predicate still matches none of them, the planner now
  publishes a structured `candidate_family_refusals` entry instead of only a
  raw `rejected_paths` trace row.
- The planner now classifies `P08_NO_MATCHING_INDEX_FOR_PREDICATE` as
  `semantic mismatch` with `SEMANTICS` cause domain, and preserves the
  original predicate text in the refusal detail so the candidate bundle stays
  useful for post-plan diagnosis.
- Focused proof after this slice is green with `7/7` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
  `QueryPlannerIntegrationTest.MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn`,
  `QueryPlannerIntegrationTest.MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn`,
  `QueryPlannerIntegrationTest.AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan`,
  `QueryPlannerIntegrationTest.UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection`,
  `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`,
  and
  `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`.
- This closes another bounded winner-or-refusal rung, but broader
  implemented-family refusal coverage, replan-trigger work, and memory
  admission still remain open.
- On `2026-04-04`, the next bounded optimizer-parity slice landed on the
  native-family promotion side of refusal preservation. Enumerated native
  families that fail their current promotion threshold no longer disappear
  into raw `rejected_paths` trace rows only.
- The planner now records those cases as structured
  `candidate_family_refusals` entries with `METRICS` cause domain. The
  current bounded family covers
  `P08_*_NATIVE_PROMOTION_THRESHOLD_NOT_MET`,
  `P08_ANN_HYBRID_FALLBACK_NOT_JUSTIFIED`, and
  `P08_TEXT_SCORE_ROWS_REQUIRED`, all mapped to
  `family-specific promotion threshold not met`.
- Focused proof after this slice is green with `8/8` passing tests in
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
- The bounded proof uses a BRIN candidate whose published summary metrics fall
  below the native promotion threshold, forcing `SEQ_SCAN` while preserving a
  structured `P08_SUMMARY_NATIVE_PROMOTION_THRESHOLD_NOT_MET` refusal in the
  candidate bundle.
- This closes another bounded implemented-family refusal rung, but broader
  family-wide refusal coverage, replan-trigger work, and memory admission
  still remain open.
- On `2026-04-04`, the next bounded optimizer-parity slice landed on the
  generalized/spatial lowering side of refusal preservation. Invalid
  generalized-family lowering states no longer collapse into a generic
  `P08_FAMILY_NOT_QUERYABLE` bucket when the lowering helper has enough shape
  information to classify the failure more precisely.
- The lowering helper now distinguishes
  `P08_OPERATOR_STRATEGY_UNBOUND`,
  `P08_SUPPORT_FUNCTION_UNVALIDATED`,
  `P08_DISTANCE_SUPPORT_UNVALIDATED`, and
  `P08_NEAREST_ORDER_UNVALIDATED`, and the planner maps the generalized
  invalid shapes that reach the candidate bundle to
  `unsupported operator shape` or `missing required runtime capability`
  instead of a generic policy refusal.
- Focused proof after this slice is green with `5/5` passing tests in
  `scratchbird_tests`:
  `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
  `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
  `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
  `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
  and
  `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`.
- The GIST integration proof now records the real planner boundary. Before
  validated opclass support exists, the planner still fails closed earlier as
  `P08_NO_MATCHING_INDEX_FOR_PREDICATE`; once the bound support function is
  published, `GIST_SCAN` enters the candidate family set and is no longer
  treated as a semantic mismatch.
- This closes another bounded implemented-family refusal rung, but broader
  family-wide refusal coverage, replan-trigger work, and memory admission
  still remain open.
- On `2026-04-04`, the next bounded optimizer-parity slice landed on the ANN
  family side of invalid lowering. ANN invalid states no longer collapse into
  generic `P08_FAMILY_NOT_QUERYABLE` when the lowering helper has enough
  information to classify the failure more precisely.
- The lowering helper now distinguishes
  `P08_ANN_NEAREST_ORDER_REQUIRED`,
  `P08_ANN_METRIC_INCOMPATIBLE`, and
  `P08_ANN_CANDIDATE_BUDGET_REQUIRED`, and the planner maps those bounded ANN
  invalid shapes to `missing required runtime capability` instead of a
  generic policy refusal.
- Focused proof after this slice is green with `6/6` passing tests in
  `scratchbird_tests`:
  `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
  `IndexFamilyLoweringTest.AnnInvalidCasesPublishSpecificRejectionCodes`,
  `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
  `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
  `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
  and
  `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`.
- The bounded ANN proof covers three distinct fail-closed cases: nearest-order
  request missing, metric compatibility missing, and candidate budget
  missing. Those cases now carry explicit refusal codes instead of sharing a
  generic family-not-queryable bucket.
- This closes another bounded implemented-family refusal rung, but broader
  family-wide refusal coverage, replan-trigger work, and memory admission
  still remain open.
- On `2026-04-04`, the next bounded optimizer-parity slice landed on the
  ranked-text side of invalid lowering. Ranked text invalid states no longer
  collapse into generic `P08_FAMILY_NOT_QUERYABLE` when the lowering helper
  has enough information to classify the failure more precisely.
- The lowering helper now distinguishes
  `P08_TEXT_CORPUS_STATS_REQUIRED` and
  `P08_TEXT_CANDIDATE_BUDGET_REQUIRED`, and the planner maps those bounded
  ranked-text invalid shapes to `missing required runtime capability`
  instead of a generic policy refusal.
- Focused proof after this slice is green with `7/7` passing tests in
  `scratchbird_tests`:
  `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`,
  `IndexFamilyLoweringTest.AnnInvalidCasesPublishSpecificRejectionCodes`,
  `IndexFamilyLoweringTest.TextInvalidCasesPublishSpecificRejectionCodes`,
  `QueryPlannerIntegrationTest.GistCandidateRequiresBoundOpclassStrategySupport`,
  `QueryPlannerIntegrationTest.SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal`,
  `QueryPlannerIntegrationTest.NonIndexedPredicatePublishesSemanticMismatchRefusal`,
  and
  `QueryPlannerIntegrationTest.TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity`.
- The bounded ranked-text proof covers two distinct fail-closed cases:
  missing corpus scoring statistics and missing score candidate budget. Those
  cases now carry explicit refusal codes instead of sharing a generic
  family-not-queryable bucket.
- This closes another bounded implemented-family refusal rung, but broader
  family-wide refusal coverage, replan-trigger work, and memory admission
  still remain open.
- On `2026-04-04`, the next bounded optimizer-parity slice landed on the
  hash-family side of invalid lowering. Non-equality hash-family predicates no
  longer collapse into generic `P08_FAMILY_NOT_QUERYABLE` when the lowering
  helper has enough information to classify the failure more precisely.
- The lowering helper now distinguishes `P08_HASH_EQ_PREDICATE_REQUIRED`, and
  the planner maps that bounded hash-family invalid shape to
  `unsupported operator shape` instead of a generic policy refusal.
- Focused proof after this slice is green with `8/8` passing tests in
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
- The bounded hash proof covers the fail-closed case where a hash-family index
  is asked to satisfy a range predicate. That case now carries an explicit
  refusal code instead of sharing a generic family-not-queryable bucket.
- This closes another bounded implemented-family refusal rung, but broader
  family-wide refusal coverage, replan-trigger work, and memory admission
  still remain open.
- On `2026-04-04`, the next bounded phase-`4` freshness/explain slice landed
  on family-native metrics provenance. The planner now emits dedicated
  `FAMILY_NATIVE_METRICS` statistics provenance rows for published family
  metrics instead of relying only on relation fields and candidate signatures.
- The new provenance detail carries the family name, freshness class,
  invalidation state, maintenance state, publication epoch, and bounded
  current-compile policy markers:
  `refresh_attempted=false`,
  `refresh_policy=ASYNC_OR_ADMIN`,
  and
  `replan_boundary=FAMILY_STATISTICS_SIGNATURE`.
- Focused proof after this slice is green with `10/10` passing tests in
  `scratchbird_tests`:
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
- The proof surface now covers both runtime-plan decoding and `EXPLAIN (FORMAT JSON)`
  visibility for typed family metrics, including the fail-closed `UNUSABLE` /
  `INVALIDATED_HARD` case that suppresses the index from winner selection.
- This closes a bounded freshness-provenance rung, but broader family-wide
  refusal coverage, explicit replan-trigger work, and memory admission still
  remain open.
- On `2026-04-04`, the next bounded phase-`4`/phase-`6` refusal-fidelity
  slice moved the planner bundle-refusal classifier into reusable canonical
  helpers and closed the new maintenance/trust mapping proof without relying
  on the currently unsupported nearest-neighbor SQL surface in the v3 test
  harness.
- The canonical helper now preserves
  `P08_MAINTENANCE_STATE_INCOMPATIBLE` as
  `maintenance state incompatible with trust class` with `METRICS` cause
  domain, and `P08_TRUST_LOCATOR_UNDECLARED` as
  `missing trust or locator classification` with `POLICY` cause domain.
- Focused proof after this slice is green with `11/11` passing tests in
  `scratchbird_tests`:
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
- The attempted ANN integration proof was deliberately abandoned after the
  current v3 compiler surface rejected the nearest-neighbor query shape at
  parse time. The preserved evidence for this slice therefore stays at the
  canonical helper and planner-regression level.
- On `2026-04-04`, the next bounded phase-`4`/phase-`6` optimizer-parity
  slice preserved `P08_MGA_GOVERNANCE_REJECTED` as its own structured refusal
  instead of leaving it in the generic fail-closed policy bucket.
- The canonical helper now classifies this path as
  `MGA governance rejected candidate under current pressure` with `POLICY`
  cause domain, and the severe-churn broad covering-index proof now asserts
  that specific class.
- Focused proof after this slice is green with `13/13` passing tests in
  `scratchbird_tests`:
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
- On `2026-04-04`, the next bounded phase-`4` slice closed the first explicit
  family-statistics replan-boundary proof instead of leaving that behavior as a
  silent plan-cache miss.
- The actual defect was in sibling detection, not in payload annotation:
  family-metrics refresh can legitimately recalibrate `cost_profile_id`, and
  the initial cache-boundary matcher was incorrectly requiring identical cost
  profile ids. That suppressed `FAMILY_STATISTICS_REPLAN` proof publication
  even though the compile really crossed a new
  `FAMILY_STATISTICS_SIGNATURE` boundary.
- The fix now counts cached sibling variants across that bounded
  cost-profile recalibration and annotates the returned compiled payload with
  `FAMILY_STATISTICS_REPLAN` statistics provenance,
  a `PLAN_CACHE/CACHE_REUSE/REJECTED` row for
  `family statistics signature boundary crossed`,
  and the optimizer control
  `FAMILY_STATISTICS_REPLAN_BOUNDARY=FAMILY_STATISTICS_SIGNATURE`.
- Focused proof after this slice is green with `11/11` passing tests in
  `scratchbird_tests`:
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
- This closes the first explicit family-statistics replan proof, but broader
  family-wide refusal coverage, wider replan-trigger work, and memory
  admission still remain open.
- On `2026-04-04`, the next bounded phase-`4`/phase-`6` optimizer-parity
  slice preserved canonical family-legality failures as distinct structured
  refusals instead of leaving them in the generic fail-closed bucket.
- The canonical bundle-refusal helper now preserves
  `P08_FAMILY_LEGALITY_UNDECLARED` as
  `missing canonical family legality classification`,
  `P08_FAMILY_LEGALITY_TRUST` as
  `trust class violates canonical family legality matrix`,
  `P08_FAMILY_LEGALITY_LOCATOR` as
  `locator granularity violates canonical family legality matrix`,
  and `P08_FAMILY_LEGALITY_VISIBILITY` as
  `visibility enforcement violates canonical family legality matrix`.
- These bounded legality paths remain in the current `POLICY` cause domain,
  which matches the existing fail-closed family-admission boundary.
- Focused proof after this slice is green with `16/16` passing tests in
  `scratchbird_tests`:
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
- This closes the bounded legality-classification gap only. Broader
  implemented-family refusal coverage, wider replan-trigger work, and memory
  admission still remain open.
- On `2026-04-04`, the next bounded optimizer-parity slice closed the
  same-column unmatched-predicate gap that was still collapsing into generic
  semantic mismatch.
- The unmatched-predicate prepass now inspects indexes on the predicate
  column before emitting `P08_NO_MATCHING_INDEX_FOR_PREDICATE`. That preserves
  family-specific lowering failures such as
  `P08_HASH_EQ_PREDICATE_REQUIRED` for hash range predicates, keeps
  `P08_PARTIAL_INDEX_PREDICATE_MISMATCH` on the partial-index boundary, and
  lets the GIST proof surface the stricter lowering refusal
  `P08_OPERATOR_STRATEGY_UNBOUND` instead of generic unmatched fallback.
- Focused proof after this slice is green with `13/13` passing tests in
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
- This closes another bounded implemented-family refusal gap, but broader
  family-wide refusal coverage, wider replan-trigger work, and memory
  admission still remain open.
- On `2026-04-04`, the next bounded phase-`5` slice closed the first live
  planner-consumption rung for durable grant feedback.
- The real blocker was identity drift across the new feedback path:
  compiler-side snapshotting and test/catalog helpers were keyed with the
  canonical `sblr::v3::stableHash64` contract, while planner lookup was still
  using a local FNV variant. The compiler also needed to hand the planner the
  effective schema from the live connection instead of an unset local schema.
- The live planner now finds the durable `memory_grant_feedback` row,
  publishes `MEMORY_GRANT_FEEDBACK_HASH_JOIN` and
  `MEMORY_GRANT_FEEDBACK_STATE_HASH_JOIN` from `CATALOG`, emits a
  `MEMORY_GRANT_FEEDBACK ... APPLIED` trace entry, and no longer rejects the
  hash candidate because `spill policy disallows` once the durable feedback
  path is active.
- Focused proof after this slice is green with `3/3` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowRejectsSpilledHashJoin`,
  and
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin`.
- This closes the first bounded phase-`5` planner-consumption rung only;
  broader operator reservation and runtime spill/workfile execution still
  remain open.
- On `2026-04-04`, the first bounded phase-`5` durability slice landed for
  canonical `memory_grant_feedback`.
- `CatalogManager` now persists `memory_grant_feedback` rows durably through
  the catalog root, with keyed upsert/get/list/delete helpers and bounded
  inline storage for `operator_kind` and feedback `state`.
- Focused proof now shows the new family rejects invalid states, preserves the
  original UUID and `created_time` across same-key updates, survives reopen,
  and deletes cleanly by `grant_key_hash`.
- Focused verification after this slice is green with `1/1` passing tests for
  `CatalogRoutingAdmissionExtensionContractTest.MemoryGrantFeedbackCatalogContracts`,
  and the widened
  `CatalogRoutingAdmissionExtensionContractTest.*` suite is also green with
  `3/3` passing tests in `scratchbird_tests`.
- This closes the first bounded phase-`5` catalog durability rung only.
  Feedback-driven grant admission, resizing, and runtime spill/workfile
  execution still remain open.
- On `2026-04-04`, the remaining bounded phase-`4`/phase-`6` refusal and
  replan slice was closed.
- `IndexFamilyLoweringTest.GeneralizedInvalidCasesPublishSpecificRejectionCodes`
  now also proves `P08_SUPPORT_FUNCTION_UNVALIDATED`, and the new
  `IndexFamilyLoweringTest.CanonicalPlannerBundleRefusalClassifiesRemainingFamilyFailureCodes`
  closes the last uncovered canonical classifier mappings for
  `P08_SUPPORT_FUNCTION_UNVALIDATED`,
  `P08_BITMAP_COMPOSE_UNAVAILABLE`,
  `P08_SKIP_SCAN_UNAVAILABLE`,
  `P08_EXPRESSION_INDEX_MISMATCH`, and
  `P08_FAMILY_NOT_QUERYABLE`.
- The focused package rerun also carried the live replan surfaces together:
  `QueryPlannerIntegrationTest.CardinalityFeedbackBypassesStaleCacheAndRebuildsPlan`,
  `QueryPlannerIntegrationTest.AdaptiveFeedbackCachedPlanReflectsLatestFeedbackStateAfterRepeatExecution`,
  and
  `QueryPlannerIntegrationTest.FamilyStatisticsSignatureBoundaryPublishesExplicitReplanProof`.
- Focused verification after this slice is green with `23/23` passing tests
  in `scratchbird_tests`, spanning the remaining classifier surface, the
  bounded promotion-threshold/refusal suite, and both explicit replan
  trigger families.
- This closes the remaining package-owned phase-`4`/phase-`6`
  refusal-model and replan-trigger slices; the next live frontier is phase
  `5` memory-grant and spill-admission work.
- On `2026-04-04`, the next bounded optimizer-parity slice closed the
  canonical classifier proof gap for the remaining promotion-threshold reason
  codes that were still unproved directly.
- The attempted ANN runtime-plan proof was blocked by the current vector query
  surface, so this slice stayed at the planner telemetry boundary instead of
  overstating ANN query-path coverage.
- Focused proof now shows the canonical bundle classifier maps
  `P08_ANN_NATIVE_PROMOTION_THRESHOLD_NOT_MET`,
  `P08_ANN_HYBRID_FALLBACK_NOT_JUSTIFIED`,
  `P08_GENERALIZED_NEAREST_NATIVE_PROMOTION_THRESHOLD_NOT_MET`,
  `P08_GENERALIZED_NATIVE_PROMOTION_THRESHOLD_NOT_MET`, and
  `P08_TEXT_SCORE_ROWS_REQUIRED` to
  `family-specific promotion threshold not met` with `METRICS` cause domain.
- Focused verification after this slice is green with `16/16` passing tests
  in `scratchbird_tests`, including the new
  `QueryPlannerIntegrationTest.CanonicalPlannerBundleRefusalClassifiesAdditionalPromotionThresholdFailures`
  proof plus the current ranked-text, bitmap/columnstore, summary-threshold,
  same-column lowering, replan-boundary, GIST-boundary, severe-churn MGA,
  and statistics-manager regression slice.
- This closes another bounded implemented-family refusal gap, but broader
  family-wide refusal coverage, wider replan-trigger work, and memory
  admission still remain open.
- On `2026-04-04`, the next bounded optimizer-parity slice closed the
  ranked-text native promotion-threshold proof gap.
- The planner was already emitting
  `P08_TEXT_NATIVE_PROMOTION_THRESHOLD_NOT_MET`, but that ranked-text path
  did not yet have preserved integration proof at the runtime-plan boundary.
- The new focused proof now shows the text family remains in
  `candidate_scan_families`, falls back to `SEQ_SCAN`, and preserves a
  structured `candidate_family_refusals` entry for
  `TEXT_BITMAP_SCAN[idx_docs_title_text]` with
  `family-specific promotion threshold not met` in the `METRICS` cause
  domain.
- Focused proof after this slice is green with `15/15` passing tests in
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
- This closes another bounded implemented-family refusal gap, but broader
  family-wide refusal coverage, wider replan-trigger work, and memory
  admission still remain open.
- On `2026-04-04`, the next bounded optimizer-parity slice closed the native
  promotion-threshold proof gap for bitmap and columnstore families.
- The planner was already emitting
  `P08_BITMAP_NATIVE_PROMOTION_THRESHOLD_NOT_MET` and
  `P08_COLUMNSTORE_NATIVE_PROMOTION_THRESHOLD_NOT_MET`, but those paths did
  not yet have preserved integration proof at the runtime-plan boundary.
- The new focused proof now shows both families remain in
  `candidate_scan_families`, fall back to `SEQ_SCAN`, and preserve structured
  `candidate_family_refusals` entries with
  `family-specific promotion threshold not met` in the `METRICS` cause
  domain.
- Focused proof after this slice is green with `14/14` passing tests in
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
- This closes another bounded implemented-family refusal gap, but broader
  family-wide refusal coverage, wider replan-trigger work, and memory
  admission still remain open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  resize-feedback and operator-reservation slice.
- The runtime-plan boundary now preserves recorded cost evidence instead of
  recomputing baseline resource metadata, so feedback-adjusted
  `memory_budget_bytes` and `spill_expected` survive for merge join, sort,
  window, aggregate, and distinct operators.
- The executor now persists durable `memory_grant_feedback` samples from
  executed `SORT` and `HASH_AGG` plans, giving the package its first
  execute-to-recompile feedback loop beyond hash join.
- The reusable-plan chooser now derives statement reservation from the peak
  operator budget across the full runtime plan and publishes
  `PLAN_MEMORY_RESERVATION_BYTES`, which closes the wrapped-operator
  under-admission case where `Limit` previously masked a child `Sort` budget.
- Focused proof after this slice is green with `9/9` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.AutoPlanProfileUsesChooserAndPublishesReuseMetadata`,
  `QueryPlannerIntegrationTest.AutoPlanProfilePublishesPeakOperatorMemoryReservationForWrappedSort`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForMergeJoin`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashAggregate`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForSort`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForWindow`,
  `QueryPlannerIntegrationTest.ExecutedSortPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  and
  `QueryPlannerIntegrationTest.ExecutedHashAggregatePersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`.
- This closes the first bounded upper-stage resize-feedback and
  chooser-visible operator reservation rung, but explicit runtime admission
  and spill/workfile execution still remain open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  runtime-admission slice.
- The executor now validates embedded runtime plans against the live session
  before execution and fails closed when `SPILL_POLICY=DISALLOW` would reject
  the already-compiled plan:
  either because the plan still carries `spill_expected=true`, or because its
  recorded memory reservation exceeds the live `WORK_MEM` ceiling.
- Focused proof after this slice is green with `12/12` passing tests in
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
- This closes the first bounded stale-bytecode runtime-admission gap, but
  runtime spill/workfile execution itself still remains open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  execute-to-recompile feedback expansion slice.
- The durable `memory_grant_feedback` path is now proven end to end for
  executed `WINDOW` and hash-`DISTINCT` plans, not just `SORT` and
  grouped/hash aggregate.
- Focused proof after this slice is green with `6/6` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.ExecutedSortPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  `QueryPlannerIntegrationTest.ExecutedWindowPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  `QueryPlannerIntegrationTest.ExecutedHashAggregatePersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  `QueryPlannerIntegrationTest.ExecutedHashDistinctPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForWindow`,
  and
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashDistinct`.
- This closes another bounded execute-to-recompile feedback gap, but
  join-family execution feedback beyond hash join and runtime spill/workfile
  execution still remain open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  join-family execute-to-recompile feedback slice.
- Hash join no longer has only catalog-seeded planner-consumption proof; the
  package now preserves executed proof that runtime feedback seeds the later
  spill-disallow compile path.
- Focused proof after this slice is green with `4/4` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowRejectsSpilledHashJoin`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin`,
  and
  `QueryPlannerIntegrationTest.ExecutedHashJoinPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`.
- This closes the executed hash-join feedback proof gap, but merge-join
  execution feedback and runtime spill/workfile execution still remain open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  merge-join runtime spill/workfile slice.
- The V3 executor no longer sorts forced `SORT_TO_MERGE` inputs purely in
  memory: low-budget merge joins now spill sorted runs to `sb_workfile`, merge
  them back in key order, and publish actual merge-join spill observation into
  durable `memory_grant_feedback`.
- Focused proof after this slice is green with `3/3` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForMergeJoin`,
  `QueryPlannerIntegrationTest.ExecutedMergeJoinPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  and
  `QueryPlannerIntegrationTest.ExecuteBytecodeRunsSpilledMergeJoinThroughWorkfileAndPersistsFeedback`.
- This closes the first bounded join-family runtime spill/workfile gap, but
  broader upper-stage runtime spill/workfile behavior still remains open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  upper-stage runtime spill/workfile slice.
- The V3 executor no longer relies on the later top-level sort plumbing for
  stale window bytecode:
  it now extracts the embedded window ordering from the V3 projection payload,
  orders the window input stream at the `WINDOW` boundary, spills oversized
  runs to `sb_workfile`, merges them back in key order, and records actual
  `WINDOW` spill observation into durable `memory_grant_feedback`.
- Focused proof after this slice is green with `3/3` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.ExecutedWindowCapturesActualSortSpillOnStaleBytecode`,
  `QueryPlannerIntegrationTest.ExecutedWindowPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  and
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForWindow`.
- This closes the bounded stale-window runtime spill/workfile gap, but hash
  join and broader operator-runtime spill behavior still remain open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  hash-join runtime spill/workfile slice.
- The V3 executor no longer relies only on planner-side spill intent for stale
  hash-join bytecode:
  it now partitions oversized build/probe inputs through `sb_workfile`,
  rejoins those partitions at execution time, and records actual `HASH_JOIN`
  spill observation into durable `memory_grant_feedback`.
- Focused proof after this slice is green with `5/5` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowRejectsSpilledHashJoin`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin`,
  `QueryPlannerIntegrationTest.ExecutedHashJoinPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  and
  `QueryPlannerIntegrationTest.ExecutedHashJoinCapturesActualSpillOnStaleBytecode`.
- This closes the bounded stale-hash-join runtime spill/workfile gap, but
  broader operator-runtime spill behavior still remains open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  upper-stage hash-distinct runtime spill/workfile slice.
- The executor no longer relies only on planner-side spill intent for stale
  `HASH_DISTINCT` bytecode:
  it now partitions oversized distinct rows through `sb_workfile`,
  deduplicates per partition, and records actual `HASH_AGG` spill observation
  into durable `memory_grant_feedback`.
- Focused proof after this slice is green with `3/3` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashDistinct`,
  `QueryPlannerIntegrationTest.ExecutedHashDistinctPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  and
  `QueryPlannerIntegrationTest.ExecutedHashDistinctCapturesActualSpillOnStaleBytecode`.
- This closes the bounded stale-hash-distinct runtime spill/workfile gap, but
  grouped hash aggregate and broader operator-runtime spill/workfile behavior
  still remain open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  grouped hash-aggregate runtime spill/workfile slice.
- The executor no longer relies only on planner-side spill intent for stale
  grouped `HASH_AGGREGATE` bytecode:
  it now partitions oversized aggregate state through `sb_workfile` and
  records actual `HASH_AGG` spill observation into durable
  `memory_grant_feedback`.
- Focused proof after this slice is green with `3/3` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashAggregate`,
  `QueryPlannerIntegrationTest.ExecutedHashAggregatePersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  and
  `QueryPlannerIntegrationTest.ExecutedHashAggregateCapturesActualSpillOnStaleBytecode`.
- This closes the bounded stale-hash-aggregate runtime spill/workfile gap, but
  stale top-level sort execution and wider runtime admission behavior still
  remained open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  top-level sort runtime spill/workfile slice.
- The executor no longer gates stale `SORT` bytecode spill purely on planner
  `spill_expected`:
  it now switches on runtime row pressure against the embedded plan budget,
  spills oversized runs to `sb_workfile`, merges them back in key order, and
  records actual `SORT` spill observation into durable
  `memory_grant_feedback`.
- Focused proof after this slice is green with `3/3` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForSort`,
  `QueryPlannerIntegrationTest.ExecuteBytecodeRunsSpilledSortThroughWorkfileAndPreservesOrdering`,
  and
  `QueryPlannerIntegrationTest.ExecutedSortCapturesActualSpillOnStaleBytecode`.
- This closes the bounded stale-sort runtime spill/workfile gap, but explicit
  spill-disallow cancellation accounting and broader runtime admission
  behavior still remained open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  spill-disallow cancellation-accounting slice.
- Runtime admission no longer drops rejection evidence on the floor for stale
  bytecode under live `SPILL_POLICY=DISALLOW`:
  the executor now increments durable `memory_grant_feedback.cancel_count`
  for the rejected operator family when spill-expected operators are
  disallowed or when embedded reservations exceed live `WORK_MEM`.
- Focused proof after this slice is green with `2/2` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.ExecuteBytecodeRejectsPreviouslyCompiledSpillPlanWhenLiveSpillPolicyDisallows`
  and
  `QueryPlannerIntegrationTest.ExecuteBytecodeRejectsPlanWhoseReservationExceedsLiveWorkMemUnderSpillDisallow`.
- This closes the bounded runtime-admission cancellation-accounting gap for
  phase `5`, but broader operator reservation and remaining spill/workfile
  behavior still remain open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  right-sizing and oscillation-consumption slice.
- The planner no longer treats durable `memory_grant_feedback` as a
  grow-only signal:
  it now consumes mature stable-underuse rows to shrink oversized operator
  reservations, lowers the runtime working-set annotation to the observed
  feedback packet during shrink, and still refuses to shrink when the durable
  row is marked `OSCILLATING`.
- Focused proof after this slice is green with `13/13` passing tests in
  `scratchbird_tests`, including
  `QueryPlannerIntegrationTest.StableUnderuseFeedbackShrinksSortReservationUnderSpillDisallow`
  and
  `QueryPlannerIntegrationTest.OscillatingSortFeedbackDoesNotShrinkReservation`.
- This closes the bounded planner-side right-sizing branch for phase `5`,
  but broader operator reservation and remaining spill/workfile behavior
  still remain open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  statement-reservation admission slice.
- The sort path now proves the compile-to-runtime reservation contract end to
  end:
  the baseline `PLAN_MEMORY_RESERVATION_BYTES` is rejected under a live
  `WORK_MEM` ceiling, while the same SQL recompiled after mature stable-
  underuse feedback is admitted and executes successfully under that same
  live ceiling.
- Focused proof after this slice is green with `4/4` passing tests in
  `scratchbird_tests`, including
  `QueryPlannerIntegrationTest.FeedbackShrunkSortReservationChangesRuntimeSpillDisallowAdmission`.
- This closes the bounded compile-to-runtime statement-reservation bridge for
  phase `5`, but broader operator reservation and remaining spill/workfile
  behavior still remain open.
- On `2026-04-04`, the package closed the next bounded phase-`5`
  memory-grant identity-hardening slice.
- The planner/runtime memory-grant key no longer lacks focused proof for
  identity separation:
  the sort path now proves that a generic feedback row is ignored by a custom
  parameter-sensitive compile, that rows seeded under the wrong planner
  policy snapshot, wrong execution-intent class, or wrong storage-layer shape
  are also ignored, and that the fully matching row is consumed.
- Focused proof after this slice is green with `3/3` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.MemoryGrantFeedbackDoesNotCrossPlanProfileIdentity`,
  `QueryPlannerIntegrationTest.MemoryGrantFeedbackRequiresExecutionIntentIdentity`,
  and
  `QueryPlannerIntegrationTest.MemoryGrantFeedbackRequiresPolicySnapshotAndStorageShapeIdentity`.
- This closes the bounded grant-key identity-hardening branch for phase `5`,
  but broader operator reservation and remaining spill/workfile behavior
  still remain open.
- On `2026-04-04`, the package closed phase `5`.
- The compiler/runtime memory-admission identity is now explicit and aligned:
  reusable plans publish `PLAN_GRANT_POLICY_SNAPSHOT` separately from the
  broader `PLAN_POLICY_SNAPSHOT`, publish `PLAN_MEMORY_FEEDBACK_SNAPSHOT`,
  and the executor persists runtime `memory_grant_feedback` rows under that
  same grant-policy identity instead of the broader reusable-plan policy key.
- Full bounded phase-`5` verification after rebuild is green with `36/36`
  passing tests in `scratchbird_tests` for the planner/executor filter that
  covers reusable-plan metadata, grant feedback, reservations, spill
  admission, underuse, oscillation, and spill/workfile execution.
- This closes phase `5` for package `08`; the active frontier moves to
  phase `6` planner parity and refusal-model closure.
- On `2026-04-04`, the package closed the first bounded phase-`6`
  join-runtime hot-path slice.
- The V3 join executor no longer rebuilds a full combined row for every
  candidate pair when the join predicate is a conjunction of direct
  left/right column comparisons.
  It now evaluates those residual terms in place inside the join loops and
  falls back to the general expression evaluator only when the predicate
  shape is outside that bounded direct-comparison subset.
- This directly covers the benchmark-shaped self-join residual family
  (`left.eq_key = right.eq_key AND left.id < right.id`) on the nested-loop
  path the planner currently chooses.
- Focused proof after rebuild is green with `4/4` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.HashJoinPlanExecutesAndReturnsExpectedRows`,
  `QueryPlannerIntegrationTest.NestedLoopSelfJoinResidualComparisonExecutesExpectedPairs`,
  `QueryPlannerIntegrationTest.DefaultSelfJoinResidualComparisonExecutesExpectedPairs`,
  and
  `QueryPlannerIntegrationTest.MergeJoinPlanExecutesAndPreservesRuntimeMetadata`.
- This closes the first bounded phase-`6` executor slice, but broader
  benchmark-visible query-path work still remains open.
- On `2026-04-04`, the package closed the next bounded phase-`6` hash-join
  executor slice.
- The V3 hash join executor no longer allocates `Value::toString()` keys for
  the supported single-column integer equality path in either the in-memory
  branch or the spilled workfile branch.
  It now normalizes those keys into compact structured scalar hash keys and
  falls back to the prior string path only for unsupported key shapes.
- This directly targets the benchmark-visible integer hash-join path behind
  `inner_join_large_result`.
- Focused proof after rebuild is green with `4/4` passing tests in
  `scratchbird_tests`:
  `QueryPlannerIntegrationTest.HashJoinPlanExecutesAndReturnsExpectedRows`,
  `QueryPlannerIntegrationTest.ExecutedHashJoinCapturesActualSpillOnStaleBytecode`,
  `QueryPlannerIntegrationTest.NestedLoopSelfJoinResidualComparisonExecutesExpectedPairs`,
  and
  `QueryPlannerIntegrationTest.DefaultSelfJoinResidualComparisonExecutesExpectedPairs`.
- This closes the next bounded phase-`6` executor slice, but broader
  benchmark-visible query-path work still remains open.
- On `2026-04-04`, the package closed the bounded phase-`6` projection and
  aggregate-key closeout slice.
- The V3 query path no longer drops the qualifier payload for `table.*`,
  relation-scoped projection requirements now prune live table loads through
  the join path, and grouped/distinct aggregate keys now use typed binary
  keys instead of `Value::toString()` strings.
- Focused proof after rebuild is green with `8/8` passing tests in
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
- Targeted fresh-runtime stress proof is preserved in
  `ScratchBird-Benchmarks/results/phase6-targeted-20260404T/`:
  `stress_scratchbird_normal_transactional_20260404_194239.json`
  (`inner_join_large_result = 13934.35ms`),
  `stress_scratchbird_normal_transactional_20260404_194317.json`
  (`aggregation_daily_sales = 2409.84ms`),
  and
  `stress_scratchbird_normal_transactional_20260404_194320.json`
  (`multi_dimensional_agg = 4806.16ms`).
- This closes phase `6` for package `08`; the active frontier moves to
  phase `7` full rerun and release evidence closure.
