# Benchmark Baseline Comparison

- Current matrix: `/home/dcalford/CliWork/ScratchBird/tests/results/full_run_metrics/20260403T054429Z/matrix-comparison-unified.csv`
- Baseline matrix: `/home/dcalford/CliWork/ScratchBird-Benchmarks/results/matrix-20260403T054429Z/matrix-comparison-unified.csv`

## Common Metrics

| Suite | Metric | Engine | Current | Baseline |
|---|---|---|---:|---:|
| `acid` | `artifact.result_json` | `firebird` | firebird/acid/acid_firebird_20260403_020659.json | firebird/acid/acid_firebird_20260403_020659.json |
| `acid` | `artifact.result_json` | `mysql` | mysql/acid/acid_mysql_20260403_020733.json | mysql/acid/acid_mysql_20260403_020733.json |
| `acid` | `artifact.result_json` | `postgresql` | postgresql/acid/acid_postgresql_20260403_020901.json | postgresql/acid/acid_postgresql_20260403_020901.json |
| `acid` | `artifact.result_json` | `scratchbird` | scratchbird/acid/acid_scratchbird_20260403_021452.json | scratchbird/acid/acid_scratchbird_20260403_021452.json |
| `acid` | `matrix.duration_seconds` | `firebird` | 4.0 | 4 |
| `acid` | `matrix.duration_seconds` | `mysql` | 1.0 | 1 |
| `acid` | `matrix.duration_seconds` | `postgresql` | 1.0 | 1 |
| `acid` | `matrix.duration_seconds` | `scratchbird` | 4.0 | 4 |
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
| `acid` | `results.atomicity.count` | `scratchbird` | 5 | 5 |
| `acid` | `results.consistency.count` | `firebird` | 6 | 6 |
| `acid` | `results.consistency.count` | `mysql` | 6 | 6 |
| `acid` | `results.consistency.count` | `postgresql` | 6 | 6 |
| `acid` | `results.consistency.count` | `scratchbird` | 6 | 6 |
| `acid` | `results.count` | `firebird` | 19 | 19 |
| `acid` | `results.count` | `mysql` | 19 | 19 |
| `acid` | `results.count` | `postgresql` | 19 | 19 |
| `acid` | `results.count` | `scratchbird` | 19 | 19 |
| `acid` | `results.durability.count` | `firebird` | 3 | 3 |
| `acid` | `results.durability.count` | `mysql` | 3 | 3 |
| `acid` | `results.durability.count` | `postgresql` | 3 | 3 |
| `acid` | `results.durability.count` | `scratchbird` | 3 | 3 |
| `acid` | `results.isolation.count` | `firebird` | 5 | 5 |
| `acid` | `results.isolation.count` | `mysql` | 5 | 5 |
| `acid` | `results.isolation.count` | `postgresql` | 5 | 5 |
| `acid` | `results.isolation.count` | `scratchbird` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.passed` | `firebird` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.passed` | `mysql` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.passed` | `postgresql` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.passed` | `scratchbird` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.total` | `firebird` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.total` | `mysql` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.total` | `postgresql` | 5 | 5 |
| `acid` | `summary.by_category.atomicity.total` | `scratchbird` | 5 | 5 |
| `acid` | `summary.by_category.consistency.passed` | `firebird` | 6 | 6 |
| `acid` | `summary.by_category.consistency.passed` | `mysql` | 6 | 6 |
| `acid` | `summary.by_category.consistency.passed` | `postgresql` | 6 | 6 |
| `acid` | `summary.by_category.consistency.passed` | `scratchbird` | 5 | 5 |
| `acid` | `summary.by_category.consistency.total` | `firebird` | 6 | 6 |
| `acid` | `summary.by_category.consistency.total` | `mysql` | 6 | 6 |
| `acid` | `summary.by_category.consistency.total` | `postgresql` | 6 | 6 |
| `acid` | `summary.by_category.consistency.total` | `scratchbird` | 6 | 6 |
| `acid` | `summary.by_category.durability.passed` | `firebird` | 3 | 3 |
| `acid` | `summary.by_category.durability.passed` | `mysql` | 3 | 3 |
| `acid` | `summary.by_category.durability.passed` | `postgresql` | 3 | 3 |
| `acid` | `summary.by_category.durability.passed` | `scratchbird` | 3 | 3 |
| `acid` | `summary.by_category.durability.total` | `firebird` | 3 | 3 |
| `acid` | `summary.by_category.durability.total` | `mysql` | 3 | 3 |
| `acid` | `summary.by_category.durability.total` | `postgresql` | 3 | 3 |
| `acid` | `summary.by_category.durability.total` | `scratchbird` | 3 | 3 |
| `acid` | `summary.by_category.isolation.passed` | `firebird` | 1 | 1 |
| `acid` | `summary.by_category.isolation.passed` | `mysql` | 1 | 1 |
| `acid` | `summary.by_category.isolation.passed` | `postgresql` | 1 | 1 |
| `acid` | `summary.by_category.isolation.passed` | `scratchbird` | 0 | 0 |
| `acid` | `summary.by_category.isolation.total` | `firebird` | 5 | 5 |
| `acid` | `summary.by_category.isolation.total` | `mysql` | 5 | 5 |
| `acid` | `summary.by_category.isolation.total` | `postgresql` | 5 | 5 |
| `acid` | `summary.by_category.isolation.total` | `scratchbird` | 5 | 5 |
| `acid` | `summary.errors` | `firebird` | 0 | 0 |
| `acid` | `summary.errors` | `mysql` | 0 | 0 |
| `acid` | `summary.errors` | `postgresql` | 0 | 0 |
| `acid` | `summary.errors` | `scratchbird` | 0 | 0 |
| `acid` | `summary.failed` | `firebird` | 0 | 0 |
| `acid` | `summary.failed` | `mysql` | 0 | 0 |
| `acid` | `summary.failed` | `postgresql` | 0 | 0 |
| `acid` | `summary.failed` | `scratchbird` | 2 | 2 |
| `acid` | `summary.passed` | `firebird` | 15 | 15 |
| `acid` | `summary.passed` | `mysql` | 15 | 15 |
| `acid` | `summary.passed` | `postgresql` | 15 | 15 |
| `acid` | `summary.passed` | `scratchbird` | 13 | 13 |

## Baseline Suite Health

| Engine | Suite | Status | Exit | Duration (s) |
|---|---|---|---:|---:|
| `firebird` | `stress` | passed | 0 | 115 |
| `firebird` | `acid` | passed | 0 | 4 |
| `firebird` | `engine-differential` | passed | 0 | 2 |
| `firebird` | `index-comparison` | failed | 1 | 1 |
| `mysql` | `stress` | passed | 0 | 16 |
| `mysql` | `acid` | passed | 0 | 1 |
| `mysql` | `engine-differential` | passed | 0 | 2 |
| `mysql` | `index-comparison` | passed | 0 | 1 |
| `postgresql` | `stress` | passed | 0 | 71 |
| `postgresql` | `acid` | passed | 0 | 1 |
| `postgresql` | `engine-differential` | passed | 0 | 244 |
| `postgresql` | `index-comparison` | passed | 0 | 2 |
| `scratchbird` | `stress` | passed | 0 | 92 |
| `scratchbird` | `acid` | failed | 1 | 4 |
| `scratchbird` | `engine-differential` | passed | 0 | 10 |
| `scratchbird` | `index-comparison` | passed | 0 | 5 |

## Baseline Key Metrics

| Suite | Metric | Engine | Baseline |
|---|---|---|---:|
| `acid` | `matrix.duration_seconds` | `firebird` | 4 |
| `acid` | `matrix.duration_seconds` | `mysql` | 1 |
| `acid` | `matrix.duration_seconds` | `postgresql` | 1 |
| `acid` | `matrix.duration_seconds` | `scratchbird` | 4 |
| `acid` | `matrix.status` | `firebird` | passed |
| `acid` | `matrix.status` | `mysql` | passed |
| `acid` | `matrix.status` | `postgresql` | passed |
| `acid` | `matrix.status` | `scratchbird` | failed |
| `acid` | `summary.errors` | `firebird` | 0 |
| `acid` | `summary.errors` | `mysql` | 0 |
| `acid` | `summary.errors` | `postgresql` | 0 |
| `acid` | `summary.errors` | `scratchbird` | 0 |
| `acid` | `summary.failed` | `firebird` | 0 |
| `acid` | `summary.failed` | `mysql` | 0 |
| `acid` | `summary.failed` | `postgresql` | 0 |
| `acid` | `summary.failed` | `scratchbird` | 2 |
| `acid` | `summary.passed` | `firebird` | 15 |
| `acid` | `summary.passed` | `mysql` | 15 |
| `acid` | `summary.passed` | `postgresql` | 15 |
| `acid` | `summary.passed` | `scratchbird` | 13 |
| `engine-differential` | `matrix.duration_seconds` | `firebird` | 2 |
| `engine-differential` | `matrix.duration_seconds` | `mysql` | 2 |
| `engine-differential` | `matrix.duration_seconds` | `postgresql` | 244 |
| `engine-differential` | `matrix.duration_seconds` | `scratchbird` | 10 |
| `engine-differential` | `matrix.status` | `firebird` | passed |
| `engine-differential` | `matrix.status` | `mysql` | passed |
| `engine-differential` | `matrix.status` | `postgresql` | passed |
| `engine-differential` | `matrix.status` | `scratchbird` | passed |
| `index-comparison` | `matrix.duration_seconds` | `firebird` | 1 |
| `index-comparison` | `matrix.duration_seconds` | `mysql` | 1 |
| `index-comparison` | `matrix.duration_seconds` | `postgresql` | 2 |
| `index-comparison` | `matrix.duration_seconds` | `scratchbird` | 5 |
| `index-comparison` | `matrix.status` | `firebird` | failed |
| `index-comparison` | `matrix.status` | `mysql` | passed |
| `index-comparison` | `matrix.status` | `postgresql` | passed |
| `index-comparison` | `matrix.status` | `scratchbird` | passed |
| `index-comparison` | `summary.errors` | `mysql` | 0 |
| `index-comparison` | `summary.errors` | `postgresql` | 0 |
| `index-comparison` | `summary.errors` | `scratchbird` | 0 |
| `index-comparison` | `summary.failed` | `mysql` | 0 |
| `index-comparison` | `summary.failed` | `postgresql` | 0 |
| `index-comparison` | `summary.failed` | `scratchbird` | 0 |
| `index-comparison` | `summary.passed` | `mysql` | 3 |
| `index-comparison` | `summary.passed` | `postgresql` | 3 |
| `index-comparison` | `summary.passed` | `scratchbird` | 3 |
| `stress` | `matrix.duration_seconds` | `firebird` | 115 |
| `stress` | `matrix.duration_seconds` | `mysql` | 16 |
| `stress` | `matrix.duration_seconds` | `postgresql` | 71 |
| `stress` | `matrix.duration_seconds` | `scratchbird` | 92 |
| `stress` | `matrix.status` | `firebird` | passed |
| `stress` | `matrix.status` | `mysql` | passed |
| `stress` | `matrix.status` | `postgresql` | passed |
| `stress` | `matrix.status` | `scratchbird` | passed |
| `stress` | `summary.errors` | `firebird` | 0 |
| `stress` | `summary.errors` | `mysql` | 0 |
| `stress` | `summary.errors` | `postgresql` | 0 |
| `stress` | `summary.errors` | `scratchbird` | 0 |
| `stress` | `summary.failed` | `firebird` | 0 |
| `stress` | `summary.failed` | `mysql` | 0 |
| `stress` | `summary.failed` | `postgresql` | 0 |
| `stress` | `summary.failed` | `scratchbird` | 0 |
| `stress` | `summary.passed` | `firebird` | 15 |
| `stress` | `summary.passed` | `mysql` | 15 |
| `stress` | `summary.passed` | `postgresql` | 15 |
| `stress` | `summary.passed` | `scratchbird` | 15 |
