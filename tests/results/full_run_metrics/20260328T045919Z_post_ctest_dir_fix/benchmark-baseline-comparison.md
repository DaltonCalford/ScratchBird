# Benchmark Baseline Comparison

- Current matrix: `/home/dcalford/CliWork/ScratchBird/tests/results/full_run_metrics/20260328T045919Z_post_ctest_dir_fix/matrix-comparison-unified.csv`
- Baseline matrix: `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/matrix-20260328-dev-baseline-small/matrix-comparison-unified.csv`

## Common Metrics

No exact `(suite, metric, engine)` overlap exists between the current ScratchBird full-run matrix and the imported benchmark baseline.

## Baseline Suite Health

| Engine | Suite | Status | Exit | Duration (s) |
|---|---|---|---:|---:|
| `firebird` | `stress` | passed | 0 | 330 |
| `firebird` | `acid` | passed | 0 | 3 |
| `firebird` | `engine-differential` | passed | 0 | 2 |
| `firebird` | `index-comparison` | passed | 0 | 8 |
| `mysql` | `stress` | passed | 0 | 96 |
| `mysql` | `acid` | passed | 0 | 1 |
| `mysql` | `engine-differential` | passed | 0 | 3 |
| `mysql` | `index-comparison` | passed | 0 | 1 |
| `postgresql` | `stress` | passed | 0 | 75 |
| `postgresql` | `acid` | passed | 0 | 0 |
| `postgresql` | `engine-differential` | passed | 0 | 242 |
| `postgresql` | `index-comparison` | passed | 0 | 7 |

## Baseline Key Metrics

| Suite | Metric | Engine | Baseline |
|---|---|---|---:|
| `acid` | `matrix.duration_seconds` | `firebird` | 3 |
| `acid` | `matrix.duration_seconds` | `mysql` | 1 |
| `acid` | `matrix.duration_seconds` | `postgresql` | 0 |
| `acid` | `matrix.status` | `firebird` | passed |
| `acid` | `matrix.status` | `mysql` | passed |
| `acid` | `matrix.status` | `postgresql` | passed |
| `acid` | `summary.errors` | `firebird` | 0 |
| `acid` | `summary.errors` | `mysql` | 0 |
| `acid` | `summary.errors` | `postgresql` | 0 |
| `acid` | `summary.failed` | `firebird` | 0 |
| `acid` | `summary.failed` | `mysql` | 0 |
| `acid` | `summary.failed` | `postgresql` | 0 |
| `acid` | `summary.passed` | `firebird` | 15 |
| `acid` | `summary.passed` | `mysql` | 15 |
| `acid` | `summary.passed` | `postgresql` | 15 |
| `engine-differential` | `matrix.duration_seconds` | `firebird` | 2 |
| `engine-differential` | `matrix.duration_seconds` | `mysql` | 3 |
| `engine-differential` | `matrix.duration_seconds` | `postgresql` | 242 |
| `engine-differential` | `matrix.status` | `firebird` | passed |
| `engine-differential` | `matrix.status` | `mysql` | passed |
| `engine-differential` | `matrix.status` | `postgresql` | passed |
| `index-comparison` | `matrix.duration_seconds` | `firebird` | 8 |
| `index-comparison` | `matrix.duration_seconds` | `mysql` | 1 |
| `index-comparison` | `matrix.duration_seconds` | `postgresql` | 7 |
| `index-comparison` | `matrix.status` | `firebird` | passed |
| `index-comparison` | `matrix.status` | `mysql` | passed |
| `index-comparison` | `matrix.status` | `postgresql` | passed |
| `index-comparison` | `summary.errors` | `firebird` | 0 |
| `index-comparison` | `summary.errors` | `mysql` | 0 |
| `index-comparison` | `summary.errors` | `postgresql` | 0 |
| `index-comparison` | `summary.failed` | `firebird` | 0 |
| `index-comparison` | `summary.failed` | `mysql` | 0 |
| `index-comparison` | `summary.failed` | `postgresql` | 0 |
| `index-comparison` | `summary.passed` | `firebird` | 3 |
| `index-comparison` | `summary.passed` | `mysql` | 3 |
| `index-comparison` | `summary.passed` | `postgresql` | 3 |
| `stress` | `matrix.duration_seconds` | `firebird` | 330 |
| `stress` | `matrix.duration_seconds` | `mysql` | 96 |
| `stress` | `matrix.duration_seconds` | `postgresql` | 75 |
| `stress` | `matrix.status` | `firebird` | passed |
| `stress` | `matrix.status` | `mysql` | passed |
| `stress` | `matrix.status` | `postgresql` | passed |
| `stress` | `summary.errors` | `firebird` | 0 |
| `stress` | `summary.errors` | `mysql` | 0 |
| `stress` | `summary.errors` | `postgresql` | 0 |
| `stress` | `summary.failed` | `firebird` | 0 |
| `stress` | `summary.failed` | `mysql` | 0 |
| `stress` | `summary.failed` | `postgresql` | 0 |
| `stress` | `summary.passed` | `firebird` | 15 |
| `stress` | `summary.passed` | `mysql` | 15 |
| `stress` | `summary.passed` | `postgresql` | 15 |
