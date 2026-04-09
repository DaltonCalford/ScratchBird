# Subsystem Correctness Invariant Catalog

Status: current_authority

## Canonical invariants

| Claim ID | Invariant |
| --- | --- |
| GOV-INV-001 | runtime execution stays on the core plus SBLR path |
| GOV-INV-002 | shared durable ID remains UUIDv7-backed |
| GOV-INV-003 | visibility and recovery governance remain MGA-derived |
| GOV-INV-004 | lowering stays fail closed and SBLR-centered |
| GOV-INV-005 | lowering provenance remains persisted in catalog metadata |
| GOV-INV-006 | parser packages lower locally and remain optional and dependency-isolated |
| GOV-INV-007 | optimizer-visible index families remain parity-governed rather than primary-versus-secondary classes |

## Current authority summary

- scratchbird_core and scratchbird_sblr remain the runtime execution roots
- UuidV7Bytes and shared ID aliases remain authoritative for durable internal identity
- transaction-manager code remains authoritative for OIT and OAT and OST and wraparound governance
- dialect compiler entry points remain authoritative for front-door lowering
- catalog artifact metadata remains authoritative for persisted provenance
- parser packages remain front-door local lowering units and must not depend on each other for execution or lowering authority
- planner canon now requires all admitted index families to compete through typed metrics rather than silent secondary-class treatment

## Negative requirements

- parser front doors must not become runtime authorities
- no competing durable primary ID type may replace UUIDv7 silently
- redo-log governance must not become authoritative for current recovery semantics
- lowering failure must abort rather than execute through another path
- provenance must not live only in transient parser state
- one parser package must not delegate lowering or execution authority to another parser package
- no admitted index family may be silently demoted into a secondary or ignored optimizer class without explicit fail-closed reason

## Non-guarantees

- no claim is made here that every dialect surface in the repo has been exhaustively re-audited
- no claim is made here that one centralized code registry already enforces these invariants
- no claim is made here that existing tests fully cover each invariant
