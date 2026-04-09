# LLVM_JIT_PROVIDER_ARTIFACT_SELECTION_AND_VM_FALLBACK_MODEL

## Status

Current code-backed authority.

## Purpose

This document defines the actual LLVM-linked JIT provider model implemented in the current ScratchBird tree. It exists to prevent a later agent from inventing a direct "native JIT execution always exists" story that the current code does not prove.

## Governing boundary

The current JIT lane is an optional provider-backed artifact generation and selection path attached to the SBLR runtime.

It is not an unconditional replacement for the deterministic VM executor.

## Build-time provider model

The SBLR runtime links a JIT-provider surface through `scratchbird_llvm_jit_provider`.

Current authoritative build behavior is:

1. if `SCRATCHBIRD_LLVM_JIT_AVAILABLE` is true, the build advertises an LLVM provider identity, provider version, and host target triple
2. if `SCRATCHBIRD_LLVM_JIT_AVAILABLE` is false, the build remains valid and the VM path remains the deterministic fallback

The engine shall therefore never require LLVM merely to remain functional.

## Native-eligible surfaces

Current native eligibility is limited to the following routine surfaces:

1. function
2. trigger
3. procedure
4. package member

Unknown or non-routine surfaces are not native eligible and shall stay on the VM path.

## Policy envelope and merge rules

The effective JIT policy is derived from:

1. database-level policy
2. session-level policy
3. object-level policy

The effective compile mode is restrictive:

1. if any level is `EXPLICIT_ONLY`, the effective compile mode is `EXPLICIT_ONLY`
2. otherwise the effective compile mode is `JIT_ALLOWED`

The effective execution policy is also restrictive:

1. if any level is `INTERPRETED_ONLY`, the effective execution policy is `INTERPRETED_ONLY`
2. else if any level is `REQUIRE_NATIVE`, the effective execution policy is `REQUIRE_NATIVE`
3. otherwise the effective execution policy is `PREFER_NATIVE`

Hints are carried through as runtime suppressors:

1. `disable_compile`
2. `disable_execute`
3. `prefer_vm`

## Runtime selection algorithm

The current runtime selection algorithm is:

1. reject native selection for non-eligible routine surfaces
2. force VM if the effective execution policy is `INTERPRETED_ONLY`
3. force VM if `disable_execute` is set
4. force VM if `prefer_vm` is set
5. verify whether a persisted artifact exists for the requested compatibility key
6. if a verified artifact exists, select native
7. if a verification failure indicates a broken or unreadable artifact, record fallback and retire the unusable artifact when required
8. if the effective execution policy is `REQUIRE_NATIVE` and no valid artifact exists, return an error
9. if the artifact is missing and compile is allowed, queue compile work and run the VM path in the meantime unless `REQUIRE_NATIVE` forbids that fallback

## Compatibility key contract

An artifact is not selected by object identity alone.

The compatibility key includes:

1. object UUID
2. canonical SBLR hash
3. target triple
4. CPU feature profile
5. native ABI version
6. compiler identity
7. compiler version
8. optimization profile
9. security policy version

Another agent shall not weaken this selection contract.

## Explicit compile contract

Explicit compile is valid only when:

1. the routine surface is native eligible
2. canonical SBLR is present
3. object UUID, module ID, and plan ID are present
4. compatibility-key compiler metadata matches the active provider metadata
5. target triple is normalized and recognized by the LLVM provider

If any of these checks fail, compile is rejected.

## Current LLVM backend scope

The current LLVM provider backend:

1. validates provider availability
2. validates target triple and provider metadata
3. emits deterministic bitcode-style native artifact payloads
4. hashes and persists the resulting native artifact blob

The file-level authority is explicit that direct lowering-to-callable-native execution remains a later closure step.

That means the current LLVM lane is a provider-backed persisted artifact system with runtime selection and fallback, not yet a claim that every eligible routine is directly lowered to a process-local machine-code callable.

## Fallback and error rules

`PREFER_NATIVE` means:

1. use a verified artifact if one exists
2. otherwise fall back to VM
3. queue compile when policy allows it

`REQUIRE_NATIVE` means:

1. use a verified artifact if one exists
2. otherwise return an error
3. do not silently continue on the VM path

## Performance and observability counters

Current runtime metrics include at least:

1. VM dispatch count
2. native dispatch count
3. error dispatch count
4. compile queue depth and saturation counts
5. explicit compile attempts, successes, and failures
6. queued compile attempts, successes, and failures
7. compile latency
8. native execution CPU time
9. fallback count
10. load failure count
11. retired unusable artifact count

## Required implementer interpretation

Another agent shall preserve these boundaries:

1. LLVM is optional
2. VM fallback is first-class and deterministic
3. native selection is compatibility-key exact
4. `REQUIRE_NATIVE` is fail-closed
5. current provider scope is persisted artifact generation and selection, not a license to invent broader native-runtime claims
