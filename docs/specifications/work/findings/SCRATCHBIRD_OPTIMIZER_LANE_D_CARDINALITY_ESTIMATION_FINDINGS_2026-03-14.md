# ScratchBird Optimizer Lane D Findings

Lane: D

Topic: Cardinality estimation

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane D covers cardinality estimation for ScratchBird optimizer planning: base-table filter selectivity, join selectivity and output rows, grouping and `DISTINCT` row counts, estimator confidence, and adaptive repair hooks for repeated misestimates.

This first pass uses ScratchBird current code as the baseline and synthesizes donor-engine patterns from PostgreSQL, MySQL, and DuckDB. The target is a deterministic, statistics-driven estimator that fits the current ScratchBird `ANALYZE`, plan-provenance, and query-profiler infrastructure without assuming a learned model.

The intended output of Lane D is not just a better scalar row estimate. It is an estimator packet that:
- produces row counts for scans, joins, aggregates, and deduplication;
- explains which statistics drove the estimate;
- emits a confidence level and error-band hint;
- exposes repair hooks when q-error is repeatedly bad on the same clause bundle or join edge.

Out of scope for this first pass:
- a fully learned cardinality-estimation model;
- broad runtime re-optimization beyond the bounded feedback hooks already present;
- exhaustive donor coverage outside PostgreSQL, MySQL, and DuckDB.

## 2. ScratchBird Current-State Baseline

ScratchBird already has more estimator infrastructure than the current row-count quality suggests.

Current statistics inventory:
- Per-column statistics include row count, null count and fraction, `num_distinct`, average width, histogram type and buckets, and MCV entries with frequencies.
- Statistics metadata already includes `stats_snapshot_id`, sample size, sample rate, modified-rows-since-analyze, staleness class, and confidence class.
- `ANALYZE` uses reservoir sampling and caps default sample collection at about 30,000 rows when a full-table sample is not requested.
- ScratchBird computes pairwise numeric column correlations and persists them.
- ScratchBird also computes synthetic expression statistics, but only for `LOWER(col)` and `UPPER(col)` on text columns.

Current clause-estimation behavior:
- Equality on a base column already follows a sound first-order pattern:
  - exact MCV frequency if the literal is in the MCV list;
  - otherwise residual mass divided by residual distinct count.
- `IS NULL` and `IS NOT NULL` use stored null fraction when column stats are present.
- Range predicates use histogram interpolation over stored buckets.
- `BETWEEN` is implemented as `sel(col >= low) - sel(col > high)`.
- `LIKE` uses fixed heuristics for prefix, suffix, and contains patterns, except an exact no-wildcard `LIKE` is treated as equality.
- `IN` sums equality selectivities and caps at `1.0`.
- `AND`, `OR`, and `NOT` are handled with independence-style formulas.

Current multi-predicate behavior:
- `AND` defaults to independence multiplication.
- There is a limited same-table correlation adjustment:
  - if both `AND` inputs resolve directly to columns from the same table;
  - and numeric pairwise correlation stats exist;
  - ScratchBird nudges the independence product upward for positive correlation and downward for negative correlation.
- This is only a heuristic nudge. It is not multivariate MCV, functional-dependency, or multivariate-ndistinct estimation.

Current join-estimation behavior:
- Equi-join selectivity is currently `1 / max(ndv_left, ndv_right)` when both sides have stats.
- Range joins fall back to the generic range heuristic.
- Compound join predicates use the same `AND`/`OR` independence formulas as local filters.
- There is no dedicated handling for:
  - MCV-vs-MCV join overlap;
  - unique-key or foreign-key joins;
  - non-equality join integration over histograms;
  - semi joins, anti joins, or outer joins as distinct row-count semantics.

Current grouping and deduplication behavior:
- Group cardinality and `DISTINCT` output are still fixed `current_rows / 10` heuristics.
- There is no NDV-based or multivariate-ndistinct-based group estimation.

Current uncertainty and repair scaffolding:
- Planner payloads already capture stats provenance, staleness, confidence, sample ratio, and modified rows.
- Costing already applies penalties for stale or low-confidence statistics.
- Query profiling already captures estimated rows, actual rows, correction factor, observation count, a max-estimation-error-ratio threshold, replan gating, and stats-refresh requests.

Current gap summary:
- Good single-column equality logic exists, but range logic does not yet combine MCV and histogram mass cleanly.
- There is no true multivariate statistics layer for correlated predicates.
- Join estimation is much weaker than base-filter estimation.
- Grouping and `DISTINCT` estimation are still placeholder heuristics.
- Confidence exists as metadata, not as an estimator output contract.
- Feedback exists as infrastructure, but not yet as q-error-oriented estimator repair policy.

## 3. Donor-Engine Research Synthesis

### PostgreSQL

PostgreSQL is the strongest donor for clause-level selectivity formulas and multivariate statistics.

Observed donor patterns:
- Single-column equality estimation uses exact MCV frequency when available, then spreads residual non-null, non-MCV mass over residual distinct values.
- Scalar range estimation uses histogram interpolation.
- When both histogram and MCV data exist, PostgreSQL applies the predicate to MCV entries exactly and applies histogram interpolation only to the non-MCV population, then combines the two contributions.
- Equality-join estimation (`eqjoinsel`) compares MCV lists across both sides when available and handles the remaining population separately.
- Extended statistics support:
  - soft functional dependencies for correlated equality predicates;
  - multivariate MCV lists for value-level combinations;
  - multivariate `ndistinct` estimates for grouping and `DISTINCT`.
- PostgreSQL applies multivariate MCV first and functional dependencies afterward on the remaining clauses for a single base relation.

Takeaway for ScratchBird:
- Adopt PostgreSQL as the primary donor for base-predicate math.
- Adapt PostgreSQL extended-statistics patterns to ScratchBird rather than trying to stretch the current Pearson-correlation nudge.

### MySQL

MySQL is the strongest donor for join-type output-row formulas and for explicit safety bias against catastrophic underestimation.

Observed donor patterns:
- Histograms are explicitly typed as singleton or equi-height.
- Histogram metadata tracks null fraction and bucket cumulative frequencies.
- Histogram maintenance is not incremental.
- Equality selectivity can draw from index-prefix cardinality (`records_per_key`) and uses longest usable prefix statistics where possible.
- When multiple candidate selectivity sources exist, MySQL prefers the less selective estimate if needed to avoid dangerous underestimation.
- Join-output formulas are explicit:
  - inner join: `left_rows * right_rows * selectivity`;
  - left outer join: preserve at least one output row per left row;
  - semijoin: left rows times a match fan-out estimate;
  - antijoin: complement of semijoin fan-out, with a floor to avoid estimating near-zero too aggressively.

Takeaway for ScratchBird:
- Adapt MySQL's explicit join-type row formulas.
- Adapt the policy bias that some uncertainty should lean toward mild overestimation, especially for anti joins and ambiguous join fan-outs.

### DuckDB

DuckDB is the strongest donor for lightweight distinct-domain tracking and statistics propagation across joins.

Observed donor patterns:
- Distinct statistics use HyperLogLog with Good-Turing-style correction.
- Join-order estimation groups equivalent join keys into relation sets and tracks total distinct domains for those sets.
- Non-equality joins are damped heuristically instead of treated as full cross products.
- Statistics propagation can prove some joins empty or always true and can propagate nullability changes across outer joins.
- DuckDB's join-order cardinality machinery is intentionally heuristic and optimized for search quality, not for exact clause-by-clause estimation.

Takeaway for ScratchBird:
- Adapt DuckDB's idea of relation-set distinct tracking and lightweight propagation.
- Do not adopt DuckDB's join estimator as the sole final row-count formula packet.

### Synthesis

Recommended donor split:
- PostgreSQL: base predicates, multivariate clause correction, join MCV overlap, `ndistinct` group estimation.
- MySQL: join-type output-row semantics, conservative anti-join floor, safety bias against catastrophic underestimation.
- DuckDB: HLL-backed distinct sketches, relation-set equivalence tracking, contradiction and nullability propagation.

Inference: ScratchBird should not pick a single donor wholesale. The highest-leverage path is PostgreSQL-style statistics math wrapped in MySQL-style join-type row semantics, with DuckDB-style cheap distinct sketches for join-order search and future propagation.

## 4. Primary Literature and Official-Document Synthesis

Official-engine documentation and benchmark literature align on the same core point: cardinality quality, not raw cost-model sophistication, dominates optimizer quality once joins and correlated predicates become nontrivial.

Primary synthesis points:
- PostgreSQL official planner-statistics documentation gives concrete row-estimation examples for:
  - equality with MCVs and residual NDV;
  - histogram-based range estimation;
  - equality-join estimation;
  - multivariate repair using dependencies, multivariate MCV, and multivariate `ndistinct`.
- PostgreSQL extended-statistics documentation and source notes show a practical order of operations:
  - multivariate MCV for value-level corrections first;
  - functional dependencies next for equality and null-style clauses;
  - `ndistinct` for group and `DISTINCT` output.
- MySQL optimizer and histogram documentation reinforce two useful operational ideas:
  - histogram shapes should be explicit;
  - severe underestimation is often riskier than mild overestimation.
- The Join Order Benchmark paper ("How Good Are Query Optimizers, Really?") showed that multi-join plan failures are dominated by cardinality errors and that even strong cost models cannot rescue bad cardinalities.
- The Cardinality Estimation Benchmark work ("Are We Ready for Learned Cardinality Estimation?") showed that mean q-error alone is not sufficient and that robustness under updates, workload shift, and practical operating constraints matters.
- The LEO work on adaptive cardinality estimation showed that bounded feedback on repeated misestimates can improve planning, but only if the feedback loop is operationally narrow and observable.

Implications for ScratchBird:
- Lane D should optimize for q-error percentiles and bad-plan prevention, not just average error.
- The first production-worthy step is a deterministic estimator with explicit confidence and repair hooks.
- Learned CE is a later option, not a prerequisite for meaningful improvement.

Inference: ScratchBird should treat cardinality estimation as a staged system:
1. statistics-based deterministic formulas;
2. confidence and provenance;
3. bounded adaptive correction;
4. only later, if still needed, learned components behind the same contract.

## 5. Normalized Algorithm Packet

The estimator should run as a normalized packet with clear ordering and explicit fallback stages.

### Step 1: Normalize the query into estimation units

Split the logical shape into:
- base-relation predicate bundles;
- join edges, each tagged with join type and join predicate class;
- grouping key sets;
- `DISTINCT` key sets;
- null-rejecting and contradiction-prone predicates.

Normalization rules:
- deduplicate `IN`-list literals before estimation;
- canonicalize reversible predicates so `5 < col` becomes `col > 5`;
- resolve supported expression statistics keys before falling back to generic expression heuristics;
- track clause bundles by relation and covered column set so multivariate stats can match them.

### Step 2: Load the best available statistics layer

For each relevant bundle, look up in descending priority:
1. exact special-case metadata:
   - uniqueness or key constraints when available;
   - exact expression statistics for supported expressions;
2. single-column stats:
   - null fraction, NDV, MCV, histogram;
3. multivariate stats:
   - multivariate MCV;
   - soft dependencies;
   - multivariate `ndistinct`;
4. pairwise correlation fallback;
5. heuristics.

### Step 3: Estimate single-clause selectivities

Use exact or near-exact per-clause formulas for:
- equality and inequality;
- null checks;
- range and `BETWEEN`;
- `LIKE`;
- `IN`.

Each clause estimate should also emit:
- the statistics basis used;
- a confidence contribution;
- whether the clause is exact, approximate, or heuristic.

### Step 4: Estimate same-relation clause bundles

Apply bundle correction in this order:
1. multivariate MCV if a bundle-level MCV object covers the clause set;
2. functional-dependency reduction on remaining compatible equality and null clauses;
3. independence multiplication on the residual unsupported clauses;
4. pairwise-correlation nudge only if no better multivariate statistics exist.

This order follows the strongest donor evidence and avoids overusing the current Pearson heuristic.

### Step 5: Convert selectivity to base rows

Maintain fractional cardinalities internally for join-order search and only round late.

Recommended rule:
- internal rows: `rows_f = max(0.0, table_rows * sel_total)`;
- materialized estimate: `rows = max(1, round(rows_f))` unless contradiction analysis can prove zero.

### Step 6: Estimate joins in two phases

Phase A: estimate join selectivity from predicate shape.
- equality join;
- non-equality join;
- compound join predicate.

Phase B: turn selectivity into output rows based on join type.
- inner;
- left/right/full outer;
- semi;
- anti.

### Step 7: Estimate grouping and `DISTINCT`

Use multivariate `ndistinct` if present. Otherwise build an effective NDV from per-key NDVs with dependency damping, then convert that NDV into expected output groups using an occupancy-style formula.

### Step 8: Attach confidence and error band

Every estimate should return:
- point estimate;
- confidence score or class;
- low/high error band;
- basis enum;
- snapshot IDs and freshness summary.

### Step 9: Emit adaptive repair hooks

For repeated large q-error:
- request stats refresh if stale or under-sampled;
- cache a bounded correction factor keyed by clause bundle or join edge;
- escalate to multivariate-stats recommendation if the same correlated column set fails repeatedly.

Inference: This normalized packet is a better architectural target for ScratchBird than expanding ad hoc logic inside the current recursive predicate walker. The recursive walker can remain, but it should call explicit bundle estimators rather than directly combining all logic inline.

## 6. Formula and Heuristic Packet

### 6.1 Notation

Use the following symbols:
- `N`: input row count for the relation being filtered.
- `f_null`: null fraction for a column.
- `ndv`: estimated non-null distinct count.
- `M`: MCV set for a column.
- `freq(v)`: stored MCV frequency for value `v`.
- `F_M = sum(freq(v) for v in M)`.
- `tail_mass = max(0, 1 - f_null - F_M)`.
- `tail_ndv = max(1, ndv - |M|)`.
- `rows(x) = input_rows * sel(x)` unless otherwise stated.

### 6.2 Base-predicate formulas

Equality:
- `sel(col IS NULL) = f_null`
- `sel(col IS NOT NULL) = 1 - f_null`
- `sel(col = v) = freq(v)` if `v` is in `M`
- `sel(col = v) = tail_mass / tail_ndv` otherwise
- `sel(col <> v) = max(0, 1 - f_null - sel(col = v))`

`IN` and `NOT IN`:
- `sel(col IN {v1...vk}) = min(1 - f_null, sum(sel(col = vi)) over deduplicated non-null literals)`
- `NOT IN` should not be implemented as plain `1 - sel(IN)` when the literal set includes `NULL`.
- Inference: first-pass ScratchBird should ignore `NULL` list entries for positive `IN`, and mark `NOT IN` involving `NULL` as low-confidence heuristic unless executor semantics are modeled explicitly.

Boolean composition:
- `sel(A AND B) = sel(A) * sel(B)` for the residual independent case.
- `sel(A OR B) = sel(A) + sel(B) - sel(A) * sel(B)` for the residual independent case.
- `sel(NOT A) = 1 - sel(A)` if null semantics are already absorbed into `sel(A)`.

### 6.3 Histogram and MCV usage

Range predicates should combine exact MCV evaluation with histogram interpolation over the non-MCV population:
- `sel(col op c) = sel_mcv(op, c) + tail_mass * hist_frac(op, c)`
- where `sel_mcv(op, c) = sum(freq(v) for v in M if v op c)`
- and `hist_frac(op, c)` is the estimated fraction of non-MCV, non-null values satisfying the predicate based on histogram interpolation.

`BETWEEN`:
- `sel(l <= col AND col <= u) = sel_mcv([l,u]) + tail_mass * hist_frac([l,u])`
- equivalently, `sel(col >= l) - sel(col > u)` if the implementation prefers the current decomposition.

Important implementation note:
- ScratchBird currently interpolates directly over histogram bucket frequencies but does not explicitly separate out MCV mass in range estimation.
- Recommendation: store or derive histogram mass as representing the tail population, not the full non-null population, so exact MCV contributions can be added without double counting.

### 6.4 Range estimation details

Numeric and timestamp types:
- Use linear interpolation within equal-height buckets unless a better bucket-local model exists.

String types:
- For prefix `LIKE 'abc%'`, estimate as a string range:
  - lower bound = `'abc'`
  - upper bound = smallest string greater than all strings with prefix `'abc'`
- This is better than a fixed 10% heuristic when histogram ordering is meaningful.
- Inference: ScratchBird can reuse its current prefix-ranking machinery as the first string-range implementation, but should mark collation-sensitive or locale-sensitive estimates as lower confidence.

Suffix and contains `LIKE`:
- `LIKE '%x'` and `LIKE '%x%'` remain heuristic in first pass.
- Suggested bootstrapping defaults:
  - suffix: `0.05`
  - contains: `0.01`
- These are acceptable only with low confidence and should be candidates for feedback-based correction.

### 6.5 Correlated-predicate handling

Preferred order for same-relation correlated predicates:

1. Multivariate MCV:
- exact bundle contribution = sum of multivariate MCV tuple frequencies that satisfy the full clause bundle.
- This handles impossible combinations and high-frequency combinations directly.

2. Functional dependencies:
- For dependency `a => b` with degree `d`,
  - `P(a=?, b=?) = P(a=?) * (d + (1 - d) * P(b=?))`
- More generally, use the widest and strongest dependency first, then recurse.

3. Residual independence:
- Multiply the remaining unsupported clauses after multivariate corrections are consumed.

4. Pairwise correlation fallback:
- Use only when no multivariate MCV or dependency object covers the bundle.
- Inference: for positive correlation, blend toward `min(selA, selB)`; for negative correlation, blend toward `max(0, selA + selB - 1)` rather than using independence unchanged.

Multivariate MCV correction policy:
- Inference: a practical first-pass formula for a covered clause bundle is:
  - `sel_bundle = clamp(sel_independent + (mcv_match_freq - mcv_match_base_freq), 0, 1)`
- where:
  - `mcv_match_freq` is the sum of actual multivariate MCV frequencies matching the bundle;
  - `mcv_match_base_freq` is the sum of base frequencies for those same combinations under independence.
- This uses multivariate MCV as a delta correction instead of replacing the whole estimate with only listed combinations.

### 6.6 Equality and non-equality joins

Equality joins should be estimated in two parts.

Part 1: exact hot-key overlap from MCVs:
- `sel_eq_mcv = sum(freq_L(v) * freq_R(v) for values v present in both MCV lists)`

Part 2: residual tail overlap:
- `tail_mass_L = max(0, 1 - f_null_L - F_ML)`
- `tail_mass_R = max(0, 1 - f_null_R - F_MR)`
- `tail_ndv_L = max(1, ndv_L - |M_L|)`
- `tail_ndv_R = max(1, ndv_R - |M_R|)`
- `sel_eq_tail = (tail_mass_L * tail_mass_R) / max(tail_ndv_L, tail_ndv_R)`

Then:
- `sel_eq_join = sel_eq_mcv + sel_eq_tail`

Fallback equality join when MCV overlap is unavailable:
- `sel_eq_join = ((1 - f_null_L) * (1 - f_null_R)) / max(ndv_L, ndv_R)`
- This is already closer to PostgreSQL than ScratchBird's current bare `1/max(ndv)` because it preserves null semantics.

Not-equal join:
- `sel(L <> R) = max(0, (1 - f_null_L) * (1 - f_null_R) - sel(L = R))`

Range and inequality joins:
- Preferred first-pass implementation for comparable scalar types:
  - integrate bucket-pair overlap and order probability across both histograms;
  - for each bucket pair, assume uniformity within the pair and accumulate the fraction satisfying `<`, `<=`, `>`, or `>=`.
- If min/max or histogram bounds prove disjointness, return `0`.
- If one side dominates the other entire domain, return `1 - f_null` adjusted for the pair where appropriate.
- Fallback when cross-histogram integration is unavailable:
  - keep a bounded heuristic such as `0.33`, but mark low confidence.
- Inference: DuckDB-style denominator dampening is useful in join-order search, but ScratchBird should not present it as a final production row estimate without stronger histogram evidence.

Compound join predicates:
- `AND`: multiply only after each join clause has had its own best-effort estimation.
- `OR`: use inclusion-exclusion and low confidence unless overlap is modeled explicitly.

### 6.7 Semi, anti, and outer joins

Semijoin should estimate left-side match probability, not inner-join output multiplicity.

Recommended first-pass semijoin model:
- `p_match_left = min(1, ndv_match_right / max(ndv_left, 1)) * sel_residual`
- `rows_semi = left_rows * clamp(p_match_left, 0, 1)`
- where:
  - `ndv_match_right` is estimated distinct RHS join-key values surviving RHS-local filters;
  - `sel_residual` captures any additional non-key join predicates.

Antijoin:
- `rows_anti = left_rows * max(1 - p_match_left, anti_floor)`
- Adopt `anti_floor = 0.10` as the first-pass safety floor, following the MySQL rationale that underestimating anti joins can be catastrophic.

Left outer join:
- `rows_left_outer = left_rows * max(right_rows * sel_join, 1.0)`

Right outer join:
- `rows_right_outer = right_rows * max(left_rows * sel_join, 1.0)`

Full outer join:
- Inference: first-pass estimate can be:
  - `rows_full_outer = rows_inner + left_rows * max(0, 1 - p_match_left) + right_rows * max(0, 1 - p_match_right)`
- This is sufficient for a first pass if exact unmatched-side correlation is unavailable.

### 6.8 Grouping and `DISTINCT` estimates

If multivariate `ndistinct` is available on the exact key set:
- use it directly as `ndv_eff`.

Otherwise construct `ndv_eff` from per-key NDVs with dependency damping:
- start with `ndv_eff = 1`
- for each key `k`, multiply by `min(ndv_k, input_rows)`
- when a dependency `X => Y` with degree `d` is known and both determinant and dependent keys are in the grouping set:
  - replace multiplication by `ndv_Y` with multiplication by `1 + (1 - d) * (ndv_Y - 1)`
- cap `ndv_eff` at `input_rows`

Convert effective NDV to expected output groups using an occupancy-style formula:
- `groups = ndv_eff * (1 - exp(-input_rows / max(ndv_eff, 1)))`
- then clamp to `[1, input_rows]`

Use the same formula for `DISTINCT` over the projected key list.

Inference: this occupancy conversion is a better first-pass replacement for ScratchBird's current `rows / 10` heuristic because it behaves reasonably both when NDV is far below input rows and when NDV is near or above input rows.

### 6.9 Fallback heuristics

When statistics are missing or unusable, keep a small, explicit fallback table:

| Predicate or operator | First-pass fallback | Notes |
| --- | --- | --- |
| equality on base column | `0.01` | Existing ScratchBird default is acceptable bootstrapping |
| generic range predicate | `0.33` | Existing default; low confidence |
| `BETWEEN` | `0.10` to `0.33` | Prefer histogram decomposition; use `0.10` only if preserving current behavior |
| prefix `LIKE` without usable text stats | `0.10` | Low confidence |
| suffix `LIKE` | `0.05` | Low confidence |
| contains `LIKE` | `0.01` | Low confidence |
| equality join with no stats | `0.01` | Replace with NDV-based formula as soon as either side has stats |
| non-equality join with no usable overlap stats | `0.33` | Low confidence |
| semijoin match probability with no key stats | `0.50` | Low confidence, then cap to `[0,1]` |
| antijoin floor | `0.10` | Safety floor, not point belief |
| grouping or `DISTINCT` with no NDV evidence | `input_rows / 10` | Only as bootstrapping fallback |

### 6.10 Confidence handling

The estimator should emit a confidence score and class for every estimate.

Recommended confidence inputs:
- freshness factor:
  - fresh: `1.00`
  - warm: `0.80`
  - stale: `0.50`
  - expired: `0.25`
  - unknown: `0.20`
- coverage factor:
  - exact key or exact MCV match: `1.00`
  - histogram plus MCV: `0.85`
  - histogram only: `0.70`
  - NDV only: `0.55`
  - heuristic only: `0.25`
- sample factor:
  - derive from sample size and sample rate, saturating at the current 30k-row target
- shape factor:
  - single supported clause: high
  - correlated bundle with multivariate stats: medium-high
  - correlated bundle without multivariate stats: low
  - heuristic join or unsupported expression: low

Inference: a practical first-pass score is a weighted average, for example:
- `confidence = 0.35 * freshness + 0.30 * coverage + 0.20 * sample + 0.15 * shape`

Map the score to classes:
- high: `>= 0.75`
- medium: `>= 0.50 and < 0.75`
- low: `< 0.50`

Recommended error bands:
- high confidence: `[est / 2, est * 2]`
- medium confidence: `[est / 4, est * 4]`
- low confidence: `[est / 10, est * 10]`

### 6.11 q-error-oriented adaptive repair hooks

Use q-error as the primary post-execution miss metric:
- `q_error(est, act) = max(max(est, 1) / max(act, 1), max(act, 1) / max(est, 1))`

Recommended repair triggers:
- q-error above the existing default threshold of `4.0`;
- repeated misses for the same clause bundle or join edge under the same stats snapshot;
- large underestimates should be considered more severe than equally large overestimates.

Recommended repair actions:
1. If stats are stale, expired, or very low sample:
   - request analyze refresh.
2. If stats are fresh but the same pattern misses repeatedly:
   - store a bounded correction factor keyed by:
     - relation or join-edge signature;
     - normalized clause bundle;
     - stats snapshot ID.
3. If misses cluster on the same correlated column set:
   - recommend or auto-schedule multivariate statistics collection.
4. Limit repair actions with the existing profiler caps to avoid unstable replanning loops.

Inference: ScratchBird should optimize for p90/p95/p99 q-error and "catastrophic underestimation count", not only average q-error, because optimizer damage is concentrated in the tail.

## 7. ScratchBird Contract Draft

The estimator contract should become explicit instead of being implied by scattered helper calls.

### Proposed estimator inputs

Base inputs:
- logical input row count for the relation or subplan being estimated;
- normalized predicate bundle or join edge;
- join type where applicable;
- grouping or `DISTINCT` key set where applicable.

Statistics inputs:
- single-column stats:
  - null fraction, NDV, MCV, histogram, sample metadata;
- expression stats:
  - first pass can continue to support `LOWER` and `UPPER`;
- multivariate stats:
  - MCV bundles;
  - dependency bundles;
  - multivariate `ndistinct`;
- correlation stats as last-resort fallback;
- future key metadata:
  - uniqueness;
  - primary-key or foreign-key hints;
  - null-rejecting flags.

### Proposed estimator outputs

Each estimate result should include:
- `estimated_rows` as `double`;
- `selectivity` as `double`;
- `confidence_score`;
- `confidence_class`;
- `error_band_low`;
- `error_band_high`;
- `basis_kind` enum;
- `stats_snapshot_ids` used;
- `staleness_summary`;
- `repair_key`.

Suggested basis enum:
- `EXACT_KEY`
- `MCV_SINGLE`
- `MCV_MULTI`
- `HISTOGRAM`
- `HISTOGRAM_PLUS_MCV`
- `DEPENDENCY`
- `NDISTINCT`
- `CORRELATION_FALLBACK`
- `HEURISTIC`
- `FEEDBACK_CORRECTED`

### Proposed API shape

Recommended logical API surface:
- `EstimateBaseRelation(bundle, stats_ctx) -> EstimateResult`
- `EstimateJoin(join_edge, left_est, right_est, stats_ctx) -> EstimateResult`
- `EstimateGroups(key_set, input_est, stats_ctx) -> EstimateResult`
- `EstimateDistinct(key_set, input_est, stats_ctx) -> EstimateResult`

### Fit with current ScratchBird infrastructure

This contract fits the current system well because ScratchBird already has:
- stats freshness and confidence metadata;
- plan payload provenance;
- query-profiler feedback signals with correction factors and replan gating.

Inference: the right first implementation is not to invent a separate feedback subsystem. It is to make `EstimateResult` first-class and let existing profiler and payload structures consume and persist that result.

### Required data-model additions

ScratchBird will need additional cataloged statistics objects for:
- multivariate MCV;
- multivariate `ndistinct`;
- soft dependencies;
- eventually, key and foreign-key metadata surfaced directly to the estimator.

The first-pass contract should also require every estimate to declare whether it is:
- exact on known values;
- approximate from statistics;
- or heuristic.

## 8. Validation and Benchmark Packet

Validation should mix synthetic fault-finding cases with established benchmark workloads.

### Synthetic validation corpus

Build focused microbench families for:
- single-column heavy skew:
  - hot-key MCV dominance;
  - long residual tail;
- range boundaries:
  - predicates near histogram edges;
  - highly skewed tails;
- correlated predicates:
  - perfect correlation (`a = b`);
  - soft functional dependency (`zip -> city`);
  - impossible combinations (`state = 'CA' AND zip starts with 10`);
- equality joins:
  - symmetric hot keys on both sides;
  - one-sided hot keys;
  - unique-key joins;
- non-equality joins:
  - overlapping and disjoint ranges;
  - `<`, `<=`, `>`, `>=`, `<>`;
- semi and anti joins:
  - sparse RHS matches;
  - dense RHS matches;
  - highly duplicated RHS keys;
- outer joins:
  - many unmatched outer rows;
  - many duplicated matches;
- grouping and `DISTINCT`:
  - independent keys;
  - dependent keys;
  - near-unique keys;
- stale-stat scenarios:
  - shifted distributions after analyze;
  - sample bias.

### Established benchmark corpus

Recommended external workloads:
- Join Order Benchmark for multi-join cardinality sensitivity;
- Cardinality Estimation Benchmark style workloads for q-error and robustness analysis;
- TPC-H subsets for practical join and aggregate planning;
- Inference: add a TPC-DS subset later if Lane D needs more complex correlation and grouping pressure.

### Evaluation metrics

Primary metrics:
- q-error percentiles: p50, p90, p95, p99;
- underestimation tail count:
  - number of estimates worse than `10x` low;
  - number worse than `100x` low;
- plan regret:
  - chosen plan runtime versus best observed plan runtime;
- join-order regret on multi-join workloads;
- group and `DISTINCT` row-count error percentiles;
- planning CPU overhead for richer estimation.

Operational metrics:
- number of feedback-triggered refresh requests;
- fraction of repeated misses repaired by refresh versus correction factor;
- plan instability introduced by adaptive correction;
- contradiction-detection correctness, especially zero-row pruning.

### Validation protocol

Recommended protocol:
1. Run baseline ScratchBird estimator.
2. Run first-pass Lane D formulas without adaptive correction.
3. Run the same with bounded q-error correction enabled.
4. Compare:
   - estimate accuracy;
   - chosen plan shape;
   - runtime;
   - planning overhead.

All validation runs should record:
- normalized query signature;
- estimated rows per operator;
- actual rows per operator;
- confidence class;
- stats snapshot IDs;
- whether feedback or refresh logic fired.

## 9. Adopt/Adapt/Reject/Defer Matrix

| Topic | Recommendation | Rationale | First-pass note |
| --- | --- | --- | --- |
| Single-column equality via MCV plus residual tail | Adopt | ScratchBird already partly does this; PostgreSQL confirms the formula | Make it the explicit canonical formula |
| Range estimation as exact MCV contribution plus histogram tail interpolation | Adopt | Strong PostgreSQL donor pattern | Requires separating or deriving non-MCV histogram mass |
| Soft functional-dependency formula for correlated equalities | Adapt | PostgreSQL provides the clearest practical formula | Needs new multivariate stats object and clause matching |
| Multivariate MCV clause-bundle correction | Adapt | Best fix for impossible combos and hot combinations | Start with equality, range, and null clauses |
| Multivariate `ndistinct` for `GROUP BY` and `DISTINCT` | Adapt | Strong PostgreSQL donor and directly fixes current `rows / 10` heuristic | High-value early feature |
| Explicit inner/outer/semi/anti row formulas | Adapt | MySQL makes join-type semantics concrete | Needed immediately because ScratchBird currently has no dedicated handling |
| Conservative anti-join floor | Adapt | MySQL's safety bias is operationally sound | Start with `0.10` floor |
| Prefer mild overestimation over catastrophic underestimation in ambiguous join cases | Adapt | Supported by MySQL rationale and benchmark experience | Apply selectively, not globally |
| HLL-backed distinct sketches and relation-set equivalence tracking | Adapt | DuckDB pattern is lightweight and useful for join-order search | Good follow-on after canonical formula cleanup |
| Stats propagation for contradictions and nullability | Adapt | DuckDB shows practical planner leverage | Good second-phase optimization once core CE is stable |
| Histogram-only equality as `non_null_fraction / ndv` primary formula | Reject | Too weak for skew and hot-key handling | Keep only as last fallback |
| Pairwise Pearson-correlation nudge as the main correlated-predicate solution | Reject | Too weak and too narrow | Retain only as last-resort fallback |
| Pure denominator-based heuristic join estimator as the production final answer | Reject | Useful for search ordering, not for user-visible row estimates | Do not present as primary CE contract |
| Fully learned cardinality estimation in first implementation | Defer | Too much operational complexity before deterministic basics are in place | Revisit only after stats-based CE, confidence, and validation are solid |
| Broad runtime re-optimization loops | Defer | Higher instability risk | Keep bounded feedback and refresh requests only |
| Rich copula or graphical-model CE | Defer | High complexity, uncertain payoff for first pass | Not needed for initial spec |

## 10. Open Questions and Integration Dependencies

Open questions:
- How should ScratchBird catalog multivariate stats objects:
  - separate object types;
  - or a single object with kind flags similar to PostgreSQL?
- Should histograms be stored as full-population buckets or explicitly as tail-population buckets once MCVs are present?
- How should expression statistics generalize beyond `LOWER` and `UPPER` without exploding analyze cost?
- How should key, uniqueness, and foreign-key metadata be exposed to the estimator?
- How should `NOT IN` null semantics be represented in selectivity estimation?
- How should full outer join unmatched-side estimation be validated in the absence of key metadata?

Integration dependencies:
- multivariate statistics collection and storage;
- planner-side normalized clause-bundle representation;
- estimator result object with confidence and basis fields;
- executor or profiler hooks that capture actual rows at the right logical boundaries;
- stable normalization keys for feedback correction and refresh requests;
- benchmark harnesses that preserve estimate-versus-actual traces.

Current implementation risks:
- current range logic may double count or misallocate hot-key mass if MCV and histogram usage are not separated;
- current group and `DISTINCT` heuristics are too weak to support serious aggregate plan quality claims;
- current join estimation is not good enough for semi, anti, or outer joins.

Inference: the minimum dependency set for Lane D usefulness is:
1. canonical single-column formulas;
2. join-type row formulas;
3. NDV-based group and `DISTINCT`;
4. confidence and repair keys.

Multivariate stats are the highest-value next layer after that minimum.

## 11. Recommended Next-Step Specification Tasks

1. Specify the `EstimateResult` contract, including `basis_kind`, confidence, error band, and repair key.
2. Replace the current group and `DISTINCT` `rows / 10` heuristics with NDV-based estimation, even before multivariate stats arrive.
3. Tighten single-column range estimation so MCV mass and histogram tail mass are combined explicitly rather than implicitly.
4. Add null-aware equality-join formulas and explicit row-output formulas for inner, outer, semi, and anti joins.
5. Define a multivariate statistics catalog and analyze pipeline for:
   - multivariate MCV;
   - dependencies;
   - multivariate `ndistinct`.
6. Add a normalized clause-bundle matcher so multivariate stats can be applied before residual independence logic.
7. Emit estimator confidence and basis into plan payloads and expose them in plan diagnostics.
8. Reuse the existing query-profiler feedback path for q-error-triggered refresh requests and bounded correction factors.
9. Build the Lane D validation corpus and reporting template around q-error percentiles, underestimation tails, and plan regret.
10. After the deterministic packet is stable, evaluate whether HLL-backed relation-set distinct tracking should be added for join-order search and whether a learned CE experiment is still justified.
