# JIT Artifact Compatibility and Signature Admission Model

Status: current_authority

## Purpose

Define how compiled native artifacts are matched, admitted, rejected, retired, and optionally signature-gated.

## Compatibility Key

Current artifact compatibility is keyed by:
- object UUID
- canonical SBLR hash
- target triple
- CPU feature profile
- native ABI version
- compiler identity
- compiler version
- optimization profile
- security policy version

An artifact that does not match every required field is non-matching.

## Persisted Artifact Payload

Current artifact records may include:
- artifact id
- module id
- plan id
- native binary blob id
- optional signature blob id
- native blob bytes
- optional signature bytes
- native hash
- artifact state
- created transaction id
- created timestamp

Native and signature blobs are persisted through the catalog and TOAST-backed storage layer, not left as heap-only runtime state.

## Admission Rules

Artifact fetch must reject artifacts when:
- artifact state is `RETIRED`
- artifact state is not `READY`
- target triple mismatches
- CPU feature profile mismatches
- native ABI version mismatches
- compiler identity mismatches
- compiler version mismatches
- optimization profile mismatches
- canonical SBLR hash mismatches
- security policy version mismatches
- native hash is not valid SHA-256 hex
- signature is required but absent

## Signature Boundary

Current runtime can require signatures.

That means:
- signature enforcement is a runtime policy decision
- unsigned artifacts may exist
- unsigned artifacts are non-admissible when signature requirement is enabled

## Retirement Rules

The runtime must retire artifacts on at least:
- dependency signature change
- security policy version change
- explicit artifact retirement

Retirement is an admission boundary, not just an observability label.

## Fail-Closed Rules

1. Compatibility mismatch must not fall through to artifact reuse.
2. Policy-version mismatch must be treated as an artifact mismatch, not a warning.
3. Missing or malformed native hash must reject artifact admission.
4. Signature requirement must be enforced before native execution path selection.

## Cross-Section References

- `23_SBLR_VM_Compiler_and_Executor/NATIVE_COMPILATION_AND_ARTIFACT_LIFECYCLE.md`
- `33_Memory_Management/BUFFERED_RUNTIME_MEMORY_MODEL.md`
