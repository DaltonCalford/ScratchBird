# Benchmark Matrix Execution and Output Root Model

## Purpose

Define the authoritative execution model for the external `ScratchBird-Benchmarks` native-engine matrix.

## Canonical Entry Point

The official matrix runner is:

- `ScratchBird-Benchmarks/scripts/run-benchmark-matrix.sh`

The canonical baseline command is:

```bash
SCRATCHBIRD_PG_QUERY_TIMEOUT_MS=30000 \
./scripts/run-benchmark-matrix.sh \
  --engines=firebird,mysql,postgresql \
  --suites=regression,stress,acid,performance,tpc-c,tpc-h,engine-differential,index-comparison \
  --report --compare
```

## Execution Model

The matrix runner shall:

1. Load local `.env` configuration if present.
2. Resolve the requested engine set.
3. Resolve the requested suite set.
4. Create one output root for the full matrix run.
5. Run one engine at a time for isolation.
6. Start the selected engine before its suite loop.
7. Run each suite through `scripts/run-benchmark.sh`.
8. Record one row per suite invocation in `.matrix-runs.tsv`.
9. Stop the engine after its suite loop unless `--keep-running` is enabled.
10. Generate `matrix-summary.json`.
11. Generate `matrix-comparison-unified.csv`.
12. Optionally generate per-suite comparison reports when `--compare` is enabled.

## Default Matrix Scope

Default engine set:

- `firebird`
- `mysql`
- `postgresql`

Default suite set:

- `regression`
- `stress`
- `acid`
- `performance`
- `tpc-c`
- `tpc-h`
- `engine-differential`
- `index-comparison`

## Output Root Rules

Default output root:

- `results/matrix-<utc-run-id>`

The output root may be overridden by:

- `--output=<dir>`
- `BENCHMARK_MATRIX_OUTPUT`

All artifacts for one matrix run shall remain under a single output root.

## Required Matrix Artifacts

Every matrix run shall produce or attempt to produce:

- `matrix-summary.json`
- `.matrix-runs.tsv`
- `matrix-comparison-unified.csv`
- `<engine>/<suite>/...` suite result trees
- `comparison-<suite>/...` when `--compare` is enabled

## `.matrix-runs.tsv` Contract

Each row shall contain:

- `engine`
- `suite`
- `started_at`
- `duration_seconds`
- `exit_code`
- `status`
- `output_dir`

This file is the invocation ledger for the run.

## `matrix-summary.json` Contract

The matrix summary shall include:

- `run_id`
- `started_at_utc`
- `completed_at_utc`
- `duration_seconds`
- `engines_requested`
- `suites_requested`
- `keep_running`
- `fail_fast`
- `generate_report`
- `generate_comparison_report`
- `total_suite_runs`
- `failed_suite_runs`
- `result`
- `suite_runs`
- `output_root`

`result` shall be:

- `passed` when `failed_suite_runs == 0`
- `failed` otherwise

## Failure Semantics

Matrix-level success means all requested suite invocations completed with zero suite failures.

`--fail-fast` changes stop behavior, not result semantics. A partial run still yields a failed matrix if any suite invocation fails.

## Comparison Report Generation

When `--compare` is enabled, the runner shall attempt cross-engine comparison report generation for every requested suite with at least two comparable result files.

For `index-comparison`, pairwise normalized index verdict artifacts are additionally generated when the specialized comparison script is available.

## Rebuild Boundary

This specification is grounded in the actual matrix runner implementation and is the authoritative artifact model for native baseline comparison before ScratchBird benchmark targets are promoted.
