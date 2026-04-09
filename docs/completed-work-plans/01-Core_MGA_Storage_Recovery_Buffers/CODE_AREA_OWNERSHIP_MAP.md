# Code Area Ownership Map

## Primary Write Scopes

| Ticket | Primary write scope | Conflict surfaces | Parallelization rule |
| --- | --- | --- | --- |
| B1-01-001 | assigned section specs plus this package | all package control files | serial only |
| B1-01-002 | include/scratchbird/core/ondisk.h, include/scratchbird/core/page_manager.h, src/core/page_manager.cpp, include/scratchbird/core/buffer_pool.h, src/core/buffer_pool.cpp, include/scratchbird/core/database.h, src/core/database.cpp, include/scratchbird/core/transaction_manager.h, src/core/transaction_manager.cpp, include/scratchbird/core/lock_manager.h, src/core/lock_manager.cpp, include/scratchbird/core/garbage_collector.h, src/core/garbage_collector.cpp, include/scratchbird/core/heap_page.h, src/core/heap_page.cpp, include/scratchbird/core/sweep_manager.h, src/core/sweep_manager.cpp, include/scratchbird/core/toast.h, src/core/toast_visibility.h, src/core/toast.cpp, src/core/toast_visibility.cpp, include/scratchbird/core/storage_engine.h, src/core/storage_engine.cpp, include/scratchbird/core/tid_resolver.h, src/core/tid_resolver.cpp, include/scratchbird/core/catalog_manager.h, src/core/catalog_manager.cpp, src/sblr/executor.cpp | same core storage, transaction, recovery, GC, TOAST, bootstrap, and catalog files | serial with B1-01-003 and B1-01-004 |
| B1-01-003 | include/scratchbird/core/ondisk.h, include/scratchbird/core/page_manager.h, src/core/page_manager.cpp, include/scratchbird/core/buffer_pool.h, src/core/buffer_pool.cpp, include/scratchbird/core/tid_resolver.h, src/core/tid_resolver.cpp, include/scratchbird/core/catalog_manager.h, src/core/catalog_manager.cpp, src/sblr/executor.cpp, src/core/database.cpp | page layout, filespace, allocator, bootstrap, and buffer surfaces | after ownership freeze |
| B1-01-004 | include/scratchbird/core/database.h, src/core/database.cpp, include/scratchbird/core/transaction_manager.h, src/core/transaction_manager.cpp, include/scratchbird/core/lock_manager.h, src/core/lock_manager.cpp, include/scratchbird/core/garbage_collector.h, src/core/garbage_collector.cpp, include/scratchbird/core/heap_page.h, src/core/heap_page.cpp, include/scratchbird/core/sweep_manager.h, src/core/sweep_manager.cpp, include/scratchbird/core/toast.h, src/core/toast.cpp, include/scratchbird/core/toast_visibility.h, src/core/toast_visibility.cpp, include/scratchbird/core/storage_engine.h, src/core/storage_engine.cpp, include/scratchbird/core/catalog_manager.h, src/core/catalog_manager.cpp | same core runtime files plus durability, lock, GC, TOAST, and time-boundary seams | after lane A foundation |
| B1-01-005 | storage, recovery, and public-beta gates | shared gate runners | after implementation tickets |

## Section Ownership Freeze

| Assigned section | Primary runtime owner(s) | Normalized audit anchors | Next consuming ticket |
| --- | --- | --- | --- |
| `02` | `TIDResolver`, `CatalogManager`, `PageManager` | `TIDResolver::resolveTablespace`, `CatalogManager::resolveTablespaceBindings`, `PageManager::allocatePageInTablespace` | B1-01-003 |
| `03` | `PageManager`, `BufferPool`, `Database`, `GarbageCollector` | `PageManager::allocatePageInTablespace`, `BufferPool::publishDirtyGeneration`, `AUDIT CONTRACT: when write_admission_fenced() is true`, `GarbageCollector::cleanPage` | B1-01-003, B1-01-004 |
| `04` | `Database`, `PageManager` | `Invalid page size in database header`, `Cannot open tablespace with different page size` | B1-01-005 |
| `05` | `ondisk.h` page-header and page-type authority | `PAGE_TYPE_DATABASE_HEADER`, `PageHeader must publish the canonical section-05 field set` | B1-01-003 |
| `06` | `Database`, `ondisk.h` bootstrap layouts | `Database::validate_bootstrap_page_map`, `BootstrapSystemStatePage` | B1-01-003 |
| `08` | `TransactionManager` | `TransactionManager::flushTransactionPublicationState` | B1-01-004 |
| `09` | `LockManager`, `StorageEngine` | `conflict_matrix_`, `StorageEngine::acquireTupleLock` | B1-01-004 |
| `10` | `HeapPage`, `GarbageCollector`, `SweepManager` | `HeapPage::scanVersionMaturity`, `GarbageCollector::cleanPage`, `SweepManager::persistSweepProgressState` | B1-01-004 |
| `11` | `ToastManager`, `ToastVisibility`, `StorageEngine` | `ToastManager::toastValue`, `ToastVisibility::evaluateChunkLifecycle`, `StorageEngine::getOrCreateToastManager` | B1-01-004 |
| `35` | `Database`, `TransactionManager` | `storeCheckpointControlState`, `AUDIT CONTRACT: when write_admission_fenced() is true` | B1-01-004 |
| `40` | `CatalogManager` | `struct ClockSourceRecord`, `CatalogManager::upsertClockSourceCatalogEntry` | B1-01-002 |
| `42` | `Database` | `storeWritebackIncidentControlState`, `AUDIT CONTRACT: when write_admission_fenced() is true` | B1-01-004 |

## Unsafe Parallel Boundaries

- any ticket that updates SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv
- any ticket that changes the same canonical spec file as another ticket
- any ticket that changes the same gate or benchmark artifact family
