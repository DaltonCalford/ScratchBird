# ACTIVE_REPO_TEST_LANES_VS_EXTERNAL_BENCHMARK_HARNESS_BOUNDARY

## Status

Current code-backed authority.

## Purpose

This document prevents implementers from mixing the repository-local test tree with the external `ScratchBird-Benchmarks` harness.

## Boundary

The repository-local ScratchBird test tree and the external `ScratchBird-Benchmarks` repository are related but separate systems.

## Repository-local system

The repository-local system exists to provide:

1. build-tree unit and integration execution
2. compatibility regression execution
3. conformance and release gate composition
4. developer-local and CI-local correctness certification

Its control roots are inside the ScratchBird repository, primarily under:

1. `tests/`
2. build-tree `CTest` metadata
3. gate scripts that orchestrate required local suites

## External benchmark harness

The external benchmark harness exists to provide:

1. benchmark-matrix orchestration across benchmarkable engines
2. Docker-oriented or environment-prepared execution
3. result normalization
4. consolidated reporting and CSV output
5. operator runbook guidance for running reproducible benchmark suites on another machine

Its control roots are inside the separate `ScratchBird-Benchmarks` repository.

## Required interpretation rules

1. Repository-local correctness certification does not imply benchmark completion.
2. External benchmark completion does not imply repository-local required-gate completion.
3. Artifacts from one system shall not be silently relabeled as if produced by the other.
4. Cross-linking is allowed only when the output model explicitly records which system produced the artifact.

## Implementer consequence

Another agent extending build, test, or benchmark automation shall preserve this separation in:

1. scripts
2. output directories
3. audit summaries
4. release-readiness claims
