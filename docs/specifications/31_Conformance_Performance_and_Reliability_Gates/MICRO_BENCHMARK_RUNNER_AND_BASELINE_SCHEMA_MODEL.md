# Micro Benchmark Runner and Baseline Schema Model

## Purpose

Define the actual behavior of the Python micro-benchmark runner in `ScratchBird-Benchmarks`.

## Canonical Entry Point

The micro runner is:

- `ScratchBird-Benchmarks/scripts/benchmark_runner.py`

## Supported Suites

Current code-backed suites:

- `micro`
- `all`

Additional parser choices such as `concurrent` and `regression` exist in the command-line surface but are not backed by equivalent execution logic in the current runner body. They are not current micro-runner authority.

## Engine Set

Current built-in engines:

- `firebird`
- `mysql`
- `postgresql`

Each engine uses a dedicated connector class and built-in connection defaults.

## Schema Setup Model

Before micro execution, the runner creates a simple `perf_test` table shape for the selected engine. Firebird additionally uses a generator-based insert pattern; MySQL and PostgreSQL use auto-increment or serial semantics.

## Current Micro Benchmarks

Current benchmark definitions are:

- `single_insert`
- `point_select`
- `simple_aggregate`

Each benchmark definition supplies:

- description
- engine-specific SQL text
- iteration count

## Execution Algorithm

For each engine:

1. connect
2. create the benchmark schema
3. warm up the benchmark for up to `10` iterations
4. run the benchmark iteration loop
5. accumulate total rows affected
6. record the total duration in milliseconds
7. write a JSON report at the selected output path

## Output JSON Contract

The runner output contains:

- `timestamp`
- `suite`
- `engines`
- `results`

Each result row contains:

- `test_name`
- `engine`
- `duration_ms`
- `iterations`
- `rows_affected`
- optional `error`

## Authority Boundary

This runner is a small direct Python benchmark path. It is not the authoritative matrix-comparison path and does not replace the matrix artifact model.
