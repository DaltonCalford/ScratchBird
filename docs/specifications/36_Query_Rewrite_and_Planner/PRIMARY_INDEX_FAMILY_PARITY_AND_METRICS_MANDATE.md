# Primary Index Family Parity and Metrics Mandate

## Status

Reconstructed required specification. Beta 1 named-family parity is now
code-backed.

## Rule

Every shipped ScratchBird index family is a primary optimizer class.

There are no secondary, optional-for-planning, or planner-ignored index families in the optimizer contract.

Primary parity applies at the admitted named-family level, not only at the
shared runtime-backend level.

That means:
- every admitted `CatalogManager::IndexType` is independently primary for
  candidate formation and costing
- shared runtime backends or shared planner families do not demote a named
  family to hint-only or fallback-only status
- sibling families may reuse one metrics or calibration substrate only when the
  optimizer still receives named-family identity plus any variation-specific
  fields needed for deterministic ranking

## Implementation-State Boundary

This document is normative specification and the current Beta 1 implementation
now proves the core parity rules it defines:

- compatible sibling families lowered through shared backends remain distinct
  primary planner candidates
- named-family identity is preserved through the metrics packet and plan-signing
  surfaces
- shared runtime reuse does not demote any admitted named family to hint-only
  or secondary status

Broader Beta 2 optimizer expansion may still deepen mixed-family ranking,
adaptive search, and cross-workload competition, but those future changes do
not weaken or reopen the Beta 1 primary-parity rule.

If an index family is admitted into the engine as a supported runtime family, then the optimizer must:

- consider it through the same planner admission path as other families
- consume family-native metrics for it
- expose its metrics through the same optimizer-facing packet model
- cost it using family-appropriate costing rules
- compare it against competing access paths without demoting it to a hint-only role

## Consequence

Missing metrics for an index family are an implementation defect.

They are not a valid reason to:

- skip the family in planning
- downgrade the family to secondary status
- hide the family behind manual-only selection
- treat the family as non-comparable against B-tree or other families

## Optimizer Admission Rule

For every shipped family, the optimizer must have:

- planner-family lowering
- queryability state
- exactness or candidate semantics
- recheck semantics where applicable
- family-native metrics
- deterministic costing inputs

If any of those are incomplete, the family remains primary-class in canon and the missing pieces become mandatory implementation work.

## Metrics Rule

The optimizer must consume metrics for all shipped families, including but not limited to:

- ordered families
- hash families
- bitmap families
- summary families
- inverted/text families
- generalized spatial families
- ANN/vector families
- LSM families
- columnstore-access families

Metrics must be family-native wherever the runtime exposes them. Generic heuristic payloads are only bounded fallback behavior and must be replaced as family-native counters become available.

## No-Ignored-Index Rule

No optimizer path may classify a shipped family as:

- secondary
- advisory only
- ignored unless manually forced
- unsupported for costing solely because the metrics path is incomplete

The only valid reasons to reject a candidate path are:

- the family lowering result is invalid for the requested predicate or ordering shape
- the family does not satisfy the semantic contract for the requested operation
- current runtime support for that exact operation is fail-closed

## MGA Rule

Primary-class optimizer treatment does not change MGA correctness.

Every family remains subordinate to:

- Firebird-style MGA visibility
- recheck where required
- heap/version truth

Index parity means parity of optimizer consideration, not parity of correctness semantics.

## Audit lookup anchors

Representative audit anchors for this file are:
- `QueryPlanner::planStatement(`
- `loadIndexFamilyMetricsPacket =`
