# Full Test Orchestrator and Specialized Suite Boundary

## Status

Current code-backed authority with reconstructed commercial-grade detail.

## Purpose

This document defines the boundary between:

- the benchmark project's authoritative engine matrix
- the benchmark project's broader full-suite orchestrator surface
- the repo-local `tests/run_tests.sh` shell selector surface
- standalone compliance shell contracts under `ScratchBird/tests/compliance`

## Full-Suite Orchestrator Entry Point

The broader orchestrator entry point is:

- `ScratchBird-Benchmarks/run-all-tests.sh`

It creates:

- `results/full-test-suite-<timestamp>/`

This entry point is an umbrella harness for development, batch execution, and report formatting. It is not the sole authority for head-to-head matrix comparison.

It is also not the same surface as the repo-local CTest shell orchestrator.

## Suite Names Exposed By Full Orchestrator

The full orchestrator advertises these suite selectors:

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

## Current Execution Truth

Current code proves a split between:

- suites with direct script-backed execution paths
- suites that are currently shell-level placeholders or delegated concepts

Script-backed paths currently include at least:

- regression
- stress
- acid

The orchestrator also exposes names for additional specialized suites, but current code does not prove that every named suite has the same direct script-backed execution depth through this umbrella entry point.

Those named areas still matter for specification recovery because they describe intended test domains, but they must not all be misrepresented as equally mature harness paths today.

## System Information and Reporting Role

The full orchestrator performs host-side conveniences that the matrix runner does not center as its main role:

- system-info collection
- report formatting across emitted JSON artifacts
- umbrella result-root management for a broad run

These capabilities make the full orchestrator valuable for local review and batch runs, but the matrix runner remains the authority for deterministic cross-engine benchmark comparison.

## Repo-local shell orchestrator boundary

The repository-local test orchestrator is:

- `ScratchBird/tests/run_tests.sh`

Its authority is different from the benchmark umbrella runner.

It does:

- build `scratchbird_test_binaries`
- dispatch CTest by labels, exclusions, and timeout classes
- expose developer-facing selectors such as `quick`, `ci`, `portable`, and `all`

It does not:

- run the external engine benchmark matrix as its primary job
- produce the benchmark repo's consolidated comparison CSV as its primary output
- replace standalone compliance shell contracts

## Standalone compliance boundary

The repo also contains shell contract lanes outside aggregate GoogleTest and
outside the benchmark umbrella runner.

Current code-backed example:

- `tests/compliance/test_vnext_scope_scan_contract.sh`

That lane:

- creates a temporary git repository
- executes `tools/compliance/vnext_scope_scan.sh`
- checks exact exit-code behavior
- checks allowlist behavior
- checks report-section behavior

The full recovered test program therefore has at least three distinct
authoritative execution surfaces:

1. benchmark umbrella orchestration
2. repo-local CTest shell orchestration
3. standalone compliance shell contracts

## Specialized Suite Boundary

The following suite names are currently authoritative as test-domain labels even when their umbrella harness path is not equally mature:

- concurrency
- data-type
- ddl
- optimizer
- protocol
- catalog
- fault-tolerance

These are valid domains for specification recovery and future benchmark/test promotion. Their presence in the umbrella runner means the benchmark project already recognizes them as first-class quality lanes.

## Correct Interpretation Rule

Interpret the benchmark project as having two layers:

1. a decision-grade matrix-comparison layer
2. a broader umbrella full-suite layer for development and expansion

The specification set must preserve both layers without collapsing them into one fictionally uniform maturity level.

## Decision-Grade vs Development-Grade

Use the matrix runner as authority for:

- official cross-engine baselines
- consolidated comparison CSVs
- normalized index-comparison verdicts

Use the full orchestrator as authority for:

- umbrella suite taxonomy
- system-info attachment
- full-suite batch execution shape
- broader future-lane benchmark/test organization

Use the repo-local shell orchestrator as authority for:

- developer-facing build plus CTest selectors
- portable versus linux-only local execution lanes
- local smoke, quick, ci, and quarantine routing

Use standalone compliance shell contracts as authority for:

- tool-level contract enforcement not modeled as aggregate GoogleTest or
  benchmark-matrix execution

## Recovery Rule

When rebuilding benchmark and test canon from this project:

- matrix-grade claims require matrix-runner evidence
- umbrella suite existence may be recovered from the full orchestrator and suite tree
- repo-local CTest selector claims require `tests/run_tests.sh` evidence
- standalone tool-contract claims require direct shell-contract evidence
- missing direct runner depth for a named suite must be treated as an implementation or promotion lane, not as proof that the domain does not matter
