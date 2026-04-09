# LLVM and Accelerator Toolchain Runtime Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines how ScratchBird discovers, normalizes, verifies, and uses optional LLVM-native and accelerator providers while preserving deterministic MGA-safe fallback when those providers are absent, incompatible, or policy-disabled.

## Current code-backed LLVM authority

Current LLVM runtime authority includes:

- toolchain discovery through `LlvmToolchainInfo`
- provider identity and provider version publication
- host target-triple publication and normalization
- backend selection between `llvm` and `null`
- deterministic null-backend fallback to VM execution
- deterministic bitcode artifact emission by the LLVM backend
- runtime artifact-envelope verification

## Current toolchain info contract

`LlvmToolchainInfo` currently publishes:

- `available`
- `provider_identity`
- `provider_version`
- `host_target_triple`

## Current build-state behavior

### LLVM-enabled build

When the build is compiled with LLVM support:

- toolchain is `available = true`
- provider identity comes from the LLVM provider build macro
- provider version comes from the LLVM provider build macro
- host target triple is normalized from the LLVM host triple macro

### LLVM-disabled build

When the build is compiled without LLVM support:

- toolchain is `available = false`
- provider identity defaults to `llvm`
- provider version is `unavailable`
- host target triple defaults to `native`

## Target-triple normalization rule

Current code normalizes the requested target triple as follows:

1. use the requested target when non-empty
2. otherwise use the host target triple
3. normalize through LLVM triple normalization when LLVM is present
4. if still empty, fall back to host target triple
5. if still empty, fall back to `native`

## Current backend model

Current backend names are:

- `llvm`
- `null`

The null backend is not an error in itself. It is the deterministic fallback backend that forces VM execution and prevents accidental native overclaim.

## Current LLVM provider admission rules

The LLVM backend currently refuses compilation when:

- toolchain is unavailable
- lowered IR is empty
- target triple is fault-injection rejected
- normalized triple has unknown architecture
- compile request compiler identity does not match the active provider
- compile request compiler version does not match the active provider

## Current emitted artifact envelope

The LLVM backend currently stamps at least these metadata globals into the artifact envelope:

- `__sb_provider_identity`
- `__sb_provider_version`
- `__sb_target_triple`
- `__sb_native_abi`
- `__sb_opt_profile`
- `__sb_lowered_ir`

## Required accelerator authority

Accelerator providers, including GPU-backed ANN providers, follow the same discipline:

- capability probe at startup or first use
- provider identity publication
- admission only when platform and policy allow
- explicit fallback when unavailable
- no authority to redefine MGA truth, visibility, or durability

## Accelerator resource model

Accelerator admission must also evaluate resource state, not just binary availability.

Required resource inputs:

- device count
- device class
- driver/runtime compatibility
- provider version
- supported metric families or opcode families
- available memory budget or VRAM budget
- queue capacity and batch-capacity limits
- operator policy allowance

Required rule:

- a provider that exists but cannot satisfy the resource policy is not `ready`

## Provider state vocabulary

The canonical provider states are:

- `ready`
- `disabled_by_policy`
- `not_installed`
- `version_mismatch`
- `target_mismatch`
- `abi_mismatch`
- `runtime_fault`
- `resource_exhausted`
- `degraded`
- `non_conforming`

## Admission order

A conforming provider admission procedure is:

1. confirm build-time provider presence
2. confirm provider identity and version compatibility
3. confirm target or metric-family compatibility
4. confirm ABI compatibility
5. confirm resource budget availability
6. confirm operator policy permits activation
7. publish provider-ready state only after all checks pass

## Non-negotiable fallback rule

If LLVM or an accelerator provider cannot be admitted, the engine must continue in a supported slower mode rather than silently degrading correctness.

Examples:

- LLVM unavailable: VM or interpreted execution remains authoritative
- GPU unavailable: CPU-resident ANN execution remains authoritative
- GPU resource exhaustion: CPU path remains authoritative or the request fails closed if explicit accelerator use was required

## Non-authority and rejection rules

The following claims are incorrect:

- provider installation alone implies runtime readiness
- accelerator availability may redefine transaction, visibility, or durability truth
- resource-exhausted accelerator providers may still advertise the same latency envelope as ready providers
