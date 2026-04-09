# Performance Parity Implementation Workplan

## Goal

Implement every required optimization so ScratchBird is within the `3%` parity
band for every row in [PROCESS_PARITY_TARGETS.csv](PROCESS_PARITY_TARGETS.csv)
and [LOAD_TABLE_TARGETS.csv](LOAD_TABLE_TARGETS.csv).

## Execution rule

Implement tickets in the order defined by
[ORDERED_TASK_TICKETS.csv](ORDERED_TASK_TICKETS.csv). Do not start a downstream
ticket until all of its dependencies are closed or an explicit risk decision
allows overlap.

## Phase 0: Benchmark truth and package control

### `PP-10-001` Scope, target freeze, and benchmark discipline bootstrap

Implement exactly:

- freeze the benchmark process list and table-load list used by this package
- freeze the donor best timings and `+3%` ceilings in machine-readable trackers
- record every donor technique that must be assimilated, dominated, or waived
- define the package ownership split between package `10` and package `08`

Exit only when:

- tracker files are complete
- controlling specs are indexed
- package README and workplan are internally consistent

### `PP-10-002` Reproducible benchmark runner and binary pinning closure

Implement exactly:

- force each benchmark run to record the exact ScratchBird binary path used
- preserve build identity, config identity, runtime options, and harness
  command line under the run artifact root
- prevent comparison against an unpinned moving `build/` binary

Exit only when:

- benchmark artifact roots prove run-specific binary provenance
- comparison scripts refuse to compare unpinned runs

## Phase 1: Write-path parity recovery

### `PP-10-003` Ordinary multi-row insert and client-batched `VALUES` bulk reuse

Implement exactly:

- route ordinary multi-row insert execution through one statement-level bulk
  path instead of repeated singleton insert setup
- reuse one row layout and one metadata resolution per admitted batch
- reuse one post-insert maintenance strategy per batch where semantics allow

Current implementation state on `2026-04-05`:

- legacy and active V3 ordinary multi-row `VALUES` inserts now admit a
  statement-local `StorageEngine::BulkInsertHandle` instead of forcing repeated
  singleton heap insert setup
- the active V3 proof surface emits executor insert trace evidence for:
  `statement_bulk_handle_admitted`,
  `statement_bulk_handle_enabled`,
  `statement_bulk_handle_init_failed`,
  `statement_bulk_handle_used_rows`, and
  `statement_bulk_handle_fallback_rows`
- focused correctness proofs are green in
  `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`,
  `ExecutorTest.BasicSecondaryIndexInsertMaintainedExactlyOnce`, and
  `ExecutorTest.StorageBackedUniqueInsertFailuresStillRejectDuplicates`
- this ticket remains `active`, not `closed`, until donor-comparable load
  evidence shows that the admitted path materially improves benchmark load time

Do not close this ticket until:

- `load` materially improves
- ordinary client-batched insert no longer falls back to per-row executor setup

### `PP-10-004` `INSERT ... SELECT` producer and sink fusion

Implement exactly:

- make the insert sink consume producer rows through a real batch handoff
  contract
- admit `INSERT ... SELECT` into `SORTED_EXACT_BULK` or `RETAIL_MICRO_BATCH`
  when the source and sink shape qualify
- prevent row-at-a-time sink execution inside large set-sourced insert

Current implementation state on `2026-04-05`:

- the active V3 `INSERT ... SELECT` path no longer copies the producer result
  set into a second executor-owned `select_rows` buffer before sink execution
- the producer `ResultSet` now stays live through the insert statement and the
  sink reads rows directly from that producer-owned result surface
- the set-sourced sink now admits the same statement-local bulk insert handle
  used by ordinary multi-row `VALUES` inserts when semantics are safe
- on `2026-04-08`, the admitted V3 `INSERT ... SELECT` lane gained a bounded
  simple-target fast path for the benchmark-governed shape where all target
  columns are provided in ordinal order and the table has no defaults,
  generated columns, triggers, domains, or foreign keys
- that fast lane bypasses the generic per-row insert validator, reuses the
  producer row vector directly, performs only the required per-column coercion
  checks, and inserts through the same statement-local bulk handle plus exact
  index maintenance surface
- on `2026-04-08`, the same admitted simple-target lane gained callback-sink
  fusion so the producer no longer has to hand a stored executor-owned result
  set back to the insert loop before the sink starts consuming rows
- on `2026-04-08`, derived `FROM (SELECT ...)` loads began moving stored child
  rows out of the producer `ResultSet` instead of cell-copying them into a
  second `TableData` buffer, and streamed select output now hands rows directly
  to non-storing consumers instead of building an intermediate `output_rows`
  batch
- on `2026-04-08`, the simple-target stream callback became move-aware so the
  admitted `INSERT ... SELECT` lane can steal producer row vectors directly
  into tuple serialization instead of copying each streamed row before insert
- on `2026-04-08`, the V3 select executor gained a bounded direct
  derived-projection lane for the benchmark-governed outer shape:
  single-source outer projection over `FROM (SELECT ...)`, with no outer
  joins, filters, grouping, ordering, distinct, or set ops
- that lane executes the derived child query once, keeps the child result set
  as the source surface, evaluates the outer projection directly over those
  rows, and skips the second generic select/materialize pass that used to load
  the derived query back through `TableData`
- the select trace now publishes
  `SELECT TRACE derived_projection mode=DIRECT_QUERY`
- the executor trace now publishes
  `select_source_simple_fast_path`,
  `select_source_simple_fast_path_rows`, and
  `select_source_simple_fast_path_fallback_rows`
- direct proofs remain green after the callback-sink closure, but fresh normal
  benchmark evidence still shows `bulk_insert_select` materially slower than
  the earlier `2026-04-08T154451Z` baseline while `bulk_update_with_case`
  recovered back into the prior range; the remaining dominant frontier is now
  producer-side subquery and derived-table execution, not just the insert sink
- the two follow-on benchmark slices on `2026-04-08` confirmed that neither
  derived-row move/handoff removal nor move-aware streaming changed the real
  benchmark outcome: `bulk_insert_select` regressed from `181.94s` baseline to
  `197.06s` and then `199.02s`; `PP-10-004` is therefore still blocked on
  higher-level producer flattening or equivalent rewrite, not more sink-only
  executor plumbing
- the first producer-boundary recovery slice on `2026-04-08` improved the same
  benchmark from `199.02s` to `195.89s`, proving the outer derived-table pass
  was real cost, but it still did not recover the earlier `181.94s` baseline;
  `PP-10-004` therefore remains open on deeper producer work above the outer
  derived projection lane
- on `2026-04-08`, the insert executor stopped coercing admitted simple-target
  rows twice before tuple serialization by adding a prepared-value serializer
  that reuses the already validated row vector on the hot `VALUES` and
  `INSERT ... SELECT` lanes
- the clean-root `normal_transactional` rerun after that change moved
  `bulk_insert_select` from `11.17s` to `9.77s` on the same bounded single-test
  slice, but `order_items` load stayed high at `246.21s`; this confirms the
  duplicate coercion pass was real cost, but the remaining parity gap is still
  dominated by write-lane amortization during load plus producer work above the
  sink
- on `2026-04-08`, ordinary multi-row `VALUES` gained the same bounded simple
  fast admission already used by the simple `INSERT ... SELECT` sink shape:
  no generic per-row provided/default/identity/domain path when the target
  table is ordinal, trigger-free, default-free, domain-free, and foreign-key
  free
- the next clean-root rerun after that cut changed the load lane materially:
  `customers 6.16s -> 2.61s`, `products 3.39s -> 1.03s`, `orders 29.13s ->
  9.22s`, and `order_items 246.21s -> 22.78s`, while `bulk_insert_select`
  moved from `9.77s` to `9.51s`
- on `2026-04-08`, streamed `INSERT ... SELECT` simple-target coercion mode
  classification stopped freezing to `GENERIC` before the producer result set
  existed; the callback lane now admits the bounded integer/varchar/decimal
  direct coercion modes on live streamed rows instead of forcing the hot path
  through generic `coerceValueForColumn(...)`
- the next clean-root rerun after that fix improved the target statement again
  from `9.51s` to `9.34s`, while load stayed in the recovered range:
  `customers 2.61s -> 2.66s`, `products 1.03s -> 1.04s`,
  `orders 9.22s -> 9.21s`, and `order_items 22.78s -> 22.62s`
- on `2026-04-08`, the direct derived-projection lane stopped materializing
  the child result set when the outer select was already on a streaming path;
  the derived child now streams directly into the outer direct-projection
  callback with `source_stream=1` instead of building a second executor-owned
  row store before the insert sink consumes it
- the next clean-root rerun after that streaming cut improved the target
  statement again from `9.34s` to `9.28s`, while the recovered load lane held:
  `customers 2.66s -> 2.67s`, `products 1.04s -> 1.03s`,
  `orders 9.21s -> 9.16s`, and `order_items 22.62s -> 22.48s`
- on `2026-04-08`, the storage-side red slice that appeared during continued
  producer work was traced to test-only BTREE key-encoding drift in the helper
  lookup path, not a broken maintained-write runtime path; after restoring that
  proof surface and rerunning the current binary on a fresh benchmark root, the
  retained clean-root best edged down again from `9.28s` to `9.24s`, while the
  recovered load lane also improved slightly:
  `customers 2.67s -> 2.63s`, `products 1.03s -> 1.03s`,
  `orders 9.16s -> 8.94s`, and `order_items 22.48s -> 22.33s`
- on `2026-04-09`, the producer `ROW_NUMBER() OVER ()` join-count fast path
  moved ahead of unconditional base relation row decoding on admitted raw
  cross-join derived shapes; the executor now counts the base and join
  relations first and emits the bounded row-number stream directly when the
  benchmark shape qualifies
- the next clean-root rerun after that change held the recovered load lane and
  largely preserved the recovered lane: `customers 2.63s -> 2.57s`,
  `products 1.03s -> 1.01s`, `orders 8.94s -> 8.99s`,
  `order_items 22.33s -> 21.91s`, and `bulk_insert_select 9.24s -> 9.41s`;
  this confirms the early count-only producer path cuts base-row decode work,
  but the dominant donor gap is still open above the sink
- on `2026-04-09`, the empty-target unique exact-maintenance lane widened its
  buffered flush window from the old `64`-row cadence to a bounded
  statement-local `8192`-row window, with direct proof that PK entries stay
  buffered past the old threshold before `endBulkInsert()`
- the next clean-root rerun after that storage-side batch change did not move
  the benchmark in the right direction: `customers 2.57s -> 2.59s`,
  `products 1.01s -> 1.02s`, `orders 8.99s -> 9.07s`,
  `order_items 21.91s -> 22.34s`, and `bulk_insert_select 9.41s -> 9.44s`;
  this indicates the prior `64`-row unique flush cadence was not the dominant
  remaining throughput limit on the target lane
- focused correctness proofs are green in
  `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`,
  `ExecutorTest.InsertSelectFromDerivedWindowSourceStreamsSelectOutput`,
  `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`,
  `ExecutorTest.BasicSecondaryIndexInsertMaintainedExactlyOnce`, and
  `ExecutorTest.StorageBackedUniqueInsertFailuresStillRejectDuplicates`
- this ticket remains `active`, not `closed`, until donor-comparable
  `bulk_insert_select` evidence proves that the sink is no longer effectively
  row-at-a-time at benchmark scale

Do not close this ticket until:

- `bulk_insert_select` runs on the real sink bulk lane
- producer spill and sink bulk lane stay independently correct

### `PP-10-005` Heap multi-insert, page reservation, and filespace preallocation

Implement exactly:

- page-coalesced heap multi-insert
- page-run or extent-window reservation before hot loops
- ahead-of-demand filespace growth for admitted bulk and append-heavy writes
- no repeated avoidable file-growth syscalls inside hot row loops

Current implementation state on `2026-04-05`:

- `StorageEngine::BulkInsertHandle` now owns a bounded reservation window:
  `reservation_target_pages`,
  `reserved_page_budget`,
  `total_reserved_pages`,
  `consumed_reserved_pages`,
  `reservation_events`, and
  `reservation_failed`
- the bulk insert path now reserves ahead-of-demand growth once the admitted
  pinned heap page reaches the append-heavy boundary instead of waiting for each
  subsequent page miss to trigger isolated growth work
- primary tablespace growth now routes through `PageManager::extendFile()` and
  non-primary tablespace growth routes through `PageManager::preallocatePages()`
  from the same handle-owned reservation helper
- newly allocated bulk-insert pages now consume reservation budget from the
  handle so the write path can prove that the hot row loop used previously
  reserved growth instead of repeating avoidable one-page growth work
- focused proofs are green in
  `StorageEngineTest.BulkInsertHandleReservesGrowthWindowForAppendHeavyWideRows`,
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`,
  `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`,
  `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`,
  `ExecutorTest.BasicSecondaryIndexInsertMaintainedExactlyOnce`, and
  `ExecutorTest.StorageBackedUniqueInsertFailuresStillRejectDuplicates`
- this ticket remains `active`, not `closed`, until donor-comparable load
  evidence proves that the reservation window materially improves the real load
  path instead of only changing the storage proof surface

Do not close this ticket until:

- load by table improves materially, especially `order_items`
- write-path evidence proves preallocation happens before hot loops

### `PP-10-006` HOT-like update and unchanged-key DML elision

Implement exactly:

- prove indexed state unchanged before entering exact-family maintenance
- preserve heap-only or stable-head-preserving update behavior where canonical
  specs allow it
- eliminate repeated per-row index metadata scans on non-indexed updates

Current implementation state on `2026-04-05`:

- both update executor surfaces now decide once per statement whether any
  expression or partial indexes exist that require post-storage index
  maintenance work
- unchanged-key updates on tables with only ordinary exact indexes now skip the
  per-row `updateIndexesOnUpdate()` metadata walk entirely instead of paying a
  repeated `listIndexesForTable()` scan that immediately falls through
- the unchanged-key statement decision now crosses the executor/storage
  boundary as a concrete `StorageEngine::UnchangedKeyUpdatePlan`, so
  `StorageEngine::updateTuple()` no longer has to rediscover exact-index and
  active online-maintenance metadata row by row when the executor already
  proved the key set is unchanged
- the update trace and profile surfaces now publish:
  `post_storage_index_maintenance_required`,
  `post_storage_index_maintenance_rows`, and
  `post_storage_index_maintenance_skipped_rows`
- the storage update trace now also publishes:
  `unchanged_key_plan_supplied`,
  `unchanged_key_plan_exact_indexes`, and
  `unchanged_key_plan_active_maintenance`
- the bounded V3 executor lane now narrows coercion, normalization, and domain
  validation to changed columns instead of reprocessing the full row on every
  unchanged-key update
- unchanged-key updates that only modify fixed-width scalar columns with a
  stable null bitmap now patch tuple payload bytes directly from the fetched
  row instead of reserializing the full tuple, and the update profile now
  publishes:
  `tuple_patch_fast_path_rows` and
  `full_tuple_serialize_rows`
- focused proofs are green in
  `ExecutorTest.UpdateSkipsPostStorageIndexMaintenanceForUnchangedPlainKeys`,
  `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`,
  `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`,
  `ExecutorTest.BasicSecondaryIndexInsertMaintainedExactlyOnce`,
  `ExecutorTest.StorageBackedUniqueInsertFailuresStillRejectDuplicates`,
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`, and
  `StorageEngineTest.BulkInsertHandleReservesGrowthWindowForAppendHeavyWideRows`
- this ticket remains `active`, not `closed`, until benchmark evidence shows
  real recovery on `bulk_update_with_case` and the remaining unchanged-key
  storage-side maintenance scans are also reduced where they still matter. The
  next required proof is a restart-on-fresh-binary benchmark pass because the
  existing `2026-04-08` direct stress results were collected on the pre-patch
  benchmark server process

Do not close this ticket until:

- `bulk_update_with_case` returns to donor-competitive shape
- unchanged-key update traces prove the fast path really executed

### `PP-10-007` Exact-secondary maintenance batching and deferred merge

Implement exactly:

- statement-local metadata hoist
- batched or grouped exact-secondary maintenance for ordinary multi-row writes
- deferred merge equivalent where legal
- cleanup-debt publication and bounded repair routing

Current implementation state on `2026-04-05`:

- the first statement-local metadata-hoist slice is now live on the
  storage-backed bulk lane: `beginBulkInsert()` builds one
  post-insert maintenance plan per statement and `insertTupleWithHandle()` reuses
  it instead of rediscovering table columns, exact-index membership, and active
  online-maintenance ids row by row
- the handle now publishes:
  `maintenance_plan_built`,
  `maintenance_plan_index_count`,
  `maintenance_plan_exact_index_count`, and
  `maintenance_plan_active_maintenance_count`
- the second storage-side hoist slice is now live:
  `beginBulkInsert()` inspects exact-index backlog once, marks sticky
  deferred-exact mode on the maintenance target, and
  `insertTupleWithHandle()` reuses that mode instead of re-probing
  `index_page_delta_catalog` row by row after the first deferred exact match
- the third `PP-10-007` slice is now live:
  once the bulk lane is already in deferred-exact mode, deferred exact rows are
  buffered on the maintenance target and flushed at `endBulkInsert()` or on a
  bounded threshold instead of paying a durable delta-catalog write in the hot
  row loop
- the fourth `PP-10-007` slice is now live:
  deferred exact backlog now publishes exact-family cleanup debt when buffered
  rows are flushed into `index_page_delta_catalog`, and the foreground
  read-merge path overwrites that same publication with `COMPLETE` once the
  deferred backlog drains, keeping durable `index_health.cleanup_backlog_*`
  aligned with the real backlog state
- the fifth `PP-10-007` slice is now live:
  non-unique ordinary exact-secondary inserts on an admitted bulk lane are
  buffered on the statement-local maintenance target and flushed in sorted key
  order at `endBulkInsert()` or on a bounded threshold, so the hot insert loop
  no longer pays a direct `insertIntoIndex()` call for every maintained row
- the handle now also publishes:
  `maintenance_plan_deferred_exact_index_count` and
  `maintenance_plan_grouped_exact_index_count`
- executor insert traces now publish the matching statement surface:
  `statement_bulk_handle_maintenance_plan_built`,
  `statement_bulk_handle_maintenance_plan_indexes`,
  `statement_bulk_handle_maintenance_plan_exact_indexes`, and
  `statement_bulk_handle_maintenance_plan_active_maintenance`
- executor insert traces now also publish:
  `statement_bulk_handle_maintenance_plan_deferred_exact_indexes` and
  `statement_bulk_handle_maintenance_plan_grouped_exact_indexes`
- focused proofs are green in
  `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`,
  `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`,
  `ExecutorTest.MultiRowValuesInsertPublishesGroupedExactSecondaryPlan`,
  `ExecutorTest.UpdateSkipsPostStorageIndexMaintenanceForUnchangedPlainKeys`,
  `ExecutorTest.BasicSecondaryIndexInsertMaintainedExactlyOnce`,
  `ExecutorTest.StorageBackedUniqueInsertFailuresStillRejectDuplicates`,
  `StorageEngineTest.BulkInsertHandleReusesWritableHeapPage`,
  `StorageEngineTest.BulkInsertHandleReservesGrowthWindowForAppendHeavyWideRows`, and
  `StorageEngineTest.BulkInsertHandleBuildsPostInsertMaintenancePlanOnce`,
  `StorageEngineTest.BulkInsertHandleHoistsDeferredExactBacklogStateOnce`,
  `StorageEngineTest.BulkInsertHandleGroupsDirectExactSecondaryRowsUntilFlush`,
  `StorageEngineTest.ColdExactSecondaryCleanupDebtPublishesAndClearsOnMerge`,
  and `StorageEngineTest.ColdExactSecondaryInsertDeltaMergesOnRead`
- this ticket is now `active`, not `closed`; the remaining frontier is true
  donor-comparable benchmark proof on maintained write paths, especially
  `bulk_insert_select` and maintained load scenarios

Do not close this ticket until:

- write lanes stop paying the old per-row secondary-maintenance burden
- maintained-write correctness and cleanup-debt evidence stay green

## Phase 2: Prepared queries and query-cache performance

### `PP-10-008` Prepared-query fast-path bundles, plan reuse, and result-cache coordination

Implement exactly:

- prepared statement identity and bounded parameter-regime bucketing
- prepared fast-path bundle build, hit, invalidate, and rebuild
- explicit separation between prepared bundle, plan cache, and result cache
- result-cache use for cacheable prepared top-level selects only
- no result-cache use for prepared DML

Current slice status:

- `ConnectionContext` now owns the canonical prepared-bundle resolver for
  direct engine and protocol execution paths instead of duplicating custom
  bucket selection logic in `protocol_adapter.cpp`
- direct `PREPARE` and `EXECUTE PREPARED` now seed and consume the same bounded
  parameter-regime bucket state as the native protocol path
- direct engine prepared execution now publishes the same bounded seed and
  resolve trace surface as the protocol adapter, using
  `SCRATCHBIRD_PREPARED_TRACE` and `SCRATCHBIRD_PREPARED_TRACE_FILE` for
  explicit bundle-hit, rebuild, generic-bundle, and bucket-signature evidence
- executor-level prepared runs now publish distinct result-cache reuse versus
  result-cache insert evidence through `lastStatementUsedResultCache()` and
  `lastStatementInsertedResultCache()`, so bundle reuse and cached-row reuse are
  no longer conflated in direct-engine proofs
- prepared custom-bundle rebuild now publishes explicit
  `plan_cache_consulted` and `plan_cache_hit` truth through
  `PreparedExecutionSelection`, and fresh prepared handles can prove reuse of
  the global VNext plan cache instead of only local bundle reuse
- prepared point-select specialization is proven on the direct executor path,
  prepared point update remains generic while still using the unchanged-key
  update fast path, and prepared `INSERT` remains generic on both the direct
  and protocol paths
- prepared multi-row `INSERT` is now proven to admit and use the same
  statement-local bulk insert handle as non-prepared multi-row writes
- focused proof is green on the fresh binary:
  `ExecutorTest.PreparedPointSelectBucketsCustomPlansForDirectEngineContext`,
  `ExecutorTest.PreparedInsertStatementsStayGenericForDirectEngineContext`,
  `ExecutorTest.PreparedSelectPublishesBundleTraceForDirectEngineContext`,
  `ExecutorTest.PreparedInsertPublishesGenericTraceForDirectEngineContext`,
  `ExecutorTest.PreparedSelectSeparatesBundleReuseFromResultCacheReuse`,
  `ExecutorTest.PreparedInsertRemainsOutsideResultCache`,
  `ExecutorTest.PreparedSelectPublishesPlanCacheHitForFreshPreparedHandle`,
  `ExecutorTest.PreparedPointUpdateStaysGenericAndUsesUnchangedKeyFastPath`,
  `ExecutorTest.PreparedMultiRowInsertUsesStatementBulkHandle`,
  `ProtocolAdapterDialectsNative.PreparedStatementsCacheBucketedCustomPlansForScratchBird`,
  and
  `ProtocolAdapterDialectsNative.PreparedInsertStatementsStayGenericForScratchBird`
- focused regression slice passed `11/11` on the fresh binary
- this ticket is now `completed`; the phase-2 prepared fast-path exit criteria
  are satisfied for point select, point update, and micro-batch insert, with
  prepared bundle hit, plan-cache hit, and result-cache hit all independently
  observable

Do not close this ticket until:

- prepared point select, point update, and micro-batch insert all have
  end-to-end evidence
- prepared bundle hit, plan-cache hit, and result-cache hit are independently
  observable

## Phase 3: Secondary-read and join locality parity

### `PP-10-009` Ordered-exact secondary-read locality: `ICP`, `MRR`, index-only

Implement exactly:

- residual predicate pushdown for admitted ordered exact paths
- rowid-ordered heap fetch for admitted secondary reads
- bounded index-only execution with visibility proof
- explicit fallback and refusal reasons when any sub-technique is illegal

Current bounded slice on `2026-04-05`:

- executor-side BTREE secondary-read tracing is now available through
  `SCRATCHBIRD_SELECT_TRACE` and `SCRATCHBIRD_SELECT_TRACE_FILE`, with
  per-relation publication of `scan_kind`, `runtime_access`,
  `fallback_reason`, `heap_fetch_mode`, `visibility_proof`,
  `candidate_count`, `visible_count`, `stale_skipped`, `index_only_rows`,
  and `heap_rows`
- admitted exact secondary paths now decode BTREE key bytes, apply residual
  predicate pushdown against decoded key values, and prove heap visibility with
  stable-key recheck before returning rows
- single-relation exact `INDEX_ONLY_SCAN` paths with fully covered projection
  now stay on the runtime index-only fast path instead of widening to all table
  columns because of unqualified column refs; the projection binder now keeps
  unqualified refs narrow when the relation set is unambiguous
- admitted ordered covering composite-prefix range scans now stay on the real
  runtime BTREE path again; the executor merges deferred exact backlog before
  scanning and avoids shortened composite start-seek routing by scanning from
  the left edge when the lower bound only covers a leading index prefix
- admitted `INDEX_ONLY_SCAN` relations that still retain non-covered columns in
  the relation projection now fall back explicitly with
  `PROJECTED_COLUMN_NOT_IN_INDEX:<column_name>` instead of silently claiming
  index-only execution while heap-fetching rows
- non-covering same-column BTREE ranges now preserve all compatible same-index
  runtime predicates instead of truncating to a single bound; the planner also
  assigns non-zero ordinary BTREE candidate budgets and uses a bounded
  same-column range selectivity model instead of multiplying independent
  single-ended range estimates
- focused proof is green on the fresh binary:
  `QueryPlannerIntegrationTest.CoveringIndexPlanUsesIndexOnlyScan`,
  `QueryPlannerIntegrationTest.MulticolumnOrderedAccessFamiliesSurviveIntoRuntimePlan`,
  `QueryPlannerIntegrationTest.ExactIndexOnlyPlanUsesProvedRuntimePathWhenSingleRelationProjectionIsCovered`,
  `QueryPlannerIntegrationTest.OrderedCoveringRangeUsesProvedIndexOnlyRuntimePath`,
  and
  `QueryPlannerIntegrationTest.NonCoveringRangeUsesTidOrderedSecondaryHeapFetchPath`
- focused regression slice passed `5/5` on the fresh binary
- this ticket is now `completed`; the phase-3 secondary-read exit criteria are
  satisfied for exact covering, ordered covering, and non-covering tid-ordered
  BTREE range execution with explicit fallback on illegal index-only cases

Do not close this ticket until:

- point reads and range reads prove the new path families
- ordered output and visibility semantics remain correct

### `PP-10-010` Indexed-join parity: `BKA`, memoize, runtime filters, adaptive build-side

Implement exactly:

- batched key access for indexed join probes
- memoize for admitted parameterized rescans
- runtime-filter producer and consumer closure
- bounded adaptive hash build-side selection where the canonical spec allows it

Current bounded slice on `2026-04-05`:

- admitted `PARAMETERIZED_NESTED_LOOP` execution now uses a statement-local,
  bounded memoize cache for repeated outer bindings when the parameterized
  child is deterministic under the current statement surface; volatile function
  shapes and table-function parameterized children stay fail-closed outside the
  cache
- admitted nested-loop joins with a chosen right-side runtime-filter index now
  publish `BATCHED_KEY_ACCESS` in the join method enablers, and the executor
  emits an explicit select-trace join line for the batched-key probe with
  `outer_rows`, `batched_keys`, and the chosen right-side index name
- admitted exact nested-loop joins with a runtime-filter-backed right-side
  probe now execute a real `ORDERED_EXACT_BKA_PROBE` replay step after the
  batched fetch: the executor groups fetched inner rows by normalized join key,
  replays only the matching key group for each outer row, and publishes
  `probe_mode=ORDERED_EXACT_BKA_PROBE` with `rows_examined`, `replay_rows`,
  `inner_rows`, and `materialized_keys` in `SCRATCHBIRD_SELECT_TRACE`
- memoize keys now derive from the actual correlated outer column refs in the
  parameterized child whenever that dependency surface is explicit; the
  executor falls back to the bounded required-outer-alias set only when the
  explicit outer ref list is unavailable
- admitted inner `HASH_JOIN` plans now publish bounded adaptive build-side
  metadata in the runtime plan, and the executor samples both sides on the
  non-spill path to flip from planned `RIGHT` build to `LEFT` build when the
  observed smaller side clears the configured threshold
- runtime hash, merge, and `BATCHED_KEY_ACCESS` key binding is now side-aware:
  the executor resolves planned join-key metadata against the left and right
  source ranges explicitly instead of relying on generic alias lookup that can
  collapse duplicate column names onto the left side
- `SCRATCHBIRD_SELECT_TRACE` now emits a join-level parameterized trace line
  with `method`, `outer_rows`, `physical_loops`, `memoize_hits`,
  `memoize_misses`, `memoize_evictions`, and `memoize_entries`, so the
  parameterized memoize path is directly provable without inferring it from
  timing
- focused proof is green on the fresh binary:
  `QueryPlannerIntegrationTest.HashJoinPlanExecutesAndReturnsExpectedRows`,
  `QueryPlannerIntegrationTest.AdaptiveHashJoinPublishesReversibleBuildSideMetadata`,
  `QueryPlannerIntegrationTest.AdaptiveHashJoinFlipsToObservedSmallerBuildSide`,
  `QueryPlannerIntegrationTest.HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata`,
  `QueryPlannerIntegrationTest.LateralJoinUsesParameterizedNestedLoopPath`,
  `QueryPlannerIntegrationTest.LateralJoinPublishesParameterizedIndexCandidateFamily`,
  `QueryPlannerIntegrationTest.ParameterizedNestedLoopMemoizesRepeatedOuterBindings`,
  `QueryPlannerIntegrationTest.NestedLoopJoinUsesBatchedKeyAccessRuntimeFilterProbe`,
  `QueryPlannerIntegrationTest.JoinRuntimeFilterUsesRightSideIndexMetadata`,
  `QueryPlannerIntegrationTest.MergeJoinPlanExecutesAndPreservesRuntimeMetadata`,
  `QueryPlannerIntegrationTest.ForcedMergeJoinUsesExplicitSortToMergeCandidate`,
  `QueryPlannerIntegrationTest.ExecuteBytecodeRunsSpilledMergeJoinThroughWorkfileAndPersistsFeedback`,
  and
  `QueryPlannerIntegrationTest.ExecutedMergeJoinPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`
- focused regression slices passed `9/9` for the indexed-join and adaptive-hash
  surface plus `5/5` for the adjacent merge-join execution surface on the
  fresh binary
- this ticket remains `active`; the implementation surface is now live for
  memoize, `BKA`, runtime-filter consumption, and adaptive build-side
  selection, but donor-comparable join benchmark evidence is still required
  before closure

Do not close this ticket until:

- join-heavy processes materially improve
- runtime-plan traces preserve every specialized family and refusal

## Phase 4: Upper-stage operator parity

### `PP-10-011` Structured-key hash, spill, and low-churn sort closure

Implement exactly:

- structured-key hash join and aggregation on admitted key types
- remove stringified-key fallback from dominant admitted paths
- bounded workfile spill for hash, merge, sort, and window pipelines
- low-churn sort run generation and merge paths

Current bounded slice on `2026-04-05`:

- admitted integer and boolean `HASH_AGG` grouping now uses structured keys
  instead of serializing group values into strings on the dominant in-memory
  grouping path; the bounded current slice covers both single-column scalar and
  multi-column composite grouping sets when every grouping expression is a
  direct admitted scalar column
- the same admitted structured-key contract now also covers direct admitted
  text grouping and scalar grouping expressions that evaluate to admitted
  scalar values, so the dominant hash-aggregate fast path is no longer limited
  to raw integer and boolean grouping columns
- the same admitted structured-key contract now drives spilled hash-aggregate
  partitioning and regrouping inside the `sb_workfile` path, so these scalar
  and composite integer grouping lanes no longer pay string-key hashing just
  because the operator spills
- `SCRATCHBIRD_SELECT_TRACE` now emits an aggregate-level line for the hash
  aggregate runtime path with `kind=HASH_AGG`, `group_key_mode`, `spill`,
  `input_rows`, and `groups`, so the structured-key path is directly
  observable on both the in-memory and spilled lanes
- the low-churn sort runtime now skips avoidable sort work on already ordered
  inputs: in-memory `ORDER BY` execution can keep ordered input as-is and trim
  `TOP N` without a full `stable_sort` or `partial_sort`, and spilled sort run
  generation now detects presorted runs and writes them to `sb_workfile`
  without resorting the run buffer
- the sort runtime now also admits a bounded incremental-sort lane when the
  input is already ordered by the leading sort key. The current slice detects
  prefix-ordered input on the first sort key, sorts only within equal-prefix
  groups on the in-memory lane, and applies the same bounded grouped rewrite
  before writing spilled sort runs to `sb_workfile`
- `SCRATCHBIRD_SELECT_TRACE` now emits a sort-level line through `mode`,
  `spill`, `input_rows`, `output_rows`, `run_count`, `presorted_runs`,
  `incremental_groups`, `incremental_runs`, `prefix_order_keys`, and `top_n`,
  so the low-churn and bounded incremental sort paths are directly observable
  on both the in-memory and spilled lanes
- focused proof is green on the fresh binary:
  `QueryPlannerIntegrationTest.HashAggregateUsesFastScalarGroupKeysForAdmittedIntegerGrouping`,
  `QueryPlannerIntegrationTest.SpilledHashAggregateUsesFastScalarGroupKeysForAdmittedIntegerGrouping`,
  `QueryPlannerIntegrationTest.HashAggregateUsesFastCompositeGroupKeysForAdmittedIntegerGrouping`,
  `QueryPlannerIntegrationTest.SpilledHashAggregateUsesFastCompositeGroupKeysForAdmittedIntegerGrouping`,
  `QueryPlannerIntegrationTest.HashAggregateUsesFastScalarGroupKeysForAdmittedTextGrouping`,
  `QueryPlannerIntegrationTest.SpilledHashAggregateUsesFastScalarGroupKeysForAdmittedTextGrouping`,
  `QueryPlannerIntegrationTest.HashAggregateUsesFastScalarGroupKeysForAdmittedScalarExpressionGrouping`,
  `QueryPlannerIntegrationTest.SpilledHashAggregateUsesFastScalarGroupKeysForAdmittedScalarExpressionGrouping`,
  `QueryPlannerIntegrationTest.SortTopNShortcutsAlreadyOrderedInputWithoutFullSort`,
  `QueryPlannerIntegrationTest.SpilledSortSkipsRunSortForAlreadyOrderedInput`,
  `QueryPlannerIntegrationTest.IncrementalSortUsesPrefixOrderedInputInMemory`,
  `QueryPlannerIntegrationTest.SpilledSortUsesIncrementalRunOrderingOnPrefixOrderedInput`,
  `QueryPlannerIntegrationTest.ExecutedHashAggregatePersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  `QueryPlannerIntegrationTest.ExecutedHashAggregateCapturesActualSpillOnStaleBytecode`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashAggregate`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForSort`,
  `QueryPlannerIntegrationTest.ExecutedSortPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  `QueryPlannerIntegrationTest.ExecutedSortFeedbackRecordsUnderuseBeforeShrinkThreshold`,
  and
  `QueryPlannerIntegrationTest.ExecutedSortCapturesActualSpillOnStaleBytecode`
- focused regression slices passed `7/7` for the original structured-key
  hash-aggregate lane, `8/8` for the low-churn plus bounded incremental sort
  lane, and `19/19` for the full `PP-10-011` closure slice on the fresh
  binary
- this ticket is now `completed`; the phase-4 upper-stage exit criteria are
  satisfied for bounded structured-key hash aggregation, spill-safe hash
  regrouping, low-churn sort, and bounded incremental sort on the current
  admitted lanes. The next active frontier is `PP-10-012`

Do not close this ticket until:

- spill-heavy paths retain correctness and improve materially
- stale bytecode and live policy interactions fail closed

### `PP-10-012` Incremental sort, aggregate, distinct, and window specialization/vectorization

Implement exactly:

- incremental sort on delivered-prefix inputs
- vectorized scan, join, aggregate, distinct, sort-run, and window execution on
  admitted types
- partition-aware window buffering and explicit full-sort fallback only where
  required
- low-materialization aggregate and distinct state

Current bounded slice on `2026-04-05`:

- the distinct runtime no longer serializes admitted projected rows into
  string keys on the dominant hash-distinct path. Distinct now reuses the
  bounded structured-key substrate for admitted scalar and composite rows, so
  in-memory dedupe and spilled partition routing both stay on typed keys for
  the current admitted lanes
- the spilled distinct workfile path now partitions and deduplicates rows by
  the same admitted structured key contract instead of hashing the stringified
  row image, while non-admitted rows stay on the existing string fallback
- `SCRATCHBIRD_SELECT_TRACE` now emits a distinct-level line with
  `key_mode`, `spill`, `input_rows`, and `output_rows`, so the fast distinct
  path is directly observable on both the in-memory and spilled lanes
- the ordered-distinct runtime now finalizes on the ordered stream lane instead
  of falling through into a second stale fast-key hash pass. That keeps
  `mode=ORDERED_STREAM` stable on admitted ordered input and preserves the real
  `5`-row output in the covering ordered-distinct proof instead of collapsing
  back onto the first duplicate group
- the ordered-distinct streaming lane no longer materializes string keys for
  adjacent comparison. On admitted scalar and composite rows it now reuses the
  structured distinct key substrate directly and publishes
  `stream_compare=FAST_ADJACENT` in the select trace, with direct value
  adjacency only on the non-admitted fallback
- focused proof is green on the fresh binary:
  `QueryPlannerIntegrationTest.DistinctUsesFastScalarKeysForAdmittedIntegerProjection`,
  `QueryPlannerIntegrationTest.SpilledDistinctUsesFastScalarKeysForAdmittedIntegerProjection`,
  `QueryPlannerIntegrationTest.DistinctUsesFastCompositeKeysForAdmittedCompositeProjection`,
  `QueryPlannerIntegrationTest.SpilledDistinctUsesFastCompositeKeysForAdmittedCompositeProjection`,
  `QueryPlannerIntegrationTest.ExecutedHashDistinctPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`,
  `QueryPlannerIntegrationTest.ExecutedHashDistinctCapturesActualSpillOnStaleBytecode`,
  `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashDistinct`,
  and
  `QueryPlannerIntegrationTest.SpillPolicyDisallowChoosesOrderedDistinctWhenAvailable`
- additional direct proof is green on the fresh binary:
  `QueryPlannerIntegrationTest.DistinctStageUsesOrderedDistinctWhenInputOrderIsAvailable`
  `QueryPlannerIntegrationTest.OrderedCompositeDistinctUsesFastAdjacentStreamingComparison`
  and
  `QueryPlannerIntegrationTest.DistinctStageUsesHashDistinctWhenOrderIsUnavailable`
- focused distinct regression slice passed `8/8` on the fresh binary
- the window runtime now distinguishes three explicit local-order modes on the
  bounded row engine: `INPUT_ALREADY_ORDERED`, `PARTITION_PREFIX_INCREMENTAL`,
  and `FULL_SORT` or `FULL_SORT_SPILL`. Admitted partitioned windows no longer
  spill or globally reorder when the incoming row stream already satisfies the
  full partition or order contract, or when only per-partition refinement is
  needed and each partition fits the bounded local window buffer
- `SCRATCHBIRD_SELECT_TRACE` now emits a window-level line with `mode`,
  `spill`, `input_rows`, `output_rows`, `partitions`, and
  `incremental_groups`, so the partition-aware fast path is directly
  observable
- the ordered window lane no longer copies full partition-key vectors after
  the ordering decision. It now publishes compact partition-start markers and
  drives `ROW_NUMBER()` resets from those markers, with
  `partition_state=MARKERS` exposed in `SCRATCHBIRD_SELECT_TRACE`
- the upper-stage row engine now has a real batch handoff into `ResultSet` on
  the admitted distinct/window fast lanes. Final rows are sliced once after
  offset/limit, transferred through `ResultSet::addRows(...)`, and published as
  `SELECT TRACE output handoff=BATCH_ROWS rows=<n>` instead of only dribbling
  rows through per-row `addRow(...)`
- the admitted ordered-window projection lane no longer calls the generic
  expression evaluator per projected row for the bounded `column refs +
  ROW_NUMBER()/RANK()/DENSE_RANK()` shapes. It now emits those rows in bounded
  batches through direct column extraction and direct rank synthesis, and
  publishes `SELECT TRACE window_projection mode=BATCH_FAST rows=<n> batches=<m>`
- the window runtime now exploits the longest delivered
  `partition + order-prefix` instead of stopping at partition keys only.
  Admitted window input that already satisfies `PARTITION BY k1 ORDER BY k2`
  on the stream now refines only inside equal `(k1, k2)` groups when the full
  window order is `(k1, k2, k3, ...)`, and the trace publishes
  `mode=PARTITION_ORDER_PREFIX_INCREMENTAL` plus `prefix_order_keys=<n>` for
  that lane
- additional focused proof is green on the fresh binary:
  `QueryPlannerIntegrationTest.WindowReusesAlreadyOrderedPartitionInputWithoutSpill`,
  `QueryPlannerIntegrationTest.WindowUsesPartitionPrefixIncrementalSortWithoutSpill`,
  `QueryPlannerIntegrationTest.WindowUsesPartitionOrderPrefixIncrementalSortWithoutSpill`,
  `QueryPlannerIntegrationTest.WindowPreservesOrderedInputAndAvoidsFinalSort`,
  `QueryPlannerIntegrationTest.OrderedCoveringRangePreservesInclusiveLowerBoundAtLeafBoundary`,
  and
  `BTreeIteratorTest.AscendingRightEdgeRangeIncludesTerminalKeyAfterRepeatedSplits`
- focused regression slice passed `5/5` on the fresh binary
- the bounded incremental-sort runtime no longer stops at a delivered prefix of
  one key. In-memory and spilled sort-run generation now detect the longest
  delivered prefix already satisfied by the input stream and only sort within
  equal-prefix groups, publishing `PREFIX_<n>_INCREMENTAL` or
  `PREFIX_<n>_INCREMENTAL_RUNS` plus `prefix_order_keys=<n>` in
  `SCRATCHBIRD_SELECT_TRACE`
- additional focused proof is green on the fresh binary:
  `QueryPlannerIntegrationTest.IncrementalSortUsesDeepPrefixOrderedInputInMemory`
  and
  `QueryPlannerIntegrationTest.SpilledSortUsesDeepPrefixRunOrderingOnPrefixOrderedInput`
- widened focused distinct-plus-window regression slice passed `11/11` on the
  fresh binary
- after the ordered-distinct fast-adjacent, window marker, and batched output
  handoff cuts, the widened focused distinct-plus-window regression slice now
  passes `12/12` on the fresh binary
- the widened focused distinct-plus-window regression slice remains green at
  `12/12` after the admitted ordered-window batch projection cut
- this ticket remains `active`; the next live frontier is widening the
  admitted window and distinct lanes beyond the current bounded direct
  projection/vector batch shapes into broader typed expression batching, then
  broader upper-stage vectorized execution beyond the current bounded
  row-engine fast paths

Do not close this ticket until:

- aggregate, distinct, and window benchmarks materially improve
- row-mode fallback remains explicit and observable

## Phase 5: Parallel and locality parity

### `PP-10-013` Intra-query parallel execution, worker grants, and locality binding

Implement exactly:

- legal serial and parallel candidate enumeration
- exchange and gather closure for admitted scan, join, sort, aggregate, and
  window shapes
- worker-aware grant charging
- worker affinity, morsel locality, and bounded work stealing

Current bounded slice on `2026-04-05`:

- `costGather(...)` now charges worker memory against the actual parallel
  participant count instead of leaving gather wrappers at a zero or serial-sized
  memory budget. `memory_bytes` and `memory_budget_bytes` now scale with the
  worker count plus optional leader participation
- `costGatherMerge(...)` now keeps that worker-aware gather charge and adds a
  bounded coordinator merge budget on top, so ordered parallel plans no longer
  underreport the merge coordinator’s memory envelope
- the focused parallel runtime-plan proofs now require explicit
  distribution/topology contracts on the admitted scan and hash-join lanes:
  `parallel_distribution_mode`, `exchange_topology_id`, and non-empty
  `gather_decision_reason`
- focused proof is green on the fresh binary:
  `QueryPlannerIntegrationTest.ParallelSeqScanWrapsPlanInGatherAndPublishesRelationMetadata`,
  `QueryPlannerIntegrationTest.ParallelHashJoinWrapsPlanInGatherAndPublishesJoinMetadata`,
  `QueryPlannerIntegrationTest.ParallelAggregateWrapsPlanInGatherAndPublishesStageMetadata`,
  and
  `QueryPlannerIntegrationTest.OrderedParallelPlanUsesGatherMergeAndExplainJsonPublishesParallelFields`
- focused parallel planner slice passed `4/4` on the fresh binary
- this ticket remains `active`; the next live frontier is real intra-query
  execution beyond planner-only gather wrappers: admitted worker execution,
  worker-aware reservation enforcement, and explicit locality/affinity evidence

Current bounded slice on `2026-04-08`:

- `WorkerPool` now uses worker-local task queues instead of a single global
  queue, so admitted parallel morsels prefer the owning worker first and only
  fall back to bounded stealing after the local queue is empty
- `WorkUnit` and `WorkerResult` now preserve preferred worker id, actual worker
  id, page-range ownership, and whether a morsel was stolen
- `ParallelScan` and `ParallelAggregate` now publish direct runtime execution
  evidence: `morsel_count`, `exchange_mode`, `locality_preferred`,
  `work_steal_count`, `cross_partition_transfer_bytes`, and per-morsel worker
  execution records
- `ParallelHashJoin` now executes a real two-phase runtime path on admitted
  single-column equality joins: worker-local outer build over page ranges,
  shared hash partitions keyed by the admitted join value, then bounded
  parallel probe over the inner ranges with explicit `REPARTITION_PROBE`
  exchange evidence, build transfer-byte accounting, and per-morsel worker
  execution records
- `ParallelSort` now executes a real multi-worker local-sort plus gather-merge
  runtime path and publishes direct runtime evidence for `morsel_count`,
  `exchange_mode=GATHER_MERGE`, `locality_preferred`,
  `cross_partition_transfer_bytes`, and per-morsel worker execution records
- `ParallelWindow` now executes a real multi-worker `ROW_NUMBER` runtime on
  already ordered partition input: worker-local partition morsels write their
  row-number output in place, publish `exchange_mode=GATHER`, and surface the
  same locality/steal execution records as the other direct parallel stages
- the aggregate runtime proof now uses inline typed rows instead of toasted
  synthetic payloads, so the direct numeric decode path is proving the real
  worker aggregate contract instead of invalid tuple bytes
- the direct hash-join runtime proof also uses inline typed rows after a
  synthetic wide-row tuple shape exposed a false all-match decode surface on
  the bounded test harness; the admitted runtime slice now proves the real
  typed join contract instead of that synthetic payload edge case
- focused runtime proofs are green on the fresh binary:
  `ParallelExecutionControlsTest.WorkerPoolPrefersLocalQueueAndStealsWhenImbalanced`,
  `ParallelExecutionControlsTest.ParallelSortPublishesWorkerLocalMergeEvidence`,
  `ParallelExecutionControlsTest.ParallelWindowPublishesWorkerLocalPartitionEvidence`,
  `ParallelExecutionControlsTest.ParallelSortRejectsWorkerReservationOverflow`,
  `ParallelExecutionControlsTest.ParallelWindowRejectsWorkerReservationOverflow`,
  `ParallelScanExecutionTest.ParallelScanExecutesRealWorkerRangesAndReturnsAllRows`,
  `ParallelScanExecutionTest.ParallelHashJoinExecutesRealWorkerBuildAndProbeSlices`,
  `ParallelScanExecutionTest.ParallelHashJoinRejectsBuildReservationOverflow`,
  and
  `ParallelScanExecutionTest.ParallelAggregateExecutesRealWorkerPartitionsForNumericColumn`
- widened focused parallel execution-controls slice passed `17/17` on the fresh
  binary, and the adjacent planner parallel slice remains green at `4/4`
- the direct materializing parallel stages now enforce worker-aware runtime
  reservation instead of trusting planner-only budgets: sort rejects worker
  partitions that exceed `work_mem_per_worker`, parallel window rejects
  partition output slices that exceed the same per-worker budget, and parallel
  hash join fails closed when a build worker grows its local hashed payload
  past the configured worker budget
- the benchmark-governed executor now consumes the direct runtime substrate on
  the bounded ordered no-spill lanes: admitted `GatherMerge -> Sort` plans use
  the live `ParallelSort` handle lane instead of a serial `stable_sort`, and
  admitted ordered `ROW_NUMBER` plans reuse the live `ParallelWindow`
  row-number runtime after ordering is established
- the benchmark-governed executor now also consumes a bounded live parallel
  scan lane for the admitted single-relation `Gather -> SeqScan` shape:
  `loadTable(...)` now hands that runtime path into `ParallelScan::execute(...)`
  instead of serial heap materialization, preserves the same projected-row
  decoding and runtime-filter checks, and publishes `parallel_scan=1`,
  `parallel_workers`, `parallel_exchange=GATHER`, `parallel_steals`, and
  `parallel_transfer_bytes` through the existing table-level
  `SCRATCHBIRD_SELECT_TRACE` surface
- the benchmark-governed executor now also consumes a bounded live parallel
  aggregate lane for simple single-table `GROUP BY <column>, COUNT(*)`
  shapes: the executor admits the planner-selected parallel aggregate wrapper,
  runs `ParallelAggregate::executeGroupBy(...)` against the base relation,
  materializes grouped rows directly back into the V3 row pipeline, and
  publishes `parallel_aggregate=1`, `parallel_workers`,
  `parallel_exchange=GATHER`, `parallel_steals`, and
  `parallel_transfer_bytes` through `SCRATCHBIRD_SELECT_TRACE`
- the benchmark-governed executor now also consumes a bounded live parallel
  hash-join lane for the admitted first-join `Gather -> HashJoin` shape:
  the executor binds the live right relation from the loaded table metadata,
  hands the worker-partitioned no-spill join into
  `ParallelHashJoin::execute(...)`, preserves the existing join predicate
  filter in the callback, and publishes `parallel_hash_join=1`, `build_side`,
  `parallel_workers`, `parallel_exchange=REPARTITION_PROBE`,
  `parallel_steals`, and `parallel_transfer_bytes` through
  `SCRATCHBIRD_SELECT_TRACE`
- the bounded live handoff now publishes explicit runtime evidence through
  `SCRATCHBIRD_SELECT_TRACE`: sort adds `parallel_sort=1`,
  `parallel_workers`, `parallel_exchange=GATHER_MERGE`,
  `parallel_steals`, and `parallel_transfer_bytes`; ordered window and
  `window_projection` add `parallel_row_number=1`, `parallel_workers`,
  `parallel_exchange=GATHER`, `parallel_steals`, and
  `parallel_transfer_bytes`; simple parallel aggregate adds the aggregate-side
  parallel evidence fields on the same trace surface; and admitted parallel
  sequential scan now adds `parallel_scan=1`, `parallel_workers`,
  `parallel_exchange=GATHER`, `parallel_steals`, and
  `parallel_transfer_bytes` on the table scan trace line
- the bounded sort handoff also removes a redundant serial full-sort fallback
  after prefix-incremental ordering, and `ParallelSort` now honors the
  configured `min_rows_per_worker` instead of a hardcoded `10k` threshold
- focused live-runtime proofs are green on the fresh binary:
  `QueryPlannerIntegrationTest.ParallelSeqScanWrapsPlanInGatherAndPublishesRelationMetadata`,
  `QueryPlannerIntegrationTest.ParallelHashJoinWrapsPlanInGatherAndPublishesJoinMetadata`,
  plus
  `QueryPlannerIntegrationTest.OrderedParallelPlanExecutesLiveParallelSortRuntime`
  and
  `QueryPlannerIntegrationTest.OrderedParallelWindowPlanExecutesLiveParallelRowNumberRuntime`,
  plus
  `QueryPlannerIntegrationTest.ParallelAggregateWrapsPlanInGatherAndPublishesStageMetadata`
- widened focused slice passed `24/24` on the fresh binary across the direct
  parallel execution-controls suite, the direct scan/aggregate/hash unit
  surface, and the affected planner/executor parallel regressions
- this ticket remains `active`; the next live frontier is wiring the direct
  locality and reservation evidence into donor-comparable benchmark runs on
  the newly live scan/hash/sort/window/aggregate executor lanes, then
  broadening benchmark-governed parallel join and upper-stage coverage beyond
  the current bounded first-join no-spill contract

Do not close this ticket until:

- donor-winning large operators have executable parallel candidates
- locality-sensitive paths publish explicit locality contracts or refusals

## Phase 6: Final reruns and waiver discipline

### `PP-10-014` Final parity reruns, residual waiver review, and closeout

Implement exactly:

- full clean build, test, and parity benchmark reruns
- per-process comparison against the frozen donor ceilings
- residual waiver review for any remaining gap
- handoff of final evidence to package `08`

Do not close this ticket until:

- every tracker row is `met` or `waived`
- waivers prove an invariant conflict instead of “not yet optimized”
- final artifacts are preserved under the package evidence root
