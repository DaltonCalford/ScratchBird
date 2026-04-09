# MGA-Aware Buffer Pool Policy

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The buffer pool is MGA-aware through explicit page classes, frame hints, dirty-state staging, and reclaim-horizon-derived signals. The policy remains advisory and must not redefine MGA visibility or recovery truth.

## Current implementation
- `MgaPageClass` is real and includes system, version-root, chain-heavy, GC-candidate, scan-probation, and temp-work roles.
- `MgaFrameHints` carries horizon, dead-version, chain-depth, and workload information.
- GC computes and publishes MGA hints after maturity scanning.
- Background writeback logic respects page class and temporary-work exclusion in candidate selection.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `MgaPageClass` | `111` | MGA-aware page-role vocabulary |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | Dirty-state audit contract | `160` | States durability staging must not redefine MGA truth |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `MgaFrameHints` | `188` | MGA page-class and horizon-hint structure |
| ScratchBird | `src/core/garbage_collector.cpp` | Reclaim-horizon capture | `572` | MGA-aware cleanup starts from reclaim-horizon authority |
| ScratchBird | `src/core/garbage_collector.cpp` | Maturity scan and hint classification | `705`, `757` | GC derives page class from reclaimable bytes and chain depth |
| ScratchBird | `src/core/garbage_collector.cpp` | `publishMgaFrameHintsGlobal` | `767` | MGA hints are published into the buffer pool |
| ScratchBird | `src/core/buffer_pool.cpp` | Background writer reads `MgaPageClass` | `5151` | Writeback policy consumes page class |
| ScratchBird | `src/core/buffer_pool.cpp` | Scan-probation writeback suppression | `5172` | MGA-aware page-class policy affects background writeback selection |

## Drift and contradictions
- The old prose implied a more complete MGA-specific residency controller than the current code proves.
- The current code does prove meaningful MGA-aware policy signals and queue decisions.

## Non-blocking expansion candidates
- A single MGA policy report showing how page class and hints influence replacement, prefetch, and flushing decisions
- Direct gate coverage for MGA page-class transitions and policy enforcement

## Suggestions
- Keep MGA truth outside buffer policy in the spec text, matching the current audit contract in code.
