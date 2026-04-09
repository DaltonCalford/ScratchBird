# Optional GPU Accelerator Index Admission and Runtime Fallback Model

Status: reconstructed_required_with_current_substrate

## Purpose

This file defines how optional GPU or accelerator-backed index paths are to be
admitted without changing correctness or causing silent family exclusion.

## Current code-backed substrate

The current recovered substrate proves:

- accelerator-sensitive planning and runtime policy already exist elsewhere in
  the toolchain and JIT surfaces
- the index-type surface already includes accelerator-oriented catalog identity,
  including `GPU_CAGRA`
- planner request and result paths already retain family-metrics snapshot
  identity and fallback streams
- optimizer metrics classes already distinguish ANN-style family handling

This is enough to define admission rules now, even though a full shipped GPU
runtime surface is not yet proven.

## Governing rule

GPU acceleration is optional.

The absence of GPU devices, drivers, providers, or runtime admission shall
never invalidate a correct CPU or host-memory path for the same logical index
family unless an explicit bounded internal certification lane requires
accelerator-only execution.

## Admission rules

Accelerator-backed family admission shall follow this order:

1. determine whether the logical family is planner-visible
2. determine whether an accelerator-capable variant exists
3. determine whether accelerator runtime admission succeeds
4. if accelerator admission fails, fall back to the strongest valid non-GPU path
   for the same logical family
5. if no valid non-GPU path exists, emit explicit rejection or unsupported
   status through the planner or execution surface

## Correctness boundary

Accelerator presence may change:

- cost
- working-set placement
- execution latency
- runtime admission

Accelerator presence shall not change:

- MGA visibility rules
- transaction semantics
- planner family identity
- requirement to consider the family in candidate formation

## No silent demotion rule

An accelerator-capable family may not disappear from planning merely because GPU
runtime proof is absent.

The planner must distinguish:

- logical family unavailable
- accelerator variant unavailable
- accelerator variant denied
- accelerator variant admitted

Only the accelerator variant may be refused on accelerator grounds.
The logical family must still be considered through any admitted CPU or
host-memory path.

## Current rebuilt implementation boundary

Current code does not yet prove a complete public GPU execution surface.

Therefore:

- GPU-capable execution remains reconstructed required behavior
- fail-closed refusal is required where accelerator-only semantics would be
  needed but are not currently proven
- planner parity and fallback discipline are still mandatory now
