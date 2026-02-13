# Transaction Core (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the transaction lifecycle, isolation levels, and commit/rollback rules
for ScratchBird MGA transactions.

## Transaction Lifecycle

States:
- `ACTIVE`
- `COMMITTED`
- `ROLLED_BACK`
- `LIMBO` (prepared)

Lifecycle rules:
- BEGIN creates a new transaction with a snapshot.
- COMMIT transitions ACTIVE -> COMMITTED.
- ROLLBACK transitions ACTIVE -> ROLLED_BACK.

## Isolation Levels

- READ COMMITTED (default)
- REPEATABLE READ
- SERIALIZABLE

Isolation semantics are enforced by MGA visibility rules.

## Autocommit

- If autocommit is enabled, each statement executes in its own transaction.
- Errors in autocommit statements roll back the statement transaction only.

## Savepoints

- Savepoints are scoped to the current transaction.
- ROLLBACK TO SAVEPOINT reverts changes after the savepoint.
- RELEASE SAVEPOINT removes the marker.

## Error Rules

- COMMIT/ROLLBACK with no active transaction -> SQLSTATE `25P01`.
- Serialization failure -> SQLSTATE `40001`.

## Related Specs

- `docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md`
- `docs/specifications/parser/v3/transaction/TRANSACTION_LOCK_MANAGER.md`
- `docs/specifications/parser/v3/transaction/07_TRANSACTION_AND_SESSION_CONTROL.md`
