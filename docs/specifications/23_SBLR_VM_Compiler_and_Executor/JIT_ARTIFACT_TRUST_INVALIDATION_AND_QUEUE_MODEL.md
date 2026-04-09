Status: current_authority_with_reconstructed_expansion

# JIT Artifact Trust, Invalidation, and Queue Model

## Purpose

This file defines how persisted native artifacts are trusted, invalidated,
retired, and rebuilt. It prevents a limited implementer from treating JIT
artifacts as opaque cache blobs with best-effort semantics.

## Current code-backed authority

The current runtime proves the following:

1. Artifact trust can require a signature.
2. When signature-required verification is active, an unsigned artifact is rejected with a deterministic signature-invalid reason.
3. Artifact hash mismatch causes the runtime to:
   - reject native use
   - retire the unusable artifact
   - queue a rebuild when policy allows compile
   - continue on the VM path for the current dispatch
4. Dependency signature changes retire artifacts bound to the affected object.
5. Security policy version changes retire artifacts bound to the affected object.
6. Queue stress preserves VM correctness rather than forcing unsafe native dispatch.
7. Target- or provider-level compile faults map deterministically to compile failure behavior.

## Trust verification order

Artifact admission shall occur in this order:

1. compatibility tuple match
2. signature validation when required
3. artifact hash validation
4. blob load
5. dispatch-path selection

The runtime shall not load or execute a blob first and validate later.

## Artifact retirement rules

An artifact shall be retired immediately when any of the following hold:

1. signature invalid
2. hash mismatch
3. dependency signature changed
4. security policy version changed
5. compatibility tuple no longer matches the request envelope

Retirement means:

1. remove the artifact from the usable set
2. prevent native dispatch using that artifact
3. increment retirement and fallback telemetry
4. queue a rebuild when compile policy allows it

## Rebuild queue rules

The compile queue is a bounded derivative work queue.

The queue contract is:

1. queueing a rebuild never changes current-statement correctness
2. current dispatch must remain correct on the VM path
3. queue exhaustion may suppress rebuild scheduling, but shall not fabricate native success
4. queued compile success shall publish new artifact availability only after verification completes

## VM-correctness rule

The VM path remains the safe correctness path whenever:

1. artifact trust fails
2. artifact compatibility fails
3. artifact load fails
4. compile is disabled
5. compile is queued but not yet complete

Queue stress or high hotness shall not weaken this rule.

## Required performance snapshot fields

The runtime performance snapshot shall expose at least:

- explicit compile attempt count
- explicit compile success count
- explicit compile failure count
- total compile latency
- native dispatch count
- VM dispatch count
- fallback count
- retired unusable artifact count
- compile queue enqueued count
- queued compile success count

Per-object performance rows shall expose at least:

- compile success count
- total dispatch count
- native dispatch count
- VM dispatch count

## Reconstructed required expansion

The rebuild requires the following additional rules even where current code is
not fully closed:

1. artifact quarantine rows shall distinguish:
   - signature failure
   - hash failure
   - compatibility mismatch
   - dependency invalidation
   - security invalidation
2. queue prioritization shall be deterministic and bounded by:
   - hotness
   - object policy
   - queue capacity
3. rebuild admission shall remain subordinate to the same strictest-wins compile policy used for direct compile requests

## Non-authority boundaries

The following are not current authority:

1. trusting stale artifacts because they are "close enough"
2. partial reuse after signature or hash failure
3. executing an artifact during invalidation and retiring it later
4. treating queue backlog as a reason to skip trust checks
