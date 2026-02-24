# UDR-CAT-002 Migration and Compatibility Plan
Last-Modified: 2026-02-23

## Scope
Forward-compatible catalog/runtime migration design to close prerequisite deltas identified in:
- `artifacts/udr/catalog/p6s1w1/udr-cat-001-delta-matrix.md`

Gate target:
- `UDR-CAT-GATE-02`

## Migration Principles
1. Additive-first changes for on-disk compatibility.
2. No destructive page layout rewrite in the first migration wave.
3. Runtime enforcement introduced behind deterministic validators.
4. Preserve replayability and rollback safety via feature flags and compatibility shims.

## Forward Migration Set

### UDR-CAT-MIG-001: Connector state model normalization
Type: `ALTER` + `CONSTRAINT`

Changes:
1. Replace remote state semantic model from `DISABLED/PROBING/READY/DEGRADED/FAILED` to canonical `REGISTERED/VALIDATED/READY/DEGRADED/DISABLED`.
2. Add strict transition validator in connector state update path.
3. Add transition audit table `remote_connector_state_history` (append-only) with:
   - `transition_uuid`, `remote_connector_uuid`, `from_state`, `to_state`, `request_uuid`, `transition_time`, `actor_uuid`, `reason_code`.

Upgrade mapping:
1. `PROBING -> VALIDATED`
2. `FAILED -> DISABLED`
3. Existing `READY/DEGRADED/DISABLED` map 1:1.

Downgrade safety:
1. Preserve old enum values in compatibility parser for one cycle.
2. Export downgraded state snapshot using reverse map (`VALIDATED -> PROBING`, `REGISTERED -> PROBING`) for rollback tooling.

### UDR-CAT-MIG-002: Metadata snapshot immutability enforcement
Type: `CONSTRAINT` + `RUNTIME_ENFORCEMENT`

Changes:
1. Disallow mutation of snapshot rows when existing status is terminal (`COMPLETE/FAILED/CANCELLED`).
2. Replace mutation pattern with new-seq insertion for rerun metadata refresh.
3. Restrict snapshot delete operation to maintenance GC path only (not control API).

Upgrade rule:
1. Existing rows remain valid; no rewrite needed.

Downgrade safety:
1. Keep old delete/update entry points behind compatibility macro but disabled by default.

### UDR-CAT-MIG-003: Execution audit append-only terminal contract
Type: `CONSTRAINT` + `RUNTIME_ENFORCEMENT`

Changes:
1. Introduce insert-only API (`insertRemoteExecutionAuditCatalogEntry`) and deprecate upsert for this family.
2. Forbid delete from control-plane path.
3. Enforce one terminal row per `(remote_connector_uuid, request_uuid)` with no post-terminal update.

Upgrade rule:
1. Existing rows retained; backfill `terminal_write=true` marker as default.

Downgrade safety:
1. Keep read APIs unchanged; optional compatibility bridge can still expose upsert signature but reject updates.

### UDR-CAT-MIG-004: Remote txn binding terminal lifecycle hardening
Type: `CONSTRAINT` + `RUNTIME_ENFORCEMENT`

Changes:
1. Enforce immutability after `terminal_time` is set.
2. Add required local transaction terminal callback hook that closes or tombstones open binding rows.
3. Add invariant checker that rejects open remote binding rows for terminal local tx states.

Upgrade rule:
1. Backfill `terminal_time` for rows whose local transaction is already terminal.

Downgrade safety:
1. Rows with `terminal_time` remain compatible with current schema.

### UDR-CAT-MIG-005: Connector+policy atomic disable transition
Type: `ADD` + `RUNTIME_ENFORCEMENT`

Changes:
1. Add atomic API: `setRemoteConnectorDisabledWithPolicy(remote_connector_id, policy_id, reason_code, request_uuid)`.
2. Commit connector state + policy transition in one catalog write transaction.

Upgrade rule:
1. No data rewrite required.

Downgrade safety:
1. Fallback to current separate writes if feature flag disabled.

### UDR-CAT-MIG-006: Capability lineage + attestation fields
Type: `ADD`

Changes:
1. Add `remote_capability_snapshot` header table:
   - `capability_snapshot_uuid`, `remote_connector_uuid`, `snapshot_seq`, `source_version_text`, `capability_hash`, `captured_time`, `is_active`.
2. Add `capability_snapshot_uuid` FK field to `remote_connector_capability`.
3. Extend `remote_connector` with attestation fields:
   - `module_signature`, `module_signer`, `attestation_status`, `attested_time`, `allowlist_profile`.

Upgrade rule:
1. Create one synthetic lineage snapshot per connector from current capability rows.
2. Set `attestation_status=PENDING` for existing connectors.

Downgrade safety:
1. Added tables/columns are optional for legacy readers.

### UDR-CAT-MIG-007: Correlation and redaction controls
Type: `ADD` + `RUNTIME_ENFORCEMENT`

Changes:
1. Extend `remote_execution_audit` with parser/control correlation fields:
   - `profile_id`, `feature_key`, `source_map_hash`, `session_chain_hash`.
2. Add redaction policy flags on audit/error write path:
   - `redaction_policy_id`, `redaction_applied`.
3. Enforce forbidden payload validation on error/audit text fields.

Upgrade rule:
1. Backfill new fields with null/default where unavailable.

Downgrade safety:
1. Legacy readers ignore unknown fields.

### UDR-CAT-MIG-008: Secret handling write-only enforcement
Type: `CONSTRAINT` + `RUNTIME_ENFORCEMENT`

Changes:
1. Encrypt `user_mapping.remote_credentials` using active encryption key manager before TOAST write.
2. Remove plain credential readback from public mapping getters; return opaque token/metadata only.
3. Add credential redaction validator for all error and audit paths.

Upgrade rule:
1. On first read/write after migration, detect legacy plaintext and re-encrypt in-place.

Downgrade safety:
1. Store encryption metadata version so rollback path can detect unsupported payloads.

## Compatibility Contract
1. Public read APIs remain available for non-secret metadata.
2. Deprecation window:
   - `deleteRemoteExecutionAuditCatalogEntry`, `deleteRemoteMetadataSnapshotCatalogEntry`, `deleteRemoteTxnBindingCatalogEntry` become maintenance-only in next cycle.
3. Compatibility macros guard old behavior until `UDR-CAT-GATE-05` closure.

## Migration Order and Dependencies
1. Run `MIG-001` first (state semantics affect all lifecycle logic).
2. Run `MIG-002` + `MIG-003` + `MIG-004` next (immutability and append-only contracts).
3. Run `MIG-005` (atomic disable contract) after `MIG-001`.
4. Run `MIG-006` and `MIG-007` in parallel once state/lifecycle constraints are stable.
5. Run `MIG-008` last in this gate because it touches credential payload transformations.

## Evidence Required for Gate Closure
1. Schema diff document (`before/after`).
2. Deterministic upgrade replay with sample pre-migration rows.
3. Rollback safety proof for additive columns/tables.
4. Constraint validation replay for immutable snapshot, append-only audit, and terminal txn binding.

## UDR-CAT-002 Gate Decision
1. `UDR-CAT-GATE-02`: PASS (forward migration and rollback plan published).
2. `UDR-G-001` may now be unlocked (catalog prereq gate 02 satisfied).
3. Engine lanes remain blocked by `UDR-CAT-GATE-03` + `UDR-GATE-03` prerequisites.
