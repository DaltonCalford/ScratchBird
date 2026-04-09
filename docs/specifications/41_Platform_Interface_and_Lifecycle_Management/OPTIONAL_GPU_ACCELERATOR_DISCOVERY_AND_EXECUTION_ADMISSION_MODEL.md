Status: reconstructed_required_with_current_substrate

# Optional GPU Accelerator Discovery and Execution Admission Model

## Purpose

This file defines how GPU or other accelerator resources are to be discovered,
admitted, and used without changing correctness. It exists to prevent a limited
implementer from confusing optional acceleration with authoritative execution
truth.

## Governing rule

GPU acceleration is optional.

The absence of a GPU, driver, provider, or compatible device shall never make a
correct CPU execution path unavailable unless an explicit policy requires
accelerator-only execution for a bounded internal test or certification lane.

## Current code-backed baseline

This recovery pass proves the following current baseline:

1. Native acceleration already follows an optional-provider discipline in the JIT subsystem.
2. Runtime selection already supports strict compatibility checks and deterministic fallback.
3. Vector search families already execute in-process and remain subordinate to heap visibility and MGA truth.
4. No current code-backed public contract in this pass proves a mandatory production GPU execution path.

## Required reconstructed accelerator model

The recovered specification requires the following model.

### Device inventory

The runtime shall maintain a device inventory containing:

- device class
- provider identity
- driver/runtime version
- memory capacity
- memory available for database use
- compute capability or equivalent feature tier
- health state
- admission state

### Accelerator admission

An accelerator may be used only when all of the following are true:

1. the provider is available
2. the device inventory row is healthy
3. the operator policy allows accelerator use
4. the target workload family is accelerator-compatible
5. the working set required for that operation has a valid host-memory canonical image
6. the correctness path is identical to CPU execution

### Fallback order

The required fallback order is:

1. accelerator execution
2. resident host-memory execution
3. ordinary CPU in-process execution

The engine shall not skip directly from accelerator failure to statement failure
unless policy explicitly requires accelerator-only execution for a bounded test
or maintenance lane.

## MGA and truth-source boundary

Accelerator memory is never authoritative truth.

The truth-source order is:

1. committed MGA-visible database state
2. resident host-memory index working set
3. accelerator mirror or accelerator-private cache

An accelerator copy shall be treated as a derivative mirror of the resident
host-memory image, which is itself a derivative execution image of durable MGA
page state.

## Ordered update model

For accelerator-backed index families:

1. read durable state or resident host-memory state
2. materialize or refresh the resident host-memory canonical image
3. mirror into device memory when admitted
4. apply mutations to the canonical host-memory image first
5. propagate to the accelerator mirror
6. flush durable on-disk changes according to the family publication rules
7. invalidate or refresh device mirrors after durable or structural generation changes

The accelerator path shall not publish mutations that bypass the canonical host
resident image.

## Failure classes

Accelerator failures shall be classified as:

1. provider unavailable
2. device unavailable
3. device unhealthy
4. insufficient device memory
5. device/host compatibility mismatch
6. mirror stale
7. mirror load failed
8. execution failed after admission

Each class shall map to deterministic:

- operator status
- observability counters
- fallback action
- refusal action where applicable

## Operator-facing requirements

The status surface shall eventually expose:

- accelerator enabled policy
- detected device count
- admitted device count
- current workload family using the device
- host resident bytes
- device resident bytes
- fallback count
- accelerator refusal count by reason

## Reconstructed family scope

The reconstructed accelerator scope applies first to vector and ANN families,
including:

- HNSW
- IVF
- vector flat
- GPU-assisted CAGRA-style families

Additional families may opt in only through explicit section-local authority.

## Fail-closed boundaries

The following are not permitted:

1. accelerator-only truth
2. device-private mutations with no host-memory canonical image
3. weaker visibility rules on accelerator paths
4. skipping heap/MGA visibility checks because an accelerator returned candidates
5. silent changes in query result semantics between CPU and accelerator paths
