Status: current_authority_with_reconstructed_expansion

# JIT Runtime Performance and Fallback Observability Model

## Purpose

This file defines the operator-visible metrics and inspection requirements for
the SBLR JIT runtime.

## Current code-backed authority

The current runtime proves that a performance snapshot exists and tracks:

- explicit compile attempts
- explicit compile successes
- explicit compile failures
- total compile latency
- native dispatch count
- VM dispatch count
- fallback count
- retired unusable artifact count
- compile queue enqueued count
- queued compile success count

The current runtime also proves that per-object performance inspection exists
for:

- compile success count
- total dispatch count
- native dispatch count
- VM dispatch count

## Required operator rows

The rebuilt specification requires at least two inspection granularities:

1. global runtime snapshot
2. object-local performance row

### Global runtime snapshot

The global runtime snapshot shall include:

- provider identity
- provider availability
- compile attempt count
- compile success count
- compile failure count
- total compile latency in microseconds
- native dispatch count
- VM dispatch count
- fallback count
- retired unusable artifact count
- compile queue depth
- compile queue enqueued count
- compile queue success count

### Object-local performance row

The object-local row shall include:

- object UUID
- last compatibility tuple used
- compile success count
- native dispatch count
- VM dispatch count
- total dispatch count
- last invalidation reason, if any

## Deterministic interpretation rules

Operators shall interpret these counters as follows:

1. rising `VM dispatch count` with flat `compile failure count` indicates fallback pressure, not necessarily correctness failure
2. rising `retired unusable artifact count` indicates artifact trust or compatibility churn
3. rising queue depth with flat queue success count indicates throughput pressure in background compilation
4. native dispatch count without matching trust and compatibility proof is non-conforming

## Reconstructed required expansion

The rebuild requires future operator visibility for:

- signature-invalid artifact count
- hash-mismatch artifact count
- dependency invalidation count
- security-policy invalidation count
- target-triple mismatch count
- queue overflow count
- queue suppression-by-policy count

## Fail-closed rule

Observability shall never relabel a VM fallback as a native success. Native
success metrics require an actually admitted native dispatch.
