# Workplan Generation Input

## Target definition

The controlling target for this package is:

```text
target_ceiling_ms = best_donor_ms * 1.03
```

Where:

- `best_donor_ms` is the fastest observed result among MySQL, PostgreSQL, and
  FirebirdSQL in the same transactional guarantee class
- `1.03` is the maximum allowed slowdown factor

Anything faster than the donor best is acceptable.

## Benchmark artifacts used

### ScratchBird baseline before regression

- `ScratchBird-Benchmarks/results/clean-rebuild-scratchbird-stress-20260404T023343Z`

### ScratchBird current regressed baseline

- `ScratchBird-Benchmarks/results/current-scratchbird-stress-20260405T035510Z`

### Donor comparison matrix

- `ScratchBird-Benchmarks/results/txmode-matrix-20260403T152011Z`

## Current headline regression facts

- `normal_transactional`
  - load: `34.99s -> 264.61s`
  - `bulk_insert_select`: `15.29s -> 197.98s`
  - `bulk_update_with_case`: `1.31s -> 41.14s`
- `autocommit`
  - load: `41.63s -> 872.30s`
  - `bulk_insert_select`: `21.26s -> 455.38s`
  - `bulk_update_with_case`: `1.54s -> 94.50s`

## Highest confidence causes carried into this package

1. ordinary multi-row insert and `INSERT ... SELECT` still behave like repeated
   singleton inserts on the server side
2. row-store insert still pays excessive per-row metadata and post-insert work
3. non-indexed updates still pay per-row exact-family bookkeeping on the
   unchanged-key path
4. prepared-query performance reuse exists in specification but is not yet
   closed as a full current-authority implementation lane
5. donor-grade `ICP`, `MRR`, `BKA`, memoize, incremental sort, runtime
   filters, vectorized upper-stage execution, and legal intra-query parallelism
   are not yet all dominant in the benchmarked paths
6. benchmark runner binary provenance was previously too loose and must be
   pinned before closeout

## Planning assumptions

- package `10` is allowed to consume completed package `08` implementation work
  as current baseline rather than restarting from zero
- package `10` is the deeper performance implementation program
- package `08` remains the downstream release-evidence consumer
- donor `no_transaction` results are stretch references only until ScratchBird
  admits an equivalent reduced-guarantee class

## Mandatory outputs from this package

- per-process parity tracker with numeric ceilings
- per-table load tracker with numeric ceilings
- donor fast-path assimilation tracker
- dependency-ordered ticket queue
- benchmark discipline closure requirements
- final reproducible parity rerun evidence pack
