# Container, Cgroup, and Resource Pressure Model

Status: current_authority_with_reconstructed_expansion
Section: 33_Memory_Management

## Purpose

Define ScratchBird behavior under container and cgroup resource limits so cloud behavior is explicit rather than inferred.

## Governing rule

Cloud support requires container-aware resource behavior. ScratchBird must not assume bare-metal resource availability when explicit cgroup or container limits are present.

## Resource identity inputs

The runtime must distinguish between:

- host physical capacity
- container or cgroup memory limit
- container or cgroup CPU quota and shares
- local temp and spill storage limits
- accelerator availability and accelerator memory limits

Admission, background maintenance, and pressure handling must use the bounded environment seen by the deployment, not the raw host total, when container limits are authoritative.

## Effective envelope derivation rule

The effective resource envelope shall be derived in this order:
1. detect the authoritative environment ceiling:
   - container or cgroup limit when present and authoritative
   - otherwise host physical capacity
2. derive default subsystem budgets and worker counts from that ceiling
3. apply explicit operator configuration overrides
4. clamp configured budgets and worker limits to the authoritative ceiling
5. publish the resulting effective envelope to admission, scheduling,
   maintenance, and pressure-control surfaces

An override may narrow the environment-derived default. It may not expand the
effective envelope beyond the authoritative environment ceiling.

## Current code-backed authority

Current Beta 1 code-backed authority now proves:
- database open records configured memory request bytes, detected environment
  ceiling bytes, effective memory budget bytes, and clamp state for the buffer
  pool
- explicit buffer-pool overrides are clamped to the authoritative environment
  ceiling instead of silently expanding beyond it
- environment-bounded ceilings fail closed when smaller than one page budget
- synthetic test ceilings may be injected for contract coverage, but they do
  not alter the canonical precedence rule

## Pressure states

1. Normal
- current usage and committed work fit within declared envelopes

2. Constrained
- new work remains admissible, but background work, caches, or prefetch must be reduced

3. Degraded
- selected non-critical work is throttled or refused
- new sessions or memory-heavy plans may be denied based on policy

4. Admission-limited
- only bounded safe work may begin
- new heavy queries, large sorts, or accelerator allocations may be refused

5. Spill-forced
- memory-resident working sets must spill where allowed by their owning sections
- if spill is not legal for the operation, the operation must fail closed rather than violate the envelope

6. Quarantine or fail-closed
- safety and correctness take precedence over forward progress when limits are incompatible with current durable operation

## Required operator-visible behavior

The runtime must expose:

- detected memory envelope
- detected CPU envelope
- effective memory and CPU budget after configuration clamps
- effective temp or spill envelope
- active pressure class
- admission throttling state
- forced spill state
- current refusal reason when new work is denied for resource reasons

## Beta 1 requirements

Beta 1 cloud support requires:

- explicit cgroup-aware memory budgeting
- explicit CPU quota awareness for worker scheduling and admission
- stable degraded-state reporting under pressure
- no silent escalation from constrained state to uncontrolled OOM behavior

## Beta 2 extension

Beta 2 may extend this model with cluster-aware resource placement and fairness, but Beta 1 must remain correct as a single-node bounded deployment.

## Audit lookup anchors

Representative audit anchors for this file are:
- `detectAuthoritativeMemoryCeilingBytes(`
- `loadBufferPoolConfig(`
