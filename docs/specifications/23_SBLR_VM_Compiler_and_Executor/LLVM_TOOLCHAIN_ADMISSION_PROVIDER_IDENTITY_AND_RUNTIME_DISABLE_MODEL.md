Status: current_authority_with_reconstructed_expansion

# LLVM TOOLCHAIN ADMISSION PROVIDER IDENTITY AND RUNTIME DISABLE MODEL

## Purpose

This file defines the canonical ScratchBird contract for:

- LLVM toolchain availability
- provider identity
- target-triple normalization
- runtime-disable and fallback behavior
- interaction with JIT queue, policy merge, and artifact admission

## Governing rule

LLVM-native execution is optional runtime acceleration.

It is not an authority override over:

- canonical SBLR
- interpreter correctness
- canonical artifact compatibility rules
- fail-closed security policy

## Build-time availability rule

Current recovered code authority proves:

- LLVM availability is decided first by build-time macros
- provider identity is exposed from the compiled build configuration
- host target triple is normalized when LLVM is compiled in
- fallback target triple is `native`

This means the runtime must distinguish:

1. built without LLVM support
2. built with LLVM support but not currently admitted for the statement
3. built with LLVM support and admitted for native execution

## Provider identity contract

The native provider identity is part of compatibility truth.

It must remain visible to:

- artifact compatibility keys
- runtime diagnostics
- policy-based admission and rejection

The provider is not anonymous. A native artifact built for one provider identity
must not be silently treated as valid for another.

## Target-triple contract

The target-triple rules are:

1. use normalized host target triple when LLVM support is present
2. preserve the target triple in compatibility identity
3. use `native` only as the canonical fallback target-triple token when a more
   specific LLVM-host triple is not available through the compiled build

The runtime must not silently erase target-triple differences during artifact
admission.

## Queue and policy interaction

Current recovered queue and policy facts are:

- queue capacity default is `128`
- queue ordering is FIFO
- dedupe is by `dedupe_key`
- duplicate enqueue returns `COMPILE_ALREADY_QUEUED`
- saturated enqueue returns `QUEUE_SATURATED`
- capacity shrink drops tail entries and clears their dedupe keys

Current recovered policy-merge facts are:

- compile-mode merge is restrictive
- any `EXPLICIT_ONLY` request wins the merge
- execution-policy precedence is:
  - `INTERPRETED_ONLY`
  - `REQUIRE_NATIVE`
  - `PREFER_NATIVE`

The LLVM toolchain contract must therefore behave as follows:

- `INTERPRETED_ONLY`: native provider must not be used
- `PREFER_NATIVE`: LLVM may be used only if toolchain, artifact, and policy
  admission all succeed
- `REQUIRE_NATIVE`: failure to admit LLVM-native execution is a hard execution
  failure, not a silent interpretation fallback

## Artifact-admission interaction

Current recovered artifact compatibility identity includes:

- object UUID
- canonical SBLR hash
- target triple
- CPU profile
- ABI version
- compiler identity
- compiler version
- optimization profile
- security policy version

Current recovered artifact-admission facts also include:

- non-ready artifacts are rejected
- retired artifacts are rejected
- malformed native hash is rejected
- optional signatures become mandatory when runtime policy enables signature
  requirement

LLVM toolchain admission is therefore subordinate to artifact admission.

A compiled LLVM toolchain does not authorize execution of:

- mismatched artifacts
- unsigned artifacts when signature policy requires signatures
- retired artifacts
- malformed artifacts

## Runtime-disable states

The canonical runtime-disable states are:

1. `BUILD_UNAVAILABLE`
2. `PROVIDER_UNAVAILABLE`
3. `POLICY_DISABLED`
4. `QUEUE_REJECTED`
5. `ARTIFACT_INCOMPATIBLE`
6. `ARTIFACT_RETIRED_OR_NOT_READY`
7. `SIGNATURE_POLICY_REJECTED`

At minimum these states must be visible in diagnostics even when public
telemetry groups them into higher-level classes.

## Fallback rule

Interpretation fallback is allowed only when:

- policy does not require native execution
- LLVM provider or artifact admission fails in a non-fatal way
- correctness-equivalent interpreter execution remains available

Interpretation fallback is not allowed when native execution is required by the
effective execution policy.

## Canonical decision order

LLVM-native admission must follow this order:

1. determine whether the build includes LLVM support
2. determine provider identity and normalized target triple
3. merge compile and execution policies
4. if policy forbids native, stop and stay interpreted
5. if policy allows or requires native, evaluate queue admission when compile is
   needed
6. evaluate artifact compatibility and readiness
7. evaluate signature policy if enabled
8. either admit native execution, fall back to interpreter, or fail closed

## Diagnostics requirement

The engine must preserve enough diagnostic detail to explain:

- why a native compile was not queued
- why a queued compile was rejected
- why an existing artifact was not admitted
- whether fallback to interpreter occurred
- whether fallback was forbidden by effective policy

## Non-authority rules

The following are incorrect:

- assuming LLVM availability from runtime desire alone when the build omitted it
- treating provider identity as cosmetic
- ignoring target-triple mismatch during artifact admission
- silently interpreting when policy requires native execution
- treating queue saturation or duplicate queue state as proof that native
  execution is already admissible
