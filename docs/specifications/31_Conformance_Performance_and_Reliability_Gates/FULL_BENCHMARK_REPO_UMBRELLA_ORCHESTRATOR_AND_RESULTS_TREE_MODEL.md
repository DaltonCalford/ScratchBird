# Full Benchmark Repo Umbrella Orchestrator and Results Tree Model

## Purpose

Define the behavior of the repository-level umbrella test script in `ScratchBird-Benchmarks`.

## Canonical Entry Point

The umbrella orchestrator is:

- `ScratchBird-Benchmarks/run-all-tests.sh`

Its role is different from the official matrix runner:

- the matrix runner is the official native baseline comparison path
- the umbrella script is a developer-facing all-suites orchestrator with system-info collection and optional text report generation

It writes to a timestamped output root:

- `results/full-test-suite-<local-timestamp>`

## Supported Suite Names

The umbrella script recognizes:

- `all`
- `regression`
- `stress`
- `acid`
- `concurrency`
- `data-type`
- `ddl`
- `optimizer`
- `protocol`
- `catalog`
- `performance`
- `tpc-c`
- `tpc-h`
- `fault-tolerance`
- `engine-differential`
- `index-comparison`

This suite vocabulary is broader than the current physically present
same-named suite roots in the benchmark repository.

## Output Root

The umbrella script writes to:

- `results/full-test-suite-<local-timestamp>`

## First-Step System Information

Before suite execution, the umbrella script attempts to collect:

- CPU model and core counts
- memory totals
- OS identity
- disk information

into:

- `system-info.json`

The collector path used by the current script is:

- `system-info/collectors/system_info.py`

## Results Tree Rules

The umbrella script writes:

- top-level suite logs such as `regression.log`, `stress.log`, `acid.log`, `engine-differential.log`, `index-comparison.log`
- suite-generated JSON artifacts under the chosen result root
- `system-info.json`
- optional text reports under `reports/`

## Report Generation

When `REPORT=true`, the umbrella script shall:

1. find result JSON under the result root
2. ignore `system-info.json`
3. run the text result formatter for each benchmark JSON
4. write human-readable reports under `reports/`

Optional report controls:

- `TAGS`
- `NOTES`

The formatter path used by the current script is:

- `system-info/submit/result_formatter.py`

## Execution Model

When `suite=all`, the umbrella script runs a fixed suite order and records
per-suite durations in memory for a summary.

This script is tolerant of suite failure in individual launcher calls because
several suite invocations are piped through `tee` and guarded with `|| true`.
Therefore, operators shall treat summary lines alone as non-authoritative and
shall inspect emitted logs and JSON outputs directly.

The umbrella script is useful for:

- broad developer or operator sweeps
- system-info capture
- text report generation
- single output-root collection of multiple suite attempts

It is not the authoritative cross-engine comparison contract. The matrix runner
and its matrix-level artifacts remain the authoritative comparison lane.

## Authority Boundary

This umbrella script is useful for repository-wide operational runs and report generation, but the official cross-engine comparison contract remains the matrix runner and its matrix-level artifacts.
