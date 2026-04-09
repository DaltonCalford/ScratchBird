# Dependencies

Status: current_authority

Section 09 depends on concrete runtime subsystems rather than the broader abstract dependency graph described by older prose. The current lock-manager core is coupled to Database, TransactionManager, StorageEngine, observability surfaces, and virtual metadata-lock reporting.

## Upstream dependencies

| Dependency | Current relationship | Evidence |
| --- | --- | --- |
| Section 08 transaction core | restart reasons, statement-snapshot behavior, and isolation handling shape lock-conflict outcomes | [transaction_manager.cpp:2960](src/core/transaction_manager.cpp:2960) |
| Database runtime | owns lock-manager lifecycle and initialization | [database.cpp:4934](src/core/database.cpp:4934) |
| StorageEngine | uses tuple locks in update paths and translates conflict outcomes | [storage_engine.cpp:5190](src/core/storage_engine.cpp:5190) |
| ProcArray and transaction state | participates in deadlock and concurrency behavior through runtime transaction state | [test_deadlock_detection.cpp](tests/unit/test_deadlock_detection.cpp) |

## Downstream dependencies

| Consumer | Current relationship | Evidence |
| --- | --- | --- |
| Metadata lock reporting | virtual catalog and performance-schema surfaces expose current lock state and waits | [test_virtual_catalogs.cpp:430](tests/unit/test_virtual_catalogs.cpp:430) |
| Storage write and update paths | tuple conflicts determine retry, timeout, deadlock, or no-wait outcomes | [test_storage_engine.cpp:1220](tests/unit/test_storage_engine.cpp:1220) |
| Deadlock observability | tests and observability surfaces consume deadlock stats and wait state | [test_deadlock_detection.cpp](tests/unit/test_deadlock_detection.cpp) |

## Non-dependencies

- Predicate and range locking are not upstream dependencies because that capability is not implemented authority.
- Metadata lock visibility must not be misread as proof of a broader metadata-object lock family.
- Serializable phantom prevention is not an upstream dependency because that capability is not implemented authority.

## Proof obligations

- Prove lifecycle ownership from Database.
- Prove restart and isolation coupling from TransactionManager.
- Prove tuple-lock use from StorageEngine.
- Prove metadata-lock visibility from tests or virtual-catalog surfaces.

## Non-guarantees

- No dependency is claimed here for unimplemented predicate or range locking.
- No dependency is claimed here for certified serializable phantom prevention.
