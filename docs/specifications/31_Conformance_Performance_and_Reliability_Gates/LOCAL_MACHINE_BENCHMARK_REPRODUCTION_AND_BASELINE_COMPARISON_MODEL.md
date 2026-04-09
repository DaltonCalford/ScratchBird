Status: reconstructed_required_with_current_substrate

# Local Machine Benchmark Reproduction and Baseline Comparison Model

## Purpose

This document defines how local operators reproduce benchmark runs and compare them against prior local baselines.

## Canonical Rule

A benchmark reproduction is valid only when the prior baseline and the current run disclose comparable machine, workload, and configuration dimensions.

## Required Baseline Record

Each local baseline shall preserve:

- workload suite and workload identity
- engine identity and config profile
- machine-preparation record
- dataset size and seed
- concurrency profile
- repetition count
- result artifact path

## Reproduction Rule

To reproduce a baseline, the operator shall either:

- match all baseline dimensions
- or explicitly mark the changed dimensions and treat the run as a variant rather than a strict reproduction

## Comparison Classes

Comparisons shall be labeled as one of:

- `STRICT_REPRODUCTION`
- `CONFIG_VARIANT`
- `HARDWARE_VARIANT`
- `WORKLOAD_VARIANT`
- `INFORMATIONAL_ONLY`

## Regression Rule

Only `STRICT_REPRODUCTION` runs may be used for direct performance regression claims without additional qualification.

## Required Comparison Outputs

A baseline comparison shall preserve:

- baseline identifier
- current run identifier
- comparable dimensions
- changed dimensions
- summary of key latency or throughput deltas
- confidence or noise note

## Non-Guarantees

This file does not require a centralized benchmark service. It requires disciplined local reproduction and honest baseline comparison semantics.
