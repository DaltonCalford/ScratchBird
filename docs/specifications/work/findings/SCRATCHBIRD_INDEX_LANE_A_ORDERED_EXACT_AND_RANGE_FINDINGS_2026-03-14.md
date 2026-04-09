# ScratchBird Index Lane A Findings

Lane: A

Topic: Ordered exact and range families (`B-tree`, `Hash`, `LSM`)

Status: First-pass findings

Date: 2026-03-14

## 1. Scope and Lane Objective

Lane A covers the access methods that should answer exact-match predicates efficiently and, where the family semantics allow it, range predicates and ordered reads.

This lane includes:

- `B-tree` as the primary mutable ordered exact-and-range family
- `Hash` as the equality-only exact family
- `LSM` as the write-optimized exact-and-range family built from memtables plus sorted immutable runs

This lane does not cover:

- summary and lossy pruning families such as `BRIN`, bitmap-family structures, and zonemap-style summaries
- generalized search families such as `GiST`, `SP-GiST`, text, inverted, spatial, or ANN/vector structures
- join enumeration or upper-plan search beyond the access-path and costing contracts consumed by those lanes

The objective is to define:

- the real ScratchBird baseline for these families
- the donor patterns worth adopting or adapting
- the MGA and lifecycle rules that keep index answers correct
- the optimizer metrics and costing inputs required for family-aware planning
- the ScratchBird contract draft for the next specification wave

Inference:

- ScratchBird already has enough implementation to justify family-specific contracts
- the main gap is not feature existence, but missing agreement between runtime capability, MGA rules, planner enumeration, and cost inputs

## 2. ScratchBird Current-State Baseline

### 2.1 Catalog and factory baseline

ScratchBird already exposes `BTREE`, `HASH`, and `LSM` as real catalog-visible families rather than placeholders.

Current control-plane facts:

- index metadata already carries `column_ids`, `include_column_ids`, expression and partial-index flags, lifecycle state, `logical_index_id`, `valid_from_xid`, and `retired_xid`
- the factory classifies `BTREE` and `HASH` as page-based runtimes and `LSM` as a file-based runtime
- `BTREE` and `HASH` advertise Bloom attachment support today
- `LSM` is treated as a primary-tablespace file-based family

Practical implication:

- ScratchBird already has the catalog shape needed for build, retire, and shadow-cutover semantics
- Lane A does not need a new catalog identity model; it needs a stricter runtime capability contract underneath the existing metadata

### 2.2 B-tree baseline

The current `B-tree` implementation is materially real and already supports the core Lane A behaviors.

Observed behavior:

- exact lookup descends the tree and applies per-entry `xmin`/`xmax` visibility filtering
- range scan walks leaf siblings and evaluates inclusive lower and upper bounds
- keys use prefix compression and separator minimization
- logical deletion marks `btn_xmax` and defers physical cleanup
- GC compaction can remove dead entries and clear page garbage flags
- adjacent pages can be merged when underutilized
- bulk load exists and avoids split churn
- optional Bloom attachment exists

Important correctness characteristics:

- the index already treats per-entry version fields as real storage data, not comments
- the runtime filters candidate entries before returning tuple identifiers
- the family can provide ordered output and should remain ScratchBird's default exact-and-range donor

Important current limitations:

- the planner currently treats only `BTREE` as order-producing
- the family contract for reverse-order scans, covering payloads, and skip-scan legality is not yet explicit
- reclaim and merge behavior still need a formal horizon contract rather than family-local heuristics

### 2.3 Hash baseline

The current `Hash` implementation is an extendible-hash style runtime with directory depth, bucket splits, and overflow chains.

Observed behavior:

- equality probes hash the key with `MurmurHash64`
- entries store only the 64-bit hash value plus tuple identifier and `he_xmin`/`he_xmax`
- insert can split buckets or allocate overflow pages
- scans traverse the bucket and its overflow chain
- GC can compact buckets and free empty overflow pages
- Bloom attachment exists

Key finding:

- the runtime stores hash value, not the original key bytes
- therefore hash lookup is collision-lossy and requires key recheck above the hash access method unless the family is changed to store enough key material to prove equality

Practical implication:

- ScratchBird `HASH` is an equality-only family
- it should never be treated as ordered
- it should never be treated as collision-free
- the optimizer contract must surface `recheck_mode = KEY_AND_VISIBILITY` for hash probes

### 2.4 LSM baseline

The current `LSM` implementation is a write-optimized family with:

- active memtable
- immutable memtable
- SSTables arranged across levels
- background leveled compaction
- tombstones for deletes
- range scan by merging sorted sources

Observed behavior:

- exact get checks active memtable, immutable memtable, then SSTables in recency order
- range scan collects overlapping sources and performs a k-way merge
- source scans already apply visibility before contributing entries
- compaction removes entries whose create and delete XIDs are both older than the current GC horizon test in the compactor

Important current strength:

- the runtime already has the ingredients for ordered range output because it merges sorted sources

Important current gaps:

- the planner does not expose `LSM` as order-producing even though the runtime performs sorted merge
- costing hard-codes level count and average SSTables per level in planning calls instead of consuming live stats
- merge duplicate selection is not contractually tied to source recency on equal keys; Lane A needs a deterministic "newest visible version wins" rule rather than key-only merge order
- the current cost model credits Bloom filtering generically, but Bloom benefit is materially different for point probes and range scans

### 2.5 Planner, costing, and statistics baseline

ScratchBird already has real optimizer infrastructure for these families, but the Lane A contract is incomplete.

Current planner behavior:

- starts from sequential scan and compares it against index-family alternatives
- can produce `INDEX_SCAN`, `INDEX_ONLY_SCAN`, `LSM_SCAN`, skip-scan-like choices, and bitmap plans built around `BTREE`
- advertises ordered output only for `BTREE`
- allows parameterized index-family paths for `BTREE` and `LSM`
- does not enumerate `HASH` as a first-class base access path

Current cost behavior:

- `B-tree` cost already uses cache-aware random-page blending and correlation-sensitive heap fetch estimates
- `LSM` cost models memtable lookup, immutable memtable lookup, Bloom checks, SSTable reads, and k-way merge CPU
- planner calls still pass hard-coded structural values such as tree height `3`, level count `3`, and average SSTables per level `2`

Current statistics behavior:

- equality selectivity uses MCV frequency first, then residual non-MCV mass divided by remaining distinct count
- range selectivity uses histogram interpolation
- correlation statistics already exist
- table stats already track scan counts, live/dead row estimates, and modification counts

Immediate gap statement:

- ScratchBird has enough machinery for family-aware Lane A planning
- it does not yet have a contract that forces the planner to consume real family capabilities and real family metrics instead of generic assumptions and hard-coded structure sizes

## 3. Donor-Engine Research Synthesis

### PostgreSQL

`PostgreSQL` is the strongest donor for `B-tree` and the clearest donor for what `Hash` should and should not promise.

Lane A takeaways:

- `B-tree` is the default exact-and-range family for `=`, `<`, `<=`, `>=`, `>`, `BETWEEN`, `IN`, `IS NULL`, anchored prefix search, and ordered retrieval
- ordered output is a first-class property, not an incidental side effect
- cost modeling combines index selectivity, correlation, and heap-page locality
- `Hash` is equality-only, stores only hash code, and is inherently lossy
- overflow chains and foreground bucket splits can erase hash advantages under skew
- no-shrink hash behavior means maintenance metrics matter, not just probe latency

ScratchBird implication:

- adopt PostgreSQL's crisp operator-family boundary
- adopt correlation-aware ordered-family costing
- treat hash overflow, collision, and no-shrink behavior as contract-visible metrics

### MySQL

`MySQL` is the strongest donor for exact/range access-path taxonomy and for the distinction between range scan, ref lookup, rowid-ordered retrieval, and multi-range read.

Lane A takeaways:

- exact lookup and range scan should be separate access-path families even when they share the same underlying index
- rowid-ordered retrieval and MRR exist because locality and heap lookup pattern change cost materially
- histograms should outrank crude index heuristics for constant equality and inequality estimation
- equality-only hash behavior in in-memory hash tables is sharply limited and should not be overgeneralized into a full ordered family

ScratchBird implication:

- add separate path families for `BTREE_EQ`, `BTREE_RANGE`, `HASH_EQ`, `LSM_EQ`, and `LSM_RANGE`
- keep "ordered result" and "exact lookup" as separate optimizer dimensions
- do not pretend that equality-only hash paths can inherit range or order behavior

### Firebird

`Firebird` is a strong donor for MGA-visible index behavior and compressed B-tree layout choices.

Lane A takeaways:

- compressed ordered trees can carry duplicate-friendly navigation metadata
- starts-with and ordered lookup legality depend on datatype and comparator semantics, not only on syntax
- read consistency and statement-level restart semantics must be considered when deciding when deleted index entries can be physically removed
- selectivity heuristics for range and between are explicit and intentionally bounded

ScratchBird implication:

- borrow Firebird's discipline that MGA and ordered-index behavior are co-designed
- expose starts-with and collation legality in capability contracts rather than inferring them from parser shape alone

### Cassandra

`Cassandra` is the strongest donor for write-optimized lifecycle and observability, not for mutable B-tree semantics.

Lane A takeaways:

- memtable plus per-SSTable index search should be surfaced as a composed read path rather than a hidden internal detail
- on-disk range pruning can use SSTable min/max term metadata before opening lower structures
- strict versus non-strict intersection and union behavior must be explicit
- virtual-table observability for build state, queryability, SSTable coverage, and segment metadata is operationally valuable

ScratchBird implication:

- adopt Cassandra-style observability for `LSM` runs, segments, min/max bounds, and queryability state
- surface overlap and segment metadata to the optimizer and validation tooling

### DuckDB

`DuckDB` is a useful secondary donor for scan eligibility gating and for combining exact indexes with coarse storage pruning.

Lane A takeaways:

- exact-and-range index use should be gated by thresholds, not chosen unconditionally
- row-group or segment-level zonemap checks can cheaply eliminate storage before any exact index work is done
- exact/range index support for constant comparisons is explicit

ScratchBird implication:

- adopt threshold-based scan gating and storage-summary prechecks around Lane A families
- do not copy DuckDB's assumptions directly into MGA behavior, because DuckDB is not the MGA donor

### ClickHouse

`ClickHouse` is the best donor for immutable ordered-part thinking, sparse primary-key pruning, and read-in-order gating.

Lane A takeaways:

- sorted immutable parts are powerful even when the primary structure is sparse
- min/max summaries and monotonic-function checks should be used to decide whether ordered access can satisfy query order efficiently
- read-in-order decisions should depend on comparator monotonicity, not just on column-name equality

ScratchBird implication:

- use ClickHouse ideas for `LSM` run pruning and ordered-read legality
- do not treat ClickHouse sparse marks as a substitute for tuple-exact `B-tree` or `Hash` semantics

Synthesis:

- `PostgreSQL` and `MySQL` are the primary donors for mutable exact-and-range contracts
- `Firebird` is the MGA and restart-safety donor
- `Cassandra` and `ClickHouse` are the primary donors for `LSM` observability and immutable-run range pruning
- `DuckDB` is a useful donor for thresholds and storage-summary gating

## 4. Primary Literature and Official-Document Synthesis

The donor source trees line up with the classic literature rather than contradicting it.

Primary literature family:

- classical `B-tree` work establishes logarithmic probe cost and leaf-order range behavior
- `B-link tree` work establishes sibling-link and fence/high-key rules for concurrent splits without losing ordered scans
- dynamic and extendible hashing literature establishes global/local depth split rules and the operational risk of overflow chains
- `LSM-tree` literature establishes the central tradeoff: lower write cost in exchange for multi-run read amplification, tombstone management, and compaction debt
- Bloom filter literature explains why point probes can avoid many disk touches while range scans usually cannot claim the same benefit

Official-document synthesis across donors:

- ordered families must publish both predicate support and ordered-output support
- equality-only families must say so explicitly and must expose whether probes are lossy
- write-optimized sorted families must expose run count, overlap, and compaction state because those values dominate read cost

Lane A distilled rules:

- ordered output is a capability, not a guess
- recheck obligations are part of the access-path contract
- compaction and reclaim are correctness concerns, not only performance concerns
- metrics must be chosen so the optimizer can distinguish "cheap point probe" from "range probe over many overlapping runs"

Useful formulas to carry into the next spec wave:

- Bloom false-positive rate:
  - `p_fp ~= (1 - e^(-k*n/m))^k`
- exact-probe read amplification for LSM:
  - `RA_exact ~= memtables_checked + sum(overlapping_runs_i) + false_positive_reads`
- range-probe merge fan-in:
  - `K_range = memtables_with_overlap + sum(overlapping_runs_i(range))`
- duplicate density:
  - `dup_density = tuples / max(ndv_full_key, 1)`

## 5. MGA and Lifecycle Correctness Packet

### 5.1 Core MGA rule

All Lane A families return candidate tuple identifiers or candidate value-bearing entries. Heap or version visibility remains the source of truth.

Required family contract:

- `B-tree` may reject invisible entries inline, but returned tuples are still candidates for residual predicate recheck
- `Hash` must recheck both visibility and key equality because the stored payload is hash-only
- `LSM` may filter visibility during source scan, but duplicate suppression must still be based on deterministic version-recency rules

### 5.2 Reclaim horizon

Lane A needs one explicit reclaim horizon for physical cleanup:

- `H_reclaim = min(H_oldest_reader, H_oldest_statement_snapshot, H_catalog_retire_safe)`

Physical deletion of an obsolete Lane A entry is legal only when:

- `xmax != 0`
- `xmax < H_reclaim`
- no shadow build, validation pass, or active reader can still require the old entry

Practical implication:

- page-local GC flags are not enough
- compaction and merge routines must consume a global reclaim horizon API, not guess from local state

### 5.3 B-tree packet

Required invariants:

- leaf sibling links must preserve forward range traversal across splits and merges
- separator or fence semantics must be sufficient to restart search safely after concurrent structural change
- logical deletion must remain visible or invisible according to MGA rules until `H_reclaim` allows physical removal
- page merge must not invalidate in-flight ordered scans

Contract implication:

- `B-tree` remains the baseline family for ordered exact and range semantics
- all other Lane A families should be compared against its correctness surface, not only its latency

### 5.4 Hash packet

Required invariants:

- equality-only lookup must never claim ordering
- collision-lossy probes must mark required key recheck explicitly
- bucket split and overflow compaction must preserve entry visibility semantics
- GC must be able to unlink empty overflow pages only after version-safety checks

Contract implication:

- `Hash` is a specialized accelerator, not a general replacement for `B-tree`

### 5.5 LSM packet

Required invariants:

- point and range reads must merge memtables and immutable runs under one snapshot rule
- duplicate suppression on equal keys must use deterministic source recency
- tombstones must remain effective until `H_reclaim`
- compaction must not physically discard a tombstoned or superseded version while an older reader can still need it
- ordered range output is legal only if merge output preserves comparator order and stable recency tie-breaks

Key contract implication:

- the current compaction rule based only on old create/delete XIDs is not sufficient as the long-term contract
- Lane A needs a shared reclaim-horizon API and a manifest-visible run-recency rule

### 5.6 Lifecycle states

Lane A should reuse the catalog lifecycle model already present in ScratchBird:

- `BUILDING`
- `VALIDATING`
- `ACTIVE`
- `RETIRING`
- `RETIRED`

Required guarantee:

- only `ACTIVE` structures may win normal planning unless a build/validation mode explicitly opts in
- `RETIRING` structures remain readable until `retired_xid < H_reclaim`

## 6. Optimizer Metrics Packet

### 6.1 Shared metrics

All Lane A families need a shared metrics packet with at least:

- tuple count
- live-entry count
- dead-entry count
- key width
- null fraction
- MCV frequencies
- histogram bounds
- full-key NDV
- left-prefix NDV for composite keys
- correlation with heap or row storage order
- stats freshness and confidence

### 6.2 B-tree metrics

Required `B-tree` metrics:

- height `H`
- internal pages
- leaf pages
- average leaf fill factor
- duplicate density by full key
- duplicate density by leading prefix
- split rate
- merge rate
- garbage fraction
- ordered-output confidence

Useful derived heuristics:

- `eq_probe_pages ~= H + expected_leaf_pages_touched`
- `range_pages ~= first_leaf_seek + leaf_pages_spanned`
- `ordered_gain = sort_cost_avoided - extra_scan_cost`

### 6.3 Hash metrics

Required `Hash` metrics:

- global depth
- average local depth
- bucket count
- average bucket occupancy
- overflow pages per probe
- hot-bucket skew ratio
- collision recheck rate
- split frequency
- GC-compacted overflow pages

Useful derived heuristics:

- `overflow_penalty = 1 + avg_overflow_pages_per_probe`
- `collision_penalty = expected_false_matches_per_probe`
- `hash_viability` falls quickly when either overflow or collision penalty grows

### 6.4 LSM metrics

Required `LSM` metrics:

- active memtable bytes
- immutable memtable count
- levels
- run count per level
- overlap factor per level
- `L0` run count
- Bloom filter false-positive rate per level
- average run min/max span
- tombstone density
- stale-version density
- compaction debt bytes
- write amplification estimate
- merge fan-in estimate for range queries

Useful derived heuristics:

- `point_run_probes = immutable_count + sum(overlap_i(point))`
- `range_run_probes = immutable_count + sum(overlap_i(range))`
- `merge_cpu ~= rows_out * log2(max(K_range, 2))`
- `compaction_penalty` should rise with `L0` fan-in and debt ratio

### 6.5 Confidence

Each metric packet should carry confidence:

- `HIGH`: fresh analyzed stats and current family runtime stats
- `MEDIUM`: analyzed value exists but family runtime stats are stale
- `LOW`: heuristic or default only

Planner implication:

- low-confidence hash and LSM metrics should add bounded pessimism
- low-confidence `B-tree` order or covering claims should not silently become hard planner truths

## 7. Access-Path and Costing Packet

### 7.1 Applicability table

| Family | Legal search shapes | Ordered output | Required recheck | Dominant cost drivers |
| --- | --- | --- | --- | --- |
| `B-tree` | full-key equality, left-prefix equality, open/closed ranges on first non-equality segment, anchored prefix search where comparator permits | yes | visibility plus residual quals | height, leaf pages touched, correlation, duplicate density |
| `Hash` | full-key equality only, batched `IN` via repeated equality probes | no | key collision plus visibility | bucket occupancy, overflow chains, skew, split rate |
| `LSM` | full-key equality, comparator-defined ranges, batched `IN` | yes if merge iterator preserves order | visibility plus residual quals | memtables checked, overlapping runs, Bloom false positives for point probes, merge fan-in, compaction debt |

### 7.2 B-tree costing

Recommended first-pass `B-tree` formula:

- `C_btree = C_seek + C_leaf + C_heap + C_cpu + C_recheck`
- `C_seek = H * c_cmp + H * rpc_eff`
- `C_leaf = leaf_pages_touched * rpc_eff`
- `C_heap = heap_pages_fetched(corr) * heap_page_cost`
- `C_cpu = rows_index * c_index_tuple + rows_heap * c_tuple`

Keep and generalize the current ScratchBird ideas:

- cache-aware random-page blending
- correlation-sensitive heap fetch estimates
- separate startup and total cost

Contract implication:

- planner must consume real `H` and real leaf-page estimates, not a fixed synthetic height

### 7.3 Hash costing

Recommended first-pass `Hash` formula:

- `C_hash_eq = C_hash_compute + C_bucket_probe + C_heap + C_recheck`
- `C_bucket_probe = (1 + overflow_pages_expected) * rpc_eff`
- `C_heap = heap_rows * heap_lookup_cost`
- `C_recheck = candidate_rows * (c_key_compare + c_visibility)`

Required heuristics:

- never reward `Hash` for sort avoidance
- penalize `Hash` sharply when overflow-chain `p95 > 1`
- penalize `Hash` when collision recheck rate is non-trivial

Planner rule:

- keep a `Hash` candidate alive only when the predicate is true equality on the hashed key and no interesting order or range advantage exists

### 7.4 LSM costing

Recommended split formulas:

- point lookup:
  - `C_lsm_eq = C_memtables + C_bloom + C_run_probe + C_heap + C_recheck`
- range scan:
  - `C_lsm_range = C_memtables + C_run_open + C_run_read + C_merge + C_heap + C_recheck`

Recommended detail:

- `C_memtables = active_probe + immutable_probe`
- `C_bloom = sum_i(overlap_i(point) * c_bloom)`
- `C_run_probe = sum_i(expected_positive_run_reads_i * rpc_eff)`
- `C_merge = rows_out * log2(max(K_range, 2)) * c_cmp`

Important rule:

- Bloom filters materially help point probes
- generic range scans should not receive the same Bloom discount unless the range predicate is narrow enough to be transformed into bounded point-like probes

### 7.5 Access-path retention heuristics

Recommended planner heuristics:

- retain `B-tree` when any order reuse, merge-join order, grouping order, or selective range exists
- retain `Hash` only for full-key equality with low overflow and low collision penalty
- retain `LSM` when write-optimized tables have bounded overlap and selective probes, especially for current/hot data
- do not collapse `LSM` exact and `LSM` range into one generic family for costing or order reasoning

### 7.6 Current ScratchBird gaps to close

Lane A should explicitly close these current issues:

- `HASH` has no first-class base access path
- `LSM` ordered output is hidden from the planner
- `INDEX_ONLY_SCAN` should require runtime proof of covering payload support, not only catalog include-column metadata
- planner cost calls should consume live family stats instead of hard-coded tree heights and level counts

## 8. ScratchBird Contract Draft

### 8.1 Required capability object

Lane A should add an explicit capability surface per family:

- `supports_exact`
- `supports_range`
- `supports_ordered_output`
- `supports_reverse_order`
- `supports_covering_payload`
- `supports_prefix_search`
- `supports_skip_scan`
- `supports_bloom_guard`
- `recheck_mode`
- `ordering_comparator_id`

Required `recheck_mode` values:

- `VISIBILITY_ONLY`
- `KEY_AND_VISIBILITY`
- `RESIDUAL_QUALS_ONLY`
- `KEY_VISIBILITY_AND_RESIDUAL`

### 8.2 Required runtime stats object

Lane A should add `OrderedIndexRuntimeStats` with:

- family type
- structural stats
- probe/read amplification stats
- maintenance stats
- reclaim-horizon-safe dead-entry stats
- confidence and last-refresh time

### 8.3 Required planner contract

Planner guarantees should be:

- do not cost order reuse unless `supports_ordered_output = true`
- do not cost index-only unless `supports_covering_payload = true`
- do not skip key recheck for `Hash`
- keep `LSM_EQ` and `LSM_RANGE` separate
- keep `B-tree` and `Hash` equality candidates both alive when they are materially different on cost, order, or recheck burden

### 8.4 Required executor contract

Executor guarantees should be:

- every Lane A path declares its recheck mode
- `Hash` paths always recheck key equality
- `LSM` equal-key merge uses deterministic source priority on equal keys
- ordered paths preserve comparator order through the final emitted candidate stream

### 8.5 Required lifecycle contract

Lifecycle guarantees should be:

- `BUILDING` indexes can gather stats but do not serve normal reads
- `VALIDATING` indexes may run shadow validation against `ACTIVE` winners
- `RETIRING` indexes remain readable until reclaim horizon clears them
- all physical cleanup consumes `H_reclaim`

### 8.6 Recommended path taxonomy

Recommended Lane A path names:

- `BTREE_EQ_SCAN`
- `BTREE_RANGE_SCAN`
- `BTREE_ORDERED_SCAN`
- `HASH_EQ_SCAN`
- `LSM_EQ_SCAN`
- `LSM_RANGE_SCAN`
- `LSM_ORDERED_RANGE_SCAN`

Inference:

- the current `INDEX_SCAN` and `LSM_SCAN` labels are too coarse for the next specification wave

## 9. Validation and Benchmark Packet

### 9.1 Existing proof points

ScratchBird already has useful starting tests:

- MGA visibility tests for multiple index families
- LSM range-scan smoke coverage
- runtime create/insert/select coverage for core index families

These prove the subsystem is real, not complete.

### 9.2 Required validation rules

Required `B-tree` validator checks:

- key monotonicity within page
- sibling-link consistency
- parent separator or fence consistency
- dead-entry versus live-entry accounting
- ordered scan restart safety after split/merge

Required `Hash` validator checks:

- directory depth and bucket ownership
- overflow-chain acyclicity
- split redistribution correctness
- collision-lossy probe accounting
- dead-entry compaction correctness

Required `LSM` validator checks:

- sorted order inside each run
- deterministic run recency ordering
- level-overlap rules
- Bloom metadata sanity
- tombstone and obsolete-version retention relative to `H_reclaim`
- manifest versus physical run consistency

### 9.3 Required correctness tests

Lane A needs at least these new correctness tests:

- `B-tree` range scan during concurrent split and merge
- `B-tree` reclaim under long-running reader
- `Hash` collision recheck correctness
- `Hash` skewed overflow-chain behavior under concurrent delete plus GC
- `LSM` equal-key merge recency correctness across active memtable, immutable memtable, and multiple levels
- `LSM` compaction under retained old snapshot
- ordered-output correctness for `B-tree` and `LSM`
- lifecycle cutover correctness for `ACTIVE` to `RETIRING`

### 9.4 Benchmark matrix

Required benchmark workloads:

- uniform point equality
- skewed hot-key equality
- short selective ranges
- long ordered ranges
- `ORDER BY ... LIMIT` over selective ranges
- mixed write-heavy exact lookup
- compaction-stressed `LSM` point probes
- overflow-stressed `Hash` equality probes

Required benchmark outputs:

- p50 and p95 latency
- pages read per probe
- candidate rows returned
- heap rows rechecked
- split and merge rates
- overflow-chain p95
- `L0` run count and compaction debt
- sort avoided versus sort paid

## 10. Adopt/Adapt/Reject/Defer Matrix

| Pattern | Disposition | Reason | ScratchBird implication |
| --- | --- | --- | --- |
| PostgreSQL `B-tree` ordered exact/range contract | `Adopt` | Clear operator, order, and cost semantics | Make `B-tree` the Lane A reference family |
| PostgreSQL correlation-aware index costing | `Adopt` | Already aligns with current ScratchBird direction | Replace hard-coded structure sizes with real stats |
| PostgreSQL hash equality-only boundary | `Adopt` | Correct family boundary | Keep `Hash` equality-only and non-ordered |
| PostgreSQL hash no-shrink behavior | `Adapt` | Metric useful, implementation literal not required | Track shrink debt and overflow debt explicitly |
| MySQL MRR and rowid-ordered retrieval thinking | `Adapt` | Valuable for locality and ordered-read costing | Add separate equality/range/order path families |
| MySQL histogram-first constant selectivity | `Adopt` | Matches current ScratchBird stats strengths | Prefer histograms and MCVs over crude key heuristics |
| Firebird compressed ordered-tree layout ideas | `Adapt` | Useful but not mandatory literal format | Borrow duplicate-friendly layout rules where they help |
| Firebird statement-level read-consistency discipline | `Adopt` | Strong MGA donor | Formalize reclaim horizon and restart-safe cleanup |
| Cassandra SAI virtual-table observability | `Adopt` | Strong operational pattern | Expose `LSM` queryability, run, and segment metadata |
| Cassandra strict versus non-strict result composition | `Adapt` | Useful for multi-run and multi-structure planning | Make intersection and union semantics explicit |
| DuckDB threshold-based index eligibility | `Adopt` | Pragmatic and cheap | Add index-versus-scan thresholds to Lane A planning |
| ClickHouse sparse primary-key exact lookup model | `Reject` | Not tuple-exact enough for Lane A row lookup | Use only for run pruning, not exact tuple semantics |
| ClickHouse minmax and monotonic ordered-read gating | `Adapt` | Useful for immutable runs | Apply to `LSM` run pruning and ordered legality |
| Full skip-scan for all Lane A families | `Defer` | Legality and costing not uniform | First wave should make it `B-tree`-only if implemented |

## 11. Open Questions and Integration Dependencies

- Should `Hash` remain hash-only-and-lossy, or should ScratchBird store enough key material to eliminate collision recheck for common types?
- Should `B-tree` ordered-output support include reverse scans in the first formal contract?
- What is the exact engine API for `H_reclaim`, and which subsystem owns it?
- How should `LSM` expose source-recency ordering to make equal-key merge deterministic?
- Should `LSM` ordered scans be allowed to satisfy `ORDER BY` immediately, or only after a stability proof on comparator and duplicate handling?
- What is the authoritative definition of covering payload support for Lane A families?
- Does ScratchBird want `Hash` paths to participate in bitmap or intersection planning, or stay as standalone equality accelerators first?
- How should partial and expression indexes enter Lane A legality for exact and range probes?
- What is the intended null-ordering contract for ordered scans across `B-tree` and `LSM`?
- Which stats live in the catalog, which live in runtime telemetry, and which are recomputed during analyze?
- How are long-running validation or shadow-build readers accounted for in reclaim horizon calculations?
- Which subsystem owns online validation and corruption reporting for file-based `LSM` structures?

## 12. Recommended Next-Step Specification Tasks

- Draft `Ordered_Index_Family_Contract` with the capability object, runtime stats object, and recheck modes defined in this findings note.
- Draft `BTree_Exact_And_Range_Runtime_Spec` covering operator legality, ordered output, reclaim horizon use, and validation rules.
- Draft `Hash_Equality_Runtime_Spec` covering collision-lossy semantics, overflow metrics, split rules, and required recheck.
- Draft `LSM_Exact_And_Range_Runtime_Spec` covering memtable plus run merge, deterministic source priority, run metrics, and reclaim-safe compaction.
- Draft `Lane_A_Statistics_And_Costing_Spec` defining the shared metric packet, family-specific metrics, and first-pass formulas.
- Update the optimizer specification so `HASH_EQ_SCAN`, `LSM_EQ_SCAN`, and `LSM_RANGE_SCAN` are first-class path families instead of hidden variants.
- Draft `Index_Reclaim_Horizon_And_Lifecycle_Spec` shared across Lane A families and later reusable by other index families.
- Draft `Lane_A_Validation_And_Benchmark_Spec` with the exact correctness gates, validator outputs, and benchmark workloads listed above.

Bottom line:

- ScratchBird already has a usable Lane A substrate
- `B-tree` is the real ordered reference family
- `Hash` is a specialized equality accelerator and must be treated as collision-lossy
- `LSM` is closer to first-class ordered exact/range support than the current planner surface admits
- the next specification wave should focus on capability truth, reclaim-horizon truth, and metric truth rather than adding more family labels
