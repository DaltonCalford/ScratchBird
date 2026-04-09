# Index Metrics Packet Schema and Queryability Model

Status: current_authority

## Purpose

This file defines the packet and JSON payload structure used to publish planner-visible index metrics.

## Packet fields

The current packet includes at least:
- `index_id`
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

## Queryability states

Current packet semantics recognize:
- `QUERYABLE`
- `LIMITED`
- `INVALID`
- `UNKNOWN`

Alias or routed surfaces may be downgraded from `QUERYABLE` to `LIMITED` even when the underlying runtime family is functional.

## Confidence classes

The current packet includes metrics confidence classes. Commercial-grade consumers must treat confidence as a cost-shaping and diagnostics-shaping input, not a cosmetic field.

## Payload envelope

The JSON payload must contain a `shared_metrics_envelope` object carrying:
- index UUID
- physical family
- runtime family
- planner family
- alias-surface flag
- native metrics mode
- semantic contract state
- stronger-semantics fail-closed flag
- queryability state
- metrics refresh xid
- confidence class
- leaf pages
- height
- row count estimate
- live entry estimate
- dead fraction
- bloat ratio
- recheck ratio
- correlation
- coverage fraction
- maintenance backlog
- publish lag
- reclaim lag

The payload also carries:
- `family_metrics_type`
- `family_metrics`
- `family_metrics.native_runtime_metrics` when native runtime counters are available

## Refresh rules

Metrics refresh must:
1. read the current catalog index row and stats row
2. preserve or update packet fields appropriately
3. rebuild the shared envelope
4. merge in `native_runtime_metrics` when available
5. upsert the refreshed stats row back into catalog state
6. refresh the in-memory packet cache

## Load rules

A packet load fails closed if:
- catalog manager is unavailable
- stats row is unavailable
- packet version is zero
- payload is absent when the packet is expected to be published

## Commercial-grade rule

Every supported index family must publish a valid packet and payload. “Supported but no published packet” is non-conforming.
