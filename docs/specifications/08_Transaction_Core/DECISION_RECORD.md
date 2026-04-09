# Decision Record - 08_Transaction_Core

## Status
Authoritative current decision record.

## Decided and fixed
- ScratchBird recovery is `MGA` state reconciliation, not `WAL` replay.
- ScratchBird is always in a transaction.
- `COMMIT` ends the current transaction and immediately starts the next one.
- `ROLLBACK` ends the current transaction and immediately starts the next one.
- `START TRANSACTION` changes transaction defaults; it is not a transition from non-transactional to transactional mode.
- `PREPARED` is a real durable limbo state.
- Savepoints are transaction-local backout frames, not durable subtransaction identities.
- Section `08` owns the canonical restart and visibility vocabulary even when execution is split across `Database` and `TransactionManager`.

## Explicit exclusions
- durable subtransaction identity is not part of the current contract
- `WAL`, redo-log, undo-log, and `LSN` authority are not part of the current contract

## Implementation guidance
- If a client or IPC front door cannot honor the savepoint model, it must reject the unsupported entry point rather than weaken core semantics.
- Cross-section backup, tooling, and runbook layers may reference section `08`, but they must not redefine transaction truth.
