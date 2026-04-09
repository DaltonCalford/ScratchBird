Status: current_authority_with_reconstructed_expansion

# INDEX FAMILY METRICS PACKET AND COST INPUT NORMALIZATION MODEL

## Purpose

This file defines the canonical optimizer-input contract for index-family
metrics packets, cost-input normalization, and planner-side handling of
family-native versus fallback metrics.

This file exists so a limited implementer does not have to infer how family
metrics become planner cost inputs.

## Governing rule

Every shipped index family is a primary optimizer class.

Therefore every shipped family must enter the planner through the same
top-level packet discipline:

1. normalize catalog surface into planner family
2. load shared metrics packet
3. validate packet quality and queryability
4. normalize family payload into cost inputs
5. rank or reject deterministically

There is no secondary-family bypass.

## Shared packet contract

The current optimizer already uses a shared `IndexFamilyMetricsPacket`
envelope.

Current recovered shared fields include:

- `physical_family`
- `planner_family`
- `queryability_state`
- `metrics_last_refresh_xid`
- `metrics_confidence_class`
- `leaf_pages`
- `height`
- `row_count_est`
- `live_entry_count_est`
- `dead_fraction`
- `bloat_ratio`
- `recheck_ratio_est`
- `correlation`
- `coverage_fraction`
- `maintenance_backlog_ops`
- `publish_lag_xids`
- `reclaim_lag_xids`
- `family_metrics_version`
- `family_metrics_type`
- `family_metrics_payload`

Recovered extended payload-envelope fields include:

- `runtime_family`
- `alias_surface`
- `native_metrics_mode`
- `semantic_contract_state`
- `requires_fail_closed_stronger_semantics`

When available, the packet also carries:

- `family_metrics.native_runtime_metrics`

## Normalization stages

### Stage 1: family lowering

The planner must first lower catalog-visible index surfaces into normalized
planner families.

This stage must use:

- family identity
- opclass evidence
- operator-strategy support
- exactness class
- recheck contract
- ordering support
- coverage posture

Raw catalog index type names are not sufficient on their own.

### Stage 2: packet load

After lowering, the planner must obtain the metrics packet for the candidate
index.

Required rule:

- packet lookup failure must remain visible to diagnostics
- packet absence is not permission to silently erase the index from the search
  space

### Stage 3: packet validation

Before costing, the planner must validate:

- `queryability_state`
- metrics confidence class
- metrics type and version
- required numeric fields
- semantic-contract state
- stronger-semantics fail-closed flags

Current recovered plan-selection behavior already rejects:

- missing required inputs
- invalid numeric inputs

## Queryability-state handling

Current recovered queryability states are:

- `QUERYABLE`
- `LIMITED`
- `INVALID`
- `UNKNOWN`

Planner handling is:

- `QUERYABLE`: admit and cost normally
- `LIMITED`: admit only under degraded or conservative costing and preserve the
  reason in diagnostics
- `INVALID`: reject explicitly
- `UNKNOWN`: either conservative-cost or reject depending on whether the query
  requires stronger semantics than the family can currently prove

## Confidence and semantic-contract handling

The planner must normalize confidence and semantic-contract state into rankable
inputs.

Required behavior:

- lower confidence increases conservative penalty
- stronger-semantics gaps may force fail-closed rejection
- alias surfaces may remain rankable only as limited or routed candidates
- generic heuristic payloads may not masquerade as family-native truth

## Mandatory normalized cost inputs

Every family candidate must normalize into a common cost-input vocabulary with
at least:

- access-shape class
- selectivity estimate
- candidate-set size estimate
- expected heap or visibility recheck burden
- maintenance/backlog penalty
- bloat or fragmentation penalty
- publication or reclaim lag penalty
- coverage benefit
- ordering-delivery benefit
- cold-start or residency penalty where applicable

## Family-native expectations

### Ordered exact families

Ordered families must normalize:

- equality selectivity
- range selectivity
- ordering-delivery benefit
- correlation
- height
- leaf-page count
- dead or bloat burden

### Hash families

Hash families must normalize:

- exact-match selectivity
- bucket or chain distribution quality
- overflow or collision burden
- dead-entry burden

### Summary families

Summary families such as BRIN-like or zonemap-like families must normalize:

- pruning fraction
- false-positive or recheck burden
- range coverage
- maintenance lag

### Spatial and generalized families

Spatial, GiST-like, SP-GiST-like, and related generalized families must
normalize:

- predicate support class
- lossy candidate rate
- recheck ratio
- shape-specific selectivity evidence
- structural depth and fanout burden where available

### Text and inverted families

Text and inverted families must normalize:

- posting selectivity
- recheck burden
- ranking or scoring overhead
- token or lexeme distribution evidence

### ANN and vector families

ANN and vector families must normalize:

- candidate recall posture
- exact rerank cost when promised
- cold-load versus warm-resident penalty
- MGA visibility reject burden
- accelerator readiness when optional GPU execution exists

### LSM and columnar access families

LSM and columnar access families must normalize:

- read-amplification burden
- compaction or backlog pressure
- segment or run pruning effectiveness
- ordering or merge penalty where applicable

## Packet-type and version rule

`family_metrics_type` and `family_metrics_version` are not cosmetic.

The planner must use them to:

- decode payload semantics safely
- reject mismatched payload expectations
- preserve deterministic fallback behavior when payload upgrades are incomplete

## Alias and routed-surface rule

If a named family is currently routed through another runtime family, the packet
must preserve:

- alias identity
- runtime family identity
- semantic contract state
- stronger-semantics fail-closed indicator

The planner must not present a routed alias as full native parity if its
runtime metrics or semantics are still indirect.

## Deterministic ranking rule

Once all admissible candidates are normalized into comparable cost inputs, the
planner must rank them under one deterministic decision process.

Current recovered tie-break behavior uses lexical plan-hash comparison for
equal-cost selection and that remains part of canonical deterministic behavior.

## Diagnostics requirement

For each index family considered, the planner must preserve diagnostics for:

- admitted normally
- admitted conservatively
- rejected for invalid metrics
- rejected for semantic-contract gap
- rejected for unsupported operator strategy
- rejected for stronger-semantics fail-closed boundary

## Reconstructed required expansion

The rebuilt commercial-grade specification requires:

- a stable reason-code vocabulary for packet rejection and conservative ranking
- family-native metrics for every shipped family
- elimination of any long-term planner state where a family survives only on
  generic heuristic payloads
- enough packet detail that no shipped family becomes implemented-but-never-used
  by omission

## Non-authority rules

The following are incorrect:

- treating generic packet presence as proof that family-native metrics exist
- treating a missing payload as permission to skip a shipped family silently
- cost-normalizing vector or ANN families as warm-resident when the packet only
  proves cold or unknown residency
- ranking an alias surface as native when the packet marks it limited or routed
