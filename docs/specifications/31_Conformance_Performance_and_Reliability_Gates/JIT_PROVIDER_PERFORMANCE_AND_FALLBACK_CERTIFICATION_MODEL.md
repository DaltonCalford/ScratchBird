Status: current_authority_with_reconstructed_expansion

# JIT Provider Performance and Fallback Certification Model

## Purpose

This file defines the certification and performance-gate expectations for the
LLVM-backed JIT runtime.

## Certification requirements

The certification lane shall prove all of the following:

1. unusable artifacts are retired rather than reused
2. unusable artifact retirement can queue a rebuild
3. rebuild completion returns a native dispatch path when compatibility and trust conditions are satisfied
4. compile latency is recorded when explicit compile succeeds
5. native hit rate is visible through global and object-local performance rows
6. unsigned artifacts are rejected when signature-required trust is active
7. queue stress preserves VM correctness
8. dependency invalidation retires affected artifacts
9. security policy version invalidation retires affected artifacts

## Required gate scenarios

The JIT certification lane shall include at least:

1. exact-match native success path
2. missing-blob fallback path
3. target mismatch fallback path
4. hash-mismatch retirement and rebuild path
5. signature-required trust rejection path
6. compile-fault deterministic failure path
7. queue stress VM-correctness path
8. dependency invalidation path
9. security-policy invalidation path

## Performance gate expectations

The performance-gate surface shall capture:

- explicit compile latency
- native dispatch rate
- VM dispatch rate
- fallback rate
- rebuild success count
- unusable artifact retirement count

The gate shall reject a run that reports native dispatch success while the
underlying trust, compatibility, or availability checks are not satisfied.

## Benchmark interpretation boundary

JIT benchmark numbers are valid only when the certification lane for trust,
fallback, and invalidation is green.

Benchmark wins achieved by skipping trust or compatibility checks are
non-conforming.

## Reconstructed required expansion

The rebuild requires an eventual benchmark lane for:

1. compile queue latency under load
2. native-hit stability after warm artifact reuse
3. invalidation churn under dependency changes
4. fallback behavior under target mismatch and provider unavailability
