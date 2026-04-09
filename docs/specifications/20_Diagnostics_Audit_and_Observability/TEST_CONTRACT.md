# Test Contract - 20_Diagnostics_Audit_and_Observability

## Required tests
- `test_observability_metric_contract.cpp` proves the canonical `sb_*` metric policy and required metric registration
- `test_observability_sql_views.cpp` proves the current SQL-view inventory and key schema fields for MGA, buffer, checkpoint, recovery, writeback, and sweep-resume surfaces
- `test_audit_logger.cpp` proves append-only audit-chain verification, deterministic package export and validation, legal hold, retention evaluation, and tamper detection
- `test_operational_support_bundle.cpp` proves alert-readiness SQL surfaces, redaction-enforced bundle generation, and retained forensic references in support-bundle output
- `test_forensic_replay_sessions.cpp` proves replay uses retained snapshot boundaries, fails closed when no retained snapshot exists, and can resolve historical schema across committed DDL
- secret-bearing text and fields are redacted from exported local audit packages and structured diagnostics payloads

## Negative tests
- non-canonical metric names and forbidden labels are rejected by policy audit
- out-of-sequence audit-chain append is rejected
- out-of-sequence audit-export segment append is rejected
- tampered audit record or tampered export package fails verification
- legal hold blocks retention expiry eligibility until release
- replay without retained local evidence fails closed

## Explicit open gaps
- full remote or object sink execution parity
- independent `sb_btree_*` operator diagnostics
- broader standalone page-walker service surface
- privileged de-redaction workflows beyond the current audited redaction substrate
- any claim that all prose-only dashboards are directly proven runtime contracts

## Beta 2 required proof additions
- error registry generation proves UUID uniqueness, stable-symbol uniqueness,
  and deterministic UUIDv5 derivation for every admitted engine error row
- static-text eradication scan proves cataloged engine error paths do not keep
  client-visible prose inline
- native render-pack validation proves every admitted registry row has one
  native ScratchBird render row
- donor error-code packet validation proves every enabled donor family has one
  current source-backed reference packet entry
- workload capture export validation proves sensitive fields are classified,
  redaction actions are recorded, and replay packs do not silently export
  protected plaintext outside admitted policy
- tamper-evident ledger proof validates digest-chain continuity, verifier-run
  persistence, and attestation-export refusal behavior
