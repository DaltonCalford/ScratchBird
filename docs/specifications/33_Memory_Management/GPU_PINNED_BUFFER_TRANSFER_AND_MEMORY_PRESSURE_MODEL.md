Status: reconstructed_required

# GPU Pinned Buffer Transfer and Memory Pressure Model

## Purpose

This document defines how ScratchBird accounts for pinned host buffers, device transfers, and memory pressure when optional GPU acceleration is used.

## Canonical Rule

Pinned host buffers and device-transfer staging memory are first-class managed memory classes. They shall not bypass the ordinary memory-accounting and pressure-escalation framework.

## Managed Classes

The memory subsystem shall classify at least:

- pinned host staging buffers
- device-resident execution buffers
- upload or download queues
- reusable transfer arenas
- fallback re-materialization buffers

## Admission Rule

GPU work may be admitted only if:

- pinned host budget is available
- device-memory budget is available
- the transfer footprint fits the current pressure policy
- fallback to CPU remains possible without violating safety budgets

## Pressure Rule

Under memory pressure, the runtime shall prefer:

1. refusing new GPU admissions
2. draining or shrinking reusable transfer pools
3. demoting resident accelerator state
4. falling back to CPU execution

It shall not retain accelerator buffers while starving core transactional or durability-critical memory.

## Transfer Lifetime Rule

Pinned transfer buffers shall belong to explicit operator, statement, or subsystem contexts. They shall be reclaimable when that owner retires.

## Metrics Requirements

The runtime shall publish:

- pinned host bytes
- device buffer bytes
- transfer queue depth
- accelerator admission refusals due to memory
- fallback count due to transfer pressure

## Resident Index Interaction

If a resident vector index can also use accelerator support, the runtime shall account separately for:

- host-resident authoritative in-memory index state
- device-side acceleration caches or projections

Loss of the device-side projection shall not discard the host-resident authoritative state.

## Non-Guarantees

This file does not require unified memory or one specific accelerator runtime. It requires accounting, lifetime ownership, and safe pressure handling.
