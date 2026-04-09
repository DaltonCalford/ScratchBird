# Repo Local CTest Results and Artifact Tree Model

## Purpose

Define the artifact model for repository-local test registration and execution in the ScratchBird tree.

## Primary Registration Surface

The authoritative local registration surface is:

- `tests/CMakeLists.txt`

This file controls:

- gtest discovery
- individual `add_test` registrations
- labels
- optional or commented-out lanes
- compatibility and gate wrappers

## Execution Surfaces

Repo-local execution is currently split across:

- direct `ctest`
- `tests/run_tests.sh` shell selectors over `ctest`
- standalone compliance shell contracts
- compatibility wrapper scripts
- conformance runner scripts
- specialized benchmark binaries
- sanitizer-specific registrations

## Important Artifact Classes

A full build and test cycle may emit:

- CTest result rows
- binary-specific stdout and stderr
- shell-level selector output from `tests/run_tests.sh`
- standalone shell contract reports
- gate result directories
- compatibility result trees
- benchmark JSON or CSV outputs
- text logs for wrapper scripts

The artifact set depends on which execution surface ran the tests.

Current code-backed selector aliases in `tests/run_tests.sh` include:

- `smoke`
- `portable`
- `windows_portable`
- `linux_only`
- `unit`
- `integration`
- `stress`
- `performance`
- `quarantine`
- `quick`
- `ci`
- `all`

Those selector names are part of the current repo-local execution contract.

## Compatibility and Conformance Wrappers

The local tree includes wrappers for:

- emulation compatibility suites
- native conformance suites
- public-beta hard gate

Those wrappers are artifact-producing surfaces in their own right and shall not be conflated with plain `ctest` output.

## Benchmark Families Inside The Repo

The repo-local benchmark family includes, among others:

- JIT performance
- parser V3 benchmark
- optimizer cost calibration
- B-tree proof corpus
- front-door mode overhead
- cache and buffer benchmarks
- auth plugin enterprise performance

These benchmark binaries belong to the repository-local evidence set, distinct from the external `ScratchBird-Benchmarks` repository.

## Standalone compliance shell family

The repo-local compliance shell family includes tool-contract lanes such as:

- `tests/compliance/test_vnext_scope_scan_contract.sh`

This family is distinct from:

- aggregate GoogleTest discovery
- direct `ctest` execution
- external benchmark-matrix execution

It produces shell-level contract evidence rather than ordinary GoogleTest rows.

## Active, Sequential, and Sanitizer Lanes

The test tree includes:

- regular discovered gtests
- sequential-only binaries
- TSAN-specific binaries
- fuzz and soak lanes

A “full clean build test cycle” must state explicitly which of these execution classes were included.

That statement must also say whether standalone compliance shell contracts were
included, because they are not implied by ordinary `ctest` execution.

## Current Proof and Rebuild Boundary

Current repository evidence proves that the local tree already contains a large, label-rich, multi-runner test architecture. This specification reconstructs the artifact model so future audit and release documentation can describe what actually ran, not a generic `ctest` shorthand.
