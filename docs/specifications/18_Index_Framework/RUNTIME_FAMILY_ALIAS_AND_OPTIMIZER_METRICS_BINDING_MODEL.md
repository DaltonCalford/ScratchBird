# Runtime Family Alias and Optimizer Metrics Binding Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines how concrete catalog index types are bound into runtime
families and how those runtime families are exposed to the optimizer metrics
contract.

It exists to prevent a limited implementer from treating some index families as
"primary" and others as optional or secondary planner decoration.

## Current code-backed authority

The current optimizer statistics path proves all of the following:

- concrete catalog index types are named and surfaced through
  `CatalogManager::IndexType`
- runtime aliasing exists through `runtimeIndexFamilyName(...)`
- optimizer-facing family metrics use typed envelopes in
  `scratchbird/optimizer/statistics.h`
- index-family metrics are classified through `IndexFamilyMetricsType`
- queryability and confidence are first-class fields through:
  - `IndexMetricsConfidenceClass`
  - `IndexMetricsQueryabilityState`

## Runtime-family binding rule

The optimizer does not reason over the raw catalog enum alone.

It binds concrete index types into runtime families that represent execution
semantics.

Current code-backed alias families include at least:

- ordered or exact family aliases routed through ordered-runtime handling
- hash-family aliases routed through hash-runtime handling
- generalized spatial aliases routed through spatial-runtime handling
- bitmap-family aliases routed through bitmap-runtime handling
- GIN-family runtime
- GiST-family runtime
- BRIN-summary-family runtime
- SP-GiST-family runtime
- ANN or vector-family runtime

This aliasing is authoritative planner input. It is not a documentation-only
taxonomy.

## Optimizer metrics binding rule

The current optimizer metrics contract exposes these family classes:

- `ORDERED_EXACT`
- `SUMMARY_CANDIDATE`
- `GENERALIZED_SPATIAL`
- `TEXT_SEARCH`
- `ANN`

Every planner-visible index type shall bind to one runtime family and one
metrics-family interpretation.

No planner-visible index type may be treated as an untyped afterthought or a
"secondary" family that bypasses the metrics contract.

## No-secondary-family rule

ScratchBird canonically requires:

1. every planner-visible index family participates in candidate formation
2. every participating family carries a typed metrics interpretation
3. alias surfaces may normalize family identity, but they may not demote the
   family out of planner consideration
4. a family may be rejected only for a concrete reason such as:
   - unsupported runtime contract
   - invalid queryability state
   - missing required statistics or freshness boundary
   - explicit fail-closed policy

It is non-conforming to keep some families permanently outside the main
optimizer path by classifying them as non-primary.

## Current code-backed parity closure

Current code already proves the alias and metrics-envelope substrate.

Current Beta 1 proof now also covers named-family parity over shared backends:

- sibling families lowered through one runtime family still publish their
  admitted named-family identity into planner-visible metrics packets
- shared calibration or runtime reuse does not authorize silent candidate
  collapse or demotion to hint-only status
- observability and planner signatures keep the named-family identity attached
  to the routed metrics surface

Broader Beta 2 cost-model expansion may refine ranking depth, but it does not
reopen the no-secondary-family rule.
