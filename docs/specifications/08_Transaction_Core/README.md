# 08_Transaction_Core

## Purpose
Define the authoritative `MGA` transaction core, TIP-backed transaction truth, commit-sequence visibility, restart reconciliation, savepoint runtime semantics, and transaction-bound lineage surfaces.

## Status
Authoritative current implementation contract.

## Current implementation summary
Section `08` owns these code-backed truths:
- transaction truth is `MGA`, TIP, and catalog-state reconciliation, not `WAL` redo
- ScratchBird is always inside a transaction
- `COMMIT` and `ROLLBACK` immediately start the next transaction
- `TransactionManager` owns begin, commit, rollback, prepare, commit-prepared, rollback-prepared, snapshot, reclaim-horizon, and startup-normalization behavior
- record visibility is commit-sequence based, with prepared transactions remaining snapshot-invisible until durable resolution
- savepoints are authoritative runtime backout semantics in `ConnectionContext` and executor paths
- durable subtransaction identity is not part of the current contract
- TIP-page and transaction-map header, entry, corruption, and startup-repair behavior are canonical surfaces
- checkpoint and startup recovery execution is split across `Database` and `TransactionManager`, but the canonical restart vocabulary is owned here

## Implementable refusal boundaries
- Any front door that cannot execute savepoint statements correctly must reject them explicitly; it must not weaken the savepoint model.
- Backup and restore runbook ownership is outside section `08`, but transaction visibility, publication, and restart safety boundaries are owned here.
- Replay, forensic, and lineage surfaces must follow the same transaction and publication rules as all other metadata.

## Canonical surfaces
- [TRANSACTION_LIFECYCLE.md](TRANSACTION_LIFECYCLE.md)
- [MGA_TRANSACTION_PUBLICATION_AND_RESTART_SEMANTICS.md](MGA_TRANSACTION_PUBLICATION_AND_RESTART_SEMANTICS.md)
- [RECORD_VISIBILITY_RULES.md](RECORD_VISIBILITY_RULES.md)
- [SAVEPOINT_AND_SUBTRANSACTION_SEMANTICS.md](SAVEPOINT_AND_SUBTRANSACTION_SEMANTICS.md)
- [SNAPSHOT_HORIZON_AND_TRANSACTION_INVENTORY.md](SNAPSHOT_HORIZON_AND_TRANSACTION_INVENTORY.md)
- [TRANSACTION_MAP_LAYOUT.md](TRANSACTION_MAP_LAYOUT.md)
- [CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md](CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md)
- [STARTUP_RECOVERY.md](STARTUP_RECOVERY.md)
- [TRANSACTION_CONTEXT_MAPPING.md](TRANSACTION_CONTEXT_MAPPING.md)
- [TRANSACTION_LINEAGE_AND_PROVENANCE_MODEL.md](TRANSACTION_LINEAGE_AND_PROVENANCE_MODEL.md)
- [FORENSIC_SNAPSHOT_CAPSULES_VISIBILITY_AND_SCHEMA_REPLAY.md](FORENSIC_SNAPSHOT_CAPSULES_VISIBILITY_AND_SCHEMA_REPLAY.md)

## Primary code anchors
- `include/scratchbird/core/transaction_manager.h`
- `src/core/transaction_manager.cpp`
- `include/scratchbird/core/connection_context.h`
- `src/core/connection_context.cpp`
- `include/scratchbird/core/ondisk.h`
- `src/core/database.cpp`
- `include/scratchbird/core/catalog_manager.h`
- `src/core/catalog_manager.cpp`

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [ALPHA_DURABILITY_MODES_AND_FLUSH_ORDERING.md](ALPHA_DURABILITY_MODES_AND_FLUSH_ORDERING.md)
- [BACKUP_RESTORE_SUPPORT_MATRIX_AND_VALIDATION.md](BACKUP_RESTORE_SUPPORT_MATRIX_AND_VALIDATION.md)
- [CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md](CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md)
- [COMMON_TRANSACTION_MULTI_ATTACHMENT_AND_REATTACH_MODEL.md](COMMON_TRANSACTION_MULTI_ATTACHMENT_AND_REATTACH_MODEL.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [FAILURE_MODEL_AND_RECOVERY_CLASSIFICATION.md](FAILURE_MODEL_AND_RECOVERY_CLASSIFICATION.md)
- [FORENSIC_SNAPSHOT_CAPSULES_VISIBILITY_AND_SCHEMA_REPLAY.md](FORENSIC_SNAPSHOT_CAPSULES_VISIBILITY_AND_SCHEMA_REPLAY.md)
- [MGA_RECORD_STATE_AND_PUBLICATION_MODEL.md](MGA_RECORD_STATE_AND_PUBLICATION_MODEL.md)
- [MGA_TRANSACTION_PUBLICATION_AND_RESTART_SEMANTICS.md](MGA_TRANSACTION_PUBLICATION_AND_RESTART_SEMANTICS.md)
- [RECORD_VISIBILITY_RULES.md](RECORD_VISIBILITY_RULES.md)
- [SAVEPOINT_AND_SUBTRANSACTION_SEMANTICS.md](SAVEPOINT_AND_SUBTRANSACTION_SEMANTICS.md)
- `SECTION_CLOSURE_MATRIX.csv`
- [SNAPSHOT_HORIZON_AND_TRANSACTION_INVENTORY.md](SNAPSHOT_HORIZON_AND_TRANSACTION_INVENTORY.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [STARTUP_RECOVERY.md](STARTUP_RECOVERY.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [TRANSACTION_CONTEXT_MAPPING.md](TRANSACTION_CONTEXT_MAPPING.md)
- [TRANSACTION_LIFECYCLE.md](TRANSACTION_LIFECYCLE.md)
- [TRANSACTION_LINEAGE_AND_PROVENANCE_MODEL.md](TRANSACTION_LINEAGE_AND_PROVENANCE_MODEL.md)
- [TRANSACTION_MAP_LAYOUT.md](TRANSACTION_MAP_LAYOUT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
