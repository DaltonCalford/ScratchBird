# Section 09: Lock Manager Core

Status: current_authority

This section is authoritative for the current ScratchBird lock-manager surface.

The canonical lock manager is an in-process engine subsystem owned by Database startup and used by transaction, storage, and observability paths. The current implementation proves PostgreSQL-like lock modes, database and table and page and tuple lock targets, deadlock detection, wait-history capture, metadata lock visibility, and MGA-aware read-consistency restart behavior.

## Current implemented truth

- LockManager lifecycle is owned by Database during database startup.
- The current implemented lock modes are table-oriented and tuple-oriented modes defined by the shipped lock-manager code, not a Firebird-style lock matrix.
- The current implemented lock targets proven in code are DATABASE, TABLE, PAGE, and TUPLE.
- Tuple update locking is integrated with StorageEngine and READ_COMMITTED_READ_CONSISTENCY restart behavior.
- Deadlock detection, wait history capture, timeout handling, no-wait refusal, and metadata lock exposure are implemented.
- Predicate or range locking, metadata-object resource families beyond the current reporting surface, and certified repeatable-read or serializable phantom protection are not implemented authority in this section.

## Capability boundary

| Capability | Status | Current authority | Non-guarantee |
| --- | --- | --- | --- |
| In-process lock manager lifecycle | implemented | Database plus LockManager | none |
| Database and table and page and tuple targets | implemented | LockManager core | none |
| Deadlock detection and victim selection | implemented | LockManager plus deadlock tests | none |
| No-wait conflict handling | implemented | StorageEngine plus lock-manager outcomes | none |
| READ_COMMITTED_READ_CONSISTENCY restart handling | implemented | StorageEngine plus TransactionManager | none |
| Metadata lock visibility and wait history | implemented | virtual catalog reporting surfaces | no broader metadata-object lock family implied |
| Predicate or range locking | unsupported | none | no predicate-lock guarantee exists |
| Serializable phantom protection | unsupported | none | no serializable or repeatable-read phantom guarantee exists |
| Multi-process or shared-memory lock table | unsupported | none | no cross-process lock-table guarantee exists |

## Cross-section boundary rules

Section 09 owns lock acquisition, conflict, timeout, deadlock, no-wait, and metadata lock visibility semantics.

Section 09 does not own transaction publication or restart semantics beyond what section 08 defines.

Section 09 does not own storage tuple visibility beyond what sections 08 and 10 define.

Section 09 does not authorize any WAL-based conflict or recovery truth.

## Primary audit entry points

- include/scratchbird/core/lock_manager.h
- src/core/lock_manager.cpp
- [database.cpp:4934](src/core/database.cpp:4934)
- [storage_engine.cpp:5190](src/core/storage_engine.cpp:5190)
- [transaction_manager.cpp:2960](src/core/transaction_manager.cpp:2960)
- [test_deadlock_detection.cpp](tests/unit/test_deadlock_detection.cpp)
- [test_storage_engine.cpp:1220](tests/unit/test_storage_engine.cpp:1220)
- [test_virtual_catalogs.cpp:430](tests/unit/test_virtual_catalogs.cpp:430)

## Non-guarantees

- This section does not certify predicate or range locking.
- This section does not certify repeatable-read or serializable behavior.
- This section does not certify shared-memory or multi-process lock-table behavior.

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [ISOLATION_LEVEL_AND_PHANTOM_PROTECTION_MATRIX.md](ISOLATION_LEVEL_AND_PHANTOM_PROTECTION_MATRIX.md)
- [LOCK_MANAGER_NORMATIVE_IMPLEMENTATION.md](LOCK_MANAGER_NORMATIVE_IMPLEMENTATION.md)
- [MGA_CONFLICT_AND_LOCKING_POLICY.md](MGA_CONFLICT_AND_LOCKING_POLICY.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
