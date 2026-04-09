# Benchmark Suite Measurement and Result Artifact Breakdown

## Status

Current code-backed authority with reconstructed commercial-grade detail.

## Purpose

This document defines what each currently authoritative benchmark suite measures, which result artifact is primary, and which fields matter most for matrix and consolidated-CSV comparison.

## Regression Suite

### Purpose

Regression integration compares ScratchBird compatibility work against the upstream engines' own regression expectations.

### Primary artifact

- `regression-<engine>-summary.json`

### Additional artifacts

- `regression-<engine>.log`
- copied raw regression output tree under `<engine>/regression/regression/<engine>/...`

### High-signal fields

- `totals.total`
- `totals.passed`
- `totals.failed`
- `totals.errors`
- `totals.pass_rate`

### Special prerequisite

Regression requires clone-path configuration for:

- Firebird repository
- MySQL repository
- PostgreSQL repository

## Stress Suite

### Purpose

Stress tests exercise multi-table joins, bulk operations, and aggregation-heavy workloads using engine-appropriate SQL.

### Primary artifact

- `stress_<engine>_<timestamp>.json`

### High-signal fields

- `summary.total_tests`
- `summary.passed`
- `summary.failed`
- `summary.errors`
- `summary.total_duration_ms`

### Operator control

`STRESS_SCALE` controls scale, with current runner default `medium`.

## ACID Suite

### Purpose

ACID tests validate transaction semantics and constraint behavior with matrix-comparable per-engine result JSON.

### Primary artifact

- `acid_<engine>_<timestamp>.json`

### Expected top-level keys

- `metadata`
- `results`
- `summary`

### High-signal fields

- `summary.total`
- `summary.passed`
- `summary.failed`
- `summary.errors`
- `summary.by_category.*`

### Category boundary

Current standalone category selectors are:

- atomicity
- consistency
- isolation
- durability

## Engine-Differential Suite

### Purpose

Engine-differential tests highlight architecture-sensitive behavior and help drive emulation decisions.

### Categories

- MySQL-optimized scenarios
- PostgreSQL-optimized scenarios
- Firebird-optimized scenarios

### Primary artifact

- `differential_<engine>_<timestamp>.json`

### High-signal fields

- `summary.total_tests`
- `summary.by_category.mysql_optimized`
- `summary.by_category.pg_optimized`
- `summary.by_category.fb_optimized`

### Timeout control

`SCRATCHBIRD_PG_QUERY_TIMEOUT_MS` is a current reproducibility control for PostgreSQL differential runs.

## Index-Comparison Suite

### Purpose

Index-comparison is the normalized plan-and-performance suite used to compare feature-equivalent access paths instead of treating engines as black boxes.

### Current scope

Current phase-1 scenario pack covers conservative stable B-tree shapes:

- point lookup
- range scan
- composite predicate with ordered output

### Primary artifact

- `index-comparison-<target>-<timestamp>.json`

### Additional comparison artifacts

- cross-engine text comparison output
- pairwise JSON verdict artifacts
- pairwise text verdict summaries

### High-signal fields

- normalized plan-family fields
- expectation-status rollups
- plan-capture success
- execution status
- comparative verdict in pairwise outputs

### Comparative boundary

Execution status and comparative verdict are separate and must never be collapsed into one status field.

## Performance, TPC-C, and TPC-H

These suites are present in the matrix set and benchmark repo, but current canon continues to treat them as non-authoritative for head-to-head decision-grade claims until their full artifact and interpretation contracts are recovered to the same depth as the suites above.

They remain valid benchmark lanes, but their current specification depth is not yet equal to:

- regression
- stress
- acid
- engine-differential
- index-comparison

## Report Artifact Locations

Per-engine text reports, when `--report` is enabled:

- `<engine>/<suite>/reports/benchmark_comparison_*.txt`

Cross-engine text reports, when `--compare` is enabled:

- `comparison-<suite>/benchmark_comparison_*.txt`

Index-comparison pairwise artifacts:

- `comparison-index-comparison/index-comparison-pairwise-*.json`
- `comparison-index-comparison/index-comparison-pairwise-*.txt`

## Matrix Projection Rule

Each suite must project its primary artifact into:

- suite-local raw JSON
- `matrix-summary.json` suite-run metadata
- `matrix-comparison-unified.csv`

The exact metric family exposed in the consolidated CSV depends on the suite artifact shape, but the suite artifact remains the source of truth.
