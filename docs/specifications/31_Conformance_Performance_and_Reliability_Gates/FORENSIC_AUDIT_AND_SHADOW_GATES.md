# Forensic Audit and Shadow Gates

Status: current_authority_with_reconstructed_expansion

## Purpose
Define the mandatory gate package for replay correctness, schema-shape replay,
audit retention, security-action audit emission, sink delivery semantics,
page-spot audit evidence, remote-management event lineage, and shadow capture
integrity.

## Scope
- retention correctness gates
- replay correctness and schema equivalence gates
- audit/export sink gates
- security and management audit lineage gates
- page-audit evidence gates
- shadow-capture and rebuild-evidence gates

## Hard Invariants
1. A passing replay gate requires both row visibility correctness and schema-shape correctness.
2. Gate success may rely on archived evidence, but archive resolution must be deterministic.
3. External sink success never substitutes for missing local immutable evidence.
4. Shadow capture is validated as derivative rebuild or audit evidence, not as recovery truth.
5. Security-policy changes, grants, role changes, group changes, masking changes, and remote-management mutation events must emit deterministic local audit evidence before any derivative shipping claim is considered complete.

## Gate Families
### T31-FOR-A Retention Correctness
- `T31-FOR-A01`: replay-required lineage, DDL lineage, security-policy lineage, and schema-epoch references survive until retention expiry.
- `T31-FOR-A02`: prune is blocked while required replay, schema history, or security-policy lineage remains within active retention window.
- `T31-FOR-A03`: archive relocation preserves selector resolution and checksum-chain continuity.

### T31-FOR-B Replay Equivalence
- `T31-FOR-B01`: replay by `tx_uuid`, `txid`, `commit_seqno`, and capsule id resolve to equivalent historical boundary when they reference the same commit.
- `T31-FOR-B02`: repeated replay under the same selector produces equivalent result sets and metadata.
- `T31-FOR-B03`: timestamp selector ambiguity is rejected deterministically.

### T31-FOR-C Schema-Shape Replay Equivalence
- `T31-FOR-C01`: replay before and after transactional DDL exposes the correct historical column and layout shape.
- `T31-FOR-C02`: dropped or renamed historical objects remain queryable only when retained schema lineage permits it.
- `T31-FOR-C03`: replay fails deterministically when required historical schema definitions were pruned.

### T31-FOR-D Audit and Sink Integrity
- `T31-FOR-D01`: local immutable evidence exists before downstream export for non-`NORMAL` lanes.
- `T31-FOR-D02`: external sink failure does not invalidate local retained evidence.
- `T31-FOR-D03`: strict audit policy blocks maintenance completion or export completion exactly where policy requires it.
- `T31-FOR-D04`: DDL payload redaction and expansion follow security policy and are auditable.
- `T31-FOR-D05`: derivative queue-state outputs preserve queue depth, oldest pending age, retryable failure count, quarantined failure count, and active backpressure class with stable schema.
- `T31-FOR-D06`: retry and quarantine actions preserve source identity, delivery-order identity, and sink-profile identity.
- `T31-FOR-D07`: `KAFKA_CHANNEL` and `REMOTE_DATABASE` sinks preserve their declared ordering scope without misclassifying sink acknowledgement as local commit truth.

### T31-FOR-E Security and Management Audit Lineage
- `T31-FOR-E01`: security grants, revokes, role changes, group membership changes, and policy changes emit deterministic audit rows with principal, object, scope, and commit-bound epoch identifiers.
- `T31-FOR-E02`: row-level security, column security, domain masking, and unmask privilege actions are auditable and replay-resolvable.
- `T31-FOR-E03`: remote-management instruction create, assess, apply, refuse, quarantine, cancel, and acknowledge actions emit deterministic audit events linked to instruction identity and target generation.
- `T31-FOR-E04`: management audit rows remain distinguishable from ordinary DDL and DML history.

### T31-FOR-F Page Spot Audit
- `T31-FOR-F01`: page-spot audit findings are emitted with correct scan mode, trigger source, and page identifiers.
- `T31-FOR-F02`: sweep-triggered page findings never perform inline repair.
- `T31-FOR-F03`: corruption-class findings map to deterministic severity and error codes.

### T31-FOR-G Shadow Capture and Rebuild Evidence
- `T31-FOR-G01`: shadow-capture manifests are emitted only after eligibility is established.
- `T31-FOR-G02`: logical shadow captures reproduce retained object content deterministically.
- `T31-FOR-G03`: physical page-copy captures remain checksum-verifiable and policy-gated.
- `T31-FOR-G04`: shadow rebuild evidence never claims primary recovery authority.
- `T31-FOR-G05`: shadow-group readiness classification is deterministic across `ACTIVE`, `DEGRADED`, `PROMOTION_READY`, and `PROMOTED`.
- `T31-FOR-G06`: full-group promotion is refused when any required member shadow is missing, stale beyond policy, or unverifiable.
- `T31-FOR-G07`: failback evidence proves restore-style reconciliation rather than blind route reuse.

## Required Evidence Bundle Additions
Each applicable gate run MUST include:
- replay selector input and resolved replay boundary
- resolved schema epoch id
- lineage, security-lineage, and DDL-lineage manifest references
- retention-policy snapshot
- sink-profile snapshot when export lanes are active
- derivative queue-state snapshot when export lanes are active
- backpressure-class snapshot when export lanes are active
- checksum-linked evidence manifest set
- shadow-capture manifest references when applicable
- shadow-group state snapshot when shadow-group protection is active
- restore, promotion, or failback continuity marker snapshot when those boundaries are exercised
- security-policy epoch snapshot for any gate that mutates security or authorization state
- instruction identity and target-generation snapshot for any remote-management gate

## Current Code-Backed Entry Points
Current maintained evidence already exists through:
- `tests/conformance/security/run_security_parity_matrix.sh`
- `tests/conformance/v3_native_inet/sql/09_security_default_access_public.sql`
- `tests/conformance/v3_native_inet/sql/10_security_ownership_alter_owner.sql`
- `tests/conformance/v3_native_inet/sql/11_security_grants_dml_execute_view.sql`
- `tests/conformance/v3_native_inet/sql/12_security_show_visibility.sql`
- `tests/conformance/v3_native_inet/sql/13_security_row_level_security.sql`
- `tests/conformance/v3_native_inet/sql/14_security_column_level.sql`
- `tests/conformance/v3_native_inet/sql/15_security_domain_masking.sql`
- `tests/integration/test_domain_security.cpp`

## Reconstructed Required Expansion
The rebuilt canon additionally requires dedicated gate bundles for:
- remote-management instruction audit lineage
- cluster-dispatched policy changes
- shadow promotion and failback audit continuity
- derivative sink quarantine and replay evidence preservation

## Pass Criteria
1. all required family tests pass across 10 repeated runs
2. no unapproved schema-shape or security-lineage diff classifications remain
3. replay and shadow evidence bundles are checksum-valid
4. failure reproduction command and selector are recorded for every failure
5. derivative-lane gates prove local MGA durability health remained separable from derivative shipping health
6. security and management audit gates prove commit-bound policy epoch and instruction identity continuity

## Cross-Section References
- `08_Transaction_Core/FORENSIC_SNAPSHOT_CAPSULES_VISIBILITY_AND_SCHEMA_REPLAY.md`
- `19_Security_Model/AUDIT_AND_FORENSIC_ACCESS_POLICY.md`
- `20_Diagnostics_Audit_and_Observability/AUDIT_EXPORT_SINKS_RETENTION_AND_IMMUTABILITY.md`
- `24_Catalog_Model_and_Virtual_Overlays/REMOTE_MANAGEMENT_CATALOG_AND_DEPLOYMENT_RECORDS.md`
- `31_Conformance_Performance_and_Reliability_Gates/EVIDENCE_ARTIFACTS_AND_REPLAY_REQUIREMENTS.md`
