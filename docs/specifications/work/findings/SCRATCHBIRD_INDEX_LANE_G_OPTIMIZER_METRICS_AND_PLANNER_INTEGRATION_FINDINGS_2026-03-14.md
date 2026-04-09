# ScratchBird Index Lane G: Optimizer Metrics, Costing, and Planner Integration Findings

Status: first pass  
Date: 2026-03-14

## 1. Scope and lane objective

Lane G owns the optimizer-facing contract for every ScratchBird index family. The target is not a storage-format survey. The target is a planner contract that answers four questions consistently across families:

1. When is an index family applicable?
2. What planner metrics must be available before the family can compete fairly?
3. How should the family be costed, including lossy, ranked, and approximate paths?
4. What planner and plan-payload surface must exist so the choice is explainable, testable, and MGA-correct?

This lane covers all canonical planner families, with catalog aliases lowered into one of them before costing:

- Ordered exact and range: `BTREE`, `ART`-like ordered trie forms, `HASH`, `LSM`
- Summary and candidate families: `BRIN`, `ZONEMAP`, `BLOOM`, `BITMAP`
- Columnar access families: `COLUMNSTORE`
- Generalized and spatial families: `GIST`, `SPGIST`, `RTREE`
- Inverted and text families: `GIN`, `INVERTED`, `FULLTEXT`, `NGRAM`, text/search aliases
- Vector and ANN families: `HNSW`, `IVF`, vector aliases

This lane does not try to finish page-layout specifications, operator-class catalogs, or full calibration implementation. It defines the contract those later specs must satisfy.

Primary inputs for this pass were ScratchBird planner, cost-model, statistics, catalog, and index code; the completed Batch 1 index findings; the earlier optimizer findings on memo architecture, statistics, cardinality, access-path enumeration, cost modeling, and observability; and targeted donor-engine reads from PostgreSQL, MySQL, DuckDB, ClickHouse, Cassandra, Milvus, and OpenSearch.

## 2. ScratchBird current-state baseline

ScratchBird already has broad index-family registration in catalog and factory code, but planner integration is still narrow.

Current baseline from code:

- The catalog exposes a wide `IndexType` taxonomy and a generic `IndexStatsCatalogInfo` envelope with row count, distinct count, null fraction, histogram and MCV handles, average key and entry sizes, leaf pages, height, clustering factor, correlation, and bloat ratio.
- Some runtimes already expose family-specific statistics internally:
  - hash: bucket count, overflow pages, load factor
  - BRIN: total ranges and average range selectivity
  - SP-GiST: depth and leaf density
  - GIN: posting-list and pending-list counters
  - HNSW: node count, deleted node count, max layer, average connections
  - LSM: memtable size and per-level SSTable counts
- Those family-specific statistics are not yet surfaced into a planner-visible, family-typed metrics packet.

Current planner behavior is materially narrower than the catalog:

- Base access enumeration still collapses each relation to a single best access path too early.
- The main competing row-returning paths are `SEQ_SCAN`, generic `INDEX_SCAN`, `INDEX_ONLY_SCAN`, `LSM_SCAN`, heuristic `SKIP_SCAN`, and a synthetic `BITMAP_INDEX_SCAN`.
- `SKIP_SCAN` and the synthetic `BITMAP_INDEX_SCAN` are restricted to matched `BTREE` predicates.
- `INDEX_SCAN` and `INDEX_ONLY_SCAN` use generic cost formulas with hard-coded structural assumptions such as index height `3`.
- `LSM_SCAN` uses a dedicated cost formula but still receives hard-coded `num_levels = 3` and `avg_sstables_per_level = 2`.
- Ordering support is effectively `BTREE`-only because planner ordering keys are derived only for `BTREE`.
- Lateral parameterization is effectively gated on the presence of `BTREE` or `LSM`.
- `BITMAP_INDEX_SCAN` does not represent the standalone bitmap family runtime. It is a planner-side combination of multiple matched `BTREE` predicates.
- `path.h` already defines `RTREE_SCAN`, but the base access enumerator does not currently produce first-class R-tree paths.

The positive baseline is that planner observability is already stronger than the actual path taxonomy:

- Runtime relation payload already records `scan_kind`, `scan_family`, `scan_family_tags`, `candidate_scan_families`, `covering_index`, `ordered_prefix_length`, `required_outer_relation_indexes`, `startup_cost`, `total_cost`, and `estimated_rows`.
- Structured access-path traces already exist and can carry richer family-specific evidence once the taxonomy is fixed.

Immediate baseline conclusion:

- ScratchBird already has enough planner plumbing to explain rich index choices.
- ScratchBird does not yet have the family-aware metrics, path taxonomy, or frontier-preserving enumeration needed to make those choices correctly across all index families.

## 3. Donor-engine research synthesis

### PostgreSQL

PostgreSQL contributes the clearest model for family-specific costing and applicability:

- A generic index-cost framework is combined with access-method-specific `amcostestimate` logic instead of one monolithic index formula.
- Partial-index predicates are folded into selectivity reasoning only when they are not already implied by query quals.
- B-tree costing treats boundary quals differently from residual quals, accounts for repeated descents in array and skip-scan style cases, and uses ordering correlation as a first-class heap-fetch locality signal.
- Bitmap and BRIN paths are explicitly lossy and recheck-aware, not mislabeled as exact index scans.
- GiST, SP-GiST, GIN, BRIN, hash, and B-tree all own separate cost hooks.

Lane G implication:

- ScratchBird should adopt access-method-specific planner packets and cost hooks, not add more special cases to a generic `INDEX_SCAN`.

### MySQL

MySQL contributes a concrete access-path taxonomy and practical selectivity heuristics:

- Distinct path kinds are exposed for `REF`, `EQ_REF`, `INDEX_RANGE_SCAN`, `INDEX_MERGE`, `ROWID_INTERSECTION`, `ROWID_UNION`, `INDEX_SKIP_SCAN`, `GROUP_INDEX_SKIP_SCAN`, `MRR`, and `FULL_TEXT_SEARCH`.
- Skip-scan cardinality is driven from `records_per_key` distinct-group estimates and range filtering on the suffix key part.
- The `index_merge` implementation documents important search-space limitations instead of pretending the path is universally explored.

Lane G implication:

- ScratchBird should expose family- and shape-specific paths directly and be explicit about search-space cutoffs, especially for skip, merge, and combined candidate paths.

### DuckDB

DuckDB contributes two useful planning patterns:

- Row-group and segment-level pruning is treated as a first-class access reduction mechanism driven by zonemap-style statistics.
- ART index scans are gated by a simple threshold rule: only use the index if estimated matches stay below `max(index_scan_max_count, index_scan_percentage * total_rows)`.

Lane G implication:

- ScratchBird should keep summary pruning and exact row-id access separate.
- When family metrics are incomplete, a bounded threshold heuristic is better than pretending to have precise costing.

### ClickHouse

ClickHouse contributes two strong ideas:

- Skipping indexes and sparse primary indexes are modeled as granule-pruning mechanisms whose value is measured by granules dropped, not by exact tuple retrieval.
- Vector similarity indexes are local to index blocks, use a candidate-budget knob analogous to `ef_search`, and require rescoring because row-oriented ANN results are expanded back to granule reads.

Lane G implication:

- ScratchBird ANN paths need explicit candidate-budget, rescoring, and granularity-mismatch metrics.
- Summary and columnar paths should be costed in terms of bytes, segments, and granules pruned, not only rows returned.

### Cassandra SAI

Cassandra storage-attached indexing contributes operational shape more than classical optimizer math:

- Index updates are synchronous with mutations.
- Incremental builds are explicit.
- Tracing, metrics, and virtual metadata are first-class.
- Query result ordering is token-ordered rather than pretending to be arbitrary relation order.

Lane G implication:

- ScratchBird should treat queryability state, publication state, and maintenance debt as planner-visible metadata, not just operator-facing diagnostics.

### Milvus

Milvus contributes segment-local ANN lifecycle rules:

- Index build is asynchronous and tracked per segment.
- Query freshness is governed by timestamp and service-time style visibility rules.
- Snapshots naturally separate sealed and growing segments.
- ANN families expose search-budget knobs such as `nprobe` and `ef`.

Lane G implication:

- ScratchBird ANN paths need planner-visible `coverage_fraction`, build state, delete backlog, and candidate-budget controls.
- Exact semantics over partial ANN coverage require either hybrid fallback or path rejection.

### OpenSearch

OpenSearch contributes practical text-ranking defaults and observability:

- BM25 defaults remain `k1 = 1.2` and `b = 0.75`.
- Explain and profile surfaces are standard, not optional extras.
- Text scoring depends on corpus statistics such as document frequency and document length normalization.

Lane G implication:

- ScratchBird text paths should cost ranking separately from boolean candidate production and must expose enough plan payload to explain score-oriented path selection.

## 4. Primary literature and official-document synthesis

The primary literature and official documentation converge on a small set of durable laws that Lane G should adopt as specification text.

### 4.1 Cost-model law

The Selinger line remains the baseline: access-path selection is a trade between startup work, run work, cardinality, and downstream plan effects. For ScratchBird this means every path must provide:

- `startup_cost`
- `run_cost`
- `total_cost = startup_cost + run_cost`
- `rows_out_est`
- `interesting_order`
- `exactness_class`

### 4.2 Cache-locality law

The Mackert-Lohman style page-reuse idea, as used in mature optimizers, matters because repeated index probes are not independent random I/O events. ScratchBird should therefore preserve:

- a first-pass locality signal from correlation, clustering, or segment contiguity
- a repeated-probe discount for nested-loop rescans, array probes, and skip scans

### 4.3 Summary-index law

BRIN, zonemap, bloom, and skip-index literature agree on a one-sided guarantee:

- false positives are acceptable
- false negatives are not

Therefore summary-family paths are not exact row-returning scans. They are candidate-producing or prune-producing paths with mandatory recheck or fallback semantics.

### 4.4 Generalized-tree law

R-tree, GiST, and SP-GiST literature converge on four planner-relevant rules:

- internal summaries or partitions must be conservative
- branch pruning must be one-sided safe
- overlap and branch skew are first-order cost drivers
- ordered nearest search is legal only when internal distance is a true lower bound

### 4.5 Inverted and ranking law

Generalized inverted-index and probabilistic retrieval literature converge on a two-stage model:

- produce candidate document IDs from postings or token structures
- optionally score and rerank those candidates

A stable BM25 packet for first-pass planning is:

```text
idf(t) = ln(1 + (N - df_t + 0.5) / (df_t + 0.5))
tf_norm(t, d) = tf_t * (k1 + 1) / (tf_t + k1 * (1 - b + b * dl / avgdl))
score(d, q) = sum over query terms of idf(t) * tf_norm(t, d)
```

Planner consequence: boolean text access and ranked text access are different path families, even when they share the same physical index.

### 4.6 Write-optimized structure law

LSM literature converges on the tradeoff that matters most to the planner:

- write cost drops
- point and range reads pay read amplification, merge CPU, tombstone filtering, and compaction debt

ScratchBird should preserve that distinction explicitly instead of treating `LSM` as just another generic index scan.

### 4.7 Approximate-nearest-neighbor law

HNSW and IVF documentation and literature agree that ANN access is governed by candidate-budget knobs:

- graph families: `ef_search`
- inverted-list families: `nprobe`

Quality and cost move together. Therefore ANN planner packets must include both a cost estimate and a quality target or quality proxy.

## 5. MGA and lifecycle correctness packet

Lane G cannot separate costing from lifecycle correctness because the planner must never cost or choose a path that is not snapshot-safe or fully published.

### 5.1 Required lifecycle state model

Every index instance should expose one lifecycle state from the planner point of view:

- `BUILDING`
- `QUERYABLE`
- `STALE`
- `MERGING`
- `RETIRED`
- `FAILED`

For segment-local or part-local families, the state must be paired with:

- `coverage_fraction = queryable_logical_rows / total_logical_rows`
- `coverage_granules`
- `stale_fraction`

### 5.2 MGA laws for all families

Required laws:

- Publish-after-complete: an index or segment becomes `QUERYABLE` only after all structures needed for snapshot-safe reads are durable and visible.
- Retire-after-drain: an old structure may move to `RETIRED` only after all snapshots that can still observe it have drained.
- No hidden visibility filter: if a family cannot enforce `xmin` and `xmax` visibility during access, the planner must mark the path as requiring post-access visibility recheck.
- No partial exactness: a partially built or partially queryable structure cannot advertise exact full-table coverage.
- No maintenance-induced false negatives: summarize, compaction, merge, or rebuild work may increase false positives or cost, but cannot silently remove visible matches.

### 5.3 Current correctness risks that matter to Lane G

The earlier Batch 1 findings exposed two current correctness hazards that should be treated as hard planning guardrails:

- the current generic inverted runtime ignores `current_xid` in search and remove paths, so it cannot be treated as an unrestricted MGA-exact family
- the dedicated R-tree compaction behavior appears too blunt to qualify as MGA-safe online maintenance

Lane G implication:

- until those are repaired, affected paths must either be rejected for MGA-sensitive execution or marked as requiring conservative fallback behavior

### 5.4 Planner lifecycle contract

The planner contract should be:

- `BUILDING`: not directly enumerable unless a hybrid indexed-plus-fallback path is defined
- `QUERYABLE`: enumerable normally
- `STALE`: enumerable with a maintenance penalty multiplier and explicit payload note
- `MERGING`: enumerable only if publication rules guarantee complete visible coverage during merge
- `RETIRED`: never chosen for new scans
- `FAILED`: never chosen

For exact semantics over partially queryable segment-local families:

```text
if exact_query and coverage_fraction < 1:
    require HYBRID_INDEX_PLUS_SCAN or reject family
if approximate_query and coverage_fraction < 1:
    allow family only if uncovered_fraction is documented in payload
```

### 5.5 Family-specific lifecycle packet

- Ordered exact families:
  - must guarantee duplicate suppression, delete visibility, and stable ordering across snapshots
- Summary and candidate families:
  - must guarantee conservative summaries and mandatory recheck semantics
- Columnstore:
  - must guarantee segment pruning is one-sided safe and deleted-row tracking is snapshot-safe
- Generalized and spatial families:
  - must guarantee conservative summaries or partitions and MGA-safe maintenance
- Inverted and text families:
  - must guarantee postings visibility or document-level visibility recheck
- ANN families:
  - must expose queryable segments, deleted-node fraction, and whether exact rescore is required for final semantics

## 6. Optimizer metrics packet

Lane G needs a family-neutral core packet plus family-specific extensions.

### 6.1 Core planner packet

Every enumerable index path should expose:

- `family`
- `metrics_version`
- `exactness_class`: `EXACT`, `LOSSY_RECHECK`, `APPROXIMATE`, `APPROXIMATE_RECHECK`, `FILTER_ONLY`
- `queryability_state`
- `coverage_fraction`
- `rows_true_est`
- `rows_candidate_est`
- `false_positive_ratio`
- `recheck_ratio`
- `supports_ordering`
- `ordering_class`: `NONE`, `PREFIX`, `FULL`, `DISTANCE_LOWER_BOUND`
- `supports_index_only`
- `supports_parameterization`
- `cost_confidence`: `HIGH`, `MEDIUM`, `LOW`
- `startup_penalty_multiplier`
- `run_penalty_multiplier`

Recommended first-pass cross-family formulas:

```text
rows_true_est = N * s_true
rows_candidate_est = rows_true_est / max(1 - fp, eps)
recheck_rows_est = rows_candidate_est * recheck_ratio
heap_rows_est = rows_candidate_est * heap_touch_fraction
coverage_fraction = queryable_rows / max(total_rows, 1)
```

Where:

- `N` is estimated visible base rows
- `s_true` is estimated true predicate selectivity
- `fp` is false-positive ratio
- `eps` is a small floor such as `1e-9`

### 6.2 Confidence and bounded pessimism

Missing metrics should not force a binary use-or-ban rule for every family. First pass:

- `HIGH`: use normal cost
- `MEDIUM`: multiply startup and run by `1.10` to `1.20`
- `LOW`: multiply by `1.25` for exact families, `1.35` for lossy families, `1.50` for ranking or ANN families

This is intentionally simple. It is better than pretending uncertain family metrics are precise.

### 6.3 Ordered exact family metrics

Required metrics:

- `height`
- `leaf_pages`
- `avg_entries_per_leaf`
- `correlation`
- `clustering_factor`
- `ndv_prefix[k]` for leading key prefixes
- `overflow_ratio` for hash-like families
- `memtable_rows`, `immutable_rows`, `sstables_per_level`, `overlap_factor`, `bloom_fp_ratio`, `tombstone_ratio`, and `merge_fanin` for `LSM`

Recommended heuristics:

- `exact_key_lookup` should be a planner-visible boolean, not a B-tree-only trace hint
- `ordered_prefix_length` should be family-derived, not implied only by `BTREE`
- `LSM` should advertise `supports_ordering = true` only if merge order and duplicate suppression preserve a stable comparator order

### 6.4 Summary, bitmap, and columnar metrics

Required metrics:

- `pages_per_range` or `rows_per_range`
- `range_count`
- `unsummarized_fraction`
- `summary_fp_ratio`
- `bytes_pruned_ratio`
- `segments_pruned_ratio`
- `bitmap_density`
- `bitmap_run_fraction`
- `bitmap_combine_arity`
- `projection_fraction`
- `late_materialization_fraction`

Useful formulas:

```text
candidate_ranges = total_ranges * summary_selectivity
rows_candidate_est = candidate_ranges * rows_per_range
bytes_scanned_est = total_bytes * (1 - bytes_pruned_ratio) * projection_fraction
bitmap_words_touched = ceil(bitmap_cardinality / word_bits)
```

For Bloom-like structures used as pruning filters:

```text
fp_bloom ~= (1 - e^(-k * n / m))^k
```

Where `k` is the number of hash functions, `n` is inserted keys, and `m` is filter bits.

### 6.5 Generalized and spatial metrics

Required metrics:

- `avg_depth`
- `coverage_ratio`
- `pair_overlap_ratio`
- `penalty_mean`
- `branch_skew`
- `all_the_same_fraction`
- `recheck_ratio`
- `lower_bound_tightness` for nearest paths

Useful formulas:

```text
coverage_ratio = sum(volume(child_summary_i)) / max(volume(parent_summary), eps)
pair_overlap_ratio = sum(volume(intersection(child_i, child_j))) / max(volume(parent_summary), eps)
branch_skew = max(branch_hits) / max(avg(branch_hits), 1)
```

### 6.6 Inverted, text, and ranking metrics

Required metrics:

- `term_df`
- `avg_posting_len`
- `posting_tree_fraction`
- `pending_backlog_ratio`
- `position_density`
- `avg_doc_len`
- `corpus_doc_count`
- `score_cpu_per_candidate`
- `recheck_ratio` for lossy token extraction or phrase verification

Useful formulas:

```text
candidate_docs_est = boolean_formula(posting_lengths, selectivities, correlations)
pending_backlog_ratio = pending_docs / max(total_docs, 1)
score_rows_est = min(rows_candidate_est, score_budget_rows)
```

### 6.7 Vector and ANN metrics

Required metrics:

- `candidate_budget`
- `candidate_budget_kind`: `EF_SEARCH`, `NPROBE`, `FIXED_K_MULTIPLIER`
- `segment_queryable_fraction`
- `deleted_fraction`
- `avg_graph_degree` or `avg_list_size`
- `rescore_fraction`
- `distance_cpu_per_candidate`
- `memory_resident_fraction`
- `recall_target` or `quality_class`

Useful formulas:

```text
rows_candidate_est = candidate_budget * segment_queryable_fraction
rows_rescore_est = rows_candidate_est * rescore_fraction
effective_k_needed = ceil(k / max(filter_selectivity, eps))
```

Planner heuristic:

- if `rows_candidate_est < effective_k_needed`, either widen the budget or reject the ANN path for filtered exact-top-k semantics

## 7. Access-path and costing packet

### 7.1 Canonical path taxonomy

ScratchBird should stop using `INDEX_SCAN` as the planner's universal abstraction. First-pass canonical row-returning and filter paths should be:

- Exact ordered and exact probe paths:
  - `BTREE_EQ_SCAN`
  - `BTREE_RANGE_SCAN`
  - `BTREE_ORDERED_SCAN`
  - `ART_EQ_SCAN`
  - `HASH_EQ_SCAN`
  - `LSM_EQ_SCAN`
  - `LSM_RANGE_SCAN`
  - `LSM_ORDERED_RANGE_SCAN`
- Summary and candidate paths:
  - `BRIN_SCAN`
  - `ZONEMAP_SCAN`
  - `BLOOM_FILTER_SCAN`
  - `BITMAP_STORAGE_SCAN`
  - `BITMAP_COMBINE_SCAN`
- Columnar paths:
  - `COLUMNSTORE_SCAN`
  - `COLUMNSTORE_LATE_MATERIALIZE_SCAN`
- Generalized and spatial paths:
  - `GIST_SCAN`
  - `SPGIST_SCAN`
  - `RTREE_SCAN`
  - `GIST_KNN_SCAN`
  - `RTREE_NEAREST_SCAN`
- Inverted and text paths:
  - `GIN_FILTER_SCAN`
  - `INVERTED_FILTER_SCAN`
  - `TEXT_SCORE_SCAN`
  - `TEXT_RECHECK_SCAN`
- Vector and ANN paths:
  - `HNSW_KNN_SCAN`
  - `IVF_KNN_SCAN`
  - `VECTOR_POSTFILTER_SCAN`
  - `VECTOR_RESCORE_SCAN`
- Hybrid coverage paths:
  - `HYBRID_INDEX_PLUS_SCAN`
  - `HYBRID_SEGMENT_INDEX_PLUS_SCAN`

### 7.2 General cost skeleton

All path costs should decompose into the same high-level structure:

```text
startup_cost =
    navigation_startup
  + runtime_key_eval
  + candidate_structure_build
  + load_penalty

run_cost =
    index_io_cost
  + summary_or_branch_cpu
  + candidate_merge_cpu
  + recheck_cpu
  + visibility_cpu
  + heap_or_segment_fetch_io
  + score_or_distance_cpu
  + late_materialization_cpu

total_cost = startup_cost + run_cost
```

This decomposition matches the existing optimizer lane cost-model direction and keeps observability practical.

### 7.3 First-pass family formulas

Ordered exact families:

```text
cost_btree =
    height * cpu_cmp
  + leaf_pages_touched * io_index
  + heap_pages_touched(correlation) * io_heap
  + rows_candidate_est * (cpu_tuple + cpu_residual)
```

```text
cost_hash =
    bucket_probe_cpu
  + overflow_pages * io_index
  + rows_candidate_est * (cpu_tuple + cpu_collision_recheck)
  + heap_pages_touched * io_heap
```

```text
cost_lsm_point =
    memtable_probe_cpu
  + immutable_probe_cpu
  + bloom_checks * cpu_bloom
  + bloom_positive_runs * io_index
  + duplicate_suppression_cpu
  + heap_pages_touched * io_heap
```

```text
cost_lsm_range =
    seek_count * cpu_seek
  + overlapping_runs_in_span * io_index
  + merge_fanin * cpu_merge
  + tombstone_ratio * rows_candidate_est * cpu_tombstone
  + heap_pages_touched * io_heap
```

Summary and candidate families:

```text
cost_brin_or_zonemap =
    summary_pages * io_seq
  + candidate_ranges * cpu_summary
  + rows_candidate_est * (cpu_recheck + cpu_visibility)
  + heap_pages_touched * io_heap
```

```text
cost_bitmap =
    sum(child_lookup_cost_i)
  + bitmap_words_touched * cpu_bitop
  + heap_pages_touched * io_heap
  + rows_candidate_est * (cpu_visibility + cpu_residual)
```

Columnar families:

```text
cost_columnstore =
    metadata_pages * io_seq
  + bytes_scanned_est / scan_bandwidth
  + rows_decoded_est * cpu_decode
  + rows_materialized_est * cpu_materialize
```

Generalized and spatial families:

```text
cost_gist_spgist_rtree =
    branch_visits * (io_index + cpu_consistent)
  + rows_candidate_est * (cpu_recheck + cpu_visibility)
  + heap_pages_touched * io_heap
```

```text
cost_nearest =
    priority_queue_ops * cpu_pq
  + branch_visits * (io_index + cpu_lower_bound)
  + rows_rescore_est * cpu_exact_distance
```

Inverted and text families:

```text
cost_inverted_boolean =
    posting_pages * io_index
  + posting_merge_ops * cpu_merge
  + rows_candidate_est * (cpu_visibility + cpu_recheck)
  + heap_pages_touched * io_heap
```

```text
cost_text_score =
    cost_inverted_boolean
  + rows_scored_est * cpu_score
  + topk_heap_ops * cpu_topk
```

ANN families:

```text
cost_ann =
    load_penalty
  + candidate_budget * cpu_distance
  + rows_rescore_est * (cpu_exact_distance + cpu_filter)
  + result_heap_ops * cpu_topk
```

### 7.4 Applicability heuristics

Recommended first-pass applicability rules:

- `BTREE_EQ_SCAN`: equality or prefix equality on leading key columns
- `BTREE_RANGE_SCAN`: comparator-defined ranges on leading key
- `BTREE_ORDERED_SCAN`: only when required order is a prefix-compatible order
- `HASH_EQ_SCAN`: full-key equality only
- `LSM_EQ_SCAN`: full-key equality or exact point probe with duplicate suppression
- `LSM_RANGE_SCAN`: comparator-defined ranges; ordered only if merge order is contractually stable
- `BRIN_SCAN` and `ZONEMAP_SCAN`: large-table, physically correlated, low-update or resummarized ranges
- `BLOOM_FILTER_SCAN`: assistive filter only; not a standalone exact path
- `BITMAP_STORAGE_SCAN`: dense, low-cardinality, candidate-oriented predicates
- `BITMAP_COMBINE_SCAN`: multiple selective predicates whose combined bitmap is cheaper than heap filtering
- `COLUMNSTORE_SCAN`: projection-heavy or scan-dominant analytical access with strong bytes-pruning opportunity
- `GIST_SCAN`, `SPGIST_SCAN`, `RTREE_SCAN`: only when operator strategy is defined and conservative pruning is guaranteed
- nearest generalized paths: only when lower-bound distance is proven
- `GIN_FILTER_SCAN` and `INVERTED_FILTER_SCAN`: boolean containment, token membership, or phrase candidate generation
- `TEXT_SCORE_SCAN`: ranking intent is present and corpus statistics exist
- `HNSW_KNN_SCAN` and `IVF_KNN_SCAN`: explicit vector top-k or nearest-neighbor predicate, candidate budget available, and coverage acceptable

### 7.5 Planner frontier requirement

The completed optimizer findings already showed that ScratchBird should not keep only one base path per relation. Lane G sharpens that into a requirement:

- keep a bounded frontier across at least:
  - minimum startup cost
  - minimum total cost
  - best ordering
  - best covering path
  - best lossy candidate path
  - best ranked text path
  - best ANN path
  - best parameterized path

Without this, families like `BRIN`, `BITMAP`, `TEXT_SCORE_SCAN`, `COLUMNSTORE_SCAN`, and `HNSW_KNN_SCAN` are discarded before join, limit, or order-sensitive optimization can use them.

## 8. ScratchBird contract draft

### 8.1 Planner-visible family contract

Every catalog index type must lower into one canonical planner family before enumeration. The lowering must define:

- `planner_family`
- `exactness_class`
- `row_returning` or `filter_only`
- `supports_ordering`
- `supports_covering`
- `supports_parameterization`
- `requires_recheck`
- `family_metrics_schema`
- `fallback_family`

Example lowering rules:

- `BTREE` -> ordered exact family
- `ART` -> exact equality or ordered trie family depending on runtime guarantees
- `HASH` -> exact equality family
- `LSM` -> write-optimized exact family
- `BRIN`, `ZONEMAP` -> summary family
- `BLOOM` -> filter-only summary family
- `BITMAP` -> compressed candidate family
- `COLUMNSTORE` -> columnar family
- `GIST`, `SPGIST`, `RTREE` -> generalized or spatial family
- `GIN`, `INVERTED`, `FULLTEXT`, `NGRAM`, text aliases -> inverted and text family
- `HNSW`, `IVF`, vector aliases -> ANN family

### 8.2 Catalog and statistics contract

The existing generic index-stats envelope is a good base but not enough. The contract should add a typed family-extension payload with versioning:

- `family_metrics_version`
- `family_metrics_type`
- `family_metrics_payload`
- `metrics_last_refresh_xid`
- `metrics_confidence_class`

The generic envelope should keep cross-family fields such as `leaf_pages`, `height`, `correlation`, and `bloat_ratio`, while family-specific payloads carry items like overlap ratios, posting sizes, bitmap density, ANN candidate budgets, or LSM overlap.

### 8.3 Access descriptor contract

`AccessPathDescriptor` and runtime plan payload should add:

- `exactness_class`
- `recheck_required`
- `coverage_fraction`
- `rows_candidate_est`
- `false_positive_ratio`
- `cost_confidence`
- `candidate_budget`
- `family_metrics_version`
- `visibility_enforcement`: `INDEX_NATIVE`, `POST_FILTER`, `HYBRID`

The current `scan_kind` and `scan_family` fields can remain, but they should describe real canonical paths instead of overloaded string labels like generic `INDEX_SCAN`.

### 8.4 Enumeration contract

Enumeration requirements:

- do not infer ordered output only from `BTREE`
- do not collapse all non-`LSM` families into generic `INDEX_SCAN`
- do not model synthetic multi-`BTREE` bitmap combination as the same thing as the standalone bitmap family
- do not enumerate an ANN path without a candidate budget and exactness annotation
- do not enumerate generalized nearest paths without lower-bound certification
- do not enumerate ranking paths without score-cost components and corpus stats

### 8.5 Immediate contract implications for the current codebase

Current code changes implied by this findings packet:

- replace generic hard-coded index structural inputs with family metrics
- extend `IndexStatsCatalogInfo` with typed family packets
- promote `RTREE_SCAN` from latent path type to actual enumerated family when runtime contract exists
- introduce planner-visible first-class paths for `HASH`, `BRIN`, standalone `BITMAP`, `COLUMNSTORE`, `GIST`, `SPGIST`, `RTREE`, `GIN`, text, and ANN families
- split `BITMAP_INDEX_SCAN` into `BITMAP_COMBINE_SCAN` and `BITMAP_STORAGE_SCAN`
- preserve multiple candidate paths per relation instead of one winner

## 9. Validation and benchmark packet

Lane G needs both correctness validation and calibration validation.

### 9.1 Correctness suite

Required correctness suites:

- snapshot-stable point lookup across `BTREE`, `HASH`, `LSM`, and `ART`-like exact families
- summary-family recheck completeness under inserts, deletes, and resummarization
- bitmap candidate completeness and duplicate suppression
- ordered-output correctness under old snapshots, especially for `BTREE` and `LSM`
- generalized-tree conservative pruning and nearest-search lower-bound correctness
- text boolean completeness and ranked-path stability under concurrent updates
- ANN segment queryability and exact fallback behavior under partial coverage
- rebuild and shadow-swap correctness with retained old snapshots

### 9.2 Calibration and access-path choice suite

Recommended benchmark matrix:

- Equality probe matrix:
  - vary selectivity, correlation, cache warmth, and duplicate rate
- Range scan matrix:
  - vary span width, clustering, and output-order usefulness
- Skip-scan matrix:
  - vary distinct leading-key count and suffix selectivity
- Summary matrix:
  - clustered vs random physical layouts, varying `pages_per_range` and unsummarized tail fraction
- Bitmap matrix:
  - sparse vs dense predicates, AND vs OR, low vs high heap locality
- Columnstore matrix:
  - projection width, bytes pruned, late materialization benefit
- Generalized/spatial matrix:
  - overlap pressure, branch skew, nearest-k size, recheck ratio
- Text matrix:
  - boolean filters, phrase queries, top-k ranking, corpus skew, pending backlog
- ANN matrix:
  - `ef_search` or `nprobe`, filter selectivity, deleted-node fraction, recall target, segment coverage

### 9.3 Validation metrics

Recommended calibration metrics:

```text
q_error = max(est, actual) / max(min(est, actual), 1)
winner_error = runtime(best_estimated_path) / runtime(best_actual_path)
recheck_error = abs(est_recheck_ratio - actual_recheck_ratio)
prune_error = abs(est_prune_ratio - actual_prune_ratio)
```

For ANN and ranked text:

- `recall_at_k`
- `latency_at_k`
- `effective_candidate_budget`
- `score_rows_est / actual_score_rows`

### 9.4 Observability requirement

The existing plan payload and runtime traces are a strong base. Validation should require that actual runs emit:

- chosen path family
- rejected family reasons
- estimated vs actual candidate rows
- estimated vs actual recheck ratio
- estimated vs actual bytes or granules pruned
- ANN budget used and rows rescored
- text rows scored and top-k heap size

## 10. Adopt/adapt/reject/defer matrix

| Concept | Donor or source pattern | Decision | Rationale |
| --- | --- | --- | --- |
| Access-method-specific cost hooks | PostgreSQL | Adopt | Best fit for ScratchBird's broad family surface. |
| Partial-index predicate implication before selectivity adjustment | PostgreSQL | Adopt | Necessary for truthful partial-index costing. |
| First-column correlation as heap-locality input | PostgreSQL | Adopt | Matches existing generic correlation use and should be widened. |
| Explicit path taxonomy for skip, merge, and text access | MySQL | Adopt | Better than hiding shapes inside generic `INDEX_SCAN`. |
| Distinct-group skip-scan cardinality from `records_per_key`-style stats | MySQL | Adapt | Needs ScratchBird-specific stats surfaces, but the idea is solid. |
| Row-group and segment pruning as a first-class scan reducer | DuckDB | Adopt | Strong model for summary and columnar families. |
| Fixed threshold gate for exact index scans when metrics are incomplete | DuckDB | Adapt | Useful fallback heuristic, but not a universal policy. |
| Granule-pruning and rescoring model for ANN | ClickHouse | Adapt | Good fit for segment-local vector access, but ScratchBird must express MGA visibility explicitly. |
| Lifecycle tied to immutable parts or segments | ClickHouse and Milvus | Adapt | Strong for ANN and columnar families, but requires shadow-swap and coverage accounting. |
| Queryability state plus metrics and virtual metadata | Cassandra SAI | Adopt | Needed for planner correctness and observability. |
| Asynchronous per-segment ANN build and quality knobs | Milvus | Adopt | Best donor model for vector-family lifecycle. |
| BM25 defaults `k1 = 1.2`, `b = 0.75` | OpenSearch and common search practice | Adopt | Good first-pass ranked text baseline. |
| Monolithic generic `INDEX_SCAN` family for all non-seq paths | Current ScratchBird shape | Reject | Hides exactness, order, and recheck semantics. |
| Treat synthetic multi-B-tree bitmap combination as the same thing as bitmap-family runtime | Current ScratchBird shape | Reject | Planner semantics differ materially. |
| Day-one universal parameterized paths for text, generalized, and ANN families | N/A | Defer | Valuable later, but not needed for first contract. |
| Day-one multivariate stats for every family packet | N/A | Defer | Desirable, but first pass should land family packets before advanced CE. |

## 11. Open questions and integration dependencies

Open questions:

1. Should `ART` advertise full ordering, prefix ordering only, or equality-only planning until runtime guarantees are specified?
2. Should `LSM_ORDERED_RANGE_SCAN` be allowed to satisfy `ORDER BY` immediately, or only after a stability proof on duplicate suppression and null ordering?
3. Does ScratchBird want standalone bitmap-family row-returning paths, or only bitmap-as-candidate with a required heap or columnar fetch phase?
4. Should `BLOOM` remain filter-only, or can some catalog aliases lower it into summary-family pruning over segment-local stores?
5. What is the minimum operator-class catalog needed before `GIST`, `SPGIST`, and `GIN` can be costed as first-class families instead of prototypes?
6. For ANN, is the query contract recall-target based, budget based, or both?
7. For exact semantics over partial ANN or segment-local coverage, should ScratchBird prefer hybrid fallback, reject the path, or allow approximate semantics only when the query opts in?
8. Should ranked text paths always materialize top-k inside access, or should the planner also expose a cheaper boolean candidate path plus external sort or rerank option?
9. What subsystem owns family-metric refresh and invalidation: `ANALYZE`, index maintenance, or both?

Integration dependencies:

- Optimizer lane A: bounded frontier and memo discipline
- Optimizer lane C and D: stronger family-aware statistics and cardinality estimation
- Optimizer lane E: base access-path enumeration redesign
- Optimizer lane G cost-model work: lower-bound and detailed cost decomposition
- Optimizer lane K: plan payload and runtime trace expansion
- Index lanes A through D: per-family runtime contracts, especially exactness, recheck, and lifecycle details

## 12. Recommended next-step specification tasks

Recommended next specification tasks, in priority order:

1. Draft a canonical `Index_Planner_Path_Taxonomy_Spec` that defines the path names in Section 7 and the alias-lowering rules in Section 8.
2. Draft an `Index_Planner_Metrics_Packet_Spec` that formalizes the core packet, confidence classes, and typed family extension payloads.
3. Draft an `Index_Lifecycle_And_Queryability_Spec` that binds planner state to `BUILDING`, `QUERYABLE`, `STALE`, `MERGING`, `RETIRED`, and `FAILED`, including `coverage_fraction`.
4. Draft an `Exact_Ordered_Families_Planner_Spec` for `BTREE`, `ART`, `HASH`, and `LSM`, including ordering, parameterization, and duplicate-suppression rules.
5. Draft a `Summary_And_Candidate_Families_Planner_Spec` covering `BRIN`, `ZONEMAP`, `BLOOM`, `BITMAP`, and `COLUMNSTORE`.
6. Draft a `Generalized_And_Spatial_Planner_Spec` covering conservative pruning, overlap metrics, nearest-path legality, and recheck contracts.
7. Draft an `Inverted_Text_And_Ranking_Planner_Spec` covering boolean, phrase, ranked, and recheck-aware text access.
8. Draft a `Vector_ANN_Planner_Spec` covering `HNSW`, `IVF`, candidate-budget semantics, hybrid fallback, recall contracts, and partial segment coverage.
9. Draft an `Index_Validation_And_Calibration_Spec` that formalizes benchmark dimensions, validation metrics, and acceptance thresholds.
10. Update the optimizer observability specification so estimated and actual index-family metrics are emitted in explain and runtime traces.

First-pass synthesis:

- ScratchBird already has enough storage diversity and plan payload plumbing to support a genuinely family-aware optimizer.
- The main blocker is not missing index code. The blocker is that planner metrics, exactness classes, and path taxonomy are still too generic.
- Lane G should therefore be treated as the contract layer that turns Batch 1 family findings into enumeratable, costable, and MGA-correct optimizer behavior.
