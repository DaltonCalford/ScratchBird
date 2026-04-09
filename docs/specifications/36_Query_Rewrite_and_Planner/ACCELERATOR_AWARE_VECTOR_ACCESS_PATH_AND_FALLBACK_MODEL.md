Status: reconstructed_required_with_current_substrate

# Accelerator-Aware Vector Access Path and Fallback Model

## Purpose

This file defines how the optimizer must treat vector and accelerator-capable
access paths so they are never silently ignored and never granted stronger
correctness than ordinary MGA-visible access paths.

## Governing rule

Vector and accelerator-capable indexes shall participate in candidate planning
under the same "no ignored index" rule that governs other families.

Lack of accelerator availability may change cost and admission, but it shall
not silently remove a valid family from consideration when a CPU or resident
host-memory path exists.

## Current code-backed baseline

The current planner and stats recovery already prove:

1. candidate bundles are family-typed
2. family metrics flow through a typed metrics packet
3. the optimizer is being rebuilt around parity rather than single-family privilege
4. vector families already have runtime implementations and metrics surfaces

## Required planner inputs

For any vector or accelerator-capable family, the planner shall consider:

- runtime family
- alias surface
- resident-state readiness
- accelerator availability
- device/host compatibility
- vector dimensionality
- distance metric
- expected candidate fanout
- visibility reject rate
- warm-load cost when resident state is cold
- accelerator admission refusal reason, when present

## Admission algorithm

For a vector family candidate:

1. construct the family candidate regardless of accelerator presence
2. bind the family metrics packet
3. evaluate whether a resident host-memory structure already exists
4. evaluate whether accelerator execution is admitted
5. if accelerator is admitted, cost the accelerator path
6. if accelerator is not admitted but resident host-memory is available, cost the CPU resident path
7. if no resident image is warm, cost the warm-load plus execution path
8. compare the resulting family cost against competing candidates

The optimizer shall not classify a family as "ignored" merely because the best
accelerator path is unavailable.

## Fallback hierarchy

The planner and executor shall agree on this fallback hierarchy:

1. accelerator path
2. resident host-memory path
3. ordinary in-process CPU path
4. fail only when the family itself cannot execute and no alternate access path is legal

## Correctness boundary

The optimizer may choose a vector family to produce candidates, but final row
acceptance still requires:

1. heap row fetch
2. MGA visibility evaluation
3. any residual predicate evaluation

The optimizer shall not assign "index-only truth" to vector families.

## Metrics parity requirements

Vector families shall publish enough metrics to prevent systematic underuse.

The minimum required vector packet is:

- family identity
- runtime family
- alias surface
- supported distance metrics
- vector dimensionality
- resident-ready flag
- warm-load cost class
- accelerator-ready flag
- accelerator refusal reason
- candidate count estimate
- visibility reject rate
- dead-entry burden
- maintenance freshness epoch

## Fail-closed rules

The optimizer shall refuse or downgrade a vector access path when:

1. the requested metric is unsupported
2. the dimensionality contract is incompatible
3. family-native metrics are unavailable and no bounded fallback estimate exists
4. the family requires accelerator-only execution and no admitted accelerator exists
5. the index is structurally quarantined

## Improvement candidates

The rebuild identifies the following improvement candidates:

1. vector-family warm-load cost calibration from benchmark data
2. accelerator-aware admission hints in plan output
3. family-native selectivity calibration instead of heuristic generic vector costs
4. resident-state prewarming for repeated vector search workloads
