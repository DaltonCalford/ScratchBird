# REPO_LOCAL_TEST_SUITE_TAXONOMY_AND_LABEL_MODEL

## Status

Current code-backed authority.

## Purpose

This document defines the authoritative taxonomy for the ScratchBird repository-local test tree. It exists to prevent another agent from treating the repository-local `tests/` tree, the `ctest` aggregate, and the external `ScratchBird-Benchmarks` matrix as one undifferentiated execution surface.

## Governing rules

1. Repository-local test authority is defined by the combination of:
   - files registered by `tests/CMakeLists.txt`
   - explicit shell entrypoints under `tests/`
   - explicit conformance and compatibility gate scripts
2. A file that exists under `tests/` but is not wired into the active CMake or shell gate path is not part of the aggregate repo-local required execution surface.
3. Repository-local test taxonomy is three-dimensional:
   - source tree placement
   - CMake and `ctest` registration
   - higher-order gate membership
4. Repository-local tests and external benchmark-harness runs are separate certification lanes.

## Canonical repository-local families

### `unit`

`unit` tests prove local subsystem contracts and narrow behavior boundaries. These tests are normally aggregated through the CMake test registration path and are expected to be runnable under the build-tree `ctest` umbrella.

Typical unit-family authority includes:
- parser and SBLR contract tests
- catalog contract tests
- optimizer and statistics contract tests
- security catalog and auth policy contract tests
- manager, proxy, connector, and transport contract tests

### `integration`

`integration` tests prove cross-subsystem behavior that cannot be certified by one narrow unit boundary alone. These tests cover engine-visible behavior such as:
- security policy enforcement
- row and column visibility behavior
- authentication runtime negotiation
- transaction, recovery, or management-path composition

Integration tests are authoritative for behavior that must be observed across real engine execution seams rather than inferred from one component in isolation.

### `compatibility`

`compatibility` tests are dialect or engine-facing regression gates that preserve claimed behavior for supported or emulated protocol/dialect surfaces. Their result trees and emission model are not identical to plain unit or integration `ctest` output and may include dedicated result directories under `tests/compatibility/.../results/ctest/...`.

### `conformance`

`conformance` tests express required certification bundles rather than one narrow subsystem. The currently important code-backed lane is the `public_beta` gate, which acts as a higher-order required gate and coordinates multiple required test families.

## Label and registration model

The authoritative execution label is the label that exists in the active CMake registration or shell gate path, not the folder name alone.

The repository-local test tree therefore uses the following hierarchy:

1. `tests/` source placement identifies intended family ownership.
2. `tests/CMakeLists.txt` identifies actual aggregate test registration.
3. Gate scripts identify required subsets, ordering, environment preparation, and failure semantics.

## Aggregate execution model

The aggregate repository-local run is not defined as "run every file under `tests/`".

The aggregate repository-local run is defined as:

1. configure the build tree with the active test targets enabled
2. build the registered test targets
3. execute the registered `ctest` surface and any required higher-order gate scripts
4. preserve the artifacts required by the current gate lane

## Distinction from benchmark harness

The repository-local `ctest` and gate tree is not the same thing as the external `ScratchBird-Benchmarks` harness.

Key differences:

1. repository-local tests certify product correctness and compatibility contracts inside the ScratchBird source tree
2. `ScratchBird-Benchmarks` is an external matrix harness that focuses on benchmarkable engines, Docker-based orchestration, result normalization, and consolidated reporting
3. a full repo-local clean/build/test cycle does not imply the external benchmark matrix was executed

## Required implementer interpretation

Another agent implementing or extending the test system shall treat the following as non-negotiable:

1. do not collapse all test lanes into one runner model
2. do not assume all files under `tests/` are active aggregate tests
3. do not claim benchmark coverage from repository-local `ctest` alone
4. do not claim repo-local required-gate completion from the external benchmark harness alone
