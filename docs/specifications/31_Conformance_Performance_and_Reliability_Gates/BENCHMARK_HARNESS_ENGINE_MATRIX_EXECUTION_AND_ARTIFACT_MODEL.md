# Benchmark Harness Engine Matrix Execution and Artifact Model

## Status

Current code-backed authority with reconstructed commercial-grade detail.

## Purpose

This document defines the authoritative engine-matrix execution path in `ScratchBird-Benchmarks`, including engine startup order, suite execution order, comparison artifacts, and completeness rules for a valid benchmark matrix run.

## Authoritative Entry Point

The canonical matrix runner is:

- `ScratchBird-Benchmarks/scripts/run-benchmark-matrix.sh`

This runner is the authoritative path for reproducible cross-engine baseline generation.

## Default Engine Set

Unless explicitly overridden, the matrix runner must execute:

- `firebird`
- `mysql`
- `postgresql`

The engine set may be overridden by:

- `--engines=<csv>`
- `BENCHMARK_ENGINES`

## Default Suite Set

Unless explicitly overridden, the matrix runner must execute:

- `regression`
- `stress`
- `acid`
- `performance`
- `tpc-c`
- `tpc-h`
- `engine-differential`
- `index-comparison`

The suite set may be overridden by:

- `--suites=<csv>`
- `BENCHMARK_SUITES`

Unknown suites must be skipped with bounded warning behavior, not silently treated as successful work.

## Output Root

The output root defaults to:

- `results/matrix-<UTC timestamp>`

It may be overridden by:

- `--output=<dir>`
- `BENCHMARK_MATRIX_OUTPUT`

All matrix-level artifacts must be rooted beneath that output directory.

## Engine Execution Order

The matrix runner is isolation-first, not maximal-parallelism-first.

For each requested engine:

1. create `<output_root>/<engine>/`
2. start that engine using `scripts/start-engine.sh <engine> start`
3. load discovered engine-port exports from `.benchmark-engine-ports/<engine>.env` when present
4. run each requested suite sequentially through `scripts/run-benchmark.sh`
5. record one row in `.matrix-runs.tsv` for every engine/suite attempt
6. stop the engine with `scripts/start-engine.sh <engine> stop` unless `--keep-running` is set

This isolation-first model is authoritative because it preserves:

- reproducibility
- host-port determinism
- clearer suite failure attribution

## Suite Invocation Contract

Each suite run must receive:

- engine name
- suite name
- output directory
- optional report flags

The per-suite output directory is:

- `<output_root>/<engine>/<suite>/`

Per-suite result status must be recorded in `.matrix-runs.tsv` as:

- engine
- suite
- UTC started-at timestamp
- elapsed seconds
- exit code
- status
- output directory

## Fail-Fast and Keep-Running

`--fail-fast` means the matrix stops after the first failed suite for the active engine, and may stop the entire matrix pass depending on where the failure occurs.

`--keep-running` means the runner leaves each engine running after its suite list completes. This is an operator override and does not change artifact completeness requirements.

## Reporting and Compare Mode

`--report` enables suite-local human-readable report generation.

`--compare` enables per-suite cross-engine comparison report generation after suite outputs exist.

For `index-comparison`, compare mode additionally invokes the specialized pairwise comparator when the comparator script exists.

## Required Matrix Artifacts

A matrix run is complete only when these artifacts exist:

- `matrix-summary.json`
- `.matrix-runs.tsv`
- `matrix-comparison-unified.csv`
- per-engine per-suite raw JSON under `<engine>/<suite>/`

When compare mode is enabled, comparison artifacts under `comparison-<suite>/` are additional expected outputs.

For `index-comparison`, pairwise normalized verdict artifacts under `comparison-index-comparison/` are required when the suite is included and comparison generation is enabled.

## Matrix Summary Contract

`matrix-summary.json` must describe at least:

- run id
- start and completion timestamps
- requested engine set
- requested suite set
- total failure count
- matrix duration
- parsed suite-run records from `.matrix-runs.tsv`
- generation flags such as fail-fast, keep-running, report, compare

This file is the authoritative run-integrity object.

## Consolidated CSV Contract

`matrix-comparison-unified.csv` is the main decision artifact.

It must support cross-engine comparison for:

- run health
- correctness rollups
- suite durations
- suite summary metrics
- index-comparison verdict and plan-family fields when present

If this file is missing, the matrix run is incomplete even if raw suite JSON exists.

## Completeness Rules

A requested matrix run is complete only when:

1. `matrix-summary.json` exists
2. `.matrix-runs.tsv` has one row per requested engine/suite attempt
3. every requested `<engine>/<suite>/` has at least one result JSON unless the suite failed before output emission and the failure is represented in matrix metadata
4. `matrix-comparison-unified.csv` exists
5. failure counts and suite rows agree with the recorded engine/suite attempts

Partial output directories must not be treated as decision-grade results.

## Boundary With Full Test Orchestrator

The matrix runner is the authoritative cross-engine baseline path.

The broader `run-all-tests.sh` full-suite orchestrator is a wider harness surface, but it is not the authoritative matrix-comparison path for head-to-head benchmark decisions.
