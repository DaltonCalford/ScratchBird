Status: current_authority

# JIT Semantic Equivalence, Scope, and Tiering Model

## Purpose

This file defines the semantic-equivalence rule, scope-admission rule, and
hotness-tiering rule for the SBLR JIT runtime.

## Governing rule

Native execution is an optimization path only.

Observable execution semantics shall match the VM path for the same:

- canonical SBLR
- visibility context
- compatibility tuple
- policy envelope

## Current semantic-equivalence authority

The current differential corpus proves VM/native equivalence for a bounded
corpus including:

- simple arithmetic addition
- `ABS`
- division
- multiplication

The current test requires equality of:

1. success state
2. result-set presence
3. row count
4. column count
5. rendered result value

## Scope-admission rule

Current code-backed scope rules are:

1. unknown routine surfaces never enter native selection
2. unknown routine surfaces fall back to VM with deterministic reason `NATIVE_SCOPE_NOT_ELIGIBLE`
3. explicit compile rejects non-routine or unsupported surfaces with `INVALID_ARGUMENT`

This means native selection is opt-in by eligible routine surface, not a blanket
runtime feature.

## Hotness-tiering model

Current code-backed hotness behavior is:

1. requests below the hotness threshold stay on the VM path with reason `HOTNESS_BELOW_THRESHOLD`
2. when the hotness threshold is reached, the runtime may:
   - keep current execution on the VM path
   - queue a compile
   - mark the request as promoted by hotness

Hotness promotion is therefore asynchronous and correctness-preserving.

## Queue saturation model

Current code-backed queue behavior is:

1. compile queue capacity is bounded
2. queue saturation keeps execution on the VM path
3. queue saturation reports deterministic reason `QUEUE_SATURATED`
4. queue saturation does not fabricate native execution

## Duplicate suppression model

Current code-backed duplicate behavior is:

1. once a hot object is already queued, a duplicate request does not enqueue another compile
2. duplicate requests remain on the VM path
3. duplicate requests report deterministic reason `COMPILE_ALREADY_QUEUED`
4. runtime performance snapshots and object-local stats record duplicate queue events

## Unsupported-opcode rule

Current code-backed explicit compile behavior rejects unsupported opcode families
with `NOT_SUPPORTED`.

This is a hard refusal, not a silent lowering to a weak native approximation.

## Runtime truth rule

The runtime truth order is:

1. canonical SBLR and VM semantics
2. compatibility and trust-verified native artifact
3. native dispatch when eligible and admitted

Native execution shall not introduce distinct user-visible behavior merely
because a routine is hot.

## Reconstructed required expansion

The rebuild requires future equivalence coverage for:

1. more opcode families
2. control flow
3. routine parameters and locals
4. package members
5. triggers
6. procedures and functions

## Fail-closed rules

The JIT runtime shall not:

1. native-compile unknown surfaces
2. native-compile unsupported opcode families
3. execute a semantically different native result when VM equivalence is not proven
4. over-enqueue duplicate hot requests for the same object
