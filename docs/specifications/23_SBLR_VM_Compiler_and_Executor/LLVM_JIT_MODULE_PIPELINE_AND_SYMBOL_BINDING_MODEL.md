Status: reconstructed_required_with_current_substrate

# LLVM JIT Module Pipeline and Symbol Binding Model

## Purpose

This document defines the canonical LLVM-backed JIT compilation and execution pipeline for ScratchBird SBLR execution artifacts. It exists so a limited implementer can build or reconcile the current JIT path without inventing cache keys, symbol scopes, or fallback rules.

## Scope

This file covers:

- admission from verified SBLR or v3 execution artifacts into the LLVM JIT path
- canonical cache-key composition
- IR generation, verification, object emission, and registration
- runtime symbol binding and relocation boundaries
- required fallback to the non-JIT execution path

This file does not define SQL parsing, catalog mutation, or GPU execution.

## Current Substrate

The current implementation substrate is the engine-owned JIT runtime, LLVM toolchain integration, and artifact store under the SBLR runtime tree. This canonical model is reconstructed from those source paths and from the surrounding statement-cache and translation-cache contracts.

## Canonical Admission Rule

LLVM JIT is optional acceleration. The execution engine remains correct without it. No statement may require LLVM JIT for semantic correctness. If the JIT path is unavailable, incompatible, unsafe, or over budget, execution shall fall back to the ordinary interpreter or compiled-runtime path for the same SBLR payload.

## Canonical JIT Pipeline

The engine shall execute the following ordered pipeline:

1. Accept only a verified statement payload and a fully resolved execution context.
2. Build the canonical JIT identity tuple.
3. Check admission gates.
4. Reuse a compatible compiled artifact if one already exists.
5. Otherwise lower the execution artifact to LLVM IR.
6. Verify the IR module.
7. Materialize the target-machine configuration.
8. Emit object code.
9. Resolve required engine-owned symbols.
10. Register the artifact in the local artifact store.
11. Bind the artifact to the current statement execution.
12. Execute the compiled path.
13. Fall back immediately if any step above fails closed.

## Canonical JIT Identity Tuple

The JIT cache key shall include all fields below:

- canonical SBLR or v3 artifact identity
- execution artifact version
- optimizer or rewrite epoch that materially changes generated code shape
- catalog epoch for all bound objects
- function signature set and collation-sensitive comparison context
- target triple
- CPU feature bitmap admitted by the engine
- code generation policy flags
- ABI-relevant runtime version
- security hardening mode

The key shall not depend on user-visible SQL text formatting or dialect spelling once the statement is lowered into canonical execution form.

## Admission Gates

The engine shall refuse JIT admission when any of the following holds:

- LLVM toolchain discovery fails
- the current platform or process policy forbids native code emission
- statement shape is outside the implemented lowering surface
- catalog bindings are unresolved
- memory budget for JIT compilation is exhausted
- operator or function calls require a runtime bridge not present in the current symbol table
- the statement is already marked as a prior JIT failure for the same identity tuple

## IR Generation Rules

The IR generator shall:

- operate only on verified canonical execution artifacts
- lower expression trees, tuple shims, comparison operators, and aggregation helpers using engine-owned calling conventions
- preserve engine-visible nullability and collation behavior
- avoid direct dependence on parser dialect state
- emit only engine-owned symbol references or runtime support shims that are part of the approved symbol registry

## Verification Rule

LLVM IR verification is mandatory. If IR verification fails, the artifact shall be discarded, the failure shall be recorded against the JIT identity tuple, and the statement shall continue on the non-JIT path.

## Symbol Binding Rules

The runtime symbol registry shall be engine-owned and explicit. The JIT layer may bind only to:

- engine runtime helpers
- tuple access helpers
- aggregation helpers
- comparison and collation helpers
- memory management helpers approved for generated code
- deterministic math and scalar helpers explicitly admitted by the engine

The JIT layer shall not bind directly to parser-owned code, protocol-owned code, storage backdoors, or ad hoc global process symbols.

## Relocation and Registration Rules

Object emission and relocation shall be process-local. A compiled artifact becomes reusable only after:

- object emission succeeds
- required symbols are fully resolved
- relocation succeeds
- the artifact store records the identity tuple and compatibility metadata

Partially emitted or partially relocated artifacts shall be treated as invalid and non-reusable.

## Fallback Rules

The runtime shall fall back to the ordinary execution path when:

- admission is refused
- compilation fails
- verification fails
- relocation fails
- symbol binding fails
- artifact loading fails
- runtime execution of the compiled path signals a fail-closed incompatibility

Fallback preserves semantic equivalence. It does not change transaction, lock, or visibility behavior.

## Cache and Invalidation Rules

Compiled artifacts shall be invalidated when any cache-key field changes, including:

- catalog epoch changes for bound objects
- runtime ABI changes
- target-feature set changes
- code generation policy changes
- statement-shape or optimizer epoch changes

Invalidation may discard an artifact eagerly or lazily, but stale artifacts shall never execute after incompatibility is known.

## Diagnostics Requirements

The runtime shall emit deterministic diagnostics for:

- admission refused
- compilation failed
- verification failed
- symbol binding failed
- relocation failed
- artifact cache hit
- artifact cache miss
- artifact invalidated
- runtime fallback after prior artifact reuse

## Non-Guarantees

The following are explicitly out of scope unless separately promoted:

- cluster-wide artifact sharing
- cross-version binary compatibility of emitted objects
- GPU code generation through the LLVM JIT path
- dialect-specific code generation shortcuts outside canonical SBLR or v3 execution artifacts
