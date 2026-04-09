Status: reconstructed_required_with_current_substrate

# ScratchBird Benchmarks Engine Prerequisite Container and Port Profile Model

## Purpose

This document defines the canonical prerequisite profile for engines, containers, ports, and local runtime dependencies used by the external ScratchBird-Benchmarks harness.

## Canonical Rule

Every benchmarked engine profile shall publish its runtime prerequisites explicitly before a suite is launched. Suite logic shall not infer ports, containers, or engine startup shape from hidden defaults.

## Required Engine Profile Fields

Each engine prerequisite profile shall preserve:

- engine identity
- startup mode
- containerized or non-containerized classification
- required exposed ports
- local bind expectations
- data-root or volume expectations
- required helper processes if any

## Port Profile Rule

The port profile shall preserve:

- control or admin ports if used
- client protocol ports
- port collisions that make the profile non-admissible on the local host
- dynamic-versus-fixed port behavior

## Container Profile Rule

For container-backed engines, the profile shall preserve:

- image identity
- compose or orchestrator identity
- required environment variables
- required mounted volumes
- health-check or readiness rule

## Bare Process Rule

For non-containerized engines, the profile shall preserve:

- binary or startup entrypoint
- required config path
- required data path
- required startup readiness evidence

## Local Operator Rule

A local operator running the benchmark harness on their own machine shall be able to determine from the profile:

- what must already be installed
- what will be started by the harness
- which ports must be free
- which paths will store data and results

## Non-Guarantees

This file does not require one startup method across all engines. It requires deterministic prerequisite publication for each profile.
