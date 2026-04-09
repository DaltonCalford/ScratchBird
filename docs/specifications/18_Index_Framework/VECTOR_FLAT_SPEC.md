# Vector FLAT Specification

Status: current_authority_with_reconstructed_expansion

## Purpose

This document defines the current ScratchBird exact-vector surface and the stronger reconstructed residency and optimizer obligations that apply while flat-vector names are still routed through the shared vector runtime.

## Current implementation boundary

`VECTOR_FLAT` and related flat-vector surfaces are currently admitted through the shared vector-family runtime defined by `HNSW_SPEC.md`.

A distinct brute-force resident segment implementation is not current implementation authority unless separately promoted.

## Current routed behavior

While routed through the shared vector family, the flat-vector surface currently preserves these external rules:

- candidate rows are scored using the declared metric family
- final row acceptance requires MGA visibility recheck
- if exact-distance semantics are promised, post-filter or rerank must enforce them
- obsolete vector candidates remain until heap reclaim proof permits cleanup

## Required reconstructed behavior

Where the product promises exact-vector semantics at commercial grade, the flat-vector family must adopt the shared vector residency model from:

- `VECTOR_INDEX_RESIDENCY_AND_ACCELERATOR_MIRROR_MODEL.md`

and must additionally make explicit:

- whether the resident state is full exact-vector memory or quantized derivative memory plus exact payload fetch
- whether cold load is required before first exact-vector pass
- whether rerank uses resident exact payload or durable payload fetch

## Required optimizer metrics

The flat-vector surface must publish at minimum:

- candidate expansion count
- exact-distance evaluation count
- exact-distance reject rate
- MGA visibility reject rate
- resident readiness or cold-load state
- vector count, dimension, and metric family
- freshness and confidence

## Planner rule

The planner must not silently down-rank flat-vector surfaces into irrelevance because ANN or ordered families have historically dominated a relation.

A flat-vector surface must be either:

- enumerated as an exact-vector candidate path
- conservatively costed because residency or metrics are incomplete
- fail-closed if the advertised exact semantics cannot currently be met

## Non-authority and rejection rules

The following claims are incorrect:

- a routed flat-vector surface is automatically a distinct brute-force engine today
- exact-vector promises may be made without accounting for cold-load and rerank cost
- flat-vector candidate acceptance may bypass MGA visibility recheck
