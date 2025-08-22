# Phase 6 — Optimizer and Statistics ✅ **COMPLETED**

## Status: **FULLY IMPLEMENTED**

#### Scope and goals

- Introduce a cost-based optimizer that turns parsed SELECT queries into efficient physical plans.
- Implement cardinality and selectivity estimation using basic stats (row counts, ndistinct, histograms, MCVs, correlation).
- Add join ordering with DP for small N and greedy heuristics for larger N.
- Integrate index selection and scan costing; produce stable EXPLAIN (and EXPLAIN ANALYZE alignment).
- Provide a plan cache for prepared statements, with invalidation hooks on catalog/stat changes.

#### Non-goals

- Parallel plan generation or execution; advanced techniques like adaptive re-optimization or feedback tuning are deferred.
- Complex window-function optimization or vectorized execution (covered in later phases).

---

### Architecture overview

- Optimizer entry: `optimize_select(const SelectQuery&) -> PhysicalPlan`.
- Physical operators: SeqScan, IndexScan, IndexOnlyScan, NestedLoopJoin, HashJoin (skeleton), Sort, HashAgg, Project, Filter, Limit.
- Cost model: I/O + CPU components; per-operator cost functions; row count propagation.
- Stats API: `CatalogManager::get_stats(oid)` + built-in estimators. Fallback to heuristics.
- Plan cache: keyed by normalized query + search_path + relevant GUCs; stores physical plan and parameter bindings.

### Statistics and estimation

- Table stats: row_count, pages_est.
- Column stats: ndistinct, correlation, histograms (equi-depth), MCV list.
- Index stats: height, leaf_pages, branch_pages, key_count, correlation.
- Selectivity estimation:
  - Equality: 1/max(1, ndistinct) (adjust with MCV if hit).
  - Range: width/hist span; correlated with correlation.
  - LIKE/ILIKE: default 0.2 unless prefix known; adjust with correlation.
  - IN list: min(1.0, k/ndistinct) bounded by MCV mass.
  - Combined predicates: independence assumption with limited correction for same-column correlations.

### Operator costing

- SeqScan: cost ~ pages + CPU per row; rows_out = rows * selectivity.
- IndexScan: cost ~ logN + pages_touched (rows_out * correlation factor); add CPU filter.
- IndexOnly: like IndexScan without heap fetch; ensure coverage by projection.
- Sort: cost ~ N log N; spill adds I/O for runs and merge.
- HashAgg: cost ~ build + probe; spill adds partition IO.
- Joins:
  - NestedLoop: outer_rows * inner_cost_per_probe; consider index on inner join key.
  - HashJoin (skeleton): build smaller side; cost ~ build + probe; spill partitions on memory pressure.

### Join ordering

- DP (Selinger-style) for up to 6-8 relations: enumerate joins, maintain best plan per subset.
- Greedy for larger N: pick next join with min incremental cost.
- Ensure bushy plans are considered where beneficial.

### Index selection

- Sargable predicate detection: equality/range on leading index keys.
- Choose among candidate indexes using selectivity and cost; prefer covering (index-only) when possible.
- Honor explicit PLAN hints (Firebird-style) for testing.

### Plan generation flow

1) Normalize query (pull WHERE into conjuncts; extract sargable quals; fold constants).
2) Acquire stats for relations/columns/indexes.
3) For each base relation: create access paths (Seq/Index/IndexOnly) with estimated rows/cost.
4) Join search: DP/greedy assemble join trees; apply join conditions; push filters.
5) Add required top ops: Group/Agg, Window (ordering), Sort (for ORDER BY), Project, Limit.
6) Produce final PhysicalPlan with operator tree and estimates; attach to EXPLAIN.

### EXPLAIN and instrumentation

- EXPLAIN prints chosen plan with cost, rows, width; show key details (index used, filter, join type).
- EXPLAIN ANALYZE compares actuals with estimates; warn on large skew; print per-node timing using existing counters.

### Plan cache and prepared statements

- Cache key: normalized SQL + relevant session settings (work_mem, search_path, enable_* knobs).
- Entry: physical plan + prepared expression IR; parameter locations.
- Invalidate on:
  - DDL affecting referenced relations/indexes.
  - ANALYZE stats refresh for referenced objects.
  - GUC changes that affect cost decisions.

### Config knobs

- enable_hashjoin, enable_mergejoin, enable_nestloop.
- join_collapse_limit (DP cutoff), from_collapse_limit.
- cpu_tuple_cost, cpu_operator_cost, random_page_cost, seq_page_cost.

### Testing strategy

- Unit tests: selectivity formulas, cost functions, join DP enumerations, index selection.
- Integration: multi-join queries; index-vs-seq choices; ORDER BY with/without index; GROUP BY paths.
- EXPLAIN/ANALYZE golden outputs for canonical queries; ensure stability with tolerances.
- TPC-H subset: verify chosen plans make sense; compare vs observed times.

### Milestones

- M1 Stats plumbing and heuristics (1 week)
- M2 Access path generation and costing (1 week)
- M3 Join ordering (DP + greedy) (1–1.5 weeks)
- M4 Top ops integration (Sort/Agg/Limit) and finalization (1 week)
- M5 Plan cache and prepared statements (1 week)
- M6 Hardening and tests (1 week)

### Risks and mitigations

- Skewed data: conservative selectivity caps; MCV/histograms used when present.
- Cost model mismatch: align constants via EXPLAIN ANALYZE; allow tuning knobs.
- Plan cache invalidation complexity: keep conservative; tie into DDL/stats hooks implemented in Phase 4.
