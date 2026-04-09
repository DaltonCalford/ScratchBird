# ScratchBird Benchmarks Local Prerequisites and Operator Runbook

## Purpose

Define what an operator needs in order to run the external benchmark harness on a local machine and how to interpret a successful run.

## Required Local Prerequisites

Required for the main native matrix:

- Docker daemon access
- Python `3.8+`
- local checkout of `ScratchBird-Benchmarks`
- benchmark dependencies from `requirements.txt`
- `bash` execution for the project shell entrypoints

Optional but required for regression lanes:

- local Firebird source or regression clone path
- local MySQL source or regression clone path
- local PostgreSQL source or regression clone path

## Required Environment Bootstrap

The operator shall:

1. change into `ScratchBird-Benchmarks`
2. copy `.env.example` to `.env`
3. set clone-path variables when regression lanes are required
4. optionally set stable output roots and engine port overrides

Key environment controls:

- `FIREBIRD_REPO_PATH`
- `MYSQL_REPO_PATH`
- `POSTGRESQL_REPO_PATH`
- `BENCHMARK_MATRIX_OUTPUT`
- `BENCHMARK_FIREBIRD_PORT`
- `BENCHMARK_MYSQL_PORT`
- `BENCHMARK_POSTGRESQL_PORT`
- `SCRATCHBIRD_PG_QUERY_TIMEOUT_MS`

The runner family also consumes discovered host-port exports from:

- `.benchmark-engine-ports/firebird.env`
- `.benchmark-engine-ports/mysql.env`
- `.benchmark-engine-ports/postgresql.env`

## Recommended Baseline Procedure

The recommended baseline workflow is:

1. bootstrap `.env`
2. optionally create the Python environment with `scripts/setup-python-env.sh`
3. run the official matrix command
4. verify `matrix-summary.json`
5. verify `.matrix-runs.tsv`
6. verify `matrix-comparison-unified.csv`
7. drill into per-engine suite output trees only after matrix integrity passes

The official baseline matrix command is:

```bash
SCRATCHBIRD_PG_QUERY_TIMEOUT_MS=30000 \
./scripts/run-benchmark-matrix.sh \
  --engines=firebird,mysql,postgresql \
  --suites=regression,stress,acid,performance,tpc-c,tpc-h,engine-differential,index-comparison \
  --report --compare
```

## Single-Suite Debug Workflow

The canonical single-engine debugging flow is:

1. `scripts/start-engine.sh <engine> start`
2. `scripts/run-benchmark.sh <engine> <suite> --report --output <dir>`
3. `scripts/start-engine.sh <engine> stop`

This flow is authoritative for isolating one failing suite outside the full matrix.

The current developer-facing umbrella alternative is:

1. `run-all-tests.sh <suite> <engine>`
2. inspect `results/full-test-suite-<timestamp>/system-info.json`
3. inspect the suite log files in the same output root
4. treat text reports as secondary to raw JSON and integrity files

## Integrity Checklist

A matrix run is complete only when:

1. `matrix-summary.json` exists
2. `matrix-summary.json.result` reflects the expected run health
3. `.matrix-runs.tsv` has one row per requested `(engine,suite)` invocation
4. every requested engine has result JSON under every requested suite directory
5. `matrix-comparison-unified.csv` exists
6. comparison report trees exist when `--compare` was enabled

## Troubleshooting Model

Primary failure classes are:

- Docker permission or startup failure
- port conflict
- invalid regression clone path
- partial matrix because a suite invocation failed

The operator shall use `.matrix-runs.tsv` and `matrix-summary.json` as the first triage surfaces before reading deeper suite artifacts.
