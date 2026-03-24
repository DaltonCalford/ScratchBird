# ScratchBird MGA Recovery And Firebird Failure Coverage

This note is the auditor entry point for ScratchBird Alpha durability and crash recovery.

## Recovery Authority

ScratchBird Alpha uses Firebird-style MGA state recovery, not ARIES/WAL redo recovery.

Authoritative recovery truth comes from:

- on-page data plus header/payload checksums
- TIP/CLOG terminal transaction state
- checkpoint and startup-reconciliation markers
- version-chain visibility rules

Primary source anchors:

- `include/scratchbird/core/database.h`
- `include/scratchbird/core/transaction_manager.h`
- `src/core/database.cpp`
- `tests/conformance/transactions/test_transaction_truth_native.cpp`

## WAL Position

ScratchBird Alpha does not use a write-ahead log as recovery truth.

- `DatabaseHeader::reserved_wal_level_compat` is a reserved compatibility slot, not live WAL machinery
- `BackupConfig::include_write_after_journal_extensions` refers to future derivative post-commit journal/export capture, not write-ahead redo
- sweep `wal_after` artifacts are write-after lineage exports emitted only after local MGA truth is durable

Primary source anchors:

- `include/scratchbird/core/database.h`
- `include/scratchbird/core/backup_manager.h`
- `include/scratchbird/core/sweep_manager.h`
- `src/core/sweep_manager.cpp`
- `include/scratchbird/core/gc_publication.h`

## Firebird-Class Failure Coverage

The matrix below maps the recurring MGA durability/recovery failure classes that historically matter in Firebird-family engines to ScratchBird source and verification surfaces.

| Failure class | ScratchBird handling | Primary source/test anchors |
| --- | --- | --- |
| Active transaction at crash | Startup reconciliation makes incomplete ACTIVE work terminal using TIP/CLOG truth; prepared work is handled separately. | `include/scratchbird/core/transaction_manager.h`, `src/core/database.cpp`, `tests/unit/test_executor_transaction_payload.cpp` |
| Interrupted statement scope | Implicit statement savepoints bound statement-local work so restart/rollback does not require redo history. | `src/core/connection_context.cpp`, `tests/unit/test_storage_engine.cpp`, `tests/unit/test_transaction_vnext_contract.cpp` |
| Interrupted head/back-version update | MGA version chains preserve prior truth; restart/visibility walks backversions and repair logic classifies incomplete state. | `src/core/storage_engine.cpp`, `tests/unit/test_mga_failpoint_replay.cpp`, `tests/conformance/transactions/test_transaction_truth_native.cpp` |
| Torn or corrupt page write | Canonical page/header checksums and degraded-mode fencing detect corruption, classify it, and prevent unsafe writes. | `include/scratchbird/core/ondisk.h`, `tests/unit/test_mga_failpoint_replay.cpp`, `tests/unit/test_copy_executor.cpp` |
| Ambiguous clean shutdown / checkpoint | Checkpoint lifecycle markers and generation tracking distinguish legal fast-path startup from restart repair. | `src/core/database.cpp`, `tests/unit/test_executor_transaction_payload.cpp` |
| Prepared transaction residue | Prepared state is tracked distinctly and reconciled against startup markers/catalog state before admission of new work. | `include/scratchbird/core/transaction_manager.h`, `src/core/database.cpp`, `tests/unit/test_executor_transaction_payload.cpp` |
| Sweep / garbage-collection debt | Sweep advances OIT and publishes cleanup debt explicitly instead of hiding it in redo history. | `include/scratchbird/core/sweep_manager.h`, `include/scratchbird/core/gc_publication.h`, `tests/unit/test_garbage_collector.cpp` |
| Index cleanup lag after reclaim | Cleanup publication records expose exact/summary/approximate debt so unfinished cleanup is observable and resumable by policy. | `include/scratchbird/core/gc_publication.h`, `tests/unit/test_garbage_collector.cpp` |
| Recovery observability gaps | Checkpoint/recovery/writeback metrics are published as first-class observability contract surfaces. | `src/core/observability_contract.cpp`, `tests/unit/test_observability_sql_views.cpp` |
| Crash-window regression risk | Required public-beta gate carries failpoint, degraded-mode, sweep, and recovery regression coverage. | `tests/conformance/public_beta/run_required_public_beta_gate.sh` |
| Replication / audit lineage export | Post-commit write-after lineage is emitted only after durability truth exists; it never becomes recovery truth. | `include/scratchbird/core/sweep_manager.h`, `src/core/sweep_manager.cpp` |

## Auditor Walkthrough

Use this order when reviewing the code:

1. `include/scratchbird/core/database.h`
2. `include/scratchbird/core/transaction_manager.h`
3. `include/scratchbird/core/sweep_manager.h`
4. `include/scratchbird/core/gc_publication.h`
5. `src/core/database.cpp`
6. `src/core/sweep_manager.cpp`
7. `tests/unit/test_mga_failpoint_replay.cpp`
8. `tests/unit/test_executor_transaction_payload.cpp`
9. `tests/conformance/public_beta/run_required_public_beta_gate.sh`

## Boundaries

This document claims the Alpha engine addresses the failure classes above through MGA state recovery, restart reconciliation, checksum-driven detection, degraded-mode fencing, and derivative write-after export. It does not claim that ScratchBird uses WAL replay, nor that redo history is part of Alpha crash recovery.
