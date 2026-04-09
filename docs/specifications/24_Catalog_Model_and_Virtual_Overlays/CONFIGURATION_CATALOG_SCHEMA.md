# Configuration Catalog Schema

## Purpose
Define canonical scalar configuration catalog tables and the bootstrap-import
boundary for configuration management.

## Authority split

`sys.config.*` owns scalar catalog-managed settings only.

The following families do not live in generic key-value rows after catalog
bootstrap:
- listener profile identity
- bind address and port tuples
- emulation bindings
- parser-pool policy rows
- runtime-target rows
- listener generation or drift rows

Those families are owned by
`LISTENER_TOPOLOGY_PARSER_POOL_AND_EMULATION_BINDING_CATALOG_MODEL.md`.

Bootstrap files may still carry seed names for both scalar and topology
settings, but import must translate topology values into the dedicated
listener-topology tables rather than persisting them as synthetic generic keys.

## Canonical Tables

### `sys.config.key`
- `key_id` u32 PK
- `key_name` STRING unique
- `value_type` enum (bool, int64, uint64, float64, string, duration, bytes,
  list_string, list_kv)
- `scope` enum (instance, database, schema, user, session)
- `default_value` STRING
- `min_value` STRING nullable
- `max_value` STRING nullable
- `allowed_values` STRING nullable
- `is_restart_required` bool
- `is_mutable` bool
- `is_bootstrap_only` bool
- `is_cluster_managed` bool
- `hot_apply_class` enum (none, post_commit_local, post_commit_cluster_dispatch)
- `is_sensitive` bool
- `description` STRING

### `sys.config.value`
- `config_value_uuid` UUID PK
- `key_id` u32 FK
- `scope_uuid` UUID nullable
- `value_text` STRING
- `source` enum (catalog, bootstrap, session_override)
- `config_generation` UINT64
- `effective_txid` u64
- `pending_restart` bool
- `updated_by` UUID
- `updated_at` TIMESTAMP
- `is_valid` bool

Constraints:
- UNIQUE(`key_id`, `scope_uuid`)

### `sys.config.change_log`
- `change_id` u64 PK
- `key_id` u32
- `scope_uuid` UUID nullable
- `old_value_text` STRING
- `new_value_text` STRING
- `change_reason` STRING nullable
- `config_generation` UINT64
- `changed_by` UUID
- `changed_at` TIMESTAMP

## Mapping to Emulated Catalogs
- Expose configuration views only through the native dialect.
- Emulated parsers MUST NOT expose these tables directly.

## Persistence
- Tables are stored as canonical system catalog tables.
- On bootstrap, only a minimal set of scalar keys is loaded from file.
- After mount, scalar catalog values become authoritative.
- Listener topology, emulation binding, and parser-pool records become
  authoritative through their dedicated tables, not through `sys.config.value`.

## Resolved Decisions

Required generic scalar catalog keys for Alpha are:
- `engine.database_root`
- `engine.database_uuid`
- `engine.mode` (`embedded|ipc_server`)
- `storage.default_filespace_path`
- `storage.default_page_size`
- `storage.filespace.autoextend`
- `storage.filespace.extend_chunk_pages`
- `ipc.enabled`
- `ipc.bind_address`
- `ipc.port`
- `listener.max_connections_total`
- `listener.accept_backlog`
- `listener.assignment_timeout_ms`
- `listener.handshake_timeout_ms`
- `listener.idle_connection_timeout_ms`
- `listener.reject_when_no_open_database`
- `audit.track_session_context`
- `audit.track_connection_context`
- `logging.level`
- `logging.path`
- `diagnostics.metrics.enabled`
- `security.auth_methods`
- `security.mfa.enabled`
- `security.encryption.enabled`
- `i18n.resource_bundle_path`
- `i18n.bootstrap_required`
- `i18n.bundle_update_mode`
- `timezone.resource_bundle_path`
- `timezone.default_name`
- `types.default_charset`
- `types.default_collation`

Bootstrap import-only topology names that MUST seed dedicated listener rows are:
- `listener.native.enabled`
- `listener.native.port`
- `listener.postgresql.enabled`
- `listener.postgresql.port`
- `listener.mysql.enabled`
- `listener.mysql.port`
- `listener.firebird.enabled`
- `listener.firebird.port`
- `listener.cassandra.enabled`
- `listener.cassandra.port`
- `listener.mongodb.enabled`
- `listener.mongodb.port`
- `listener.neo4j.enabled`
- `listener.neo4j.port`
- `listener.redis.enabled`
- `listener.redis.port`
- `listener.milvus.enabled`
- `listener.milvus.port`
- `listener.bind_address`
- `listener.mgmt_ipc.enabled`
- `listener.mgmt_ipc.bind_address`
- `listener.mgmt_ipc.port`
- `parser.pool.min`
- `parser.pool.max`
- `parser.pool.queue_max`
- `parser.pool.queue_timeout_ms`
- `parser.pool.idle_timeout_ms`
- `parser.pool.spawn_backoff_ms`
- `parser.pool.health_interval_ms`
- `parser.pool.missed_heartbeat_threshold`
- `parser.pool.warm_replenish_timeout_ms`

Keys not listed above are non-authoritative until added by spec change control
in section 00.

## Open Questions
- None.
