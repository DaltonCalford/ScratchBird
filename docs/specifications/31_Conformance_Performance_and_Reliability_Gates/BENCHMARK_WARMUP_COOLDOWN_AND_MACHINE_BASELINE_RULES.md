# Benchmark Warmup, Cooldown, and Machine Baseline Rules

## Purpose

This document defines the required warmup, cooldown, and machine-baseline rules
for ScratchBird benchmarks and load tests.

It exists so benchmark numbers are reproducible, interpretable, and auditable
instead of being casual one-off timing samples.

## Current code-backed authority

Current code-backed benchmark and load-test defaults already include:
- `BenchmarkRunner` defaults:
  - `warmup_duration = 10` seconds
  - `cooldown_duration = 5` seconds
  - `sample_interval_ms = 100`
  - `iterations = 1`
- `QueryLoadGenerator` defaults:
  - `pool_size = 10`
  - `pool_min_idle = 2`
  - `query_rate = 1000`
  - `duration = 60` seconds
  - `warmup = 10` seconds
  - `cooldown = 5` seconds
- local connection-pool warmup currently fills the pool until
  `min_connections` is reached
- remote connection-pool registry warmup currently exists as a declared surface
  but is not yet materially implemented

## Warmup contract

Warmup is mandatory for any benchmark or load test that claims to represent
steady-state performance.

Warmup shall:
1. occur before measurement begins
2. be excluded from reported latency and throughput aggregates
3. allow the following to stabilize when applicable:
   - connection pools
   - parser pools
   - page residency
   - resident ANN or vector index structures
   - JIT or LLVM artifacts
   - derivative caches and metadata caches
4. be recorded in the artifact manifest

The canonical minimum warmup modes are:
- `NONE`
- `TIME_ONLY`
- `UNTIL_POOL_READY`
- `UNTIL_RESIDENT_INDEX_READY`
- `UNTIL_JIT_READY`
- `COMBINED`

Current code proves only time-based warmup directly. The richer readiness-based
modes are required reconstructed behavior.

## Cooldown contract

Cooldown is mandatory when benchmark artifacts capture resource-monitor or tail
latency samples.

Cooldown shall:
1. occur after the measured interval
2. allow deferred flush, sample finalization, and monitor shutdown
3. be recorded in the artifact manifest
4. never be included in reported steady-state throughput

## Machine baseline capture

Every benchmark artifact set shall capture a machine baseline record with at
least:
- host identity
- operating system and kernel
- CPU model
- logical and physical core count
- RAM total
- storage class and filesystem
- page size
- compiler identity and version
- build profile
- protocol used
- network locality
- GPU or accelerator inventory when present
- driver/runtime versions for any accelerator used

If the machine baseline is missing, the benchmark artifact is non-conforming.

## Warmup readiness gates

A benchmark run may report itself as `warm` only when all required readiness
conditions for that suite are true.

The readiness classes are:
- `POOL_READY`
- `CACHE_READY`
- `RESIDENT_INDEX_READY`
- `ACCELERATOR_READY`
- `JIT_READY`
- `GOVERNANCE_READY`

A suite may declare one or more required readiness classes.

## Pool warmup rules

Current local pool warmup behavior is:
- create connections until `min_connections` is satisfied
- add created connections to the idle pool
- increment connection-pool statistics accordingly

Canonical pool-ready semantics are:
- all required minimum idle connections exist
- failed connection attempts are recorded
- the pool is not still in a first-connection penalty path

Remote pool warmup is required reconstructed behavior. It may not be claimed as
implemented merely because the API surface exists.

## Resident-index warmup rules

A benchmark that exercises resident vector or accelerator-backed index families
must declare whether it is measuring:
- cold first-use latency
- warm resident latency
- degraded fallback latency

These modes may not be mixed in one headline metric without explicit labeling.

## Artifact labeling requirements

Every benchmark artifact must label:
- `warmup_mode`
- `warmup_seconds`
- `cooldown_seconds`
- `readiness_classes_required`
- `machine_baseline_id`
- `steady_state_claim`
- `cold_or_warm_mode`

## Full clean/build/test cycle integration

A full clean/build/test/benchmark cycle shall emit separate artifact groups for:
- build outputs
- unit and integration test outputs
- conformance outputs
- in-repo microbenchmark outputs
- external benchmark-matrix outputs
- machine baseline capture

Benchmark artifacts may not be treated as valid if they cannot be tied back to a
specific build profile and machine baseline.

## Required reconstructed expansion

The rebuild requires additional benchmark artifact fields beyond what current
code directly proves:
- resident-index readiness markers
- accelerator readiness markers
- JIT readiness markers
- first-use versus warm-run labeling
- fallback mode labeling
- pool-ready versus time-only warmup labeling

## Non-guarantees

Warmup does not guarantee:
- globally stable thermal behavior across all hardware
- perfectly deterministic cache residency
- absence of operating-system noise
- identical results across different drivers, kernels, or CPU models

It guarantees only that ScratchBird declares and records which readiness gates
were satisfied before measurement.
