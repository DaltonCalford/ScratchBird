# ScratchBird Benchmark Regression And Donor Fast Path Audit

Date: 2026-04-05

## 1. Scope

This audit answers four questions:

1. Why did the latest ScratchBird stress benchmark get materially worse after a sequence of "speed up" patches?
2. Which parts of the regression are benchmark-environment noise versus real engine regressions?
3. Why are MySQL, PostgreSQL, and FirebirdSQL faster across the benchmark processes?
4. What is the ranked implementation queue to make ScratchBird as fast as possible, and then faster than the donors where practical?

This audit is based on:

- ScratchBird stress result:
  - `clean-rebuild-scratchbird-stress-20260404T023343Z`
- Regressed ScratchBird stress result:
  - `current-scratchbird-stress-20260405T035510Z`
- Cross-engine donor matrix:
  - `txmode-matrix-20260403T152011Z`
- Benchmark harness and Python driver code under:
  - `ScratchBird-Benchmarks/`
  - `ScratchBird-driver/`
- ScratchBird runtime paths under:
  - `ScratchBird/src/core/`
  - `ScratchBird/src/sblr/`
- Donor engine source clones under:
  - `docs/reference/project_clones/local_existing/mysql`
  - `docs/reference/project_clones/local_existing/postgresql`
  - `docs/reference/project_clones/local_existing/firebird`
- Existing MySQL insert audit:
  - `SCRATCHBIRD_MYSQL_INSERT_FAST_PATH_DELTA_2026-04-03`

## 2. Executive Findings

### 2.1 Primary conclusions

1. The new ScratchBird regression is real, not a harmless benchmark artifact.
2. The regression is dominated by write-paths, especially:
   - data load
   - `bulk_insert_select`
   - `bulk_update_with_case`
3. The strongest concrete ScratchBird regression source is the unchanged-key update path:
   - `bulk_update_with_case` updates a non-indexed column
   - the new path still enumerates indexes and maintenance state per updated row
   - that turns a previously modest update into a catastrophic per-row metadata workload
4. ScratchBird still lacks a true ordinary multi-row / `INSERT ... SELECT` fast path:
   - the benchmark harness batches on the client
   - the ScratchBird Python driver also rewrites batchable inserts into multi-row `VALUES`
   - but the server still executes ordinary row-by-row executor/storage maintenance for those rows
5. MySQL and PostgreSQL are much faster because they amortize work at statement, page, and executor-node level.
6. Firebird is slower than MySQL/PostgreSQL on load, but still materially faster than current ScratchBird on most runtime DML and analytical cases because its mature MGA record store/modify and stream executor are cheaper than ScratchBird's current paths.
7. Benchmark provenance also needs tightening:
   - the benchmark launcher resolves `sb_server` from the live repo `build/` tree
   - it does not pin a run-specific binary artifact in the result directory
   - that means clean comparison discipline is weaker than it should be even when the code comparison is otherwise valid

### 2.2 Highest confidence root causes

The highest-confidence causes of the current ScratchBird slowdown are:

1. `INSERT ... SELECT` and ordinary multi-row insert still bypass the bulk insert handle.
2. Ordinary insert and load still pay row-by-row executor and storage maintenance.
3. Non-indexed updates now pay per-row stable-TID index-effect bookkeeping.
4. ScratchBird still trails donor engines on specialized join, sort, aggregate, and window execution paths.

## 3. Benchmark Evidence

### 3.1 Headline comparison

#### Normal transactional

| Row | Load | Tests | `bulk_insert_select` | `bulk_update_with_case` | `inner_join_large_result` | `multi_dimensional_agg` |
|---|---:|---:|---:|---:|---:|---:|
| Original ScratchBird | 34.99s | 45.22s | 15.29s | 1.31s | 13.92s | 5.05s |
| Regressed ScratchBird | 264.61s | 267.88s | 197.98s | 41.14s | 8.19s | 5.90s |
| FirebirdSQL | 110.90s | 22.72s | 1.86s | 4.44s | 4.09s | 1.21s |
| MySQL | 5.72s | 7.16s | 545.39ms | 115.03ms | 1.86s | 1.59s |
| PostgreSQL | 4.78s | 2.96s | 155.21ms | 195.76ms | 461.06ms | 851.81ms |

#### Autocommit

| Row | Load | Tests | `bulk_insert_select` | `bulk_update_with_case` | `inner_join_large_result` | `multi_dimensional_agg` |
|---|---:|---:|---:|---:|---:|---:|
| Original ScratchBird | 41.63s | 54.30s | 21.26s | 1.54s | 15.85s | 5.09s |
| Regressed ScratchBird | 872.30s | 582.08s | 455.38s | 94.50s | 8.71s | 6.68s |
| FirebirdSQL | 113.30s | 23.40s | 4.67s | 6.78s | 4.07s | 1.28s |
| MySQL | 5.70s | 8.92s | 558.07ms | 129.65ms | 1.61s | 1.35s |
| PostgreSQL | 4.76s | 3.09s | 192.39ms | 201.84ms | 586.55ms | 842.94ms |

### 3.2 Where the ScratchBird regression actually landed

The regression is not uniform.

- `inner_join_large_result` improved:
  - 13.92s -> 8.19s normal
  - 15.85s -> 8.71s autocommit
- The catastrophic regressions are concentrated in write-heavy paths:
  - `bulk_insert_select`: 15.29s -> 197.98s normal, 21.26s -> 455.38s autocommit
  - `bulk_update_with_case`: 1.31s -> 41.14s normal, 1.54s -> 94.50s autocommit
  - data load: 34.99s -> 264.61s normal, 41.63s -> 872.30s autocommit

### 3.3 Load regression by table

#### Normal transactional load

| Table | Original SB | Regressed SB | Delta |
|---|---:|---:|---:|
| `customers` | 2.57s | 6.38s | 2.48x |
| `products` | 1.06s | 3.56s | 3.36x |
| `orders` | 8.91s | 30.04s | 3.37x |
| `order_items` | 22.45s | 224.63s | 10.00x |

#### Autocommit load

| Table | Original SB | Regressed SB | Delta |
|---|---:|---:|---:|
| `customers` | 2.97s | 43.92s | 14.78x |
| `products` | 1.25s | 22.52s | 18.04x |
| `orders` | 10.45s | 130.42s | 12.48x |
| `order_items` | 26.96s | 675.44s | 25.05x |

The load collapse is dominated by `order_items`, which is exactly where the benchmark has the largest row volume and therefore where any row-at-a-time overhead compounds most aggressively.

## 4. Benchmark Harness Facts That Matter

### 4.1 The harness is already batching on the client

The load runner:

- generates rows in batches
- calls `executemany(sql, batch_values)`
- commits after each batch

The workload indexes are created after data load, not before it. That means the load-phase regression is not caused by the benchmark building the secondary workload indexes during the large data pump.

### 4.2 Driver behavior by engine

#### ScratchBird

The ScratchBird Python driver rewrites batchable insert statements into multi-row `VALUES` batches:

- `_BATCHABLE_INSERT_RE`
- `_DEFAULT_EXECUTEMANY_BATCH_ROWS = 1024`
- `_MAX_EXECUTEMANY_BATCH_PARAMS = 6144`
- `_MAX_EXECUTEMANY_BATCH_SQL_BYTES = 256 * 1024`

So ScratchBird is already receiving batched insert statements from the benchmark driver.

#### MySQL

PyMySQL `executemany()` rewrites matching insert statements into one multi-row statement via `_do_execute_many`.

#### PostgreSQL

The benchmark harness uses `psycopg2.extras.execute_values()` for batchable insert statements, which explicitly constructs `VALUES (...), (...), ...`.

#### FirebirdSQL

The `fdb` driver `executemany()` is just `execute()` in a loop over the parameter list.

### 4.3 What this means

This is a critical audit result:

- MySQL and PostgreSQL do get client-side batching wins.
- ScratchBird also gets client-side batching.
- FirebirdSQL does not.

Therefore ScratchBird's catastrophic new slowdown cannot be explained by "the client stopped batching".

It must be explained mainly by server-side execution cost.

## 5. ScratchBird Root Cause Analysis

### 5.1 `INSERT ... SELECT` is still row-oriented

The current benchmark's worst query is `bulk_insert_select`.

The SQL is a large set-based insert:

- build `seq` from a `ROW_NUMBER()` subquery
- generate 100,000 rows
- insert them into `bulk_insert_test`

ScratchBird already has a reusable bulk insert handle in storage:

- `StorageEngine::beginBulkInsert()`
- `StorageEngine::insertTupleWithHandle()`

But the ordinary executor insert path still loops rows and calls ordinary `insertTuple()`:

- constraint checks
- unique checks
- foreign key checks
- storage insert
- domain uniqueness registration
- `updateIndexesOnInsert()` for SQL-evaluated index classes

That means ordinary multi-row insert and `INSERT ... SELECT` still behave like repeated singleton inserts with only client-side batching around them.

This is fundamentally different from donor engines:

- MySQL starts statement-level bulk insert mode
- PostgreSQL has `heap_multi_insert()`
- Firebird performs engine-native set-based insert even if its Python driver is less sophisticated

### 5.2 Ordinary insert still pays heavy post-insert work per row

`StorageEngine::performPostInsertMaintenance()` still does substantial work for every inserted row:

- `listIndexesForTable()`
- `getColumns()`
- `computeColumnLayout()`
- extract ordinary index keys
- insert or defer secondary exact-index entries
- online-maintenance delta capture

This is exactly the kind of per-row metadata and maintenance work donor engines try hard to hoist or amortize.

### 5.3 `bulk_update_with_case` is strongly consistent with a newly introduced overhead

`bulk_update_with_case` updates `order_items.discount_pct`.

The benchmark workload indexes on `order_items` are:

- `order_id`
- `product_id`

So this benchmark is a classic non-indexed-column update.

The executor computes `indexed_keys_unchanged`, and the storage engine now routes unchanged-key updates through `captureUnchangedStableTidIndexEffects()`.

That helper still does per-row work:

- `listIndexesForTable()`
- skip expression/partial indexes
- `getIndexPtr()`
- `bumpIndexContentionCounters()`
- resolve active online maintenance id
- potentially capture maintenance deltas

For a large `CASE` update that touches many rows, this is exactly the wrong place to introduce per-row catalog/index scanning.

This is the strongest high-confidence explanation for the `bulk_update_with_case` collapse:

- 1.31s -> 41.14s normal
- 1.54s -> 94.50s autocommit

### 5.4 Current regression map by benchmark process

#### Load

Dominant issue:

- row-at-a-time executor/storage path despite client batching

Most likely ScratchBird weakness:

- no ordinary multi-insert page/buffer fast path
- repeated per-row maintenance and metadata lookup

#### `bulk_insert_select`

Dominant issue:

- set-based query feeding a row-at-a-time insert path

Most likely ScratchBird weakness:

- bulk insert handle not used for `INSERT ... SELECT`
- no statement-level or page-level amortization comparable to MySQL or PostgreSQL

#### `bulk_update_with_case`

Dominant issue:

- unchanged-key update path now pays new per-row index-effect bookkeeping

Most likely ScratchBird weakness:

- non-indexed updates still walk index state and maintenance metadata per row

#### `bulk_update_with_join`

Dominant issue:

- join + update path still pays update machinery, but the regression is far smaller than `bulk_update_with_case`

Most likely ScratchBird weakness:

- update runtime still heavier than donor engines, but this path did not inherit the same collapse magnitude

#### Join-heavy paths

- `inner_join_simple`
- `inner_join_multiple_conditions`
- `inner_join_large_result`
- `left_join_all_customers`
- `four_table_join`
- `self_join_same_country`

Dominant issue:

- join planning/runtime specialization still behind donor engines

Most likely ScratchBird weakness:

- weaker join ordering and runtime operator specialization
- incomplete parity with mature hash/merge/sorted join pipelines

#### Aggregate-heavy paths

- `aggregation_daily_sales`
- `multi_dimensional_agg`
- `agg_full_table_scan`
- `agg_distinct_counts`
- `nested_subquery_agg`

Dominant issue:

- aggregate and group execution remain materially heavier than PostgreSQL and MySQL

Most likely ScratchBird weakness:

- weaker aggregate/sort fusion
- weaker distinct strategy selection
- heavier materialization and expression overhead

#### Window path

- `window_function_ranking`

Dominant issue:

- donor engines have mature sort + window pipelines

Most likely ScratchBird weakness:

- window execution still materially more expensive even after recent phase-5 spill work

## 6. Donor Engine Audit

## 6.1 MySQL

### Insert/load fast paths

The existing MySQL insert audit was confirmed by direct source review:

- statement-level bulk insert setup:
  - `ha_start_bulk_insert(insert_many_values.size())`
- batched auto-increment reservation:
  - `handler::ha_start_bulk_insert()` and surrounding handler logic
- reusable prebuilt insert-row infrastructure:
  - `row_get_prebuilt_insert_row(prebuilt)`
- optimistic insert before pessimistic fallback:
  - `btr_cur_optimistic_insert(...)`
  - fallback to `btr_cur_pessimistic_insert(...)`
- deferred secondary exact-index work for eligible pages:
  - `ibuf_should_try(...)`

These are exactly the kinds of amortization ScratchBird is still missing on ordinary multi-row insert and `INSERT ... SELECT`.

### Join/sort/window fast paths

MySQL also has mature executor structures for the query-heavy benchmark cases:

- hash join row buffer using structured in-memory hash tables:
  - `HashJoinRowBuffer`
- explicit filesort pipeline:
  - `SortingIterator::DoSort()`
  - `::filesort(...)`
- window execution using materialization and buffering iterators:
  - `WindowIterator`
  - `BufferingWindowIterator`

### Why MySQL is faster than ScratchBird in this benchmark

MySQL wins because it amortizes both write and read costs:

- bulk statement setup for inserts
- engine-side row conversion reuse
- optimistic B-tree insertion
- change buffering for eligible secondary indexes
- specialized join/sort/window executors

In the benchmark matrix this shows up most dramatically in:

- `bulk_insert_select`
- `bulk_update_with_case`
- join-heavy read paths
- window ranking

## 6.2 PostgreSQL

### Insert/load fast paths

PostgreSQL has the cleanest donor story for load and `INSERT ... SELECT`:

- benchmark harness uses `execute_values()`
- engine supports `heap_multi_insert()`

The `heap_multi_insert()` comment is explicit about the win:

- single page lock/unlock for multiple tuples on one page
- single WAL record covering multiple tuples

That is the closest donor analogue to the missing ScratchBird ordinary multi-insert path.

### Update fast path for non-indexed changes

PostgreSQL also has a direct donor answer for `bulk_update_with_case`:

- `heap_update()` can choose `use_hot_update = true`
- HOT update avoids ordinary index churn when modified columns do not intersect hot-blocking indexed attributes

ScratchBird currently does not have an equivalent narrow fast path for this benchmark shape.

### Join/sort/window/aggregate fast paths

PostgreSQL also has specialized executor/runtime support:

- hybrid hash join:
  - `ExecHashJoinImpl`
- mature work-memory-aware tuplesort:
  - `tuplesort_begin_common()`
- dedicated WindowAgg:
  - `nodeWindowAgg.c`
  - partition-ordered input
  - tuplestore-backed partition buffering

### Why PostgreSQL is faster than ScratchBird in this benchmark

PostgreSQL wins because it amortizes work at nearly every layer:

- client batching
- heap-page multi-insert
- HOT update for non-indexed-column modifications
- mature hash join / tuplesort / window executor nodes

This is why PostgreSQL is the best donor in most benchmark processes, especially:

- load
- `bulk_insert_select`
- most joins
- aggregates

## 6.3 FirebirdSQL

### Insert/load behavior

FirebirdSQL does not enjoy the same Python driver batching advantage:

- `fdb.executemany()` is just `execute()` in a loop

That is why FirebirdSQL load is much slower than MySQL and PostgreSQL.

However, Firebird still beats current ScratchBird by large margins on many runtime cases.

### Runtime DML and executor path

Firebird's mature internal paths observed here:

- core MGA record modify path:
  - `VIO_modify(...)`
- core MGA record store path:
  - `VIO_store(...)`
- nested-loop join executor:
  - `NestedLoopJoin`
- sort executor:
  - `SortedStream`
- aggregate executor:
  - `AggregatedStream`
- window executor:
  - `WindowedStream`

### Why Firebird is still faster than ScratchBird here

Firebird's client path is weaker than MySQL/PostgreSQL, but its engine paths are mature and cheap enough that it still outperforms ScratchBird on:

- `bulk_insert_select`
- `bulk_update_with_case`
- most analytical runtime cases

This matters because it means ScratchBird cannot explain its current performance gap away as "only a Python driver problem".

## 7. Benchmark Provenance And Comparison Hygiene

The benchmark launcher resolves ScratchBird from the live repo `build/` tree:

- `example_db_manager.sh` chooses:
  - `build/src/sb_server`
  - `build/src/server/sb_server`

The full build/test runner similarly configures `build/` directly and only then invokes benchmark scripts.

That creates a comparison hygiene issue:

- benchmark results are not self-contained binary artifacts
- the result directory does not independently pin the exact `sb_server` binary used
- build-type and binary-path provenance must be recorded inside the benchmark result set itself

This is a benchmarking-discipline problem, not the main reason for the regression, but it does need to be fixed.

## 8. What ScratchBird Must Do To Catch Up

### 8.1 Tier 0: Fix benchmark discipline

1. Stamp every ScratchBird benchmark result with:
   - git SHA
   - `CMAKE_BUILD_TYPE`
   - exact `sb_server` path
   - binary mtime
2. Allow the benchmark harness to pin a specific `sb_server` path instead of implicitly following the live `build/` tree.
3. Reject comparison reports when one side lacks build provenance.

### 8.2 Tier 1: Recover the lost performance first

1. Remove the per-row unchanged-key index-effect scan from non-indexed updates.
   - fast-return when there is no active online maintenance
   - batch contention/stat publication at statement scope, not row scope
   - do not enumerate table indexes per updated row if no physical index mutation is required
2. Extend the bulk insert handle to ordinary multi-row insert and `INSERT ... SELECT`.
   - not just `COPY`
3. Hoist invariant insert metadata out of row loops.
   - columns
   - column layout info
   - ordinary index list
   - serializers/extractors
   - toast strategy
4. Batch post-insert exact-index work.
   - commit-group apply
   - per-batch key extraction
   - per-page amortization
5. Preserve the existing cold exact-secondary deferral path, but move more ordinary insert traffic through it safely.

### 8.3 Tier 2: Match donor write-path architecture

1. Add a true ScratchBird analogue to `heap_multi_insert()`.
   - one page pin/lock cycle for many tuples
   - one statement/batch maintenance envelope
2. Add a HOT-like update path for non-indexed column changes.
   - stable-TID / heap-only versioning
   - skip full secondary maintenance when indexed columns are untouched
3. Add statement-level bulk setup for ordinary inserts.
   - prepare once
   - reserve once
   - finalize once

### 8.4 Tier 3: Match donor read/compute execution

1. Continue join runtime specialization beyond the current bounded phase-6 closures.
   - better join order and cardinality fidelity
   - broader structured-key hash join support
   - cheaper large-result join pipelines
2. Improve sort/aggregate/window fusion.
   - reduce materialization churn
   - prefer streaming where legal
   - exploit workfile only when needed
3. Improve group/distinct execution.
   - lower-cost aggregate state
   - better distinct strategy selection

## 9. Ranked Implementation Queue

### 9.1 Immediate recovery queue

1. `bulk_update_with_case` regression fix:
   - rewrite unchanged-key stable-TID maintenance so benchmark-shape non-indexed updates do not enumerate indexes per row
2. `bulk_insert_select` fast path:
   - route ordinary `INSERT ... SELECT` through bulk insert handle
3. ordinary multi-row insert fast path:
   - route executor multi-row `INSERT ... VALUES` into the same bulk path
4. insert-loop metadata hoisting:
   - resolve columns/indexes/layout once per statement
5. batch exact-secondary maintenance:
   - statement-local or commit-group-local apply

### 9.2 Second queue

1. HOT-like non-indexed update path
2. stronger join runtime specialization for multi-table join and large-result join
3. aggregate and window materialization reduction

## 10. Final Audit Judgment

The recent ScratchBird slowdown is not a mystery.

The benchmark evidence and local code inspection point to a consistent answer:

- the write-path work landed in the wrong shape
- ordinary multi-row insert is still too row-oriented
- non-indexed updates now do too much per-row metadata work
- donor engines are faster because they amortize work at statement, page, and executor-node level

The fastest path to a major recovery is therefore not another broad optimization sweep.

It is a targeted recovery program:

1. remove unchanged-key per-row index-effect overhead
2. route ordinary multi-row insert and `INSERT ... SELECT` into a true bulk path
3. add HOT-like non-indexed update behavior
4. continue join/sort/aggregate/window specialization until the large-result and analytical gaps close

Until those items land, ScratchBird will continue to lose badly on the exact benchmark shapes that should be its easiest wins.

## Appendix A. Full Scenario Matrix

### A.1 Normal transactional

| Scenario | Original SB | Regressed SB | FirebirdSQL | MySQL | PostgreSQL |
|---|---:|---:|---:|---:|---:|
| `agg_distinct_counts` | 1.39s | 2.91s | 439.19ms | 606.22ms | 350.79ms |
| `agg_full_table_scan` | 1.12s | 1.42s | 4.48s | 292.38ms | 71.82ms |
| `aggregation_daily_sales` | 2.37s | 3.32s | 624.16ms | 1.25s | 346.33ms |
| `bulk_insert_select` | 15.29s | 197.98s | 1.86s | 545.39ms | 155.21ms |
| `bulk_update_with_case` | 1.31s | 41.14s | 4.44s | 115.03ms | 195.76ms |
| `bulk_update_with_join` | 118.06ms | 143.26ms | 2.68s | 170.46ms | 341.30ms |
| `four_table_join` | 1.46s | 2.93s | 2.85ms | 1.81ms | 4.24ms |
| `inner_join_large_result` | 13.92s | 8.19s | 4.09s | 1.86s | 461.06ms |
| `inner_join_multiple_conditions` | 522.50ms | 566.80ms | 8.54ms | 20.84ms | 3.73ms |
| `inner_join_simple` | 1.11s | 1.29s | 532.94ms | 308.79ms | 64.72ms |
| `left_join_all_customers` | 483.92ms | 608.02ms | 186.60ms | 140.61ms | 34.27ms |
| `multi_dimensional_agg` | 5.05s | 5.90s | 1.21s | 1.59s | 851.81ms |
| `nested_subquery_agg` | 488.62ms | 762.03ms | 2.03s | 198.99ms | 70.38ms |
| `self_join_same_country` | 285.61ms | 314.84ms | 115.46ms | 53.68ms | 11.94ms |
| `window_function_ranking` | 290.70ms | 407.98ms | 16.46ms | 1.77ms | 1.55ms |

### A.2 Autocommit

| Scenario | Original SB | Regressed SB | FirebirdSQL | MySQL | PostgreSQL |
|---|---:|---:|---:|---:|---:|
| `agg_distinct_counts` | 1.48s | 3.04s | 454.01ms | 869.74ms | 330.19ms |
| `agg_full_table_scan` | 1.21s | 1.53s | 279.39ms | 395.04ms | 67.56ms |
| `aggregation_daily_sales` | 2.63s | 4.05s | 663.82ms | 3.01s | 348.82ms |
| `bulk_insert_select` | 21.26s | 455.38s | 4.67s | 558.07ms | 192.39ms |
| `bulk_update_with_case` | 1.54s | 94.50s | 6.78s | 129.65ms | 201.84ms |
| `bulk_update_with_join` | 118.93ms | 151.49ms | 2.16s | 188.46ms | 336.27ms |
| `four_table_join` | 1.64s | 3.54s | 16.95ms | 3.36ms | 3.49ms |
| `inner_join_large_result` | 15.85s | 8.71s | 4.07s | 1.61s | 586.55ms |
| `inner_join_multiple_conditions` | 606.20ms | 657.08ms | 18.46ms | 24.61ms | 7.51ms |
| `inner_join_simple` | 1.08s | 1.40s | 594.69ms | 275.31ms | 55.04ms |
| `left_join_all_customers` | 580.56ms | 771.10ms | 180.60ms | 146.95ms | 34.86ms |
| `multi_dimensional_agg` | 5.09s | 6.68s | 1.28s | 1.35s | 842.94ms |
| `nested_subquery_agg` | 536.02ms | 748.13ms | 2.07s | 301.88ms | 71.09ms |
| `self_join_same_country` | 368.52ms | 428.51ms | 129.23ms | 51.38ms | 8.73ms |
| `window_function_ranking` | 310.60ms | 491.40ms | 28.94ms | 3.14ms | 3.34ms |

### A.3 Donor no-transaction reference

| Scenario | MySQL no-transaction | PostgreSQL no-transaction |
|---|---:|---:|
| `agg_distinct_counts` | 595.21ms | 320.57ms |
| `agg_full_table_scan` | 311.54ms | 63.81ms |
| `aggregation_daily_sales` | 1.02s | 354.98ms |
| `bulk_insert_select` | 540.16ms | 140.33ms |
| `bulk_update_with_case` | 114.75ms | 203.84ms |
| `bulk_update_with_join` | 182.67ms | 312.14ms |
| `four_table_join` | 1.90ms | 4.18ms |
| `inner_join_large_result` | 1.62s | 416.34ms |
| `inner_join_multiple_conditions` | 23.55ms | 4.78ms |
| `inner_join_simple` | 267.73ms | 52.06ms |
| `left_join_all_customers` | 161.41ms | 35.19ms |
| `multi_dimensional_agg` | 1.47s | 824.84ms |
| `nested_subquery_agg` | 204.75ms | 66.96ms |
| `self_join_same_country` | 52.53ms | 8.77ms |
| `window_function_ranking` | 1.56ms | 1.61ms |
