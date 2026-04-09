# Benchmark Baseline Comparison

- Current matrix: `/home/dcalford/CliWork/ScratchBird/tests/results/full_run_metrics/20260401T161737Z/matrix-comparison-unified.csv`
- Baseline matrix: `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/matrix-20260401T161737Z/matrix-comparison-unified.csv`

## Common Metrics

| Suite | Metric | Engine | Current | Baseline |
|---|---|---|---:|---:|
| `acid` | `artifact.result_json` | `firebird` | firebird/acid/acid_firebird_20260401_140909.json | firebird/acid/acid_firebird_20260401_140909.json |
| `acid` | `artifact.result_json` | `mysql` | mysql/acid/acid_mysql_20260401_140946.json | mysql/acid/acid_mysql_20260401_140946.json |
| `acid` | `artifact.result_json` | `postgresql` | postgresql/acid/acid_postgresql_20260401_141120.json | postgresql/acid/acid_postgresql_20260401_141120.json |
| `acid` | `matrix.duration_seconds` | `firebird` | 3.0 | 3 |
| `acid` | `matrix.duration_seconds` | `mysql` | 1.0 | 1 |
| `acid` | `matrix.duration_seconds` | `postgresql` | 1.0 | 1 |
| `acid` | `matrix.duration_seconds` | `scratchbird` | 0.0 | 0 |
| `acid` | `matrix.exit_code` | `firebird` | 0 | 0 |
| `acid` | `matrix.exit_code` | `mysql` | 0 | 0 |
| `acid` | `matrix.exit_code` | `postgresql` | 0 | 0 |
| `acid` | `matrix.exit_code` | `scratchbird` | 1 | 1 |
| `acid` | `matrix.status` | `firebird` | passed | passed |
| `acid` | `matrix.status` | `mysql` | passed | passed |
| `acid` | `matrix.status` | `postgresql` | passed | passed |
| `acid` | `matrix.status` | `scratchbird` | failed | failed |
| `acid` | `results.atomicity.count` | `firebird` | 5 | 5 |
| `acid` | `results.atomicity.count` | `mysql` | 5 | 5 |
| `acid` | `results.atomicity.count` | `postgresql` | 5 | 5 |
| `acid` | `results.consistency.count` | `firebird` | 6 | 6 |
| `acid` | `results.consistency.count` | `mysql` | 6 | 6 |
| `acid` | `results.consistency.count` | `postgresql` | 6 | 6 |
| `acid` | `results.count` | `firebird` | 19 | 19 |
| `acid` | `results.count` | `mysql` | 19 | 19 |
| `acid` | `results.count` | `postgresql` | 19 | 19 |
| `acid` | `results.durability.count` | `firebird` | 3 | 3 |
| `acid` | `results.durability.count` | `mysql` | 3 | 3 |
| `acid` | `results.durability.count` | `postgresql` | 3 | 3 |
| `acid` | `results.isolation.count` | `firebird` | 5 | 5 |
| `acid` | `results.isolation.count` | `mysql` | 5 | 5 |
| `acid` | `results.isolation.count` | `postgresql` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.passed` | `firebird` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.passed` | `mysql` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.passed` | `postgresql` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.total` | `firebird` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.total` | `mysql` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.total` | `postgresql` | 5 | 5 |
| `acid` | `summary.by_category.consistency.passed` | `firebird` | 6 | 6 |
| `acid` | `summary.by_category.consistency.passed` | `mysql` | 6 | 6 |
| `acid` | `summary.by_category.consistency.passed` | `postgresql` | 6 | 6 |
| `acid` | `summary.by_category.consistency.total` | `firebird` | 6 | 6 |
| `acid` | `summary.by_category.consistency.total` | `mysql` | 6 | 6 |
| `acid` | `summary.by_category.consistency.total` | `postgresql` | 6 | 6 |
| `acid` | `summary.by_category.durability.passed` | `firebird` | 3 | 3 |
| `acid` | `summary.by_category.durability.passed` | `mysql` | 3 | 3 |
| `acid` | `summary.by_category.durability.passed` | `postgresql` | 3 | 3 |
| `acid` | `summary.by_category.durability.total` | `firebird` | 3 | 3 |
| `acid` | `summary.by_category.durability.total` | `mysql` | 3 | 3 |
| `acid` | `summary.by_category.durability.total` | `postgresql` | 3 | 3 |
| `acid` | `summary.by_category.isolation.passed` | `firebird` | 1 | 1 |
| `acid` | `summary.by_category.isolation.passed` | `mysql` | 1 | 1 |
| `acid` | `summary.by_category.isolation.passed` | `postgresql` | 1 | 1 |
| `acid` | `summary.by_category.isolation.total` | `firebird` | 5 | 5 |
| `acid` | `summary.by_category.isolation.total` | `mysql` | 5 | 5 |
| `acid` | `summary.by_category.isolation.total` | `postgresql` | 5 | 5 |
| `acid` | `summary.errors` | `firebird` | 0 | 0 |
| `acid` | `summary.errors` | `mysql` | 0 | 0 |
| `acid` | `summary.errors` | `postgresql` | 0 | 0 |
| `acid` | `summary.failed` | `firebird` | 0 | 0 |
| `acid` | `summary.failed` | `mysql` | 0 | 0 |
| `acid` | `summary.failed` | `postgresql` | 0 | 0 |
| `acid` | `summary.passed` | `firebird` | 15 | 15 |
| `acid` | `summary.passed` | `mysql` | 15 | 15 |
| `acid` | `summary.passed` | `postgresql` | 15 | 15 |
| `acid` | `summary.skipped` | `firebird` | 4 | 4 |
| `acid` | `summary.skipped` | `mysql` | 4 | 4 |
| `acid` | `summary.skipped` | `postgresql` | 4 | 4 |
| `acid` | `summary.total` | `firebird` | 19 | 19 |
| `acid` | `summary.total` | `mysql` | 19 | 19 |
| `acid` | `summary.total` | `postgresql` | 19 | 19 |
| `engine-differential` | `artifact.result_json` | `firebird` | firebird/engine-differential/differential_firebird_20260401_140911.json | firebird/engine-differential/differential_firebird_20260401_140911.json |
| `engine-differential` | `artifact.result_json` | `mysql` | mysql/engine-differential/differential_mysql_20260401_140948.json | mysql/engine-differential/differential_mysql_20260401_140948.json |
| `engine-differential` | `artifact.result_json` | `postgresql` | postgresql/engine-differential/differential_postgresql_20260401_141523.json | postgresql/engine-differential/differential_postgresql_20260401_141523.json |
| `engine-differential` | `matrix.duration_seconds` | `firebird` | 3.0 | 3 |
| `engine-differential` | `matrix.duration_seconds` | `mysql` | 4.0 | 4 |
| `engine-differential` | `matrix.duration_seconds` | `postgresql` | 248.0 | 248 |
| `engine-differential` | `matrix.duration_seconds` | `scratchbird` | 0.0 | 0 |
| `engine-differential` | `matrix.exit_code` | `firebird` | 0 | 0 |
| `engine-differential` | `matrix.exit_code` | `mysql` | 0 | 0 |
| `engine-differential` | `matrix.exit_code` | `postgresql` | 0 | 0 |
| `engine-differential` | `matrix.exit_code` | `scratchbird` | 1 | 1 |

## Baseline Suite Health

| Engine | Suite | Status | Exit | Duration (s) |
|---|---|---|---:|---:|
| `firebird` | `stress` | passed | 0 | 215 |
| `firebird` | `acid` | passed | 0 | 3 |
| `firebird` | `engine-differential` | passed | 0 | 3 |
| `firebird` | `index-comparison` | failed | 1 | 1 |
| `mysql` | `stress` | passed | 0 | 16 |
| `mysql` | `acid` | passed | 0 | 1 |
| `mysql` | `engine-differential` | passed | 0 | 4 |
| `mysql` | `index-comparison` | passed | 0 | 1 |
| `postgresql` | `stress` | passed | 0 | 72 |
| `postgresql` | `acid` | passed | 0 | 1 |
| `postgresql` | `engine-differential` | passed | 0 | 248 |
| `postgresql` | `index-comparison` | passed | 0 | 3 |
| `scratchbird` | `stress` | failed | 1 | 2502 |
| `scratchbird` | `acid` | failed | 1 | 0 |
| `scratchbird` | `engine-differential` | failed | 1 | 0 |
| `scratchbird` | `index-comparison` | failed | 1 | 1 |

## Baseline Key Metrics

| Suite | Metric | Engine | Baseline |
|---|---|---|---:|
| `acid` | `matrix.duration_seconds` | `firebird` | 3 |
| `acid` | `matrix.duration_seconds` | `mysql` | 1 |
| `acid` | `matrix.duration_seconds` | `postgresql` | 1 |
| `acid` | `matrix.duration_seconds` | `scratchbird` | 0 |
| `acid` | `matrix.status` | `firebird` | passed |
| `acid` | `matrix.status` | `mysql` | passed |
| `acid` | `matrix.status` | `postgresql` | passed |
| `acid` | `matrix.status` | `scratchbird` | failed |
| `acid` | `summary.errors` | `firebird` | 0 |
| `acid` | `summary.errors` | `mysql` | 0 |
| `acid` | `summary.errors` | `postgresql` | 0 |
| `acid` | `summary.failed` | `firebird` | 0 |
| `acid` | `summary.failed` | `mysql` | 0 |
| `acid` | `summary.failed` | `postgresql` | 0 |
| `acid` | `summary.passed` | `firebird` | 15 |
| `acid` | `summary.passed` | `mysql` | 15 |
| `acid` | `summary.passed` | `postgresql` | 15 |
| `engine-differential` | `matrix.duration_seconds` | `firebird` | 3 |
| `engine-differential` | `matrix.duration_seconds` | `mysql` | 4 |
| `engine-differential` | `matrix.duration_seconds` | `postgresql` | 248 |
| `engine-differential` | `matrix.duration_seconds` | `scratchbird` | 0 |
| `engine-differential` | `matrix.status` | `firebird` | passed |
| `engine-differential` | `matrix.status` | `mysql` | passed |
| `engine-differential` | `matrix.status` | `postgresql` | passed |
| `engine-differential` | `matrix.status` | `scratchbird` | failed |
| `index-comparison` | `matrix.duration_seconds` | `firebird` | 1 |
| `index-comparison` | `matrix.duration_seconds` | `mysql` | 1 |
| `index-comparison` | `matrix.duration_seconds` | `postgresql` | 3 |
| `index-comparison` | `matrix.duration_seconds` | `scratchbird` | 1 |
| `index-comparison` | `matrix.status` | `firebird` | failed |
| `index-comparison` | `matrix.status` | `mysql` | passed |
| `index-comparison` | `matrix.status` | `postgresql` | passed |
| `index-comparison` | `matrix.status` | `scratchbird` | failed |
| `index-comparison` | `summary.errors` | `mysql` | 0 |
| `index-comparison` | `summary.errors` | `postgresql` | 0 |
| `index-comparison` | `summary.failed` | `mysql` | 0 |
| `index-comparison` | `summary.failed` | `postgresql` | 0 |
| `index-comparison` | `summary.passed` | `mysql` | 3 |
| `index-comparison` | `summary.passed` | `postgresql` | 3 |
| `stress` | `matrix.duration_seconds` | `firebird` | 215 |
| `stress` | `matrix.duration_seconds` | `mysql` | 16 |
| `stress` | `matrix.duration_seconds` | `postgresql` | 72 |
| `stress` | `matrix.duration_seconds` | `scratchbird` | 2502 |
| `stress` | `matrix.status` | `firebird` | passed |
| `stress` | `matrix.status` | `mysql` | passed |
| `stress` | `matrix.status` | `postgresql` | passed |
| `stress` | `matrix.status` | `scratchbird` | failed |
| `stress` | `summary.errors` | `firebird` | 0 |
| `stress` | `summary.errors` | `mysql` | 0 |
| `stress` | `summary.errors` | `postgresql` | 0 |
| `stress` | `summary.errors` | `scratchbird` | 15 |
| `stress` | `summary.failed` | `firebird` | 0 |
| `stress` | `summary.failed` | `mysql` | 0 |
| `stress` | `summary.failed` | `postgresql` | 0 |
| `stress` | `summary.failed` | `scratchbird` | 0 |
| `stress` | `summary.passed` | `firebird` | 15 |
| `stress` | `summary.passed` | `mysql` | 15 |
| `stress` | `summary.passed` | `postgresql` | 15 |
| `stress` | `summary.passed` | `scratchbird` | 0 |
