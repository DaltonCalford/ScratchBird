# Schema DDL State Machine

## Purpose

This file restates the schema-facing `DDL` lifecycle for consumers of section `37`. It does not weaken section `24`; it explains the same transaction-scoped publication model from the schema-management point of view.

## Governing rules

- ScratchBird is always inside a transaction.
- `COMMIT` and `ROLLBACK` each end the current transaction and immediately start the next one.
- `START TRANSACTION` changes transaction defaults; it is not an entry point into transactional mode.
- `DDL` and `DML` obey the same transaction, savepoint, lock, visibility, MGA, and recovery rules.
- Only committed schema epochs are globally visible.

## Consumer-facing state machine

### State `S0_COMMITTED_BASELINE`

A transaction begins with a committed schema epoch baseline. All other transactions see only their own committed baselines.

### State `S1_LOCAL_SCHEMA_MUTATION`

A schema-changing statement runs inside the active transaction. Object-definition rows, dependency rows, and other catalog state are mutated transactionally. The owning transaction may continue binding against that local overlay.

### State `S2_PENDING_PUBLICATION`

The transaction has staged schema-changing work that is not yet committed. No other transaction may observe it as committed metadata.

### State `S3_COMMIT_PUBLICATION`

On successful commit:

1. staged transactional `DDL` batches are flushed
2. one new committed schema epoch is appended if schema mutation occurred
3. caches and metadata consumers must treat that new epoch as the authoritative publication boundary
4. the next transaction starts from the newly committed baseline

### State `S4_ROLLBACK_RETIREMENT`

On rollback:

1. staged transactional `DDL` batches are discarded
2. the transaction-start schema epoch is restored
3. no committed schema epoch is appended for the abandoned work
4. the next transaction starts from the rolled-back committed baseline

## Autocommit rule

In autocommit mode, a successful schema-changing statement commits and therefore publishes its schema epoch before the next statement begins. If the statement errors, no commit occurs, the transaction remains active, and the client may fix the issue or roll back.

## Savepoint rule

Schema-changing work belongs to the same savepoint model as other transactional work. If a given `DDL` primitive cannot be safely rewound to an interior savepoint, the engine must refuse it or fail closed rather than silently allowing non-transactional schema escape.

## Refusal rules

The schema subsystem must reject any implementation path that would:

- publish `DDL` without commit
- expose uncommitted schema changes to other transactions as committed metadata
- advance schema publication state after a failed statement
- treat `DDL` as outside the MGA visibility model
