# ScratchBird Index Lane H Findings

Lane: H

Topic: Governance, benchmarks, contradiction review, and synthesis

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_INDEX_AND_OPTIMIZER_INTEGRATION_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_INDEX_SPECIFICATION_COMPLETION_WORKPLAN_2026-03-14.md`
- `SCRATCHBIRD_INDEX_RESEARCH_BATCH_TRACKER_2026-03-14.md`
- `SCRATCHBIRD_INDEX_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

Primary research inputs:
- `SCRATCHBIRD_INDEX_IMPLEMENTATION_AND_OPTIMIZER_INTEGRATION_FINDINGS_2026-03-14.md`
- `SCRATCHBIRD_INDEX_LANE_A_ORDERED_EXACT_AND_RANGE_FINDINGS_2026-03-14.md`
- `SCRATCHBIRD_INDEX_LANE_B_SUMMARY_BITMAP_AND_COLUMNAR_FINDINGS_2026-03-14.md`
- `SCRATCHBIRD_INDEX_LANE_C_GENERALIZED_SEARCH_AND_SPATIAL_FINDINGS_2026-03-14.md`
- `SCRATCHBIRD_INDEX_LANE_D_INVERTED_AND_TEXT_FINDINGS_2026-03-14.md`
- `SCRATCHBIRD_INDEX_LANE_E_VECTOR_AND_ANN_FINDINGS_2026-03-14.md`
- `SCRATCHBIRD_INDEX_LANE_F_MGA_LIFECYCLE_AND_GC_FINDINGS_2026-03-14.md`
- `SCRATCHBIRD_INDEX_LANE_G_OPTIMIZER_METRICS_AND_PLANNER_INTEGRATION_FINDINGS_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_LANE_E_ACCESS_PATH_ENUMERATION_FINDINGS_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_LANE_G_COST_MODEL_AND_CALIBRATION_FINDINGS_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_LANE_L_GOVERNANCE_AND_SYNTHESIS_FINDINGS_2026-03-14.md`
- `18_Index_Framework` canonical contracts
- `23_SBLR_VM_Compiler_and_Executor/CARDINALITY_STATISTICS_AND_COST_MODEL_GOVERNANCE.md`
- `31_Conformance_Performance_and_Reliability_Gates/BTREE_HARDENING_AND_CRASH_SAFE_INDEX_GATES.md`
- historical `INDEX_REFACTOR_REWRITE_WORKTREE` gate and test-strategy artifacts

Evidence note:
- Web research was not required for this first pass. The local index research corpus, canonical contracts, historical gate material, and current ScratchBird code were sufficient.

## 1. Scope and lane objective

Lane H owns the governance gap rather than a single index family gap. It converts the index research program into one executable release model with benchmark corpus, contradiction review, scorecards, release gates, and research-to-spec handoff rules.

This first pass now covers the full program boundary and includes the completed evidence set from all index research lanes:

- the current-code and canonical-spec baseline
- index lane findings A through G
- historical index gate patterns
- optimizer governance patterns from optimizer lanes E, G, and L

Batch 2 completion changes the remaining work. The open blockers are no longer missing lane packets. The open blockers are:

- unresolved contradictions between canonical spec, current code, and lane contracts
- conversion of lane findings into canonical engineering specs
- gate and scorecard execution against the completed cross-family packets

Lane H objective for this pass:

1. define one governance model for all index families
2. define one benchmark corpus and scorecard schema
3. define one contradiction-review method against section 18, current code, and optimizer work
4. define one release-gate stack that prevents unsupported planner enablement
5. define the next specification tasks that turn the research lanes into engineering contracts

Synthesis implication:

- Lane H does not create a second copy of section 18
- Lane H creates the closure machinery that decides when section 18, current code, and later implementation specs agree enough for release claims

## 2. ScratchBird current-state baseline

ScratchBird already has a broad index runtime substrate. The current runtime-class registry and executor routing cover:

- `BTREE`
- `HASH`
- `LSM`
- `GIN`
- `GIST`
- `BRIN`
- `RTREE`
- `SPGIST`
- `BITMAP`
- `COLUMNSTORE`
- `HNSW`
- `INVERTED`

The planner does not govern that full surface yet. In the normal relational path it still reasons mainly in terms of:

- sequential scan
- `INDEX_SCAN`
- `INDEX_ONLY_SCAN`
- synthetic `BITMAP_INDEX_SCAN` over B-tree predicates
- `LSM_SCAN`
- heuristic skip-scan-like behavior

That asymmetry is the main baseline fact for Lane H: ScratchBird has more executor-visible index reality than planner-visible index governance.

Canonical spec maturity is not the blocker. Section 18 already defines:

- family semantics
- MGA visibility rules
- metrics and costing schemas
- maintenance and lifecycle rules
- test contracts

Section 23 already defines:

- typed statistics
- freshness and confidence classes
- formula-profile and calibration governance

Section 31 already proves that ScratchBird accepts release-blocking gate suites for index work, at least for the B-tree hardening surface.

The missing baseline artifact is one integrated governance packet that ties together:

- family taxonomy
- planner path taxonomy
- MGA correctness
- family metrics
- benchmark coverage
- contradiction closure
- release gates

Most material baseline contradictions already visible from the source set:

| Contradiction | Current evidence | Governance implication |
| --- | --- | --- |
| Stored bitmap family versus synthetic `BITMAP_INDEX_SCAN` path | Lane B plus current planner code | Stored bitmap and bitmap-combine must become separate planner contracts. |
| Family-aware runtime exists, but planner still uses hard-coded structural values for Lane A choices | Lane A plus current planner code | Real family metrics are not yet authoritative planning inputs. |
| `GiST` and `SPGiST` catalog surface is richer than the live opclass binding and search routing | Lane C plus current executor code | Outward capability claims must be narrowed until catalog-selected opclass binding and strategy routing are validator-backed. |
| `FULLTEXT`, `GIN`, and scored inverted behavior are split across incompatible routing assumptions | Lane D plus current executor code | Text storage identity and planner-visible path contracts must be unified before enablement. |
| Columnstore exists but has overlapping runtime assumptions and no first-class planner path | Lane B | One columnstore contract and one publication model must be chosen. |
| Vector and ANN labels mostly route to one prototype runtime while family-specific options are not durably persisted | Lane E | Planner-visible vector support must lower to honest physical families with persisted metadata, not parser-only labels. |
| Family-local GC rules still mix `current_xid`, oldest-active, and oldest-interesting tests | Lane F | One shared index reclaim horizon must replace family-local reclaim guesses. |
| Planner metrics, path taxonomy, and exactness classes are still too generic for most families | Lane G | Completed research now supports a family-typed planner contract, so generic `INDEX_SCAN` semantics become a release blocker. |
| Executor routes many families that the planner cannot cost or explain | Program baseline | "Supported" can no longer be inferred from executor routing alone. |

Historical baseline that Lane H should preserve:

- the prior index rewrite worktree already used phase gates `IX-GATE-00` through `IX-GATE-08`
- those gates already separated taxonomy, parser/emitter, family correctness, maintenance, metrics, visibility/security, and final conformance
- Lane H should reuse that governance shape rather than inventing a new one

## 3. Donor-engine research synthesis

Lane H should not pick one donor engine and copy its governance whole. The completed family lanes point to a donor matrix where each donor contributes a stable governance or benchmark pattern.

| Donor | Stable lesson | Lane H synthesis implication |
| --- | --- | --- |
| PostgreSQL | Clear family boundaries, opclass validation, recheck honesty, BRIN/GIN/GiST/SP-GiST contracts, correlation-aware costing | Primary donor for exactness, path legality, generalized-search governance, and contradiction review against overclaimed capability. |
| Firebird | MGA-native statement and snapshot discipline with conservative cleanup | Primary donor for reclaim-horizon governance and "visibility first, cleanup later" rules. |
| MySQL | Explicit skip-scan and index-merge taxonomy, calibration discipline, crossover-aware costing | Primary donor for path taxonomy and benchmark crossover design. |
| Cassandra | Immutable-generation publication, storage-attached observability, merge debt visibility | Primary donor for generation publish/retire/reclaim governance on Lane B and Lane D families. |
| DuckDB | Threshold-based index eligibility, row-group pruning, Top-N-aware access choices | Donor for analytic benchmark shapes and ordered-versus-sort scorecards. |
| ClickHouse | Granule and segment pruning, ordered-read gating | Donor for columnar benchmark axes and pruning scorecards, not tuple-exact semantics. |
| MongoDB | Sparse semantics, field weights, geo-cell covering as optional policy bundles | Secondary donor for optional family policies, not first-wave storage identity. |
| Redis | Approximate spatial prefilter economics and explicit recheck value | Secondary donor for lossy-path economics and validation, not core tree-family storage. |
| OpenSearch | Index/search analyzer separation and ranked retrieval policy | Donor for analyzer and scoring policy only; refresh-based visibility remains rejected. |

Cross-donor synthesis:

- donor family names should not become ScratchBird storage identities
- donor patterns are useful only when translated into MGA-native publication and reclaim rules
- governance must borrow the donor's validation discipline and benchmark shape, not just the donor's feature label

## 4. Primary literature and official-document synthesis

Across the family-lane literature packets, official engine documentation, section 18, and optimizer governance material, a small set of stable laws emerges.

1. Candidate generation and exact qualification must be treated separately.
   - Lossy access methods are acceptable only when `requires_recheck` survives into planning, execution, diagnostics, and benchmark reporting.

2. Tail behavior matters more than mean behavior.
   - p95 and p99 latency, q-error percentiles, stale-hit counts, false-positive rates, and lower-bound violations are release metrics.

3. Ordered-path claims require proof, not implication.
   - Ordered generalized search, nearest-neighbor, score-first retrieval, and ordered exact scans all need validated lower-bound or exact-order contracts before planner enablement.

4. Publication and reclaim behavior are part of benchmark truth.
   - A family cannot be considered benchmark-ready if it only works under empty-snapshot or no-maintenance assumptions.

5. Explicit formulas and explicit counters beat opaque heuristics.
   - Hidden structural constants, undocumented fallback routes, and planner-only magic numbers are governance failures even when they appear to work on small workloads.

Practical implication for ScratchBird:

- the benchmark corpus must combine correctness, lifecycle, access-path, and cost-model evidence
- generic smoke tests and isolated family unit tests are necessary but not sufficient
- section 18 and section 23 already contain much of the normative truth; Lane H adds the proof program that decides whether that truth is actually met

## 5. MGA and lifecycle correctness packet

Lane H should normalize family-local lifecycle language into one MGA-safe program contract.

Non-negotiable cross-family invariants:

1. Index entries, postings, summaries, segments, and ANN candidates are never user-visible truth by themselves. Heap or row-version visibility remains authoritative.
2. Delete and update are logical visibility changes first and physical cleanup second.
3. Build, rebuild, merge, and major maintenance publish via generation swap or equivalent shadow publication, not by mutating the only visible generation in place.
4. Physical reclaim is legal only after the global safe horizon passes and any required replacement generation is published.
5. Every lossy, approximate, or analyzer-sensitive family must propagate `requires_recheck`.
6. No family may substitute refresh timing or WAL-style replay assumptions for MGA visibility truth.

Normalized reclaim rule:

`H_reclaim = min(H_oldest_reader, H_oldest_statement_snapshot, H_catalog_retire_safe)`

Physical reclaim is legal only when:

- the entry or generation is no longer visible to any snapshot at `H_reclaim`
- the family-specific replacement or absence state is already published
- the removal does not strand a still-visible branch, posting, summary, or segment reference

Normalized lifecycle states:

| Normalized state | Meaning | Allowed family-local sub-states |
| --- | --- | --- |
| `BUILDING` | Private structure generation under construction | `SEALED` pre-publish preparation |
| `VALIDATING` | Internal consistency is being checked; structure not planner-eligible | `SEALED`, validator-only open |
| `ACTIVE` | Planner-eligible and queryable for new work | `QUERYABLE`, `MERGING`, background maintenance |
| `RETIRING` | Superseded for new planning but still visible to old snapshots | `OBSOLETE`, `RETIRED_PENDING_SNAPSHOT_DRAIN` |
| `RECLAIMABLE` | Safe for physical reclaim after horizon check | none |

Family-specific lifecycle consequences:

- Lane A exact/range families:
  - precise dead-entry deletion is required
  - ordered scans cannot lose reachability during split, merge, or compaction
  - `LSM` compaction must honor run-recency and tombstone retention until `H_reclaim`

- Lane B summary/bitmap/columnar families:
  - BRIN must prefer resummarize or rebuild over blind range tombstoning
  - stored bitmap cleanup removes only globally dead postings
  - columnstore segments publish as coherent generation manifests, not as partial side effects

- Lane C generalized-search and spatial families:
  - ancestor summaries may be conservative, but never under-conservative
  - no split or maintenance action may make a visible descendant unreachable
  - opclass determinism and null policy are part of lifecycle correctness, not optional metadata

- Lane D inverted and text families:
  - per-posting version windows or generation fences must preserve tuple revalidation
  - merge debt and old-generation retirement must be tracked explicitly
  - analyzer policy version is lifecycle metadata because exactness depends on it

- Lane E vector and ANN families:
  - `VECTOR_FLAT` is the exact truth baseline and first exact fallback family
  - `HNSW` is the first credible approximate family, but only after metadata persistence, delete debt, and graph-quality triggers are formalized
  - `IVF` is viable only as a train-and-publish family with durable centroid and rerank artifacts
  - `SCANN`, `DISKANN`, and other ANN labels remain deferred until their physical runtimes, metadata, and I/O or quantization metrics exist

Release implication:

- no family may pass Lane H governance without a published lifecycle state machine and a reclaim-horizon owner

## 6. Optimizer metrics packet

Lane H should define one shared metrics envelope plus family-specific extensions. A family is not planner-ready until its required metrics packet is complete and explainable.

### 6.1 Shared packet

All families should publish the following shared packets:

| Packet | Required fields | Governance use |
| --- | --- | --- |
| Cardinality | `row_count_est`, `distinct_count_est`, `null_frac`, histograms, MCVs | Base selectivity and crossover reasoning |
| Structure | `height`, `internal_pages`, `leaf_pages`, `avg_key_len`, `avg_entry_len`, `bytes_used`, `bytes_allocated`, `fragmentation_ratio` | Structural cost inputs and health review |
| Usage | `scan_count`, `tuple_read`, `tuple_returned`, `blocks_read`, `blocks_hit`, `total_time_ns`, `last_used_at` | Cost calibration and benchmark output |
| Contention | lock waits, latch waits, deadlocks, hot-key or hotspot counters | Concurrency and tail-latency scorecards |
| Health | light-scan and diagnostic-scan timestamps, corruption counters, orphan or duplicate counters | Release gating and postmortem evidence |
| Governance | `stats_snapshot_id`, `last_analyzed_at`, `sample_ratio`, `modified_rows_since_analyze`, `staleness_class`, `confidence_class`, `formula_profile_id`, `calibration_profile_id` | Freshness, confidence, and replayable cost evidence |

### 6.2 Family extensions

Required family-specific additions from the completed lane corpus:

- Lane A exact/range:
  - overflow debt
  - dead-version density
  - compact-before-split count
  - split and merge rates
  - `L0` run count, Bloom false-positive rate, compaction debt
  - sort-avoided versus sort-paid counters for ordered paths

- Lane B summary/columnar:
  - `pages_per_range`
  - unsummarized-tail fraction
  - range false-positive rate
  - posting density and compression ratio
  - row-groups touched versus total
  - bytes per projected column and reconstruction cost

- Lane C generalized-search/spatial:
  - `recheck_ratio`
  - `false_positive_ratio`
  - `coverage_ratio`
  - `pair_overlap_ratio`
  - `penalty_mean`
  - `branch_skew`
  - `all_the_same_fraction`
  - `lb_violation_count`

- Lane D inverted/text:
  - `N_live_docs`
  - `N_terms`
  - `N_tokens`
  - `avgdl`
  - `df(term)`
  - posting bytes
  - pending-entry depth
  - live and merging segment counts
  - dead-document fraction
  - phrase-position hit rate
  - merge debt

- Lane E vector/ANN:
  - `live_vectors`, `dead_vectors`, and `dead_fraction`
  - `candidate_budget` and `candidate_budget_kind`
  - `candidate_count_p50`, `candidate_count_p95`
  - `recall_estimate_at_k`
  - `candidate_amplification`
  - `deleted_fraction`, `avg_graph_degree`, and `visited_nodes_*` for graph families
  - `nlist`, `nprobe_default`, `avg_list_len`, `list_cv`, `centroid_drift`, and `rerank_ratio` for partitioned families
  - `memory_resident_fraction`, `cache_hit_ratio`, and `ssd_reads_per_query` for disk-assisted families once exposed

### 6.3 Governance rules

Metrics governance rules:

1. A family path may not be chosen when its required packet is absent and no conservative fallback estimate exists.
2. Freshness and confidence are planner-visible contract fields, not diagnostics-only fields.
3. Formula-profile and calibration identities must survive into plan payloads and benchmark reports.
4. Scorecards consume the same metrics packet that planning consumes; there is no private release-only metric surface.

## 7. Access-path and costing packet

Lane H should freeze a normalized planner path taxonomy before implementation work resumes.

### 7.1 Required path families

| Family group | Required planner-visible paths | Contract note |
| --- | --- | --- |
| Lane A | `BTREE_EQ_SCAN`, `BTREE_RANGE_SCAN`, `INDEX_ONLY_SCAN`, `HASH_EQ_SCAN`, `LSM_EQ_SCAN`, `LSM_RANGE_SCAN`, optional `SKIP_SCAN` | Replace generic exact-path assumptions with family capability truth. |
| Lane B | `BRIN_SCAN`, `BITMAP_STORAGE_SCAN`, `BITMAP_COMBINE_SCAN`, `COLUMNSTORE_SCAN` | Stored bitmap and synthetic bitmap-combine must stay separate. |
| Lane C | `RTREE_SCAN`, `RTREE_NEAREST_SCAN`, `GIST_SCAN`, `GIST_KNN_SCAN`, `SPGIST_SCAN` | Strategy routing and lower-bound legality are required for enablement. |
| Lane D | `GIN_FILTER_SCAN`, `TEXT_BITMAP_SCAN`, `TEXT_SCORE_SCAN`, `TEXT_RECHECK_SCAN` | Boolean, ranked, and residual-recheck shapes must be distinct. |
| Lane E | `VECTOR_EXACT_SCAN`, `HNSW_KNN_SCAN`, `IVF_KNN_SCAN`, future `SCANN_KNN_SCAN` and `DISKANN_KNN_SCAN` | `VECTOR_EXACT_SCAN` and `HNSW_KNN_SCAN` are the only plausible first-wave planner-visible paths; the rest require dedicated persisted metadata and cost packets first. |

### 7.2 Costing governance

Lane H should adopt the optimizer Lane G split between:

- `LowerBoundCost` for search pruning
- `DetailedCost` for winner comparison and diagnostics

Every index-family cost packet should carry:

- startup cost
- total cost
- CPU component
- I/O component
- memory and spill component
- candidate or recheck component
- heap-fetch component
- uncertainty or confidence band

Cross-family costing rules:

1. Ordered-path retention and interesting-order rules come from the frontier model in optimizer Lane E; one locally cheapest base path is not sufficient.
2. Costing must consume family metrics instead of hard-coded structural values.
3. `requires_recheck` is a cost dimension, not just an executor flag.
4. Candidate-region scans, late materialization, and score-first retrieval must each have explicit cost shapes.
5. If a family cannot prove path legality, exactness, or lower-bound safety, the planner must not choose it.

### 7.3 Current contradictions to close before final freeze

- generic `INDEX_SCAN` semantics still mask family-specific exactness and ordering differences
- `BITMAP_INDEX_SCAN` currently conflates stored and synthetic meanings
- no first-class planner nodes exist yet for most executor-routed families
- no family-agnostic contract exists yet for candidate-region scans or late materialization
- vector-family path legality is now defined, but routed-label honesty and persisted metadata still block broad planner enablement
- cross-family planner contracts are now specified, but the live code still needs typed family packets and canonical path lowering

## 8. ScratchBird contract draft

Lane H should produce one governing contract set for the entire index program.

Required governance artifacts:

1. `IndexFamilyCapabilityRegistry`
   - one runtime class per exposed family or alias
   - one planner path set per runtime class
   - one exactness and recheck model per path

2. `IndexLifecycleAndReclaimContract`
   - normalized state machine
   - reclaim-horizon owner
   - publish/retire/reclaim evidence requirements

3. `IndexMetricsAndCalibrationRegistry`
   - required shared and family-specific metrics
   - freshness and confidence policy
   - formula-profile and calibration identity rules

4. `IndexBenchmarkRegistry`
   - benchmark suites
   - dataset descriptors
   - seed-control rules
   - donor-reference snapshot rules
   - required outputs

5. `IndexScorecardSnapshot`
   - per-family readiness state
   - release blockers
   - evidence completeness summary

6. `IndexContradictionLog`
   - normalized mismatch list
   - owner and disposition
   - closure state

7. `IndexDeferredRegister`
   - all non-shipped optional families and advanced features
   - explicit rationale and reopen condition

Governance guarantees:

- no family is planner-visible by default without a complete capability object, lifecycle contract, metrics packet, and benchmark mapping
- no donor family name may imply a separate ScratchBird storage identity without runtime-class proof
- no deferred feature disappears silently; every deferral remains visible in the register
- no contradiction may be resolved only by implementation drift; closure must update spec, code, or defer record explicitly

### 8.1 Contradiction-review method

Lane H contradiction review should follow this method:

1. Freeze authoritative inputs:
   - section 18, section 23, and section 31 canonical contracts
   - current ScratchBird code baseline
   - current integration findings
   - completed family-lane findings A through G
   - current defer register for unsupported first-wave families and unresolved contradictions

2. Normalize every family to one comparison schema:
   - runtime class
   - alias lowering
   - visibility model
   - publication model
   - planner paths
   - metrics packet
   - benchmark suites

3. Compare four views:
   - canonical spec
   - live code
   - lane findings
   - optimizer governance patterns

4. Classify every mismatch as one of:
   - `spec_gap`
   - `code_gap`
   - `naming_collision`
   - `overstated_surface`
   - `pending_dependency`

5. Assign closure owner:
   - section-18 update
   - planner-spec update
   - code change
   - deferred dependency

6. Re-run the scorecard and gates.
   - Any Red contradiction on an enabled family blocks release.

Synthesis implication:

- Lane H is the closure lane that decides whether the family lanes can be converted into engineering specs without reopening baseline research

## 9. Validation and benchmark packet

### 9.1 Benchmark corpus

Lane H should freeze one benchmark corpus with five layers.

1. Correctness corpus
   - snapshot visibility equivalence to sequential scan
   - insert, update, delete, and reclaim under old and new snapshots
   - build, publish, retire, and reclaim correctness
   - exactness and required-recheck truth tables

2. Family microbenchmark corpus
   - Lane A: uniform and skewed equality, selective and long ranges, `ORDER BY ... LIMIT`, compaction debt, overflow debt
   - Lane B: clustered and unclustered BRIN ranges, exact and lossy bitmap execution, wide and narrow columnstore projections, late materialization
   - Lane C: overlap, containment, nearest search, partition skew, publication amplification
   - Lane D: boolean term search, wide OR, phrase and proximity, ranked `TOP K`, write-heavy merge debt
   - Lane E: exact-vector truth runs, HNSW quality and delete-debt runs, IVF training and rerank runs, and deferred-family rejection tests

3. Planner crossover corpus
   - ordered scan versus sort
   - BRIN versus sequential scan on clustered and stale layouts
   - stored bitmap versus bitmap-combine versus sequential scan
   - columnstore projection versus row-store reconstruction
   - text score-first versus filter-plus-sort
   - spatial lower-bound path versus scan fallback

4. Workload and donor-reference corpus
   - `TPC-H`
   - `TPC-DS` where feasible
   - `Star Schema Benchmark`
   - `Join Order Benchmark` slices where index choice matters
   - correlated-predicate synthetic workloads
   - skew and heavy-hitter workloads
   - plan-stability regression corpus

5. Release regression corpus
   - deterministic reruns of all acceptance and rejection matrices
   - migration and backward-compatibility checks
   - full conformance sweep for planner-visible families

Required outputs for every benchmark suite:

- seed and configuration hash
- p50, p95, and p99 latency
- candidate count
- recheck count
- heap fetch count
- bytes or pages read
- family-specific debt counters
- false-positive and false-negative counts
- plan choice and cost breakdown
- formula-profile and calibration-profile identifiers

### 9.2 Scorecard schema

Lane H should use one shared readiness scorecard with `Green`, `Yellow`, `Red`, and `Pending` states.

Required columns:

- contract completeness
- MGA correctness
- planner integration
- metrics completeness
- benchmark coverage
- contradiction status
- release state

Provisional first-pass scorecard from the current corpus:

| Scope | Contract | MGA | Planner | Metrics | Benchmarks | Contradictions | Release state |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Lane A exact/range | Yellow | Yellow | Yellow | Yellow | Yellow | Yellow | Blocked on real structural metrics, reclaim-horizon API, and final path taxonomy |
| Lane B summary/columnar | Yellow | Yellow | Red | Red | Yellow | Red | Blocked on taxonomy split, columnstore unification, and planner path enablement |
| Lane C generalized/spatial | Yellow | Red | Red | Yellow | Yellow | Red | Blocked on opclass binding, strategy routing, and MGA-safe maintenance replacement |
| Lane D inverted/text | Yellow | Red | Red | Yellow | Yellow | Red | Blocked on transaction-correct routing unification and planner-visible path contracts |
| Lane E vector/ANN | Yellow | Red | Red | Yellow | Yellow | Red | Blocked on HNSW correctness fixes, durable family metadata, and honest routed-label policy |
| Lane F cross-family MGA | Yellow | Red | Yellow | Yellow | Yellow | Red | Blocked on shared reclaim-horizon API adoption and removal of family-local physical-delete shortcuts |
| Lane G cross-family planner | Yellow | Yellow | Red | Yellow | Yellow | Red | Blocked on typed family metrics, canonical path taxonomy, and frontier-preserving enumeration |
| Overall program | Yellow | Red | Red | Yellow | Yellow | Red | Research-complete but not release-ready |

Interpretation:

- no current lane is releasable
- full lane coverage now exists across A through G
- final signoff is blocked by unresolved contradictions, unimplemented family-typed planner contracts, and missing gate evidence rather than missing research packets

### 9.3 Release gates

Lane H should preserve the historical phase-gate shape and update it for the new research program.

| Gate | Phase | Purpose | Minimum evidence |
| --- | --- | --- | --- |
| `IX-GATE-00` | PH0 | Taxonomy and alias freeze | family/runtime matrix, rejection matrix, contradiction log seed |
| `IX-GATE-01` | PH0 | DDL, parser, and binder contract | grammar matrix, action matrix, alias-lowering audit |
| `IX-GATE-02` | PH0 | Planner/emitter/executor payload contract | payload audit, opcode or plan-node audit, exactness and recheck field audit |
| `IX-GATE-03` | PH1 | Family correctness matrix | family implementation matrix, family test matrix, validator outputs |
| `IX-GATE-04` | PH2 | Online publish, maintenance, and reclaim | online maintenance state audit, delta-apply or generation-swap audit |
| `IX-GATE-05` | PH2 | Metrics, health, and corruption reporting | metrics update audit, health-scan results, scorecard packet |
| `IX-GATE-06` | PH2 | Reporting and explainability | `SHOW` or `EXPLAIN` contract audit, index reporting contract |
| `IX-GATE-07` | PH3 | MGA and security enforcement | visibility matrix, policy audit, recheck truth matrix |
| `IX-GATE-08` | PH3 | Performance, migration, and full conformance | perf regression report, migration audit, full conformance report |
| `G15_BTREE_HARDENING` | Specialized | Ordered exact-family release proof | crash-window matrix, validator output, seed maps, repeated-run evidence |

Gate rules:

1. All acceptance and rejection matrices rerun at least three times with identical outputs.
2. Every gate emits manifest, summary, checksums, and evidence file indexes.
3. Any planner-visible family also needs the relevant specialized hardening gate if its family semantics require it.
4. Future equivalents to `G15_BTREE_HARDENING` should be defined for text, generalized-search, and vector families once lanes D, C, and E settle their validator packets.

## 10. Adopt/adapt/reject/defer matrix

| Program pattern | Disposition | Reason | ScratchBird implication |
| --- | --- | --- | --- |
| Historical phase-gated index release stack | `Adopt` | Already matches ScratchBird governance culture and evidence flow | Reuse `IX-GATE-*` as the cross-family release backbone |
| Family-specialized hardening gates such as `G15_BTREE_HARDENING` | `Adopt` | High-risk families need stronger proof than general conformance alone | Extend the model to text, spatial, and vector families after the base contracts settle |
| Lane-to-spec conversion before implementation claims | `Adopt` | Prevents drift from findings notes straight into code | Draft engineering specs from lanes before shipping planner-visible behavior |
| Benchmark-led signoff and shared scorecards from optimizer Lane L | `Adopt` | Best fit for cross-subsystem governance | One scorecard format should cover index and planner closure together |
| Donor-reference comparisons on fixed corpora | `Adapt` | Useful for regression detection but ScratchBird workloads are not identical to donor workloads | Record donor-reference snapshots, but gate on ScratchBird-defined pass bands |
| PostgreSQL-style opclass validation and recheck honesty | `Adopt` | Stable contract pattern across Lane C and Lane D | Validator-backed exactness becomes mandatory |
| Cassandra-style generation publication and observability | `Adopt` | Strong fit for Lane B and Lane D lifecycle governance | Publish and retire generations explicitly with backlog metrics |
| Optimizer lower-bound versus detailed-cost split | `Adopt` | Needed to reconcile frontier search with explainable signoff | Index paths must emit both pruning and final cost evidence |
| Shared name for stored bitmap and synthetic bitmap-combine paths | `Reject` | Hides materially different lifecycle and costing behavior | Split the taxonomy and reporting surfaces now |
| Refresh-based visibility or non-MGA publication truth | `Reject` | Conflicts with ScratchBird invariants | Visibility remains snapshot-based for every family |
| Executor-routing proof as sufficient support claim | `Reject` | Planner, metrics, and benchmark governance would still be absent | Support claims require full path, metric, and benchmark closure |
| Donor family names as long-term storage identities | `Reject` | Pollutes taxonomy and hides runtime-class truth | Keep donor names as aliases or policy bundles only |
| Full-program signoff before lane contracts, contradiction log, and gate evidence go green | `Reject` | Completed research alone is not a release claim | Treat Lane H as the spec-conversion and gate-control layer until the scorecard is green |
| Optional advanced text, spatial, and vector families beyond baseline contracts | `Defer` | First-wave governance must settle exactness, lifecycle, and metrics first | Keep advanced families in the defer register with explicit reopen rules |

## 11. Open questions and integration dependencies

1. What is the authoritative persisted metadata packet for vector and ANN families, including build knobs, search defaults, payload mode, and artifact generation?
2. Should exact vector rerank use heap-fetched vectors, index-stored raw vectors, or both depending on payload mode?
3. What is the final engine API and ownership model for the captured index lifecycle snapshot and `H_index_reclaim`?
4. Should catalog generation visibility remain XID-based only, or does it need an explicit statement-snapshot token for some maintenance and replay scenarios?
5. Columnstore still needs one authoritative runtime and publication contract.
6. Bitmap storage still needs a stable row-identifier model and clear separation from synthetic bitmap-combine paths.
7. `GiST` and `SPGiST` still need a bootstrap model for built-in opclasses and strategy maps.
8. Lane D still needs a final routing decision between `GIN`, scored inverted runtime, and alias-lowered `FULLTEXT` behavior.
9. ScratchBird still needs a policy for which families support ordered output, reverse scan, and index-only behavior in v1.
10. Benchmark environment, seed-control policy, and donor-reference snapshot retention still need one canonical registry format.
11. Lane H still needs follow-on family-specialized hardening gates beyond the current B-tree gate.
12. Which subsystem owns family-metric refresh and invalidation for planner use: `ANALYZE`, maintenance, sampled health runs, or a mixed model?

## 12. Recommended next-step specification tasks

1. Draft `Index_Governance_And_Benchmark_Contract` that defines the gate stack, scorecard schema, contradiction log, defer register, and release rules.
2. Draft `Index_Runtime_Taxonomy_And_Alias_Lowering_Spec` that settles runtime classes, family names, and planner path names.
3. Draft `Index_MGA_Publication_And_Reclaim_Spec` shared across lanes A through F.
4. Draft `Index_Family_Metrics_And_Calibration_Spec` that combines section 18 metrics, section 23 confidence rules, and the family extensions from lanes A through G.
5. Draft `Index_Planner_Path_Taxonomy_And_Exactness_Spec` covering family path names, `requires_recheck`, candidate-region scans, late materialization, and lower-bound ordered paths.
6. Draft `Index_Benchmark_Corpus_And_Scorecard_Spec` with the corpus layers and outputs from Section 9.
7. Draft `Index_Contradiction_Resolution_Note` that closes the currently known A-through-G contradictions against section 18 and current code.
8. Convert lanes A through E into family engineering specs in the workplan order already defined by the index completion workplan, then land lanes F and G as shared cross-family engineering specs.
9. After the lane-derived engineering specs are drafted, rerun the Lane H contradiction review and replace the first-pass scorecard with a spec-wave closure scorecard.

Bottom line:

- ScratchBird already has enough implemented index reality to justify family-specific engineering specs now
- ScratchBird now has full research-lane coverage across the index program, including vector, cross-family MGA, and cross-family planner packets
- ScratchBird still does not have enough contradiction closure, gate evidence, or engineering-spec conversion to make release or planner-support claims across the full family surface
- Lane H should therefore be treated as the release-control lane: benchmark registry, contradiction closure, scorecards, and gates first, then engineering-spec conversion and gate execution before any full-program signoff
