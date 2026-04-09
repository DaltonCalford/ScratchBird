# Version Placement, Locality, and Fragmentation Policy

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The reviewed code proves version-related page classification and frame hints. It does not prove a separate version-placement engine or a generalized fragmentation policy as described by older prose.

## Current implementation
- `MgaPageClass` includes `VERSION_ROOT`, `CHAIN_HEAVY`, and `GC_CANDIDATE`.
- GC computes chain depth, oldest interesting TXID, reclaimable bytes, and reclaim horizon.
- Those values are published as buffer-pool hints.
- Actual version placement remains grounded in heap-page and reclaim logic rather than a standalone locality service.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `MgaPageClass`, `MgaFrameHints` | `111`, `188` | Version-aware page-class and hint structures |
| ScratchBird | `src/core/garbage_collector.cpp` | `scanVersionMaturity` | `705` | GC classifies reclaimable version state |
| ScratchBird | `src/core/garbage_collector.cpp` | Chain-depth derivation | `729` | GC computes version-chain depth hints |
| ScratchBird | `src/core/garbage_collector.cpp` | Page-class assignment from maturity and chain depth | `757` | Version-root/chain-heavy/gc-candidate selection |
| ScratchBird | `src/core/garbage_collector.cpp` | Oldest-interesting TXID, reclaim horizon, reclaimable bytes, chain depth hints | `763`, `764`, `765`, `766` | Advisory version-locality hints are published here |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::allocatePageInTablespace` | `957` | Actual placement authority remains the page allocator, not a separate version-placement engine |

## Drift and contradictions
- The old prose read like version placement had a broader allocator-level policy authority than the current code proves.
- The code currently proves hint publication and page-role classification, not a complete placement controller.

## Non-blocking expansion candidates
- A direct version-placement policy if the engine intends one beyond heap-local behavior
- Fragmentation metrics and operator-visible evidence tied to version-chain locality
- Tests or gates proving any future locality or placement promises

## Suggestions
- Keep version-placement claims conservative until the runtime implements a real policy engine beyond page hints.
