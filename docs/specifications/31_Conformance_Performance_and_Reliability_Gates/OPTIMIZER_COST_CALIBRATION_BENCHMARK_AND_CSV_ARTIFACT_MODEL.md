Status: current_authority

# Optimizer Cost Calibration Benchmark and CSV Artifact Model

## Purpose

This file defines the benchmark contract for fixed-corpus optimizer cost-model
calibration evidence.

## Current benchmark authority

The current benchmark constructs fixed calibration cases containing:

- table pages
- table rows
- index pages
- index rows
- correlation

The benchmark evaluates at least two workload profiles:

1. OLTP-like profile
2. analytic-scan profile

Each profile binds a concrete cost-formula profile and calibration-profile
identity.

## Required benchmark outputs

For each case and profile, the benchmark records:

- profile ID
- workload profile
- case ID
- table pages
- table rows
- index pages
- index rows
- correlation
- sequential total cost
- index total cost
- cost delta
- boolean `index_prefers`
- formula profile ID
- calibration profile ID

## CSV artifact contract

When `SB_OPTIMIZER_COST_BENCHMARK_CSV` is set, the benchmark shall emit a CSV
artifact containing:

1. one header row
2. one row per profile/corpus case combination
3. quoted CSV-safe columns

The environment variable path is part of the current artifact contract.

## Gate rule

The current benchmark requires:

1. at least one case where the OLTP profile prefers the index path
2. at least one case where the analytic profile prefers the index path
3. analytic index preference count must not exceed OLTP index preference count

## Interpretation rule

This benchmark is a calibration-evidence lane, not a substitute for end-to-end
query benchmarking.

It proves:

1. profile IDs and calibration IDs are attached to cost estimates
2. expanded terms are present
3. workload profile changes can alter index-versus-scan preference

## Reconstructed required expansion

The rebuild requires future calibration rows for:

1. vector and ANN families
2. summary and spatial families
3. warm resident versus cold resident vector states
4. accelerator-admitted versus CPU fallback vector paths
