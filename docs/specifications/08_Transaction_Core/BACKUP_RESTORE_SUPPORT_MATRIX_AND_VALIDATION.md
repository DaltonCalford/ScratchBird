# Backup Restore Support Matrix and Validation

## Purpose

This file defines the transaction-core guarantees that backup, restore, restore rehearsal, and restore validation shall preserve. Operational backup orchestration belongs to `BackupManager` and to the backup/restore section set. This file owns only the transaction guarantees that those flows must respect.

ScratchBird remains always in a transaction. Backup and restore flows shall not create a non-transactional execution mode.

## Owning Boundary

`BackupManager` owns backup artifact creation, rehearsal, restore execution, and operator workflow.

Section `08` owns transaction visibility, committed publication order, schema-epoch binding, forensic-capsule binding, prepared-state legality, and transaction-map correctness.

Section `35` owns startup recovery classification and reopen-time containment.

## Supported Backup and Restore Classes

### Physical Backup and Physical Restore

Physical backup and restore are supported when the artifact preserves the authoritative on-disk transaction map, control state, committed schema epochs, and catalog-backed forensic linkage required by the restored database.

### Restore Rehearsal

Restore rehearsal is supported. Rehearsal shall run the same structural and transaction-integrity validation that a real restore would require, while refusing to publish the rehearsed instance as the authoritative target database.

### Restore Validation

Restore validation is supported. Validation shall confirm that the artifact can reopen without violating transaction-map, schema-epoch, checkpoint, or corruption-containment rules.

### Point-in-Time Recovery

Point-in-time recovery based on WAL, redo logs, archived log streams, or LSN targets is not supported.

### WAL-Based Incremental Replay

WAL-based incremental replay is not supported.

## Transaction Guarantees a Backup Must Preserve

A backup artifact shall preserve the transaction-map durable state required to distinguish active, committed, rolled-back, and prepared transactions at restore time.

A backup artifact shall preserve committed schema epochs required for parser, catalog, and replay correctness.

A backup artifact shall preserve forensic snapshot capsule bindings and lineage bindings required for replay validation.

A backup artifact shall not claim committed visibility for any transaction that was not durably committed in the source database.

A backup artifact shall not erase evidence needed to classify repaired reopen drift, startup repair, or fail-closed corruption.

## Restore Validation Algorithm

1. Open the artifact in validation context.
2. Validate primary-file and fixed bootstrap page placement.
3. Validate transaction-map header fields, page counts, and entry encodings.
4. Validate committed publication order uniqueness and presence rules.
5. Validate prepared-state cross references against the durable prepared inventory.
6. Validate committed schema-epoch inventory and required catalog rows.
7. Validate lineage-to-forensic-capsule bindings required for replayable transactions.
8. Validate checkpoint and control-state coherence.
9. Produce one of the allowed restore classifications.

## Allowed Restore Classifications

### `RESTORE_VALID`

The artifact may be restored and reopened under normal startup rules.

### `RESTORE_VALID_REPAIRED`

The artifact is structurally restorable, but reopen requires startup repair that stays within the repairable classes owned by section `35`.

### `RESTORE_REHEARSAL_ONLY`

The artifact may be rehearsed for inspection but shall not be promoted as a valid production restore without further operator action.

### `RESTORE_REFUSED_CORRUPTION`

The artifact is refused because opening it would require unsupported recovery semantics or would violate transaction correctness.

## Required Refusal Cases

Restore shall be refused if the artifact would require WAL replay, redo replay, undo replay, or LSN-based ordering.

Restore shall be refused if a transaction is marked prepared without the required durable prepared-state evidence.

Restore shall be refused if a committed publication order is missing where commit visibility requires it.

Restore shall be refused if duplicate committed publication orders cannot be normalized under the repairable reopen-drift rules.

Restore shall be refused if committed schema epochs required for replay binding are missing.

Restore shall be refused if forensic capsule linkage required for replayable transactions is missing or contradictory.

## DDL and DML Preservation Rules

DDL and DML are both transaction-scoped. Restore validation shall treat committed schema publication and committed data publication as the same class of transaction truth.

A restored database shall not expose committed DDL that was not durably committed in the source database.

A restored database shall not expose committed DML that was not durably committed in the source database.

Uncommitted schema changes and uncommitted data changes shall not be promoted during restore.

## Rehearsal Rules

Rehearsal shall use the same transaction validation rules as a real restore.

Rehearsal may open the artifact in a bounded inspection context, but it shall not override any fail-closed corruption classification.

A successful rehearsal does not widen the transaction correctness guarantees of the artifact. It confirms only that the artifact passes the current validation contract.

## Explicit Non-Goals

This file does not define backup storage media, scheduling, retention, or operator policy.

This file does not define WAL-compatible PITR.

This file does not define cross-engine import semantics.
