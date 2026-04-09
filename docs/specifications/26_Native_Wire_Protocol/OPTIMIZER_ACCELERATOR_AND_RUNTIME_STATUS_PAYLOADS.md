# Optimizer, Accelerator, and Runtime Status Payloads

## Purpose

This document defines the native wire-protocol payload rules for runtime status
surfaces related to:
- optimizer and index-metrics readiness
- accelerator and resident-index state
- derivative-lane and shadow readiness
- warmup and degraded-state inspection

## Current code-backed authority

Current code-backed native protocol behavior already proves:
- `STATUS_RESPONSE` is encoded as:
  - one `request_type` byte
  - one `entry_count` uint32
  - repeated key/value string pairs
- `ParameterStatus` frames are emitted on the native attach path
- current attach-time status keys include:
  - `profile_contract_version`
  - `server_feature_mask`
  - `profile_capability_mask`
  - `profile_count`
  - `profile_bundle_set`
  - `startup_quarantine_active`
  - `derivative_backpressure_class`
  - `wal_after_backlog_depth`
  - `wal_after_export_failures`
  - `last_wal_after_segments_emitted`
  - `shadow_filespace_count`
  - `shadow_group_ready_members`
  - `shadow_group_required_members`
  - `shadow_group_state`

## Canonical transport rule

ScratchBird shall continue to use existing native status surfaces for runtime
inspection. It shall not invent a second incompatible status-transport family
for optimizer or accelerator state.

The permitted public shapes are:
- attach-time `ParameterStatus` key/value pairs for compact process and runtime
  capability state
- `STATUS_RESPONSE` key/value pairs for bounded runtime inspection requests
- ordinary result sets for tabular, per-index, per-device, or per-candidate
  inspection output

## ParameterStatus scope rule

`ParameterStatus` is for compact, connection-scoped state only.
It is appropriate for:
- capability masks
- startup quarantine state
- derivative-lane health summary
- shadow-group summary
- compact accelerator summary fields

It is not appropriate for large tabular payloads such as:
- one row per index
- one row per optimizer candidate
- one row per accelerator device when the inventory is large

Those larger surfaces must use ordinary result sets or service-query result
rows.

## Required compact status keys

The rebuilt canonical status key set shall include at minimum:
- existing shipped keys
- `optimizer_trace_capable`
- `index_metrics_contract_version`
- `accelerator_inventory_present`
- `accelerator_degraded`
- `resident_index_count`
- `resident_index_degraded_count`
- `warmup_readiness_class`

These are required reconstructed status keys even where code is partial.

## Required STATUS_RESPONSE request families

The status request layer shall support deterministic request families for:
- `RUNTIME_HEALTH_SUMMARY`
- `DERIVATIVE_AND_SHADOW_SUMMARY`
- `ACCELERATOR_SUMMARY`
- `WARMUP_AND_RESIDENCY_SUMMARY`
- `OPTIMIZER_INSPECTION_CAPABILITIES`

Every request family must return a fixed key set defined by the protocol
contract version.

## Required tabular inspection surfaces

The following payloads shall use ordinary result-set semantics rather than
compact key/value status:
- index-family runtime metrics
- optimizer candidate bundle traces
- resident-index inventory
- accelerator device inventory
- workload-governance runtime rows

The row schemas for those surfaces are defined in section `20` and must remain
transport-equivalent when surfaced through the wire protocol.

## Failure rules

The protocol must fail closed when:
- an unknown status request family is issued
- a required status key cannot be produced for the declared contract version
- a tabular inspection request would otherwise be collapsed into lossy key/value
  pairs

## Compatibility rule

Clients and drivers shall negotiate status interpretation by:
- native protocol version
- profile contract version
- explicit capability masks

A client may not assume new runtime status keys are present unless negotiated or
otherwise declared by the connection profile.
