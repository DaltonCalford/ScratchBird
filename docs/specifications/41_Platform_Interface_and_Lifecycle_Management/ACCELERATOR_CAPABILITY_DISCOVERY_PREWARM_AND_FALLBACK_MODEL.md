# Accelerator Capability Discovery, Prewarm, and Fallback Model

## Purpose

This document defines how ScratchBird discovers optional accelerator resources,
how it prepares resident accelerator-backed artifacts, and how it falls back
when accelerator execution is unavailable.

This file sits beside the LLVM toolchain model. LLVM/JIT and accelerator lanes
share a common rule:
- optional execution acceleration must be explicitly discoverable
- compatibility must be explicit
- fallback must preserve correctness

## Current code-backed authority

Current code-backed authority already proves:
- LLVM toolchain discovery exposes:
  - provider identity
  - provider version
  - host target triple
  - availability state
- JIT artifacts are keyed by compatibility dimensions including:
  - canonical SBLR hash
  - target triple
  - CPU feature profile
  - ABI version
  - compiler identity and version
  - optimization profile
  - security policy version
- vector and ANN families already expose declared accelerator-related syntax,
  including `GPU_CAGRA`
- runtime and planner code already distinguish vector and ANN families from
  ordinary ordered or text families

## Required accelerator capability record

Every runtime instance shall maintain an accelerator capability record with at
least:
- `capability_generation`
- `device_kind`
- `device_id`
- `vendor_identity`
- `driver_identity`
- `driver_version`
- `runtime_identity`
- `runtime_version`
- `compute_capability`
- `total_memory_bytes`
- `usable_memory_bytes`
- `supports_ann_search`
- `supports_ann_build`
- `supports_vector_residency`
- `supports_jit_offload`
- `health_state`
- `degraded_reason`
- `last_validation_ms`

## Discovery lifecycle

Capability discovery shall execute in this order:
1. process start or capability refresh begins
2. enumerate optional accelerator backends
3. probe each candidate device
4. record compatibility and health
5. publish the resulting capability generation
6. only after publication may accelerator admission use the device inventory

Capability discovery failures may degrade acceleration, but they may not change
MGA transaction truth or database availability unless policy explicitly requires
accelerator presence for a configured workload.

## Prewarm policy

Prewarm policy is required reconstructed behavior.

Supported prewarm modes are:
- `NONE`
- `ON_FIRST_USE`
- `STARTUP_HOTSET`
- `MANUAL_ONLY`

The minimum required behavior for resident vector and ANN families is
`ON_FIRST_USE`.

When `STARTUP_HOTSET` is configured, the startup or post-recovery warmup lane
shall:
1. discover the configured resident hotset
2. validate device capability and budget
3. load or build derivative resident structures from durable MGA index state
4. mark each index as `READY`, `DEGRADED`, or `FAILED`
5. publish status before query execution begins relying on that residency

## Fallback rules

Fallback rules are:
- accelerator absence may not silently change query semantics
- fallback may only change execution path, cost, latency, or throughput
- when a family contract requires accelerator service and fallback is disabled,
  the statement shall fail closed before execution starts
- when fallback is allowed, the planner/runtime shall record:
  - that fallback occurred
  - why fallback occurred
  - which CPU family path was used instead

## Resident-index lifecycle

Accelerator-backed resident indexes shall follow this lifecycle:
- `UNLOADED`
- `LOADING`
- `READY`
- `REFRESH_PENDING`
- `DEGRADED`
- `EVICTED`
- `FAILED`

Transitions:
1. `UNLOADED -> LOADING` on first-use or prewarm start
2. `LOADING -> READY` after validation and build/load success
3. `READY -> REFRESH_PENDING` when durable MGA state advances and the derivative
   resident structure must catch up
4. `REFRESH_PENDING -> READY` after refresh
5. `READY -> DEGRADED` on device-health or memory-pressure problems where CPU
   fallback remains legal
6. `DEGRADED -> FAILED` when no usable execution path remains
7. `READY` or `DEGRADED -> EVICTED` only under explicit residency-governance
   action or unavoidable pressure policy

## Compatibility rules

An accelerator-resident artifact is compatible only when all of the following
still match:
- canonical index identity
- canonical durable index format version
- accelerator capability generation or explicitly compatible device profile
- metric family contract
- vector dimension and encoding contract
- runtime software version
- security policy version

Any mismatch requires rebuild or reload. It may not be ignored.

## Recovery and restart

After server restart:
- authoritative truth remains the durable MGA database image
- resident accelerator structures are treated as rebuildable derivatives
- restart may retain only metadata describing the previous resident state, not a
  trust transfer of authority to device memory
- first-use or prewarm reconstruction is the normal recovery path

## Required operator surfaces

The platform layer shall expose:
- discovered device inventory
- compatibility generation
- device health state
- last validation time
- prewarm mode and status
- resident-index counts and bytes
- fallback counts and reasons

## Improvement candidates preserved during recovery

The rebuild identifies these concrete improvement candidates:
- allow hotset prewarm scheduling through the existing job scheduler and
  workload-governance lanes
- use device capability generations the same way JIT artifact compatibility keys
  use CPU and compiler generations
- separate build-device and search-device pools when the runtime supports both
- capture accelerator degradation inside support bundles alongside buffer,
  checkpoint, and governance pressure
