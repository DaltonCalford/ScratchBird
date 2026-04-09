# Beta 2 High Performance OLTP Benchmark And Contention Gate Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 certification gates for high-performance OLTP workloads.

## Required benchmark families

- point-read mix
- point-update mix
- append-heavy insert mix
- prepared small-batch write mix
- hot-key contention mix
- mixed OLTP plus background analytical interference mix

## Required metrics

- throughput
- p50, p95, p99 latency
- abort or retry rate
- admission wait time
- lock or fence wait time
- queue depth by service class

## Hotspot certification

The gate suite must include:

- one monotonic-key hot-right-edge workload
- one single-key hotspot workload
- one shard or range skew workload

Evidence must prove:

- mitigation is applied
- refusal paths are deterministic where mitigation is impossible
- OLTP-critical service classes remain protected from analytical or maintenance
  saturation

## Replay and artifact requirements

Each gate run shall publish:

- environment profile
- dataset and key-distribution profile
- prepared versus unprepared mix
- benchmark seed
- result CSV or JSON artifacts
- regression classification against the previous accepted baseline

## Refusal rules

- `OLTP_GATE_DATASET_PROFILE_MISSING`
- `OLTP_GATE_LATENCY_ARTIFACT_MISSING`
- `OLTP_GATE_SERVICE_CLASS_DISCLOSURE_MISSING`

## Cross-section requirements

- section `31` owns the benchmark corpus and regression thresholds
- section `25` owns service-class disclosure
- section `36` owns plan-shape disclosure for fast-path lanes
