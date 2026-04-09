Status: current_authority_with_reconstructed_expansion

# LLVM JIT Provider Discovery, Compatibility, and Fallback Model

## Purpose

This file defines the platform and runtime contract for the optional LLVM-backed
SBLR JIT provider. It fixes provider discovery, artifact compatibility, target
matching, and fallback behavior so a limited implementer does not guess how JIT
selection works.

## Current code-backed authority

The current implementation proves the following:

1. LLVM JIT is optional at build time.
2. Provider discovery is exposed through toolchain metadata:
   - `available`
   - `provider_identity`
   - `provider_version`
   - `host_target_triple`
3. The shipped provider identity is `llvm`.
4. The host target triple is normalized before use.
5. When the provider is unavailable in the current build:
   - explicit compile attempts fail with backend-unavailable semantics
   - runtime path selection must fall back to the VM path unless policy requires native
6. Compile requests are admitted only when:
   - lowered IR is non-empty
   - target triple normalizes to a known architecture
   - request compiler identity matches provider identity
   - request compiler version matches provider version
7. The current LLVM provider emits a deterministic native artifact blob containing:
   - provider identity
   - provider version
   - normalized target triple
   - native ABI version
   - optimization profile
   - lowered IR payload
8. Cross-target artifact reuse is not allowed.
9. Missing or unloadable native blobs do not silently succeed. Runtime selection must fall back to VM with a deterministic reason code, or fail if policy requires native.
10. The current shipped closure is persisted artifact generation and selection. Direct lowering to an immediately callable native code path is not yet current authority.

## Provider identity contract

The runtime shall treat provider identity as a strict compatibility key, not as
an informational label.

The provider identity tuple is:

- `provider_identity`
- `provider_version`
- `target_triple`
- `native_abi_version`
- `optimization_profile`
- `cpu_feature_profile`
- `security_policy_version`

An artifact compiled under one tuple shall not be executed under a different
tuple.

## Discovery algorithm

The provider discovery algorithm is:

1. Query the toolchain metadata surface.
2. If `available = false`, publish provider-unavailable state.
3. Normalize the host target triple.
4. Bind the normalized triple to all explicit compile and artifact-selection requests.
5. Publish provider identity and version as the only compiler identity allowed for this provider.

The engine shall not fabricate alternate LLVM provider identities.

## Compile admission algorithm

For an LLVM compile request:

1. Refuse the request if the provider is unavailable.
2. Refuse the request if lowered IR is empty.
3. Normalize the request target triple.
4. Refuse the request if the normalized target triple resolves to an unknown architecture.
5. Refuse the request if request compiler identity does not equal `llvm`.
6. Refuse the request if request compiler version does not equal the live toolchain version.
7. Emit a deterministic artifact blob.
8. Persist the compatibility tuple with the artifact.

The compile path shall not downgrade mismatched metadata into "best effort"
execution.

## Runtime path selection algorithm

The runtime selector shall evaluate in this order:

1. Resolve effective policy from database, session, and object scopes.
2. Apply the strictest compile control.
3. Apply the strictest execution control.
4. Apply explicit disable hints:
   - disable compile
   - disable execute
5. Search for a matching persisted artifact.
6. Refuse cross-target or cross-provider artifact reuse.
7. If the artifact is missing or unloadable:
   - use VM when execution policy allows fallback
   - return deterministic error when execution policy requires native
8. If the artifact key matches and the blob loads successfully, choose the native path.

## Policy resolution rules

Policy resolution is strictest-wins:

1. `INTERPRETED_ONLY` overrides `PREFER_NATIVE` and `REQUIRE_NATIVE`.
2. `EXPLICIT_ONLY` overrides `JIT_ALLOWED`.
3. `REQUIRE_NATIVE` is legal only when no stronger interpreted-only scope is active.
4. `disable_compile` forbids queue promotion and background compile.
5. `disable_execute` forces the VM path even when a native artifact exists.

## Deterministic reason-code requirements

The implementation shall preserve stable reason-code classes for:

- backend unavailable
- compile failed
- artifact blob load failed
- target triple mismatch
- compiler identity mismatch
- compiler version mismatch
- disabled by hint
- native required but unavailable

These codes are operator-facing and test-facing contracts, not debug-only text.

## Reconstructed required expansion

The rebuild requires the following even where current code is not yet fully
closed:

1. Provider capability rows shall eventually expose:
   - supported triples
   - supported CPU feature profiles
   - native ABI families
   - deterministic artifact format version
2. Provider selection shall remain local and explicit. There is no "pick any available native backend" rule.
3. Any future alternate native backend shall use a different provider identity and separate artifact namespace.
4. GPU-assisted code generation, if introduced later, shall remain subordinate to this compatibility discipline and shall not weaken tuple matching.

## Non-authority and fail-closed boundaries

The following are not current authority:

1. universal native execution across heterogeneous machines
2. opaque best-effort retargeting of stored artifacts
3. provider-version compatibility ranges
4. automatic upgrade-in-place of stored native artifacts
5. direct claim that LLVM artifacts are the truth of execution semantics

SBLR bytecode and MGA-visible database state remain the truth. Native artifacts
are cacheable derivative execution aids.
