Status: current_authority_with_current_substrate

# Listener Topology Parser Pool and Emulation Binding Catalog Model

## Purpose

This document defines the canonical catalog-owned model for listener topology,
emulation binding, port assignment, and parser-pool policy.

## Canonical Rule

Listener topology is database-controlled. Ports, emulation families,
parser-pool sizes, and related listener runtime policy are not ad hoc
listener-local truth. They are derived from engine-owned configuration and
catalog-backed management state.

The generic scalar configuration catalog must not flatten these families into
synthetic key-value rows after catalog bootstrap. Bootstrap files may seed them,
but durable truth lives in the dedicated row families below.

## Current code-backed authority

Current code now proves:
- physical catalog-root allocation for the dedicated listener-topology row
  families below
- bootstrap reconciliation that seeds missing listener-topology rows from
  bootstrap inputs
- durable upsert and list APIs for listener profile, binding, emulation
  binding, parser-pool policy, runtime-target, and generation-record families

## Catalog Object Families

The canonical object families are:
- listener profile
- listener binding
- emulation binding
- parser-pool policy
- listener runtime target
- listener generation record

## Table: `listener_profile`

Columns:
- `listener_profile_uuid` `[sb_dom]cat_uuid` PK
- `profile_name` `[sb_dom]cat_object_name`
- `protocol_family` `[sb_dom]cat_identifier`
- `enabled` `[sb_dom]cat_bool`
- `manager_fronted` `[sb_dom]cat_bool`
- `owner_database_uuid` `[sb_dom]cat_uuid` nullable
- `desired_state` `[sb_dom]cat_identifier`
- `applied_generation` `[sb_dom]cat_version_u64`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`profile_name`)

## Table: `listener_binding`

Columns:
- `listener_binding_uuid` `[sb_dom]cat_uuid` PK
- `listener_profile_uuid` `[sb_dom]cat_uuid`
- `bind_address` `[sb_dom]cat_host_name`
- `bind_port` `[sb_dom]cat_port_u16`
- `bind_transport` `[sb_dom]cat_identifier`
- `bind_scope` `[sb_dom]cat_identifier`
- `is_primary` `[sb_dom]cat_bool`
- `configuration_generation` `[sb_dom]cat_version_u64`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`bind_address`, `bind_port`, `bind_transport`)
- UNIQUE(`listener_profile_uuid`, `bind_address`, `bind_port`, `bind_transport`)

## Table: `listener_emulation_binding`

Columns:
- `listener_emulation_binding_uuid` `[sb_dom]cat_uuid` PK
- `listener_profile_uuid` `[sb_dom]cat_uuid`
- `emulation_family` `[sb_dom]cat_identifier`
- `protocol_surface` `[sb_dom]cat_identifier`
- `enabled` `[sb_dom]cat_bool`
- `parser_pool_policy_uuid` `[sb_dom]cat_uuid` nullable
- `configuration_generation` `[sb_dom]cat_version_u64`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`listener_profile_uuid`, `emulation_family`)

## Table: `parser_pool_policy`

Columns:
- `parser_pool_policy_uuid` `[sb_dom]cat_uuid` PK
- `policy_name` `[sb_dom]cat_object_name`
- `parser_library_family` `[sb_dom]cat_identifier`
- `min_workers` `[sb_dom]cat_uint16`
- `preferred_workers` `[sb_dom]cat_uint16`
- `max_workers` `[sb_dom]cat_uint16`
- `queue_max` `[sb_dom]cat_uint16`
- `queue_timeout_ms` `[sb_dom]cat_uint64`
- `idle_timeout_ms` `[sb_dom]cat_uint64`
- `spawn_backoff_ms` `[sb_dom]cat_uint64`
- `health_interval_ms` `[sb_dom]cat_uint64`
- `missed_heartbeat_threshold` `[sb_dom]cat_uint16`
- `warm_replenish_timeout_ms` `[sb_dom]cat_uint64`
- `memory_guardrail_bytes` `[sb_dom]cat_uint64`
- `workload_guardrail_class` `[sb_dom]cat_identifier`
- `configuration_generation` `[sb_dom]cat_version_u64`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`policy_name`)
- `min_workers <= preferred_workers <= max_workers`

## Table: `listener_runtime_target`

Columns:
- `listener_runtime_target_uuid` `[sb_dom]cat_uuid` PK
- `listener_profile_uuid` `[sb_dom]cat_uuid`
- `target_kind` `[sb_dom]cat_identifier`
- `target_database_uuid` `[sb_dom]cat_uuid` nullable
- `target_server_uuid` `[sb_dom]cat_uuid` nullable
- `inner_listener_profile_uuid` `[sb_dom]cat_uuid` nullable
- `current_generation` `[sb_dom]cat_version_u64`
- `pending_generation` `[sb_dom]cat_version_u64` nullable
- `last_applied_generation` `[sb_dom]cat_version_u64` nullable
- `last_refused_generation` `[sb_dom]cat_version_u64` nullable
- `last_error_code` `[sb_dom]cat_identifier` nullable
- `last_error_detail_uuid` `[sb_dom]cat_uuid` nullable
- `last_observed_at` `[sb_dom]cat_timestamp` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`listener_profile_uuid`)

## Table: `listener_generation_record`

Columns:
- `listener_generation_uuid` `[sb_dom]cat_uuid` PK
- `target_database_uuid` `[sb_dom]cat_uuid`
- `listener_profile_uuid` `[sb_dom]cat_uuid`
- `committed_generation` `[sb_dom]cat_version_u64`
- `applied_generation` `[sb_dom]cat_version_u64`
- `refused_generation` `[sb_dom]cat_version_u64` nullable
- `drift_state` `[sb_dom]cat_identifier`
- `last_instruction_uuid` `[sb_dom]cat_uuid` nullable
- `observed_at` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`target_database_uuid`, `listener_profile_uuid`)

## Required identifier vocabularies

`protocol_family` values:
- `native`
- `postgresql`
- `mysql`
- `firebird`
- `cassandra`
- `mongodb`
- `neo4j`
- `redis`
- `milvus`
- `management_ipc`

`bind_transport` values:
- `inet`
- `unix`
- `local_ipc`

`bind_scope` values:
- `global`
- `database`
- `manager_inner`
- `management`

`desired_state` values:
- `ENABLED`
- `DISABLED`
- `QUARANTINED`

`target_kind` values:
- `DIRECT_DATABASE`
- `MANAGER_FRONT_DOOR`
- `MANAGER_INNER_NATIVE`
- `MANAGEMENT_IPC`

`drift_state` values:
- `CONSISTENT`
- `PENDING_APPLY`
- `REFUSED`
- `DIVERGED`

## Runtime target rule

The catalog model shall be able to express both:
- inner listener targets behind a manager-owned outer front door
- direct listener targets for deployments without the manager layer

## Generation rule

Every materially changed listener topology or parser-pool policy shall advance
a configuration generation so the controller path can distinguish:
- currently applied generation
- pending generation
- refused generation

The generation written into the affected row family must agree with the
`listener_generation_record` entry published for the same profile and target.

## Bootstrap import rule

Bootstrap file values such as `listener.<family>.port`, `listener.bind_address`,
and `parser.pool.*` are seed inputs only.

On first mount or explicit reconcile:
- seed inputs are translated into the dedicated row families above
- committed dedicated rows become authoritative
- generic scalar config rows must not retain duplicate authority for the same
  bind or parser-pool topology choice

## Non-Guarantees

This file does not require full remote-management orchestration for every
listener topology mutation in current code. It defines the canonical catalog
truth and the currently implemented durable row-family substrate.
