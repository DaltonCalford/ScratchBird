# JIT Policy, Queue, Hotness, and Unusable-Artifact Recompile Model

## Status

Current code-backed authority with reconstructed commercial-grade detail.

## Purpose

This document defines how the JIT runtime resolves layered policy, chooses interpreted versus native execution, queues compilation work, and reacts to unusable native artifacts.

## Layered Policy Envelope

JIT policy is layered from:

- database-level compile mode and execution policy
- session-level compile mode and execution policy
- object-level compile mode and execution policy
- hint flags

The effective policy is produced by restricting compile permissiveness downward and merging execution policy through the same layered precedence model.

This means object and session scope may tighten behavior relative to database defaults, but must not silently broaden a stricter upper-level restriction.

## Effective Execution Outcomes

The runtime may resolve to:

- interpreted-only behavior
- native-allowed behavior
- native-required behavior

If the effective execution policy is interpreted-only, the runtime must refuse native selection immediately and record the reason accordingly.

If native is required and no valid artifact exists, the runtime must fail closed rather than silently running the interpreted path.

## Hint Semantics

Hints may further constrain behavior, including current lanes such as:

- disable execute
- prefer VM
- disable compile

Hints are subordinate to the layered policy system and may only make behavior stricter, never less strict.

## Canonical Request Shape

JIT runtime requests are keyed by:

- object UUID
- module id
- plan id
- compatibility key
- layered policy envelope

This ensures compile and execution decisions are object- and plan-aware rather than generic.

## Dispatch Decision Sequence

The current dispatch path must proceed in this order:

1. resolve effective policy
2. if interpreted-only, select VM path immediately
3. attempt verified artifact fetch
4. if a valid artifact exists, select native path
5. if a prior artifact exists but is unusable:
   - record fallback
   - optionally retire the unusable artifact
6. if native is required and no valid artifact exists, fail closed
7. if compilation is admissible, consider queueing a compile request
8. return interpreted path while background compile is pending unless policy requires otherwise

This preserves deterministic native fallback behavior.

## Hotness and Compile Admission

Compilation is not purely eager.

Current reason codes prove the runtime distinguishes:

- hotness below threshold
- explicit-only compile mode
- compile already queued
- queue saturated

The canonical meaning is:

- compile admission may be denied for cold objects
- compile admission may be denied by policy
- compile admission may be deferred when the queue already contains equivalent work

## Compile Queue Model

Queued compile work is represented by `JitQueueEntry` with:

- queue id
- object UUID
- module id
- plan id
- compile request
- dedupe key
- priority

The queue is bounded by capacity.

It must:

- reject new entries when saturated
- deduplicate by pending key
- return stable reason codes for duplicate or saturated work

This prevents uncontrolled compile storms.

## Lowering and Backend Model

Compilation proceeds through:

1. canonical SBLR legality check
2. lowering into a lowered routine containing:
   - canonical SBLR
   - lowered IR
   - side-effect-order preservation flag
3. backend compilation

The current backend model includes:

- LLVM backend
- null backend for deterministic VM fallback behavior

Unsupported opcode families must fail before backend compilation.

## LLVM Backend Contract

The LLVM backend must:

- reject unavailable backend builds
- reject unknown target triples
- reject compiler metadata mismatches
- emit deterministic bitcode artifact payloads

The backend does not get to weaken the compatibility key. It compiles only for the exact requested provider and target context.

## Unusable Artifact Handling

When a stored artifact exists but cannot be used because of load, hash, signature, or payload verification failure:

- the runtime must record fallback
- the runtime may distinguish hard load failure from softer mismatch cases
- the artifact may be retired as unusable
- recompilation may be queued when policy permits

This is the canonical repair path for stale or corrupted native artifacts.

## Recompile Queueing After Retirement

If an unusable artifact is retired and compile policy allows recompilation:

- the runtime should queue a new compile request for the same object and compatibility context
- the response detail should indicate that fallback occurred and recompilation was queued

This gives the engine a deterministic self-healing path without pretending the stale artifact was usable.

## Performance Counters

Current runtime performance tracking includes at least:

- fallback count
- retired unusable artifact count
- per-object fallback tracking

These counters are authoritative operator evidence for JIT instability, not debug-only metadata.

## MGA and Correctness Rule

Native execution is an optimization layer only.

JIT queueing, fallback, recompilation, or native refusal must never compromise:

- MGA visibility
- transaction correctness
- ordered side effects
- policy-version correctness

The VM path remains the correctness fallback whenever native execution cannot be admitted safely.
