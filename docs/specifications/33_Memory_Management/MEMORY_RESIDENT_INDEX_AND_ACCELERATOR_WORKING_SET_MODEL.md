# Memory Resident Index and Accelerator Working Set Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines how resident ANN indexes and accelerator providers consume memory, when they load, when they flush, when they may degrade, and which families are required to remain resident after first use.

## Working Set Classes

### CPU Resident Canonical Index Working Set
Used for:
- HNSW graph state
- IVF centroid and posting assignment structures
- vector-flat resident search state where enabled

### Accelerator Working Set
Used for:
- GPU-resident graph or adjacency sidecars
- GPU distance-computation buffers
- batch query staging buffers

## State Model

- `cold`
- `warming`
- `resident_clean`
- `resident_dirty`
- `resident_required`
- `evicting`
- `evicted`
- `accelerator_faulted`
- `accelerator_degraded`

## Resident-Required Rule for Vector Families

Vector search families are not ordinary caches.

Required behavior:
1. on first use or explicit warmup, the canonical CPU-resident vector index state is loaded from the durable database image
2. once loaded, the canonical CPU-resident state remains resident in memory
3. mutations update the resident canonical state and are flushed to durable database state
4. ordinary LRU-style eviction of canonical vector state is non-conforming

Permitted exceptions:
- process restart
- explicit administrative unload or rebuild
- fail-closed emergency demotion with operator-visible degraded-state publication

## Accelerator Sidecar Rule

Accelerator state is derivative.

Required behavior:
- accelerator sidecars may be dropped and rebuilt
- accelerator faults do not redefine canonical CPU-resident truth
- planner and runtime must observe accelerator availability as an optimization state, not a visibility state

## Budgeting

A resident family must declare:
- minimum warm bytes
- steady-state bytes
- rebuild cost class
- flush cost class
- whether acceleration is optional or required
- whether canonical CPU residency is required after first touch

## Flush and Restart Rules

- dirty resident changes are flushed as durable database state, not external logs
- accelerator sidecars may be discarded and rebuilt
- restart reloads resident state from the durable database image on first use or explicit warmup
- restart does not entitle the system to drop the resident-required rule for vector families after reload

## Planner and Admission Rules

1. planner state must reflect cold-start and warm-state penalties
2. planner must not treat resident-required vector families as optional secondary indexes to be ignored
3. memory governor may refuse accelerator promotion before it degrades canonical CPU-resident state
4. any emergency demotion of canonical resident vector state must emit degraded-state markers and fail closed where correctness or SLA policy requires residency

## Observability

Required outputs:
- resident index bytes by object
- resident-required flag by object
- accelerator bytes by object
- warm and cold transitions
- flush latency
- eviction or emergency-demotion count
- reload count after restart
- degraded-state reason
