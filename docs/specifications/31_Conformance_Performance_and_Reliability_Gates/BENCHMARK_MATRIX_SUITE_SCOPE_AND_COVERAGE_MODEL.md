# Benchmark Matrix Suite Scope and Coverage Model

## Purpose

Define which suites are part of the official matrix, which additional suites exist in the repository, and how those lanes should be interpreted.

## Official Matrix Coverage

The official matrix currently covers:

- `regression`
- `stress`
- `acid`
- `performance`
- `tpc-c`
- `tpc-h`
- `engine-differential`
- `index-comparison`

Official engine set:

- `firebird`
- `mysql`
- `postgresql`

## Primary Output Expectations by Suite

- `regression`
  - `regression-<engine>-summary.json`
- `stress`
  - `stress_<engine>_*.json`
- `acid`
  - `acid_<engine>_*.json`
- `performance`
  - `performance-<engine>-*.json`
- `tpc-c`
  - `tpc-c-<engine>-*.json`
- `tpc-h`
  - `tpc-h-<engine>-*.json`
- `engine-differential`
  - `differential_<engine>_*.json`
- `index-comparison`
  - suite JSON plus pairwise comparison artifacts when comparison mode is enabled

## Additional Repository Lanes

The repository also contains specialized test areas that are not part of the default matrix unless explicitly wired into matrix execution, including areas such as:

- catalog
- protocol
- optimizer
- DDL
- data-type
- fault-tolerance

These lanes are real repository work areas but are not automatically part of the official matrix decision model.

## Interpretation Rules

1. Official cross-engine decisions shall be made from the official matrix suite set.
2. Additional repository lanes are valid specialized development inputs but are not default head-to-head authority unless promoted into matrix execution.
3. `index-comparison` is the normalized plan-quality lane and shall be interpreted differently from raw throughput suites.
4. `engine-differential` is informative and engine-shape oriented, not a strict transactional correctness gate.

## Coverage Validation

Coverage is complete only when the output root contains:

- a complete matrix summary
- a complete run ledger
- suite JSON for every requested engine and suite
- a unified CSV with rows for every suite

Missing outputs are a coverage failure, not merely a reporting inconvenience.
