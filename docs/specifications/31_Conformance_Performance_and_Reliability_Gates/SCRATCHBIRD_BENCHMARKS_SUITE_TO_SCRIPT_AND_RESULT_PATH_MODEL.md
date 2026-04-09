Status: reconstructed_required_with_current_substrate

# ScratchBird Benchmarks Suite to Script and Result Path Model

## Purpose

This document defines the canonical mapping between benchmark suites in the external ScratchBird-Benchmarks project, the orchestrator or script surfaces that execute them, and the result-path classes they emit.

## Canonical Rule

Every benchmark suite shall map to a deterministic execution surface and result-path class. A suite is not implementation-ready if another agent would need to guess which script runs it or where its artifacts land.

## Required Mapping Fields

Each suite mapping shall preserve:

- suite identity
- suite family
- primary runner script or orchestrator
- supporting helper scripts if required
- engine-matrix applicability
- default result-root class
- per-run result subdirectory identity

## Suite Families

The canonical suite families include at minimum:

- regression suites
- engine-differential suites
- ACID or transactional suites
- stress suites
- index-comparison suites
- workload-matrix suites

## Runner Rule

If a suite can be launched by more than one orchestration surface, the canon shall distinguish:

- the umbrella orchestrator
- the suite-local direct runner
- any dependency ordering between them

## Result Path Rule

The result-path contract shall distinguish:

- raw suite output
- per-engine result trees
- summarized comparison output
- environment or machine-preparation sidecar records

## Traceability Rule

From any result path, an operator shall be able to determine:

- which suite produced it
- which runner launched it
- which engine set participated
- which run identifier or timestamp scope applies

## Non-Guarantees

This file does not require every suite to share one identical directory layout. It requires a deterministic mapping from suite to runner and result-root class.
