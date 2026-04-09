# ScratchBird Benchmarks Project and Matrix Model

Status: current_authority_with_reconstructed_expansion

## 1. Purpose

`ScratchBird-Benchmarks` is an external, Docker-first benchmark harness. Its
current purpose is to establish repeatable upstream baselines and to execute
the active ScratchBird Beta 1 native benchmark lane under the same artifact
model.

The project has three active jobs:

- measure current upstream engine behavior under one harness
- measure ScratchBird Beta 1 native behavior under the same harness
- maintain the normalized comparison model used by ScratchBird native and
  ScratchBird emulation target lanes

It is not just a raw throughput leaderboard. It is intended to answer:

- which access path the engine chose
- whether the plan stayed on the expected index family
- whether the candidate fell back to a scan or weaker access path
- whether normalized plan quality and runtime were better, equivalent, or worse

## 2. Current and deferred benchmark targets

### 2.1 Current benchmarkable targets

Current upstream engines explicitly benchmarkable today are:

- FirebirdSQL
- MySQL
- PostgreSQL
- ScratchBird native
### 2.2 Current registry-defined ScratchBird target lanes

The target registry actively defines these ScratchBird comparison lanes:

- `scratchbird-native`
- `scratchbird-firebird`
- `scratchbird-mysql`
- `scratchbird-postgresql`

The native lane is Beta 1 release-significant and participates in the active
matrix and single-suite runners. The emulation lanes are enabled in the target
registry for targeted compare runs that explicitly select the ScratchBird
runtime service while keeping the donor dialect family.

## 3. Suite model

### 3.1 Official matrix-default suites

The official matrix runner `scripts/run-benchmark-matrix.sh` defaults to:

- `regression`
- `stress`
- `acid`
- `performance`
- `tpc-c`
- `tpc-h`
- `engine-differential`
- `index-comparison`

### 3.2 Authoritative suites for decision-grade interpretation today

The current benchmark README and strategy documents treat these as authoritative today:

- `index-comparison`
- `stress`
- `acid`
- `engine-differential`
- `regression` when local upstream clones are available

### 3.3 Present but not yet decision-grade lanes

These lanes are physically present in the project and wired into matrix dispatch, but current canon must still treat them as scaffold or work-in-progress comparison lanes unless explicitly hardened further:

- `performance`
- `tpc-c`
- `tpc-h`

### 3.4 Broader suite vocabulary advertised by the master orchestrator

`run-all-tests.sh` advertises a larger vocabulary than the official matrix default, including:

- `concurrency`
- `data-type`
- `ddl`
- `optimizer`
- `protocol`
- `catalog`
- `fault-tolerance`

These names exist in the orchestrator surface, but the authoritative benchmark matrix still comes from `run-benchmark-matrix.sh` and the concrete suite runner directories it invokes.

## 4. Comparison model

Two comparison classes are first-class in current canon.

### 4.1 Native upstream baseline comparison

This compares upstream engines against each other using one harness and one artifact model.

### 4.2 Normalized index-equivalence comparison

This compares feature-equivalent access behavior, not just engine-black-box runtime.

Current conservative phase-1 scope explicitly includes:

- B-tree point lookup
- B-tree range scan
- B-tree composite predicate with ordered output

This lane is the future bridge for:

- upstream engine vs ScratchBird emulation mode
- ScratchBird native vs upstream engine
- directional verdicts such as `better`, `equivalent`, `worse`, `fallback`, `unsupported`, and `invalid`

## 5. Execution model

### 5.1 Matrix execution

The authoritative matrix runner is:

- `scripts/run-benchmark-matrix.sh`

It:

- loads `.env` if present
- validates Python availability
- accepts engine list and suite list
- creates `results/matrix-<run-id>/`
- starts one engine at a time
- runs requested suites one engine at a time for isolation
- optionally generates per-suite text comparisons
- writes `.matrix-runs.tsv`
- writes `matrix-summary.json`
- writes `matrix-comparison-unified.csv`

The current default engine set is:

- `firebird`
- `mysql`
- `postgresql`
- `scratchbird`

### 5.2 Single-engine execution

The authoritative single-engine suite runner is:

- `scripts/run-benchmark.sh`

It:

- verifies exactly one intended engine is running
- collects system info into the output directory
- dispatches one suite against one engine
- optionally generates text reports
- writes raw suite JSON into the per-run output directory

### 5.3 Engine isolation rule

Benchmark execution is isolation-first:

- one engine should be running for a given benchmark pass
- if multiple engines are found, the single-suite runner attempts to stop non-target engines
- matrix execution starts and stops each engine around its suite batch unless `--keep-running` is enabled

## 6. Runtime prerequisites

### 6.1 Required for normal Docker-first use

- Docker daemon access
- Python 3
- Python benchmark dependencies from `requirements.txt`

ScratchBird native is the exception to the container-first rule. It is started
from the sibling `ScratchBird` workspace through the example manager runtime
instead of a benchmark-repo Docker image.

### 6.2 Required for regression lane

Local upstream source/test trees are required for regression:

- `FIREBIRD_REPO_PATH`
- `MYSQL_REPO_PATH`
- `POSTGRESQL_REPO_PATH`

The single-suite runner validates those paths before a regression run.

### 6.3 Port and env handling

Runtime ports are discovered and exported through:

- `.benchmark-engine-ports/firebird.env`
- `.benchmark-engine-ports/mysql.env`
- `.benchmark-engine-ports/postgresql.env`
- `.benchmark-engine-ports/scratchbird.env`

Optional operator configuration comes from:

- `.env`
- `.env.example`

## 7. Artifact model

For matrix output root `results/matrix-<run-id>/`, authoritative artifacts are:

- `matrix-summary.json`
- `.matrix-runs.tsv`
- `matrix-comparison-unified.csv`
- `<engine>/<suite>/*.json`
- `comparison-<suite>/benchmark_comparison_*.txt` when `--compare` is enabled
- `comparison-index-comparison/index-comparison-pairwise-*.json`
- `comparison-index-comparison/index-comparison-pairwise-*.txt`

For single-suite output root `results/<suite>-<engine>-<timestamp>/`, authoritative artifacts are:

- `system-info.json`
- raw suite JSON output
- optional `reports/benchmark_comparison_*.txt`
- regression copied output tree and `regression-<engine>-summary.json` when running the regression lane

## 8. Suite-specific semantics

### 8.1 `acid`

Correctness gate for atomicity, consistency, isolation, and durability.

### 8.2 `stress`

Mixed-workload stability and throughput lane.

### 8.3 `engine-differential`

Engine-biased scenario pack for architectural divergence analysis.

### 8.4 `index-comparison`

Normalized plan and verdict lane. Plan correctness is first, speed second.

ScratchBird native is required to emit structured JSON explain output for this
lane, and targeted emulation compare runs may bind `scratchbird-firebird`,
`scratchbird-mysql`, or `scratchbird-postgresql` as the logical benchmark
target.

### 8.5 `regression`

Upstream-regression integration lane using local upstream clones when configured.

### 8.6 `performance`, `tpc-c`, `tpc-h`

Present and dispatchable, but not yet promoted to the same decision-grade status as `acid`, `stress`, and `index-comparison`.

## 9. Decision workflow

The current benchmark strategy requires the following review order:

1. matrix health from `matrix-summary.json`
2. suite-run integrity from `.matrix-runs.tsv`
3. correctness counts from suite summaries or regression totals
4. runtime comparison from `matrix-comparison-unified.csv`
5. raw artifact drill-down from `artifact.result_json` and per-suite JSON
6. index-comparison pairwise verdict review when that lane is present

## 10. Non-authority and rejection rules

The following claims are incorrect:

- the benchmark project is only a speed leaderboard
- the matrix default and the master orchestrator suite lists are identical in authority
- every dispatchable suite is already decision-grade
- regression is part of the normal Docker-first path without local upstream clones
- `index-comparison` is only a latency lane rather than a normalized access-path lane
