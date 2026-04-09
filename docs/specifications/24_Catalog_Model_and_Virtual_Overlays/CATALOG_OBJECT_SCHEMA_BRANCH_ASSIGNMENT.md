# Catalog Object Schema Branch Assignment

## Purpose
Assign every ScratchBird catalog object to a canonical schema branch so placement is deterministic and implementation has no ambiguity.

## Scope
- On-disk catalog tables.
- Virtual or in-memory system views.
- Security, monitoring, cluster, NoSQL, and emulation-related catalog objects.

## Placement Invariants
1. Every catalog object has exactly one canonical branch path.
2. Branch assignment is independent from parser dialect naming and wire protocol behavior.
3. Emulated parser visibility is profile-gated and may expose overlays, not raw internal branches.
4. Objects under `root.sys.information` are read-only views/materializations and must not be direct write targets.
5. Objects under `root.sys.security` are policy-critical and require security-context enforcement.

## Canonical Branch Taxonomy
- `root.sys.system.catalog.core`: database, schema, object identity registry.
- `root.sys.system.catalog.relations`: table/column/constraint/view/trigger/partition metadata.
- `root.sys.system.catalog.types`: types, domains, operators, charset/collation/timezone.
- `root.sys.system.catalog.code`: procedures, packages, exceptions, UDR metadata.
- `root.sys.system.catalog.index`: index definitions and index lifecycle metadata.
- `root.sys.system.catalog.storage`: filespace, LOB, and backup metadata.
- `root.sys.system.catalog.integration`: emulation profiles, reserved words, FDW and remote bindings.
- `root.sys.system.catalog.replication`: extension/publication/subscription metadata.
- `root.sys.system.catalog.engine_specific`: engine-specific metadata required for compatibility.
- `root.sys.system.catalog.cluster`: cluster and shard topology metadata.
- `root.sys.system.catalog.cluster.routing`: workload routing and admission metadata.
- `root.sys.node.catalog.identity`: node identity and role/service bindings.
- `root.sys.node.catalog.lifecycle`: node bootstrap, lifecycle, and clock-state metadata.
- `root.sys.node.catalog.capacity`: node capacity profiles and capability metadata.
- `root.sys.system.catalog.olap`: OLAP and cube metadata.
- `root.sys.system.catalog.text_search`: text search parser/dictionary/config metadata.
- `root.sys.system.catalog.admin`: creator/provisioning metadata.
- `root.sys.security.catalog.principals`: users, roles, groups, memberships.
- `root.sys.security.catalog.authorization`: permissions, policies, security epochs.
- `root.sys.security.catalog.mapping`: auth and group mapping metadata.
- `root.sys.security.catalog.auth`: auth key metadata.
- `root.sys.security.catalog.pki`: certificate, trust anchor, and revocation metadata.
- `root.sys.security.catalog.crypto`: encryption profile, key, and key-shard metadata.
- `root.sys.security.catalog.sessions`: security session context metadata.
- `root.sys.security.catalog.audit`: audit trail metadata.
- `root.sys.config.catalog`: configuration key/value/change-log metadata.
- `root.sys.monitor.catalog.runtime`: connection and transaction runtime attribution.
- `root.sys.monitor.catalog.metrics`: optimizer, diagnostics, and emulated metric storage.
- `root.sys.monitor.catalog.incident`: failure detection, alerts, partitions, healing metadata.
- `root.sys.jobs.catalog.scheduler`: job types, schedules, runs, and secrets.
- `root.sys.information.views.*`: read-only system and compatibility views.
- `root.sys.information.views.integration`: read-only integration and migration status overlays.

## Resolved Object-to-Branch Map

| Catalog Object | Kind | Canonical Branch | Exposure |
| --- | --- | --- | --- |
| `admission_binding` | `table` | `root.sys.system.catalog.cluster.routing` | `system_internal` |
| `admission_policy` | `table` | `root.sys.system.catalog.cluster.routing` | `system_internal` |
| `admission_tuning_event` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `alert_ack` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `alert_event` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `alert_route` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `alert_rule` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `alert_silence` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `alert_target` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `audit_log` | `table` | `root.sys.security.catalog.audit` | `security_controlled` |
| `auth_mapping` | `table` | `root.sys.security.catalog.mapping` | `security_controlled` |
| `authkey` | `table` | `root.sys.security.catalog.auth` | `security_controlled` |
| `autoscale_action` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `autoscale_policy` | `table` | `root.sys.system.catalog.cluster.routing` | `system_internal` |
| `backup_history` | `table` | `root.sys.system.catalog.storage` | `system_internal` |
| `blob_filter` | `table` | `root.sys.system.catalog.engine_specific` | `parser_profile_gated` |
| `charset` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `check_constraint` | `view` | `root.sys.system.catalog.relations` | `system_internal` |
| `cluster` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `cluster_policy` | `table` | `root.sys.system.catalog.cluster.routing` | `system_internal` |
| `clock_policy` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `clock_source` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `clock_violation_event` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `collation` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `column` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `column_drop_history` | `table` | `root.sys.system.catalog.engine_specific` | `parser_profile_gated` |
| `column_permission` | `table` | `root.sys.security.catalog.authorization` | `security_controlled` |
| `column_stats` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `connection` | `table` | `root.sys.monitor.catalog.runtime` | `monitoring_controlled` |
| `cube` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `cube_dimension` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `cube_hierarchy` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `cube_hierarchy_level` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `cube_job` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `cube_job_step` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `cube_level` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `cube_materialization` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `cube_measure` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `cube_refresh_policy` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `cube_stats` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `database` | `table` | `root.sys.system.catalog.core` | `system_internal` |
| `db_creator` | `table` | `root.sys.system.catalog.admin` | `system_internal` |
| `default_privilege` | `table` | `root.sys.security.catalog.authorization` | `security_controlled` |
| `dependency` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `domain` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `domain_constraint` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `domain_integrity` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `domain_param_key` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `domain_parameter` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `domain_security` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `domain_validation` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `dormant_transaction` | `table` | `root.sys.monitor.catalog.runtime` | `monitoring_controlled` |
| `emulated_database` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `emulated_stat_def` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `emulated_stat_map` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `emulated_stat_value` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `emulation_profile` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `emulation_server` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `emulation_type` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `parser_profile` | `table` | `root.sys.system.catalog.integration` | `system_internal` |
| `parser_capability_entry` | `table` | `root.sys.system.catalog.integration` | `system_internal` |
| `parser_transform_entry` | `table` | `root.sys.system.catalog.integration` | `system_internal` |
| `parser_error_map_entry` | `table` | `root.sys.system.catalog.integration` | `system_internal` |
| `parser_feature_precedence` | `table` | `root.sys.system.catalog.integration` | `system_internal` |
| `encoding_conversion` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `sys.security.cert_registry` | `table` | `root.sys.security.catalog.pki` | `security_controlled` |
| `sys.security.private_key_store` | `table` | `root.sys.security.catalog.pki` | `security_controlled` |
| `sys.security.trust_anchor` | `table` | `root.sys.security.catalog.pki` | `security_controlled` |
| `sys.security.channel_cert_binding` | `table` | `root.sys.security.catalog.pki` | `security_controlled` |
| `sys.security.cert_revocation` | `table` | `root.sys.security.catalog.pki` | `security_controlled` |
| `sys.security.pki_distribution_state` | `table` | `root.sys.security.catalog.pki` | `security_controlled` |
| `sys.security.trust_anchor_rollover` | `table` | `root.sys.security.catalog.pki` | `security_controlled` |
| `sys.security.encryption_profile` | `table` | `root.sys.security.catalog.crypto` | `security_controlled` |
| `sys.security.encryption_key` | `table` | `root.sys.security.catalog.crypto` | `security_controlled` |
| `sys.security.encryption_key_shard` | `table` | `root.sys.security.catalog.crypto` | `security_controlled` |
| `sys.security.encryption_bootstrap_info` | `table` | `root.sys.security.catalog.crypto` | `security_controlled` |
| `event` | `table` | `root.sys.system.catalog.code` | `system_internal` |
| `exception` | `table` | `root.sys.system.catalog.code` | `system_internal` |
| `extension` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `failure_detector` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `fdw` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `fdw_server` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `fdw_user_mapping` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `migration_apply_audit` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `migration_cursor` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `migration_error` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `migration_event` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `migration_job` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `migration_object_map` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `migration_read_audit` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `filespace_files` | `table` | `root.sys.system.catalog.storage` | `system_internal` |
| `filespace_stats` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `filespaces` | `table` | `root.sys.system.catalog.storage` | `system_internal` |
| `fk_constraint` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `foreign_table` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `function` | `view` | `root.sys.information.views.code` | `read_only_view` |
| `function_param` | `view` | `root.sys.information.views.code` | `read_only_view` |
| `group` | `table` | `root.sys.security.catalog.principals` | `security_controlled` |
| `group_mapping` | `table` | `root.sys.security.catalog.mapping` | `security_controlled` |
| `group_membership` | `table` | `root.sys.security.catalog.principals` | `security_controlled` |
| `home_schema_binding` | `table` | `root.sys.security.catalog.principals` | `security_controlled` |
| `healing_action` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `healing_action_param` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `healing_policy` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `healing_run` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `healing_step` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `index` | `table` | `root.sys.system.catalog.index` | `system_internal` |
| `index_access_method` | `table` | `root.sys.system.catalog.index` | `system_internal` |
| `index_build_delta` | `table` | `root.sys.system.catalog.index` | `system_internal` |
| `index_column` | `table` | `root.sys.system.catalog.index` | `system_internal` |
| `index_contention` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `index_health` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `index_maintenance` | `table` | `root.sys.system.catalog.index` | `system_internal` |
| `index_maintenance_delta` | `table` | `root.sys.system.catalog.index` | `system_internal` |
| `index_opclass` | `table` | `root.sys.system.catalog.index` | `system_internal` |
| `index_opclass_fn` | `table` | `root.sys.system.catalog.index` | `system_internal` |
| `index_option` | `table` | `root.sys.system.catalog.index` | `system_internal` |
| `index_stats` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `index_storage` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `index_usage` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `job` | `table` | `root.sys.jobs.catalog.scheduler` | `scheduler_controlled` |
| `job_dependency` | `table` | `root.sys.jobs.catalog.scheduler` | `scheduler_controlled` |
| `job_param` | `table` | `root.sys.jobs.catalog.scheduler` | `scheduler_controlled` |
| `job_run` | `table` | `root.sys.jobs.catalog.scheduler` | `scheduler_controlled` |
| `job_schedule` | `table` | `root.sys.jobs.catalog.scheduler` | `scheduler_controlled` |
| `job_secret` | `table` | `root.sys.jobs.catalog.scheduler` | `scheduler_controlled` |
| `job_type` | `table` | `root.sys.jobs.catalog.scheduler` | `scheduler_controlled` |
| `job_type_param` | `table` | `root.sys.jobs.catalog.scheduler` | `scheduler_controlled` |
| `job_type_policy` | `table` | `root.sys.jobs.catalog.scheduler` | `scheduler_controlled` |
| `language` | `table` | `root.sys.system.catalog.code` | `system_internal` |
| `lob` | `table` | `root.sys.system.catalog.storage` | `system_internal` |
| `lob_page` | `table` | `root.sys.system.catalog.storage` | `system_internal` |
| `migration_history` | `table` | `root.sys.system.catalog.storage` | `system_internal` |
| `network_partition_event` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `network_partition_member` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `sys.node.node` | `table` | `root.sys.node.catalog.identity` | `system_internal` |
| `sys.node.bootstrap_token` | `table` | `root.sys.node.catalog.lifecycle` | `system_internal` |
| `sys.node.capability` | `table` | `root.sys.node.catalog.capacity` | `system_internal` |
| `sys.node.capacity_profile` | `table` | `root.sys.node.catalog.capacity` | `system_internal` |
| `sys.node.clock_state` | `table` | `root.sys.node.catalog.lifecycle` | `system_internal` |
| `sys.node.lifecycle_event` | `table` | `root.sys.node.catalog.lifecycle` | `system_internal` |
| `sys.node.role_binding` | `table` | `root.sys.node.catalog.identity` | `system_internal` |
| `sys.node.service` | `table` | `root.sys.node.catalog.identity` | `system_internal` |
| `object_comment` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `object_definition` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `object_name` | `table` | `root.sys.system.catalog.core` | `system_internal` |
| `object_permission` | `table` | `root.sys.security.catalog.authorization` | `security_controlled` |
| `object` | `table` | `root.sys.system.catalog.core` | `system_internal` |
| `olap_ingest_log` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `olap_partition` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `olap_segment` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `olap_watermark` | `table` | `root.sys.system.catalog.olap` | `system_internal` |
| `operator` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `package` | `table` | `root.sys.system.catalog.code` | `system_internal` |
| `package_member` | `table` | `root.sys.system.catalog.code` | `system_internal` |
| `partition` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `partitioned_table` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `permission` | `table` | `root.sys.security.catalog.authorization` | `security_controlled` |
| `policy` | `table` | `root.sys.security.catalog.authorization` | `security_controlled` |
| `prepared_transaction` | `table` | `root.sys.monitor.catalog.runtime` | `monitoring_controlled` |
| `procedure` | `table` | `root.sys.system.catalog.code` | `system_internal` |
| `procedure_param` | `table` | `root.sys.system.catalog.code` | `system_internal` |
| `publication` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `publication_schema` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `publication_table` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_apply_log` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_channel` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_channel_member` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_conflict` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_cursor` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_error` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_origin` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_origin_progress` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_retry_queue` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_split_brain_event` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `replication_txn_batch` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `reserved_words` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `role` | `table` | `root.sys.security.catalog.principals` | `security_controlled` |
| `role_membership` | `table` | `root.sys.security.catalog.principals` | `security_controlled` |
| `role_setting` | `table` | `root.sys.security.catalog.authorization` | `security_controlled` |
| `rule` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `scan_reports` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `schema` | `table` | `root.sys.system.catalog.core` | `system_internal` |
| `search_path_entry` | `table` | `root.sys.security.catalog.sessions` | `security_controlled` |
| `search_path_profile` | `table` | `root.sys.security.catalog.sessions` | `security_controlled` |
| `security_class` | `table` | `root.sys.security.catalog.authorization` | `security_controlled` |
| `security_label` | `table` | `root.sys.security.catalog.authorization` | `security_controlled` |
| `security_policy_epoch` | `table` | `root.sys.security.catalog.authorization` | `security_controlled` |
| `sequence` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `server_registry` | `table` | `root.sys.system.catalog.integration` | `parser_profile_gated` |
| `session` | `table` | `root.sys.security.catalog.sessions` | `security_controlled` |
| `shard` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `shard_key` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `shard_migration` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `shard_policy` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `shard_policy_param` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `shard_range` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `shard_replica` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `shard_scope` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `shard_zone` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `shard_zone_range` | `table` | `root.sys.system.catalog.cluster` | `system_internal` |
| `slo_binding` | `table` | `root.sys.system.catalog.cluster.routing` | `system_internal` |
| `slo_burn_event` | `table` | `root.sys.monitor.catalog.incident` | `monitoring_controlled` |
| `slo_profile` | `table` | `root.sys.system.catalog.cluster.routing` | `system_internal` |
| `slo_window` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `storage_engine` | `view` | `root.sys.information.views.system` | `read_only_view` |
| `subscription` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `subscription_table` | `table` | `root.sys.system.catalog.replication` | `parser_profile_gated` |
| `synonym` | `table` | `root.sys.system.catalog.core` | `system_internal` |
| `table` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `table_constraint` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `table_inheritance` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `table_stats` | `table` | `root.sys.monitor.catalog.metrics` | `monitoring_controlled` |
| `timezone` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `transaction` | `table` | `root.sys.monitor.catalog.runtime` | `monitoring_controlled` |
| `trigger` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `trigger_message` | `table` | `root.sys.system.catalog.engine_specific` | `parser_profile_gated` |
| `ts_config` | `table` | `root.sys.system.catalog.text_search` | `system_internal` |
| `ts_config_map` | `table` | `root.sys.system.catalog.text_search` | `system_internal` |
| `ts_dictionary` | `table` | `root.sys.system.catalog.text_search` | `system_internal` |
| `ts_parser` | `table` | `root.sys.system.catalog.text_search` | `system_internal` |
| `ts_template` | `table` | `root.sys.system.catalog.text_search` | `system_internal` |
| `type` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `type_cast` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `type_io` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `type_modifier` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `type_transform` | `table` | `root.sys.system.catalog.types` | `system_internal` |
| `udr` | `table` | `root.sys.system.catalog.code` | `system_internal` |
| `udr_engine` | `table` | `root.sys.system.catalog.code` | `system_internal` |
| `udr_module` | `table` | `root.sys.system.catalog.code` | `system_internal` |
| `user` | `table` | `root.sys.security.catalog.principals` | `security_controlled` |
| `view` | `table` | `root.sys.system.catalog.relations` | `system_internal` |
| `workload_class` | `table` | `root.sys.system.catalog.cluster.routing` | `system_internal` |
| `workload_route` | `table` | `root.sys.system.catalog.cluster.routing` | `system_internal` |
| `sys.config.change_log` | `table` | `root.sys.config.catalog` | `system_internal` |
| `sys.config.key` | `table` | `root.sys.config.catalog` | `system_internal` |
| `sys.config.value` | `table` | `root.sys.config.catalog` | `system_internal` |
| `sys.migration_audit_summary` | `view` | `root.sys.information.views.integration` | `read_only_view` |
| `sys.migration_status` | `view` | `root.sys.information.views.integration` | `read_only_view` |
| `sys.replication_channel_status` | `view` | `root.sys.information.views.integration` | `read_only_view` |
| `sys.replication_conflict_queue` | `view` | `root.sys.information.views.integration` | `read_only_view` |
| `sys.replication_cursor_status` | `view` | `root.sys.information.views.integration` | `read_only_view` |
| `sys.plugin` | `view` | `root.sys.information.views.system` | `read_only_view` |
| `sys.prepared_statement` | `view` | `root.sys.information.views.runtime` | `read_only_view` |
| `sys.shard_migrations` | `view` | `root.sys.information.views.cluster` | `read_only_view` |
| `sys.shard_status` | `view` | `root.sys.information.views.cluster` | `read_only_view` |

## Validation Rule
- Any catalog object not mapped to a canonical branch is a specification error and blocks implementation.
- `UNASSIGNED` entries are not allowed.

## Compatibility Note
- Emulated system catalogs are exposed as overlay views and must source from objects placed in the branches above.
- Native parser may expose broader metadata, but must preserve branch ACL and visibility constraints.

## Resolved Review Decisions
1. Node-family catalogs are normalized under `root.sys.node.*` and referenced as `sys.node.<object_name>`.
2. `cluster_policy` remains in `root.sys.system.catalog.cluster.routing`.
3. `session` remains in `root.sys.security.catalog.sessions`.
