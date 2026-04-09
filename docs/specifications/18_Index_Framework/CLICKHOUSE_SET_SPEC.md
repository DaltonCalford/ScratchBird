Status: canonical_beta2_immediate_implementation

# ClickHouse Set Index Specification

## Purpose

Define the `CLICKHOUSE_SET` family required to emulate ClickHouse
`set(...)` MergeTree secondary indexes.

## Donor Basis

The donor shape is grounded in:

- `MergeTreeIndices.cpp` registration of `set`
- `MergeTreeIndexSet.h`
- `MergeTreeIndexSet.cpp`

Those donor files prove:

- `set` is a real secondary index type
- each granule stores a bounded set of distinct values
- the granule also stores per-column min/max hyperrectangle bounds
- predicates are evaluated against the stored set and hyperrectangle

## Canonical Identity

- admitted named family:
  - `CLICKHOUSE_SET`
- donor engines supported:
  - `ClickHouse`
- physical family:
  - `SET`
- planner family:
  - `SUMMARY`
- metrics type:
  - `SUMMARY_CANDIDATE`
- lifecycle model:
  - mutable summary with per-granule rebuild on merge

## Required DDL Surface

Canonical donor-compatible form:

```sql
CREATE INDEX idx_name ON t (expr) USING CLICKHOUSE_SET(max_rows);
```

Required options:

- `max_rows`:
  - unsigned integer
  - exactly one argument
  - `0` means unbounded donor mode is not allowed in ScratchBird Beta 2;
    use a positive value only

## On-Disk Model

Each summary granule stores:

- `distinct_value_count`
- `overflowed` flag
- sorted deduplicated encoded values for every indexed key expression
- `min_value` and `max_value` hyperrectangle entries for each key column

If the number of distinct values for a granule exceeds `max_rows`:

- the `overflowed` flag is set
- exact set membership probing is disabled for that granule
- the min/max hyperrectangle remains authoritative

## Build Flow

1. Start an MGA-consistent scan of the base relation.
2. Partition rows into index granules according to the owning table or part
   granularity.
3. For each visible row:
   - evaluate the indexed expression list
   - insert the deduplicated key tuple into the granule set
   - update per-column min/max bounds
4. If cardinality exceeds `max_rows`, mark the granule `overflowed`.
5. Persist the set summary pages and publish metrics.

## Probe Semantics

The planner may use `CLICKHOUSE_SET` for:

- equality
- `IN`
- simple conjunctions that can be reduced to set-membership probes
- range pruning through the stored min/max hyperrectangle

The planner shall not use `CLICKHOUSE_SET` for:

- ordered delivery
- k-nearest search
- full residual-free execution

Every qualifying path remains recheck-required against heap truth.

## Metrics Contract

The native metrics payload for `CLICKHOUSE_SET` must include:

- `named_family = "CLICKHOUSE_SET"`
- `granule_count`
- `max_rows_limit`
- `avg_distinct_values_per_granule`
- `overflowed_granule_fraction`
- `set_membership_hit_ratio`
- `hyperrectangle_prune_ratio`
- `summary_false_positive_ratio`
- `resident_summary_bytes`
- `rebuild_merge_debt`

The optimizer must use:

- `overflowed_granule_fraction` as a penalty to exact-membership confidence
- `hyperrectangle_prune_ratio` as the main prune credit
- `summary_false_positive_ratio` as a recheck amplifier

## Required Pseudocode

```cpp
bool may_granule_match(const SetGranule& g, const Predicate& p) {
    if (!hyperrectangle_may_match(g.bounds, p)) {
        return false;
    }
    if (g.overflowed) {
        return true;
    }
    return distinct_set_may_match(g.keys, p);
}
```

## Required Refusal Rules

Create must fail if:

- `max_rows` is absent
- `max_rows` is not an unsigned integer
- the indexed expression is non-deterministic
- the key type cannot be normalized into the summary key encoding

## Optimizer First-Class Rule

`CLICKHOUSE_SET` is a primary candidate family for summary pruning. It may
never be omitted because it is not a B-tree or because it is donor-specific.
