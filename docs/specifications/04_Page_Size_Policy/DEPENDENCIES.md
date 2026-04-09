# Section 04 Dependencies

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current implementation dependencies
- The supported-size boundary depends on `isValidAlphaPageSize` in `ondisk.h`.
- Header lower/upper/special offset math depends on the page header helper functions in `ondisk.h`.
- Database bootstrap and open-time page-size authority depends on `Database::init_header_page`, `Database::create`, and `Database::open`.
- Tablespace page-size consistency depends on `PageManager::openTablespace`.
- Restore-time page-size and block-size compatibility depends on `backup_manager.cpp`.
- Heap tuple capacity and item-pointer semantics depend on `heap_page.h` and `heap_page.cpp`.

## Downstream dependents
- Section `02`: filespace/tablespace lifecycle depends on one page size across the database and attached tablespaces.
- Section `03`: allocator, FSM, and buffer-pool geometry depend on page size.
- Section `05`: page header layout and page-contract semantics depend directly on page size.
- Section `10`: reclaim and GC page-space calculations depend on page size.
- Section `18` and other index families: page-local structure sizing depends on `db_->page_size()`.

## Implementation code map
- `ScratchBird/include/scratchbird/core/ondisk.h:1003`
- `ScratchBird/include/scratchbird/core/ondisk.h:1020`
- `ScratchBird/include/scratchbird/core/ondisk.h:1355`
- `ScratchBird/src/core/database.cpp:4281`
- `ScratchBird/src/core/database.cpp:4403`
- `ScratchBird/src/core/database.cpp:4666`
- `ScratchBird/src/core/page_manager.cpp:1665`
- `ScratchBird/src/core/backup_manager.cpp:562`
- `ScratchBird/src/core/backup_manager.cpp:605`
- `ScratchBird/include/scratchbird/core/heap_page.h:37`
- `ScratchBird/src/core/heap_page.cpp:381`

## Drift and contradictions
- The older dependency file understated the code-level dependence on restore validation and tablespace-open checks.
- It also implied a stronger config-subsystem dependence than this pass proved. The reviewed engine authority is API/bootstrap/open driven more than config-driven.

## Non-blocking expansion candidates
- A machine-readable page-size compatibility matrix across subsystems
- A canonical mapping from page-size policy to each storage/index family’s tested envelope

## Suggestions
- Keep this file focused on code-owned dependencies, not inherited recommendation prose.
