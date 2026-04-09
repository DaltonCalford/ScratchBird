# Catalog: Enum Registry

## Purpose
Define canonical enum values used by catalog tables. These values are part of the on-disk contract and MUST NOT be renumbered without a catalog migration.

## Enum: object_type
Values:
- 0 `SCHEMA`
- 1 `TABLE`
- 2 `COLUMN`
- 3 `INDEX`
- 4 `VIEW`
- 5 `SEQUENCE`
- 6 `CONSTRAINT`
- 7 `TRIGGER`
- 8 `PROCEDURE`
- 9 `FUNCTION`
- 10 `DOMAIN`
- 11 `COMPOSITE_TYPE`
- 12 `ROLE`
- 13 `USER`
- 14 `GROUP`
- 15 `FILESPACE` (legacy code uses TABLESPACE)
- 16 `DATABASE`
- 17 `EMULATION_TYPE`
- 18 `EMULATION_SERVER`
- 19 `EMULATED_DATABASE`
- 20 `COLLATION`
- 21 `CHARSET`
- 22 `PACKAGE`
- 23 `UDR`
- 24 `EXCEPTION`
- 25 `COMMENT`
- 26 `DEPENDENCY`
- 27 `PERMISSION`
- 28 `STATISTIC`
- 29 `TIMEZONE`
- 30 `EXTENSION`
- 31 `FOREIGN_SERVER`
- 32 `FOREIGN_TABLE`
- 33 `USER_MAPPING`
- 34 `SERVER_REGISTRY`
- 35 `UDR_ENGINE`
- 36 `UDR_MODULE`
- 37 `CLUSTER`
- 38 `SYNONYM`
- 39 `POLICY`
- 40 `JOB`
- 255 `UNKNOWN`

## Enum: permission_object_type
Values:
- 0 `SCHEMA`
- 1 `TABLE`
- 2 `VIEW`
- 3 `SEQUENCE`
- 4 `PROCEDURE`
- 5 `FUNCTION`
- 6 `DOMAIN`
- 7 `DATABASE`
- 8 `JOB`

## Enum: grantee_type
Values:
- 0 `USER`
- 1 `ROLE`
- 2 `GROUP`
- 3 `PUBLIC`

## Privilege Bitmask: privilege_bits
Bit values:
- 0x00000001 `SELECT`
- 0x00000002 `INSERT`
- 0x00000004 `UPDATE`
- 0x00000008 `DELETE`
- 0x00000010 `TRUNCATE`
- 0x00000020 `REFERENCES`
- 0x00000040 `TRIGGER`
- 0x00000080 `CREATE`
- 0x00000100 `USAGE`
- 0x00000200 `SEQUENCE_USAGE`
- 0x00000400 `SEQUENCE_UPDATE`
- 0x00000800 `EXECUTE`
- 0x00001000 `CONNECT`
- 0x00002000 `TEMPORARY`
- 0x00004000 `COPY_FILE`
- 0x00008000 `CREATE_JOB`
- 0x00010000 `VIEW_JOB_HISTORY`
- 0x00020000 `EXECUTE_EXTERNAL_JOB`
- 0xFFFFFFFF `ALL`

## Enum: constraint_type
Values:
- 0 `PRIMARY_KEY`
- 1 `UNIQUE`
- 2 `CHECK`
- 3 `FOREIGN_KEY`
- 4 `NOT_NULL`
- 5 `EXCLUSION`

## Enum: fk_action
Values:
- 0 `NO_ACTION`
- 1 `RESTRICT`
- 2 `CASCADE`
- 3 `SET_NULL`
- 4 `SET_DEFAULT`

## Enum: fk_match_type
Values:
- 0 `SIMPLE`
- 1 `FULL`
- 2 `PARTIAL`

## Enum: trigger_timing
Values:
- 0 `BEFORE`
- 1 `AFTER`
- 2 `INSTEAD_OF`

## Enum: trigger_scope
Values:
- 0 `TABLE`
- 1 `DATABASE`

## Enum: trigger_event
Bit flags:
- 0x01 `INSERT`
- 0x02 `UPDATE`
- 0x04 `DELETE`

## Enum: trigger_granularity
Values:
- 0 `FOR_EACH_ROW`
- 1 `FOR_EACH_STATEMENT`

## Enum: database_trigger_event
Values:
- 0 `ON_CONNECT`
- 1 `ON_DISCONNECT`
- 2 `ON_TRANSACTION_START`
- 3 `ON_TRANSACTION_COMMIT`
- 4 `ON_TRANSACTION_ROLLBACK`

## Enum: dependency_type
Values:
- 0 `NORMAL`
- 1 `AUTO`
- 2 `INTERNAL`
- 3 `PIN`

## Enum: table_kind
Values:
- 0 `HEAP`
- 1 `INDEX_ORGANIZED`
- 2 `TEMPORARY`
- 3 `EXTERNAL`
- 4 `MATERIALIZED_VIEW`
- 5 `TOAST`
- 6 `COLUMNSTORE`
- 7 `HYBRID`
- 8 `SYSTEM`

## Enum: row_uuid_mode
Values:
- 0 `INTERNAL_ONLY`
- 1 `SURFACED_PK`
- 2 `SURFACED_UNIQUE`

## Enum: system_column_kind
Values:
- 0 `NONE`
- 1 `ROW_UUID`
- 2 `LAST_EDIT_TXID`

## Enum: toast_storage_strategy
Values:
- 0 `PLAIN`
- 1 `EXTENDED`
- 2 `COMPRESSED`
- 3 `EXTERNAL`

## Enum: temp_metadata_scope
Values:
- 0 `NONE`
- 1 `GLOBAL`
- 2 `SESSION`

## Enum: temp_data_scope
Values:
- 0 `NONE`
- 1 `SESSION`
- 2 `TRANSACTION`

## Enum: temp_on_commit
Values:
- 0 `NONE`
- 1 `DELETE_ROWS`
- 2 `PRESERVE_ROWS`
- 3 `DROP`

## Enum: mv_refresh_strategy
Values:
- 0 `COMPLETE`
- 1 `INCREMENTAL`
- 2 `FAST`

## Enum: histogram_type
Values:
- 0 `EQUAL_HEIGHT`
- 1 `EQUAL_WIDTH`
- 255 `NONE`

## Enum: collation_type
Values:
- 0 `BINARY`
- 1 `CASE_SENSITIVE`
- 2 `CASE_INSENSITIVE`
- 3 `ACCENT_INSENSITIVE`
- 4 `CI_AI`
- 5 `UNICODE`
- 6 `NATURAL`
- 7 `NUMERIC`

## Enum: collation_strength
Values:
- 1 `PRIMARY`
- 2 `SECONDARY`
- 3 `TERTIARY`
- 4 `QUATERNARY`
- 5 `IDENTICAL`

## Enum: group_type
Values:
- 0 `LOCAL`
- 1 `AD`
- 2 `LDAP`

## Enum: auth_method
Values:
- 0 `TRUST`
- 1 `REJECT`
- 2 `PASSWORD`
- 3 `MD5`
- 4 `SCRAM_SHA_256`
- 5 `SCRAM_SHA_512`
- 6 `CERTIFICATE`
- 7 `LDAP`
- 8 `KERBEROS`
- 9 `PEER`
- 10 `IDENT`
- 11 `RADIUS`
- 12 `PAM`

## Enum: home_schema_source
Values:
- 0 `USER`
- 1 `ROLE`
- 2 `GROUP`
- 3 `PUBLIC`
- 4 `EMULATED_BASE`

## Enum: home_principal_type
Values:
- 0 `USER`
- 1 `ROLE`
- 2 `GROUP`

## Enum: home_scope_kind
Values:
- 0 `GLOBAL`
- 1 `WORKGROUP`
- 2 `CLUSTER`
- 3 `EMULATED_DATABASE`

## Enum: search_path_scope_kind
Values:
- 0 `USER`
- 1 `ROLE`
- 2 `GROUP`
- 3 `WORKGROUP`
- 4 `CLUSTER`
- 5 `SESSION`
- 6 `EMULATED_DATABASE`

## Enum: authkey_status
Values:
- 0 `ACTIVE`
- 1 `REVOKED`
- 2 `EXPIRED`
- 3 `SUSPENDED`

## Enum: authkey_usage
Values:
- 0 `UNLIMITED`
- 1 `LIMITED`
- 2 `SINGLE_USE`

## Enum: transport
Values:
- 0 `LOCAL`
- 1 `IPC`
- 2 `INET`

## Enum: transaction_state
Values:
- 0 `IN_PROGRESS`
- 1 `COMMITTED`
- 2 `ABORTED`
- 3 `PREPARED`

## Enum: artifact_state
Values:
- 0 `QUEUED`
- 1 `COMPILING`
- 2 `READY`
- 3 `FAILED`
- 4 `RETIRED`

## Enum: queue_state
Values:
- 0 `QUEUED`
- 1 `RUNNING`
- 2 `RETRY_WAIT`
- 3 `FAILED`
- 4 `COMPLETED`

## Enum: policy_type
Values:
- 0 `ALL`
- 1 `SELECT`
- 2 `INSERT`
- 3 `UPDATE`
- 4 `DELETE`

## Enum: job_type
Values:
- 0 `SQL`
- 1 `PROCEDURE`
- 2 `EXTERNAL`

## Enum: job_class
Values:
- 0 `UNSPECIFIED`
- 1 `LOCAL_SAFE`
- 2 `LEADER_ONLY`
- 3 `QUORUM_REQUIRED`

## Enum: schedule_kind
Values:
- 0 `CRON`
- 1 `AT`
- 2 `EVERY`

## Enum: job_state
Values:
- 0 `ENABLED`
- 1 `DISABLED`
- 2 `PAUSED`

## Enum: job_run_state
Values:
- 0 `PENDING`
- 1 `RUNNING`
- 2 `COMPLETED`
- 3 `FAILED`
- 4 `CANCELLED`

## Enum: job_on_completion
Values:
- 0 `PRESERVE`
- 1 `DROP`

## Enum: procedure_type
Values:
- 0 `PROCEDURE`
- 1 `FUNCTION`

## Enum: procedure_language
Values:
- 0 `PSQL`
- 1 `SQL`
- 2 `UDR`
- 3 `PLPGSQL`

## Enum: parameter_mode
Values:
- 0 `IN`
- 1 `OUT`
- 2 `INOUT`

## Enum: sql_security
Values:
- 0 `DEFINER`
- 1 `INVOKER`

## Enum: udr_type
Values:
- 0 `FUNCTION`
- 1 `PROCEDURE`
- 2 `TRIGGER`

## Enum: udr_engine_type
Values:
- 0 `NATIVE`
- 1 `JAVA`
- 2 `PYTHON`
- 3 `JAVASCRIPT`
- 4 `DOTNET`
- 5 `LUA`
- 6 `WASM`

## Enum: encryption_algorithm
Values:
- 0 `NONE`
- 1 `AES128_GCM`
- 2 `AES256_GCM`

## Enum: kdf_algorithm
Values:
- 0 `ARGON2ID`
- 1 `SCRYPT`
- 2 `PBKDF2`

## Enum: key_rotation_policy
Values:
- 0 `MANUAL`
- 1 `INTERVAL`
- 2 `ON_DEMAND`

## Enum: key_kind
Values:
- 0 `MASTER`
- 1 `DATA`
- 2 `TLS`
- 3 `SIGNING`
- 4 `CLUSTER`

## Enum: key_status
Values:
- 0 `ACTIVE`
- 1 `DISABLED`
- 2 `RETIRED`
- 3 `PENDING`

## Enum: cert_kind
Values:
- 0 `TLS_SERVER`
- 1 `TLS_CLIENT`
- 2 `SIGNING`
- 3 `CLUSTER`

## Enum: cert_status
Values:
- 0 `ACTIVE`
- 1 `REVOKED`
- 2 `EXPIRED`
- 3 `PENDING`

## Enum: trust_anchor_state
Values:
- 0 `STAGED`
- 1 `ACTIVE`
- 2 `RETIRING`
- 3 `RETIRED`
- 4 `REVOKED`

## Enum: tls_version
Values:
- 0 `TLS1_2`
- 1 `TLS1_3`

## Enum: revocation_source
Values:
- 0 `OCSP`
- 1 `CRL`
- 2 `MANUAL`
- 3 `CLUSTER`

## Enum: revocation_reason
Values:
- 0 `UNSPECIFIED`
- 1 `KEY_COMPROMISE`
- 2 `CA_COMPROMISE`
- 3 `SUPERSEDED`
- 4 `CESSATION`

## Enum: pki_artifact_kind
Values:
- 0 `CERT`
- 1 `TRUST_ANCHOR`
- 2 `CRL`
- 3 `BINDING`

## Enum: distribution_state
Values:
- 0 `PENDING`
- 1 `DISTRIBUTED`
- 2 `APPLIED`
- 3 `FAILED`

## Enum: rollover_phase
Values:
- 0 `PLANNED`
- 1 `STAGING`
- 2 `DUAL_TRUST`
- 3 `CUTOVER`
- 4 `RETIRE_OLD`
- 5 `COMPLETE`
- 6 `ABORTED`

## Enum: unlock_result
Values:
- 0 `NONE`
- 1 `SUCCESS`
- 2 `TIMEOUT`
- 3 `FAILED`
- 4 `PARTIAL`

## Enum: server_role
Values:
- 0 `PRIMARY`
- 1 `REPLICA`
- 2 `STANDBY`
- 3 `WITNESS`

## Enum: server_state
Values:
- 0 `ONLINE`
- 1 `OFFLINE`
- 2 `SYNCING`
- 3 `MAINTENANCE`
- 4 `FAILED`

## Enum: migration_phase
Values:
- 0 `MIGRATION_NONE`
- 1 `MIGRATION_INIT`
- 2 `MIGRATION_COPYING`
- 3 `MIGRATION_CATCH_UP`
- 4 `MIGRATION_READY_FOR_SWAP`
- 5 `MIGRATION_SWAP`
- 6 `MIGRATION_CLEANUP`
- 7 `MIGRATION_COMPLETE`
- 8 `MIGRATION_FAILED`
- 9 `MIGRATION_ABORTED`

## Enum: isolation_level
Values:
- 0 `READ_COMMITTED`
- 1 `READ_COMMITTED_READ_CONSISTENCY`
- 2 `SNAPSHOT`
- 3 `SNAPSHOT_TABLE_STABILITY`

## Enum: dormant_statement_type
Values:
- 0 `UNKNOWN`
- 1 `DDL`
- 2 `DML`
- 3 `OTHER`

## Enum: dormant_statement_status
Values:
- 0 `UNKNOWN`
- 1 `IN_PROGRESS`
- 2 `COMPLETED`
- 3 `FAILED`

## Enum: dormant_transaction_state
Values:
- 0 `DORMANT`
- 1 `REATTACHED`
- 2 `ROLLED_BACK`
- 3 `EXPIRED`

## Enum: dormant_access_mode
Values:
- 0 `READ_WRITE`
- 1 `READ_ONLY`

## Enum: dormant_wait_mode
Values:
- 0 `WAIT`
- 1 `NO_WAIT`

## Enum: emulation_engine
Values:
- 0 `NATIVE`
- 1 `FIREBIRD`
- 2 `POSTGRESQL`
- 3 `MYSQL`
- 4 `CASSANDRA`
- 5 `MILVUS`
- 6 `MONGODB`
- 7 `NEO4J`
- 8 `REDIS`

## Enum: storage_profile
Values:
- 0 `RELATIONAL`
- 1 `NATIVE_EMULATION`
- 2 `HYBRID`

## Enum: cluster_mode
Values:
- 0 `WORKGROUP`
- 1 `CLUSTER`

## Enum: cluster_state
Values:
- 0 `HEALTHY`
- 1 `DEGRADED`
- 2 `PARTITIONED`
- 3 `READ_ONLY`
- 4 `FROZEN`

## Enum: consensus_mode
Values:
- 0 `NONE`
- 1 `RAFT`
- 2 `PAXOS`
- 3 `EXTERNAL`

## Enum: node_state
Values:
- 0 `JOINING`
- 1 `SYNCING`
- 2 `WARMING`
- 3 `ONLINE`
- 4 `DRAINING`
- 5 `OFFLINE`
- 6 `SUSPECT`
- 7 `FAILED`

## Enum: node_role
Values:
- 0 `METADATA`
- 1 `OLTP_DATA`
- 2 `ROUTER`
- 3 `PARSER`
- 4 `LISTENER`
- 5 `BACKUP`
- 6 `SCHEDULER`
- 7 `METRICS`
- 8 `OLAP_INGEST`
- 9 `OLAP_STORAGE`
- 10 `OLAP_COMPUTE`
- 11 `VECTOR_INDEX`
- 12 `SEARCH_INDEX`
- 13 `GRAPH_COMPUTE`
- 14 `CACHE`

## Enum: service_type
Values:
- 0 `OLTP_RPC`
- 1 `OLAP_INGEST`
- 2 `OLAP_QUERY`
- 3 `VECTOR_QUERY`
- 4 `TEXT_SEARCH`
- 5 `GRAPH_QUERY`
- 6 `BACKUP`
- 7 `METRICS`
- 8 `ADMIN`

## Enum: service_state
Values:
- 0 `STARTING`
- 1 `ONLINE`
- 2 `DRAINING`
- 3 `OFFLINE`

## Enum: shard_state
Values:
- 0 `ACTIVE`
- 1 `READ_ONLY`
- 2 `DRAINING`
- 3 `OFFLINE`
- 4 `FAILED`

## Enum: shard_kind
Values:
- 0 `DATA`
- 1 `INDEX`
- 2 `SYSTEM`

## Enum: replica_role
Values:
- 0 `PRIMARY`
- 1 `SECONDARY`
- 2 `LEARNER`
- 3 `STANDBY`

## Enum: replica_state
Values:
- 0 `ONLINE`
- 1 `SYNCING`
- 2 `LAGGING`
- 3 `OFFLINE`
- 4 `FAILED`
- 5 `REBUILDING`

## Enum: shard_key_kind
Values:
- 0 `HASH`
- 1 `RANGE`
- 2 `CONSISTENT_HASH`
- 3 `COMPOSITE`
- 4 `EXPRESSION`
- 5 `TIME`
- 6 `GEOGRAPHY`
- 7 `TENANT`

## Enum: shard_range_kind
Values:
- 0 `NUMERIC`
- 1 `DATETIME`
- 2 `BYTES`
- 3 `TOKEN`
- 4 `GEO`
- 5 `HASH_BUCKET`

## Enum: range_order
Values:
- 0 `LEXICOGRAPHIC`
- 1 `NUMERIC`
- 2 `DATETIME`
- 3 `BYTES`

## Enum: consistency_level
Values:
- 0 `ONE`
- 1 `QUORUM`
- 2 `ALL`
- 3 `LOCAL_QUORUM`
- 4 `SERIAL`
- 5 `LOCAL_SERIAL`

## Enum: failover_mode
Values:
- 0 `MANUAL`
- 1 `AUTOMATIC`

## Enum: rebalance_mode
Values:
- 0 `MANUAL`
- 1 `AUTO`
- 2 `SCHEDULED`

## Enum: hash_function
Values:
- 0 `XXHASH64`
- 1 `MURMUR3`
- 2 `SIPHASH`
- 3 `BUILTIN`

## Enum: migration_state
Values:
- 0 `PLANNED`
- 1 `COPYING`
- 2 `CATCHUP`
- 3 `SWITCHOVER`
- 4 `COMPLETE`
- 5 `FAILED`
- 6 `CANCELED`

## Enum: migration_source_engine
Values:
- 0 `FIREBIRD`
- 1 `POSTGRESQL`
- 2 `MYSQL`
- 3 `CASSANDRA`
- 4 `MONGODB`
- 5 `NEO4J`
- 6 `REDIS`
- 7 `MILVUS`
- 8 `SCRATCHBIRD`

## Enum: migration_runtime_mode
Values:
- 0 `PROXY_ONLY`
- 1 `EMULATED_BUILD`
- 2 `DUAL_WRITE`
- 3 `DUAL_READ_AUDIT`
- 4 `PRIMARY_EMULATED`
- 5 `MIRROR_LEGACY`
- 6 `RETIRED`

## Enum: migration_compare_policy
Values:
- 0 `ROW_COUNT_ONLY`
- 1 `ROW_COUNT_AND_CHECKSUM`
- 2 `FULL_ROW_COMPARE`

## Enum: migration_write_policy
Values:
- 0 `STRICT`
- 1 `LENIENT`

## Enum: migration_return_source
Values:
- 0 `LEGACY`
- 1 `EMULATED`

## Enum: migration_cursor_kind
Values:
- 0 `SNAPSHOT`
- 1 `LOGICAL_REPLICATION`
- 2 `BINLOG`
- 3 `CHANGE_STREAM`
- 4 `CDC_TABLE`
- 5 `CUSTOM`

## Enum: migration_object_state
Values:
- 0 `PENDING`
- 1 `SNAPSHOT_COPIED`
- 2 `CDC_CATCHUP`
- 3 `VERIFIED`
- 4 `CUTOVER`
- 5 `FAILED`
- 6 `SKIPPED`

## Enum: migration_apply_state
Values:
- 0 `PENDING`
- 1 `APPLIED`
- 2 `RETRY`
- 3 `FAILED`
- 4 `SKIPPED`

## Enum: migration_compare_state
Values:
- 0 `MATCH`
- 1 `MISMATCH`
- 2 `ERROR`
- 3 `SKIPPED`

## Enum: migration_event_kind
Values:
- 0 `MODE_CHANGE`
- 1 `MANUAL_APPROVAL`
- 2 `AUTO_GUARD_BLOCK`
- 3 `CUTOVER`
- 4 `ROLLBACK`
- 5 `RETIRE`

## Enum: migration_error_class
Values:
- 0 `SOURCE_CONNECT`
- 1 `SOURCE_PERMISSIONS`
- 2 `SCHEMA_DRIFT`
- 3 `APPLY_CONFLICT`
- 4 `TIMEOUT`
- 5 `FATAL_INTERNAL`

## Enum: object_kind
Values:
- 0 `DATABASE`
- 1 `SCHEMA`
- 2 `TABLE`
- 3 `INDEX`

## Enum: throttle_state
Values:
- 0 `NONE`
- 1 `LOW`
- 2 `MEDIUM`
- 3 `HIGH`

## Enum: shard_policy_param_type
Values:
- 0 `BOOL`
- 1 `INT`
- 2 `FLOAT`
- 3 `STRING`
- 4 `UUID`
- 5 `JSON`

## Enum: workload_match_kind
Values:
- 0 `ROLE`
- 1 `USER`
- 2 `DATABASE`
- 3 `SCHEMA`
- 4 `CLIENT_APP`
- 5 `STATEMENT_TAG`
- 6 `QUERY_TYPE`
- 7 `REGEX`
- 8 `RESOURCE_TAG`
- 9 `CUSTOM`

## Enum: route_target_kind
Values:
- 0 `NODE`
- 1 `SERVICE`
- 2 `ROLE`
- 3 `SHARD`
- 4 `TIER`

## Enum: admission_reject_mode
Values:
- 0 `REJECT`
- 1 `QUEUE`
- 2 `SHED_LOW_PRIORITY`

## Enum: admission_target_kind
Values:
- 0 `CLUSTER`
- 1 `NODE`
- 2 `SERVICE`
- 3 `WORKLOAD_CLASS`

## Enum: cluster_policy_kind
Values:
- 0 `BASE`
- 1 `SECURITY`
- 2 `ROUTING`
- 3 `HEALING`
- 4 `CUSTOM`

## Enum: failure_detector_kind
Values:
- 0 `PHI`
- 1 `HEARTBEAT`
- 2 `GOSSIP`

## Enum: alert_rule_kind
Values:
- 0 `METRIC`
- 1 `EVENT`
- 2 `LOG`

## Enum: alert_severity
Values:
- 0 `INFO`
- 1 `WARNING`
- 2 `CRITICAL`

## Enum: alert_target_kind
Values:
- 0 `EMAIL`
- 1 `WEBHOOK`
- 2 `SYSLOG`
- 3 `PAGER`
- 4 `SMS`
- 5 `SLACK`
- 6 `CUSTOM`

## Enum: alert_route_kind
Values:
- 0 `IMMEDIATE`
- 1 `BATCH`
- 2 `ESCALATION`

## Enum: alert_event_state
Values:
- 0 `OPEN`
- 1 `ACKED`
- 2 `RESOLVED`
- 3 `SUPPRESSED`

## Enum: alert_silence_scope
Values:
- 0 `CLUSTER`
- 1 `NODE`
- 2 `RULE`
- 3 `TARGET`

## Enum: partition_state
Values:
- 0 `OPEN`
- 1 `RESOLVED`

## Enum: healing_trigger_kind
Values:
- 0 `PARTITION`
- 1 `FAILOVER`
- 2 `CAPACITY`
- 3 `MANUAL`

## Enum: healing_action_kind
Values:
- 0 `RESTART_NODE`
- 1 `REBALANCE_SHARDS`
- 2 `REPAIR_REPLICA`
- 3 `PROMOTE_REPLICA`
- 4 `ISOLATE_NODE`
- 5 `NOTIFY`

## Enum: healing_param_type
Values:
- 0 `BOOL`
- 1 `INT`
- 2 `FLOAT`
- 3 `STRING`
- 4 `UUID`
- 5 `JSON`

## Enum: healing_run_state
Values:
- 0 `QUEUED`
- 1 `RUNNING`
- 2 `COMPLETED`
- 3 `FAILED`
- 4 `CANCELLED`

## Enum: healing_step_state
Values:
- 0 `PENDING`
- 1 `RUNNING`
- 2 `COMPLETED`
- 3 `FAILED`
- 4 `SKIPPED`

## Enum: job_group
Values:
- 0 `USER_DEFINED`
- 1 `SYSTEM_LOCAL`
- 2 `MANAGEMENT`
- 3 `GROUP`
- 4 `CLUSTER`
- 5 `IT_MANAGEMENT`
- 6 `OLAP`

## Enum: job_param_type
Values:
- 0 `BOOL`
- 1 `INT`
- 2 `FLOAT`
- 3 `STRING`
- 4 `UUID`
- 5 `JSON`

## Enum: token_state
Values:
- 0 `ACTIVE`
- 1 `REVOKED`
- 2 `EXPIRED`

## Enum: lifecycle_event_type
Values:
- 0 `START_BEGIN`
- 1 `START_READY`
- 2 `STOP_BEGIN`
- 3 `STOP_COMPLETE`
- 4 `FAILURE`

## Enum: cube_range_kind
Values:
- 0 `TIME`
- 1 `HASH`
- 2 `RANGE`
- 3 `TENANT`

## Enum: olap_compression
Values:
- 0 `NONE`
- 1 `LZ4`

## Enum: olap_tier
Values:
- 0 `HOT`
- 1 `WARM`
- 2 `COLD`

## Enum: olap_ingest_state
Values:
- 0 `QUEUED`
- 1 `INGESTING`
- 2 `COMMITTED`
- 3 `FAILED`

## Enum: cube_status
Values:
- 0 `ACTIVE`
- 1 `DISABLED`
- 2 `REBUILDING`

## Enum: cube_source_kind
Values:
- 0 `COLUMN`
- 1 `EXPRESSION`

## Enum: cube_agg_function
Values:
- 0 `SUM`
- 1 `COUNT`
- 2 `MIN`
- 3 `MAX`
- 4 `AVG`
- 5 `APPROX_COUNT_DISTINCT`

## Enum: cube_null_handling
Values:
- 0 `IGNORE_NULLS`
- 1 `INCLUDE_NULLS`

## Enum: cube_materialization_state
Values:
- 0 `BUILDING`
- 1 `ACTIVE`
- 2 `STALE`
- 3 `FAILED`

## Enum: cube_refresh_mode
Values:
- 0 `MANUAL`
- 1 `INTERVAL`
- 2 `ON_COMMIT`
- 3 `ON_SCHEDULE`

## Enum: cube_job_type
Values:
- 0 `BUILD`
- 1 `REFRESH`
- 2 `REBUILD`
- 3 `DROP`

## Enum: cube_job_state
Values:
- 0 `QUEUED`
- 1 `RUNNING`
- 2 `COMPLETED`
- 3 `FAILED`

## Enum: subscription_table_state
Values:
- 0 `INIT`
- 1 `DATA_COPY`
- 2 `CATCHUP`
- 3 `READY`
- 4 `ERROR`

## Enum: replication_direction
Values:
- 0 `ONE_WAY`
- 1 `BIDIRECTIONAL`

## Enum: replication_channel_state
Values:
- 0 `INIT`
- 1 `SNAPSHOT`
- 2 `CATCHUP`
- 3 `STREAMING`
- 4 `PAUSED`
- 5 `DEGRADED`
- 6 `FENCED`
- 7 `STOPPED`
- 8 `FAILED`

## Enum: replication_member_role
Values:
- 0 `PUBLISHER`
- 1 `SUBSCRIBER`
- 2 `PEER`

## Enum: replication_cursor_state
Values:
- 0 `ACTIVE`
- 1 `STALLED`
- 2 `ERROR`
- 3 `CLOSED`

## Enum: replication_txn_state
Values:
- 0 `RECEIVED`
- 1 `VALIDATED`
- 2 `APPLIED`
- 3 `SKIPPED`
- 4 `FAILED`
- 5 `CONFLICT`

## Enum: replication_retry_state
Values:
- 0 `QUEUED`
- 1 `RUNNING`
- 2 `EXHAUSTED`
- 3 `DEAD_LETTER`

## Enum: replication_ddl_policy
Values:
- 0 `BLOCK`
- 1 `MANUAL_APPROVE`
- 2 `SAFE_ONLY`
- 3 `FULL`

## Enum: replication_conflict_policy
Values:
- 0 `SOURCE_WINS`
- 1 `TARGET_WINS`
- 2 `LAST_COMMIT_WINS`
- 3 `ORIGIN_PRIORITY`
- 4 `MANUAL_REQUIRED`

## Enum: replication_conflict_kind
Values:
- 0 `UPDATE_UPDATE`
- 1 `DELETE_UPDATE`
- 2 `UNIQUE_CONSTRAINT`
- 3 `DDL_DML`
- 4 `DDL_DDL`
- 5 `TYPE_MISMATCH`

## Enum: replication_resolution_state
Values:
- 0 `OPEN`
- 1 `AUTO_RESOLVED`
- 2 `MANUAL_PENDING`
- 3 `MANUAL_RESOLVED`
- 4 `IGNORED`

## Enum: replication_event_kind
Values:
- 0 `CHANNEL_START`
- 1 `CHANNEL_PAUSE`
- 2 `CHANNEL_RESUME`
- 3 `CHANNEL_STOP`
- 4 `LAG_ALERT`
- 5 `SPLIT_BRAIN_DETECTED`
- 6 `SPLIT_BRAIN_CLEARED`
- 7 `RECOVERY_START`
- 8 `RECOVERY_COMPLETE`

## Enum: rule_event
Values:
- 0 `SELECT`
- 1 `INSERT`
- 2 `UPDATE`
- 3 `DELETE`

## Enum: rule_action_kind
Values:
- 0 `INSTEAD`
- 1 `ALSO`

## Enum: package_member_kind
Values:
- 0 `PROCEDURE`
- 1 `FUNCTION`

## Enum: event_status
Values:
- 0 `ENABLED`
- 1 `DISABLED`
- 2 `SLAVESIDE_DISABLED`

## Enum: event_on_completion
Values:
- 0 `DROP`
- 1 `PRESERVE`

## Enum: language_kind
Values:
- 0 `INTERNAL`
- 1 `SQL`
- 2 `PSQL`
- 3 `PLPGSQL`
- 4 `PLPYTHON`
- 5 `PLLUA`
- 6 `PLJAVASCRIPT`
- 7 `PLDOTNET`
- 8 `PLJAVA`
- 9 `PLWASM`
- 10 `CUSTOM`

## Enum: backup_kind
Values:
- 0 `FULL`
- 1 `INCREMENTAL`
- 2 `DIFFERENTIAL`
- 3 `SNAPSHOT`
- 4 `LOGICAL`

## Enum: backup_status
Values:
- 0 `STARTED`
- 1 `RUNNING`
- 2 `SUCCESS`
- 3 `FAILED`
- 4 `CANCELLED`

## Enum: partition_strategy
Values:
- 0 `RANGE`
- 1 `LIST`
- 2 `HASH`

## Enum: partition_bound_kind
Values:
- 0 `RANGE`
- 1 `LIST`
- 2 `HASH`
- 3 `DEFAULT`

## Enum: inheritance_kind
Values:
- 0 `INHERITS`
- 1 `PARTITION`
