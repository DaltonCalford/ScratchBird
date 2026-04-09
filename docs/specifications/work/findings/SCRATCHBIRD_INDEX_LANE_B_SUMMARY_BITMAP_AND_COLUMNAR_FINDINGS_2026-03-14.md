# ScratchBird Index Research Lane B Findings

Status: first-pass findings for specification drafting. This document is intentionally implementation-oriented and non-normative until the follow-on specs land.

## 1. Scope and lane objective

Lane B covers three families that ScratchBird already names or partially implements, but does not yet treat as a coherent optimizer and lifecycle surface:

- Summary families: BRIN and zonemap-style range summaries.
- Bitmap families: compressed posting structures for equality, `IN`, and set-combination access paths.
- Columnar families: columnstore storage and column-pruning/segment-pruning scans.

The lane objective is to answer six questions for these families:

- What ScratchBird already has in code and tests.
- Which donor-engine patterns are worth adopting.
- Which MGA invariants are non-negotiable.
- Which planner metrics and costing terms are required before these paths are selectable.
- Which contract clauses can be drafted now.
- Which items should be adopted, adapted, rejected, or deferred.

Non-goals for this lane:

- Replacing B-tree or LSM as the default exact OLTP access path.
- Designing approximate or vector indexes.
- Defining a full DDL grammar beyond the minimum contract implications required for follow-on specs.

The central framing is that Lane B structures are not all the same thing. BRIN is a lossy summary index. Bitmap is a compressed candidate-set index. Columnstore is a storage/access model that can expose summary behavior, but it is not just another exact secondary index.

## 2. ScratchBird current-state baseline

### 2.1 Runtime inventory

ScratchBird already registers BRIN, zonemap, bitmap, and columnstore families in the index factory and type taxonomy. The codebase therefore has runtime presence for Lane B, but only B-tree and LSM have mature planner integration. Current planner path selection remains centered on `SEQ_SCAN`, `INDEX_SCAN`, `INDEX_ONLY_SCAN`, synthetic `BITMAP_INDEX_SCAN`, and `LSM` paths.

The key baseline fact is that ScratchBird has lane-family implementations without lane-family planning contracts.

### 2.2 BRIN baseline

Current BRIN behavior is recognizably a page-range summary implementation:

- The runtime stores range summaries with min/max-style state and transaction metadata.
- Scan output is lossy and returns block-level candidates rather than exact row versions.
- Range visibility is checked using transaction metadata before candidate blocks are returned.
- A revmap-like acceleration exists in memory on open, but on-disk layout is still simpler than the canonical meta plus revmap plus data split.
- Inserts widen summaries rather than rewriting the represented heap region.

Current debt:

- Dead-entry handling currently allows range-level dead marking behavior, but summary families should not rely on blind tombstoning as steady-state maintenance.
- There is no full planner contract for `pages_per_range`, correlation sensitivity, false-positive rate, or unsummarized-tail handling.
- Runtime behavior is ahead of the statistics and costing surface required to select BRIN safely.

Practical interpretation: ScratchBird BRIN is already a lossy summary index in behavior, but it is not yet treated as such by the optimizer or the lifecycle contract.

### 2.3 Bitmap baseline

Current bitmap behavior is a real storage index family and not merely an execution-time bitmap:

- The runtime uses roaring-like array, bitset, and run containers.
- Entries are versioned with MGA-relevant metadata such as `xmin` and `xmax`.
- Deletes are modeled as logical tombstones rather than in-place destructive removal.
- The implementation already exposes garbage-collection hooks and tests for dead-entry removal.

The main ambiguity is planner naming:

- ScratchBird planner `BITMAP_INDEX_SCAN` currently means "combine multiple conventional indexes into an execution-time bitmap-like candidate set."
- That planner path is not the same thing as the `BitmapIndex` runtime family.

This distinction matters because the runtime bitmap family needs its own catalog, stats, costing, and lifecycle contract. Reusing the synthetic path name as if it already covered the storage index would hide real semantic differences around visibility, exactness, maintenance, and memory budgeting.

### 2.4 Columnstore baseline

ScratchBird already contains substantial columnstore code, including:

- A richer `ColumnstoreIndex` implementation with metadata pages, segment chains, buffered inserts, flush, scan, and dead-entry cleanup.
- A simpler `ColumnstoreIndexSimple` implementation that overlaps conceptually with the richer path.
- Segment metadata such as min/max, null bitmap, visibility bitmap, and row or TID mapping.
- Predicate pushdown and segment pruning logic, currently strongest for `INT32`.
- Compression support with `NONE`, `RLE`, `DICTIONARY`, `BITPACK`, and `DELTA`.

Current debt:

- The presence of two overlapping columnstore implementations is a shipping ambiguity.
- Header comments and implementation detail are not fully aligned on which compression modes are actually supported in production terms.
- The planner does not currently expose a dedicated columnar scan path, late materialization contract, or projection-sensitive costing model.

Practical interpretation: ScratchBird columnstore is the most capability-rich Lane B runtime, but it is also the least integrated contractually.

### 2.5 Statistics and planner baseline

ScratchBird already has generic table and column statistics, selectivity estimation, and cost-model scaffolding. What is missing is family-specific observability:

- BRIN has no planner-visible range density, range false-positive rate, or correlation metric.
- Bitmap has no planner-visible posting density, container compression ratio, or churn metric.
- Columnstore has no planner-visible row-group, segment, bytes-per-column, or prune-hit metrics.

As a result, the current optimizer cannot distinguish:

- Lossy summary pruning versus exact row lookup.
- Stored bitmap index retrieval versus execution-time bitmap combination.
- Columnar projection savings versus row-store reconstruction cost.

### 2.6 Baseline conclusion

ScratchBird does not have a "missing implementation" problem for Lane B. It has a taxonomy, contract, and costing problem:

- BRIN exists but is not specified as a lossy summary path.
- Bitmap exists but is planner-shadowed by a synthetic operator of the same general name.
- Columnstore exists but is not yet a single, planner-addressable storage family.

## 3. Donor-engine research synthesis

### 3.1 PostgreSQL: BRIN and bitmap execution

PostgreSQL provides the clearest donor pattern for two separate concepts that ScratchBird currently blurs:

- BRIN is a lossy block-range summary index whose value depends on physical correlation and `pages_per_range`.
- Bitmap execution is an executor-time candidate-set mechanism that can combine multiple conventional indexes, degrade from exact to lossy under memory pressure, and requires heap recheck when exactness is not guaranteed.

Relevant donor lessons:

- Keep BRIN explicitly lossy and cheap to maintain.
- Expose `pages_per_range` as a first-class precision versus footprint knob.
- Treat unsummarized ranges as a real maintenance state, not an edge case.
- Separate storage bitmap semantics from executor bitmap semantics.
- When bitmap memory pressure rises, degrade representation, not correctness.

ScratchBird implication:

- Keep `BitmapIndex` and planner bitmap combination as distinct path families.
- Define `BRIN_SCAN` as a summary path that always advertises recheck capability.
- Add a memory-budgeted exact-to-lossy conversion policy for execution-time bitmaps, while keeping storage bitmap visibility rules independent.

### 3.2 Cassandra SAI: storage-attached immutable generation publication

Cassandra SAI is useful less as a direct structure donor and more as a lifecycle donor:

- Index artifacts are storage-attached to immutable base-data components.
- Shared row/token/partition mappings reduce duplicated locator state.
- Flush and compaction produce new index generations off to the side.
- Publication is atomic at the generation boundary.
- Query execution still performs final row filtering because storage-level summaries and postings are not full truth for tombstones or non-indexed predicates.

ScratchBird implication:

- Lane B structures should publish immutable sealed generations.
- Shared row-location metadata should be standardized where multiple structures need it.
- Final visibility and predicate recheck remains mandatory unless the plan node can prove exactness under the snapshot.

### 3.3 ClickHouse: skip indexes and granule-level columnar pruning

ClickHouse demonstrates the value of keeping summary behavior physically close to columnar storage:

- Data skipping is performed over granules rather than row-by-row structures.
- Min-max summaries and small set summaries are attached to storage layout rather than treated as generic secondary indexes.
- Readers operate over a current multi-version set of parts.
- Marks and granules turn skip decisions into direct read avoidance.

ScratchBird implication:

- Columnstore pruning should be modeled around row groups or segments, not around per-row secondary semantics.
- BRIN and zonemap behavior should share a summary-family costing model where possible.
- Columnstore should expose metadata sufficient to estimate bytes avoided, not only rows avoided.

### 3.4 DuckDB: zonemap pruning tied to row groups and segment statistics

DuckDB reinforces a related but simpler lesson:

- Zonemap decisions are bound to per-segment and row-group statistics.
- Filters consult statistics early and can suppress whole scan regions before tuple materialization.
- Compression, scan vectors, and pruning are integrated rather than split across unrelated secondary access methods.

ScratchBird implication:

- The columnstore path should own row-group metadata, pruning checks, and compression-aware scan costing.
- A zonemap or BRIN-like summary path can share formulas with columnstore pruning, but the contract must stay explicit about whether the structure is storage-native or secondary.

### 3.5 Cross-donor synthesis

Across donors, five principles recur:

1. Lossy summary structures win only when the system can measure and bound false positives.
2. Bitmap structures need exact and lossy modes, but the exactness contract must be explicit to the executor.
3. Columnar pruning is fundamentally a bytes-avoided optimization, not merely a rows-avoided optimization.
4. Immutable generation publication is easier to reason about than in-place mutation for complex secondary artifacts.
5. Final visibility authority remains with the transactional snapshot, not with summary metadata.

## 4. Primary literature and official-document synthesis

### 4.1 Bitmap literature

The bitmap literature consistently treats design as a balance between encoding density, compression, and execution-time combination cost:

- Chan and Ioannidis show that bitmap performance depends on encoding choice, cardinality, and compression rather than on a single universal bitmap layout.
- Roaring-style work shows that hybrid container selection across sparse, dense, and run-heavy regions is a practical win because real distributions are not stationary.

ScratchBird implication:

- The current roaring-like direction is sound.
- The spec should require container-level adaptivity and planner-visible compression metrics.
- Bitmap is strongest for equality and `IN` predicates over low- to medium-cardinality dimensions, plus set combination of multiple predicates.

### 4.2 Column-store literature

Column-store literature converges on three principles:

- Storage layout should align with narrow projection and scan-heavy workloads.
- Compression is not just a space optimization; it must be integrated with scan execution.
- Mutable write paths and read-optimized segments should be separated by generation or compaction boundaries.

The C-Store line of work is especially relevant:

- Read-optimized projections and write-optimized delta behavior are separated.
- Publication between states is explicit rather than implicit.
- Late materialization matters because projecting fewer columns is the main win.

ScratchBird implication:

- Columnstore should not be specified as "another exact index."
- The contract needs explicit row-group, segment, projection, and reconstruction semantics.
- Compression policy should be chosen as part of scan cost and maintenance policy, not only as a storage option.

### 4.3 Summary-index and official-document synthesis

Official documentation from mature engines reinforces the same lane split:

- PostgreSQL BRIN is intentionally lossy and correlation-sensitive.
- PostgreSQL bitmap heap scans combine exact and lossy page knowledge with explicit recheck.
- ClickHouse skip indexes and DuckDB zonemaps are storage-adjacent summaries used to suppress unnecessary reads.
- Cassandra SAI proves the operational value of immutable index generations and atomic publication.

The literature and official docs therefore point to a shared conclusion for ScratchBird:

- BRIN and zonemap belong to a summary family.
- Bitmap belongs to a posting or candidate-set family.
- Columnstore belongs to a segment- and projection-oriented scan family.

Treating all three as interchangeable "indexes" would produce a weak contract and poor costing.

## 5. MGA and lifecycle correctness packet

### 5.1 Lane-wide invariants

All Lane B structures should inherit the same hard rules:

- Index or segment metadata may nominate candidates, but snapshot visibility remains authoritative.
- Any path that cannot prove exact row-version visibility must advertise `recheck_required`.
- Publication must be generation-based: build privately, validate, publish atomically, reclaim later.
- Reclaim is gated by the oldest active snapshot horizon, not by local convenience.

Recommended lifecycle states:

- `BUILDING`: new summary, bitmap, or segment artifact is not visible to readers.
- `SEALED`: artifact is complete and internally validated.
- `PUBLISHED`: catalog or manifest pointer exposes the generation to new readers.
- `OBSOLETE`: superseded by a newer published generation but still visible to old snapshots.
- `RECLAIMABLE`: older than the global safe horizon.

### 5.2 BRIN-specific correctness

BRIN rules:

- BRIN summaries are always candidates, never final truth.
- A range may be summarized, unsummarized, or stale; these must be explicit states.
- Dead-row cleanup must prefer `resummarize_from_heap(range)` over `tombstone_range(range)` unless the full range is provably empty.
- Unsummarized tail ranges must not silently disappear from execution. The executor must either scan them directly or force synchronous summarization.

Recommended BRIN maintenance operations:

- `summarize_range`
- `resummarize_range`
- `summarize_tail`
- `drop_empty_range` only when emptiness is proven against the authoritative base storage

### 5.3 Bitmap-specific correctness

Bitmap rules:

- Stored postings may be exact for row identifiers while still requiring snapshot visibility checks.
- Logical deletion is correct; physical removal is safe only when the posting is globally dead.
- Executor-time lossy conversion is acceptable under memory pressure, but it must convert to "candidate pages with recheck," not to silent omission.
- Bitmap intersection and union must preserve MGA semantics by carrying forward candidate row identifiers or page summaries plus the recheck flag.

Recommended bitmap modes:

- `EXACT_ROWID`
- `EXACT_PAGE_OFFSETS`
- `LOSSY_PAGE`

Only the first two can ever claim "no structural false positives." Even then, visibility may still require recheck.

### 5.4 Columnstore-specific correctness

Columnstore rules:

- Columnstore segments should be immutable once sealed.
- Buffered writes land in a mutable delta or write buffer and are folded into sealed segments by flush or compaction.
- A published generation must expose row-group metadata, visibility metadata, and locator metadata as one coherent manifest.
- If reconstruction reads the row store or a row-version side structure, that dependency is part of the contract, not an implementation detail.

Recommended publication rule:

- Publish the columnstore generation only after row counts, segment boundaries, min/max metadata, and locator maps are validated as a self-consistent set.

### 5.5 MGA-safe publish formulae

The spec should carry simple lifecycle formulae:

```text
safe_reclaim_epoch = min(active_snapshot_epochs)
generation_reclaimable(g) iff g.publish_epoch < safe_reclaim_epoch
```

For BRIN resummarization:

```text
range_summary_exactness = false
range_summary_fresh(g, range) iff g.base_version >= range.last_heap_rewrite_version
```

For bitmap cleanup:

```text
posting_removable(p) iff p.xmax is globally_dead and no active snapshot can still see p
```

These are contract implications, not merely implementation notes.

## 6. Optimizer metrics packet

### 6.1 BRIN metrics

Required BRIN planner metrics:

| Metric | Meaning | Why it matters |
| --- | --- | --- |
| `pages_per_range` | Heap pages summarized per BRIN range | Main precision versus footprint knob |
| `total_ranges` | `ceil(table_pages / pages_per_range)` | Scan search space |
| `summarized_fraction` | Summarized ranges / total ranges | Tail risk and maintenance debt |
| `order_correlation` | Correlation between key order and physical order | Main predictor of usefulness |
| `range_false_positive_rate` | Rechecked rows or pages / candidate rows or pages | Costing and plan stability |
| `avg_rows_per_range` | Average visible rows covered by one range | Recheck CPU term |

Recommended formulas:

```text
total_ranges = ceil(table_pages / pages_per_range)
corr_eff = clamp(abs(order_correlation), 0.25, 1.0)
ranges_touched = min(total_ranges, ceil(predicate_selectivity * total_ranges / corr_eff))
candidate_pages = ranges_touched * pages_per_range
brin_fp_rate = 1 - qualifying_rows / max(candidate_rows, 1)
```

### 6.2 Bitmap metrics

Required bitmap planner metrics:

| Metric | Meaning | Why it matters |
| --- | --- | --- |
| `distinct_key_count` | Number of indexed logical keys | Equality workload fit |
| `avg_postings_per_key` | Mean posting-list length | Candidate-set size estimate |
| `container_mix` | Fraction of array, bitset, run containers | Compression and CPU behavior |
| `compressed_bytes_per_key` | Mean storage per logical key | Memory and IO cost |
| `update_churn` | Invalidated or replaced postings / total postings over window | Maintenance pressure |
| `bitmap_false_positive_rate` | Candidate rows from lossy mode that fail recheck | Costing and plan risk |

Recommended formulas:

```text
candidate_rows = row_count * equality_or_in_selectivity
exact_bitmap_bytes = candidate_rows * tid_ref_bytes + container_overhead
lossy_bitmap_bytes = ceil(table_pages / 8)
heap_pages_touched = min(table_pages, ceil(candidate_rows / avg_visible_rows_per_page))
bitmap_mode = EXACT if exact_bitmap_bytes <= bitmap_memory_budget else LOSSY
```

### 6.3 Columnstore metrics

Required columnstore planner metrics:

| Metric | Meaning | Why it matters |
| --- | --- | --- |
| `row_group_rows` | Rows per row group or segment group | Main pruning granularity |
| `row_group_count` | Total groups in the object | Scan search space |
| `avg_compressed_bytes[column]` | Compressed bytes per column per row group | Projection costing |
| `segment_prune_hit_rate` | Fraction of groups eliminated by min/max or null stats | Main pruning predictor |
| `compression_ratio[column]` | Uncompressed / compressed bytes | IO and CPU balance |
| `late_materialization_fraction` | Rows that require reconstruction after prune | Reconstruction cost |
| `delta_fraction` | Rows still in mutable buffer or delta store | Write-path penalty |

Recommended formulas:

```text
projection_ratio = bytes_needed_columns / max(bytes_all_columns, 1)
rowgroup_survival = touched_row_groups / max(total_row_groups, 1)
bytes_read = sum(needed_columns * touched_row_groups * avg_compressed_bytes_per_row_group)
late_materialization_rows = qualifying_rows * late_materialization_fraction
```

### 6.4 Confidence and staleness

All Lane B stats should also expose:

- `sample_epoch`
- `staleness_rows`
- `staleness_bytes`
- `confidence`

Planner rule:

- If `confidence` falls below threshold or staleness exceeds threshold, cap the path's attractiveness rather than pretending the stale metric is exact.

## 7. Access-path and costing packet

### 7.1 Required plan-node split

ScratchBird should distinguish at least these path families:

- `BRIN_SCAN`
- `BITMAP_STORAGE_SCAN`
- `BITMAP_COMBINE_SCAN`
- `COLUMNSTORE_SCAN`

`BITMAP_COMBINE_SCAN` is the existing synthetic multi-index path. `BITMAP_STORAGE_SCAN` is the runtime bitmap family. They should not share one logical cost function.

### 7.2 BRIN costing

Recommended first-pass BRIN model:

```text
cost_brin =
    brin_probe_io +
    summary_page_reads +
    candidate_pages * heap_page_cost +
    candidate_rows * recheck_cpu_cost +
    unsummarized_tail_penalty
```

Where:

```text
summary_page_reads ~= ceil(ranges_touched / summaries_per_page)
unsummarized_tail_penalty = unsummarized_fraction * seq_scan_cost
```

Heuristics:

- Prefer BRIN when `order_correlation >= 0.6`.
- Strong preference when `range_false_positive_rate <= 0.5`.
- Penalize heavily when `summarized_fraction < 0.9`.
- Do not use BRIN for point lookup unless the table is extremely clustered and the base exact index is absent.

### 7.3 Bitmap costing

Recommended first-pass bitmap-storage model:

```text
cost_bitmap_storage =
    key_probe_cost +
    bitmap_materialization_cpu +
    bitmap_memory_penalty +
    heap_pages_touched * heap_page_cost +
    candidate_rows * visibility_recheck_cpu +
    lossy_recheck_penalty
```

Where:

```text
bitmap_memory_penalty = 0 if exact_bitmap_bytes <= bitmap_memory_budget
bitmap_memory_penalty > 0 otherwise
lossy_recheck_penalty = bitmap_false_positive_rate * candidate_rows * qual_cpu_cost
```

Heuristics:

- Best for equality and small `IN` lists.
- Good for selective conjunctions where multiple bitmap-capable predicates intersect.
- Penalize when `update_churn` is high.
- Penalize when predicate selectivity is so high that bitmap build plus recheck approaches sequential scan cost.

### 7.4 Columnstore costing

Recommended first-pass columnstore model:

```text
cost_columnstore =
    metadata_scan_cost +
    bytes_read / effective_io_bandwidth +
    decompression_cpu +
    late_materialization_rows * reconstruct_cpu_cost +
    delta_merge_penalty
```

Where:

```text
decompression_cpu = sum(decoded_values[column] * decode_cpu_cost[column])
delta_merge_penalty = delta_fraction * row_count * delta_visibility_cpu
```

Heuristics:

- Prefer columnstore when `projection_ratio <= 0.35`.
- Prefer strongly when `rowgroup_survival <= 0.60`.
- Prefer strongly for aggregation and scan-heavy analytics.
- Penalize for high-update OLTP paths until delta and compaction behavior are planner-visible.

### 7.5 Family selection guidance

First-pass routing guidance:

- Use BRIN for physically clustered range predicates over large tables.
- Use bitmap storage for selective equality or `IN` predicates over low- to medium-cardinality columns.
- Use columnstore for narrow-projection scans, aggregations, and pruning-friendly analytic workloads.
- Fall back to sequential scan when the pruning or candidate-set benefit does not exceed recheck and materialization cost.

## 8. ScratchBird contract draft

### 8.1 Taxonomy contract

Proposed contract clauses:

- `BRIN` and `ZONEMAP` are members of the summary-index family.
- `BITMAP` is a posting-index family and is distinct from executor bitmap combination.
- `COLUMNSTORE` is a segment-oriented storage or access family with optional summary metadata and late materialization semantics.

### 8.2 Visibility contract

Proposed contract clauses:

- Every Lane B path returns either exact row identifiers or candidate regions plus `recheck_required`.
- Snapshot visibility remains authoritative unless the path proves an equivalent visibility structure for the active snapshot.
- Returning a row without MGA validation is never allowed.

### 8.3 Publication contract

Proposed contract clauses:

- New Lane B artifacts are built as private generations.
- A generation is publishable only after internal validation of row-count, locator-map, and metadata consistency.
- Publication is a single metadata swap or manifest pointer change.
- Obsolete generations remain readable until the global safe horizon passes.

### 8.4 Maintenance contract

Proposed contract clauses:

- BRIN supports summarize and resummarize operations as first-class maintenance.
- Bitmap supports dead-posting cleanup and container recompression.
- Columnstore supports flush, seal, compact, and rewrite-compression operations.
- All maintenance work surfaces planner-visible freshness metrics.

### 8.5 Statistics contract

Proposed contract clauses:

- Each Lane B family must publish the metrics listed in Section 6.
- Stats refresh may be asynchronous, but stale confidence must be visible to the optimizer.
- Costing code may not silently reuse B-tree assumptions for Lane B families.

### 8.6 Current-state implementation implications

Immediate implications for the current codebase:

- Unify `ColumnstoreIndex` and `ColumnstoreIndexSimple` behind one published contract.
- Rename or otherwise disambiguate planner-facing bitmap-combine logic from the runtime `BitmapIndex` family.
- Rework BRIN dead-entry handling toward resummarization-first maintenance.
- Add lane-family-specific stats objects before enabling plan selection.

## 9. Validation and benchmark packet

### 9.1 Correctness validation

Required correctness suites:

- BRIN snapshot recheck with inserts, deletes, and resummarization across old and new snapshots.
- Bitmap visibility with exact and lossy execution modes, including dead-posting cleanup.
- Columnstore publication with concurrent readers over old and new generations.
- Cross-family tests that confirm all three return the same visible rows as a sequential scan.

### 9.2 BRIN benchmark packet

Benchmark axes:

- Clustered versus unclustered physical order.
- Small versus large `pages_per_range`.
- Freshly summarized versus stale or unsummarized tail.
- Range predicates with selectivity from `0.1%` to `50%`.

Key measures:

- `ranges_touched / total_ranges`
- `candidate_pages / qualifying_pages`
- `recheck_rows / output_rows`
- end-to-end latency versus sequential scan and B-tree baseline

### 9.3 Bitmap benchmark packet

Benchmark axes:

- Cardinality from very low to high.
- Equality and `IN` predicates.
- Single predicate, conjunction, and disjunction.
- Low churn versus high churn write workload.
- Exact-memory fit versus forced lossy execution.

Key measures:

- bitmap build time
- memory footprint
- heap pages touched
- false-positive rate under lossy mode
- latency versus sequential scan and synthetic bitmap-combine path

### 9.4 Columnstore benchmark packet

Benchmark axes:

- Wide versus narrow projection.
- Strong versus weak segment pruning.
- Aggregation-heavy versus row-returning queries.
- Compression mode sensitivity.
- Delta fraction from `0%` to operationally hot ranges.

Key measures:

- bytes read versus full row-store scan
- row groups touched versus total
- decompression CPU
- reconstruction CPU
- end-to-end latency versus row-store scan

### 9.5 Acceptance heuristics for first enablement

Suggested minimum gates before planner enablement:

- BRIN: demonstrated benefit on clustered workloads with bounded false-positive rate.
- Bitmap: exact or lossy mode must remain row-correct under MGA and outperform seq scan on the intended selectivity band.
- Columnstore: measurable byte reduction and stable correctness across generation publication and late materialization.

## 10. Adopt/adapt/reject/defer matrix

| Item | Decision | Reason and contract impact |
| --- | --- | --- |
| PostgreSQL-style lossy BRIN with mandatory recheck | Adopt | Matches ScratchBird BRIN runtime shape and MGA rules |
| `pages_per_range` as primary BRIN tuning knob | Adopt | Needed for sizing and costing |
| BRIN unsummarized-range state as explicit maintenance state | Adopt | Required for correctness and planner penalties |
| PostgreSQL-style executor bitmap exact-to-lossy downgrade under memory pressure | Adapt | Keep for execution-time bitmap paths, but separate from stored bitmap family semantics |
| Shared name or contract for stored bitmap and synthetic bitmap-combine paths | Reject | Hides materially different lifecycle and costing behavior |
| Cassandra-style immutable generation publication | Adopt | Best fit for MGA-safe publication of Lane B artifacts |
| Cassandra-style shared locator metadata across attached structures | Adapt | Useful, but only after ScratchBird settles row-identifier strategy |
| ClickHouse-style granule or row-group pruning semantics | Adopt | Direct fit for columnstore scan planning |
| ClickHouse-style full projection subsystem in first Lane B delivery | Defer | Valuable, but too large before core columnstore contract is unified |
| DuckDB-style early zonemap pruning from segment statistics | Adopt | Strong fit for columnstore and zonemap summary logic |
| Blind BRIN dead-range tombstoning as normal maintenance | Reject | Conflicts with summary-family correctness |
| Full Cassandra token and partition routing semantics | Reject | Not aligned with ScratchBird storage model |
| Shipping two overlapping columnstore implementations | Reject | Creates contract ambiguity |
| Multi-opclass or advanced BRIN summary variants on day one | Defer | Useful later, but min-max style summaries are enough for first contract |

## 11. Open questions and integration dependencies

Open questions:

- Does ScratchBird want BRIN to support synchronous tail summarization, explicit unsummarized tail scan, or both?
- Are bitmap postings keyed by stable row IDs, physical TIDs, or a hybrid indirection layer?
- Is columnstore a secondary structure over row storage, an alternate primary storage mode, or both?
- Which visibility metadata is sufficient to avoid redundant heap lookups for columnstore under MGA?
- What memory budget interface should govern exact-to-lossy bitmap conversion?

Integration dependencies:

- Catalog support for Lane B generation metadata and publication epochs.
- Optimizer support for family-specific statistics and new path node kinds.
- Executor support for `recheck_required`, late materialization, and candidate-region scans.
- Garbage-collection horizon interface that can answer "globally dead" for postings and obsolete generations.
- A single authoritative columnstore runtime contract.

## 12. Recommended next-step specification tasks

1. Draft a Lane B path taxonomy spec that formally separates `BRIN_SCAN`, `BITMAP_STORAGE_SCAN`, `BITMAP_COMBINE_SCAN`, and `COLUMNSTORE_SCAN`.
2. Draft a BRIN lifecycle addendum covering summarized, unsummarized, stale, and resummarized range states.
3. Draft a bitmap storage spec that defines row-identifier semantics, exact versus lossy modes, and MGA-safe cleanup.
4. Draft a columnstore unification spec that chooses one runtime contract and one publication model.
5. Draft a Lane B metrics and costing spec that adds the Section 6 and Section 7 fields to optimizer statistics.
6. Draft a Lane B executor contract for `recheck_required`, candidate-region scans, and late materialization.
7. Draft a validation spec with the benchmark packets and enablement gates from Section 9.

Recommended execution order:

- Taxonomy and lifecycle first.
- Metrics and costing second.
- Columnstore unification and bitmap storage contract third.
- Planner and benchmark gating last.

Bottom line:

ScratchBird can support Lane B without inventing new theory. The codebase already contains the core runtime pieces. The missing work is to make summary, bitmap, and columnar families explicit in the contract, publish them safely under MGA, and give the optimizer enough family-specific evidence to choose them for the workloads where they actually win.
