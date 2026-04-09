# Index Residency and Accelerator Policy

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines which index families are page-walk driven, which are resident-by-default after use, and how accelerator sidecars must behave under MGA rules.

## Residency classes

### Class A: page-driven durable families
These operate directly from durable pages plus buffer-pool caching.
Examples include:
- B-tree
- hash
- BRIN
- bitmap
- GiST
- SP-GiST
- R-tree and related spatial families
- inverted and GIN-style families unless explicitly promoted to resident sidecars

### Class B: resident-by-default after first use
These maintain a resident in-memory runtime image backed by a durable database image.
Required reconstructed class B families include:
- HNSW
- IVF variants
- vector-flat resident search structures where enabled
- GPU-backed ANN derivatives layered on canonical CPU-resident state

### Class C: optional accelerator sidecars
These are derived, non-authoritative structures that may be built from a class A or class B canonical family to improve lookup speed.
Examples:
- GPU search graphs
- compressed search tables
- approximate cache layers

## General rules

- Durable database state remains the only persistence authority.
- Resident or accelerator state must be rebuildable from the durable image.
- Uncommitted changes must not become visible through a resident or accelerator structure before MGA publication makes them visible.
- Eviction of resident state is a performance event, not a correctness event.

## Admission and pressure

Residency admission must respect:
- global memory budget
- family-local budget
- per-index hotness
- rebuild cost
- observed query demand
- accelerator availability

Eviction order should penalize:
- idle resident indexes
- high-memory low-hit structures
- stale accelerator mirrors that can be cheaply rebuilt

## Flush rules

Resident dirty structures must flush durable state only after commit publication and in an order consistent with MGA durability and forced-write policy.

Derivative accelerator state may lag durable state briefly, but planner metrics must mark the lag and may temporarily penalize or disable the accelerator path.

## Observability

Every resident or accelerator-capable family must expose:
- residency class
- resident bytes
- last load timestamp
- last flush timestamp
- dirty delta count
- load failures
- eviction count
- accelerator state
- cold-start penalty estimate
