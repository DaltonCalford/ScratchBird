# Accelerator Residency and Fallback Gates

## Purpose

This document defines the certification gates for:
- resident vector and ANN index readiness
- accelerator admission and fallback correctness
- cold versus warm execution classification
- durability-safe refresh of derivative resident structures

## Gate classes

The canonical gate classes are:
- `GATE-ACCEL-001` capability discovery
- `GATE-ACCEL-002` cold first-use load
- `GATE-ACCEL-003` warm resident reuse
- `GATE-ACCEL-004` degraded-device fallback
- `GATE-ACCEL-005` no-authority-in-device-memory
- `GATE-ACCEL-006` refresh-after-durable-update
- `GATE-ACCEL-007` observability completeness
- `GATE-ACCEL-008` admission and queue behavior

## `GATE-ACCEL-001` capability discovery

Must prove:
- accelerator inventory is discoverable
- device identity and health are recorded
- incompatible devices are rejected before execution attempts rely on them

Required artifacts:
- capability inventory snapshot
- driver/runtime version capture
- discovery time and generation id

## `GATE-ACCEL-002` cold first-use load

Must prove:
- first-use execution can load or build the derivative resident structure from
  durable MGA state
- the resulting run is labeled `cold`
- the warmup state changes from `UNLOADED` or `LOADING` to `READY`

Required artifacts:
- before and after resident-index status
- execution timing split showing cold-start penalty
- failure reason when the first-use load does not succeed

## `GATE-ACCEL-003` warm resident reuse

Must prove:
- a second run can execute against a ready resident structure without repeating
  first-load work
- the resulting run is labeled `warm`
- the resident structure remains tied to the same canonical durable index

Required artifacts:
- resident-index status before and after
- warm-run latency sample set
- evidence that the same canonical index identity was used

## `GATE-ACCEL-004` degraded-device fallback

Must prove:
- when accelerator service becomes unavailable and fallback is allowed, the
  statement still returns correct results
- the run is labeled `fallback`
- the fallback reason is captured explicitly

Required artifacts:
- degraded-device state or injected unavailability record
- fallback reason code
- result equivalence check against the non-fallback path

## `GATE-ACCEL-005` no-authority-in-device-memory

Must prove:
- loss of resident or device memory does not lose committed truth
- authoritative MGA storage remains sufficient to rebuild the derivative
  structure

Required artifacts:
- resident-loss or eviction scenario
- rebuild or reload evidence from durable state
- post-rebuild correctness check

## `GATE-ACCEL-006` refresh-after-durable-update

Must prove:
- logical updates are durable in MGA truth before the derivative resident
  structure is refreshed
- refresh state is visible as `REFRESH_PENDING` until completed
- no derivative refresh path becomes authoritative over the durable index image

Required artifacts:
- update sequence trace
- durable publication marker
- resident refresh status transition

## `GATE-ACCEL-007` observability completeness

Must prove the operator can inspect:
- device health
- resident-index state
- fallback count and reason
- active and queued accelerator admissions
- memory pressure
- warmup readiness

Required artifacts:
- status rows or schema-equivalent payloads
- support-bundle capture showing the same fields

## `GATE-ACCEL-008` admission and queue behavior

Must prove:
- accelerator concurrency budgets are enforced
- queue depth and timeout are honored
- `ACCELERATOR_REQUIRED` requests fail closed when no legal device is available
- `ACCELERATOR_PREFERRED` requests fall back only when policy permits it

Required artifacts:
- queue pressure scenario
- rejection or fallback reason
- device-budget accounting before and after

## Pass and fail rules

A family is not certified as accelerator-ready unless all applicable gate
classes pass for that family and runtime mode.

A family is not certified as resident-ready unless cold, warm, refresh, and
rebuild semantics all pass.

## Minimum family coverage

The certification program shall at minimum cover:
- CPU-only vector families with resident host-memory policy
- accelerator-preferred ANN families
- accelerator-required families where supported
- degraded fallback path when `FALLBACK_CPU = true`
- hard refusal path when `FALLBACK_CPU = false`

## Benchmark labeling rules

Benchmark reports must explicitly say whether reported numbers are:
- `cold`
- `warm`
- `fallback`
- `degraded`

Any report that merges these modes without labeling is non-conforming.
