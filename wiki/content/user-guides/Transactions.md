# Transactions

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

ScratchBird uses a Firebird-style Multi-Generational Architecture (MGA/MVCC). Each
row can have multiple versions, and visibility is decided by transaction IDs
tracked in Transaction Inventory Pages (TIP). This keeps readers and writers from
blocking each other while preserving isolation.

## Transaction control

| Command | Purpose | Notes |
| --- | --- | --- |
| BEGIN / START TRANSACTION | Start a transaction block | START TRANSACTION is an alias |
| COMMIT / END | Make changes durable | END is an alias |
| ROLLBACK / ABORT | Discard changes | ABORT is an alias |

Example:

```
BEGIN;
UPDATE accounts SET balance = balance - 100 WHERE account_id = 'savings-123';
UPDATE accounts SET balance = balance + 100 WHERE account_id = 'checking-456';
COMMIT;
```

## Savepoints

Savepoints allow partial rollback inside a transaction.

| Command | Purpose |
| --- | --- |
| SAVEPOINT name | Create a savepoint |
| ROLLBACK TO SAVEPOINT name | Roll back to a savepoint |
| RELEASE SAVEPOINT name | Remove a savepoint |

Example:

```
BEGIN;
INSERT INTO orders (customer_id, order_date) VALUES (101, CURRENT_DATE);
SAVEPOINT order_created;
INSERT INTO order_lines (order_id, product_id, quantity)
VALUES (currval('order_id_seq'), 9999, 1); -- error
ROLLBACK TO SAVEPOINT order_created;
COMMIT;
```

## Isolation levels

ScratchBird supports standard SQL isolation levels:

- READ COMMITTED (default): each statement sees committed data at statement start.
- REPEATABLE READ: all statements in the transaction see the same snapshot.
- SERIALIZABLE: adds predicate protection to ensure serializable behavior.

Set per-transaction:

```
BEGIN;
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
-- work
COMMIT;
```

Set session default:

```
SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL REPEATABLE READ;
```

## Locking and concurrency

ScratchBird provides explicit locks for cases where MVCC alone is not enough.

- LOCK TABLE ... IN ACCESS EXCLUSIVE MODE for coarse-grained locks.
- SELECT ... FOR UPDATE / FOR SHARE for row-level locking.
- SKIP LOCKED and NOWAIT are supported in SELECT locking clauses.

Example:

```
BEGIN;
SELECT * FROM job_queue
WHERE status = 'PENDING'
ORDER BY created_at
LIMIT 1
FOR UPDATE SKIP LOCKED;
COMMIT;
```

## MGA visibility basics

- Each tuple version has xmin (creator) and xmax (deleter) transaction IDs.
- TIP pages store the state of each transaction (active, committed, aborted).
- Visibility is decided by TIP state and MGA rules, not PostgreSQL-style snapshots.
- Old versions are removed by cooperative garbage collection when safe.

## Distributed transactions (planned)

ScratchBird plans to support two-phase commit (2PC) with PREPARE TRANSACTION,
COMMIT PREPARED, and ROLLBACK PREPARED for distributed workloads.

## Current Alpha implementation status

Per the transaction specifications, the Alpha codebase currently provides:

- Basic 32-bit XID tracking only.
- No MGA/MVCC implementation.
- No distributed transactions.
- No lock manager (uses std::mutex for page-level locking).
- No savepoints.
- Single-threaded operation.

These are the primary gaps to close before full MGA parity.

## References

- `docs/specifications/transaction/TRANSACTION_MAIN.md`
- `docs/specifications/transaction/TRANSACTION_MGA_CORE.md`
- `docs/specifications/transaction/TRANSACTION_LOCK_MANAGER.md`
- `docs/specifications/transaction/TRANSACTION_DISTRIBUTED.md`
- `docs/specifications/transaction/07_TRANSACTION_AND_SESSION_CONTROL.md`
- `docs/specifications/MGA_RULES.md`
