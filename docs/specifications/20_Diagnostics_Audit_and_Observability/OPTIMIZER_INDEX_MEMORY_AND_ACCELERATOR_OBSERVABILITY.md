# Optimizer, Index, Memory, and Accelerator Observability

## Purpose

This document defines the required operator-visible observability surfaces for:
- optimizer candidate selection
- index-family metrics and queryability
- host-memory residency and buffer pressure
- accelerator device health, admission, and resident-index state

This file complements existing MGA, writeback, recovery, and buffer-pool views.
It does not redefine authoritative truth. It defines mandatory inspection
surfaces.

## Current code-backed authority

Current code-backed observability already proves or strongly anchors:
- canonical MGA and recovery SQL views for:
  - buffer pool stats
  - buffer-domain stats
  - buffer policy health
  - prefetch health
  - checkpoint and writeback pressure
  - recovery status and incidents
  - transaction history and snapshot blockers
- support-bundle collection for:
  - SLO status
  - error-budget status
  - admission tuning
- optimizer traces and execution summaries that already carry:
  - candidate budgets
  - chosen path traces
  - resource governance outcome
  - memory budget bytes
  - workload profile
  - search budget
- index-family metrics packets consumed by the planner

## Required canonical public views

The following views are required canonical observability surfaces.
If the physical implementation chooses an alternate export path, it must remain
row-equivalent to these schemas.

### `sb_index_family_runtime_metrics`

Purpose:
- expose the exact planner-facing index metrics envelope and its freshness state

Required columns:
- `db_uuid`
- `relation_uuid`
- `index_uuid`
- `index_name`
- `declared_index_type`
- `runtime_family`
- `alias_surface`
- `native_metrics_mode`
- `semantic_contract_state`
- `queryability_state`
- `confidence_class`
- `requires_fail_closed_stronger_semantics`
- `stats_signature`
- `shared_metrics_json`
- `native_runtime_metrics_json`
- `last_refresh_ms`
- `staleness_ms`

### `sb_optimizer_candidate_bundle_trace`

Purpose:
- expose the planner bundle and rejection model so no usable secondary index is
  silently ignored

Required columns:
- `plan_trace_uuid`
- `statement_uuid`
- `relation_uuid`
- `bundle_stage`
- `candidate_budget`
- `candidate_count`
- `chosen_family_signature`
- `chosen_index_uuid`
- `rejected_count`
- `rejection_reasons_json`
- `parameterized_required`
- `ordering_required`
- `text_ranking_required`
- `ann_order_required`
- `observed_at_ms`

### `sb_workload_governance_runtime`

Purpose:
- expose current governance outcomes, including accelerator overlays

Required columns:
- `scope`
- `class_name`
- `policy_name`
- `binding_priority`
- `queue_depth`
- `active_queries`
- `queued_queries`
- `resource_tag`
- `accelerator_profile`
- `accelerator_required`
- `fallback_allowed`
- `last_tuning_event_id`
- `observed_at_ms`

### `sb_accelerator_runtime_status`

Purpose:
- expose accelerator inventory and health

Required columns:
- `device_kind`
- `device_id`
- `driver_identity`
- `driver_version`
- `runtime_identity`
- `runtime_version`
- `health_state`
- `degraded_reason`
- `total_memory_bytes`
- `reserved_memory_bytes`
- `free_memory_bytes`
- `active_searches`
- `active_builds`
- `queued_searches`
- `queued_builds`
- `resident_indexes`
- `forced_fallbacks_total`
- `last_heartbeat_ms`

### `sb_resident_index_status`

Purpose:
- expose in-memory or device-resident derivative index state

Required columns:
- `relation_uuid`
- `index_uuid`
- `index_name`
- `runtime_family`
- `resident_class`
- `residency_location`
- `warmup_policy`
- `warmup_state`
- `dirty_refresh_pending`
- `resident_bytes`
- `last_load_ms`
- `last_refresh_ms`
- `last_eviction_ms`
- `last_failure_reason`

## Field semantics

The following fields are normative:
- `runtime_family`:
  - the planner-effective family, not only the declared syntax label
- `alias_surface`:
  - the declared surface name when it routes through another family
- `native_metrics_mode`:
  - whether the payload is family-native, routed, heuristic, or absent
- `semantic_contract_state`:
  - whether the runtime family fully satisfies the declared surface contract,
    only partially satisfies it, or is fail-closed
- `queryability_state`:
  - whether the planner may use the family directly, in limited form, or not at
    all
- `resident_class`:
  - `HOST_MEMORY`, `DEVICE_MEMORY`, `HYBRID_HOST_DEVICE`, or `NOT_RESIDENT`

## Required correlation rules

The observability layer shall allow an operator to correlate:
- a chosen plan
- the candidate bundle it was chosen from
- the index metrics packet seen by the planner
- the governance outcome applied to that statement
- the residency or accelerator state of the chosen family

These surfaces may be row-based SQL views, structured status payloads, support
bundle exports, or all three. They must remain schema-equivalent.

## Mandatory fail-closed behavior

The system shall refuse to pretend a family is fully observable when its public
metrics are only heuristic.

When family-native metrics are missing:
- `native_metrics_mode` must say so
- `semantic_contract_state` must say so
- `queryability_state` may be downgraded to `LIMITED`
- the planner may not silently treat the family as fully instrumented

## Operator guarantees

Operators must be able to answer these questions without source inspection:
- why a usable index family was or was not considered
- whether the planner saw native metrics or heuristic metrics
- whether a vector or ANN path ran on CPU or accelerator
- whether a resident index was warm, cold, loading, or degraded
- whether poor latency came from MGA durability pressure, host-memory pressure,
  governance pressure, or accelerator pressure

## Improvement findings preserved during recovery

The rebuild identifies the following concrete improvement candidates:
- make candidate-bundle traces queryable without relying on executor text traces
- surface family-native metric freshness and sampling confidence directly in SQL
- add first-class accelerator health and resident-index views instead of leaving
  the state implicit in runtime internals
- correlate optimizer bundle rejection with workload-governance and memory
  pressure in a single support-bundle export
