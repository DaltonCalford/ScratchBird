# REPO_LOCAL_TEST_RESULTS_TREE_AND_ARTIFACT_EMISSION_MODEL

## Status

Current code-backed authority.

## Purpose

This document defines the artifact and results-tree model produced by a repository-local clean/build/test cycle.

## Governing boundary

Repository-local test artifacts are emitted by the ScratchBird repository build and test system.

They are distinct from:

1. external benchmark-matrix artifacts emitted by `ScratchBird-Benchmarks`
2. production observability or operator audit exports

## Required artifact classes

A repository-local clean/build/test cycle shall be interpreted as producing artifact classes in the following buckets:

### Build-tree artifacts

These include:

1. compiled test binaries under the active build tree
2. generated CMake and `CTest` control files
3. intermediate objects and generated support outputs needed by the test binaries

### `ctest` session metadata

The build tree shall contain the `CTest` execution metadata for aggregate test sessions. This metadata is authoritative for:

1. which registered tests were seen by the runner
2. pass/fail status at the `ctest` layer
3. the active test session identity inside the build tree

### Dedicated lane result roots

Compatibility and higher-order conformance lanes may emit dedicated result directories outside the narrow build-tree `CTest` metadata. Current authoritative examples include result roots under compatibility trees that carry timestamped `ctest` result directories.

### Per-script logs and preserved outputs

Gate scripts and benchmark-adjacent helper scripts may retain:

1. copied logs
2. summary files
3. normalized CSV or JSON outputs
4. preserved subordinate stdout and stderr

## Repository-local result-root rules

1. Build-tree `CTest` metadata belongs to the active build directory and shall not be conflated with compatibility result roots.
2. Compatibility and conformance result roots may live under the repository `tests/` tree and are authoritative for those lanes even when `ctest` also ran.
3. A successful generic `ctest` run does not prove every dedicated compatibility result root was created.

## Full clean/build/test interpretation

A full repository-local clean/build/test cycle shall be interpreted as the ordered composition of:

1. build-root preparation
2. test-target compilation
3. aggregate `ctest` execution for the registered tests
4. any explicitly-invoked gate or compatibility scripts
5. preservation of the result roots those scripts define

## Audit questions the artifact tree must answer

The emitted repository-local artifacts must allow a later reviewer to determine:

1. which build tree was used
2. which registered tests were visible to `ctest`
3. whether compatibility or conformance sub-lanes were invoked
4. where their result roots landed
5. whether the run was a plain developer cycle or a higher-order certification pass

## Non-authority clarification

The repository-local artifact tree is not an implicit benchmark result tree.

If the external benchmark harness did not run, the repository-local build and test artifacts shall not be used to claim benchmark execution or benchmark coverage.
