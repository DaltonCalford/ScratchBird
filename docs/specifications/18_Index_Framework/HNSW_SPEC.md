# HNSW and Shared Vector ANN Specification

Status: current_authority_with_reconstructed_expansion

## Purpose

This document defines the current HNSW-backed vector runtime and the stronger reconstructed residency contract that routed vector families must inherit.

## Current code-backed HNSW authority

Current code authority proves:

- a durable HNSW page family using `SBHnswPage`, `SBHnswNode`, and `HnswNeighbor`
- stable heap TID references from HNSW nodes
- page and node MGA fields `xmin` and `xmax`
- soft delete and later cleanup instead of destructive immediate removal
- root-page creation with vector dimensions, distance metric, and graph parameter publication
- graph search and insert with current-transaction MGA filtering
- default graph controls in current code paths:
  - `M = 16`
  - `ef_construction = 200`
  - `ef_search = 100`
- dimension validation in current create path from `1` through `65536`

The current code path is durable-page-backed HNSW. It is not yet proof of a universal separate in-memory graph runtime for every routed vector family.

## Shared family coverage

Current routed vector-family surfaces include:

- `HNSW`
- `NEO4J_VECTOR`
- `IVF`
- `VECTOR_FLAT`
- `VECTOR_BIN_FLAT`
- `IVF_FLAT`
- `BIN_IVF_FLAT`
- `IVF_PQ`
- `IVF_SQ8`
- `IVF_SQ8_HYBRID`
- `RHNSW_PQ`
- `RHNSW_SQ`
- `ANNOY`
- `NSG`
- `DISKANN`
- `SCANN`
- `GPU_CAGRA`

If a named surface requires stronger semantics than the current routed HNSW-backed runtime can actually provide, that surface must be parser-gated, planner-degraded, or fail-closed until separately promoted.

## MGA-first contract

- vector indexes return candidate TIDs only
- final acceptance requires MGA visibility recheck after vector-distance evaluation
- updates and deletes create new heap/version truth first; old vector candidates remain until reclaim proof allows cleanup
- ANN routing quality does not override visibility correctness

## Search and graph contract

Current code-backed HNSW behavior includes:

- probabilistic layer selection for insertion
- entry-point descent from the highest available layer
- greedy or bounded search behavior per layer
- beam-like expansion governed by `ef_search` or `ef_construction`
- bi-directional link maintenance between graph nodes

Required reconstructed behavior for any public HNSW control surface:

- `ef_search` exposed to the operator or query surface must never be accepted below requested `top_k`
- stronger recall claims require explicit rerank or exact post-filter where ANN routing alone is insufficient
- graph-degree and expansion settings must be policy-validated before admission into production profiles

## Residency rule

HNSW and all routed vector families must inherit the family-level residency and accelerator rules from:

- `VECTOR_INDEX_RESIDENCY_AND_ACCELERATOR_MIRROR_MODEL.md`

That means the long-term canonical model is:

- durable HNSW image remains authoritative in the database
- CPU-resident canonical vector state is loaded on first use or explicit preload
- accelerator mirrors are derived from CPU-resident canonical state only

## Cleanup and maintenance rules

- stale vector candidates and graph edges remain maintenance debt until heap reclaim proof and structural safety allow cleanup
- compaction, graph rebuild, or neighbor repair is structural maintenance only
- optimizer-visible calibration must include runtime candidate expansion and actual acceptance quality
- quantized representations must preserve identity with the graph state that uses them

## Required optimizer metrics

The shared vector-family metrics packet shall include at minimum:

- vector count
- effective dimension and metric family
- average graph or routing degree
- candidate expansion count
- post-filter exact-distance reject rate
- recall calibration signal where available
- stale-entry debt
- MGA visibility reject rate
- rebuild or repair debt
- cold-load penalty signal or resident-state readiness
- metrics freshness and confidence

## Current-vs-required boundary

Current code-backed authority:

- durable HNSW page image
- MGA node visibility filtering
- graph insertion, search, and soft deletion
- configurable HNSW graph parameters

Required reconstructed behavior:

- memory-resident vector runtime on first use for designated vector families
- explicit cold vs warm costing distinction
- stronger public control validation and recall-grade surface contracts

## Non-authority and rejection rules

The following claims are incorrect:

- routed vector aliases already have distinct mature runtime families just because they have separate names
- HNSW graph hits bypass heap or MGA visibility confirmation
- graph cleanup may remove stale nodes before heap reclaim proof exists
- accelerator mirrors may redefine vector truth or durability semantics
