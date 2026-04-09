# Product Completion Specification

Date: 2026-02-24
Status: execution baseline
Applies to: `PCG-001..PCG-072`

## 1) Purpose

Define mandatory requirements to close product-level capability gaps expected for a production database platform, while preserving ScratchBird architecture invariants.

## 2) Non-Negotiable Invariants

1. SBLR remains canonical and authoritative.
2. MGA semantics remain the Alpha recovery core.
3. Any PITR lane is optional extension scope and must not replace MGA recovery semantics.
4. Native execution remains optional optimization and never replaces canonical semantics.
5. No ticket is complete without required evidence artifacts and checksums.

## 3) Program Model

1. Execution is ticket-driven (`PCG-*`) with hard dependencies from `DEPENDENCY_GRAPH.csv`.
2. Phase transition requires gate closure (`PCG-GATE-00..PCG-GATE-07`).
3. `PCG-GATE-05`, `PCG-GATE-06`, and `PCG-GATE-07` are launch-blocking.
4. Gate artifacts are normative as listed in `GATE_EVIDENCE_MATRIX.csv`.

## 4) Requirement Catalog

### 4.1 PH0 - Scope and Measurement Baseline

| Requirement ID | Ticket | Normative Requirement | Mandatory Evidence |
| --- | --- | --- | --- |
| PCG-RQ-001 | PCG-001 | The program scope and baseline file set MUST be frozen and checksummed before feature-phase execution. | `INPUT_SCOPE_LOCK.md`, `BASELINE_HASHES.txt`, `DECISION_LOG.md` |
| PCG-RQ-002 | PCG-002 | Cross-engine capability gaps MUST be published in a normalized risk-ranked ledger with explicit closure mapping. | `CAPABILITY_GAP_LEDGER.csv`, `ENGINE_EXPECTATION_MATRIX.csv`, `GAP_RISK_CLASSIFICATION.csv` |
| PCG-RQ-003 | PCG-003 | Program success metrics and SLO targets MUST be defined and measurable before PH1 starts. | `SUCCESS_METRICS_CONTRACT.md`, `PROGRAM_SLO_MATRIX.csv`, `MEASUREMENT_PLAN.md` |

### 4.2 PH1 - Backup, Restore, and DR

| Requirement ID | Ticket | Normative Requirement | Mandatory Evidence |
| --- | --- | --- | --- |
| PCG-RQ-010 | PCG-010 | Backup policy metadata MUST be versioned, queryable, and enforceable by deterministic rules. | `BACKUP_POLICY_SCHEMA.md`, `BACKUP_CATALOG_TABLES.csv`, `RETENTION_RULE_MATRIX.csv` |
| PCG-RQ-011 | PCG-011 | Full and incremental backup execution MUST support resumable deterministic job states. | `BACKUP_EXECUTION_STATE_MACHINE.md`, `BACKUP_JOB_RESULTS.md`, `BACKUP_THROUGHPUT_PROFILE.csv` |
| PCG-RQ-012 | PCG-012 | Restore validation and DR rehearsals MUST prove target RPO/RTO objectives with repeatable evidence. | `RESTORE_VALIDATION_MATRIX.csv`, `DR_REHEARSAL_RESULTS.md`, `RPO_RTO_REPORT.csv` |
| PCG-RQ-013 | PCG-013 | PITR extension support MAY be provided, but MUST remain optional and MUST NOT alter MGA core semantics. | `PITR_EXTENSION_DESIGN.md`, `LOG_RETENTION_AUDIT.csv`, `PITR_NEGATIVE_TEST_RESULTS.md` |
| PCG-RQ-014 | PCG-014 | SQL/operator contracts for backup/restore MUST be documented with runbooks and drill checklists. | `BACKUP_SQL_API_CONTRACT.md`, `RUNBOOK_BACKUP_RESTORE.md`, `OPERATOR_DRILL_CHECKLIST.csv` |

### 4.3 PH2 - Upgrade and Rollback Lifecycle

| Requirement ID | Ticket | Normative Requirement | Mandatory Evidence |
| --- | --- | --- | --- |
| PCG-RQ-020 | PCG-020 | On-disk format versions MUST be explicit, and compatibility decisions MUST be deterministic. | `FORMAT_VERSION_REGISTRY.md`, `COMPATIBILITY_MATRIX.csv`, `UPGRADE_BLOCKER_RULES.md` |
| PCG-RQ-021 | PCG-021 | Rolling upgrade orchestration MUST enforce safe node transition ordering and mixed-version guardrails. | `ROLLING_UPGRADE_STATE_MACHINE.md`, `NODE_DRAIN_JOIN_TEST_RESULTS.md`, `MIXED_VERSION_COMPAT_AUDIT.csv` |
| PCG-RQ-022 | PCG-022 | Downgrade/rollback MUST fail closed on incompatible states using explicit checkpoint contracts. | `ROLLBACK_DECISION_MATRIX.csv`, `DOWNGRADE_NEGATIVE_RESULTS.md`, `CHECKPOINT_MARKER_SCHEMA.md` |
| PCG-RQ-023 | PCG-023 | Release channels, LTS cadence, and deprecation notice policy MUST be published and enforceable. | `RELEASE_CHANNEL_POLICY.md`, `LTS_SUPPORT_MATRIX.csv`, `DEPRECATION_NOTICE_POLICY.md` |
| PCG-RQ-024 | PCG-024 | Upgrade and rollback rehearsal suites MUST pass before PH3 entry. | `UPGRADE_REHEARSAL_RESULTS.md`, `COMPATIBILITY_CERT_REPORT.md`, `ROLLBACK_DRILL_RESULTS.md` |

### 4.4 PH3 - Security and Governance

| Requirement ID | Ticket | Normative Requirement | Mandatory Evidence |
| --- | --- | --- | --- |
| PCG-RQ-030 | PCG-030 | Control and data plane communication MUST enforce approved TLS policy and cert rotation workflows. | `TLS_PROFILE_MATRIX.csv`, `CERT_ROTATION_RUNBOOK.md`, `MUTUAL_TLS_TEST_RESULTS.md` |
| PCG-RQ-031 | PCG-031 | At-rest encryption and key lifecycle management MUST be policy-bound and rotation-tested. | `ENCRYPTION_KEY_LIFECYCLE.md`, `KMS_INTEGRATION_AUDIT.csv`, `KEY_ROTATION_RESULTS.md` |
| PCG-RQ-032 | PCG-032 | Security audit logs MUST be immutable and tamper-evident. | `AUDIT_EVENT_SCHEMA.md`, `TAMPER_EVIDENCE_HASH_CHAIN.md`, `AUDIT_IMMUTABILITY_TEST_RESULTS.md` |
| PCG-RQ-033 | PCG-033 | Authorization defaults MUST enforce least privilege using deterministic RBAC/ABAC policy behavior. | `AUTHZ_POLICY_MATRIX.csv`, `DEFAULT_ROLE_BASELINE.md`, `PRIVILEGE_ESCALATION_TESTS.md` |
| PCG-RQ-034 | PCG-034 | Sensitive data in logs/support artifacts MUST be redacted by documented masking policy. | `DATA_MASKING_POLICY.md`, `REDACTION_COVERAGE_MATRIX.csv`, `SECURE_LOGGING_RESULTS.md` |
| PCG-RQ-035 | PCG-035 | Retention and legal-hold behavior MUST be explicit, auditable, and resistant to unsafe purge operations. | `RETENTION_POLICY_CONTRACT.md`, `LEGAL_HOLD_STATE_MACHINE.md`, `PURGE_GUARD_TEST_RESULTS.md` |
| PCG-RQ-036 | PCG-036 | Incident response and vulnerability remediation workflows MUST be documented with explicit SLA ownership. | `INCIDENT_RESPONSE_PLAYBOOK.md`, `VULNERABILITY_SLA_MATRIX.csv`, `ESCALATION_TREE.md` |

### 4.5 PH4 - Conformance and Migration Continuity

| Requirement ID | Ticket | Normative Requirement | Mandatory Evidence |
| --- | --- | --- | --- |
| PCG-RQ-040 | PCG-040 | A deterministic conformance harness MUST execute per-engine parity vectors and diff oracles. | `CONFORMANCE_VECTOR_REGISTRY.csv`, `HARNESS_EXECUTION_MODEL.md`, `DIFF_ORACLE_POLICY.md` |
| PCG-RQ-041 | PCG-041 | UDF connector certification for fourteen engines MUST publish pass/fail evidence per engine/version lane. | `UDF_CONNECTOR_CERT_MATRIX.csv`, `CONNECTOR_GAP_REPORT.md`, `PER_ENGINE_GATE_RESULTS.md` |
| PCG-RQ-042 | PCG-042 | Parser emulation certification for thirteen engines MUST publish coverage and gap-closure logs. | `PARSER_CERT_MATRIX.csv`, `SURFACE_COVERAGE_REPORT.md`, `PARSER_GAP_REMEDIATION_LOG.csv` |
| PCG-RQ-043 | PCG-043 | Post-cutover CDC divergence MUST be detected and reconciled by deterministic policy. | `CDC_STATE_MODEL.md`, `DIVERGENCE_RECONCILIATION_RULES.md`, `SYNC_LAG_SLO_REPORT.csv` |
| PCG-RQ-044 | PCG-044 | Migration cutover/rollback orchestration MUST be safe, reproducible, and auditable. | `CUTOVER_ROLLBACK_STATE_MACHINE.md`, `MIGRATION_FAILOVER_RESULTS.md`, `CUTOVER_AUDIT_TRAIL.csv` |
| PCG-RQ-045 | PCG-045 | Migration telemetry and evidence index MUST be complete for operational and audit review. | `MIGRATION_METRIC_NAMESPACE.csv`, `MIGRATION_DASHBOARD_SPEC.md`, `MIGRATION_EVIDENCE_INDEX.csv` |

### 4.6 PH5 - Operability and Reliability

| Requirement ID | Ticket | Normative Requirement | Mandatory Evidence |
| --- | --- | --- | --- |
| PCG-RQ-050 | PCG-050 | Service-level objectives and error-budget policy MUST be explicit and service-tiered. | `SLO_CATALOG.md`, `ERROR_BUDGET_POLICY.md`, `SERVICE_TIER_MATRIX.csv` |
| PCG-RQ-051 | PCG-051 | Alerting, dashboards, and readiness-health model MUST be production complete and deterministic. | `ALERT_RULEBOOK.md`, `DASHBOARD_INVENTORY.md`, `HEALTH_MODEL_STATE_MACHINE.md` |
| PCG-RQ-052 | PCG-052 | Support bundle generation MUST provide forensic completeness with PII redaction enforcement. | `SUPPORT_BUNDLE_MANIFEST.md`, `FORENSIC_CAPTURE_POLICY.md`, `PII_REDACTION_AUDIT.csv` |
| PCG-RQ-053 | PCG-053 | Fault-injection and chaos suites MUST cover critical failure modes with objective pass criteria. | `FAULT_INJECTION_SCENARIOS.csv`, `CHAOS_AUTOMATION_RESULTS.md`, `FAILURE_MODE_COVERAGE.csv` |
| PCG-RQ-054 | PCG-054 | Soak and capacity gates MUST establish long-run stability and practical capacity boundaries. | `SOAK_TEST_RESULTS.md`, `CAPACITY_LIMIT_REPORT.md`, `LONG_RUN_STABILITY_AUDIT.csv` |
| PCG-RQ-055 | PCG-055 | Operations runbooks and on-call escalation protocols MUST be complete and drill-backed. | `OPERATIONS_RUNBOOK_INDEX.md`, `ON_CALL_ESCALATION_PROTOCOL.md`, `DRILL_CALENDAR.csv` |

### 4.7 PH6 - Supply-Chain and Compliance Closure

| Requirement ID | Ticket | Normative Requirement | Mandatory Evidence |
| --- | --- | --- | --- |
| PCG-RQ-060 | PCG-060 | Every release candidate MUST include validated SBOM and dependency inventory artifacts. | `SBOM_GENERATION_PIPELINE.md`, `DEPENDENCY_INVENTORY.csv`, `SBOM_VALIDATION_RESULTS.md` |
| PCG-RQ-061 | PCG-061 | Release artifacts MUST be signed with verifiable provenance attestations. | `ARTIFACT_SIGNING_POLICY.md`, `PROVENANCE_ATTESTATION_SCHEMA.md`, `SIGNATURE_VERIFICATION_RESULTS.md` |
| PCG-RQ-062 | PCG-062 | CVE intake and patching MUST meet explicit SLA automation and exception-management policy. | `CVE_INTAKE_WORKFLOW.md`, `PATCH_SLA_REPORT.csv`, `EXCEPTION_REGISTER.md` |
| PCG-RQ-063 | PCG-063 | Build process MUST support reproducible outputs with drift detection and manifest verification. | `REPRODUCIBLE_BUILD_CONTRACT.md`, `MANIFEST_VERIFICATION_RESULTS.md`, `BUILD_DRIFT_AUDIT.csv` |
| PCG-RQ-064 | PCG-064 | Compliance/legal packaging MUST be complete and auditable before release distribution. | `LEGAL_NOTICE_BUNDLE.md`, `COMPLIANCE_CHECKLIST.md`, `PACKAGE_AUDIT_RESULTS.csv` |

### 4.8 PH7 - Integrated Launch Signoff

| Requirement ID | Ticket | Normative Requirement | Mandatory Evidence |
| --- | --- | --- | --- |
| PCG-RQ-070 | PCG-070 | Integrated gameday scenarios MUST validate reliability, security, and migration behavior as one system. | `INTEGRATED_GAMEDAY_PLAN.md`, `GAMEDAY_EXECUTION_RESULTS.md`, `RESIDUAL_RISK_REGISTER.csv` |
| PCG-RQ-071 | PCG-071 | Performance/recovery/upgrade release gates MUST satisfy objective thresholds with no unresolved critical blockers. | `RELEASE_GATE_SUMMARY.md`, `PERFORMANCE_SLO_REPORT.csv`, `RECOVERY_UPGRADE_REPORT.md` |
| PCG-RQ-072 | PCG-072 | Final launch readiness and support handoff package MUST be complete and approved. | `LAUNCH_READINESS_CHECKLIST.md`, `SUPPORT_HANDOFF_PACKET.md`, `FINAL_SIGNOFF.md` |

## 5) Gate Criteria

1. A gate is `pass` only when all required artifacts listed in `GATE_EVIDENCE_MATRIX.csv` exist, validate, and are checksummed.
2. Any unresolved critical risk in `RISK_DECISION_LOG.md` is gate-blocking.
3. Missing negative-path evidence is gate-blocking.
4. `PH6` and `PH7` gate failures are launch-blocking with no automatic override.

## 6) Cross-Cutting Test Contract

1. Every ticket MUST include: functional, negative-path, and evidence-integrity checks.
2. Performance-sensitive tickets MUST include threshold-based reports.
3. Security-sensitive tickets MUST include misuse/adversarial tests.
4. Operational tickets MUST include drill execution evidence.

## 7) Change Control and Drift Prevention

1. Any change to ticket dependencies requires updates to both `ORDERED_TASK_TICKETS.csv` and `DEPENDENCY_GRAPH.csv`.
2. Any gate artifact change requires synchronized updates to `GATE_EVIDENCE_MATRIX.csv`.
3. A ticket cannot be re-scoped without updating this specification and the phase workplan.
4. Scope reductions require explicit risk acceptance in `RISK_DECISION_LOG.md`.

## 8) Completion Definition

The product-completion program is complete only when:

1. All tickets `PCG-001..PCG-072` are `done` in trackers.
2. All gates `PCG-GATE-00..PCG-GATE-07` are passed with checksummed evidence.
3. No unresolved critical risks remain.
4. Launch handoff artifacts are approved and archived.
