# Risk and Decision Log

## 2026-04-05: Package `10` is the parity execution owner

- Decision:
  - package `10` owns donor-parity implementation closure
  - package `08` remains the downstream release-evidence consumer
- Reason:
  - package `08` already carries gate and release burden; parity execution needs
    its own deeper tracker set

## 2026-04-05: `3%` ceiling is strict and machine-tracked

- Decision:
  - every process and load-table row carries a numeric ceiling in CSV trackers
- Reason:
  - qualitative “near donor” language is too loose for closeout

## 2026-04-05: donor `no_transaction` is stretch input only

- Decision:
  - donor `no_transaction` rows are recorded as stretch references, not the
    blocking parity ceiling
- Reason:
  - ScratchBird does not yet expose an equivalent reduced-guarantee class

## 2026-04-05: benchmark provenance is release-blocking for parity claims

- Decision:
  - a run without pinned binary identity is not valid parity evidence
- Reason:
  - the audit found prior provenance weakness around moving build-tree binaries

## 2026-04-05: waivers require invariant proof

- Decision:
  - “not yet implemented” is not a valid waiver
- Reason:
  - donor assimilation is mandatory unless blocked by MGA, UUID, or
    parser-boundary invariants

## 2026-04-05: `PP-10-003` correctness proof is not enough to close the ticket

- Decision:
  - the first multi-row `VALUES` bulk-handle slice may move tracker state to
    `active`, but not `closed`, until a donor-comparable load rerun proves
    material improvement
- Reason:
  - package `10` requires performance closure, not only executor correctness;
    focused proof without benchmark recovery is insufficient

## 2026-04-05: `PP-10-004` direct producer-to-sink handoff is still benchmark-pending

- Decision:
  - removing the extra `INSERT ... SELECT` result copy and admitting the
    set-sourced sink to the statement bulk handle is enough to mark the ticket
    `active`, but not `closed`
- Reason:
  - the package exit rule requires preserved `bulk_insert_select` benchmark
    evidence, not just executor proof that the sink path improved structurally

## 2026-04-08: `PP-10-004` remains blocked above the sink boundary

- Decision:
  - keep `PP-10-004` open after the derived-row move, streamed output handoff,
    and move-aware callback slices; do not spend more ticket time on sink-only
    plumbing until the producer boundary is reworked
- Reason:
  - fresh normal benchmark evidence on `2026-04-08` moved
    `bulk_insert_select` from `181.94s` baseline to `197.06s` and then
    `199.02s`, so the remaining cost is not explained by the executor-owned
    result copy, the final `output_rows` batch, or the streamed-row copy into
    the insert sink
- Evidence:
  - `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T154451Z/stress_scratchbird_normal_transactional_20260408_115250.json`
  - `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T165623Z/stress_scratchbird_normal_transactional_20260408_130517.json`
  - `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T171252Z/stress_scratchbird_normal_transactional_20260408_132149.json`

## 2026-04-05: `PP-10-005` reservation windows count only when proven ahead of the hot loop

- Decision:
  - handle-owned heap reservation and filespace preallocation may move
    `PP-10-005` to `active`, but not `closed`, until benchmark evidence proves
    that append-heavy load no longer pays repeated avoidable growth work inside
    the hot row loop
- Reason:
  - storage-level reservation counters and focused proofs show the mechanism is
    real, but package `10` closes only on donor-comparable load recovery,
    especially for `order_items`

## 2026-04-05: `PP-10-006` is active after the executor skip gate, not complete

- Decision:
  - statement-level skipping of post-storage index maintenance for unchanged-key
    updates on plain exact-index tables is enough to move `PP-10-006` to
    `active`, but not `closed`
- Reason:
  - this closes one concrete per-row metadata loop and is proven in update
    trace, but the package still requires benchmark recovery on
    `bulk_update_with_case` and any remaining hot storage-side unchanged-key
    scans still need to be reduced if they remain measurable

## 2026-04-05: `PP-10-006` keeps `active` status after the storage handoff slice

- Decision:
  - the executor-proved unchanged-key update plan may be passed into
    `StorageEngine::updateTuple()` and counted as another `PP-10-006` recovery
    slice, but the ticket remains `active`
- Reason:
  - the storage path no longer needs to rediscover exact-index and active
    online-maintenance metadata for every unchanged-key row when the executor
    has already built the same plan once per statement; that closes the
    remaining cross-layer metadata loop, but donor-comparable
    `bulk_update_with_case` evidence is still required before closure

## 2026-04-05: `PP-10-007` is active after the bulk-lane maintenance-plan hoist

- Decision:
  - one-time post-insert maintenance planning for `BulkInsertHandle` is enough
    to move `PP-10-007` from `queued` to `active`, but not to close it
- Reason:
  - the maintained-write bulk lane no longer has to rebuild ordinary exact-index
    metadata for every inserted row once the handle is admitted, and that is now
    proven in storage and executor trace surfaces; however, the package still
    requires grouped exact-secondary application, cleanup-debt evidence, and
    donor-comparable benchmark recovery before the ticket can close

## 2026-04-05: `PP-10-007` hoists deferred exact backlog state once per admitted bulk lane

- Decision:
  - exact-secondary deferred mode should become a sticky maintenance-plan
    property after backlog is detected once for the current bulk lane
- Reason:
  - row-by-row `index_page_delta_catalog` probes were still left in the hot
    insert loop after the first maintenance-plan hoist; paying that backlog
    discovery cost once in `beginBulkInsert()` removes the repeated probe from
    subsequent `insertTupleWithHandle()` calls on the same statement-local lane
- Evidence:
  - `StorageEngineTest.BulkInsertHandleHoistsDeferredExactBacklogStateOnce`
  - `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`
  - `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`

## 2026-04-05: `PP-10-007` moves deferred exact catalog writes out of the hot bulk row loop

- Decision:
  - once a statement-local bulk lane is already in deferred-exact mode, buffer
    deferred exact rows on the maintenance target and flush them at
    `endBulkInsert()` or on a bounded threshold
- Reason:
  - the second hoist slice still paid one durable `index_page_delta_catalog`
    write per inserted row; buffering those rows keeps correctness but removes
    the durable delta write from the hot insert loop
- Evidence:
  - `StorageEngineTest.BulkInsertHandleHoistsDeferredExactBacklogStateOnce`
  - focused `9/9` executor/storage regression slice on the fresh binary

## 2026-04-05: `PP-10-007` publishes and clears exact-family cleanup debt on deferred merge lanes

- Decision:
  - deferred exact backlog must publish exact-family cleanup debt when rows are
    flushed into `index_page_delta_catalog`, and merge drain must overwrite that
    publication with `COMPLETE` when the backlog reaches zero
- Reason:
  - without a shared publication bridge, maintained write lanes could accumulate
    exact-secondary backlog invisibly and leave `index_health.cleanup_backlog_*`
    stale even though the deferred merge path was working; publishing on flush
    and clearing on merge keeps the debt ledger aligned with the real exact
    backlog state
- Evidence:
  - `StorageEngineTest.BulkInsertHandleHoistsDeferredExactBacklogStateOnce`
  - `StorageEngineTest.ColdExactSecondaryCleanupDebtPublishesAndClearsOnMerge`
  - `StorageEngineTest.ColdExactSecondaryInsertDeltaMergesOnRead`
  - focused `11/11` executor/storage regression slice on the fresh binary

## 2026-04-05: `PP-10-007` groups direct non-unique exact-secondary inserts on admitted bulk lanes

- Decision:
  - non-unique ordinary exact-secondary maintenance on an admitted
    statement-local bulk lane should buffer encoded keys and tuple ids on the
    maintenance target, then flush them in sorted order at `endBulkInsert()` or
    on a bounded threshold
- Reason:
  - after the maintenance-plan hoist and deferred-exact routing slices, the
    remaining maintained-write hot-loop cost was still the direct
    `insertIntoIndex()` call for every non-deferred exact secondary row;
    grouping those rows on the bulk lane removes the repeated direct index call
    from the row loop while preserving exact-secondary correctness and keeping
    the flush boundary explicit and observable
- Evidence:
  - `ExecutorTest.MultiRowValuesInsertPublishesGroupedExactSecondaryPlan`
  - `StorageEngineTest.BulkInsertHandleBuildsPostInsertMaintenancePlanOnce`
  - `StorageEngineTest.BulkInsertHandleGroupsDirectExactSecondaryRowsUntilFlush`
  - focused `13/13` executor/storage regression slice on the fresh binary

## 2026-04-05: `PP-10-008` is active after prepared-bundle resolution moved into `ConnectionContext`

- Decision:
  - prepared bundle selection, bucket reuse, and generic-vs-custom mode
    transitions should live in `ConnectionContext` and be shared by direct
    engine `PREPARE` / `EXECUTE PREPARED` and the native protocol adapter
- Reason:
  - package `10` needed one canonical prepared fast-path authority; the native
    protocol path already had bounded bucket logic, but direct engine prepared
    execution still stored only one bytecode blob and could not reuse or rebuild
    parameter-regime bundles. Centralizing selection in `ConnectionContext`
    removes the duplicated policy, makes prepared DML stay generic in both
    surfaces, and gives both execution fronts the same prepared-bundle hit and
    rebuild behavior
- Evidence:
  - `ExecutorTest.PreparedPointSelectBucketsCustomPlansForDirectEngineContext`
  - `ExecutorTest.PreparedInsertStatementsStayGenericForDirectEngineContext`
  - `ProtocolAdapterDialectsNative.PreparedStatementsCacheBucketedCustomPlansForScratchBird`
  - `ProtocolAdapterDialectsNative.PreparedInsertStatementsStayGenericForScratchBird`
  - focused `4/4` prepared execution regression slice on the fresh binary

## 2026-04-05: direct engine prepared trace must match protocol bundle observability

- Decision:
  - direct engine prepared execution must emit the same bounded seed and
    resolve trace contract as the protocol adapter instead of treating prepared
    bundle observability as protocol-only evidence
- Reason:
  - `PP-10-008` needs low-friction proof that prepared bundle hit, rebuild,
    generic-bundle fallback, and bucket signature are observable on both entry
    points. Keeping that trace only in `protocol_adapter.cpp` would leave the
    direct executor path under-instrumented and make prepared-plan debugging
    depend on the wire protocol even though the authoritative bundle resolver
    now lives in `ConnectionContext`
- Evidence:
  - `ExecutorTest.PreparedSelectPublishesBundleTraceForDirectEngineContext`
  - `ExecutorTest.PreparedInsertPublishesGenericTraceForDirectEngineContext`
  - `ExecutorTest.PreparedPointSelectBucketsCustomPlansForDirectEngineContext`
  - `ExecutorTest.PreparedInsertStatementsStayGenericForDirectEngineContext`
  - `ProtocolAdapterDialectsNative.PreparedStatementsCacheBucketedCustomPlansForScratchBird`
  - `ProtocolAdapterDialectsNative.PreparedInsertStatementsStayGenericForScratchBird`
  - focused `6/6` prepared execution regression slice on the fresh binary

## 2026-04-05: prepared bundle reuse and result-cache reuse must be observable as separate surfaces

- Decision:
  - executor-level prepared execution must expose whether the selected bytecode
    reused a cached result or populated the result cache, instead of forcing
    tests to infer cache behavior from bundle-hit state alone
- Reason:
  - `PP-10-008` is not complete when prepared `SELECT` proves only plan-bundle
    reuse. The parity package also requires result-cache coordination for
    cacheable prepared reads and explicit non-use for prepared DML. Adding
    direct executor flags lets the prepared fast path prove `bundle_hit=1`
    without claiming the rows were cached, and prove that prepared `INSERT`
    remains outside the result cache entirely
- Evidence:
  - `ExecutorTest.PreparedSelectSeparatesBundleReuseFromResultCacheReuse`
  - `ExecutorTest.PreparedInsertRemainsOutsideResultCache`
  - `ExecutorTest.PreparedSelectPublishesBundleTraceForDirectEngineContext`
  - `ExecutorTest.PreparedInsertPublishesGenericTraceForDirectEngineContext`
  - `ProtocolAdapterDialectsNative.PreparedStatementsCacheBucketedCustomPlansForScratchBird`
  - `ProtocolAdapterDialectsNative.PreparedInsertStatementsStayGenericForScratchBird`
  - focused `8/8` prepared execution regression slice on the fresh binary

## 2026-04-05: prepared custom-bundle rebuild must expose VNext plan-cache reuse separately from local bundle reuse

- Decision:
  - prepared execution selection must publish whether a custom rebuild actually
    consulted the global VNext plan cache and whether that compile was served by
    the cache, instead of leaving plan-cache reuse implicit inside compiler
    stats only
- Reason:
  - `PP-10-008` was still open because bundle reuse and plan-cache reuse are
    different artifacts. A fresh prepared handle should be able to miss its
    local bundle map, rebuild a custom variant, and still hit the shared plan
    cache. Exposing `plan_cache_consulted` and `plan_cache_hit` on
    `PreparedExecutionSelection` makes that behavior directly testable on the
    direct executor path and trace surface
- Evidence:
  - `ExecutorTest.PreparedSelectPublishesPlanCacheHitForFreshPreparedHandle`
  - `ExecutorTest.PreparedSelectPublishesBundleTraceForDirectEngineContext`
  - focused `10/10` prepared execution regression slice on the fresh binary

## 2026-04-05: prepared point update evidence must prove generic DML still takes the unchanged-key update fast path

- Decision:
  - prepared point update remains on the generic DML bundle, but the execution
    proof must also demonstrate that the prepared path still takes the bounded
    unchanged-key update fast path and remains outside the result cache
- Reason:
  - the package exit criteria explicitly require prepared point-update evidence.
    Proving only prepared point select and prepared insert leaves a gap where
    prepared DML might execute correctly but silently lose the update-side
    speedup. The direct prepared update proof closes that gap by asserting the
    generic prepared bundle, zero result-cache use, and the unchanged-key trace
    markers on the executed update
- Evidence:
  - `ExecutorTest.PreparedPointUpdateStaysGenericAndUsesUnchangedKeyFastPath`
  - `ExecutorTest.PreparedInsertRemainsOutsideResultCache`
  - `ExecutorTest.UpdateSkipsPostStorageIndexMaintenanceForUnchangedPlainKeys`
  - focused `11/11` prepared execution regression slice on the fresh binary

## 2026-04-05: admitted non-exact secondary runtime must fail closed until BTREE range locality is proven end-to-end

- Decision:
  - the phase-3 secondary-read runtime slice now admits ordered covering
    composite-prefix BTREE range execution again, but it does so with a
    correctness-first left-edge scan when the lower bound only covers a leading
    index prefix. `INDEX_ONLY_SCAN` relations that still retain non-covered
    columns must continue to fall back explicitly with
    `PROJECTED_COLUMN_NOT_IN_INDEX:<column_name>`
- Reason:
  - the first executor attempt at direct BTREE range walking produced a bounded
    partial-read failure on the ordered multicolumn proof because the runtime
    sought into a composite index using only the serialized leading-column
    lower bound. That routing is not yet safe for composite-prefix range
    execution. Scanning from the left edge and applying predicate pushdown
    restores correctness and keeps the runtime fast path alive while the lower
    BTree prefix-range contract remains unproven
- Evidence:
  - `QueryPlannerIntegrationTest.MulticolumnOrderedAccessFamiliesSurviveIntoRuntimePlan`
  - `QueryPlannerIntegrationTest.ExactIndexOnlyPlanUsesProvedRuntimePathWhenSingleRelationProjectionIsCovered`
  - `QueryPlannerIntegrationTest.OrderedCoveringRangeUsesProvedIndexOnlyRuntimePath`
  - focused `4/4` secondary-read regression slice on the fresh binary

## 2026-04-05: unqualified column refs in single-relation SELECT must not force full-row widening

- Decision:
  - relation projection binding now keeps unqualified column refs narrow when
    the SELECT has exactly one source relation, so covered exact
    `INDEX_ONLY_SCAN` plans can stay on the runtime index-only path
- Reason:
  - the initial phase-3 runtime slice proved visibility and key recheck, but
    the executor still widened single-table projections to all columns because
    unqualified refs were treated as ambiguous. That falsely forced heap fetch
    on a truly covered exact probe. Single-relation unqualified refs are not
    ambiguous, so the binder must preserve the narrow projection set
- Evidence:
  - `QueryPlannerIntegrationTest.ExactIndexOnlyPlanUsesProvedRuntimePathWhenSingleRelationProjectionIsCovered`
  - `QueryPlannerIntegrationTest.CoveringIndexPlanUsesIndexOnlyScan`
  - focused `4/4` secondary-read regression slice on the fresh binary

## 2026-04-05: batched key access must replay only matching key groups after the ordered-exact inner fetch

- Decision:
  - admitted exact nested-loop joins with a runtime-filter-backed right-side
    BTREE probe now execute a real `ORDERED_EXACT_BKA_PROBE` replay step after
    the batched fetch. The executor groups fetched inner rows by normalized
    join key and replays only the matching group for each outer row; if the
    join-key positions are not explicit and safe, execution falls back to the
    generic nested-loop join path
- Reason:
  - the earlier phase-3/phase-4 join slice only batched the right-side fetch.
    After loading the filtered inner rows, the executor still rescanned the
    entire fetched right batch for every outer row. That preserved correctness
    but left the dominant nested-loop cost shape too close to row-at-a-time
    probing. Keyed replay closes the real `BKA` execution gap without changing
    semantics, and the fallback guard keeps non-explicit or unsafe join-key
    layouts out of the specialization
- Evidence:
  - `QueryPlannerIntegrationTest.NestedLoopJoinUsesBatchedKeyAccessRuntimeFilterProbe`
  - `QueryPlannerIntegrationTest.JoinRuntimeFilterUsesRightSideIndexMetadata`

## 2026-04-05: runtime join-key binding must resolve against explicit left and right source ranges

- Decision:
  - runtime hash, merge, and `BATCHED_KEY_ACCESS` execution now bind planned
    join-key metadata with side-aware source-range resolution. The executor
    must first honor the planner-provided left/right qualifier intent against
    the left and right source bindings, and only then fall back to a bounded
    same-side column-name search
- Reason:
  - the first adaptive hash-join implementation published correct planner
    metadata but failed to execute the runtime adaptive path because generic
    alias lookup could collapse duplicate join-column names onto the left side.
    That left the hash and merge execution paths unable to prove a left/right
    key split even though the runtime plan already carried legal join-key
    metadata. Side-aware binding restores the admitted hash, merge, and BKA
    paths without widening semantics or guessing across relation boundaries
- Evidence:
  - `QueryPlannerIntegrationTest.AdaptiveHashJoinPublishesReversibleBuildSideMetadata`
  - `QueryPlannerIntegrationTest.AdaptiveHashJoinFlipsToObservedSmallerBuildSide`
  - `QueryPlannerIntegrationTest.NestedLoopJoinUsesBatchedKeyAccessRuntimeFilterProbe`
  - `QueryPlannerIntegrationTest.MergeJoinPlanExecutesAndPreservesRuntimeMetadata`
  - focused `9/9` indexed-join regression slice
  - focused `5/5` merge-join regression slice

## 2026-04-05: sort runtime must skip avoidable churn when input or spill runs are already ordered

- Decision:
  - the `ORDER BY` runtime now detects already ordered input before the
    in-memory sort step and must skip the full `stable_sort` or `partial_sort`
    when the input is already in final order. On the spilled lane, run
    generation must detect presorted runs and write them directly to
    `sb_workfile` instead of resorting each run buffer. The executor must
    publish this behavior through `SCRATCHBIRD_SELECT_TRACE` with sort-level
    `mode`, `spill`, `input_rows`, `output_rows`, `run_count`,
    `presorted_runs`, `incremental_groups`, `incremental_runs`,
    `prefix_order_keys`, and `top_n`
- Reason:
  - the first phase-4 aggregate slice removed stringified hot keys from
    admitted hash aggregation, but the sort lane still paid avoidable
    `stable_sort` and `partial_sort` churn even when heap order already matched
    the requested order and when spill runs were already monotonic. That kept
    the dominant sort hot path too close to worst-case work even on donor-like
    ordered ingest surfaces. Detecting and skipping already ordered input and
    presorted spill runs closes that bounded gap without changing result
    ordering or spill semantics
- Evidence:
  - `QueryPlannerIntegrationTest.SortTopNShortcutsAlreadyOrderedInputWithoutFullSort`
  - `QueryPlannerIntegrationTest.SpilledSortSkipsRunSortForAlreadyOrderedInput`
  - `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForSort`
  - `QueryPlannerIntegrationTest.ExecutedSortPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`
  - `QueryPlannerIntegrationTest.ExecutedSortFeedbackRecordsUnderuseBeforeShrinkThreshold`
  - `QueryPlannerIntegrationTest.ExecutedSortCapturesActualSpillOnStaleBytecode`
  - focused `6/6` sort regression slice on the fresh binary

## 2026-04-05: sort runtime must incrementally refine first-key prefix-ordered input instead of full sorting it again

- Decision:
  - when a sort has more than one key and the runtime input is already ordered
    by the first sort key, the executor now admits a bounded incremental-sort
    lane. The in-memory path sorts only within contiguous equal-prefix groups,
    and spilled sort run generation applies the same grouped rewrite before the
    run is written to `sb_workfile`. The current slice is intentionally bounded
    to one delivered prefix key; deeper prefixes and partially ordered lanes
    remain future work
- Reason:
  - the fully presorted optimization removed needless full sorts only when the
    entire requested order was already satisfied. That still left a large
    avoidable churn shape for donor-like inputs that were already grouped by a
    leading key but unsorted on the suffix keys. Sorting only within the equal
    prefix groups preserves semantics while removing the dominant redundant
    global reorder work from that bounded lane
- Evidence:
  - `QueryPlannerIntegrationTest.IncrementalSortUsesPrefixOrderedInputInMemory`
  - `QueryPlannerIntegrationTest.SpilledSortUsesIncrementalRunOrderingOnPrefixOrderedInput`
  - focused `8/8` sort regression slice on the fresh binary

## 2026-04-05: admitted integer hash aggregation must not stringify group keys on either the in-memory or spilled path

- Decision:
  - the current `PP-10-011` slice admits structured group keys for direct
    integer and boolean `HASH_AGG` grouping when every grouping expression is a
    direct admitted scalar column. That now covers both single-column scalar
    grouping and multi-column composite grouping, and the same admitted key
    contract governs in-memory grouping and spilled partition/regroup workfile
    routing while all non-admitted shapes stay on the existing string-key
    fallback
- Reason:
  - the join runtime already had a bounded structured-key path, but hash
    aggregation still serialized dominant integer grouping workloads into
    strings before hashing, partitioning, and regrouping. That left a large
    upper-stage overhead exactly on the hot `GROUP BY integer_column` and
    `GROUP BY integer_column_1, integer_column_2` patterns that donor engines
    keep on typed hash keys. Reusing typed scalar components for the admitted
    aggregate slice removes that avoidable churn without guessing across
    expression-heavy or non-scalar grouping shapes
- Evidence:
  - `QueryPlannerIntegrationTest.HashAggregateUsesFastScalarGroupKeysForAdmittedIntegerGrouping`
  - `QueryPlannerIntegrationTest.SpilledHashAggregateUsesFastScalarGroupKeysForAdmittedIntegerGrouping`
  - `QueryPlannerIntegrationTest.HashAggregateUsesFastCompositeGroupKeysForAdmittedIntegerGrouping`
  - `QueryPlannerIntegrationTest.SpilledHashAggregateUsesFastCompositeGroupKeysForAdmittedIntegerGrouping`
  - `QueryPlannerIntegrationTest.ExecutedHashAggregatePersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`
  - `QueryPlannerIntegrationTest.ExecutedHashAggregateCapturesActualSpillOnStaleBytecode`
  - focused `7/7` hash-aggregate regression slice on the fresh binary
  - focused `5/5` indexed-join regression slice on the fresh binary

## 2026-04-05: the structured-key aggregate fast path must cover admitted text and scalar-expression grouping before `PP-10-011` can close

- Decision:
  - `PP-10-011` is not considered closed until the structured-key hash
    aggregate lane admits bounded direct text grouping and admitted scalar
    grouping expressions, and the full hash-plus-sort closure slice stays green
    on the fresh binary
- Reason:
  - donor engines do not limit typed aggregate hashing to raw integer columns.
    Leaving text or simple scalar-expression grouping on the stringified-key
    fallback would keep a large avoidable serialization cost on common `GROUP
    BY` shapes and would leave the ticket only partially closed. Extending the
    admitted key substrate to text and scalar expressions closes the remaining
    dominant upper-stage gap without guessing across non-admitted types
- Evidence:
  - `QueryPlannerIntegrationTest.HashAggregateUsesFastScalarGroupKeysForAdmittedTextGrouping`
  - `QueryPlannerIntegrationTest.SpilledHashAggregateUsesFastScalarGroupKeysForAdmittedTextGrouping`
  - `QueryPlannerIntegrationTest.HashAggregateUsesFastScalarGroupKeysForAdmittedScalarExpressionGrouping`
  - `QueryPlannerIntegrationTest.SpilledHashAggregateUsesFastScalarGroupKeysForAdmittedScalarExpressionGrouping`
  - focused `19/19` `PP-10-011` closure slice on the fresh binary

## 2026-04-05: `PP-10-012` distinct must reuse typed keys before any broader vectorized upper-stage closure

- Decision:
  - the first active `PP-10-012` slice makes hash-distinct reuse the bounded
    structured-key substrate on admitted scalar and composite projected rows
    for both in-memory and spilled execution, with explicit trace publication
    of `key_mode`, `spill`, `input_rows`, and `output_rows`
- Reason:
  - distinct still serialized full projected rows into strings on its dominant
    hash path even after `PP-10-011` removed the same overhead from hash
    aggregation. That left avoidable row-image churn on a hot upper-stage
    operator and would have made later vectorized work less trustworthy. The
    typed-key distinct slice closes that overhead first, while leaving
    non-admitted rows on the explicit string fallback
- Evidence:
  - `QueryPlannerIntegrationTest.DistinctUsesFastScalarKeysForAdmittedIntegerProjection`
  - `QueryPlannerIntegrationTest.SpilledDistinctUsesFastScalarKeysForAdmittedIntegerProjection`
  - `QueryPlannerIntegrationTest.DistinctUsesFastCompositeKeysForAdmittedCompositeProjection`
  - `QueryPlannerIntegrationTest.SpilledDistinctUsesFastCompositeKeysForAdmittedCompositeProjection`
  - `QueryPlannerIntegrationTest.ExecutedHashDistinctPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile`
  - `QueryPlannerIntegrationTest.ExecutedHashDistinctCapturesActualSpillOnStaleBytecode`
  - `QueryPlannerIntegrationTest.SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashDistinct`
  - `QueryPlannerIntegrationTest.SpillPolicyDisallowChoosesOrderedDistinctWhenAvailable`
  - focused `8/8` distinct regression slice on the fresh binary

## 2026-04-05: window specialization must avoid global sort when partition order is already delivered

- Decision:
  - the next active `PP-10-012` slice keeps the bounded row-engine window path
    explicit but adds three legal local ordering modes:
    `INPUT_ALREADY_ORDERED`, `PARTITION_PREFIX_INCREMENTAL`, and
    `FULL_SORT` or `FULL_SORT_SPILL`. Partitioned windows now skip spill and
    global reorder when the incoming row stream is already fully ordered for
    the window, or when only per-partition refinement is needed and each
    partition fits the bounded local window buffer
- Reason:
  - the existing window runtime always built a full local sort surface for any
    window with `PARTITION BY` or `ORDER BY`, even when the upstream rows were
    already in final window order or only needed sorting inside contiguous
    partition groups. That unnecessary full-sort and spill work would have
    kept `window_function_ranking` far behind the donor ceiling even after the
    earlier incremental sort closure
- Evidence:
  - `QueryPlannerIntegrationTest.WindowReusesAlreadyOrderedPartitionInputWithoutSpill`
  - `QueryPlannerIntegrationTest.WindowUsesPartitionPrefixIncrementalSortWithoutSpill`
  - `QueryPlannerIntegrationTest.WindowPreservesOrderedInputAndAvoidsFinalSort`
  - `QueryPlannerIntegrationTest.OrderedCoveringRangePreservesInclusiveLowerBoundAtLeafBoundary`
  - `BTreeIteratorTest.AscendingRightEdgeRangeIncludesTerminalKeyAfterRepeatedSplits`
  - focused `5/5` window and key-order regression slice on the fresh binary

## 2026-04-05: incremental sort must exploit the longest delivered prefix, not only the first key

- Decision:
  - the next active `PP-10-012` slice generalizes the bounded incremental-sort
    runtime from `PREFIX_1` only to the longest delivered prefix already
    satisfied by the incoming row stream. Both in-memory sort and spilled
    sort-run generation now refine only within equal-prefix groups and publish
    `PREFIX_<n>_INCREMENTAL` or `PREFIX_<n>_INCREMENTAL_RUNS` with
    `prefix_order_keys=<n>`
- Reason:
  - the earlier sort closure only recognized a delivered prefix of one key.
    Inputs that already satisfied `(k1, k2)` but still needed refinement on
    `k3` kept paying broader sort work than necessary, especially on spilled
    run generation. That left avoidable churn on multi-key order paths that
    donor engines typically exploit
- Evidence:
  - `QueryPlannerIntegrationTest.IncrementalSortUsesPrefixOrderedInputInMemory`
  - `QueryPlannerIntegrationTest.SpilledSortUsesIncrementalRunOrderingOnPrefixOrderedInput`
  - `QueryPlannerIntegrationTest.IncrementalSortUsesDeepPrefixOrderedInputInMemory`
  - `QueryPlannerIntegrationTest.SpilledSortUsesDeepPrefixRunOrderingOnPrefixOrderedInput`
  - `QueryPlannerIntegrationTest.SortTopNShortcutsAlreadyOrderedInputWithoutFullSort`
  - `QueryPlannerIntegrationTest.SpilledSortSkipsRunSortForAlreadyOrderedInput`
  - `QueryPlannerIntegrationTest.WindowPreservesOrderedInputAndAvoidsFinalSort`
  - `QueryPlannerIntegrationTest.OrderedCoveringRangePreservesInclusiveLowerBoundAtLeafBoundary`
  - `BTreeIteratorTest.AscendingRightEdgeRangeIncludesTerminalKeyAfterRepeatedSplits`
  - focused `11/11` key-order, sort, and window regression slice on the fresh binary

## 2026-04-05: non-covering same-column BTREE ranges must carry both bounds into runtime and use bounded selectivity

- Decision:
  - ordinary BTREE same-column range candidates now preserve the full compatible
    same-index predicate set into runtime, publish non-zero candidate budgets,
    and estimate bounded ranges by subtraction instead of multiplying
    independent single-ended range predicates
- Reason:
  - the first non-covering `MRR` attempt still degraded to `SEQ_SCAN` because
    the planner both overestimated the bounded range and truncated runtime
    predicates to a single bound. That left the executor without a safe upper
    stop condition and the planner without a locality-aware candidate budget.
    Carrying both bounds and using a bounded selectivity model is the minimum
    fix that makes the non-covering tid-ordered path legal and chosen
- Evidence:
  - `QueryPlannerIntegrationTest.NonCoveringRangeUsesTidOrderedSecondaryHeapFetchPath`
  - `QueryPlannerIntegrationTest.OrderedCoveringRangeUsesProvedIndexOnlyRuntimePath`
  - focused `5/5` secondary-read regression slice on the fresh binary

## 2026-04-05: parameterized memoize must stay statement-local and fail closed on volatile child shapes

- Decision:
  - the first indexed-join parity slice uses only a statement-local bounded
    memoize cache on `PARAMETERIZED_NESTED_LOOP`, keyed by the explicit
    correlated outer column refs when available and fail-closed for volatile
    function shapes or parameterized table-function children
- Reason:
  - repeated parameterized rescans are currently one of the most obvious join
    locality losses in the live executor path, but unconditional memoize would
    be unsafe for volatile parameterized children. Statement-local reuse on the
    deterministic correlated-key surface recovers the repeated-probe speedup
    without weakening semantics
- Evidence:
  - `QueryPlannerIntegrationTest.ParameterizedNestedLoopMemoizesRepeatedOuterBindings`
  - `QueryPlannerIntegrationTest.LateralJoinUsesParameterizedNestedLoopPath`
  - focused `4/4` indexed-join regression slice on the fresh binary

## 2026-04-05: nested-loop batched key access must be published only after runtime-filter admission is finalized

- Decision:
  - the planner publishes `BATCHED_KEY_ACCESS` only from the finalized
    right-side runtime-filter decision point, and the executor now emits an
    explicit select-trace join line for the admitted batched-key probe
- Reason:
  - the first planner attempt attached the `BATCHED_KEY_ACCESS` enabler before
    runtime-filter admission had been finalized, so the runtime fast path could
    execute without the runtime plan proving it. Binding the enabler to the
    actual runtime-filter decision keeps the join contract truthful and makes
    the batched-key probe directly testable
- Evidence:
  - `QueryPlannerIntegrationTest.NestedLoopJoinUsesBatchedKeyAccessRuntimeFilterProbe`
  - `QueryPlannerIntegrationTest.JoinRuntimeFilterUsesRightSideIndexMetadata`
  - focused `5/5` indexed-join regression slice on the fresh binary

## 2026-04-05: ordered distinct must not fall through into a stale fast-key hash pass

- Decision:
  - once the distinct runtime admits the ordered-stream lane, it finalizes that
    output directly and must not reuse the original pre-stream fast-key vector
    in a second hash-dedup pass
- Reason:
  - the first ordered-distinct implementation deduplicated correctly on the
    ordered stream, then fell through into the generic hash path with the stale
    pre-stream key vector from the original 320 input rows. On ordered input
    that collapses the result back onto the first duplicate group and hides the
    real `ORDERED_STREAM` mode
- Evidence:
  - `QueryPlannerIntegrationTest.DistinctStageUsesOrderedDistinctWhenInputOrderIsAvailable`
  - `QueryPlannerIntegrationTest.DistinctStageUsesHashDistinctWhenOrderIsUnavailable`
  - focused `7/7` distinct regression slice on the fresh binary

## 2026-04-05: window refinement must exploit the longest delivered partition-plus-order prefix

- Decision:
  - the window runtime now detects the longest delivered prefix over
    `PARTITION BY` keys plus `ORDER BY` keys and refines only inside equal
    prefix groups when those groups fit the admitted local buffer
- Reason:
  - stopping at partition-only refinement leaves easy locality on the table for
    inputs already ordered by the first one or more window order keys. Reusing
    the deeper delivered prefix keeps the row engine on bounded local work and
    avoids unnecessary full window sorts
- Evidence:
  - `QueryPlannerIntegrationTest.WindowUsesPartitionOrderPrefixIncrementalSortWithoutSpill`
  - `QueryPlannerIntegrationTest.WindowUsesPartitionPrefixIncrementalSortWithoutSpill`
  - focused `11/11` distinct-plus-window regression slice on the fresh binary

## 2026-04-05: ordered distinct must compare adjacent admitted rows without string materialization

- Decision:
  - the ordered distinct stream now compares adjacent admitted rows through the
    structured distinct key substrate and only falls back to direct value
    adjacency when no fast typed key exists
- Reason:
  - the original ordered-stream path still rebuilt `make_values_key()` strings
    per row even after the hash-distinct lane had moved to typed keys. That
    left avoidable allocation and hashing on one of the most obvious
    low-materialization distinct paths
- Evidence:
  - `QueryPlannerIntegrationTest.DistinctStageUsesOrderedDistinctWhenInputOrderIsAvailable`
  - `QueryPlannerIntegrationTest.OrderedCompositeDistinctUsesFastAdjacentStreamingComparison`
  - focused `8/8` distinct regression slice on the fresh binary

## 2026-04-05: partitioned row-number windows should store partition starts, not copied partition keys

- Decision:
  - once the window lane has already established final row order, it now stores
    compact partition-start markers instead of copying the partition key values
    back into a second per-row vector for `ROW_NUMBER()` reset logic
- Reason:
  - the previous fast window path paid a second materialization cost after the
    ordering decision even though the row-number executor only needs boundary
    transitions. Marker state keeps the same semantics with materially less
    row-engine allocation and copy work
- Evidence:
  - `QueryPlannerIntegrationTest.WindowReusesAlreadyOrderedPartitionInputWithoutSpill`
  - `QueryPlannerIntegrationTest.WindowUsesPartitionPrefixIncrementalSortWithoutSpill`
  - `QueryPlannerIntegrationTest.WindowUsesPartitionOrderPrefixIncrementalSortWithoutSpill`
  - focused `12/12` distinct-plus-window regression slice on the fresh binary

## 2026-04-05: upper-stage fast lanes must cross into ResultSet through an explicit batch handoff

- Decision:
  - once distinct/window upper-stage processing has produced final rows, the
    executor now slices offset/limit once, transfers the surviving rows through
    `ResultSet::addRows(...)`, and publishes `handoff=BATCH_ROWS`
- Reason:
  - the previous row engine still moved final rows across the result boundary
    one row at a time after all upper-stage work had already converged. That
    kept avoidable per-row dispatcher overhead on the exact lanes where we are
    trying to establish true vector/batch execution closure
- Evidence:
  - `QueryPlannerIntegrationTest.DistinctStageUsesOrderedDistinctWhenInputOrderIsAvailable`
  - `QueryPlannerIntegrationTest.WindowReusesAlreadyOrderedPartitionInputWithoutSpill`
  - `QueryPlannerIntegrationTest.WindowUsesPartitionOrderPrefixIncrementalSortWithoutSpill`
  - focused `12/12` distinct-plus-window regression slice on the fresh binary

## 2026-04-05: admitted ordered-window projections must bypass generic row-by-row expression evaluation

- Decision:
  - once the ordered window lane has already established final row order and
    the projection list is bounded to direct column refs plus
    `ROW_NUMBER()/RANK()/DENSE_RANK()`, the executor now emits those projected
    rows through a bounded batch fast path instead of calling `evalExpr(...)`
    per row
- Reason:
  - the previous window fast lane still paid generic row-engine expression
    dispatch after the expensive order and partition work had already been
    avoided. That left avoidable interpreter overhead on the exact admitted
    `ROW_NUMBER()` benchmark shapes the package is trying to close
- Evidence:
  - `QueryPlannerIntegrationTest.WindowReusesAlreadyOrderedPartitionInputWithoutSpill`
  - `QueryPlannerIntegrationTest.WindowUsesPartitionPrefixIncrementalSortWithoutSpill`
  - `QueryPlannerIntegrationTest.WindowUsesPartitionOrderPrefixIncrementalSortWithoutSpill`
  - widened focused `12/12` distinct-plus-window regression slice on the fresh
    binary

## 2026-04-05: parallel gather wrappers must charge worker memory, not just worker count

- Decision:
  - `costGather(...)` now multiplies worker memory by the admitted participant
    count, and `costGatherMerge(...)` adds an explicit bounded coordinator
    merge budget on top of that worker charge
- Reason:
  - the previous parallel wrapper cost path priced setup and tuple-transfer
    overhead, but it left the gather memory envelope at zero or a serial-sized
    budget. That made the runtime plan understate worker reservations exactly
    where the parity package needs worker-aware grant charging
- Evidence:
  - `QueryPlannerIntegrationTest.ParallelSeqScanWrapsPlanInGatherAndPublishesRelationMetadata`
  - `QueryPlannerIntegrationTest.ParallelHashJoinWrapsPlanInGatherAndPublishesJoinMetadata`
  - `QueryPlannerIntegrationTest.ParallelAggregateWrapsPlanInGatherAndPublishesStageMetadata`
  - `QueryPlannerIntegrationTest.OrderedParallelPlanUsesGatherMergeAndExplainJsonPublishesParallelFields`
  - focused `4/4` parallel planner slice on the fresh binary

## 2026-04-08: parallel worker scheduling must publish real locality-first execution evidence

- Decision:
  - the direct parallel executor now uses worker-local queues with bounded
    stealing, and the scan/aggregate runtime paths now publish per-morsel
    execution records instead of only planner-side gather metadata
- Reason:
  - the previous package state proved only planner admission and gather-wrapper
    metadata. It did not prove that live workers preferred local morsels first,
    whether stealing occurred, or which page-range each worker actually ran.
    That left the ticket short of its locality-binding obligation even though
    gather costing was already fixed
- Evidence:
  - `ParallelExecutionControlsTest.WorkerPoolPrefersLocalQueueAndStealsWhenImbalanced`
  - `ParallelScanExecutionTest.ParallelScanExecutesRealWorkerRangesAndReturnsAllRows`
  - `ParallelScanExecutionTest.ParallelAggregateExecutesRealWorkerPartitionsForNumericColumn`
  - focused `11/11` parallel execution-controls slice on the fresh binary
  - adjacent planner parallel slice remains green at `4/4`

## 2026-04-08: parallel hash join must prove live build and probe work, not just gather-wrapper admission

- Decision:
  - `PP-10-013` now requires a direct executor proof for parallel hash join
    build/probe work with worker-range evidence, transfer-byte accounting, and
    real match production on typed rows
- Reason:
  - planner-side `Gather` admission was already proven, but the runtime join
    path was still a stub. Closing the ticket on gather-wrapper metadata alone
    would leave join locality, build/probe ownership, and actual worker-side
    join correctness unproven. The first direct proof also exposed that padded
    synthetic row shapes can create a false all-match decode surface, so the
    canonical bounded proof now uses inline typed rows for the admitted join
    contract
- Evidence:
  - `ParallelScanExecutionTest.ParallelHashJoinExecutesRealWorkerBuildAndProbeSlices`
  - widened `12/12` parallel execution-controls slice on the fresh binary
  - adjacent planner parallel slice remains green at `4/4`

## 2026-04-08: parallel sort and window proofs must follow the real locality contract, not an impossible zero-steal ideal

- Decision:
  - `PP-10-013` now treats direct parallel sort and window execution as part of
    the mandatory runtime surface, and the window proof accepts bounded steals
    when the worker-local queue policy makes them observable after local queues
    drain
- Reason:
  - the ticket required executable multi-worker sort and window stages, not
    only planner-side `Gather` wrappers. The first direct window proof exposed
    that a locality-first queue policy can still legally steal work after local
    morsels complete, so the correct contract is explicit locality preference
    plus steal evidence, not a blanket zero-steal assertion
- Evidence:
  - `ParallelExecutionControlsTest.ParallelSortPublishesWorkerLocalMergeEvidence`
  - `ParallelExecutionControlsTest.ParallelWindowPublishesWorkerLocalPartitionEvidence`
  - widened `14/14` parallel execution-controls slice on the fresh binary
  - adjacent planner parallel slice remains green at `4/4`

## 2026-04-08: direct parallel runtime must enforce worker-local memory budgets, not only publish planner-side grant math

- Decision:
  - the direct parallel runtime now fails closed when a materializing worker
    exceeds `work_mem_per_worker`: sort partitions, bounded ordered window
    partitions, and hash-join build workers all reject with an explicit memory
    reservation error instead of silently overrunning the worker-local budget
- Reason:
  - `PP-10-013` required worker-aware grant binding, but the live direct
    runtime still trusted planner-side grant math and did not enforce any
    worker-local ceiling. That left the new multi-worker sort and window paths
    faster but still unconstrained. The correct bounded contract is explicit
    per-worker refusal on the materializing stages until benchmark-governed
    executor lanes consume the same runtime substrate
- Evidence:
  - `ParallelExecutionControlsTest.ParallelSortRejectsWorkerReservationOverflow`
  - `ParallelExecutionControlsTest.ParallelWindowRejectsWorkerReservationOverflow`
  - `ParallelScanExecutionTest.ParallelHashJoinRejectsBuildReservationOverflow`
  - widened `17/17` parallel execution-controls slice on the fresh binary

## 2026-04-08: benchmark-governed ordered lanes must consume the live parallel substrate, not stop at planner metadata

- Decision:
  - `PP-10-013` now requires the real executor to hand admitted ordered
    no-spill plans into the direct parallel runtime: `GatherMerge -> Sort`
    must use `ParallelSort`, and admitted ordered `ROW_NUMBER` must reuse the
    direct `ParallelWindow` row-number runtime after ordering is established
- Reason:
  - planner-side `GatherMerge` proofs and direct-runtime unit tests were both
    green, but the benchmark executor was still doing serial sort and serial
    row-number projection even on admitted parallel plans. That left the
    locality/worker-grant substrate disconnected from the only lanes that
    matter for the package-10 parity benchmark. The bounded closure also
    exposed two runtime details that had to be fixed before the handoff was
    meaningful: the no-spill sort proof must stay within the bounded runtime
    sort budget, and `ParallelSort` must honor the configured
    `min_rows_per_worker` instead of a hardcoded `10k` threshold
- Evidence:
  - `QueryPlannerIntegrationTest.OrderedParallelPlanExecutesLiveParallelSortRuntime`
  - `QueryPlannerIntegrationTest.OrderedParallelWindowPlanExecutesLiveParallelRowNumberRuntime`
  - widened `26/26` focused slice on the fresh binary across direct parallel
    controls, the adjacent planner parallel slice, and the affected
    sort/window executor regressions

## 2026-04-08: benchmark-governed parallel sequential scan must use live worker scan execution, not serial heap materialization

- Decision:
  - admitted `Gather -> SeqScan` plans now execute through the direct
    `ParallelScan` substrate on the real V3 `loadTable(...)` path, with the
    existing projected-row decode and runtime-filter checks preserved after the
    worker-local scan callback
- Reason:
  - planner-side parallel scan admission and direct `ParallelScan` unit proofs
    were already green, but the benchmark-governed executor still loaded the
    base relation through a serial `createScan(...)` loop before later stages
    ran. That left the scan lane short of the same live-runtime closure the
    sort, window, and aggregate lanes already had, so `Gather -> SeqScan`
    could not yet prove worker-local scan execution or executor-side parallel
    trace evidence on the real query path
- Evidence:
  - `QueryPlannerIntegrationTest.ParallelSeqScanWrapsPlanInGatherAndPublishesRelationMetadata`
  - widened `24/24` focused slice on the fresh binary across the direct
    parallel execution-controls suite, the direct scan/aggregate/hash unit
    surface, and the affected planner/executor parallel regressions

## 2026-04-08: planner-selected parallel aggregate must execute live on the benchmark-governed V3 path

- Decision:
  - `PP-10-013` now treats simple single-table `GROUP BY <column>, COUNT(*)`
    shapes as part of the live parallel executor surface. When the runtime
    plan already chose the parallel aggregate wrapper, the V3 executor must
    admit the bounded direct `ParallelAggregate::executeGroupBy(...)` lane
    instead of falling back to the older serial spilling hash aggregate path
- Reason:
  - the planner already published `Gather -> Aggregate` parallel metadata for
    the bounded count-by-group shape, and the direct aggregate substrate had
    real worker-partition coverage for scalar aggregation. The remaining gap
    was in the real executor path: `COUNT(*)` compiled as
    `AGG_COUNT(args=[LITERAL_INT64 1])`, so a narrow empty-args matcher left
    the live V3 path on the serial spill-prone hash aggregate even when the
    plan had already admitted parallel aggregate
- Evidence:
  - `ParallelScanExecutionTest.ParallelAggregateExecutesRealWorkerPartitionsForSimpleGroupByCount`
  - `QueryPlannerIntegrationTest.ParallelAggregateWrapsPlanInGatherAndPublishesStageMetadata`
  - widened `24/24` focused parallel slice on the fresh binary across the
    direct controls/unit surface and the affected planner/executor parallel
    regressions

## 2026-04-08: benchmark-governed parallel hash join must bind the loaded right relation, not the raw join-node stub

- Decision:
  - admitted `Gather -> HashJoin` plans now execute through the direct
    `ParallelHashJoin` substrate on the real V3 path using the already loaded
    right relation metadata (`right.info.table_id`,
    `right.materialized_from_view`) instead of the raw `join.table` stub
- Reason:
  - planner-side `Gather -> HashJoin` admission and the direct
    `ParallelHashJoin` worker proofs were already green, but the
    benchmark-governed executor still declined the live lane because
    `join.table.info.table_id` stayed empty on this execution path. That left
    the last disconnected benchmark-governed parallel family on the serial
    adaptive hash path even though the runtime plan had already admitted the
    worker-partitioned join. The bounded closure is to bind live
    physical-table identity from the loaded right relation and preserve the
    existing join predicate filter inside the parallel callback
- Evidence:
  - `QueryPlannerIntegrationTest.ParallelHashJoinWrapsPlanInGatherAndPublishesJoinMetadata`
  - widened `24/24` focused parallel slice on the fresh binary across the
    direct controls/unit surface and the affected planner/executor parallel
    regressions

## 2026-04-08: unchanged-key fixed-width updates must patch tuple payloads instead of reserializing full rows

- Decision:
  - on the V3 executor path, unchanged-key updates that only modify fixed-width
    scalar columns with stable null state now patch the existing tuple payload
    bytes directly and narrow coercion/domain work to changed columns instead
    of reserializing the full row image
- Reason:
  - the direct `2026-04-08` ScratchBird stress rerun still showed
    `bulk_update_with_case` as a dominant regression even though exact-index
    maintenance had already been skipped. The remaining hot cost surface was
    full-row coercion plus full-row tuple serialization on a workload that only
    changes one non-indexed decimal column. The bounded recovery step is to
    reuse the fetched tuple bytes when row shape stays stable and keep per-row
    validation work proportional to the changed column set
- Evidence:
  - `ExecutorTest.UpdateSkipsPostStorageIndexMaintenanceForUnchangedPlainKeys`
  - direct `2026-04-08` benchmark evidence from
    `pp10-013-small-stress-direct-20260408T150315Z` showing the write-heavy
    regressions still dominated after the parallel runtime slices

## 2026-04-08: simple `INSERT ... SELECT` benchmark targets must bypass the generic insert validator

- Decision:
  - on the V3 executor path, admitted `INSERT ... SELECT` statements that feed
    all target columns in ordinal order into a simple table with no defaults,
    generated columns, triggers, domains, or foreign keys now take a bounded
    simple fast lane instead of the generic per-row insert validator
- Reason:
  - the direct `2026-04-08` normal bulk rerun showed that the unchanged-key
    update patch moved `bulk_update_with_case`, but `bulk_insert_select`
    remained the dominant write-path regression. The sink was still paying the
    full generic insert validation surface on every producer row even when the
    benchmark target shape was simple enough to validate once and keep the
    remaining per-row work to coercion, serialization, heap insert, and exact
    index maintenance
- Evidence:
  - `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`
  - `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`
  - `ExecutorTest.BasicSecondaryIndexInsertMaintainedExactlyOnce`
  - `ExecutorTest.StorageBackedUniqueInsertFailuresStillRejectDuplicates`

## 2026-04-08: `bulk_insert_select` remains producer-bound after sink fusion

- Decision:
  - keep `PP-10-004` open after the simple-target fast lane and callback-sink
    fusion; the next bounded recovery step must attack producer-side derived
    table and subquery execution instead of adding more insert-sink-only
    changes
- Reason:
  - fresh normal benchmark evidence on `2026-04-08` after both sink-side cuts
    still left `bulk_insert_select` slower than the earlier fresh
    `2026-04-08T154451Z` baseline, while `bulk_update_with_case` recovered into
    the prior range. That separates the remaining regression surface from the
    unchanged-key update lane and from the generic stored-result-set handoff;
    the benchmark query shape is still dominated by producer work before rows
    reach the sink
- Evidence:
  - fresh benchmark artifact
    `write-path-bulk-slice-20260408T163126Z/stress_scratchbird_normal_transactional_20260408_123933.json`
  - comparison against
    `write-path-bulk-slice-20260408T154451Z/stress_scratchbird_normal_transactional_20260408_115250.json`

## 2026-04-08: direct derived projection recovers only part of the `bulk_insert_select` regression

- Decision:
  - keep `PP-10-004` open after the first producer-boundary cut; the next
    bounded recovery step must go deeper than the outer derived projection lane
    and attack the benchmark producer shape itself
- Reason:
  - the new direct derived-projection path removed the second generic outer
    select/materialize pass and improved `bulk_insert_select` from `199.02s` to
    `195.89s`, but that is still materially slower than the earlier
    `181.94s` fresh baseline. This proves the outer derived boundary was part
    of the cost, but not the dominant remaining cost
- Evidence:
  - executor proof
    `ExecutorTest.InsertSelectFromDerivedWindowSourceStreamsSelectOutput`
  - benchmark artifact
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T181000Z/stress_scratchbird_normal_transactional_20260408_134813.json`
  - comparisons against
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T171252Z/stress_scratchbird_normal_transactional_20260408_132149.json`
    and
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T154451Z/stress_scratchbird_normal_transactional_20260408_115250.json`

## 2026-04-08: prepared-value serialization recovers another bounded slice but does not close parity

- Decision:
  - keep `PP-10-004` open after the prepared-value serializer cut; continue on
    write-lane amortization and remaining producer work instead of declaring
    the `bulk_insert_select` lane recovered
- Reason:
  - the clean-root `normal_transactional` rerun after removing the second
    coercion pass improved `bulk_insert_select` from `11.17s` to `9.77s` on
    the same bounded slice, proving the duplicated per-row coercion work was
    real cost. But `order_items` load still took `246.21s`, and the target
    statement remains materially slower than donor engines, so the dominant
    remaining frontier is still above pure tuple serialization
- Evidence:
  - executor proofs
    `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`
    `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`
    `ExecutorTest.InsertSelectFromDerivedWindowSourceStreamsSelectOutput`
  - fresh benchmark artifact
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T222052Z/stress_scratchbird_normal_transactional_20260408_182620.json`
  - comparison baseline
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T213431Z/stress_scratchbird_normal_transactional_20260408_173836.json`

## 2026-04-08: ordinary multi-row VALUES still needed the simple insert fast lane

- Decision:
  - keep `PP-10-004` open, but shift the dominant write-lane risk from raw
    load admission to the remaining producer side of `bulk_insert_select`
- Reason:
  - the clean-root rerun after admitting ordinary multi-row `VALUES` to the
    same bounded simple insert fast lane cut load time dramatically:
    `customers 6.16s -> 2.61s`, `products 3.39s -> 1.03s`,
    `orders 29.13s -> 9.22s`, and `order_items 246.21s -> 22.78s`. That
    proves the benchmark load path was still paying generic row-by-row insert
    machinery even after the earlier bulk-handle and prepared-serializer cuts.
    `bulk_insert_select` improved only slightly (`9.77s -> 9.51s`), so the
    dominant remaining parity gap is now the producer statement itself, not the
    simple `VALUES` load lane
- Evidence:
  - executor proof
    `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`
  - fresh benchmark artifact
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T223519Z/stress_scratchbird_normal_transactional_20260408_183638.json`
  - comparison baseline
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T222052Z/stress_scratchbird_normal_transactional_20260408_182620.json`

## 2026-04-08: streamed simple-target coercion was still frozen to generic mode

- Decision:
  - keep `PP-10-004` open after fixing streamed simple-target coercion-mode
    classification; record the improvement, but continue on higher producer
    work because the donor parity gap is still large
- Reason:
  - the streamed `INSERT ... SELECT` lane was precomputing coercion modes
    before the producer result set existed, so the callback path froze every
    column to `GENERIC` and missed the bounded integer/varchar/decimal direct
    lanes that the simple target shape should already admit. Fixing that bug
    improved the clean-root `normal_transactional` benchmark from `9.51s` to
    `9.34s` while keeping load in the recovered range (`customers 2.61s ->
    2.66s`, `products 1.03s -> 1.04s`, `orders 9.22s -> 9.21s`,
    `order_items 22.78s -> 22.62s`). That proves the streamed coercion freeze
    was real cost, but ScratchBird still trails the donor engines by a large
    multiple on the target statement
- Evidence:
  - executor proofs
    `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`
    `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`
    `ExecutorTest.InsertSelectFromDerivedWindowSourceStreamsSelectOutput`
  - fresh benchmark artifact
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260409T014451Z/stress_scratchbird_normal_transactional_20260408_214608.json`
  - comparison baseline
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260408T223519Z/stress_scratchbird_normal_transactional_20260408_183638.json`

## 2026-04-08: streaming child derived projection is real but still not donor-close

- Decision:
  - keep `PP-10-004` open after removing the child derived-query materialize
    step on the direct derived-projection lane; record the new best clean-root
    ScratchBird slice and continue on higher producer work
- Reason:
  - the benchmark query still had one extra executor artifact after the earlier
    sink fixes: the child derived query feeding the outer direct projection was
    materialized into an executor-owned `ResultSet` before the insert sink
    consumed it. Streaming that child directly into the outer callback proved
    to be real cost and improved the clean-root `normal_transactional`
    benchmark from `9.34s` to `9.28s` while the recovered load lane stayed in
    range (`customers 2.66s -> 2.67s`, `products 1.04s -> 1.03s`,
    `orders 9.21s -> 9.16s`, `order_items 22.62s -> 22.48s`). That is the new
    best ScratchBird slice on this ticket, but ScratchBird still trails the
    donor engines materially on the target statement
- Evidence:
  - executor proofs
    `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`
    `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`
    `ExecutorTest.InsertSelectFromDerivedWindowSourceStreamsSelectOutput`
  - fresh benchmark artifact
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260409T020109Z/stress_scratchbird_normal_transactional_20260408_220226.json`
  - comparison baseline
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260409T014451Z/stress_scratchbird_normal_transactional_20260408_214608.json`

## 2026-04-08: storage red slice was test helper drift; validated build sets a new retained best

- Decision:
  - keep `PP-10-004` open, record the storage red slice as test-only BTREE
    key-encoding drift, and retain the fresh clean-root rerun as the new best
    current ScratchBird slice for this ticket
- Reason:
  - continued producer work surfaced red storage tests on the exact-secondary
    proof slice, but the failure widened beyond the maintained unique lane and
    traced back to test helpers still probing BTREE pages with raw `INT32`
    bytes after the engine-side lookup path had moved to order-preserving
    encoded keys. Restoring the helper lookup to the same encoded key contract
    cleared the focused storage plus executor slice without changing the
    maintained-write semantics. A fresh clean-root benchmark on the validated
    binary then edged the retained ScratchBird best from `9.28s` to `9.24s`,
    while the recovered load lane also improved slightly (`customers 2.67s ->
    2.63s`, `products 1.03s -> 1.03s`, `orders 9.16s -> 8.94s`,
    `order_items 22.48s -> 22.33s`). This preserves the recovered lane, but
    ScratchBird still trails the donor engines materially on the target
    statement
- Evidence:
  - focused repaired proof slice
    `StorageEngineTest.BulkInsertHandleGroupsDirectExactSecondaryRowsUntilFlush`
    `StorageEngineTest.BulkInsertHandleBuffersEmptyUniqueMaintenanceForExactIndex`
    `StorageEngineTest.BulkInsertHandleRejectsDuplicateKeyWithBufferedEmptyUniqueMode`
    `StorageEngineTest.ColdExactSecondaryCleanupDebtPublishesAndClearsOnMerge`
    `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`
    `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`
    `ExecutorTest.InsertSelectFromDerivedWindowSourceStreamsSelectOutput`
  - fresh benchmark artifact
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260409T033246Z/stress_scratchbird_normal_transactional_20260408_233404.json`
  - prior retained best
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260409T020109Z/stress_scratchbird_normal_transactional_20260408_220226.json`

## 2026-04-09: early row-count fast path removes base-row decode but does not
close the donor gap

- Decision: keep the early count-only producer path and continue on deeper
  producer/runtime work
- Reason:
  - the benchmark `bulk_insert_select` shape is an admitted raw cross-join
    derived query with `ROW_NUMBER() OVER ()` and `LIMIT`, so unconditional base
    relation row decoding was avoidable work
  - the executor now counts the base relation and raw cross-join inputs before
    deciding whether it can emit the bounded row-number stream directly
  - the first clean-root rerun after the change preserved the recovered load
    lane and improved load-side row counting, but the target statement still
    moved only from `9.24s` to `9.41s`, which leaves ScratchBird materially
    behind the donor engines
  - this proves the row-decode cut was real but not dominant; the remaining gap
    is still producer-side work above the sink and above the count-only
    specialization boundary
- Evidence:
  - focused executor proof slice
    `ExecutorTest.InsertSelectFromDerivedWindowSourceStreamsSelectOutput`
    `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`
    `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`
  - fresh benchmark artifact
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260409T075615Z/stress_scratchbird_normal_transactional_20260409_035732.json`
  - prior retained best
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260409T033246Z/stress_scratchbird_normal_transactional_20260408_233404.json`

## 2026-04-09: wider buffered unique exact batches do not recover the target lane

- Decision: keep the wider buffered unique exact batch window because the
  correctness and bounded-buffer proof is good, but do not treat it as the next
  parity lever for `bulk_insert_select`
- Reason:
  - empty-target unique exact maintenance was still flushing every `64` rows,
    so widening that batch window to `8192` rows was a plausible remaining
    write-lane improvement for load plus `INSERT ... SELECT`
  - the direct storage proof now shows buffered PK entries stay off the index
    past the old legacy threshold until `endBulkInsert()`
  - the first clean-root benchmark after that change regressed slightly on both
    load and the target statement (`bulk_insert_select 9.41s -> 9.44s`)
  - this means the old unique flush cadence was not the dominant remaining
    throughput limit on the benchmark lane
- Evidence:
  - focused proof slice
    `StorageEngineTest.BulkInsertHandleBuffersEmptyUniqueMaintenanceForExactIndex`
    `StorageEngineTest.BulkInsertHandleRejectsDuplicateKeyWithBufferedEmptyUniqueMode`
    `StorageEngineTest.BulkInsertHandleKeepsBufferedEmptyUniqueRowsOffIndexPastLegacyThreshold`
    `ExecutorTest.InsertSelectFromDerivedWindowSourceStreamsSelectOutput`
    `ExecutorTest.InsertSelectUsesDirectResultSetBulkSink`
    `ExecutorTest.MultiRowValuesInsertUsesStatementBulkHandle`
  - fresh benchmark artifact
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260409T084132Z/stress_scratchbird_normal_transactional_20260409_044250.json`
  - prior retained best
    `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/write-path-bulk-slice-20260409T075615Z/stress_scratchbird_normal_transactional_20260409_035732.json`
