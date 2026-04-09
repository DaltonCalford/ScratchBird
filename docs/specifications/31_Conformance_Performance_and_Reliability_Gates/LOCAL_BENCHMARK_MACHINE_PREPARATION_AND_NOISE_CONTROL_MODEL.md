Status: reconstructed_required_with_current_substrate

# Local Benchmark Machine Preparation and Noise Control Model

## Purpose

This document defines how an operator prepares a local machine for reproducible ScratchBird benchmark runs.

## Canonical Rule

Benchmark results are only comparable when the machine-preparation model is recorded. A benchmark run without a machine-preparation record is informational only.

## Required Preparation Record

Each benchmark run shall preserve:

- host identity or profile name
- CPU and memory inventory
- storage layout and free-space state
- operating-system version
- container runtime version if Docker is used
- accelerator inventory if relevant
- background-load classification

## Noise-Control Requirements

Before running a local benchmark, the operator shall:

- identify whether the run is shared-host or isolated-host
- record significant background services or disable them where possible
- avoid concurrent build, test, or backup jobs on the benchmark host
- preserve whether caches are warm or cold before each suite

## Resource Preparation Rules

The operator shall confirm:

- sufficient free disk space for datasets and result artifacts
- memory headroom for the intended concurrency and dataset size
- stable CPU governor or power mode if applicable
- stable network path if the benchmark uses remote endpoints

## Container and Engine Preparation

If the harness uses containerized engines, the operator shall record:

- image identity
- exposed ports
- environment variables or compose profile
- volume paths used for data durability
- cleanup policy between runs

## Repeatability Rule

When rerunning a suite locally, the operator shall preserve or explicitly vary:

- dataset seed
- iteration count
- concurrency profile
- cache-warmth profile
- engine configuration profile

## Result Interpretation Rule

If the machine-preparation record differs materially between two runs, those runs shall not be treated as a strict regression pair unless the changed dimensions are the explicit subject of the comparison.

## Non-Guarantees

This file does not require a dedicated benchmark appliance. It requires that local noise and machine state be recorded and controlled well enough for honest comparison.
