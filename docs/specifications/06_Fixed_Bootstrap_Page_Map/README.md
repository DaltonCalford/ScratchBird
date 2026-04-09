# 06_Fixed_Bootstrap_Page_Map

## Purpose
Define the canonical fixed bootstrap page map for the main database file so startup can validate and open the database without scanning for bootstrap roots.

## Status
Authoritative - code-backed `partial` as of `2026-03-27`.

## Current implementation summary
The fixed bootstrap map for the main database file is real and deterministic in the current code:
- page `0`: database header
- page `1`: system state control page
- page `2`: catalog root page
- page `3`: FSM root page
- page `4`: transaction map root page
- page `5`: reserved bootstrap page

The strongest drift from the previous prose is:
- page `1` is not just "system settings and membership state"; it is a restart-safe control page used by checkpoint, startup reconciliation, writeback incident fencing, and sweep progress persistence
- page `2` is fixed and real, but the durable payload is owned by catalog code rather than a static bootstrap list written directly at database creation time
- page `4` is a real TIP root page with bootstrap transaction entries, not only a pointer page
- tablespace-local page `0` or `1` contracts are separate and must not be conflated with the main database bootstrap map

## Primary code anchors
- `include/scratchbird/core/ondisk.h`
- `include/scratchbird/core/database.h`
- `include/scratchbird/core/transaction_manager.h`
- `src/core/database.cpp`
- `src/core/page_manager.cpp`
- `src/core/catalog_manager.cpp`

## Search-key audit anchors
- `src/core/database.cpp` search `Database::validate_bootstrap_page_map` for
  the open-path bootstrap validator.
- `src/core/database.cpp` search `create_reserved_bootstrap_page` for canonical
  page `5` materialization.
- `include/scratchbird/core/ondisk.h` search `BootstrapSystemStatePage` for the
  bootstrap system-state payload contract.

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [BOOTSTRAP_PAGE_LAYOUTS.md](BOOTSTRAP_PAGE_LAYOUTS.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
