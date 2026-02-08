# Transaction Lock Manager (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the lock manager, lock modes, and lock ordering rules for ScratchBird.
The lock manager follows Firebird-style semantics.

## Lock Types

- Database
- Relation (table)
- Page
- Record
- Metadata (schema/catalog)

## Lock Modes

- `NULL`, `SHARED`, `PROTECTED`, `EXCLUSIVE`

Compatibility and conflict matrix MUST follow Firebird lock semantics.

## Ordering Rules (Mandatory)

Lock acquisition order to prevent deadlocks:
1. Database
2. Relation
3. Page
4. Record
5. Metadata

Locks MUST be acquired in this order. If a lower-order lock is required after a
higher-order lock, the transaction MUST release and reacquire in order.

## Deadlock Handling

- Deadlock detection runs on wait graph cycles.
- The youngest transaction in the cycle MUST be aborted.
- SQLSTATE for deadlock: `40P01`.

## Lock Waits

- `WAIT` and `NO WAIT` behaviors follow Firebird semantics.
- Lock wait timeout is configurable per session.

## Related Specs

- `docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md`
- `docs/specifications/parser/v3/transaction/FIREBIRD_CONSTANTS_REFERENCE.md`
