# Definitive Specset Index

This file freezes the authoritative current boundaries and planned closure
targets for the commercial-parity gaps in scope.

## Gap 1: Transparent At-Rest Encryption and Rekey

- Current boundary:
  - `docs/specifications/19_Security_Model/DOMAIN_SECURITY_MASKING_ENCRYPTION_AND_AUDIT_MODEL.md`
    + `encryption metadata`
- Planned canonical target:
  - `docs/specifications/19_Security_Model/BETA2_TRANSPARENT_AT_REST_ENCRYPTION_AND_REKEY_MODEL.md`

## Gap 2: Protected-Query Encryption and Enclave-Safe Execution

- Current boundary:
  - absence of canonical matches for `Always Encrypted`, `enclave`,
    `searchable encryption`, `confidential computing`
- Planned canonical target:
  - `docs/specifications/19_Security_Model/BETA2_PROTECTED_QUERY_ENCRYPTION_AND_ENCLAVE_EXECUTION_MODEL.md`

## Gap 3: HA / DR / PITR / Clustered Failover

- Current boundaries:
  - `docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAULT_TOLERANCE_NON_GUARANTEES.md`
    + `no transparent failover or session migration guarantee`
  - `docs/specifications/08_Transaction_Core/FAILURE_MODEL_AND_RECOVERY_CLASSIFICATION.md`
    + `This file does not define PITR`
- Planned canonical targets:
  - `docs/specifications/25_Runtime_Modes/BETA2_CLUSTER_HA_DR_AND_FAILOVER_MODEL.md`
  - `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/BETA2_PITR_LOG_RETENTION_AND_REHEARSAL_MODEL.md`
  - `docs/specifications/42_Failure_Model_and_Fault_Tolerance/BETA2_FAILOVER_SESSION_CONTINUITY_AND_RECOVERY_CLASSIFICATION_MODEL.md`

## Gap 4: Hard Multi-Tenant Isolation, Quotas, Reservations, and QoS

- Current boundaries:
  - `docs/specifications/38_Workload_Governance_and_Parallelism/RESOURCE_ISOLATION_AND_FAIRNESS_BOUNDARY.md`
    + `no hard tenant isolation guarantee`
  - `docs/specifications/25_Runtime_Modes/CLOUD_SUPPORT_SCOPE_AND_BETA1_BETA2_PROGRAM_MODEL.md`
    + `automatic multi-tenant SaaS isolation`
- Planned canonical targets:
  - `docs/specifications/38_Workload_Governance_and_Parallelism/BETA2_HARD_MULTI_TENANT_ISOLATION_QUOTA_AND_QOS_MODEL.md`
  - `docs/specifications/33_Memory_Management/BETA2_TENANT_RESERVATION_AND_ENFORCED_BUDGET_MODEL.md`

## Gap 5: Archive Tier / ILM / Legal Hold / Replay From Archive

- Current boundary:
  - `docs/specifications/10_GC_and_Sweep/SWEEP_ARCHIVE_AND_RETENTION_POLICY.md`
    + `It does not prove the broad archive-transfer and verified archive-before-prune pipeline`
- Planned canonical targets:
  - `docs/specifications/10_GC_and_Sweep/BETA2_ARCHIVE_TIER_ILM_AND_LEGAL_HOLD_MODEL.md`
  - `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/BETA2_ARCHIVE_REPLAY_AND_RESTORE_MODEL.md`

## Gap 6: Production Workload Capture, Replay, and Rehearsal

- Current boundary:
  - `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/WEAK_DONOR_CHANGE_CAPTURE_REPLAY_QUARANTINE_AND_RESUME_MODEL.md`
    + `managed migration stream, not an ordinary transactional replication stream`
- Planned canonical targets:
  - `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/BETA2_PRODUCTION_WORKLOAD_CAPTURE_REPLAY_AND_REHEARSAL_MODEL.md`
  - `docs/specifications/20_Diagnostics_Audit_and_Observability/BETA2_WORKLOAD_TRACE_CAPTURE_PRIVACY_AND_REDACTION_MODEL.md`

## Gap 7: Open Table Formats and Object-Store Table Semantics

- Current boundaries:
  - `docs/specifications/02_Filespace_Lifecycle/TABLESPACE_DDL_AND_OPERATOR_LIFECYCLE_MODEL.md`
    + `Attach external tablespace file`
  - `docs/specifications/28_Parser_Implementations/EMULATED_ENGINE_PACKAGE_MODEL.md`
    + `external table or equivalent family-specific support objects`
- Planned canonical targets:
  - `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/BETA2_OPEN_TABLE_FORMAT_AND_OBJECT_STORE_TABLE_MODEL.md`
  - `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/BETA2_EXTERNAL_TABLE_MANIFEST_AND_SNAPSHOT_CATALOG_MODEL.md`

## Primary Reference Roots

- `docs/reference/workspace_library/technical_specs/`
- `docs/reference/workspace_library/whitepapers/`
- `docs/reference/workspace_library/third_party_implementations/`
- `docs/reference/reference_library/`

## Implementation-Grade Research Rule

Every closure lane must produce:

- process flows
- state models
- refusal rules
- algorithms or selection logic
- data-structure and metadata requirements
- protocol or API behaviors where applicable
- sample pseudocode or implementation skeletons where complexity warrants it
