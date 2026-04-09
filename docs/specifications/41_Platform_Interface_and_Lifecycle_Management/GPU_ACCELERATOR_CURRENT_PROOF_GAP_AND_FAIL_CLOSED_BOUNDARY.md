Status: unsupported_boundary

# GPU Accelerator Current Proof Gap and Fail-Closed Boundary

## Purpose

This file records the current-proof boundary for GPU accelerator support so the
canonical specification does not overclaim shipped behavior.

## Current proof result

The current rebuild pass did not find a code-backed ScratchBird runtime surface
for:

- CUDA execution
- CAGRA provider execution
- public GPU device management
- public GPU benchmark or certification lanes

This means the current canonical position is:

1. GPU acceleration is part of the reconstructed required architecture
2. GPU acceleration is not yet a code-proven shipped runtime contract in the currently inspected tree

## What is current authority

The current code-backed authority remains:

1. optional native-provider discipline in the LLVM/JIT subsystem
2. vector-family runtime and residency contracts on CPU/in-process paths
3. routed alias names that may reference future GPU-capable families

## Fail-closed rule

Until a real GPU runtime is promoted from code:

1. parser surfaces shall not imply guaranteed GPU execution
2. planner surfaces shall not assume a shipped GPU path exists
3. benchmarking and certification shall not claim GPU results as current product evidence
4. any `GPU_CAGRA` or similar alias surface must remain routed, degraded, or explicitly unsupported unless the implementation proves otherwise

## Promotion requirements

A GPU runtime may be promoted into current authority only after code-backed proof
exists for:

1. provider discovery
2. device admission
3. fallback behavior
4. observability
5. benchmark and certification lanes
