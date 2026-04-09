Status: reconstructed_required_with_current_substrate

# Family Native Index Metrics Packet and Cost Calibration Model

## Purpose

This document defines how the optimizer consumes family-native index metrics and calibrates cost across heterogeneous index families.

## Canonical Rule

All implemented index families are primary optimizer candidates. The optimizer shall compare them through calibrated family-native metrics rather than by granting first-class treatment only to one historical family.

## Candidate Generation Rule

For every predicate, ordering request, ANN request, text-search request, or spatial request, candidate generation shall:

1. enumerate all implemented families that can legally satisfy the requested semantics
2. request the current metrics packet for each family
3. normalize family-native metrics into the shared planner comparison frame
4. rank candidates without suppressing a family solely because it is not ordered B-tree

## Normalized Comparison Frame

The comparison frame shall include:

- semantic exactness
- ordering coverage
- residual filter requirement
- expected candidate count
- expected visibility reject rate
- memory residency status
- startup cost
- per-tuple continuation cost
- maintenance debt penalty
- stale-metrics penalty

## Cost Calibration Rule

The optimizer shall apply family-specific calibration functions before comparison. Calibration may differ across ordered, inverted, spatial, ANN, summary, hash, bitmap, and columnar families, but the output shall be a comparable planner cost frame.

## Required Penalties

The optimizer shall model explicit penalties for:

- stale metrics
- degraded confidence
- high visibility reject rate
- maintenance backlog
- non-resident ANN or vector warm-load cost
- required post-filter cost

## Required Credits

The optimizer shall model explicit credits for:

- full semantic exactness
- native ordering
- resident-memory admission
- low visibility reject rate
- high-confidence current metrics
- narrow candidate fanout

## No Ignored Family Rule

It is non-conforming for the optimizer to:

- exclude an implemented family from comparison because its metrics packet is different
- treat a family as advisory-only when it can produce a legal access path
- use ordered-family defaults as the hidden baseline for all other families

## Accelerator Interaction

Where a family has optional accelerator support, the optimizer shall compare:

- accelerator-admitted cost
- non-accelerated family cost
- ordinary fallback paths

The optimizer shall prefer the accelerator path only when the admission state and calibrated cost justify it.

## Diagnostics Requirements

The planner shall produce operator-visible diagnostics for:

- family candidates considered
- metrics freshness and confidence
- normalized cost components
- reason a candidate lost
- reason a candidate was refused

## Certification Boundary

Section 31 certification shall require proof that each implemented family can appear as a winning plan under appropriate workload and metrics conditions.
