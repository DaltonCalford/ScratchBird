Status: reconstructed_required_with_current_substrate

# Repo Test Lane Label to Runner and Artifact Root Model

## Purpose

This document defines the canonical mapping from repo-local test lanes and labels to the runner surfaces and artifact roots that execute them.

## Canonical Rule

Every active test lane or label shall map to:

- a runner surface
- an execution scope
- an artifact-root class

No active lane may exist only as a label without a determinable runner and result location.

## Required Mapping Fields

Each lane mapping shall preserve:

- lane or label identity
- runner identity
- lane family
- default artifact-root class
- per-run subdirectory rule
- whether the lane participates in aggregate `ctest`, dedicated shell runners, or both

## Runner Classes

The canonical runner classes are:

- build-system registered `ctest` lane
- dedicated shell or harness runner
- compatibility or conformance runner
- benchmark-adjacent repo-local runner

## Artifact Root Rule

The artifact root shall distinguish:

- build-local test output
- compatibility results
- conformance results
- stress or special-run results

## Traceability Rule

From any lane label, an operator shall be able to determine:

- how to run the lane
- whether it is part of the aggregate clean-build-test cycle
- where its artifacts land

## Non-Guarantees

This file does not require every file under `tests/` to be part of one aggregate runner. It requires the active lanes to be mappable and auditable.
