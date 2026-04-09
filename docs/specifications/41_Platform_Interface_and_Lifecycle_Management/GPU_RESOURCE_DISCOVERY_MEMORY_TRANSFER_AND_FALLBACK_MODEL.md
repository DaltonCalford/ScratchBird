Status: reconstructed_required

# GPU Resource Discovery Memory Transfer and Fallback Model

## Purpose

This document defines the reconstructed required model for optional GPU resource use in ScratchBird.

## Canonical Rule

GPU resources are optional acceleration only. Absence of a GPU, refusal of a GPU, or runtime GPU failure shall never compromise query correctness. The engine shall fall back to CPU execution using the same canonical semantics.

## Discovery Inputs

GPU discovery shall publish:

- device identity
- driver or runtime identity
- available memory
- supported execution capabilities
- admission policy state
- compatibility result with the current engine build

## Admission States

The platform layer shall publish one of:

- `NOT_PRESENT`
- `PRESENT_BUT_DISABLED`
- `INCOMPATIBLE`
- `ADMITTED`
- `DEGRADED`

Only `ADMITTED` and explicitly accepted `DEGRADED` states may permit accelerator planning.

## Eligible Workloads

GPU admission may be considered only for operator classes explicitly admitted by canon, such as:

- vector similarity or ANN search
- dense scan or filter kernels where the runtime explicitly supports them
- batch-oriented analytic primitives explicitly admitted by the engine

## Transfer Rule

Before a GPU path may execute, the runtime shall account for:

- host-to-device transfer cost
- device-to-host transfer cost
- memory residency state
- batch size
- fallback cost if the device path refuses at runtime

## Failover Rule

If a GPU path fails before result publication, the engine shall:

- record the refusal or failure reason
- invalidate only the affected accelerator admission state as needed
- retry on CPU or other admitted path
- preserve transaction and MGA visibility semantics

## Visibility Rule

GPU execution never becomes an alternative visibility authority. Heap or version truth remains authoritative, and any GPU-assisted index or operator path is still subordinate to MGA correctness.

## Operator Diagnostics

The runtime shall expose:

- device admission state
- device memory pressure
- acceleration hit or miss count
- fallback count
- reason codes for refusal or demotion

## Non-Guarantees

This file does not claim every current operator is already GPU-enabled. It defines the required admission and fallback model for any GPU-enabled path.
