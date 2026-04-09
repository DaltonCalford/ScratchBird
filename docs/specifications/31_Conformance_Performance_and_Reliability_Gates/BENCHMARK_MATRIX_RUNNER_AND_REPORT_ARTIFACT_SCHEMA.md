# Benchmark Matrix Runner and Report Artifact Schema

Status: current_authority

## Purpose

This file defines the artifact layout emitted by the external benchmark matrix runner.

## Current runner model

The external benchmark harness currently includes:
- Python benchmark runner objects with per-test `BenchmarkResult` rows
- engine connector classes for Firebird, MySQL, and PostgreSQL
- micro-benchmark definitions and schema setup
- `run-all-tests.sh` aggregate orchestration
- `scripts/run-benchmark-matrix.sh` matrix orchestration across engines and suites

## Aggregate full-suite run root

`run-all-tests.sh` currently emits a full-suite root of the form:
- `results/full-test-suite-<timestamp>`

That root may contain:
- `system-info.json`
- suite logs such as `regression.log`, `stress.log`, `acid.log`
- optional `reports/` text outputs when reporting is enabled

## Matrix run root

`run-benchmark-matrix.sh` currently emits a matrix root of the form:
- `results/matrix-<run-id>`

Required high-signal artifacts include:
- `matrix-summary.json`
- `.matrix-runs.tsv`
- `matrix-comparison-unified.csv`
- `comparison-<suite>/...` comparison outputs
- per-engine suite directories under `<engine>/<suite>/`
- raw suite JSON files
- optional per-engine text reports

## `.matrix-runs.tsv` schema

Each line records:
- engine
- suite
- started_at
- duration_seconds
- exit_code
- status
- output_dir

## `matrix-summary.json` scope

The summary captures matrix-level execution metadata such as:
- engines requested
- suites requested
- total suite runs
- failed suite runs
- overall result
- suite run entries with engine, suite, status, duration, exit code, and output directory

## `matrix-comparison-unified.csv` scope

The consolidated CSV is the authoritative comparative matrix artifact. It must carry:
- run id
- suite
- metric
- one column per engine

Metric families include:
- matrix runtime metadata
- totals
- suite summary fields
- expectation-status summaries
- result-derived counts
- source artifact provenance

## System info rule

Comparative reporting is incomplete without machine-context capture. The aggregate and matrix runners must preserve system information whenever the collector is available.
