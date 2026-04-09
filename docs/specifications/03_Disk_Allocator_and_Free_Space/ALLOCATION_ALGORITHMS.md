# Allocation Algorithms

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The reviewed implementation proves page-level free-space management,
tablespace-local allocation, autoextend, preallocation, and reconstruction. It
does not prove the richer extent-class allocator described by older prose, and
section `03` does not currently require that richer allocator.

## Current implementation
- The primary free-space root is bootstrapped as page `3` with a bitmap-based FSM root page.
- `PageManager::allocatePageInTablespace` is the current allocation authority.
- `PageManager` exposes create, open, close, extend, preallocate, update-header, flush, and reconstruct surfaces around tablespace allocation state.
- `preallocatePages` expands capacity conservatively and updates tablespace-local free-space state.
- `freePageGlobal` exists, but this pass centered on allocation and growth proof rather than full free or recycle policy closure.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/page_manager.h` | FSM authority contract | `32` | Declares allocation/FSM publication ownership boundary |
| ScratchBird | `include/scratchbird/core/page_manager.h` | Primary FSM bootstrap page comment | `42` | States page `3` as the primary bitmap-backed FSM root |
| ScratchBird | `include/scratchbird/core/page_manager.h` | `allocatePageInTablespace` | `101` | Current allocator entry point |
| ScratchBird | `include/scratchbird/core/page_manager.h` | `freePageGlobal` | `113` | Free-page surface exists, though recycle policy is not fully closed in this pass |
| ScratchBird | `include/scratchbird/core/page_manager.h` | `extendTablespace`, `preallocatePages`, `updateTablespaceHeader` | `208`, `233`, `249` | Growth, preallocation, and header persistence surfaces |
| ScratchBird | `include/scratchbird/core/page_manager.h` | `reconstructFromPages`, `TablespaceFSM` | `297`, `390` | Rebuild and FSM structure surfaces |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::allocatePageInTablespace` | `957` | Current page-level allocation implementation |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::extendTablespace` | `1916` | Conservative capacity growth path |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::preallocatePages` | `2308` | Preallocation implementation |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::updateTablespaceHeader` | `2188` | Header update path used by allocator-side growth state |
| ScratchBird | `src/core/database.cpp` | `Database::create_fsm_page` | `4039` | Bootstrap FSM root creation |
| ScratchBird | `src/core/database.cpp` | `Database::create` calling `create_fsm_page` | `4484` | Primary database bootstrap wires the FSM root into creation |

## Drift and contradictions
- The old spec described `extent_free_class` and `page_free_class` style allocation policy more strongly than the code proves.
- The current code proves a durable allocator, but it is driven by FSM bitmap state and tablespace growth rather than a generalized extent-placement engine.

## Deferred beyond current canonical scope
- A proven extent allocator beyond the current page-level FSM model
- A single operator-visible allocation-pressure surface covering reserve pages,
  autoextend failures, and fragmentation pressure
- Full code-backed closure for free-page recycling semantics and any
  family-specific allocation exceptions

## Competitive parity closure requirements

For the competitive-performance parity package, allocator growth and
preallocation are speed-significant rather than just lifecycle substrate.

Before parity closure, the allocator shall:

1. accept ahead-of-demand growth hints from admitted batch write shapes owned
   by sections `34` and `39`
2. reserve a page run or growth window large enough that the hot write loop
   does not repeatedly trigger extend or preallocate syscalls
3. amortize FSM and header publication across the admitted growth window
4. publish refusal or pressure state if it cannot reserve the requested window
   legally

Repeated page-by-page file growth or preallocation inside a benchmark-governed
load, multi-row insert, or `INSERT ... SELECT` hot loop is non-conforming when
future demand is already known.

## Suggestions
- Rewrite future allocator work around the code that exists today: page allocation, growth, preallocation, header update, and reconstruction.
- Treat extent-class placement as missing implementation until a code-backed authority exists.
