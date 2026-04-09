# Benchmark and Load Shape Inputs

This file translates each benchmark process into the dominant physical
behaviors that the implementation must optimize.

## Data load

- Dominant shape:
  - append-heavy multi-row insert into row store
- Must-win closures:
  - server-side bulk handle reuse
  - page-coalesced heap insertion
  - ahead-of-demand filespace growth
  - exact-secondary maintenance batching or deferral where legal
- Owning tickets:
  - `PP-10-003`
  - `PP-10-005`
  - `PP-10-007`

## `bulk_insert_select`

- Dominant shape:
  - large set producer feeding a large insert sink
- Must-win closures:
  - producer and sink fusion
  - set-sourced insert lane selection
  - sink bulk handle reuse
  - page and extent preallocation
- Owning tickets:
  - `PP-10-004`
  - `PP-10-005`
  - `PP-10-007`

## `bulk_update_with_case`

- Dominant shape:
  - large non-indexed update over existing rows
- Must-win closures:
  - unchanged-key exact-family elision
  - HOT-like or stable-head-preserving update path
  - no repeated index metadata enumeration per updated row
- Owning tickets:
  - `PP-10-006`
  - `PP-10-007`

## `inner_join_simple`

- Dominant shape:
  - point or narrow indexed join
- Must-win closures:
  - `ICP`
  - `BKA`
  - memoize when parameterized rescans occur
- Owning tickets:
  - `PP-10-009`
  - `PP-10-010`

## `inner_join_large_result`

- Dominant shape:
  - large join with high emitted row count
- Must-win closures:
  - structured-key hash join
  - runtime filters
  - spill-safe workfile path
  - legal parallel execution
- Owning tickets:
  - `PP-10-010`
  - `PP-10-011`
  - `PP-10-013`

## `inner_join_multiple_conditions`

- Dominant shape:
  - selective indexed join with additional residual filters
- Must-win closures:
  - `ICP`
  - memoize or `BKA` when legal
- Owning tickets:
  - `PP-10-009`
  - `PP-10-010`

## `left_join_all_customers`

- Dominant shape:
  - outer join plus grouped aggregate
- Must-win closures:
  - join specialization
  - aggregate specialization
  - low-materialization grouped execution
- Owning tickets:
  - `PP-10-010`
  - `PP-10-012`

## `four_table_join`

- Dominant shape:
  - deep join tree with mixed filters and wide row flow
- Must-win closures:
  - indexed join specialization
  - structured-key hash join
  - legal parallelism and locality
- Owning tickets:
  - `PP-10-010`
  - `PP-10-012`
  - `PP-10-013`

## `self_join_same_country`

- Dominant shape:
  - self-join on selective grouping key
- Must-win closures:
  - indexed join locality
  - legal parallel candidate when row volume justifies it
- Owning tickets:
  - `PP-10-010`
  - `PP-10-013`

## `aggregation_daily_sales`

- Dominant shape:
  - join plus grouped aggregate with date bucketing
- Must-win closures:
  - hash aggregate or ordered aggregate with bounded spill
  - vectorized aggregate state
  - legal parallel partial/final execution
- Owning tickets:
  - `PP-10-011`
  - `PP-10-012`
  - `PP-10-013`

## `agg_full_table_scan`

- Dominant shape:
  - large scan plus aggregate
- Must-win closures:
  - vectorized scan and aggregate
  - low-churn aggregate state
  - bounded spill or in-memory admission
- Owning tickets:
  - `PP-10-011`
  - `PP-10-012`

## `agg_distinct_counts`

- Dominant shape:
  - large distinct aggregate
- Must-win closures:
  - distinct state specialization
  - spill-safe hash or ordered distinct
  - vectorized aggregate/distinct
- Owning tickets:
  - `PP-10-011`
  - `PP-10-012`

## `multi_dimensional_agg`

- Dominant shape:
  - wide grouping and aggregate fanout
- Must-win closures:
  - vectorized aggregate pipeline
  - partial/final parallel aggregate
  - worker-aware memory grants
- Owning tickets:
  - `PP-10-011`
  - `PP-10-012`
  - `PP-10-013`

## `nested_subquery_agg`

- Dominant shape:
  - mixed join and nested aggregate pipeline
- Must-win closures:
  - join reuse and memoization when legal
  - aggregate specialization
  - low-materialization subquery pipeline
- Owning tickets:
  - `PP-10-010`
  - `PP-10-012`

## `window_function_ranking`

- Dominant shape:
  - ordered window partition processing
- Must-win closures:
  - incremental sort
  - partition-aware buffering
  - bounded workfile spill
  - vectorized window execution
  - legal partition-preserving parallelism where admissible
- Owning tickets:
  - `PP-10-011`
  - `PP-10-012`
  - `PP-10-013`
