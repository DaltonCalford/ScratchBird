# In Repo Microbenchmark and Calibration Lane

Status: current_authority

## Purpose

This file defines the purpose and output model of the in-repo benchmark lane.

## Current code-backed authority

The in-repo benchmark lane is a GTest-backed benchmark and calibration suite. It includes:
- generic benchmark harness timing and throughput reporting
- simulated performance suites for scans, aggregates, joins, and similar core operators
- optimizer cost calibration evidence tests
- parser and front-door benchmarks
- auth-plugin performance tests
- JIT performance envelope tests
- cache and buffer benchmarks

## Benchmark harness model

The core benchmark harness already provides:
- warmup iterations before measured runs
- timed iteration loops
- printed benchmark result structure carrying:
  - benchmark name
  - iterations
  - total time ms
  - average time ms
  - operations per second
  - optional data size and derived row rate

## Optimizer calibration lane

The optimizer calibration benchmark currently uses fixed corpus cases and profile-driven cost-model comparisons. It produces evidence tying:
- profile id
- calibration profile id
- workload profile
- table and index size inputs
- correlation
- seq and index total cost
- cost delta
- preference outcome

to emitted calibration rows.

## JIT performance lane

The current JIT benchmark lane measures interpreted versus preferred-native execution and records p95 microsecond envelopes. It is a comparative latency gate, not merely a smoke test.

## Artifact rules

The in-repo benchmark lane may emit:
- stdout benchmark summaries
- GTest pass or fail status
- CSV or delimited calibration evidence where the benchmark explicitly writes artifacts
- benchmark-related result files folded into the broader repo-local results tree when the orchestrator captures them

## Non-authority

This in-repo benchmark lane is not the external comparative matrix harness. Cross-engine comparison belongs to the `ScratchBird-Benchmarks` project.
