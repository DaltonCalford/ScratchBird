# Section 08 Specification Outline

## Status
Authoritative current outline.

## Canonical scope
Section `08` is the transaction-core authority for:
- transaction lifecycle and publication semantics
- snapshot and record visibility rules
- runtime savepoint semantics
- TIP-page and transaction-map layout and restart-repair semantics
- checkpoint and startup decision behavior inside the transaction core
- transaction-side backup and restore guarantee boundaries
- lineage, schema-epoch provenance, and forensic replay binding

Section `08` is not the primary operational owner for:
- backup and restore execution runbooks owned elsewhere
- tooling-specific operator surfaces owned by client or orchestration sections
- durable subtransaction identity, which is not part of the current contract

## Canonical file map
1. Transaction lifecycle and publication:
[TRANSACTION_LIFECYCLE.md](TRANSACTION_LIFECYCLE.md), [MGA_TRANSACTION_PUBLICATION_AND_RESTART_SEMANTICS.md](MGA_TRANSACTION_PUBLICATION_AND_RESTART_SEMANTICS.md), [SNAPSHOT_HORIZON_AND_TRANSACTION_INVENTORY.md](SNAPSHOT_HORIZON_AND_TRANSACTION_INVENTORY.md)
2. Visibility and savepoint semantics:
[RECORD_VISIBILITY_RULES.md](RECORD_VISIBILITY_RULES.md), [SAVEPOINT_AND_SUBTRANSACTION_SEMANTICS.md](SAVEPOINT_AND_SUBTRANSACTION_SEMANTICS.md)
3. Durable transaction-map and restart contract:
[TRANSACTION_MAP_LAYOUT.md](TRANSACTION_MAP_LAYOUT.md), [CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md](CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md), [STARTUP_RECOVERY.md](STARTUP_RECOVERY.md)
4. Recovery boundary and validation linkage:
[FAILURE_MODEL_AND_RECOVERY_CLASSIFICATION.md](FAILURE_MODEL_AND_RECOVERY_CLASSIFICATION.md), [ALPHA_DURABILITY_MODES_AND_FLUSH_ORDERING.md](ALPHA_DURABILITY_MODES_AND_FLUSH_ORDERING.md), [BACKUP_RESTORE_SUPPORT_MATRIX_AND_VALIDATION.md](BACKUP_RESTORE_SUPPORT_MATRIX_AND_VALIDATION.md)
5. Lineage, context, and replay:
[TRANSACTION_CONTEXT_MAPPING.md](TRANSACTION_CONTEXT_MAPPING.md), [TRANSACTION_LINEAGE_AND_PROVENANCE_MODEL.md](TRANSACTION_LINEAGE_AND_PROVENANCE_MODEL.md), [FORENSIC_SNAPSHOT_CAPSULES_VISIBILITY_AND_SCHEMA_REPLAY.md](FORENSIC_SNAPSHOT_CAPSULES_VISIBILITY_AND_SCHEMA_REPLAY.md)
6. Support surfaces:
[TEST_CONTRACT.md](TEST_CONTRACT.md), [DEPENDENCIES.md](DEPENDENCIES.md), [DECISION_RECORD.md](DECISION_RECORD.md)

## Implementation closure rules
1. Savepoint semantics are authoritative in the core runtime.
2. Unsupported savepoint entry points must fail closed rather than degrade semantics.
3. Restart classification vocabulary is owned here even though execution is split across `Database` and `TransactionManager`.
4. Backup and restore sections may reference these rules but may not redefine transaction truth.
