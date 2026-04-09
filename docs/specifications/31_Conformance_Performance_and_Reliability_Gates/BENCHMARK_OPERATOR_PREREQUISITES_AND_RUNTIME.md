# Benchmark Operator Prerequisites and Runtime

Status: current_authority_with_reconstructed_expansion

## 1. Purpose

This file defines what an operator needs in order to run `ScratchBird-Benchmarks` correctly and what the benchmark runtime actually does.

## 2. Mandatory prerequisites

Normal Docker-first benchmark use requires:

- Docker daemon access as the current user
- Python 3
- Python dependencies from `requirements.txt`

Recommended operator bootstrap:

1. `cd /home/dcalford/CliWork/ScratchBird-Benchmarks`
2. `python3 -m venv .venv`
3. `source .venv/bin/activate`
4. `pip install -r requirements.txt`
5. `cp .env.example .env`

## 3. Regression prerequisites

Regression requires local upstream source/test clones and valid env wiring:

- `FIREBIRD_REPO_PATH`
- `MYSQL_REPO_PATH`
- `POSTGRESQL_REPO_PATH`

Regression is not part of the minimal Docker-only path.

## 4. Engine-runtime prerequisites

Benchmark execution assumes the benchmark project controls benchmark engine containers and ports.

Important runtime rules:

- one engine at a time for the measured run
- host ports may be auto-discovered and written to `.benchmark-engine-ports/*.env`
- `run-benchmark.sh` validates the currently running engine and rejects the wrong-engine case

## 5. Canonical operator commands

### 5.1 Matrix baseline

Canonical matrix form:

```bash
SCRATCHBIRD_PG_QUERY_TIMEOUT_MS=30000 \
./scripts/run-benchmark-matrix.sh \
  --engines=firebird,mysql,postgresql \
  --suites=regression,stress,acid,performance,tpc-c,tpc-h,engine-differential,index-comparison \
  --report --compare
```

### 5.2 Single engine and suite

```bash
./scripts/start-engine.sh mysql start
./scripts/run-benchmark.sh mysql stress --report --output results/debug-mysql-stress
./scripts/start-engine.sh mysql stop
```

## 6. Runtime behavior of the single-suite runner

`run-benchmark.sh` currently does the following:

1. load `.env` if present
2. resolve Python interpreter
3. resolve default regression repo paths
4. verify the requested benchmark engine is the active one
5. collect `system-info.json`
6. dispatch the requested suite runner
7. optionally generate text reports
8. print result-summary guidance

## 7. Runtime behavior of the matrix runner

`run-benchmark-matrix.sh` currently does the following:

1. load `.env` if present
2. validate Python
3. parse `--engines`, `--suites`, `--output`, `--report`, `--compare`, `--fail-fast`, `--keep-running`
4. create matrix output root
5. start one engine
6. run all requested suites for that engine
7. optionally stop that engine
8. repeat for the next engine
9. write `.matrix-runs.tsv`
10. write `matrix-summary.json`
11. write `matrix-comparison-unified.csv`
12. optionally generate per-suite comparison reports

## 8. Present suite directories

The benchmark repository currently contains concrete directories for:

- `regression-suites`
- `stress-tests`
- `acid-tests`
- `performance-tests`
- `tpc-c`
- `tpc-h`
- `engine-differential-tests`
- `index-comparison-tests`
- `system-info`

That physical presence is stronger authority than any older placeholder description that implied these were only future lanes.

## 9. Artifact expectations by operator mode

### 9.1 Single-suite run

Expect:

- `system-info.json`
- one or more suite JSON files
- optional `reports/`
- regression copied output tree and summary when running `regression`

### 9.2 Matrix run

Expect:

- `.matrix-runs.tsv`
- `matrix-summary.json`
- `matrix-comparison-unified.csv`
- per-engine per-suite raw JSON trees
- `comparison-<suite>/` text reports when `--compare` is enabled

## 10. Quality and completeness checklist

A run is complete only when:

1. requested engines and suites are reflected in `matrix-summary.json`
2. `.matrix-runs.tsv` has the expected `(engine,suite)` rows
3. every requested suite directory contains raw JSON
4. consolidated CSV exists for matrix interpretation
5. pairwise artifacts exist when `index-comparison` and `--compare` are enabled

## 11. Non-authority and rejection rules

The following claims are incorrect:

- Docker alone is sufficient for regression runs
- benchmark ports are always fixed rather than discoverable
- the master orchestrator alone defines authoritative benchmark runtime
- a successful single-suite run implies matrix-summary and consolidated CSV artifacts exist
