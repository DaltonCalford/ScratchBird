# 20_Diagnostics_Audit_and_Observability

## Purpose
Code-backed canonical specification for ScratchBird diagnostics, audit export,
redaction, and operator observability surfaces.

## Status
Authoritative - `partial`.

## Current authority summary
- `AuditLogger` is the real authority for append-only audit chain integrity,
  deterministic local package export or validation, legal hold, and retention
  evaluation.
- `ObservabilityContract` is the real authority for the current `sb_*` metric
  contract and privileged SQL-view inventory for MGA, buffer, checkpoint,
  recovery, and writeback surfaces.
- MGA horizon vocabulary in this section is normalized to `OIT`, `OAT`, and
  `OST` to match section `08` transaction inventory canon.
- `secure_diagnostics` and `structured_logger` are the real authority for
  redaction of secret-bearing diagnostics text and fields.
- `support_bundle_builder` is the real authority for readiness summaries,
  redaction-enforced bundle output, and restart-continuous operational evidence
  packaging.
- sweep-integrated findings plus executor diagnostic-scan paths provide bounded
  page-corruption and repair-required observability.
- independent `sb_btree_*` operator observability is not proven by this pass and
  remains fail closed.

## Primary audit lookup anchors

- `src/core/audit_logger.cpp` search `exportAuditPackage(` for append-only
  audit-chain export, validation, legal hold, and retention authority
- `src/core/secure_diagnostics.cpp` search `redactSensitiveDiagnosticText(` for
  operator-visible text and field redaction authority
- `src/core/support_bundle_builder.cpp` search `generateSupportBundle(` for
  support-bundle readiness and evidence packaging authority
- `src/core/observability_contract.cpp` search
  `makeMetricDefinition("sb_mga_ost",` for current MGA metric registration and
  operator surface authority

## Section Entry Points
- `AUDIT_EXPORT_SINKS_RETENTION_AND_IMMUTABILITY.md`
- `STORAGE_METRICS.md`
- `BUFFER_CACHE_OBSERVABILITY.md`
- `MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md`
- `RECOVERY_AND_CHECKPOINT_OBSERVABILITY.md`
- `PAGE_WALKER_AND_REPAIR.md`
- `ERROR_REFERENCE_UUID_REGISTRY_AND_TEXT_EXTERNALIZATION_MODEL.md`

## Links
- Back to root index: [../README.md](../README.md)

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [ALERT_HEALING_PARTITION_AND_AUTOSCALE_OBSERVABILITY_MODEL.md](ALERT_HEALING_PARTITION_AND_AUTOSCALE_OBSERVABILITY_MODEL.md)
- [AUDIT_EXPORT_SINKS_RETENTION_AND_IMMUTABILITY.md](AUDIT_EXPORT_SINKS_RETENTION_AND_IMMUTABILITY.md)
- [BETA2_TAMPER_EVIDENT_LEDGER_AND_ATTESTATION_MODEL.md](BETA2_TAMPER_EVIDENT_LEDGER_AND_ATTESTATION_MODEL.md)
- [BETA2_WORKLOAD_TRACE_CAPTURE_PRIVACY_AND_REDACTION_MODEL.md](BETA2_WORKLOAD_TRACE_CAPTURE_PRIVACY_AND_REDACTION_MODEL.md)
- [BTREE_HARDENING_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md](BTREE_HARDENING_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md)
- [BUFFER_CACHE_OBSERVABILITY.md](BUFFER_CACHE_OBSERVABILITY.md)
- [CLOUD_HEALTH_PROBE_TELEMETRY_AND_SUPPORT_BUNDLE_MODEL.md](CLOUD_HEALTH_PROBE_TELEMETRY_AND_SUPPORT_BUNDLE_MODEL.md)
- [CLUSTER_FABRIC_MANAGER_AND_REMOTE_DRIFT_OBSERVABILITY_MODEL.md](CLUSTER_FABRIC_MANAGER_AND_REMOTE_DRIFT_OBSERVABILITY_MODEL.md)
- [CLUSTER_IDENTITY_SECURITY_QUORUM_AND_WRITE_FENCE_OBSERVABILITY_MODEL.md](CLUSTER_IDENTITY_SECURITY_QUORUM_AND_WRITE_FENCE_OBSERVABILITY_MODEL.md)
- [CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md](CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md)
- [CLUSTER_SHARED_OBJECT_DRIFT_AND_DEPENDENCY_BLOCK_OBSERVABILITY_MODEL.md](CLUSTER_SHARED_OBJECT_DRIFT_AND_DEPENDENCY_BLOCK_OBSERVABILITY_MODEL.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [ERROR_REFERENCE_UUID_REGISTRY_AND_TEXT_EXTERNALIZATION_MODEL.md](ERROR_REFERENCE_UUID_REGISTRY_AND_TEXT_EXTERNALIZATION_MODEL.md)
- [EXECUTION_CACHE_PLAN_AND_JIT_OBSERVABILITY.md](EXECUTION_CACHE_PLAN_AND_JIT_OBSERVABILITY.md)
- [INDEX_FAMILY_METRICS_FRESHNESS_CONFIDENCE_AND_VISIBILITY_REJECT_OBSERVABILITY_MODEL.md](INDEX_FAMILY_METRICS_FRESHNESS_CONFIDENCE_AND_VISIBILITY_REJECT_OBSERVABILITY_MODEL.md)
- [JIT_RUNTIME_PERFORMANCE_AND_FALLBACK_OBSERVABILITY_MODEL.md](JIT_RUNTIME_PERFORMANCE_AND_FALLBACK_OBSERVABILITY_MODEL.md)
- [LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md](LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md)
- [MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md](MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md)
- [OPTIMIZER_INDEX_MEMORY_AND_ACCELERATOR_OBSERVABILITY.md](OPTIMIZER_INDEX_MEMORY_AND_ACCELERATOR_OBSERVABILITY.md)
- [PAGE_WALKER_AND_REPAIR.md](PAGE_WALKER_AND_REPAIR.md)
- [RECOVERY_AND_CHECKPOINT_OBSERVABILITY.md](RECOVERY_AND_CHECKPOINT_OBSERVABILITY.md)
- [SECURE_DIAGNOSTIC_REDACTION_FIELD_AND_TEXT_MODEL.md](SECURE_DIAGNOSTIC_REDACTION_FIELD_AND_TEXT_MODEL.md)
- [SHARED_DEFINITION_PUBLICATION_ROLLBACK_AND_REPAIR_OBSERVABILITY_MODEL.md](SHARED_DEFINITION_PUBLICATION_ROLLBACK_AND_REPAIR_OBSERVABILITY_MODEL.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [STORAGE_METRICS.md](STORAGE_METRICS.md)
- [SUPPORT_BUNDLE_READINESS_REDACTION_AND_OPERATIONAL_EVIDENCE_MODEL.md](SUPPORT_BUNDLE_READINESS_REDACTION_AND_OPERATIONAL_EVIDENCE_MODEL.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Maintenance
- Update file list with `../skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`.
