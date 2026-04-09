# Benchmark Suite Breakdown and Local Execution Requirements

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines what the external `ScratchBird-Benchmarks` project currently
contains, what suite roots are physically present, what suite names are exposed
by its runners, what artifacts it emits, and what an operator needs in order to
run it locally without guessing.

## Project role

`ScratchBird-Benchmarks` is the Docker-first external benchmark and comparative
execution harness. It is separate from the in-repo `ScratchBird/tests/benchmark`
microbenchmark lane.

Its current authoritative jobs are:

- establish repeatable upstream baselines
- preserve a normalized comparison model for later ScratchBird target runs
- record machine context and result artifacts together
- generate consolidated matrix artifacts for cross-engine analysis

## Physical suite and support roots present in the current tree

The current checked-in benchmark repository proves these top-level suite and
support roots:

1. `acid-tests/`
- ACID and transactional-behavior validation lane

2. `engine-differential-tests/`
- cross-engine architecture-sensitive comparison lane

3. `index-comparison-tests/`
- normalized index-family comparison lane
- includes a maintained verdict model file

4. `regression-suites/`
- upstream regression orchestration lane
- includes:
  - `run-regression-suite.sh`
  - `compare_results.py`
  - donor-engine Dockerfiles

5. `stress-tests/`
- saturation and long-running workload lane

6. `system-info/`
- machine and environment capture layer

7. `scripts/`
- runner and orchestration layer, including:
  - `run-benchmark-matrix.sh`
  - `run-benchmark.sh`
  - `run-benchmarks.sh`
  - `benchmark_runner.py`
  - engine-start scripts

8. `docs/`
- operator runbook and reporting guidance

9. `engines/`
- engine profile and target ownership surface

The tree does not currently prove same-named top-level roots for every suite
name exposed by the runners.

## Runner-exposed suite vocabulary

The current runner surfaces expose a broader suite vocabulary than the physical
top-level directory layout.

### Matrix-default suites from `scripts/run-benchmark-matrix.sh`

- `regression`
- `stress`
- `acid`
- `performance`
- `tpc-c`
- `tpc-h`
- `engine-differential`
- `index-comparison`

### Additional suite names exposed by `run-all-tests.sh`

- `concurrency`
- `data-type`
- `ddl`
- `optimizer`
- `protocol`
- `catalog`
- `fault-tolerance`

These suite names are real runner-surface vocabulary, but they must not be
described as already having same-named physical suite roots in the current tree
unless the code for those roots is actually present.

## Concrete benchmark content recovered from current code

### `index-comparison-tests/`

This lane is authoritative for normalized access-path comparison, not just raw
latency.

Current proven artifacts and rules:

- pairwise verdict model in `index-comparison-tests/VERDICT_MODEL.md`
- maintained verdict vocabulary:
  - `better`
  - `equivalent`
  - `worse`
  - `fallback`
  - `unsupported`
  - `invalid`
- comparison noise-band rule documented as `5%`

This lane is the current canonical bridge for later:

- upstream vs ScratchBird emulation comparison
- upstream vs ScratchBird native comparison

### `engine-differential-tests/`

This lane is for behaviorally or architecturally biased scenarios, not merely a
generic throughput benchmark.

The benchmark strategy and matrix docs prove:

- category-sensitive scenario interpretation
- raw per-engine JSON output
- matrix-level timing summary integration
- explicit runtime control through `SCRATCHBIRD_PG_QUERY_TIMEOUT_MS`

### `regression-suites/`

This lane is optional and clone-dependent.

`run-regression-suite.sh` proves:

- donor-engine selection:
  - `firebird`
  - `mysql`
  - `postgresql`
  - `all`
- target modes:
  - `original`
  - `scratchbird`
- donor-runner use through Docker services
- copied raw regression results plus summary artifacts under a timestamped
  results tree

Regression therefore is authoritative as a separate lane, but it is not part of
the minimal Docker-first baseline unless donor regression assets are available.

### Microbenchmark path in `scripts/benchmark_runner.py`

The checked-in Python runner proves an additional concrete microbenchmark lane
for the three upstream engines:

- connectors:
  - Firebird via `fdb`
  - MySQL via `pymysql`
  - PostgreSQL via `psycopg2`
- benchmark schema:
  - `perf_test`
- benchmark result envelope:
  - `test_name`
  - `engine`
  - `duration_ms`
  - `iterations`
  - `rows_affected`
  - optional `error`

Current maintained microbenchmark scenarios in that runner are:

- `single_insert`
  - `1000` iterations
- `point_select`
  - `1000` iterations
- `simple_aggregate`
  - `100` iterations

This runner belongs to the current benchmark-project authority and must be kept
distinct from the official matrix-default suite set.

## Local execution requirements

### Required for the main Docker-first baseline

- Docker daemon access
- Python `3`
- dependencies from `requirements.txt`
- local checkout of `ScratchBird-Benchmarks`

### Optional but required for regression lanes

- local Firebird regression or source tree path
- local MySQL regression or source tree path
- local PostgreSQL regression or source tree path

### Environment and port bootstrap

Operators are expected to bootstrap from:

- `.env.example`
- `.env`

Port and runtime discovery are coordinated through:

- `.benchmark-engine-ports/firebird.env`
- `.benchmark-engine-ports/mysql.env`
- `.benchmark-engine-ports/postgresql.env`

## Canonical local execution model

The current maintained local flow is:

1. prepare `.env`
2. optionally create the Python environment with `scripts/setup-python-env.sh`
3. start engines with `scripts/start-engine.sh` or `scripts/start-engines.sh`
4. run one of:
   - `scripts/run-benchmark-matrix.sh`
   - `scripts/run-benchmark.sh`
   - `run-all-tests.sh`
   - `regression-suites/run-regression-suite.sh`
5. inspect matrix or suite artifacts before making any comparison claim

The canonical official baseline matrix command remains:

```bash
SCRATCHBIRD_PG_QUERY_TIMEOUT_MS=30000 \
./scripts/run-benchmark-matrix.sh \
  --engines=firebird,mysql,postgresql \
  --suites=regression,stress,acid,performance,tpc-c,tpc-h,engine-differential,index-comparison \
  --report --compare
```

## Artifact model

The benchmark project currently governs these artifact families:

- `matrix-summary.json`
- `.matrix-runs.tsv`
- `matrix-comparison-unified.csv`
- per-engine suite JSON under `<engine>/<suite>/`
- suite comparison text reports under `comparison-<suite>/`
- pairwise index-comparison artifacts under `comparison-index-comparison/`
- `system-info.json`
- suite logs and copied regression result trees for regression runs

Benchmark results live under the benchmark-project `results/` tree, not under
the in-repo `ScratchBird/tests/results/` tree.

## Authority and rejection rules

The following claims are incorrect:

- every suite name advertised by the umbrella or matrix runners already has a
  same-named physical suite root in the checked-in repository
- the external benchmark repo is the same evidence lane as the in-repo
  `tests/benchmark` family
- a declared future ScratchBird target is already active comparison authority
- regression is part of the normal minimal Docker-first baseline without donor
  regression assets
- benchmark claims can ignore system information, port identity, suite identity,
  or output-root integrity
