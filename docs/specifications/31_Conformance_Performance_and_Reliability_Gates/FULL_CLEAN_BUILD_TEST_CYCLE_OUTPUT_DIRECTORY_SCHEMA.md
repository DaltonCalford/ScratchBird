Status: reconstructed_required_with_current_substrate

# Full Clean Build Test Cycle Output Directory Schema

## Purpose

This document defines the canonical artifact directory schema for a full clean, build, test, and benchmark cycle across the main ScratchBird repo and the external benchmark harness.

## Canonical Rule

The output model shall distinguish:

- repo-local build outputs
- repo-local test outputs
- compatibility and conformance result trees
- external benchmark harness outputs

Those outputs shall not be flattened into one anonymous directory.

## Repo-Local Build Outputs

The clean and build cycle shall preserve directories or files for:

- build-system generated files
- compiled binaries
- test executables
- intermediate objects if retained
- build logs or summary logs if emitted

## Repo-Local Test Outputs

The test cycle shall preserve directories or files for:

- ctest result summaries
- per-suite or per-lane logs where emitted
- compatibility result trees
- conformance result trees
- stress or special-lane outputs where emitted

## Compatibility and Conformance Tree Rule

Compatibility and conformance outputs shall preserve:

- engine or dialect family
- run timestamp or run identifier
- per-case outputs
- summary rollups if emitted

## External Benchmark Outputs

The benchmark harness shall preserve:

- raw run outputs
- per-engine result trees
- per-suite summaries
- comparison outputs if produced
- benchmark-matrix metadata describing the executed dimensions

## Canonical Artifact Identity

Every persisted artifact directory shall be attributable by:

- workflow class
- run identifier
- timestamp or monotonic run ordinal
- suite or lane identifier
- engine identity where relevant

## Cleanup Rule

Cleanup may remove obsolete outputs, but it shall never make a surviving artifact ambiguous about which workflow produced it.

## Certification Rule

Section 31 certification evidence shall be able to point from a single clean-build-test-benchmark run to:

- build artifacts
- test artifacts
- compatibility artifacts
- benchmark artifacts

without requiring inference from unrelated directories.

## Non-Guarantees

This file does not require one exact filesystem layout for every future tool. It requires stable classes, identity, and traceability of outputs.
