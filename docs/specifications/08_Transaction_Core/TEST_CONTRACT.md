# Test Contract

## Status

- Specification status: current_authority
- Last code-audit date: 2026-03-30

## Purpose

This file defines the required proof surface for section `08`. The listed tests and coverage buckets are the authoritative minimum contract for changes that affect transaction state, MGA visibility, savepoints, lineage, replay binding, or restore validation behavior.

## Current implementation-backed lane-B proof

- `test_transaction_manager.cpp` proves transaction begin publication and
  rollback publication refuse an open write-admission fence without consuming a
  new XID or terminally publishing rollback state
- `test_mga_failpoint_replay.cpp` proves commit pre-TIP and post-TIP crash
  windows normalize to `ABORTED` or remain durably `COMMITTED` as required
- `test_executor_transaction_payload.cpp` proves writeback incidents persist,
  reopen with fenced service posture, and block transaction progress while
  durability posture is unsafe

## Required Coverage Buckets

### Transaction Manager Core

`test_transaction_manager.cpp` shall cover transaction creation, terminal publication, transaction-map state transitions, and restart-safe publication invariants.

`test_transaction_vnext_contract.cpp` shall cover stable externalized transaction behavior that other sections rely on.

### Concurrency and MGA Visibility

`test_concurrent_transactions.cpp` shall cover concurrent transaction interaction, visibility separation, and committed publication order.

`test_mga_failpoint_replay.cpp` shall cover MGA failure-injection, restart-safe handling, and replay-sensitive correctness.

### Group Commit and Durability Modes

`test_group_commit.cpp` shall cover group-commit batching, commit acknowledgement ordering, and mode-specific durability guarantees.

### Savepoints and Subtransactions

`test_subtransactions.cpp` shall cover savepoint creation, nested rollback behavior, and preservation of the one-top-level-transaction model.

### Executor and Session Integration

`test_executor_transaction_payload.cpp` shall cover transaction payload handoff between execution and transaction-core surfaces.

`test_temp_table_semantics.cpp` shall cover transaction-scoped temporary object semantics where transaction visibility matters.

### Forensic Replay and Catalog Runtime Context

`test_forensic_replay_sessions.cpp` shall cover lineage, schema epoch binding, forensic capsule binding, and replay refusal cases.

`test_catalog_runtime_context_extension_contract.cpp` shall cover committed publication of runtime context and catalog-backed transaction metadata.

### Restore Validation and Rehearsal

`test_restore_validation_rehearsal.cpp` shall cover restore validation, rehearsal classification, and transaction-correctness refusal cases.

## Required Behavioral Assertions

The test surface shall prove that ScratchBird is always in a transaction.

The test surface shall prove that successful `COMMIT` immediately transitions the connection into the next transaction.

The test surface shall prove that successful `ROLLBACK` immediately transitions the connection into the next transaction.

The test surface shall prove that `AUTOCOMMIT` performs post-success commit only and does not commit a failed statement.

The test surface shall prove that DDL and DML obey the same transaction publication rules.

The test surface shall prove that the engine does not depend on WAL semantics for transaction correctness.

## Gate Rule

A change that affects section `08` behavior shall not be treated as implementation-ready unless the affected coverage bucket remains green or is replaced by a stronger proof artifact.

Replacing a listed proof artifact requires updating this file first.

## Explicit Non-Goals

This file does not require one monolithic transaction-core gate binary.

This file does not define performance certification thresholds.
