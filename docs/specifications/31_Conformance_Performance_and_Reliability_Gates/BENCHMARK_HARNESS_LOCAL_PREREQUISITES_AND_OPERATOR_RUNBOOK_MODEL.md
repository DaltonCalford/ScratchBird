# Benchmark Harness Local Prerequisites and Operator Runbook Model

## Status

Current code-backed authority with reconstructed commercial-grade detail.

## Purpose

This document defines what an operator needs to run the benchmark harness locally and which environment controls materially affect reproducibility.

## Required Runtime Prerequisites

Normal matrix and benchmark operation requires:

- Docker Engine or Docker Desktop
- `python3`
- Python packages from `requirements.txt`
- local ability to execute the harness shell scripts

The matrix runner fails closed when `python3` is unavailable because Python reporting and summary generation are mandatory for matrix-grade output.

## Optional Inputs

The following are optional and must not be treated as universal prerequisites:

- local upstream source clones for regression-only or source-tree-driven lanes
- custom `.env` overrides
- suite-local report tags and notes

Local upstream source trees are optional for:

- stress
- acid
- engine-differential
- index-comparison

They are relevant for regression and similar clone-backed workflows only.

## Environment File Model

The matrix runner loads `.env` from the benchmark project root when present.

This environment file may provide:

- engine-selection overrides
- suite-selection overrides
- output-root overrides
- engine-connection settings consumed by start scripts and suite runners

Port-discovery files under `.benchmark-engine-ports/` are runtime outputs used to pass exact discovered host ports into subsequent suite runs.

## Canonical Operator Commands

The canonical matrix command model is:

```bash
SCRATCHBIRD_PG_QUERY_TIMEOUT_MS=30000 \
./scripts/run-benchmark-matrix.sh \
  --engines=firebird,mysql,postgresql \
  --suites=regression,stress,acid,performance,tpc-c,tpc-h,engine-differential,index-comparison \
  --report --compare
```

The canonical full-suite orchestrator command model is:

```bash
./run-all-tests.sh [suite] [engine]
```

These commands serve different purposes and must not be conflated.

## Reproducibility Controls

For reproducible benchmark operation, the operator must control at least:

- output root via `BENCHMARK_MATRIX_OUTPUT` or `--output`
- PostgreSQL query timeout where relevant, such as `SCRATCHBIRD_PG_QUERY_TIMEOUT_MS`
- requested engine set
- requested suite set
- whether compare and report generation are enabled

Mixing partial and complete runs inside the same output tree is non-conforming.

## Result Tree Expectations

The operator must expect one output root per matrix run or per full-suite run.

Matrix runs emit:

- `results/matrix-<timestamp>/...`

Full-suite runs emit:

- `results/full-test-suite-<timestamp>/...`

These trees are distinct and must remain distinct.

## System Information Collection

The full-suite orchestrator attempts to collect system information before running suites.

When Python is available and the collector script exists, the operator should expect:

- `system-info.json`

This artifact is intended to support report formatting and result interpretation.

The matrix runner is benchmark-decision authority even when the full-suite orchestrator has richer host-side report formatting behavior.

## Report Generation Boundary

Human-readable report generation is optional.

It may be enabled by:

- `REPORT=true` for the full-suite orchestrator
- `--report` for the matrix runner

Human-readable reports are secondary outputs. Raw JSON, matrix summary, and unified CSV remain authoritative.

## Operator Refusal Conditions

An operator must treat a run as non-authoritative when any of the following hold:

- `python3` is missing for the matrix path
- the result root lacks matrix summary or unified CSV
- the requested engine/suite set does not match the actual emitted suite rows
- raw per-suite JSON is missing for requested suites without corresponding recorded failure metadata
- output trees from unrelated runs are mixed together

## Local Machine Guidance

To run your own machine baseline correctly, the operator should:

1. install Docker and Python
2. install harness dependencies from `requirements.txt`
3. set any engine credentials or port overrides in `.env` only when necessary
4. choose a dedicated output root for the run
5. run the matrix path for cross-engine baselines
6. inspect `matrix-summary.json` and `matrix-comparison-unified.csv` before drawing conclusions

This is the minimum reproducible local-machine benchmark contract.
