Status: reconstructed_required_with_current_substrate

# ScratchBird Benchmarks Regression Matrix Phase and Artifact Progress Model

## Purpose

This document defines the canonical phase model for a benchmark-matrix run and the artifact-progress evidence each phase must emit.

## Canonical Rule

The benchmark matrix is executed in phases. Each phase shall emit enough progress evidence that a later auditor can determine where a run succeeded, failed, or stopped.

## Canonical Phases

The phases are:

1. prerequisite validation
2. engine profile preparation
3. dataset preparation
4. suite launch
5. per-engine execution
6. result normalization
7. comparison and regression disposition

## Progress Evidence Rule

Each phase shall preserve:

- phase identity
- phase start marker
- phase completion marker or failure marker
- produced artifact root or sidecar evidence
- engine and suite identity in scope

## Matrix Rule

For matrix runs involving multiple engines or suites, phase progress shall be attributable by:

- matrix run identifier
- engine identifier
- suite identifier
- repetition or shard identifier where applicable

## Non-Guarantees

This file does not require one single orchestrator implementation. It requires explicit phase progress and artifact traceability.
