# Competitive Performance Parity Workpack

Date: 2026-04-05

## Goal

Drive ScratchBird to donor-competitive performance on the benchmarked write,
join, sort, aggregate, and window paths.

The package target is:

- ScratchBird shall finish within `3%` of the fastest donor in the same
  transactional guarantee class for every benchmark process in the active
  comparable matrix.

Transactional guarantee classes:

- `normal_transactional`: compare against FirebirdSQL normal, MySQL normal, and
  PostgreSQL normal
- `autocommit`: compare against FirebirdSQL autocommit, MySQL autocommit, and
  PostgreSQL autocommit
- donor-only `no_transaction` runs remain mandatory design input and stretch
  references until ScratchBird admits an equivalent reduced-guarantee execution
  class

## Donor assimilation rule

If a donor engine uses a legal technique that improves a benchmark process,
ScratchBird must do one of the following:

1. implement an equivalent native optimization
2. implement a stronger ScratchBird-native optimization that dominates it
3. record an explicit waiver proving that the technique conflicts with
   non-negotiable MGA, UUID, or parser-boundary invariants

Leaving a proven donor fast path unassimilated while remaining benchmark-slower
is not acceptable.

## Mandatory donor-technique assimilation matrix

The audit findings are binding implementation input for this workpack. The
minimum donor techniques that must be matched or dominated are:

### MySQL-derived write-path techniques

- statement-level bulk insert setup
- reusable row-conversion and insert-row infrastructure
- optimistic exact-page insert before pessimistic fallback
- eligible deferred exact-secondary merge equivalent to change buffering

### PostgreSQL-derived write-path techniques

- heap multi-insert with one page pin or lock cycle for many tuples
- HOT-like non-indexed update behavior when indexed state is unchanged
- set-sourced insert admission for `INSERT ... SELECT` and comparable row
  production

### FirebirdSQL-derived write and MGA discipline

- cheap native MGA store or modify path without batch-shape collapse
- stream-oriented executor and DML path that do not require donor client
  batching to stay competitive

### Donor-derived runtime operator techniques

- index condition pushdown for admitted secondary paths
- rowid-ordered heap fetch equivalent to `MRR`
- batched key access for indexed join probes
- bounded index-only execution with explicit visibility proof
- memoized parameterized rescans
- incremental sort on delivered-prefix input
- runtime filters and dynamic pruning
- bounded adaptive build-side selection for legal hash joins
- vectorized batch execution for scan, join, sort, aggregate, distinct, and
  window
- structured-key hash join without stringified join-key fallback on admitted
  types
- low-churn sort pipeline with bounded workfile spill
- aggregate and distinct state that avoid unnecessary materialization
- partition-aware window execution with explicit buffering only where required

If ScratchBird remains slower than a donor on a benchmark process, the matching
donor technique above is presumed mandatory until a stronger native technique
proves it unnecessary.

## Required cross-section closures

### Unified orchestration closure

- section `23`
  - `QUERY_PERFORMANCE_ORCHESTRATION_AND_CROSS_LAYER_COORDINATION_MODEL.md`
    is the controlling cross-layer authority for how sections `18`, `23`,
    `33`, `36`, `12`, `03`, `02`, `34`, and `39` combine for each query and
    mutation class
  - parity-package closure is not allowed while any benchmark-governed query
    type still depends on implicit coordination or implementer guesswork across
    those sections

### Row-store write closures

- section `34`
  - heap multi-insert
  - page-coalesced insert reservation
  - heap-only or stable-head-preserving non-indexed update

### Index-maintenance closures

- section `18`
  - statement-local metadata hoist
  - unchanged-key non-indexed update elision
  - commit-group exact maintenance on ordinary multi-row and set-sourced insert
  - `ICP`, `MRR`, `BKA`, and bounded index-only closure for ordered exact
    families

### Ingest-lane closures

- section `39`
  - lane classification across `COPY`, client-batched multi-row insert, and
    `INSERT ... SELECT`
  - sorted exact bulk for all admitted large additive row sources

### Storage growth and preallocation closures

- sections `02` and `03`
  - ahead-of-demand filespace growth reservation for admitted bulk and
    append-heavy write paths
  - page-run or extent-window reservation so hot loops do not pay repeated OS
    growth calls
  - per-row or per-page file-growth syscalls in benchmark hot paths are
    non-conforming when future demand is already known

### Memory and spill closures

- sections `33` and `12`
  - operator grant and spill feedback must continue closing the remaining
    sort, join, window, and aggregate gaps under pressure
  - repeated avoidable spill on a benchmark-governed operator is non-conforming
  - worker-aware grant binding must charge leader and worker memory explicitly
  - planner-only spill metadata is insufficient for package closure

### Parallel execution and locality closures

- sections `36`, `23`, and `03`
  - intra-query parallel candidates must be enumerated and executable for
    admitted scan, join, sort, aggregate, and window shapes
  - serial slow fallback may not remain the dominant path when donor engines
    win through legal multi-worker execution
  - worker affinity and memory locality must be explicit on hosts where NUMA
    or partition-local memory materially affects speed

### Planner and runtime operator closures

- sections `36` and `23`
  - complete specialized join, sort, aggregate, and window paths
  - `MEMOIZE_WRAP`, `INCREMENTAL_SORT`, runtime filters, and bounded adaptive
    hash-join build-side selection
  - vectorized batch execution for benchmark-governed upper stages
  - no generic slow fallback may remain the dominant path for a benchmark
    process when a donor uses a specialized operator

## Benchmark-process to closure mapping

- `load`
  - section `34` heap multi-insert
  - section `18` statement-local metadata hoist
  - section `39` row-source lane admission
  - sections `02` and `03` preallocated growth window
- `bulk_insert_select`
  - section `39` `SET_SOURCED_INSERT` admission into `SORTED_EXACT_BULK` or
    `RETAIL_MICRO_BATCH`
  - section `34` page-coalesced heap writes
  - section `18` batched exact-family maintenance
  - sections `02` and `03` ahead-of-demand file growth reservation
- `bulk_update_with_case`
  - section `34` heap-only or stable-head-preserving non-indexed update
  - section `18` unchanged-key exact-family elision
- join-heavy processes
  - sections `36` and `23` specialized join paths
  - section `18` `BKA`, `MRR`, and `ICP` where secondary access is involved
  - section `33` reservation and spill fit
- aggregate and distinct processes
  - sections `36`, `23`, and `33` specialized aggregate state and spill
    discipline
  - section `23` vectorized upper-stage execution
- `window_function_ranking`
  - sections `23`, `12`, and `33` partition-aware window pipeline and bounded
    spill behavior
  - sections `23` and `36` incremental sort and vectorized window execution
- any process whose best donor result depends on multi-worker execution
  - sections `36`, `23`, `33`, and `03` explicit parallel admission, worker
    budgeting, and locality binding

## Process-level acceptance gates

The benchmark parity gate applies to:

- data load
- `bulk_insert_select`
- `bulk_update_with_case`
- `bulk_update_with_join`
- `inner_join_simple`
- `inner_join_multiple_conditions`
- `inner_join_large_result`
- `left_join_all_customers`
- `four_table_join`
- `self_join_same_country`
- `aggregation_daily_sales`
- `agg_full_table_scan`
- `agg_distinct_counts`
- `multi_dimensional_agg`
- `nested_subquery_agg`
- `window_function_ranking`

## Immediate priority order

1. recover write-path regressions
   - unchanged-key non-indexed update elision
   - ordinary multi-row insert and `INSERT ... SELECT` bulk path reuse
2. land row-store batch semantics
   - page-coalesced heap multi-insert
   - layout and metadata hoisting
   - ahead-of-demand filespace growth reservation
3. close donor write-path parity
   - optimistic insert-first behavior where legal
   - delayed exact-secondary merge and batch apply where legal
   - HOT-like non-indexed update behavior
4. close donor executor parity
   - join, sort, aggregate, and window specializations until every benchmark
     process is within the parity band
5. close donor parallel and locality parity
   - admitted intra-query parallel execution
   - worker-aware grant reservation
   - NUMA or partition-local memory binding where it materially affects speed
6. close donor secondary-read and adaptive-planning parity
   - `ICP`, `MRR`, `BKA`, and bounded index-only
   - `MEMOIZE_WRAP`, `INCREMENTAL_SORT`, runtime filters, and bounded adaptive
     build-side selection

## Release-blocking parity rule

Package completion is not achieved by implementing isolated fast paths. The
package remains open until every benchmark process in the active comparable
matrix is within the `3%` parity band or carries an explicit recorded waiver
that proves the remaining delta is caused by a non-negotiable correctness
invariant rather than a missing optimization.

The package also remains open if any of the following remain true:

- a benchmark-governed operator still relies on planner-only spill metadata
  without a bounded runtime workfile path
- a legal parallel candidate is absent, non-executable, or unbudgeted on a
  workload where donor engines win through multi-worker execution
- a multi-worker path ignores memory locality or worker affinity on a host
  where locality materially affects benchmark speed
- a benchmark-governed write path still performs repeated avoidable file-growth
  or preallocation syscalls inside the hot row loop

## Benchmark discipline requirements

Every parity comparison run shall record:

- git SHA
- exact `sb_server` path
- `CMAKE_BUILD_TYPE`
- benchmark launcher path and driver revision
- transaction mode and suite scale

No result may be accepted into the parity matrix if the ScratchBird binary
provenance is incomplete.
