# Optimizer Candidate Bundle and Access Path Annotation Pipeline

Status: current_authority

## Purpose

This file defines how the optimizer gathers access-path candidates, annotates them with metrics and rejection reasons, freezes the candidate bundle, and records the chosen path.

## Current code-backed authority

The current planner builds per-relation access bundles containing:
- candidate access paths
- candidate scan families
- family identity signatures
- family statistics signatures
- rejection reasons
- candidate budget
- chosen-path diagnostics

The bundle is frozen and annotated around the `P08_ACCESS_PATH_ANNOTATE` boundary.

## Candidate bundle contents

For each relation bundle the planner records:
- all viable candidate paths
- `candidate_scan_families`
- `candidate_family_identity_signatures`
- `candidate_family_statistics_signatures`
- `candidate_bundle_candidate_count`
- `candidate_bundle_frozen`
- `rejected_composition_reasons`

The current candidate bundle is also responsible for preserving:
- chosen candidate identity
- chosen plan hash or stable chosen-path trace
- isolation or legality filters applied before final choice
- effective queryability posture per family-lowered candidate

## Metrics loading and use

The planner loads `IndexFamilyMetricsPacket` per index and may also parse the embedded payload JSON to read family-native metrics.

These metrics directly inform:
- cost calibration input
- confidence and queryability posture
- ordered-family completeness
- parameterized access-path admission
- ANN and other family-local candidate budgets

## Family-lowering and route normalization

The candidate pipeline is not allowed to operate on raw catalog index labels alone. It must first lower catalog index types into planner families.

The current family-lowering layer groups raw catalog types into normalized families including:
- B-tree-like ordered families
- hash-like exact families
- summary families
- summary-filter families
- generalized or spatial families
- vector families
- text or inverted families

The family-lowering result must publish at minimum:
- normalized planner family
- family name
- path name
- exactness class
- visibility-enforcement class
- `requires_recheck`
- `supports_ordering`
- `supports_covering`
- `supports_parameterization`
- queryability state

For generalized families the lowering result may be invalid if operator strategy, opclass support, or function support is not present. Invalid lowered families must stay visible in rejection traces; they are not allowed to disappear silently.

## Rejection recording

Rejected candidates are not allowed to disappear silently. The planner records:
- candidate label
- reason code
- detail
- startup cost
- total cost
- estimated rows

## Choice and freeze algorithm

1. build all base access candidates for the relation
2. lower raw index or access-method surfaces into normalized planner families
3. collect family signatures and statistics signatures
4. annotate each candidate with legality, parameterization, coverage, and queryability posture
5. rank candidates under cost, order-delivery, coverage, and parallelism rules
6. filter candidates that violate higher-order legality or isolation requirements
7. choose the best candidate or synthesize one from the best discovered path
8. when equal-cost ties remain, use the deterministic plan-hash tie-break path
9. freeze the bundle and publish candidate count and signatures into runtime relation metadata
10. persist rejection reasons and chosen-path traces for explainability and diagnostics

## Parallelism rule

Parallel scan candidates may compete inside the same bundle. If a serial path wins, the planner must still record that a parallel candidate existed and why it lost.

## Parameterized lateral rule

For lateral and parameterized paths, the planner must only claim parameterized index support when:
- the family lowering supports parameterization
- lifecycle legality is valid
- maintenance state is compatible
- effective queryability is not invalid

## Generalized-family operator support rule

For generalized families the planner may only admit a candidate when the operator family can be mapped to a supported generalized strategy and the opclass proves the required support function or strategy capability.

If the operator cannot be lowered lawfully, the planner must:
- mark the lowered family invalid or non-queryable
- require recheck where appropriate
- keep the rejection visible in candidate diagnostics

## Non-negotiable rule

A chosen path does not erase the existence of the rest of the bundle. Candidate bundle diagnostics are part of current implementation truth and are required for commercial-grade explainability.

No family-lowered candidate may be dropped solely because it is inconvenient for explainability or parity accounting.
