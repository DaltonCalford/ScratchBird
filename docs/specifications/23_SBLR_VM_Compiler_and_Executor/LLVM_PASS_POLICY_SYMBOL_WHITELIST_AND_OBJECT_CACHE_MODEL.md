Status: reconstructed_required

# LLVM Pass Policy Symbol Whitelist and Object Cache Model

## Purpose

This document defines the canonical pass-policy, symbol-whitelist, and object-cache boundary for LLVM-backed JIT.

## Canonical Rule

The engine owns the admitted LLVM pass policy and symbol whitelist. User-supplied or arbitrary pass chains and unrestricted symbol exposure are non-conforming.

## Pass Policy

The canonical pass policy shall preserve:

- enabled pass set
- disabled pass set
- optimization level
- target-compatibility constraints
- safety or hardening constraints

## Symbol Whitelist

The canonical symbol whitelist shall preserve:

- helper symbol identity
- helper function class
- ABI or calling-convention compatibility
- security or policy admission

Only whitelisted runtime helpers may be exposed to emitted object code.

## Object Cache Rule

The object cache may store emitted code objects only when:

- pass policy matches the artifact envelope
- symbol whitelist generation matches
- target and ABI compatibility match

## Invalidation Rule

Change to pass policy or symbol whitelist invalidates affected cached objects unless an explicit compatibility rule says otherwise.

## Non-Guarantees

This file does not require one fixed optimization level forever. It requires engine-owned explicit pass and symbol policy.
