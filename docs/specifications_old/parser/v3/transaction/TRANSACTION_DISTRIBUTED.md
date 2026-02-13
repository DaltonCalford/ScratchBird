# Distributed Transactions (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define distributed transaction coordination rules. Distributed transactions are
optional; if disabled, any distributed transaction request MUST be rejected.

## Scope

In scope:
- Two-phase commit (2PC) orchestration
- XA-style prepare/commit/rollback

Out of scope:
- Cluster-wide consensus protocols
- Global WAL or log shipping (forbidden in V3)

## Core Rules

- Coordinator is responsible for prepare/commit/rollback ordering.
- Prepared transactions MUST be durable in catalog storage.
- If a prepare succeeds, all participants MUST be able to commit or roll back
  even after restart.
- If distributed transactions are disabled, any `PREPARE TRANSACTION` MUST fail
  with SQLSTATE `0A000`.

## Two-Phase Commit (2PC)

### Phase 1: Prepare
1. Coordinator sends PREPARE to all participants.
2. Each participant writes a prepared record to catalog and replies OK/FAIL.
3. If any participant fails, coordinator issues ROLLBACK to all that prepared.

### Phase 2: Commit
1. Coordinator sends COMMIT to all prepared participants.
2. Each participant commits and removes prepared record.

### Heuristics

- Heuristic outcomes are not supported in V3.
- Any heuristic request MUST be rejected with SQLSTATE `0A000`.

## Recovery

- On startup, the engine scans prepared transaction catalog entries.
- Any prepared transaction without a final decision MUST remain in limbo
  until explicitly committed or rolled back by administrator action.

## Related Specs

- `docs/specifications/parser/v3/transaction/TRANSACTION_MAIN.md`
- `docs/specifications/parser/v3/transaction/TRANSACTION_LOCK_MANAGER.md`
