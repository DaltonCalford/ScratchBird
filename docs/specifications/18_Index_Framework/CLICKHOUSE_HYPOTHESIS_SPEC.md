Status: canonical_beta2_immediate_implementation

# ClickHouse Hypothesis Index Specification

## Purpose

Define the `CLICKHOUSE_HYPOTHESIS` family required for donor-compatible
emulation of ClickHouse `hypothesis` indexes.

## Donor Basis

The donor shape is grounded in:

- `MergeTreeIndices.cpp` registration of `hypothesis`
- `MergeTreeIndexHypothesis.cpp`

Those donor files prove:

- the family stores one boolean-like state per granule
- the build path aggregates a single expression
- ordinary `createIndexCondition(...)` is explicitly unsupported in donor code
- the family participates through specialized merged-condition handling

## Canonical Identity

- admitted named family:
  - `CLICKHOUSE_HYPOTHESIS`
- donor engines supported:
  - `ClickHouse`
- physical family:
  - `HYPOTHESIS_SUMMARY`
- planner family:
  - `SUMMARY`
- metrics type:
  - `SUMMARY_CANDIDATE`
- queryability model:
  - primary, but only for predicates that lower to the family-specific
    hypothesis contract

## Required DDL Surface

Canonical donor-compatible form:

```sql
CREATE INDEX idx_name ON t (expr) USING CLICKHOUSE_HYPOTHESIS();
```

Required rules:

- exactly one expression is allowed
- the expression must normalize to a family-supported hypothesis function
- generic unsupported predicates must refuse at plan time rather than silently
  pretending normal index-search support

## Runtime Model

Per donor behavior, the summary state for each granule records whether the
family-specific condition was "met" across the granule.

ScratchBird Beta 2 shall materialize:

- `is_empty`
- `hypothesis_met`
- `expression_hash`
- `supported_predicate_class`

## Planner Rule

`CLICKHOUSE_HYPOTHESIS` is first-class only for query shapes that lower to its
specialized hypothesis predicate class.

For all other query shapes:

- the planner must record the family as semantically incapable
- the family must not be silently dropped from diagnostics
- `EXPLAIN` must show why the family was rejected

## Metrics Contract

The native payload must include:

- `named_family = "CLICKHOUSE_HYPOTHESIS"`
- `supported_predicate_class`
- `met_granule_fraction`
- `deterministic_skip_ratio`
- `predicate_shape_match_ratio`
- `planner_refusal_count`
- `merge_debt`
- `visibility_recheck_fraction`

## Required Pseudocode

```cpp
if (!predicate_matches_hypothesis_contract(query_predicate, index_meta)) {
    return incapable_candidate("predicate shape not supported");
}
return enumerate_summary_candidate(index_meta, metrics_packet);
```

## Refusal Rules

Create must fail if:

- more than one expression is supplied
- the expression class cannot be bound to the family contract

Plan-time use must fail closed if:

- the query predicate does not match the supported hypothesis shape

## First-Class Rule

`CLICKHOUSE_HYPOTHESIS` is not downgraded to a hidden advisor surface. It is a
primary admitted family with a narrow but explicit semantic contract.
