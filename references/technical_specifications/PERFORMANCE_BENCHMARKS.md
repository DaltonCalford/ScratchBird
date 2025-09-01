# Performance Benchmarks Specification

## Definitions

- Transaction (TPS): one complete unit of work consisting of begin (implicit), a single INSERT or UPDATE of one row, and commit (implicit). For read‑only workloads, one SELECT with result materialized.
- Latency: end‑to‑end client‑perceived latency (avg, p95, p99).
- Throughput: operations per second (steady state).

## Hardware Baseline (Alpha)

- CPU: 4 vCPU (x86_64)
- RAM: 8 GB
- Storage: SSD (no RAID), ext4/xfs
- OS: Ubuntu 22.04 LTS, kernel ≥ 5.15
- Build: Release, no sanitizers

## Datasets

- Tiny: 100 rows (smoke tests)
- Small: 10K rows
- Medium: 1M rows

## Workloads

1) Write‑light OLTP (single‑row INSERT):
- Pre‑create table with appropriate schema
- Loop INSERT of single rows; commit every N rows (N=1, 10)

2) Read‑mostly (point SELECT):
- Table pre‑loaded with M rows (M as dataset)
- Random lookup on primary key; materialize 1 row

3) Scan + filter:
- Sequential scan with simple filter (e.g., WHERE value > threshold)

## Targets (Alpha)

- Single‑row INSERT: 100 TPS minimum on Tiny/Small
- Point SELECT: p95 < 5 ms on Small
- Scan + filter: linear throughput with dataset size (within memory constraints)

## Measurement Protocol

- Warmup: 10 seconds (excluded)
- Measurement window: 60 seconds
- Report: avg, p50, p95, p99 latency; throughput; CPU% and I/O stats (iostat)

## Acceptance

- A workload “passes” if all target thresholds are met on the hardware baseline and dataset specified.
- Report environment and commit hash in the benchmark output.

