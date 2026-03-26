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
| Forced writes and ordered durable publication | Commit durability is fenced through the engine-owned sync path for the primary database file plus registered durable filespaces; TIP publication is routed through the same attributed writeback path instead of a raw side write. | `include/scratchbird/core/database.h`, `src/core/database.cpp`, `src/core/transaction_manager.cpp`, `tests/unit/test_executor_transaction_payload.cpp` |
| Active transaction at crash | Startup reconciliation makes incomplete ACTIVE work terminal using TIP/CLOG truth; prepared work is handled separately. | `include/scratchbird/core/transaction_manager.h`, `src/core/database.cpp`, `tests/unit/test_executor_transaction_payload.cpp` |
| Interrupted statement scope | Implicit statement savepoints bound statement-local work so restart/rollback does not require redo history. | `src/core/connection_context.cpp`, `tests/unit/test_storage_engine.cpp`, `tests/unit/test_transaction_vnext_contract.cpp` |
| Interrupted head/back-version update | MGA version chains preserve prior truth; restart/visibility walks backversions and repair logic classifies incomplete state. | `src/core/storage_engine.cpp`, `tests/unit/test_mga_failpoint_replay.cpp`, `tests/conformance/transactions/test_transaction_truth_native.cpp` |
| Torn or corrupt page write | Canonical page/header checksums and degraded-mode fencing detect corruption, classify it, and prevent unsafe writes. | `include/scratchbird/core/ondisk.h`, `tests/unit/test_mga_failpoint_replay.cpp`, `tests/unit/test_copy_executor.cpp` |
| Ambiguous clean shutdown / checkpoint | Checkpoint lifecycle markers and generation tracking distinguish legal fast-path startup from restart repair. | `src/core/database.cpp`, `tests/unit/test_executor_transaction_payload.cpp` |
| Long-running transaction horizon pinning | Long-running transaction policy classifies age, GC-horizon pin, and snapshot-horizon pin pressure separately, then publishes notice, rollback, or termination directives that are consumed at attachment boundaries. | `include/scratchbird/core/long_transaction_monitor.h`, `src/core/long_transaction_monitor.cpp`, `src/core/connection_context.cpp`, `tests/unit/test_long_transaction_governance_contract.cpp` |
| Old hanging detached work after restart | Dormant work uses explicit engine-owned tokens, policy-driven cleanup and reattach preferences, SQL-visible monitoring, and replacement attachment/transaction reopen semantics after restart instead of blind reconnect replay. | `include/scratchbird/core/database.h`, `src/core/database.cpp`, `src/core/observability_contract.cpp`, `tests/unit/test_auth_bootstrap_claim.cpp`, `tests/unit/test_protocol_adapter_dialects.cpp` |
| Prepared transaction residue | Prepared state is tracked distinctly and reconciled against startup markers/catalog state before admission of new work. | `include/scratchbird/core/transaction_manager.h`, `src/core/database.cpp`, `tests/unit/test_executor_transaction_payload.cpp` |
| Sweep / garbage-collection debt | Sweep advances OIT and publishes cleanup debt explicitly instead of hiding it in redo history. | `include/scratchbird/core/sweep_manager.h`, `include/scratchbird/core/gc_publication.h`, `tests/unit/test_garbage_collector.cpp` |
| Index cleanup lag after reclaim | Cleanup publication records expose exact/summary/approximate debt so unfinished cleanup is observable and resumable by policy. | `include/scratchbird/core/gc_publication.h`, `tests/unit/test_garbage_collector.cpp` |
| Physical shadow copy and promotion | Optional shadow filespaces backfill page-for-page copies, mirror subsequent writes, and can be promoted as a new live route for a filespace without changing MGA recovery authority. | `include/scratchbird/core/database.h`, `src/core/database.cpp`, `tests/unit/test_shadow_filespaces.cpp` |
| Recovery observability gaps | Checkpoint/recovery/writeback metrics are published as first-class observability contract surfaces. | `src/core/observability_contract.cpp`, `tests/unit/test_observability_sql_views.cpp` |
| Crash-window regression risk | Required public-beta gate carries failpoint, degraded-mode, sweep, and recovery regression coverage. | `tests/conformance/public_beta/run_required_public_beta_gate.sh` |
| Replication / audit lineage export | Optional post-commit write-after lineage is emitted only after durability truth exists, can target file, remote database, and Kafka-style sinks, and never becomes recovery truth. | `include/scratchbird/core/sweep_manager.h`, `src/core/sweep_manager.cpp`, `tests/unit/test_garbage_collector.cpp` |

## Auditor Walkthrough

Use this order when reviewing the code:

1. `include/scratchbird/core/database.h`
2. `include/scratchbird/core/transaction_manager.h`
3. `include/scratchbird/core/sweep_manager.h`
4. `include/scratchbird/core/gc_publication.h`
5. `src/core/database.cpp`
6. `src/core/transaction_manager.cpp`
7. `src/core/sweep_manager.cpp`
8. `tests/unit/test_executor_transaction_payload.cpp`
9. `tests/unit/test_long_transaction_governance_contract.cpp`
10. `tests/unit/test_auth_bootstrap_claim.cpp`
11. `tests/unit/test_shadow_filespaces.cpp`
12. `tests/unit/test_garbage_collector.cpp`
13. `tests/conformance/public_beta/run_required_public_beta_gate.sh`

## Boundaries

This document claims the Alpha engine addresses the failure classes above through MGA state recovery, forced-write fences, ordered publication, restart reconciliation, checksum-driven detection, long-running transaction governance, optional derivative shadowing, and optional derivative write-after export. It does not claim that ScratchBird uses WAL replay, nor that redo history is part of Alpha crash recovery.
