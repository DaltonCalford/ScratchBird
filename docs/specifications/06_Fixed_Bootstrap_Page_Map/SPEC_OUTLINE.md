# Spec Outline - 06_Fixed_Bootstrap_Page_Map

## Purpose
Define the fixed bootstrap page contract for the main database file and the fail-closed validation path that proves those pages exist at the canonical locations.

## Scope
This section applies to the main database file bootstrap map only.

It does not define:
- tablespace-local header or FSM pages
- catalog payload contents beyond fixed catalog-root placement
- full TIP page lifecycle beyond fixed root placement

## Canonical fixed bootstrap map
Authoritative constants live in `include/scratchbird/core/ondisk.h`:
- `BOOTSTRAP_PAGE_DATABASE_HEADER = 0`
- `BOOTSTRAP_PAGE_SYSTEM_STATE = 1`
- `BOOTSTRAP_PAGE_CATALOG_ROOT = 2`
- `BOOTSTRAP_PAGE_FSM_ROOT = 3`
- `BOOTSTRAP_PAGE_TX_MAP_ROOT = 4`
- `BOOTSTRAP_PAGE_RESERVED = 5`
- `BOOTSTRAP_FIXED_PAGE_COUNT = 6`

## Authoritative page roles
- page `0`: `DatabaseHeader`
- page `1`: `BootstrapSystemStatePage`
- page `2`: fixed `PAGE_TYPE_CATALOG_ROOT` root page
- page `3`: `PAGE_TYPE_FSM_ROOT` bootstrap FSM page
- page `4`: `TIPPageHeader` bootstrap transaction inventory root
- page `5`: `PAGE_TYPE_BOOTSTRAP_RESERVED`

## Bootstrapping behavior
`Database::create` writes the six bootstrap pages directly.

`Database::open`:
- reads page `0` to determine page size
- validates the header contract
- validates bootstrap page ids, types, and page-size agreement for pages `1..5`
- initializes runtime subsystems only after bootstrap-map validation succeeds

## Runtime ownership
- page `1` is reused by runtime control paths for startup reconciliation, checkpoint markers, writeback incident fencing, and sweep progress
- page `2` is fixed bootstrap placement, but durable payload ownership belongs to catalog code
- page `3` is the main-file FSM root consumed by `PageManager`
- page `4` is the TIP root consumed by transaction code

## Main implementation drift corrected in this audit
- the previous prose overstated static field-level bootstrap contracts for pages `1`, `2`, and `4`
- the previous prose understated the role of page `1` as a multiplexed restart-safe control page
- the previous prose blurred the main database bootstrap map with tablespace-local page `0..1` contracts

## Failure semantics
Open must fail closed when any bootstrap page has:
- invalid magic
- wrong page size
- wrong page id
- wrong page type
- invalid header checksum where required by the current validation path

## Compatibility notes
The fixed map is format-bearing:
- page assignments are part of the on-disk contract
- changes require format-aware handling and must not silently drift

## Test contract
See `TEST_CONTRACT.md`.
