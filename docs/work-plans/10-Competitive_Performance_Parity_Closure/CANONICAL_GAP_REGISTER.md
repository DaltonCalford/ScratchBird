# Canonical Gap Register

## GAP-10-001 Ordinary multi-row insert is still server-side row oriented

- Current impact:
  - `load`
  - `bulk_insert_select`
- Current evidence:
  - current ScratchBird load is `264611.224 ms` normal and `872301.070 ms`
    autocommit
- Required closure:
  - bulk handle reuse on all ordinary multi-row insert surfaces
  - statement-local metadata hoist
  - one row layout and one maintenance posture per batch
- Owning tickets:
  - `PP-10-003`
  - `PP-10-005`
  - `PP-10-007`

## GAP-10-002 `INSERT ... SELECT` sink still bypasses the true bulk lane

- Current impact:
  - `bulk_insert_select`
- Current evidence:
  - current ScratchBird `bulk_insert_select` is `197980.251 ms` normal and
    `455376.877 ms` autocommit
- Required closure:
  - producer and sink fusion
  - set-sourced lane admission
  - sorted or retail micro-batch sink reuse without per-row executor fallthrough
- Owning tickets:
  - `PP-10-004`
  - `PP-10-005`
  - `PP-10-007`

## GAP-10-003 Non-indexed updates still pay unchanged-key index bookkeeping

- Current impact:
  - `bulk_update_with_case`
- Current evidence:
  - current ScratchBird `bulk_update_with_case` is `41136.615 ms` normal and
    `94499.180 ms` autocommit
- Required closure:
  - HOT-like or stable-head-preserving path
  - unchanged-key exact-family elision
  - batch-level metadata reuse for admitted updates
- Owning tickets:
  - `PP-10-006`
  - `PP-10-007`

## GAP-10-004 Prepared-query performance and result-cache coordination are not fully closed

- Current impact:
  - prepared point reads
  - prepared point DML
  - prepared micro-batch insert
- Required closure:
  - prepared fast-path bundles
  - explicit bundle invalidation
  - result-cache coordination for cacheable prepared top-level selects
- Owning tickets:
  - `PP-10-008`

## GAP-10-005 Ordered exact secondary paths still lack full donor locality closure

- Current impact:
  - `inner_join_simple`
  - `inner_join_multiple_conditions`
  - `inner_join_large_result`
  - `self_join_same_country`
- Required closure:
  - `ICP`
  - `MRR`
  - bounded index-only execution
- Owning tickets:
  - `PP-10-009`

## GAP-10-006 Indexed joins still lack full donor probe specialization

- Current impact:
  - `inner_join_simple`
  - `inner_join_large_result`
  - `left_join_all_customers`
  - `four_table_join`
  - `nested_subquery_agg`
- Required closure:
  - `BKA`
  - memoize
  - runtime filters
  - bounded adaptive hash build-side selection
- Owning tickets:
  - `PP-10-010`

## GAP-10-007 Upper-stage operators still rely on generic or spill-heavy paths

- Current impact:
  - `aggregation_daily_sales`
  - `agg_full_table_scan`
  - `agg_distinct_counts`
  - `multi_dimensional_agg`
  - `window_function_ranking`
- Required closure:
  - structured-key hash join and spill closure
  - incremental sort and low-churn sort runs
  - aggregate, distinct, and window specialization
  - vectorized upper-stage execution
- Owning tickets:
  - `PP-10-011`
  - `PP-10-012`

## GAP-10-008 Parallel admission and locality are not yet dominant on donor-winning shapes

- Current impact:
  - large joins
  - large aggregates
  - large sort and window paths
- Required closure:
  - legal serial and parallel candidate enumeration
  - worker-aware grant charging
  - exchange closure
  - locality binding and bounded work stealing
- Owning tickets:
  - `PP-10-013`

## GAP-10-009 Benchmark provenance is not yet fully pinned

- Current impact:
  - donor comparisons
  - regression attribution
  - final parity evidence
- Required closure:
  - run-specific binary pinning
  - preserved config snapshot
  - preserved exact benchmark command line and artifact root
- Owning tickets:
  - `PP-10-001`
  - `PP-10-002`
