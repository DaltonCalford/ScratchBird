# Transaction and Session Control (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define transaction control (TCL) and session control commands for ScratchBird.
These rules apply to ScratchBird native and all emulated dialects unless
explicitly overridden by dialect emulation specs.

## Transaction Commands

Supported commands:
- `BEGIN` / `START TRANSACTION`
- `COMMIT`
- `ROLLBACK`
- `SAVEPOINT <name>`
- `ROLLBACK TO SAVEPOINT <name>`
- `RELEASE SAVEPOINT <name>`

Rules:
- `SET TRANSACTION ISOLATION LEVEL` must be the first statement in a transaction.
- Savepoints are scoped to a transaction and are discarded on COMMIT/ROLLBACK.

## Isolation Levels

Supported levels:
- `READ COMMITTED` (default)
- `REPEATABLE READ`
- `SERIALIZABLE`

Isolation semantics are defined in:
- `TRANSACTION_MGA_CORE.md`
- `TRANSACTION_MAIN.md`

## Session Commands

Supported commands:
- `SET <param>`
- `RESET <param>`
- `SHOW <param>`
- `SET LOCAL <param>` (transaction-scoped)

Session parameter rules:
- Parameters are scoped to the session unless `SET LOCAL` is used.
- `SET LOCAL` values revert at transaction end.

## Error Rules

- Invalid isolation level -> SQLSTATE `0A000`.
- Unsupported parameter -> SQLSTATE `22023`.

## Related Specs

- `docs/specifications/parser/v3/TRANSACTION_CONTROL.md`
- `docs/specifications/parser/v3/PSQL_RUNTIME_V3.md`
