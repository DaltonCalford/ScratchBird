# LLVM Toolchain Detection and Target Triple Normalization

Status: current_authority

## Purpose

Define how the runtime detects LLVM JIT availability, reports provider identity, and normalizes requested target triples.

## Current Toolchain Information Model

Current toolchain information carries:
- `available`
- `provider_identity`
- `provider_version`
- `host_target_triple`

## Detection Rules

When LLVM JIT is compiled in:
- toolchain availability is `true`
- provider identity comes from the compiled provider macro
- provider version comes from the compiled provider version macro
- host target triple is normalized from the compiled host triple macro

When LLVM JIT is not compiled in:
- toolchain availability is `false`
- provider identity defaults to `llvm`
- provider version is `unavailable`
- host target triple defaults to `native`

If host target triple resolves empty, it must fall back to `native`.

## Target Triple Normalization

`normalizeLlvmTargetTriple` must:
1. start from the requested triple
2. if the request is empty, substitute the host target triple
3. when LLVM is available, normalize the triple through LLVM triple normalization
4. if normalization still yields empty, fall back to host target triple
5. if host target triple is empty, fall back to `native`

## Canonical Rules

1. Toolchain detection is platform and build dependent, not a universal runtime constant.
2. A missing LLVM toolchain must remain explicit in runtime status and policy decisions.
3. Target triple normalization is deterministic and must not depend on parser dialect or client choice.

## Fail-Closed Rules

1. The runtime must not advertise compiled LLVM availability when the compiled feature macro is absent.
2. Empty target triples must never escape normalization.
3. Toolchain absence must not silently masquerade as a successful native backend availability claim.

## Cross-Section References

- `23_SBLR_VM_Compiler_and_Executor/JIT_RUNTIME_POLICY_QUEUE_AND_HOTNESS_MODEL.md`
- `23_SBLR_VM_Compiler_and_Executor/JIT_ARTIFACT_COMPATIBILITY_AND_SIGNATURE_ADMISSION_MODEL.md`
