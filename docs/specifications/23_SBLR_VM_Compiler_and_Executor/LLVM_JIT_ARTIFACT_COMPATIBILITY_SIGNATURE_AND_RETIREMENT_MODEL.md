# LLVM JIT Artifact Compatibility, Signature, and Retirement Model

## Status

Current code-backed authority with reconstructed commercial-grade detail.

## Purpose

This document defines the persisted native artifact model used by the LLVM JIT path, the compatibility key required to admit an artifact for execution, the verification rules applied before native selection, and the retirement rules used when artifacts become unusable.

## Artifact Identity Model

Each persisted native JIT artifact is identified by:

- artifact id
- module id
- plan id
- binary blob id
- optional signature blob id
- created transaction id
- created-at timestamp

The persisted payload is a native artifact blob, not an execution log and not a WAL record.

## Compatibility Key

Native artifact selection is keyed by a strict compatibility record containing:

- object UUID
- canonical SBLR hash
- target triple
- CPU feature profile
- native ABI version
- compiler identity
- compiler version
- optimization profile
- security policy version

All of these fields are part of admission truth. A near match is not sufficient.

## Canonical SBLR Hash Rule

The canonical SBLR hash is the byte-identity anchor for native artifact reuse.

If canonical SBLR changes, the artifact is not reusable, even if:

- object UUID is unchanged
- target platform is unchanged
- provider identity is unchanged

Artifact reuse without canonical-SBLR-hash equality is non-conforming.

## Artifact State Rule

Persisted artifacts carry a catalog state. Only `READY` artifacts are eligible for native selection.

Artifacts in any other state must be refused for execution.

Retired artifacts are never executable.

## Verification Pipeline

Before native selection, the runtime must:

1. locate candidate artifacts for the compatibility key's object UUID
2. reject retired artifacts
3. reject artifacts not in `READY`
4. compare compatibility key fields exactly
5. load the native blob
6. verify the stored payload hash
7. verify the LLVM artifact envelope
8. require signature presence when signature-required mode is enabled
9. load the signature blob when referenced

Only after these checks may the artifact be considered valid for native execution.

## Exact Mismatch Classes

Current mismatch classes include:

- target triple mismatch
- CPU profile mismatch
- native ABI mismatch
- compiler identity mismatch
- compiler version mismatch
- optimization profile mismatch
- security policy mismatch
- canonical SBLR hash mismatch

These mismatches are first-class reason codes, not generic verification failures.

## Hash and Signature Rules

If the artifact blob is present, it must carry or derive a valid SHA-256 hash.

Hash failures include:

- hash metadata not valid hex
- hash metadata not matching payload
- payload load failure

When signature enforcement is enabled:

- signature presence is mandatory
- signature load failure is fatal
- signature verification failure is fatal

Unsigned artifacts are therefore allowed only when signature enforcement policy permits them.

## LLVM Artifact Envelope Rule

The persisted LLVM bitcode artifact must include required metadata globals proving:

- provider identity
- provider version
- target triple
- native ABI
- optimization profile
- lowered IR content presence

If those metadata globals are absent or inconsistent with the compatibility key, the artifact is invalid.

The persisted artifact is therefore self-describing and self-checking.

## Artifact Selection Outcome

When a verified artifact is found:

- native path may be selected
- the runtime records artifact execution
- the outcome detail must identify native artifact selection

When no valid artifact exists:

- fallback or native refusal depends on effective execution policy
- the runtime records fallback when an artifact existed but could not be used

## Retirement Rules

Artifacts must be retired when they become unusable or stale. Current retirement paths include:

- explicit retirement by artifact id
- retirement on dependency signature change
- retirement on security policy version change
- retirement of unusable artifacts discovered during fetch or load

Retirement is catalog truth. A retired artifact is not a soft hint; it is out of service.

## Policy-Version and Dependency Invalidation

Security policy version is part of artifact compatibility. If the policy version changes:

- prior artifacts for the object must be retired
- native reuse must not cross the policy boundary

Likewise, dependency signature changes require retirement so the native artifact cannot outlive the semantic environment it was compiled against.

## MGA Rule

JIT artifacts are derivative execution accelerators.

They do not alter:

- MGA visibility
- transaction truth
- recovery truth
- security policy truth

Native artifact validity is subordinate to canonical SBLR, policy version, and runtime verification.
