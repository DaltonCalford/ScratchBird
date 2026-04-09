# LLVM Artifact Compatibility and Runtime Fallback Rules

Status: current_authority

## Purpose

This file defines the exact compatibility key, persisted artifact envelope, verification sequence, and refusal or fallback behavior for current LLVM-backed native artifacts.

## Compatibility key

Current compatibility is keyed by:
- object UUID
- canonical SBLR hash
- target triple
- CPU feature profile
- native ABI version
- compiler identity
- compiler version
- optimization profile
- security policy version

## Persisted artifact model

A current persisted artifact carries:
- artifact id
- module id
- plan id
- binary blob id
- optional signature blob id
- optional native hash
- native blob payload
- optional signature blob payload
- compatibility key
- artifact state
- created transaction id
- created timestamp

Persistent payloads are catalog-plus-TOAST backed.

## Verification sequence

Current verified-fetch behavior follows this sequence.

1. enumerate artifact rows for the object UUID
2. sort candidates newest-first by created time, then created transaction id, then artifact id text
3. reject `RETIRED` artifacts
4. reject non-`READY` artifacts
5. match:
   - target triple
   - CPU feature profile
   - native ABI version
   - compiler identity
   - compiler version
   - optimization profile
   - canonical SBLR hash
   - security policy version
6. validate that the stored hash is 64-character SHA-256 hex
7. if signature is required, reject rows without a signature blob id
8. load the native blob from TOAST
9. compute SHA-256 over the loaded native blob and require equality with catalog metadata
10. run LLVM envelope verification over the loaded payload
11. when a signature blob id exists, load the signature payload from TOAST
12. admit the artifact only if all previous checks succeed

## Current LLVM envelope verification

Current LLVM envelope verification requires:
- native blob is non-empty
- payload is valid LLVM bitcode when LLVM verification support is compiled in
- presence of metadata globals:
  - `__sb_provider_identity`
  - `__sb_provider_version`
  - `__sb_target_triple`
  - `__sb_native_abi`
  - `__sb_opt_profile`
- target triple matches after normalization
- provider identity matches compatibility key
- provider version matches compatibility key
- native ABI matches compatibility key
- optimization profile matches compatibility key
- lowered IR payload is present and non-empty through `__sb_lowered_ir`

## Current reason-code families

Current refusal and fallback reasons include at least:
- artifact not found
- target triple mismatch
- CPU profile mismatch
- native ABI mismatch
- compiler identity mismatch
- compiler version mismatch
- optimization profile mismatch
- security policy mismatch
- canonical SBLR hash mismatch
- hash invalid
- blob load failed
- hash mismatch
- payload invalid
- signature invalid
- artifact retired
- artifact state not ready

## Runtime fallback rules

When verification fails:
- the runtime must remain correct on the VM or interpreted path
- load or payload failures count as fallback-worthy artifact problems
- payload or load-failure classes may trigger retirement of the unusable artifact
- `REQUIRE_NATIVE` may convert absence of a valid artifact into an error outcome rather than silent VM execution

## Signature rule

Current code supports optional signatures.
If `require_artifact_signature` is enabled at runtime, artifacts without a signature blob must be refused.

## LLVM-disabled build rule

When LLVM verification support is absent, current envelope verification fails closed with the effect that the native artifact is not admitted as verified native execution material.
A persisted artifact is never portable merely because it exists in catalog storage.
