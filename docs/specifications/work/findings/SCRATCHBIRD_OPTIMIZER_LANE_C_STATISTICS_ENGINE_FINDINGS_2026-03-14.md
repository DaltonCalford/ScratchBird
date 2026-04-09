# ScratchBird Optimizer Lane C Findings

Lane: C

Topic: Statistics engine and collection model

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

This lane defines the first-pass statistics engine and collection model that ScratchBird should expose to the optimizer.

The scope includes:
- Table statistics used for base-rowcount, storage, and freshness decisions.
- Single-column statistics covering `nullfrac`, average width, `ndistinct`, histograms, most-common values (MCVs), and correlation.
- Multicolumn statistics covering joint `ndistinct`, multicolumn MCVs, and soft functional dependencies.
- Join-key and expression statistics where single-column metadata is not sufficient for cardinality estimation.
- Collection mechanics: sampling, refresh policy, confidence/freshness modeling, and storage layout.
- A contract draft for what the planner may assume is present, optional, stale, or absent.

This lane does not attempt to finish the full cost model, join enumerator, or `ANALYZE` grammar. It supplies the statistics-side contract those later specifications should consume.

Success criteria for this first pass are:
- Preserve and formalize what ScratchBird already implements.
- Identify the minimum additional statistics families needed to move beyond single-column selectivity estimation.
- Separate data that should be persisted from data that may remain runtime-derived.
- Produce implementation-oriented recommendations rather than donor-engine description only.

## 2. ScratchBird Current-State Baseline

Current ScratchBird already has a real statistics subsystem, but it is still centered on single-column estimation.

Implemented now:
- Table-level metadata exists through the runtime table-statistics manager: live-row estimate, dead-row estimate, scan counts, DML counters, `mod_since_analyze`, analyze timestamps, and vacuum-related counters.
- Per-column persisted statistics include rowcount context, `num_nulls`, `null_fraction`, `num_distinct`, `avg_width`, histogram metadata and buckets, and an MCV list.
- Histogram collection already uses an equal-height model.
- MCV collection already stores the top values and frequencies.
- `ANALYZE` supports table-level, column-level, verbose, and explicit sample-rate forms.
- Sampling already uses reservoir sampling over a table scan.
- Freshness and confidence classes already exist and are propagated into planning provenance.
- Automatic re-analysis already exists via a modification-threshold trigger.
- Pairwise numeric cross-column correlation is already computed and stored.
- Limited expression statistics already exist for `LOWER(col)` and `UPPER(col)` on string columns.

Important current semantics:
- Default sample sizing is approximately 10 percent of the estimated table rowcount with a floor of 64 rows and a cap of 30,000 rows.
- Freshness classes are time- and churn-based: `FRESH`, `WARM`, `STALE`, `EXPIRED`.
- Confidence classes are sample-size- and sample-rate-based, then downgraded by staleness.
- Planner costing already penalizes plans that rely on warm, stale, expired, or low-confidence statistics.

Gaps that matter for Lane C:
- There is no dedicated persisted table-statistics object for optimizer use; table metadata is partly runtime-derived.
- `ndistinct` is estimated from sample distinct counts with simple extrapolation; there is no HyperLogLog-backed estimator yet.
- Histograms are not explicitly modeled as MCV-compressed histograms, so heavy hitters can distort remaining-bucket density.
- Correlation currently means cross-column numeric correlation, not the PostgreSQL-style physical-order correlation used for range-scan costing.
- There is no first-class multicolumn statistics object, so conjunctions, `GROUP BY`, and compound join keys still fall back to independence assumptions.
- There is no functional-dependency model.
- Join estimation remains mostly `1 / max(ndistinct_left, ndistinct_right)` for equality joins, with no skew, nullability, overlap, uniqueness, or foreign-key awareness.
- Expression statistics are narrow and synthetic; there is no general canonical expression statistics family.
- Confidence is coarse; there is no continuous score, no observed-q-error feedback, and no explicit storage of model coverage.

Baseline conclusion:
- ScratchBird should not replace its existing statistics system.
- ScratchBird should evolve it into a layered contract: table stats, single-column stats, extended multicolumn stats, optional join-key stats, and optional storage-segment stats.

## 3. Donor-Engine Research Synthesis

PostgreSQL provides the most complete donor model for optimizer-facing statistics.
- It separates base single-column statistics from extended statistics objects.
- Single-column stats include `nullfrac`, average width, `ndistinct`, MCVs, histograms, and physical-order correlation.
- `ndistinct` supports both absolute and relative encodings, which is valuable when a column behaves like "fraction of rowcount" rather than a stable absolute count.
- Extended statistics cover multicolumn `ndistinct`, soft functional dependencies, multicolumn MCVs, and expression statistics.
- Histograms are built after removing MCV values, which prevents heavy-hitter duplication in the histogram model.
- Extended statistics improve base-relation estimates strongly, but they are not a full cross-table join-statistics system.

MySQL is a useful donor for storage shape and refresh controls.
- Histogram metadata is persisted as an explicit JSON contract.
- Histogram metadata carries sampling rate, last-update time, requested bucket count, null fraction, collation identity, and an auto-update flag.
- Histogram type selection is simple and practical: use singleton histograms when the number of distinct values fits the bucket budget, otherwise use equi-height histograms.
- Auto-update can be opt-in per histogram, which is a good operational model for expensive or low-value columns.
- MySQL donor evidence is mostly per-column; it does not suggest a rich multicolumn statistics architecture.

DuckDB is a useful donor for low-cost metadata and propagation, not for full histogram-driven cardinality estimation.
- Core persisted storage statistics are lightweight: min/max-style range metadata, nullability, max string length, and approximate distinct tracking.
- Distinct estimation uses HyperLogLog with sample-aware correction.
- Optimizer propagation aggressively narrows bounds through filters and joins.
- This is strong for pruning, range narrowing, and low-overhead maintenance.
- Inference: DuckDB's main lesson for ScratchBird is that not every statistics family has to be rowset-wide and histogram-based; segment-local stats can coexist with richer optimizer stats.

Cross-engine synthesis:
- PostgreSQL is the best donor for logical optimizer statistics.
- MySQL is the best donor for persisted histogram envelope and per-object auto-refresh policy.
- DuckDB is the best donor for lightweight approximate distinct support and storage-attached pruning stats.
- No inspected donor supplied a complete, ready-made cross-table join-statistics model. ScratchBird will need a narrow first-pass join-key design rather than a full learned join model.

## 4. Primary Literature and Official-Document Synthesis

Official engine documentation aligns with the source-level findings.

PostgreSQL official documentation confirms:
- `ANALYZE` is the mechanism that collects planner statistics.
- Single-column statistics are automatic planner inputs.
- Extended statistics require explicit statistics objects plus `ANALYZE`.
- Auto-analyze is driven by a threshold plus scale-factor model rather than fixed time only.
- The documented planner model depends heavily on `nullfrac`, width, `ndistinct`, MCVs, histograms, and correlation for single-column clauses, then uses extended statistics to relax false independence assumptions.

MySQL official documentation confirms:
- Column histograms are explicit optimizer statistics objects.
- Histogram metadata is exposed in catalog views in serialized form.
- Auto-update is a real part of the histogram contract rather than only an implementation detail.
- Histograms are a complement to index metadata, not a replacement for it.

DuckDB official documentation confirms:
- Automatic zonemap-style min/max statistics exist and are central to pruning.
- Storage-introspection surfaces show statistics at storage granularity, not only table granularity.
- DuckDB's public emphasis is on lightweight pruning statistics and approximate metadata rather than PostgreSQL-style extended optimizer statistics.

First-pass synthesis for ScratchBird:
- The official-doc consensus is not "collect everything."
- The consensus is "collect a small number of high-value statistics families with explicit refresh semantics and clear planner meaning."
- Inference: ScratchBird should therefore favor a compact, typed contract over an unbounded "generic stats blob" design.

## 5. Normalized Algorithm Packet

Recommended collection model:

Table statistics:
- Persist a table-statistics record with `live_rows`, `dead_rows_estimate`, `pages`, `avg_row_width`, `mod_since_analyze`, `last_analyze_time`, `last_autoanalyze_time`, `sample_rows`, `sample_rate`, freshness, confidence, and snapshot identity.
- Prefer optimizer-facing table stats to be persisted snapshots, not only runtime counters, so planning is reproducible.

Single-column statistics:
- Collect `nullfrac`, average width, `ndistinct`, MCV list, histogram, and physical-order correlation for each eligible base column.
- Keep the current per-column reservoir sample as the base mechanism.
- Replace simple extrapolated `ndistinct` with a two-path estimator:
  - exact count when the sample is the full table or when the sample saturates the observed domain;
  - HLL-backed estimate otherwise.
- Store `ndistinct` in absolute form for stable small domains and relative form for high-cardinality domains close to table cardinality.
- Compute MCVs first.
- Build histograms from the remaining non-null, non-MCV sample so histogram density does not double-count heavy hitters.
- Use singleton histograms when remaining `ndistinct` fits the bucket budget; otherwise use equi-height histograms.

Correlation:
- Split the current concept into two families.
- `physical_correlation`: one-column statistic measuring correlation between value order and storage order for scan and index costing.
- `predicate_correlation`: multicolumn statistic measuring interaction among columns used together in filters.
- Preserve the current pairwise numeric correlation logic only as a possible seed for `predicate_correlation`; do not overload it as the sole correlation contract.

Multicolumn statistics:
- Add an explicit extended-statistics object keyed by table plus ordered column/expression set.
- First-pass families should be:
  - joint `ndistinct` for subsets up to width 3;
  - multicolumn MCV list for equality-heavy predicate combinations;
  - soft functional dependencies with degree values in `[0,1]`.
- Limit first-pass collection to column sets justified by one of: composite indexes, primary/unique keys, foreign keys, or repeated workload use.

Join-key statistics:
- Add a narrow join-key statistics family instead of a general cross-table statistics system.
- Per join-key set, store effective `ndistinct`, null fraction, top heavy hitters, uniqueness/key-role metadata, and optional overlap sketch state.
- Inference: overlap sketches can be deferred initially; the heavy-hitter intersection and uniqueness/FK metadata provide most of the early value.

Expression statistics:
- Promote expression stats from ad hoc synthetic rows to a declared statistics family.
- Restrict first pass to immutable deterministic scalar expressions and indexed expressions.
- Canonicalize expression text or expression tree identity so the planner and collector resolve the same object.

Sample design:
- Keep one uniform reservoir sample per analyzed table scan.
- Allow extended-statistics objects to raise the required sample size when their target width or skew demands it.
- Reuse the base sample for all single-column stats and for most multicolumn stats to keep collection cost bounded.
- Inference: a second focused pass should be optional only for explicitly declared high-value join-key or multicolumn objects.

Refresh policy:
- Keep threshold-based auto-analyze.
- Add per-statistics-object policy: `manual`, `auto`, `inherit-table-policy`.
- Refresh expensive extended statistics only when one of these is true:
  - enough rows changed;
  - a participating column changed significantly;
  - observed plan error identifies the object as low-confidence.

Confidence and freshness:
- Preserve the current coarse classes for compatibility.
- Add continuous scores so planner consumers can trade off multiple imperfect stats objects rather than only branch on classes.

Storage tradeoffs:
- Keep JSON or TOAST-style payloads for first-pass compatibility where ScratchBird already serializes histograms and MCVs.
- Introduce typed object families instead of storing every new concept inside synthetic overloaded column-stat records.
- Inference: binary payloads should be a later optimization after object boundaries stabilize.

## 6. Formula and Heuristic Packet

Recommended first-pass formulas:

Table rowcount and width:
- `live_rows` comes from the analyze snapshot, not from current runtime counters alone.
- `avg_row_width = mean(encoded_row_width(sample_row))`.
- `pages` remains storage-derived.

Null fraction and width:
- `nullfrac = null_count / sample_rows`.
- `avg_width = mean(encoded_value_width(non_null_sample_values))`.

Distinct count:
- If `sample_rows == live_rows`, use exact `ndistinct`.
- Otherwise use an HLL-backed estimate capped to `[1, live_rows_non_null]`.
- Store `ndistinct_abs` when `estimate <= 0.95 * live_rows`.
- Store `ndistinct_rel = -(estimate / live_rows)` when `estimate > 0.95 * live_rows`.
- Reserve `-1.0` for known-unique semantics when a validated unique or primary key proves uniqueness.

MCV selection:
- Let `mcv_budget = min(100, max(16, statistics_target / 2))`.
- Keep values with highest estimated frequency until either the budget is full or remaining values fall below a minimum support floor.
- Recommended support floor: `max(2 / sample_rows, 0.005)`.
- Store each MCV entry as `(value, estimated_frequency, base_count)`.

Histogram construction:
- Remove nulls and selected MCV values from the histogram input.
- If remaining `ndistinct <= bucket_budget`, emit a singleton histogram.
- Else emit an equi-height histogram with monotone bucket bounds and bucket frequencies summing to the non-MCV mass.
- Recommended `bucket_budget = min(100, statistics_target)`.

Physical-order correlation:
- Compute correlation between value rank and storage-order rank on the sampled rows.
- Persist a coefficient in `[-1, 1]`.
- Values near `1` mean ascending physical locality, near `-1` descending locality, and near `0` random order.

Soft functional dependency:
- Store dependency degree `d` in `[0,1]` for determinant set `A` and dependent set `B`.
- Clause reduction rule for `A = a AND B = b`:
- `P(A = a AND B = b) = P(A = a) * (d + (1 - d) * P(B = b))`.
- Apply the same degree only when all determinant columns are constrained.

Multicolumn `ndistinct`:
- Store selected subset counts for declared column sets up to width 3.
- Use the multicolumn count directly for `GROUP BY`, `DISTINCT`, and conjunction cardinality when the constrained subset matches.

Join-key estimation:
- For equality join on key sets `K_left`, `K_right`:
- `heavy_sel = sum(freq_left(v) * freq_right(v))` over shared heavy hitters.
- `uniform_sel = overlap_ratio * max(0, 1 - left_heavy_mass) * max(0, 1 - right_heavy_mass) / max(ndv_left_eff, ndv_right_eff)`.
- `join_sel = heavy_sel + uniform_sel`.
- `overlap_ratio = 1.0` when no overlap evidence exists; refine it when overlap sketches are available.
- If validated FK-to-unique-key metadata exists, clamp join rows so the referencing side cannot produce more matches than its non-null rowcount.

Freshness score:
- Keep current class thresholds.
- Add `mod_ratio = modified_rows_since_analyze / max(1, live_rows)`.
- Recommended continuous score:
- `freshness_score = 0.5 * exp(-age_seconds / target_age_seconds) + 0.5 * max(0, 1 - mod_ratio / stale_mod_ratio)`.
- Suggested first-pass parameters: `target_age_seconds = 3600`, `stale_mod_ratio = 0.20`.

Confidence score:
- Recommended score in `[0,1]`:
- `confidence_score = 0.45 * sample_component + 0.25 * freshness_score + 0.20 * coverage_component + 0.10 * validation_component`.
- `sample_component = min(1, sample_rows / target_sample_rows)`.
- `coverage_component` reflects how much probability mass is explained by MCVs, histograms, or extended stats.
- `validation_component` starts neutral and is adjusted later from observed q-error.
- Preserve current `LOW`, `MEDIUM`, `HIGH` classes as score buckets for backward compatibility.

Auto-refresh heuristics:
- Keep `max(64, live_rows / 5)` as a reasonable table-level modification trigger for first-pass continuity.
- Add per-object suppression so low-value stats are not rebuilt every table analyze.
- Rebuild a multicolumn object when any participating column exceeds the table trigger or when the object has low confidence and was used recently.

## 7. ScratchBird Contract Draft

Recommended contract shape:

`table_statistics` object:
- Identity: table id, snapshot id, analyze timestamp.
- Core fields: `live_rows`, `dead_rows_estimate`, `pages`, `avg_row_width`.
- Provenance: `sample_rows`, `sample_rate`, source kind, analyzer version.
- Freshness: class, score, `modified_rows_since_analyze`.
- Confidence: class, score.

`column_statistics` object:
- Identity: table id, column id, snapshot id.
- Core fields: `nullfrac`, `avg_width`, `ndistinct`, `ndistinct_encoding`.
- Distribution fields: MCV list, histogram kind, histogram payload, histogram bucket count.
- Correlation fields: `physical_correlation`.
- Provenance and quality: same freshness/confidence envelope as table stats.

`extended_statistics` object:
- Identity: table id, stats object id, ordered key list of columns and/or canonical expressions.
- Kind set: `multicolumn_ndistinct`, `multicolumn_mcv`, `functional_dependencies`, `predicate_correlation`, `expression`.
- Payload: typed payload per kind, not a synthetic overload of column-stat columns.
- Policy: collection mode, target size, auto-refresh mode.

`join_key_statistics` object:
- Identity: relation pair or declared reusable key-domain object, plus key-column lists.
- Core fields: left/right effective `ndistinct`, left/right null fractions, heavy hitters, relationship annotations.
- Optional fields: domain-overlap sketch, last validation q-error.
- Status: first-pass optional; planner must degrade cleanly if absent.

`segment_statistics` object:
- Identity: table id, segment or row-group id, column id.
- Core fields: min/max or prefix min/max, nullability flags, max string length, approximate distinct sketch.
- Purpose: pruning and runtime narrowing, not replacement for table-level optimizer stats.
- Inference: this family can start as storage-owned metadata and be surfaced to the optimizer later.

Planner contract rules:
- Every statistics lookup returns one of: exact object, stale object, low-confidence object, or missing object.
- Planner consumers must treat freshness and confidence as first-class inputs, not annotation only.
- If an extended stats object is missing, the planner falls back to single-column logic.
- If join-key stats are missing, the planner falls back to single-column `ndistinct` plus key metadata.
- If a stats object is stale but still the best available, its estimate is usable with a planner penalty multiplier.

Collection contract rules:
- `ANALYZE` on a table refreshes table stats and all `inherit-table-policy` objects.
- Object-specific auto-refresh may skip expensive objects when modification evidence is weak.
- Explicit analyze of a column or stats object must preserve the table snapshot envelope but may update only the targeted object.

Storage tradeoffs and implementation recommendation:
- Preserve the current serialized histogram and MCV storage path for the first migration so existing code paths remain usable.
- Add catalog families for table stats and extended stats instead of encoding them as synthetic column rows.
- Prefer typed headers plus payload blobs over free-form JSON-only storage.
- Keep JSON serialization as a debug/export representation.
- Inference: parse cost and catalog bloat will become material if every future stats family is stored as JSON text only.

## 8. Validation and Benchmark Packet

Validation should prove both correctness of collected metadata and usefulness for planning.

Round-trip and invariants:
- Persist and reload every statistics family without loss of bucket order, frequency totals, or provenance fields.
- Verify every probability-bearing payload sums to valid mass.
- Verify `nullfrac`, MCV mass, and histogram mass do not exceed `1.0`.
- Verify relative `ndistinct` decodes correctly against table rowcount changes.

Synthetic data cases:
- Uniform column, low-NDV column, high-NDV near-unique column.
- Heavy skew with one or more dominant values.
- Range-correlated table order for testing `physical_correlation`.
- Compound predicates with strong positive dependency.
- Compound predicates with independence.
- Functional dependency examples such as `zip -> city`-style data.
- Composite join keys with skew and nullable foreign keys.
- Expression filters on indexed and non-indexed immutable expressions.

Benchmark metrics:
- Predicate selectivity q-error for equality, range, null, conjunction, disjunction, and join predicates.
- Cardinality q-error at scan, join, and aggregation nodes.
- Plan-choice stability before and after statistics refresh.
- Collection runtime, sample size, payload size, and catalog growth.
- Benefit of extended stats compared with single-column-only estimation.

Refresh-policy validation:
- Measure how quickly confidence drops under insert-heavy, update-heavy, and delete-heavy churn.
- Verify that auto-refresh does not thrash on hot small tables.
- Verify that stale but high-value extended stats are preferred over missing stats when appropriate.

Comparative validation:
- Reproduce representative workloads where PostgreSQL-style extended stats are known to help: correlated predicates, multicolumn `GROUP BY`, and skewed equality filters.
- Reproduce workloads where MySQL-style singleton histograms outperform coarse equi-height histograms on low-NDV columns.
- Reproduce workloads where DuckDB-style lightweight segment stats improve pruning even when global histograms are unchanged.

Recommended acceptance targets for first implementation:
- Median predicate q-error no worse than current ScratchBird on any existing test.
- Material improvement on correlated-conjunction and skewed-equality workloads.
- No more than moderate catalog-growth overhead for default-enabled stats.
- `ANALYZE` runtime increase should remain bounded by one table scan plus bounded post-processing for default-enabled objects.

## 9. Adopt/Adapt/Reject/Defer Matrix

| Item | Donor | Decision | Rationale |
| --- | --- | --- | --- |
| Single-column `nullfrac`, width, `ndistinct`, MCV, histogram core | PostgreSQL | Adopt | Already aligns with ScratchBird and is the right optimizer baseline. |
| Relative negative `ndistinct` encoding for near-unique domains | PostgreSQL | Adapt | Valuable, but ScratchBird should store the encoding explicitly instead of relying on overloaded sign only. |
| MCV-compressed histogram build order | PostgreSQL | Adopt | Prevents heavy hitters from corrupting histogram density. |
| Physical-order correlation statistic | PostgreSQL | Adapt | Needed, but separate from current cross-column correlation. |
| Multicolumn `ndistinct` statistics objects | PostgreSQL | Adopt | High planner value and low conceptual risk. |
| Soft functional dependencies | PostgreSQL | Adopt | Best available first-pass model for correlated equality predicates. |
| Multicolumn MCV lists | PostgreSQL | Adapt | High value, but scope should be limited to declared narrow column sets first. |
| Expression statistics objects | PostgreSQL | Adapt | Needed, but first pass should restrict to immutable deterministic expressions. |
| Explicit histogram JSON-style envelope with last update, sample rate, and auto-refresh flag | MySQL | Adapt | Good operational metadata pattern; store as typed fields with optional JSON export. |
| Singleton histogram fallback for low-NDV columns | MySQL | Adopt | Cheap and clearly correct when bucket budget covers the domain. |
| Per-object auto-update policy | MySQL | Adopt | Prevents rebuild cost from spreading to every stats object. |
| JSON-only histogram persistence as the long-term storage model | MySQL | Reject | Useful for compatibility, but not sufficient as the only long-term representation. |
| Lightweight segment-local min/max and nullability stats | DuckDB | Adapt | Strong complement for pruning and runtime refinement. |
| HyperLogLog-backed approximate distinct statistics | DuckDB | Adopt | Better than current simple extrapolation for high-NDV columns. |
| Histogram-free optimizer design | DuckDB | Reject | ScratchBird needs richer selectivity metadata than zonemaps alone provide. |
| Full cross-table learned join-statistics system | None inspected | Defer | Useful eventually, but not required for the first optimizer contract. |
| Overlap sketches for join-key domains | None inspected | Defer | Promising, but heavy-hitter plus key-role metadata should come first. |
| Arbitrary expression statistics on all scalar expressions | None inspected | Defer | Too broad for the first implementation and costly to maintain. |

## 10. Open Questions and Integration Dependencies

Open questions:
- How should statistics objects be declared: implicit from indexes and constraints only, or explicit via a `CREATE STATISTICS`-style DDL as well?
- Should table analyze always refresh single-column stats for every column, or should low-value columns become opt-out?
- How much planner complexity is acceptable in the first join-key model before overlap sketches exist?
- Should `physical_correlation` be collected for all comparable data types or numeric/date types only in phase one?
- How should expression identity be canonicalized so semantically equivalent expressions share one stats object?

Integration dependencies:
- Catalog work is required for first-class table and extended-statistics objects.
- `ANALYZE` execution must expose per-object sample and refresh controls.
- Planner/cardinality-estimation work must consume multicolumn objects and key-role metadata.
- Constraint metadata must be available to statistics collection so unique and foreign-key semantics can inform join-key stats.
- Runtime telemetry should record observed rowcount error so confidence can later incorporate validation feedback.
- Storage-layer metadata should expose segment-level stats without forcing the logical optimizer to depend on storage internals.

Design constraints carried forward:
- Statistics absence must be safe; planner fallback must remain deterministic.
- Collection must remain bounded in cost on large tables.
- Freshness/confidence semantics must be stable enough that later planner lanes can depend on them.
- Inference: the implementation should prefer a narrow, typed set of statistics families over a generic "plugin stats" model until planner consumers stabilize.

## 11. Recommended Next-Step Specification Tasks

1. Define catalog schemas for `table_statistics`, `column_statistics`, and `extended_statistics`, including typed payload boundaries and snapshot provenance.
2. Specify the `ndistinct` encoding contract, including exact, estimated-absolute, and estimated-relative forms.
3. Specify histogram construction precisely: MCV exclusion, singleton fallback, bucket semantics, null handling, and serialization format.
4. Define `physical_correlation`, `predicate_correlation`, and their distinct planner uses so current cross-column correlation is not overloaded.
5. Specify first-pass extended statistics families and creation rules: multicolumn `ndistinct`, multicolumn MCV, functional dependencies, and expression stats.
6. Define the join-key statistics minimum viable object: heavy hitters, effective `ndistinct`, nullability, uniqueness/FK annotations, and fallback behavior.
7. Extend freshness/confidence from classes to scored values while preserving current class thresholds for compatibility.
8. Define auto-refresh policy and object-level overrides, including when table-level `ANALYZE` is allowed to skip expensive objects.
9. Define validation workloads and q-error acceptance thresholds before implementation begins.
10. Sequence implementation in phases:
- Phase 1: table-statistics persistence and single-column contract cleanup.
- Phase 2: HLL-backed `ndistinct` and MCV-compressed histograms.
- Phase 3: physical correlation and extended stats catalog.
- Phase 4: multicolumn and functional-dependency estimation.
- Phase 5: join-key and expression statistics.
