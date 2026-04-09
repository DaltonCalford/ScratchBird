# Alpha Durability Modes and Flush Ordering

## Purpose

This file defines the authoritative durability modes, commit publication fences, rollback publication fences, and flush-ordering rules for ScratchBird Alpha. ScratchBird uses MGA transaction handling. ScratchBird does not use WAL, redo-log, undo-log, or LSN-authoritative commit semantics.

ScratchBird is always in a transaction. `COMMIT` ends the current transaction and immediately starts the next transaction. `ROLLBACK` ends the current transaction and immediately starts the next transaction.

## Owning Components

`TransactionManager` owns transaction-state publication and commit-sequence assignment.

`Database` owns checkpoint-state load, durable-open classification, and service-state fencing.

Section `03` owns dirty-page inventory, buffer-pool flush ordering, and writeback execution.

Section `35` owns startup recovery classification and corruption containment.

## Durability Modes

### `STRICT`

`STRICT` is the default production durability mode.

A successful `COMMIT` under `STRICT` shall not be acknowledged until the transaction's durable publication records are persisted through the required sync fence for the current database state.

A successful `ROLLBACK` under `STRICT` shall not be acknowledged until the rollback publication records required for restart-safe visibility are persisted through the required sync fence for the current database state.

### `GROUP_COMMIT`

`GROUP_COMMIT` is a production mode that preserves the same durable outcome as `STRICT` while allowing multiple committers to share one durability fence.

`GROUP_COMMIT` may batch multiple committing transactions into a single flush-and-sync cycle. Every transaction in the batch shall still receive a unique committed publication order. The batch shall not be acknowledged until the shared durability fence completes successfully.

A rollback may be published outside a group batch if no shared fence is required for the specific rollback state transition. If the current service state requires a durability fence for rollback publication, rollback shall wait for that fence before acknowledgement.

### `DEVELOPMENT_UNSAFE`

`DEVELOPMENT_UNSAFE` is a non-production mode.

`DEVELOPMENT_UNSAFE` may relax the sync timing used by `STRICT` and `GROUP_COMMIT`, but it shall not change MGA visibility rules, transaction ordering rules, commit-sequence uniqueness rules, or corruption-classification rules.

When `DEVELOPMENT_UNSAFE` is active, the engine shall expose an explicit unsafe-durability service marker through diagnostics and operational status. This mode shall be treated as non-compliant for production certification, backup certification, and conformance gating.

## Common Commit Publication Rules

The following rules apply in every durability mode.

The engine shall validate that the current transaction is active and terminal publication has not already occurred.

The engine shall finalize the transaction's runtime lineage and provenance data before terminal publication.

The engine shall materialize the durable transaction-map state change before the transaction becomes visible as committed to future transactions.

The engine shall never report commit success before the transaction has crossed the durability fence required by the active durability mode.

After commit acknowledgement, the connection shall immediately continue in the next transaction context.

## Common Rollback Publication Rules

The following rules apply in every durability mode.

The engine shall validate that the current transaction is active and terminal publication has not already occurred.

The engine shall publish the rollback terminal state to the transaction map and lineage model before reporting rollback success.

The engine shall never report rollback success while the service is in a write-admission fence state that declares rollback publication unsafe.

After rollback acknowledgement, the connection shall immediately continue in the next transaction context.

## Normative Commit Algorithm

1. Validate that the connection owns an active transaction context.
2. Freeze the transaction's terminal lineage payload and runtime context payload.
3. Materialize the transaction-map terminal state transition for commit.
4. Allocate or confirm the committed publication order for the transaction.
5. Enqueue any required dirty-page and metadata flush work needed for terminal publication.
6. Execute the durability fence required by the active durability mode.
7. Publish commit success to waiters and observability surfaces.
8. Open the successor transaction context for the connection.

If any step before step `6` fails, the commit shall fail without reporting success.

If step `6` fails, the connection shall not observe commit success. The engine shall classify the incident under the recovery and durability rules owned by section `35`.

## Normative Group Commit Algorithm

1. Validate the active transaction and freeze terminal publication inputs.
2. Assign the transaction to the current group-commit batch.
3. Materialize the transaction's durable terminal state and committed publication order within the batch.
4. Hold acknowledgement until the batch flush and sync fence completes.
5. Publish commit success for all transactions in the completed batch.
6. Open successor transaction contexts for the participating connections.

A transaction shall not be reported as committed because it entered a batch. It is committed only after the batch durability fence completes.

## Flush Ordering Rules

Dirty page writeback ordering belongs to section `03`, but the transaction core requires the following publication ordering.

The durable transaction-map terminal state shall not be reordered after a commit acknowledgement.

The checkpoint state shall never advance past transaction publication that has not crossed the required durability fence.

Metadata publication for committed DDL shall observe the same fence as committed DML.

A page write that would allow future recovery to observe a transaction as committed before the transaction map says it is committed is forbidden.

A page write that would allow future recovery to observe committed schema effects before the committed schema epoch is durable is forbidden.

## Write-Admission Fences

If startup classification, degraded storage state, checkpoint debt, or corruption containment places the database in a fenced state, commit and rollback acknowledgements shall obey that fence.

A write-admission fence may allow read-only continuation while blocking commit and rollback acknowledgement.

A write-admission fence shall surface a stable incident class and operator-visible refusal reason.

## Observability Requirements

The engine shall expose the active durability mode.

The engine shall expose group-commit batching counters when `GROUP_COMMIT` is active.

The engine shall expose explicit unsafe-durability markers when `DEVELOPMENT_UNSAFE` is active.

The engine shall expose write-admission fence incidents that block terminal publication.

## Explicit Non-Goals

ScratchBird Alpha does not define WAL flush ordering.

ScratchBird Alpha does not define redo log replay ordering.

ScratchBird Alpha does not define LSN-driven commit visibility.

ScratchBird Alpha does not define log-shipping or PITR durability semantics in this file.
