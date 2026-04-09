# Vector ANN Planner Spec

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines how vector and ANN indexes participate in planning, what residency model they must expose, and how the optimizer must rank them against exact or non-vector alternatives.

## Current code-backed authority

Current runtime proof exists for page-backed vector and ANN families including HNSW-related structures with MGA-aware tuple lineage and soft-delete semantics. Durable state is stored in the database and reopened through ordinary storage surfaces.

## Required reconstructed behavior

Vector index families that are used for ANN search must operate as resident-memory runtime structures after first use.

This applies to:
- HNSW
- IVF and IVF variants
- vector-flat acceleration structures when configured as resident
- GPU-accelerated ANN derivatives such as CAGRA sidecars

## Residency model

The canonical residency state machine is:
- `cold_on_disk`
- `loading_from_durable_image`
- `resident_clean`
- `resident_dirty`
- `flushing_dirty_deltas`
- `evicted_or_unavailable`

Rules:
1. the durable database image remains the authoritative persisted form
2. first query or explicit warmup may trigger load into the resident search structure
3. subsequent searches must use the resident structure, not ad hoc page-walk ANN traversal
4. committed mutations update the resident structure and dirty the runtime image
5. dirty runtime state must be flushed back to durable image using MGA-safe commit publication rules
6. restart reconstructs resident state by loading from the durable image, not from an external WAL replay

## Planner admission

A vector ANN candidate must publish:
- distance metric
- dimension count
- expected recall at configured probe depth
- resident-state indicator
- cold-start penalty
- acceleration mode (`cpu_resident`, `gpu_resident`, `none`)
- maintenance debt
- stale graph or centroid debt if applicable

Planner ranking rules:
- resident clean candidates are preferred over cold ANN candidates at equal estimated recall and cost
- cold candidates remain admissible but carry a cold-load penalty
- if residency policy is declared required and the family is not resident or loadable, the optimizer must either:
  - fall back to an exact or alternative path explicitly
  - or fail closed if the query demands ANN-only semantics

## MGA interaction

Vector indexes are candidate finders, not visibility truth.

After ANN candidate production:
1. tuple or record identity is resolved to heap truth
2. MGA visibility is checked against the querying transaction
3. invisible, rolled-back, or reclaimed versions are rejected
4. visibility reject rate feeds back into metrics and costing

## Exact versus ANN fallback

The planner must compare:
- resident ANN path
- cold ANN path
- exact vector scan or exact ordered path where available

An ANN family must never suppress an exact path unless the caller explicitly requested approximate semantics.

## Observability

Every vector family must expose:
- resident bytes
- load time
- dirty delta count
- flush count
- recall estimate
- heap visibility reject rate
- accelerator residency state where applicable
