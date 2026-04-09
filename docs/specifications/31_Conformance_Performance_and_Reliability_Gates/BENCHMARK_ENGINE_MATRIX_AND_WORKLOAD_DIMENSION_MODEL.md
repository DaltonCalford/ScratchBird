Status: reconstructed_required_with_current_substrate

# Benchmark Engine Matrix and Workload Dimension Model

## Purpose

This document defines the canonical benchmark matrix used by the external ScratchBird-Benchmarks project and by any local operator reproducing those runs.

## Canonical Rule

The benchmark harness evaluates multiple engines, workloads, and execution dimensions. ScratchBird is one engine in that matrix; the harness is not a ScratchBird-only test runner.

## Matrix Axes

The benchmark matrix shall be modeled across these axes:

- engine under test
- benchmark suite
- workload family
- dataset size
- concurrency level
- run repetition count
- hardware profile
- result-capture profile

## Engine Axis

The engine axis shall distinguish:

- ScratchBird targets
- donor or comparison engines
- compatibility or emulation targets when relevant

Each result row shall preserve the engine identity exactly and shall never collapse unlike engines into a shared anonymous baseline.

## Suite Axis

The suite axis shall differentiate at minimum:

- ACID or transactional suites
- differential or engine-comparison suites
- regression suites
- stress suites
- index-comparison suites
- workload-specific benchmark suites

## Workload Axis

Each benchmarked workload shall be tagged with:

- OLTP-like transactional profile
- OLAP or scan-heavy profile
- mixed read or write profile
- index-heavy profile
- text, vector, or spatial profile where applicable
- migration or replication stress profile where applicable

## Dataset Axis

Dataset sizing shall preserve:

- row or document count
- value-width or payload class
- index count and family mix
- warm or cold-cache assumptions when applicable

## Concurrency Axis

Concurrency shall preserve:

- client count
- session count
- worker or thread count where configured
- transaction pattern class
- autocommit or multi-statement behavior

## Hardware Profile Axis

The hardware profile shall preserve:

- CPU model or class
- core and thread count
- memory size
- storage class
- accelerator presence if any
- operating-system family

## Repetition and Results Rule

Every benchmark row shall preserve:

- repetition ordinal
- run start and end timestamps
- command or configuration profile used
- output path for raw results
- derived summary path if produced

## Comparison Rule

Engine comparison is valid only when the workload and hardware axes are materially comparable. Cross-engine comparison shall never erase workload-shape or hardware differences.

## Non-Guarantees

This file does not require every suite to run against every engine. It requires a canonical way to represent the matrix when it does.
