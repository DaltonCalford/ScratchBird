# GPU CAGRA Spec

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the optional GPU accelerator model for vector ANN execution. GPU use is an acceleration lane, not a separate durability or visibility model.

## Current code-backed baseline

Current canonical proof for ScratchBird guarantees:

- durable vector state in the database
- CPU-side runtime selection logic for ANN families
- LLVM and optional accelerator provider admission vocabulary

Current code does not yet prove a universal production-grade GPU CAGRA runtime with full family-native planner and observability parity. That remains reconstructed required behavior below.

## Family truth order

GPU CAGRA follows the family-level vector residency rules from:

- `VECTOR_INDEX_RESIDENCY_AND_ACCELERATOR_MIRROR_MODEL.md`

The required truth order is:

1. durable MGA-backed vector index image
2. CPU-resident canonical vector runtime state
3. GPU-resident CAGRA mirror

## Required reconstructed specification

Where GPU resources are available and policy permits them, the CAGRA accelerator model is:

- derivative of the canonical durable vector index
- loaded through CPU-resident canonical runtime state
- copied or transformed into a GPU-resident search structure
- used only for candidate generation and optional accelerator-side coarse filtering
- never authoritative for MGA visibility, commit truth, or durability

## Capability detection and admission

At startup or first accelerator admission the runtime must determine:

- GPU presence
- driver and runtime compatibility
- supported distance metrics and dimension limits
- available VRAM budget
- build compatibility of the accelerator provider
- queue and batch-capacity limits for the current deployment profile

The result must be published as one of:

- `accelerator_ready`
- `accelerator_unavailable`
- `accelerator_misconfigured`
- `accelerator_policy_disabled`
- `accelerator_non_conforming`
- `accelerator_degraded`

## Runtime model

1. validate durable vector metadata and resident CPU state identity
2. verify dimension, metric, and provider compatibility
3. allocate GPU-resident graph or adjacency structures
4. build or copy accelerator-native layout from CPU-resident canonical state
5. serve ANN candidate generation from GPU memory
6. validate candidates against CPU-side identity and MGA visibility
7. flush committed structural changes back through the canonical CPU and durable database path

## Update and refresh ordering

For committed vector changes the required order is:

1. commit durable vector truth under MGA ordering
2. refresh CPU-resident canonical state
3. enqueue or apply GPU mirror refresh
4. publish accelerator-ready state for later readers only after refresh success

GPU refresh lag is allowed only as a derivative-state lag and must be observable.

## Fallback rules

If the GPU lane is unavailable or unhealthy:

- the runtime falls back to CPU-resident ANN when supported
- or falls back further to exact vector scan if family policy allows
- or refuses the operation if the request explicitly requires GPU acceleration

GPU failure must not corrupt canonical ANN durability or visibility semantics.

## Required metrics

A GPU-backed vector family must publish:

- provider identity and version
- VRAM bytes reserved and used
- load time and rebuild time
- resident graph node count
- batch throughput
- recall estimate relative to configured probe depth
- fallback count to CPU mode
- accelerator eviction or reset count
- accelerator refresh debt
- visibility reject rate after heap confirmation
- cold-load versus warm-load execution count

## Planner rule

The planner must treat GPU readiness as a cost and admission dimension, not as a hidden implementation detail.

Required rule:

- if GPU readiness is absent, the planner must cost the CPU path instead of assuming accelerator latency
- if accelerator startup or reload is required, cold-load cost must be reflected in candidate ranking or the path must be conservatively degraded
- if the request explicitly requires accelerator execution, lack of GPU readiness must fail closed rather than silently claiming accelerator-grade latency

## Security and safety

GPU artifacts are runtime derivatives and must not be treated as privileged persistence stores. Sensitive data protections and masking rules remain governed by ordinary security and row or column visibility rules.

## Non-authority and rejection rules

The following claims are incorrect:

- GPU memory is the authoritative vector index image
- GPU candidate generation bypasses CPU-side MGA visibility confirmation
- accelerator failure implies durable vector corruption
- GPU latency envelopes may be promised when accelerator readiness is absent or degraded
