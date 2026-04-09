# JIT Runtime Policy Artifact Verification and Fallback Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the current JIT runtime dispatch model for SBLR execution,
including policy resolution, artifact verification, fallback behavior, compile
queue admission, and retirement of unusable native artifacts.

## Current code-backed authority

The current runtime is defined by:

- `scratchbird/sblr/jit/jit_runtime.h`
- `src/sblr/jit/jit_runtime.cpp`
- `scratchbird/sblr/jit/jit_artifact_store.h`
- `src/sblr/jit/jit_artifact_store.cpp`
- `src/sblr/jit/jit_llvm_toolchain.cpp`

## Policy envelope and effective policy

The current runtime policy model has three stacked scopes:

- database
- session
- object

The current policy envelope carries:

- compile mode
- execution policy
- hints

Current compile modes:

- `EXPLICIT_ONLY`
- `JIT_ALLOWED`

Current execution policies:

- `INTERPRETED_ONLY`
- `PREFER_NATIVE`
- `REQUIRE_NATIVE`

Current hints:

- `disable_compile`
- `disable_execute`
- `prefer_vm`

The effective policy is resolved before any artifact lookup or compile-queue
decision.

## Native-eligible surfaces

The current runtime admits native execution only for these routine surfaces:

- `FUNCTION`
- `TRIGGER`
- `PROCEDURE`
- `PACKAGE_MEMBER`

The following surface is not native-eligible:

- `UNKNOWN`

If the surface is not native-eligible, dispatch must fail closed to the VM path
with a native-scope refusal reason.

## Dispatch path order

The current `selectPath(...)` runtime dispatch order is:

1. reject non-native-eligible surfaces to VM
2. resolve the effective policy
3. if `INTERPRETED_ONLY`, force VM
4. if `disable_execute`, force VM
5. if `prefer_vm`, force VM
6. fetch and verify the best matching artifact by compatibility key
7. if a valid artifact exists, choose native
8. if artifact verification fails:
   - record fallback
   - optionally retire the unusable artifact
   - choose error or VM depending on effective policy
9. if no artifact exists:
   - honor `disable_compile`
   - honor `EXPLICIT_ONLY`
   - otherwise enqueue compile and remain on VM

## Require-native boundary

When execution policy is `REQUIRE_NATIVE`:

- absence of a valid artifact is an error path
- the runtime must not silently fall back to the VM path as if native were only
  preferred
- compile queuing may still occur when policy permits it, but the current
  dispatch result remains an error rather than implicit interpreted execution

## Artifact verification contract

Artifacts are selected and verified by compatibility key fields:

- `object_uuid`
- `canonical_sblr_hash`
- `target_triple`
- `cpu_feature_profile`
- `native_abi_version`
- `compiler_identity`
- `compiler_version`
- `optimization_profile`
- `security_policy_version`

The current artifact store persists and verifies:

- artifact id
- module id
- plan id
- binary blob id
- optional signature blob id
- native hash
- compatibility key
- state
- created transaction id
- created timestamp

Current artifact states are catalog-backed and include at least:

- `QUEUED`
- `READY`
- `RETIRED`

Only `READY` artifacts are admissible for native dispatch.

## Artifact load or verification failure behavior

The runtime currently treats these reason classes as artifact-load or
verification failures:

- invalid hash
- blob load failure
- hash mismatch
- invalid payload
- invalid signature

When these failures occur, the runtime must:

- record fallback evidence
- distinguish load failure from other fallback classes
- retire the artifact when the verification-failure class requires retirement

This is not optional hygiene. It is part of the current native-dispatch safety
model.

## Compile queue and hotness boundary

The runtime keeps an explicit compile queue and hotness accounting.

The current runtime tracks at least:

- queue capacity
- queued compile count
- duplicate queue count
- saturated queue count
- hotness observations
- hotness promotions
- explicit compile attempts and outcomes
- queued compile attempts and outcomes

Compile queue admission is therefore a first-class operational surface, not an
implicit implementation detail.

## Performance and object-level accounting

Current snapshots include:

- VM dispatch count
- native dispatch count
- error dispatch count
- compile-queue statistics
- compile latency
- native execution CPU usage
- fallback count
- load-failure count
- retired-unusable-artifact count

Object-level performance accounting is keyed by object UUID and includes:

- total dispatch count
- VM dispatch count
- native dispatch count
- error dispatch count
- compile queue counts
- hotness counts
- compile success or failure counts
- fallback count

## LLVM toolchain boundary

The current LLVM toolchain layer exposes:

- provider identity
- provider version
- host target triple
- availability flag
- normalized target-triple derivation

If LLVM JIT support is compiled out:

- toolchain availability is false
- provider version is `unavailable`
- host target triple falls back to `native`

This means the runtime must not overclaim LLVM presence merely because JIT
policy exists.

## Governing correctness rule

The JIT path is an optional acceleration surface over canonical SBLR execution.

It may change:

- dispatch path
- latency
- compiled artifact reuse
- compile queue behavior

It may not change:

- MGA visibility
- correctness of statement semantics
- fail-closed behavior when artifact compatibility cannot be proven
