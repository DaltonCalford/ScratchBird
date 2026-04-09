# Performance, TPC-C, and TPC-H Current Runner State and Promotion Boundary

## Status

Current code-backed authority with reconstructed promotion boundary.

## Purpose

This document defines the current implementation state of the `performance`, `tpc-c`, and `tpc-h` benchmark lanes in `ScratchBird-Benchmarks` and preserves the difference between:

- current emitted artifact truth
- required future benchmark authority

## Current Matrix Presence

These three suites are part of the current matrix default set:

- `performance`
- `tpc-c`
- `tpc-h`

They are therefore first-class benchmark lanes in the benchmark program and must remain part of the specification set.

## Current Runner Entry Points

The current suite runners are:

- `performance-tests/runners/performance_test_runner.py`
- `tpc-c/runners/tpc_c_runner.py`
- `tpc-h/runners/tpc_h_runner.py`

## Current Code Truth

All three current runners are placeholder implementations.

They currently:

- parse engine connection arguments
- parse limited suite-specific configuration arguments
- emit a result JSON file
- populate metadata
- mark the result payload as placeholder
- emit zeroed summary counters

They do not yet implement decision-grade benchmark workloads.

## Current Artifact Model

### Performance

Primary artifact:

- `performance-<engine>-<timestamp>.json`

Current metadata fields:

- `engine`
- `suite = performance`
- `timestamp`
- `host`

Current result payload:

- `results.status = placeholder`
- `results.message = Performance tests not yet fully implemented`

Current summary fields:

- `summary.total_tests = 0`
- `summary.passed = 0`
- `summary.failed = 0`
- `summary.score = N/A`

### TPC-C

Primary artifact:

- `tpc-c-<engine>-<timestamp>.json`

Current metadata fields:

- `engine`
- `suite = tpc-c`
- `timestamp`
- `warehouses`
- `duration`
- `host`

Current result payload:

- `results.status = placeholder`
- `results.message = TPC-C benchmark not yet fully implemented`

Current summary fields:

- `summary.total_tests = 0`
- `summary.passed = 0`
- `summary.failed = 0`
- `summary.score = N/A`

### TPC-H

Primary artifact:

- `tpc-h-<engine>-<timestamp>.json`

Current metadata fields:

- `engine`
- `suite = tpc-h`
- `timestamp`
- `scale_factor`
- `host`

Current result payload:

- `results.status = placeholder`
- `results.message = TPC-H benchmark not yet fully implemented`

Current summary fields:

- `summary.total_tests = 0`
- `summary.passed = 0`
- `summary.failed = 0`
- `summary.score = N/A`

## Matrix and CSV Implication

Because these runners emit valid JSON artifacts, they are still visible to:

- the matrix runner
- `matrix-summary.json`
- the unified CSV generator

However, their current placeholder payloads must not be treated as decision-grade throughput or workload evidence.

## Canonical Interpretation Rule

The specification must preserve both truths:

1. these suites are real benchmark lanes in the program and in the matrix
2. their current runner implementations are placeholder-grade and not yet authoritative for performance decisions

It is incorrect to describe them as absent. It is also incorrect to describe them as fully mature benchmark programs today.

## Required Promotion Target

To be promoted to decision-grade authority, each of these lanes must later specify and implement:

- workload definition
- data generation model
- scale controls
- warmup rules
- measurement windows
- correctness checks
- concurrency model where applicable
- summary metrics beyond placeholder counts
- stable verdict and comparison semantics

Until that promotion is completed, they remain:

- present
- artifact-producing
- matrix-visible
- non-authoritative for final benchmark decisions

## Relation To Full Spec Rebuild

These lanes are part of the rebuild stage model:

- current code proves artifact shape and placeholder state
- canon preserves the required benchmark intent
- future promotion closes the gap between the two
