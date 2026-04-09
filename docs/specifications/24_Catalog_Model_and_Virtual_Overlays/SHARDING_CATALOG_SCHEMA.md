# Catalog: Sharding and Cluster Schema

## Purpose
Define canonical catalog tables required to support sharding, cluster membership, and shard placement. These tables are authoritative persisted state for routing, balancing, migration, and emulated engine catalogs.

## Rebuild reconciliation rule

This file is a mixed recovery surface:

- some row families are already current code-backed authority
- some row families remain reconstructed-required specification for the cluster
  lane that was interrupted when specifications were lost

Canonical rule:

1. persisted cluster, node, shard, and placement rows remain the authoritative
   catalog home for cluster topology
2. runtime routing epoch, leader term, write fencing, and shard commit-log
   state do not replace catalog truth; they operate on top of it
3. if a runtime lane is only partially shipped, the catalog requirement still
   stands and implementation drift must be tracked outside canon

## Current code-backed versus reconstructed-required split

### Current code-backed substrate

Current code-backed recovery already proves persisted support for:

- cluster identity
- node rows and heartbeat timestamps
- node role bindings
- node service rows
- failure-detector policy rows
- key-shard custody rows

### Reconstructed-required shard substrate

The broader sharding schema in this file remains canonical required behavior for
the promoted cluster lane, including:

- shard policy
- shard key
- shard scope
- shard range
- shard replica

These definitions remain authoritative even where current runtime code still
implements only a bounded subset of the full cluster topology path.

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.
- `*_toast_uuid` columns reference TOAST values.

## Table: `cluster`
Columns:
- `cluster_uuid` `[sb_dom]cat_cluster_uuid` PK
- `cluster_name` `[sb_dom]cat_identifier`
- `cluster_mode` `[sb_dom]cat_enum_cluster_mode`
- `cluster_state` `[sb_dom]cat_enum_cluster_state`
- `cluster_state_version` `[sb_dom]cat_version_u64`
- `consensus_mode` `[sb_dom]cat_enum_consensus_mode`
- `policy_uuid` `[sb_dom]cat_cluster_policy_uuid` nullable
- `config_version` `[sb_dom]cat_version_u64`
- `created_txid` `[sb_dom]cat_txid`
- `last_modified_txid` `[sb_dom]cat_txid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_state_change_time` `[sb_dom]cat_timestamp`
- `description` `[sb_dom]cat_text` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`cluster_name`)

## Table: `node`
Columns:
- `node_uuid` `[sb_dom]cat_node_uuid` PK
- `cluster_uuid` `[sb_dom]cat_cluster_uuid`
- `node_name` `[sb_dom]cat_identifier`
- `node_role` `[sb_dom]cat_enum_node_role`
- `host` `[sb_dom]cat_host_name`
- `port` `[sb_dom]cat_port_u16`
- `transport` `[sb_dom]cat_enum_transport`
- `region` `[sb_dom]cat_identifier` nullable
- `zone` `[sb_dom]cat_identifier` nullable
- `rack` `[sb_dom]cat_identifier` nullable
- `state` `[sb_dom]cat_enum_node_state`
- `last_heartbeat_time` `[sb_dom]cat_timestamp` nullable
- `created_txid` `[sb_dom]cat_txid`
- `last_modified_txid` `[sb_dom]cat_txid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`cluster_uuid`, `node_name`)

Indexes:
- INDEX(`state`)
- INDEX(`region`, `zone`)

## Table: `sys.node.role_binding`
Columns:
- `binding_uuid` `[sb_dom]cat_node_role_binding_uuid` PK
- `node_uuid` `[sb_dom]cat_node_uuid`
- `role` `[sb_dom]cat_enum_node_role`
- `is_primary` `[sb_dom]cat_bool`
- `created_txid` `[sb_dom]cat_txid`
- `last_modified_txid` `[sb_dom]cat_txid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`node_uuid`, `role`)

## Table: `sys.node.service`
Columns:
- `service_uuid` `[sb_dom]cat_node_service_uuid` PK
- `node_uuid` `[sb_dom]cat_node_uuid`
- `role` `[sb_dom]cat_enum_node_role`
- `service_type` `[sb_dom]cat_enum_service_type`
- `transport` `[sb_dom]cat_enum_transport`
- `host` `[sb_dom]cat_host_name`
- `port` `[sb_dom]cat_port_u16`
- `tls_profile_uuid` `[sb_dom]cat_tls_profile_uuid` nullable
- `auth_profile_uuid` `[sb_dom]cat_auth_profile_uuid` nullable
- `state` `[sb_dom]cat_enum_service_state`
- `created_txid` `[sb_dom]cat_txid`
- `last_modified_txid` `[sb_dom]cat_txid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`node_uuid`, `service_type`, `port`)

Indexes:
- INDEX(`state`)

## Table: `sys.node.capability`
Columns:
- `capability_uuid` `[sb_dom]cat_node_capability_uuid` PK
- `node_uuid` `[sb_dom]cat_node_uuid`
- `capability_key` `[sb_dom]cat_identifier`
- `capability_value` `[sb_dom]cat_text`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`node_uuid`, `capability_key`)

## Table: `shard_policy`
Columns:
- `policy_uuid` `[sb_dom]cat_shard_policy_uuid` PK
- `policy_name` `[sb_dom]cat_identifier`
- `replication_factor` `[sb_dom]cat_uint16`
- `consistency_read` `[sb_dom]cat_enum_consistency_level`
- `consistency_write` `[sb_dom]cat_enum_consistency_level`
- `failover_mode` `[sb_dom]cat_enum_failover_mode`
- `rebalance_mode` `[sb_dom]cat_enum_rebalance_mode`
- `shard_key_required` `[sb_dom]cat_bool`
- `allow_cross_shard_txn` `[sb_dom]cat_bool`
- `default_shard_count` `[sb_dom]cat_uint32`
- `shard_size_target_mb` `[sb_dom]cat_uint32`
- `shard_growth_trigger_pct` `[sb_dom]cat_percent_u8`
- `rebalance_interval_ms` `[sb_dom]cat_interval_ms`
- `created_txid` `[sb_dom]cat_txid`
- `last_modified_txid` `[sb_dom]cat_txid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`policy_name`)

## Table: `shard_policy_param`
Columns:
- `policy_param_uuid` `[sb_dom]cat_shard_policy_param_uuid` PK
- `policy_uuid` `[sb_dom]cat_shard_policy_uuid`
- `param_key` `[sb_dom]cat_identifier`
- `param_type` `[sb_dom]cat_enum_shard_policy_param_type`
- `val_u64` `[sb_dom]cat_uint64` nullable
- `val_i64` `[sb_dom]cat_int64` nullable
- `val_f64` `[sb_dom]cat_f64` nullable
- `val_bool` `[sb_dom]cat_bool` nullable
- `val_text` `[sb_dom]cat_text` nullable
- `val_uuid` `[sb_dom]cat_uuid` nullable
- `val_json` `[sb_dom]cat_json` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`policy_uuid`, `param_key`)
- Exactly one of the `val_*` columns must be non-null and must match `param_type`.

## Table: `shard_key`
Columns:
- `shard_key_uuid` `[sb_dom]cat_shard_key_uuid` PK
- `table_uuid` `[sb_dom]cat_table_uuid`
- `shard_key_kind` `[sb_dom]cat_enum_shard_key_kind`
- `key_columns_uuid` `[sb_dom]cat_shard_key_columns_uuid` nullable
- `key_expression_sblr_uuid` `[sb_dom]cat_shard_key_expr_uuid` nullable
- `hash_function` `[sb_dom]cat_enum_hash_function`
- `partition_count` `[sb_dom]cat_uint32` nullable
- `range_order` `[sb_dom]cat_enum_range_order` nullable
- `key_version` `[sb_dom]cat_uint32`
- `is_active` `[sb_dom]cat_bool`
- `created_txid` `[sb_dom]cat_txid`
- `last_modified_txid` `[sb_dom]cat_txid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- Exactly one of `key_columns_uuid` or `key_expression_sblr_uuid` must be non-null.
- UNIQUE(`table_uuid`, `is_active`) where `is_active=true`.

## Table: `shard`
Columns:
- `shard_uuid` `[sb_dom]cat_shard_uuid` PK
- `shard_name` `[sb_dom]cat_identifier`
- `cluster_uuid` `[sb_dom]cat_cluster_uuid`
- `shard_state` `[sb_dom]cat_enum_shard_state`
- `shard_kind` `[sb_dom]cat_enum_shard_kind`
- `policy_uuid` `[sb_dom]cat_shard_policy_uuid`
- `created_txid` `[sb_dom]cat_txid`
- `last_modified_txid` `[sb_dom]cat_txid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`cluster_uuid`, `shard_name`)

## Table: `shard_scope`
Columns:
- `scope_uuid` `[sb_dom]cat_shard_scope_uuid` PK
- `shard_uuid` `[sb_dom]cat_shard_uuid`
- `object_uuid` `[sb_dom]cat_object_uuid`
- `object_kind` `[sb_dom]cat_enum_object_kind`
- `shard_key_uuid` `[sb_dom]cat_shard_key_uuid` nullable
- `is_primary_scope` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`shard_uuid`, `object_uuid`, `object_kind`)

## Table: `shard_range`
Columns:
- `range_uuid` `[sb_dom]cat_shard_range_uuid` PK
- `shard_uuid` `[sb_dom]cat_shard_uuid`
- `range_kind` `[sb_dom]cat_enum_shard_range_kind`
- `range_type_uuid` `[sb_dom]cat_type_uuid` nullable
- `range_min_bytes` `[sb_dom]cat_blob_binary` nullable
- `range_max_bytes` `[sb_dom]cat_blob_binary` nullable
- `range_min_s64` `[sb_dom]cat_int64` nullable
- `range_max_s64` `[sb_dom]cat_int64` nullable
- `inclusive_min` `[sb_dom]cat_bool`
- `inclusive_max` `[sb_dom]cat_bool`
- `hash_bucket` `[sb_dom]cat_uint32` nullable
- `zone_tag` `[sb_dom]cat_identifier` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- For `range_kind=TOKEN` or `HASH_BUCKET`, use `range_min_s64`/`range_max_s64` and leave `range_min_bytes`/`range_max_bytes` NULL.
- For `range_kind=BYTES` or `GEO`, use `range_min_bytes`/`range_max_bytes` and leave `range_min_s64`/`range_max_s64` NULL.
- For `range_kind=HASH_BUCKET`, `hash_bucket` MUST be non-null and `range_min_*`/`range_max_*` MUST be NULL.

## Table: `shard_replica`
Columns:
- `replica_uuid` `[sb_dom]cat_shard_replica_uuid` PK
- `shard_uuid` `[sb_dom]cat_shard_uuid`
- `node_uuid` `[sb_dom]cat_node_uuid`
- `replica_role` `[sb_dom]cat_enum_replica_role`
- `replica_state` `[sb_dom]cat_enum_replica_state`
- `last_applied_txid` `[sb_dom]cat_txid` nullable
- `last_sync_time` `[sb_dom]cat_timestamp` nullable
- `is_voting` `[sb_dom]cat_bool`
- `weight` `[sb_dom]cat_weight_u8`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`shard_uuid`, `node_uuid`)

Indexes:
- INDEX(`replica_state`)

## Table: `shard_migration`
Columns:
- `migration_uuid` `[sb_dom]cat_shard_migration_uuid` PK
- `shard_uuid` `[sb_dom]cat_shard_uuid`
- `source_node_uuid` `[sb_dom]cat_node_uuid`
- `target_node_uuid` `[sb_dom]cat_node_uuid`
- `state` `[sb_dom]cat_enum_migration_state`
- `bytes_total` `[sb_dom]cat_bytes_u64`
- `bytes_copied` `[sb_dom]cat_bytes_u64`
- `rows_total` `[sb_dom]cat_count_u64`
- `rows_copied` `[sb_dom]cat_count_u64`
- `throttle_state` `[sb_dom]cat_enum_throttle_state`
- `started_time` `[sb_dom]cat_timestamp`
- `updated_time` `[sb_dom]cat_timestamp`
- `completed_time` `[sb_dom]cat_timestamp` nullable
- `error_code` `[sb_dom]cat_identifier` nullable
- `error_message` `[sb_dom]cat_text` nullable
- `is_valid` `[sb_dom]cat_bool`

Indexes:
- INDEX(`state`)

## Table: `shard_zone`
Columns:
- `zone_uuid` `[sb_dom]cat_shard_zone_uuid` PK
- `zone_name` `[sb_dom]cat_identifier`
- `description` `[sb_dom]cat_text` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`zone_name`)

## Table: `shard_zone_range`
Columns:
- `zone_range_uuid` `[sb_dom]cat_shard_zone_range_uuid` PK
- `zone_uuid` `[sb_dom]cat_shard_zone_uuid`
- `range_uuid` `[sb_dom]cat_shard_range_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`zone_uuid`, `range_uuid`)

## Runtime Views
- `sys.shard_status` derived from shard and node health.
- `sys.shard_migrations` derived from `shard_migration` with live counters.

## Emulated Catalog Coverage
- MongoDB config.* maps to `shard`, `shard_range`, `shard_zone_range`, `table`, `database`.
- Cassandra system.peers maps to `node` and `shard_replica`.

## Test Contract
- Routing can be reconstructed from shard catalogs.
- Shard migration transitions are persisted and resumable.
- Emulated catalogs populate from canonical data.
