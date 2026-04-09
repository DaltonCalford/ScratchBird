# MGA Buffer Locality and Visibility

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The current implementation separates locality hints from visibility truth. MGA visibility remains transaction or horizon authority, while the buffer pool only carries advisory state about page role, dirty progress, and locality-relevant hints.

## Current implementation
- Buffer-pool audit contracts explicitly state that policy is not recovery or visibility authority.
- Frame snapshots carry owner/home partition, page class, workload class, residency tier, lifecycle state, and dirty state.
- GC publishes locality-relevant hints such as oldest interesting TXID, reclaim horizon, reclaimable bytes, and chain depth.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | Buffer-pool audit contract | `42` | Explicitly states buffer policy is not MGA recovery truth |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | Dirty-state audit contract | `160` | Dirty staging is durability posture, not visibility truth |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `MgaFrameHints`, `MgaFrameSnapshot` | `188`, `201` | Advisory locality and visibility-adjacent hint surfaces |
| ScratchBird | `src/core/garbage_collector.cpp` | Chain-depth and oldest-interesting-TXID derivation | `729`, `763` | GC derives locality-relevant MGA hints |
| ScratchBird | `src/core/garbage_collector.cpp` | Horizon, dead-version bytes, and chain-depth hint population | `764`, `765`, `766` | Advisory locality hints are populated here |
| ScratchBird | `src/core/garbage_collector.cpp` | Page-class selection from maturity scan | `757` | Hint publication begins from GC maturity state, not visibility authority |

## Drift and contradictions
- The old prose mixed locality and visibility more than the reviewed code does.
- The current code is clearer: locality is advisory, visibility is not delegated to the buffer pool.

## Non-blocking expansion candidates
- A direct operator evidence surface tying MGA visibility pressure to buffer-local hints
- A proof table linking locality hints to concrete runtime decisions beyond the currently reviewed GC and writeback paths

## Suggestions
- Preserve the current fail-closed wording: locality policy may not redefine MGA visibility or checkpoint truth.
