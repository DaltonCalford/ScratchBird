# Definitive Specset Index

This index freezes the main current canonical files that the package must use as
its baseline rather than inventing new architecture from scratch.

## Native Equivalent Source Packet

- `docs/specifications/work/audits/SCRATCHBIRD_SQLSERVER_AZURE_NATIVE_EQUIVALENT_ANALYSIS_2026-04-03/SQLSERVER_AZURE_NATIVE_EQUIVALENT_ANALYSIS.md`
- `docs/specifications/work/audits/SCRATCHBIRD_SQLSERVER_AZURE_NATIVE_EQUIVALENT_ANALYSIS_2026-04-03/SQLSERVER_AZURE_NATIVE_EQUIVALENT_MATRIX.csv`

## Existing Canon That Must Be Expanded Or Closed

### Security

- `docs/specifications/19_Security_Model/BETA2_TRANSPARENT_AT_REST_ENCRYPTION_AND_REKEY_MODEL.md`
- `docs/specifications/19_Security_Model/BETA2_PROTECTED_QUERY_ENCRYPTION_AND_ENCLAVE_EXECUTION_MODEL.md`
- `docs/specifications/19_Security_Model/ROW_LEVEL_SECURITY_POLICY_AND_FORCE_RLS_MODEL.md`
- `docs/specifications/19_Security_Model/ROW_COLUMN_DOMAIN_MASKING_AND_SANDBOX_SECURITY_MODEL.md`
- `docs/specifications/19_Security_Model/AUTH_PLUGIN_METHOD_SPECIFICATIONS.md`
- `docs/specifications/19_Security_Model/SYSARCH_SYSADMIN_AND_CLUSTER_ADMIN_BOOTSTRAP_AUTHORITY_MODEL.md`

### Runtime, Cluster, And Service Control Plane

- `docs/specifications/25_Runtime_Modes/BETA2_CLUSTER_HA_DR_AND_FAILOVER_MODEL.md`
- `docs/specifications/25_Runtime_Modes/BETA2_HIGH_PERFORMANCE_OLTP_SERVICE_CLASS_AND_NODE_SPECIALIZATION_MODEL.md`
- `docs/specifications/25_Runtime_Modes/BETA2_CLUSTER_REMOTE_FRAGMENT_EXECUTION_EXCHANGE_AND_ADMISSION_MODEL.md`
- `docs/specifications/25_Runtime_Modes/BETA2_SHARD_PLACEMENT_REBALANCE_SPLIT_MERGE_AND_READ_ROUTING_MODEL.md`
- `docs/specifications/42_Failure_Model_and_Fault_Tolerance/BETA2_FAILOVER_SESSION_CONTINUITY_AND_RECOVERY_CLASSIFICATION_MODEL.md`

### Planner, Federation, And Tuning

- `docs/specifications/36_Query_Rewrite_and_Planner/BETA2_CROSS_MACHINE_QUERY_DECOMPOSITION_DATA_MOTION_AND_RESULT_STITCHING_MODEL.md`
- `docs/specifications/36_Query_Rewrite_and_Planner/COMMERCIAL_PLAN_STORE_BASELINES_AND_REGRESSION_GOVERNANCE_BETA2_MODEL.md`
- `docs/specifications/36_Query_Rewrite_and_Planner/ADAPTIVE_QUERY_PROCESSING_MEMORY_GRANT_AND_INTERLEAVED_EXECUTION_BETA2_MODEL.md`
- `docs/specifications/36_Query_Rewrite_and_Planner/BETA2_HIGH_PERFORMANCE_OLTP_PLAN_SHAPES_CONTENTION_AVOIDANCE_AND_PREPARED_EXECUTION_MODEL.md`

### Catalog And External Data

- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/BETA2_EXTERNAL_TABLE_MANIFEST_AND_SNAPSHOT_CATALOG_MODEL.md`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/BETA2_DISTRIBUTED_QUERY_FRAGMENT_LOCATION_COST_AND_EXCHANGE_CATALOG_MODEL.md`
- `docs/specifications/17_Functions_and_Procedures/BETA2_ODBC_DATASOURCE_CRUD_AND_REMOTE_SQL_UDR_MODEL.md`
- `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/BETA2_OPEN_TABLE_FORMAT_AND_OBJECT_STORE_TABLE_MODEL.md`

### Temporal, Replay, And Observability

- `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/BETA2_ARCHIVE_REPLAY_AND_RESTORE_MODEL.md`
- `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/BETA2_PRODUCTION_WORKLOAD_CAPTURE_REPLAY_AND_REHEARSAL_MODEL.md`
- `docs/specifications/20_Diagnostics_Audit_and_Observability/BETA2_WORKLOAD_TRACE_CAPTURE_PRIVACY_AND_REDACTION_MODEL.md`
- `docs/specifications/21_V3_Dialect_Surface/BETA2_DONOR_DIALECT_AST_GRAMMAR_AND_DESUGAR_EXPANSION_MODEL.md`

### OLTP, OLAP, And Graph

- `docs/specifications/18_Index_Framework/COLUMNSTORE_SPEC.md`
- `docs/specifications/18_Index_Framework/BETA2_OLAP_STORAGE_SEGMENT_PRUNING_VECTOR_SCAN_AND_CUBE_ACCELERATION_MODEL.md`
- `docs/specifications/17_Functions_and_Procedures/BETA2_GRAPH_SCIENCE_AND_NETWORK_ANALYSIS_UDR_MODEL.md`
- `docs/specifications/38_Workload_Governance_and_Parallelism/BETA2_HARD_MULTI_TENANT_ISOLATION_QUOTA_AND_QOS_MODEL.md`
- `docs/specifications/33_Memory_Management/BETA2_TENANT_RESERVATION_AND_ENFORCED_BUDGET_MODEL.md`

## New Canonical Files Expected From This Package

The package is expected to create or expand files in these families:

- transactional eventing, durable queues, and notifications
- database scheduler, alerting, and operator messaging
- managed `WASM/WASI` extensibility runtime
- native changefeeds and consumer offsets
- relational temporal versioning and history binding
- tamper-evident ledger and attestation
- property graph storage and pattern matching
- federation implementation closure
- transactional blob and file namespace tables
- plan-store implementation closure
- service tiers and tenant pools
- serverless autosuspend and autoscale
- replicated topology and geo failover
- memory-optimized OLTP lanes
- distributed atomic coordination
- enterprise identity federation
- encryption, RLS, masking, and columnstore implementation-closure canon
