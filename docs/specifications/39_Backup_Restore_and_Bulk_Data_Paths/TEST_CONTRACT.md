# Section 39 Test Contract

Section `39` is implementation-ready only if maintained evidence covers the
current backup, restore, and bulk-data behaviors it claims.

## Required certification lanes

- backup and restore
  - backup creation, verification, and restore validation obey the documented
    current MGA-only contract
  - unsupported WAL or log-replay restore semantics are rejected
- bulk import and export
  - current copy, import, or export surfaces obey the documented format and
    transactional boundaries
  - retail micro-batch, sorted exact bulk, and shadow-load cutover each obey
    the documented lineage, publication, and validation rules
- snapshot and retention
  - any claimed snapshot-like artifact or retention behavior is backed by
    documented current evidence
  - Beta 2 PITR chains, archive replay targets, and object-store snapshot
    commits obey the documented continuity and refusal rules
- portability and externalization
  - export or restore portability limits are enforced fail closed where the
    section says they are limited
- native changefeed proofs show commit-envelope publication, cursor resume, and
  retention-expiry refusal behavior
- transactional blob or file namespace proofs show path atomicity, object
  retention, and orphan refusal behavior
- operator-facing guarantees
  - no HA or DR platform equivalence is claimed unless explicitly certified in
    this section

## Negative requirements

- no test may infer enterprise retention or replica-grade disaster-recovery
  breadth from section `39` unless explicitly stated
- no test may infer timestamp-based WAL-style replay semantics
- no test may infer donor `NOLOGGING` or relaxed-durability behavior for bulk
  ingest
