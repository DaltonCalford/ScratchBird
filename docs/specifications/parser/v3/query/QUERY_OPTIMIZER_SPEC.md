# Query Optimizer Specification (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the V3 query optimizer behavior, cost model, and determinism rules.

## Scope

In scope:
- Join order selection
- Join method selection
- Index vs table scan selection
- Parallel plan selection (intra-node only)

Out of scope:
- Adaptive runtime re-optimization
- Distributed query optimization

## Statistics Inputs

The optimizer MUST use:
- Table stats: row count, page count
- Index stats: key cardinality, depth
- Column stats: null fraction, distinct count

If statistics are missing, the optimizer MUST use defaults:
- selectivity equality: 0.1
- selectivity range: 0.33
- selectivity LIKE: 0.2

## Cost Model (Deterministic)

Total cost:
```
cost = io_cost + cpu_cost
io_cost = pages_read * cost_page_read
cpu_cost = rows_processed * cost_cpu_row
```

Constants are read from config:
- `cost_page_read`
- `cost_cpu_row`

## Plan Selection Rules

### Scan Selection

- Use index scan if estimated cost < table scan cost.
- If equal, prefer index scan.

### Join Order

- Use greedy join ordering by lowest estimated result cardinality.
- Ties are broken by lexicographic order of table path.

### Join Method

- If equi-join and hash table fits memory -> hash join.
- Else if both inputs are ordered -> merge join.
- Else -> nested loop.

### Parallel Plans

- Enable parallelism if estimated rows > `parallel_threshold_rows`.
- DOP = min(`parallel_max_dop`, `parallel_default_dop`).

## Determinism Rules

- All ties MUST be broken deterministically using lexicographic ordering.
- The same query with the same stats MUST produce identical plans.

## Related Specs

- `docs/specifications/parser/v3/query/PARALLEL_EXECUTION_ARCHITECTURE.md`
- `docs/specifications/parser/v3/types/VALUE_SPEC_STORAGE_ENCODINGS.md`
