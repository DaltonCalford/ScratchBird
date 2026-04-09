Status: reconstructed_required_with_current_substrate

# LLVM Toolchain Discovery Compatibility and Admission Model

## Purpose

This document defines how ScratchBird discovers, validates, and admits the optional LLVM toolchain used by the engine-owned JIT runtime.

## Canonical Rule

LLVM is an optional acceleration dependency, not a correctness dependency. The engine shall remain fully functional when LLVM is missing, incompatible, policy-disabled, or platform-disabled.

## Discovery Inputs

Toolchain discovery shall use engine-owned configuration and platform probes only. Canonical discovery inputs are:

- build-time feature enablement
- runtime configuration gates
- shared-library or packaged-toolchain availability
- CPU architecture and feature surface
- process hardening policy
- platform support policy

## Discovery Order

The engine shall evaluate LLVM availability in this order:

1. Confirm the build contains the optional LLVM integration surface.
2. Confirm runtime configuration allows JIT use.
3. Probe the packaged or linked LLVM toolchain components required by the runtime.
4. Confirm the current process architecture and feature set are compatible.
5. Confirm writable and executable code-emission policy is allowed by the process sandbox and operating system.
6. Publish an admitted, refused, or unavailable result.

## Compatibility Manifest

The admitted toolchain shall be checked against an engine-owned compatibility manifest that includes:

- expected LLVM API or ABI family
- supported target triples
- supported relocation model
- supported code model
- supported object format
- required runtime helper surface

If the discovered toolchain does not match the compatibility manifest, the toolchain shall be refused.

## Admission States

The platform layer shall publish one of the following states:

- `DISABLED_BY_POLICY`
- `NOT_BUILT`
- `NOT_FOUND`
- `INCOMPATIBLE`
- `ADMITTED`

Only `ADMITTED` permits JIT compilation attempts.

## Fail-Closed Rules

The platform layer shall refuse admission when any of the following holds:

- expected components are missing
- runtime symbol registration cannot be completed
- target triple cannot be derived deterministically
- memory-execute policy conflicts with engine hardening rules
- the discovered LLVM family is outside the approved compatibility set

Refusal does not degrade query correctness. It only disables the JIT acceleration path.

## Publication Rules

The platform layer shall expose the following diagnostics to the engine and operator surfaces:

- discovery state
- admitted target triple
- admitted CPU feature bitmap
- compatibility-manifest identifier
- refusal reason code
- timestamp or startup epoch of the last discovery attempt

## Lifecycle Rules

Toolchain discovery may occur at startup or first JIT demand, but the result shall be cached until a lifecycle event requires refresh. Refresh triggers include:

- process restart
- configuration reload affecting JIT policy
- toolchain path change
- compatibility-manifest change

## Non-Guarantees

This file does not require:

- hot replacement of the LLVM toolchain inside a running process
- cluster-wide uniform LLVM availability
- GPU offload through the LLVM runtime
- acceptance of user-supplied arbitrary LLVM passes or plugins
