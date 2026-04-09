# Implemented Candidate Bundle and No Ignored Index Runtime Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the current planner-side candidate-bundle model and the
reconstructed no-ignored-index rule that must govern all planner-visible index
families.

## Current code-backed planner substrate

The current planner implementation proves the following planner-side structures:

- `BaseAccessChoice`
- `BaseRelationCandidateBundle`
- `JoinDecision`
- `StatementPlanRequest`
- `StatementPlanResult`

The current candidate-bundle surface already carries:

- relation index
- candidate path set
- candidate scan families
- candidate family identity signatures
- candidate family statistics signatures
- selectivity
- access-path descriptor material

The current statement-planning request or result path also carries planning
snapshot identities such as:

- `statistics_snapshot_id`
- `family_metrics_snapshot_id`
- `planner_policy_snapshot_id`
- invalidation dependencies
- fallback and rejection stream

This is already richer than a planner that merely picks one implicit access path
with no retained candidate evidence.

## Candidate-formation rule

For each base relation, the planner shall form a candidate bundle that includes
all planner-visible family options admitted by:

- relation shape
- predicate shape
- available family metrics
- runtime-family alias mapping
- current fail-closed policy

Candidate bundles are part of the current planner substrate, not future design
fiction.

## No ignored index rule

ScratchBird requires the following:

1. every planner-visible index family is part of the same primary candidate
   formation regime
2. no family is silently excluded because it is treated as a secondary class
3. family exclusion requires an explicit reason recorded through the planner's
   rejection or fallback path
4. a scan fallback is allowed only when the planner cannot admit a valid index
   candidate under current metrics, queryability, or policy constraints

## Admissible rejection classes

A family candidate may be rejected only for concrete planner reasons such as:

- invalid or insufficient family metrics
- non-queryable or limited-queryability state where the query requires stronger
  semantics
- explicit rewrite or policy incompatibility
- unavailable runtime implementation for the routed family
- accelerator-only requirement without an admitted fallback path

It is non-conforming to reject or omit a family merely because it belongs to a
less mature implementation lane.

## Relationship to optimizer snapshots

The planner request and result surfaces already prove that family-metrics
identity is part of plan formation.

Therefore:

- access-path choice is snapshot-bound
- family metrics are part of plan identity
- invalidation dependencies must cover family-metrics changes when those changes
  affect candidate admission or cost

## Current code-backed parity closure

Current code proves:

- candidate bundles exist
- family metrics snapshots exist
- fallback and rejection recording exists
- compatible sibling families lowered through shared backends remain distinct
  planner candidates with distinct family identity signatures

The remaining optimizer work in this area is broader search depth and later
Beta 2 ranking expansion, not no-ignored-index or named-family parity drift.
