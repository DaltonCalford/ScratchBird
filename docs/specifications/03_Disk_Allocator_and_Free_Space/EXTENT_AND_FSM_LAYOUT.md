# Extent and FSM Layout

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The current code proves a bootstrap FSM root page, tablespace-local FSM structures, and tablespace metrics. It does not prove the full extent-map and multi-class FSM hierarchy described by older prose.

## Current implementation
- The primary FSM is bootstrapped on page `3` as a bitmap-backed root page.
- `PageManager` exposes a `TablespaceFSM` structure and tablespace metrics surface.
- Tablespace open and header decode logic validate versioned tablespace state before allocation or growth continues.
- `reconstructFromPages` exists as a recovery or rebuild surface for tablespace-local page manager state.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/page_manager.h` | Primary FSM bootstrap comment | `42` | Declares the primary bitmap-backed FSM root on bootstrap page `3` |
| ScratchBird | `include/scratchbird/core/page_manager.h` | `reconstructFromPages` | `297` | Rebuild surface for page-manager state |
| ScratchBird | `include/scratchbird/core/page_manager.h` | `TablespaceMetrics`, `getTablespaceMetrics` | `309`, `328` | Metrics surface for tablespace free-space state |
| ScratchBird | `include/scratchbird/core/page_manager.h` | `TablespaceFSM` | `390` | Current in-memory FSM structure surface |
| ScratchBird | `src/core/page_manager.cpp` | `decodeTablespaceHeader` | `105` | Versioned tablespace header decode and validation |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::updateTablespaceHeader` | `2188` | Durable header persistence tied to current FSM-backed state |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::preallocatePages` | `2308` | FSM-aware capacity growth and preallocation |
| ScratchBird | `src/core/database.cpp` | `Database::create_fsm_page` | `4039` | Bootstrap FSM page construction |
| ScratchBird | `src/core/database.cpp` | `fsm_header->page_type = PAGE_TYPE_FSM_ROOT` | `4047` | Identifies the bootstrap page as an FSM root |
| ScratchBird | `src/core/database.cpp` | `fsm_data->total_pages`, `fsm_data->free_pages` | `4066`, `4067` | Current persisted FSM counters at bootstrap |
| ScratchBird | `src/core/database.cpp` | `fsm_data->bitmap[0] = 0x3F` | `4069` | Initial bitmap allocation state for fixed bootstrap pages |
| ScratchBird | `src/core/database.cpp` | `Database::create` calling `create_fsm_page` | `4484` | Creation path persists the FSM root as part of bootstrap |

## Drift and contradictions
- The old spec implied a richer extent-tree or class hierarchy than the current code proves.
- The reviewed code proves real FSM durability, but not a generalized extent allocator contract.

## Non-blocking expansion candidates
- A code-backed extent inventory if extents remain part of the intended design
- A canonical description of per-tablespace FSM persistence beyond the currently proven header and page-manager surfaces
- An operator-visible FSM integrity or reconstruction evidence surface

## Suggestions
- Keep `FSM` as the authoritative current term and treat `extent` language as provisional unless backed by new code.
