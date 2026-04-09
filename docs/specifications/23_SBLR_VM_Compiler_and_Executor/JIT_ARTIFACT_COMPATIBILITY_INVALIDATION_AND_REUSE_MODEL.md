Status: reconstructed_required_with_current_substrate

# JIT Artifact Compatibility Invalidation and Reuse Model

## Purpose

This document defines the canonical compatibility rules for storing, reusing, and invalidating LLVM JIT artifacts.

## Canonical Rule

JIT artifacts are reusable only while their full compatibility envelope remains valid. Reuse of an artifact outside that envelope is non-conforming.

## Compatibility Envelope

Each artifact shall record:

- canonical statement or execution-artifact identity
- catalog epoch or bound-object compatibility marker
- optimizer or lowering epoch
- target triple
- CPU feature set
- code-generation policy
- runtime ABI or helper-surface version
- security hardening mode

## Reuse Rule

An artifact may be reused only when every compatibility-envelope field matches the current execution environment or is admitted by an explicit compatibility rule.

## Invalidation Triggers

Artifacts shall be invalidated when any of the following changes:

- bound object definition or catalog epoch
- lowering or optimization shape
- target triple
- CPU feature admission
- runtime helper ABI
- process hardening policy
- JIT runtime compatibility manifest

## Failure Rule

If compatibility cannot be established deterministically, the artifact shall be treated as invalid and the engine shall fall back to a non-JIT path or compile a new artifact if policy allows.

## Reuse Provenance

The artifact store shall preserve enough provenance to explain:

- why an artifact was reused
- why an artifact was invalidated
- whether the artifact was discarded eagerly or lazily
- whether fallback occurred after reuse was attempted

## Store Hygiene

The artifact store may evict old entries for space or pressure reasons, but it shall never silently retag a stale artifact as compatible with a different envelope.

## Non-Guarantees

This file does not require cross-process or cross-host artifact portability. Compatibility is process-local unless a stricter future contract is promoted.
