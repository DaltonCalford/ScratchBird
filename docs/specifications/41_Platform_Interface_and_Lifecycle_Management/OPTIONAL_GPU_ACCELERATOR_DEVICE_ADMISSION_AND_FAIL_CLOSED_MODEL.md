# OPTIONAL_GPU_ACCELERATOR_DEVICE_ADMISSION_AND_FAIL_CLOSED_MODEL

## Status

Required reconstructed specification with current code-backed boundary.

## Purpose

This document defines how ScratchBird shall use GPU or other accelerator resources when available without compromising MGA truth, determinism, or fail-closed behavior.

## Current code-backed boundary

The current source-backed state is:

1. planner and catalog surfaces already name accelerator-adjacent ANN families such as `GPU_CAGRA`
2. the current ScratchBird core source tree does not yet prove a full CUDA, OpenCL, ROCm, HIP, or Vulkan execution backend

Therefore the accelerator lane is a required reconstructed specification, not a claim that the current runtime already has a finished device backend.

## Governing rules

1. GPU use is optional.
2. Engine correctness shall never depend on GPU availability.
3. GPU execution shall never become the source of transaction, visibility, durability, or recovery truth.
4. If accelerator admission fails, the engine shall fall back to the CPU path or fail closed according to the execution policy for that operation.

## Eligible accelerator surfaces

The primary eligible accelerator surfaces are:

1. ANN candidate generation
2. vector distance batches
3. rerank kernels
4. quantization or vector transform kernels
5. other explicitly admitted numeric kernels whose semantics are already defined by the CPU path

GPU acceleration is not a license to invent new query semantics.

## Device admission contract

Before any accelerator path is used, the runtime shall prove:

1. device discovery succeeded
2. driver/runtime version is compatible
3. required kernel family is available
4. device memory budget is sufficient
5. the operation's semantic contract is already defined on the CPU path

If any of these checks fail, accelerator use shall be rejected deterministically.

## Fallback rules

1. CPU fallback is the default recovery path when accelerator admission fails for a non-required acceleration lane.
2. If a future execution policy marks an operation `REQUIRE_ACCELERATOR`, failure to admit the device shall return an error rather than silently changing execution semantics.
3. Planner recognition of an accelerator-flavored family name does not by itself authorize runtime execution on a GPU.

## Truth and durability rules

GPU device memory is derivative runtime state only.

That means:

1. database pages remain durable truth
2. MGA transaction inventory remains visibility truth
3. device-resident ANN state is a cache or execution image derived from durable truth
4. crash recovery never depends on replaying device memory

## Relationship to resident vector families

If a vector family is both memory-resident and accelerator-capable:

1. the host-resident image remains the authoritative runtime image
2. any device-resident image is subordinate to the host-resident image
3. flush, commit, rollback, GC, and restart are governed by the host-resident and durable MGA model, not by device memory state

## Required observability

Accelerator-capable builds shall expose at least:

1. device availability
2. admitted device identity
3. accelerator-enabled family list
4. accelerator fallback count
5. accelerator admission failure reason counts
6. device memory budget and current reserved bytes

## GPU family boundary in the current planner

Because the planner already recognizes `GPU_CAGRA`, the canonical rule is:

1. the family name may exist in planner inventory
2. it shall not become an execution promise until a real accelerator runtime path exists
3. if no runtime capability is present, the family must be rejected or lowered to a non-GPU family through an explicit fail-closed rule

## Required implementer interpretation

Another agent implementing accelerator support shall preserve:

1. CPU semantic truth
2. MGA-first durability and visibility
3. deterministic admission and fallback
4. explicit observability of accelerator state and failures
