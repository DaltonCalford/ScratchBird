Status: current_authority

# JIT Latency Envelope and Native Fallback Benchmark Model

## Purpose

This file defines the in-repo JIT benchmark contract for interpreted versus
native execution latency.

## Current benchmark authority

The current benchmark executes the same simple statement in two modes:

1. VM path:
   - compile mode `EXPLICIT_ONLY`
   - execution policy `INTERPRETED_ONLY`
2. JIT-capable path:
   - compile mode `JIT_ALLOWED`
   - execution policy `PREFER_NATIVE`

The benchmark currently:

1. runs 20 interpreted samples
2. runs 20 JIT-capable samples
3. measures each sample in microseconds
4. computes a p95 latency for each mode
5. emits:
   - `vm_p95_us`
   - `jit_p95_us`

## Gate rule

The current gate rule is:

1. `vm_p95_us > 0`
2. `jit_p95_us > 0`
3. `jit_p95_us <= vm_p95_us * 3 + 1`

This is a bounded envelope rule, not a universal claim that the JIT path must
always outperform the VM path in every environment.

## Interpretation rule

The benchmark proves:

1. JIT-capable execution remains within a bounded latency envelope
2. native path availability does not allow unbounded regression over the VM path
3. the benchmark is suitable as a sanity-performance gate, not as a final production capacity study

## Artifact model

The benchmark artifact model for this lane is:

1. stdout key/value pairs for:
   - `vm_p95_us`
   - `jit_p95_us`
2. pass/fail status from the gtest benchmark lane

## Coupling to certification

This benchmark is only valid when the JIT trust, invalidation, and fallback
certification lane is also green.

An implementation shall not claim JIT benchmark success while:

1. trust checks are bypassed
2. invalidation paths are broken
3. fallback reason codes are non-deterministic

## Reconstructed required expansion

The rebuild requires future additions for:

1. cold compile versus warm artifact reuse separation
2. per-object hit-rate reporting in benchmark artifacts
3. queue-pressure benchmark variants
4. target-mismatch and provider-unavailable benchmark variants
