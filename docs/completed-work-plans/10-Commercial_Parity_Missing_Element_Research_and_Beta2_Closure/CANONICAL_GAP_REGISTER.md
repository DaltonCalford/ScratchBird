# Canonical Gap Register

## CPG-10-G01 Transparent At-Rest Encryption and Rekey

- Status: open
- Current bounded proof:
  - `docs/specifications/19_Security_Model/DOMAIN_SECURITY_MASKING_ENCRYPTION_AND_AUDIT_MODEL.md`
    + `encryption metadata are stored with the domain security payload`
- Gap:
  - no canonical TDE-style page, filespace, index, TOAST/LOB, backup, snapshot,
    restore, or rekey lifecycle
- Closing tickets:
  - `CPG-10-002`
  - `CPG-10-003`

## CPG-10-G02 Protected-Query Encryption and Enclave-Safe Execution

- Status: open
- Current bounded proof:
  - no current canonical file owns Always-Encrypted-style execution, enclaves,
    or searchable encryption semantics
- Gap:
  - no parser/engine/security contract for encrypted parameter handling,
    protected comparisons, enclave-safe operators, or keyed search posture
- Closing tickets:
  - `CPG-10-004`
  - `CPG-10-005`

## CPG-10-G03 HA / DR / PITR / Clustered Failover

- Status: partial_fail_closed
- Current bounded proof:
  - `docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAULT_TOLERANCE_NON_GUARANTEES.md`
    + `no transparent failover or session migration guarantee`
  - `docs/specifications/08_Transaction_Core/FAILURE_MODEL_AND_RECOVERY_CLASSIFICATION.md`
    + `This file does not define PITR`
- Gap:
  - no complete commercial-grade HA/DR/PITR/failover canon with MGA authority
    preserved
- Closing tickets:
  - `CPG-10-006`
  - `CPG-10-007`

## CPG-10-G04 Hard Multi-Tenant Isolation, Quotas, Reservations, and QoS

- Status: partial_fail_closed
- Current bounded proof:
  - `docs/specifications/38_Workload_Governance_and_Parallelism/RESOURCE_ISOLATION_AND_FAIRNESS_BOUNDARY.md`
    + `no hard tenant isolation guarantee`
    + `no full quota and reservation system`
- Gap:
  - no hard tenant-grade isolation, reservations, or enterprise QoS guarantee
- Closing tickets:
  - `CPG-10-008`
  - `CPG-10-009`

## CPG-10-G05 Archive Tier / ILM / Legal Hold / Replay From Archive

- Status: partial_drift
- Current bounded proof:
  - `docs/specifications/10_GC_and_Sweep/SWEEP_ARCHIVE_AND_RETENTION_POLICY.md`
    + `It does not prove the broad archive-transfer and verified archive-before-prune pipeline`
- Gap:
  - no fully proven or canonically narrowed archive-tier and replay-from-archive
    model
- Closing tickets:
  - `CPG-10-010`
  - `CPG-10-011`

## CPG-10-G06 Production Workload Capture, Replay, and Rehearsal

- Status: open
- Current bounded proof:
  - `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/WEAK_DONOR_CHANGE_CAPTURE_REPLAY_QUARANTINE_AND_RESUME_MODEL.md`
    + `managed migration stream`
- Gap:
  - no production workload capture/replay/rehearsal model for upgrades,
    regressions, or incident replay
- Closing tickets:
  - `CPG-10-012`
  - `CPG-10-013`

## CPG-10-G07 Open Table Formats and Object-Store Table Semantics

- Status: open
- Current bounded proof:
  - `docs/specifications/02_Filespace_Lifecycle/TABLESPACE_DDL_AND_OPERATOR_LIFECYCLE_MODEL.md`
    + `Attach external tablespace file`
  - `docs/specifications/28_Parser_Implementations/EMULATED_ENGINE_PACKAGE_MODEL.md`
    + `external table or equivalent family-specific support objects`
- Gap:
  - no explicit object-store table, snapshot manifest, refresh, schema
    evolution, or predicate-pushdown canon
- Closing tickets:
  - `CPG-10-014`
  - `CPG-10-015`

## Bounded Scope Rule

Only the seven gaps above are in direct closure scope for this package.

If research uncovers a prerequisite not already listed here:

- record it in `RISK_DECISION_LOG.md`
- tie it to the affected gap ticket
- do not silently expand the package into unrelated feature families
