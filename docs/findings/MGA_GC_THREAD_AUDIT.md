# MGA Garbage Collection Thread Audit

## Scope
- Transaction GC thread and configuration in `ScratchBird/docs/specifications/transaction/TRANSACTION_MAIN.md`.
- MGA garbage collection (vacuum) process in `ScratchBird/docs/specifications/transaction/TRANSACTION_MGA_CORE.md`.
- MGA sweep semantics in `ScratchBird/MGA_RULES.md`.

## Implemented (code-truth)
- GarbageCollector subsystem supports cooperative/background policies, page-level pruning, index cleanup, dirty page tracking, adaptive tuning, and metrics.
  - Code: `ScratchBird/src/core/garbage_collector.cpp:27-419`
- Cooperative GC hook on page reads.
  - Code: `ScratchBird/src/core/storage_engine.cpp:810-814`
- Dirty page tracking from DML operations for background GC.
  - Code: `ScratchBird/src/core/storage_engine.cpp:1000-1004`
- Sweep notifies GC when OIT advances.
  - Code: `ScratchBird/src/core/sweep_manager.cpp:157-162`
- Manual vacuum and tuple freezing utilities exist (table/database vacuum and freeze).
  - Code: `ScratchBird/src/core/vacuum.cpp:40-107`, `ScratchBird/src/core/vacuum.cpp:597-703`
- GC metrics exposed via `MON_GARBAGE_COLLECTION`.
  - Code: `ScratchBird/src/sblr/executor.cpp:22917-22996`

## Missing or Partial vs Spec

### F-GC-001 Background GC thread not started from recovery/transaction manager
- Spec: `ScratchBird/docs/specifications/transaction/TRANSACTION_MAIN.md:136-159` (tm_gc_thread, config),
  `ScratchBird/docs/specifications/transaction/TRANSACTION_MAIN.md:627-629` (start_garbage_collector).
- Code: TransactionManager has no GC thread/config fields.
  `ScratchBird/include/scratchbird/core/transaction_manager.h:101-399`
- Code: Database initializes GarbageCollector but never starts background GC.
  `ScratchBird/src/core/database.cpp:1074-1086`
- Code: `startBackgroundGC` exists but is only invoked in unit tests.
  `ScratchBird/src/core/garbage_collector.cpp:110-135`,
  `ScratchBird/tests/unit/test_garbage_collector.cpp:249-255`
- Status: Missing

### F-GC-002 GC configuration not wired to TransactionConfig
- Spec: `ScratchBird/docs/specifications/transaction/TRANSACTION_MAIN.md:157-159`
  (`tc_gc_interval_ms`, `tc_gc_freeze_min_age`).
- Code: GC reads from config section `garbage_collection` (policy, background_interval_ms,
  cooperative_rate, adaptive_tuning), not from TransactionConfig.
  `ScratchBird/src/core/garbage_collector.cpp:552-619`
- Status: Partial

### F-GC-003 MGA vacuum phases not implemented
- Spec: Full vacuum phases include index vacuum, truncate empty pages, FSM/VM updates, statistics
  updates, and frozen XID update.
  `ScratchBird/docs/specifications/transaction/TRANSACTION_MGA_CORE.md:647-683`
- Code: `Vacuum::vacuumTable` scans heap, removes dead tuples, and prunes version chains only.
  No index vacuum, page truncation, FSM/VM updates, relation stats, or frozen XID update.
  `ScratchBird/src/core/vacuum.cpp:40-107`
- Status: Missing

### F-GC-004 GC horizon and freezing are not integrated into GC passes
- Spec: GC uses `gc_oldest_xmin` and freeze ages in GCContext.
  `ScratchBird/docs/specifications/transaction/TRANSACTION_MGA_CORE.md:628-655`
- Code: GarbageCollector uses OIT from TransactionManager and does not consult ProcArray horizon
  or freeze ages during GC passes.
  `ScratchBird/src/core/garbage_collector.cpp:342-399`
- Code: Freezing exists only as a manual vacuum operation.
  `ScratchBird/src/core/vacuum.cpp:597-703`
- Status: Partial

### F-GC-005 Sweep space reclamation stubbed
- Spec: MGA sweep removes back versions (not primary records).
  `ScratchBird/MGA_RULES.md:389-415`
- Code: `SweepManager::reclaimSpace` is a placeholder and logs not implemented.
  `ScratchBird/src/core/sweep_manager.cpp:215-228`
- Status: Missing

### F-GC-006 Transaction stats missing GC metrics
- Spec: TransactionStats includes `ts_gc_cycles` and `ts_tuples_vacuumed`.
  `ScratchBird/docs/specifications/transaction/TRANSACTION_MAIN.md:566-570`
- Code: TransactionManager::Stats does not include GC metrics; GC stats exist separately in
  `MON_GARBAGE_COLLECTION`.
  `ScratchBird/include/scratchbird/core/transaction_manager.h:267-279`,
  `ScratchBird/src/sblr/executor.cpp:22917-22996`
- Status: Partial

## Notes
- Current GC is OIT-based and page-local (MGA-style), while the MGA core spec describes a
  relation-wide vacuum with Postgres-like phases. This is a spec alignment gap that should be
  resolved before wiring a background GC thread to a production lifecycle.
