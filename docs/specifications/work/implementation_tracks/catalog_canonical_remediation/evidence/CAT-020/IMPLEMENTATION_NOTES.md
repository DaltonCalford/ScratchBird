# Implementation Notes

Status: `Completed`

## Completed in this pass
- Extended CAT-020 with full root/bootstrap/backfill wiring for all security extension and PKI/crypto tables:
  - `auth_mapping`, `role_setting`, `security_label`, `security_class`
  - `cert_registry`, `private_key_store`, `trust_anchor`, `channel_cert_binding`
  - `cert_revocation`, `pki_distribution_state`, `trust_anchor_rollover`
  - `encryption_profile`, `encryption_key`, `encryption_key_shard`, `encryption_bootstrap_info`
- Added deterministic CAT-020 record contracts and CRUD APIs for remaining PKI families:
  - `private_key_store`
  - `channel_cert_binding`
  - `cert_revocation`
  - `pki_distribution_state`
  - `trust_anchor_rollover`
- Enforced deterministic constraints and validation checks:
  - strict enum validation (`CertKind`, `CertStatus`, `KeyMaterialKind`, `TlsVersion`, `RevocationSource`, `RevocationReason`, `PkiArtifactKind`, `DistributionState`, `RolloverPhase`)
  - uniqueness contracts:
    - `private_key_store`: `UNIQUE(cert_id)`
    - `channel_cert_binding`: `UNIQUE(channel_name, cert_kind)`
    - `cert_revocation`: `UNIQUE(cert_id, source_kind, revoked_time)`
    - `pki_distribution_state`: `UNIQUE(member_id, artifact_kind, artifact_id)`
    - `trust_anchor_rollover`: one active non-`COMPLETE` rollover per `rollover_group_id`
  - temporal invariants:
    - revocation expiry window ordering
    - PKI distribution attempt/success timestamp ordering
    - rollover quorum bounds and time-order rules
- Expanded CAT-020 unit coverage to exercise new CRUD paths and negative constraints for all newly added table families.
