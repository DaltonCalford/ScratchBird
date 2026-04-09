Status: reconstructed_required_with_current_substrate

# JIT Compilation Failure Classification and Fallback Diagnostic Model

## Purpose

This document defines the canonical classification of JIT compilation failures and the diagnostics emitted when the engine falls back.

## Canonical Rule

JIT failure is an acceleration failure class, not a semantic engine failure class, unless it reveals corruption in canonical execution artifacts. The engine shall classify the failure precisely and fall back safely.

## Failure Classes

The canonical classes are:

- admission refused
- IR generation failed
- IR verification failed
- toolchain or target failure
- object emission failed
- symbol binding failed
- artifact load or reuse failed
- runtime compiled-path failure

## Fallback Rule

Every failure class above shall produce:

- a stable failure class
- a reason code
- a fallback outcome
- the resulting execution path selected

## Escalation Rule

JIT failure escalates to a broader engine failure only when:

- canonical statement artifacts are invalid
- required runtime helpers are corrupted or incompatible in a way that also blocks non-JIT execution
- policy explicitly fences execution because repeated failures indicate unsafe state

## Diagnostics Requirements

The runtime shall preserve:

- artifact identity
- compatibility envelope
- failure class
- reason code
- fallback path
- whether the artifact was invalidated

## Non-Guarantees

This file does not require public exposure of every low-level LLVM message. It requires stable engine-owned failure classes and fallback diagnostics.
