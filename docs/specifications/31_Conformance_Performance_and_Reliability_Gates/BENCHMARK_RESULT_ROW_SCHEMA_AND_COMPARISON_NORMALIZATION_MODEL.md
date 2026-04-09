Status: reconstructed_required_with_current_substrate

# Benchmark Result Row Schema and Comparison Normalization Model

## Purpose

This document defines the canonical row schema for benchmark results and the normalization rules used when comparing runs.

## Canonical Rule

Every benchmark result shall be representable as a structured row with enough identity and environment data to support honest comparison.

## Required Result Row Fields

Each result row shall include:

- run identifier
- engine identity
- suite identity
- workload identity
- dataset size or class
- concurrency profile
- repetition ordinal
- hardware profile identifier
- result metric name
- result metric value
- unit
- noise or confidence annotation if available

## Metric Families

The row schema shall support at minimum:

- throughput
- latency percentiles
- error count
- timeout count
- resource usage samples where emitted
- startup or load time where emitted

## Normalization Rule

Comparison tools shall normalize only across rows whose identity dimensions are materially comparable. If dimensions differ, the comparison shall be labeled as variant comparison rather than strict regression.

## Comparison Labels

Comparison labels shall include:

- `STRICT_BASELINE_COMPARISON`
- `DIMENSION_VARIANT`
- `HARDWARE_VARIANT`
- `ENGINE_VARIANT`
- `INFORMATIONAL_ONLY`

## Aggregation Rule

Aggregated summaries shall preserve the raw-row linkage. No summary may erase which raw rows and which dimensions produced it.

## Non-Guarantees

This file does not require one file format. It requires a canonical row schema that CSV, JSON, or other result formats can map onto.
