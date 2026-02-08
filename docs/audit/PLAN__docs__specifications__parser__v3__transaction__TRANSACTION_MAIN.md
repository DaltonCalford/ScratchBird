# Implementation Plan: TRANSACTION_MAIN.md

**Spec Path:** `docs/specifications/parser/v3/transaction/TRANSACTION_MAIN.md`

**Category:** transaction

## Scope Summary
- Implement transaction and MGA requirements.

## Dependencies
- `docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md`

## Implementation Steps (Detailed)
- Define transaction state machine and TIP layout
- Define snapshot algorithm per isolation level
- Define commit/rollback step‑by‑step
- Define savepoint representation and rollback
- Define recovery behavior without WAL

## Manual Gap Analysis (Missing/Unclear Details)
- No TIP layout
- No step‑by‑step commit/rollback
- No recovery algorithm

## Verification
- Transaction correctness and recovery tests.
