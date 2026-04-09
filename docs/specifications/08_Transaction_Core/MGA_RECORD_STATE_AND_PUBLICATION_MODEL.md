# MGA Record State and Publication Model

Status: reconstructed_required_with_current_substrate

## Purpose

Own one coherent engine-wide MGA record-state model covering row-version state, publication order, snapshot visibility, reclaim legality, restart reconciliation, and cleanup hooks for indexes and oversized-value storage.

## Governing invariants

1. ScratchBird is always inside a transaction.
2. Durable truth is page image plus transaction inventory plus committed catalog state, not WAL replay.
3. Updates and deletes are version-producing operations, not in-place truth replacement.
4. A reader decides visibility from transaction state, snapshot state, version lineage, and statement rules.
5. Reclaim is legal only after no still-relevant snapshot can require the retired version.
6. Index and oversized-value cleanup remain subordinate to heap/version truth.

## Record-state vocabulary

| State | Meaning |
| --- | --- |
| `ABSENT` | no visible logical row version exists |
| `UNCOMMITTED_HEAD` | newest version created by a live transaction |
| `COMMITTED_HEAD_VISIBLE` | newest committed version visible to qualifying snapshots |
| `SUPERSEDED_BACKVERSION` | prior committed version retained for older snapshots or reclaim gating |
| `DELETE_INTENT_HEAD` | delete represented as a new transaction-owned head version |
| `ROLLED_BACK_GHOST` | version created by a rolled-back transaction; never authoritative for future snapshots |
| `PREPARED_OR_LIMBO` | prepared or limbo state blocks some publication/reclaim decisions until resolved |
| `RECLAIM_ELIGIBLE` | version proven unreachable by all relevant visibility and repair rules |
| `RECLAIMED` | physical storage retired after required cleanup hooks succeed |

## Publication order

### Insert

1. allocate row version
2. stamp creating transaction identity
3. generate or preserve stable row UUID
4. link version into lineage
5. keep version local to creator snapshot until commit publication

### Update

1. read current visible head or permitted write target
2. allocate successor version
3. copy or transform payload
4. preserve stable row UUID and lineage pointer to prior version
5. stage index delta and oversized-value reachability delta
6. publish only when transaction becomes committed

### Delete

1. represent delete as a new lineage head carrying delete intent
2. preserve stable row UUID and backversion link
3. keep prior committed version available to older snapshots
4. retire visibility only at commit publication

### Commit publication

1. force-write all required page images for the transaction’s durable state changes
2. force-write dependent structural updates required for durable lineage correctness
3. transition the transaction inventory state to committed
4. start the next transaction immediately

### Rollback publication

1. mark the transaction inventory as rolled back
2. leave rolled-back versions non-authoritative for future snapshots
3. start the next transaction immediately
4. defer physical retirement to reclaim legality

## Snapshot semantics

A snapshot read shall:

1. start from the lineage head
2. inspect transaction state for each version encountered
3. accept the first version visible to the reader’s snapshot rules
4. continue through backversions when the head is uncommitted, too new, rolled back, or deleted from the reader’s point of view
5. fail closed on broken lineage or undecidable transaction state

## Reclaim legality

A version is reclaim-eligible only when all of the following hold:

1. no active or retained snapshot can still require it
2. prepared or limbo resolution does not keep it relevant
3. lineage correctness is preserved without it
4. required oversized-value reachability cleanup is complete or proven unnecessary
5. required index cleanup prerequisites are complete
6. repair, audit, or forensic retention policy does not fence reclaim

## Restart reconciliation

On restart the engine shall:

1. rebuild authoritative transaction state from durable transaction inventory and committed state records
2. treat row versions according to durable transaction state, not by speculative replay ordering
3. preserve committed lineage
4. preserve unresolved or limbo lineage as blocking state where required
5. resume or rewind GC/sweep progress safely from durable progress state
6. refuse any shortcut that would reclaim, publish, or hide a version without durable state proof

## Index cleanup hook rule

1. Index structures are candidate finders, not visibility authorities.
2. Index cleanup may not outrun heap reclaim proof.
3. Family-local cleanup implementations are permitted, but the legality gate is owned by this MGA model.
4. If family-local cleanup cannot prove safe retirement, the version remains unreclaimed.

## Oversized-value and LOB cleanup hook rule

1. Overflow or LOB payload retirement is subordinate to the owning row-version lifecycle.
2. A detached payload may be reclaimed only after row-version reclaim legality and payload reachability proof both succeed.
3. Restart reconciliation shall prefer orphan retention over destructive premature prune.

## Cross-section ownership

- section `10` owns sweep execution and retained-evidence behavior
- section `11` owns overflow/LOB page families and payload layout
- section `18` owns family-local index cleanup mechanics
- section `35` owns checkpoint and forced-write durability ordering
- section `42` owns failure classification and degraded-mode handling

## Audit lookup anchors

- `src/core/transaction_manager.cpp` search
  `TransactionManager::flushTransactionPublicationState` for durable commit and
  rollback publication.
- `include/scratchbird/core/transaction_manager.h` search
  `flushTransactionPublicationState` for the transaction-manager declaration
  seam used by adjacent durability and restart code.

## Implementation boundary

This file is the governing contract even where current code still distributes pieces across heap, sweep, index-family, and durability surfaces.
That distributed current state is implementation substrate, not a reason to weaken the canonical model.
