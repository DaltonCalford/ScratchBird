# Workload Index Advisor and What-If Replanning Model

## Status

Current code-backed authority with reconstructed commercial-grade detail.

## Purpose

This document defines the workload-driven index advisor, its usage counters, its recommendation classes, and the what-if replanning evidence path used to rank index recommendations.

## Scope

This section governs:

- recording table, column, query-pattern, and index-usage signals
- advisor-side table profiles built from catalog and statistics-manager data
- recommendation classes for new, unused, and rebuild-worthy indexes
- hypothetical lowering and cost comparison against current access paths
- deterministic recommendation ordering

This section does not authorize:

- autonomous index creation
- hidden physical index synthesis
- optimizer-side assumption that advisor recommendations already exist

## Advisor Signal Inventory

The advisor records four signal classes:

- query pattern records
- per-table usage stats
- per-column usage stats
- per-index usage and maintenance stats

Per-column usage must distinguish at least:

- equality predicates
- range predicates
- `LIKE`
- `IN` lists
- join participation
- `ORDER BY`
- `GROUP BY`
- select-list coverage demand

These counts are advisory workload evidence. They are not committed optimizer statistics and must not replace the statistics manager as the optimizer's authoritative metrics source.

## Table Usage Model

Each tracked table profile must retain:

- total query count
- sequential scan count
- index scan count
- rows fetched estimate
- last query timestamp
- per-column usage counters

During analysis, the advisor must build an `AdvisorTableProfile` that merges:

- table usage signals
- committed catalog table metadata
- committed table statistics
- committed column statistics
- multivariate statistics when present
- current index inventory for the table

If table statistics are missing, the advisor must fall back to bounded heuristics rather than failing open. Current fallback values are:

- row count fallback around `1000`
- row width fallback around `64` bytes
- derived page count from bounded rows-per-page estimation

## Recommendation Classes

Current recommendation types are:

- create B-tree
- create hash
- create LSM
- create composite
- create partial
- drop unused
- rebuild fragmented index

The current code path maps practical hypothetical creation primarily through:

- B-tree
- hash
- LSM

Composite and partial recommendation classes may appear in the type system, but they must not be overclaimed beyond the current evidence path actually produced by analysis.

## Covering and Ordering Semantics

The advisor must reason separately about:

- predicate support
- ordering support
- covering support

An existing or hypothetical index is covering only when every workload-referenced column is either:

- a key column
- an include column

Ordering demand is derived from observed `ORDER BY` counts on the table profile. Ordered output matters only when the lowered family can actually preserve ordered output for the relevant predicate shape.

## Baseline Plan Selection

The baseline comparison path must consider:

- sequential scan baseline
- existing index baselines

The sequential baseline must include:

- table scan cost
- qualification cost
- explicit sort cost when the workload profile requires ordering and the baseline does not provide it

Existing index baselines must be built only when the index can match the observed leading-column workload shape. The advisor must not fabricate hypothetical prefix usability that the lowered family does not support.

## Existing Index What-If Path

For an existing index baseline, the advisor must:

1. identify matched leading columns from the workload profile
2. derive the strongest predicate shape from current workload counters
3. build the planner-family lowering request
4. lower through the current `PlannerFamilyLoweringResult`
5. reject candidates lowered as `INVALID`
6. load current index-family metrics when available
7. estimate rows from column and multivariate statistics
8. estimate index pages touched, heap pages touched, and correlation effect
9. choose the proper cost-model method for the lowered family

Family-specific costing must remain aligned with the lowered planner family. Current examples include:

- summary and BRIN-like families using summary-scan costing
- bitmap families using bitmap-storage costing
- LSM families using LSM costing
- ordinary exact or ordered families using index-scan or index-only costing

## Hypothetical Recommendation Path

For a hypothetical recommendation, the advisor must:

1. map recommendation type into the current index type family
2. map the leading workload column into a predicate shape
3. lower the hypothetical family request through the same planner-family lowering path
4. reject hypothetical candidates whose lowered family is `INVALID`
5. estimate selectivity from column and multivariate statistics
6. estimate result rows, index size, index pages, tree height, and heap fetch cost
7. account for covering behavior and ordering support
8. run the resulting hypothetical cost through the current cost model

Hypothetical paths must therefore use the same lowering and costing vocabulary as current real plans. The advisor may not compare apples to oranges by using a separate fake model.

## What-If Evidence Record

Each recommendation may include a what-if evidence record with:

- whether replanning actually occurred
- baseline access family
- baseline index name
- baseline total cost
- baseline estimated rows
- hypothetical access family
- hypothetical index name
- hypothetical total cost
- hypothetical estimated rows
- estimated cost delta
- estimated speedup ratio
- ordering improvement flag
- covering improvement flag
- human-readable evidence detail

The evidence string is explanatory only. The structured fields are authoritative.

## Deterministic Ranking Rules

Recommendations must be sorted deterministically.

Current ordering must prefer:

1. higher priority
2. higher what-if estimated speedup ratio
3. higher what-if estimated cost delta
4. lexicographic or otherwise stable fallback ordering when prior keys tie

The advisor must not return unstable ordering across identical evidence sets.

## Unused Index Detection

Unused index analysis must consider:

- scan count
- maintenance cost
- protected roles such as primary-key, unique, and foreign-key obligations
- age since last use

An index that is structurally required for correctness or catalog contract may not be suggested for drop just because scan counts are low.

## Relationship to Optimizer Parity

The advisor does not replace the optimizer's no-ignored-index rule.

Instead:

- the optimizer must consider every admitted family through the planner-family lowering path
- the advisor must use the same family vocabulary when recommending changes
- metrics gaps in a family are an implementation defect lane, not a license to ignore that family

## MGA Rule

Advisor recommendations must remain subordinate to MGA semantics.

No what-if recommendation may claim correctness that depends on:

- WAL-style truth
- stale visibility assumptions
- index-only acceptance without MGA visibility proof or required recheck

## Current Improvement Capture Boundary

The following remain explicit improvement lanes, not current authority:

- richer advisor support for OR bundles
- broader composite recommendation synthesis
- partial-index recommendation with full predicate proof
- deeper family-native metrics for every shipped family

These are valid future promotions, but they are not part of the current guaranteed advisor contract.
