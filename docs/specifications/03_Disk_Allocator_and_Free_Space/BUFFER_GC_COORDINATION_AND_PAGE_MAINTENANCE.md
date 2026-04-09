# Buffer GC Coordination and Page Maintenance

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
GC or buffer coordination is real for heap-page cleanup, dirty-page tracking, sweep-blocked wake logic, and MGA frame-hint publication. The reviewed code does not prove one central page-maintenance subsystem spanning every page family.

## Current implementation
- Cooperative and background GC modes are real.
- Dirty pages are tracked in `GarbageCollector` using a priority map.
- Background GC sleeps, wakes, and can be unblocked by sweep completion or sweep evidence release.
- GC pins heap pages through `BufferPool` using vacuum access strategy.
- GC validates heap layout, audits version-chain metadata, computes reclaimability, prunes heap pages, and unpins dirty if modified.
- GC publishes MGA frame hints back into the buffer pool.
- Index cleanup publication is invoked after heap-proof cleanup when dead TIDs exist.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/garbage_collector.h` | `GCPolicy` | `32` | Cooperative/background/combined GC policy vocabulary |
| ScratchBird | `include/scratchbird/core/garbage_collector.h` | `GarbageCollector` | `117` | Heap-page cleanup coordinator type |
| ScratchBird | `include/scratchbird/core/garbage_collector.h` | `processPageCooperative`, `markPageDirty`, `getStatistics` | `132`, `140`, `153` | Cooperative cleanup, dirty tracking, and stats surface |
| ScratchBird | `include/scratchbird/core/garbage_collector.h` | `notifySweepComplete`, `notifySweepEvidenceBlocked`, `publishCleanupAfterHeapProof` | `160`, `161`, `165` | Sweep coordination and post-heap-proof publication surface |
| ScratchBird | `include/scratchbird/core/garbage_collector.h` | `shouldRunCooperativeGC` | `251` | Cooperative rate-limiter surface |
| ScratchBird | `src/core/garbage_collector.cpp` | `GarbageCollector::markPageDirty` | `362` | Dirty-page priority tracking |
| ScratchBird | `src/core/garbage_collector.cpp` | `notifySweepComplete`, `notifySweepEvidenceBlocked` | `447`, `461` | Sweep wake and prune-block propagation |
| ScratchBird | `src/core/garbage_collector.cpp` | `GarbageCollector::backgroundGCLoop` | `486` | Background GC main loop |
| ScratchBird | `src/core/garbage_collector.cpp` | `GarbageCollector::cleanPage` | `569` | Heap-page cleanup implementation |
| ScratchBird | `src/core/garbage_collector.cpp` | `captureReclaimHorizons` handoff | `573` | GC cleanup uses reclaim-horizon authority |
| ScratchBird | `src/core/garbage_collector.cpp` | `buffer_pool()->pinPage(... Vacuum ...)` | `586` | GC enters heap cleanup through buffer pool |
| ScratchBird | `src/core/garbage_collector.cpp` | `auditVersionChainMetadata`, `scanVersionMaturity` | `657`, `708` | Cleanup legality and maturity classification |
| ScratchBird | `src/core/garbage_collector.cpp` | `MgaFrameHints` classification and publication | `757`, `767` | GC publishes MGA-aware frame hints into buffer policy |
| ScratchBird | `src/core/garbage_collector.cpp` | `prunePage`, `unpinPage`, `publishCleanupAfterHeapProof` | `772`, `832`, `860` | Heap prune, dirty publication, and index cleanup publication |
| ScratchBird | `src/core/garbage_collector.cpp` | `shouldRunCooperativeGC` | `973` | Cooperative GC rate-limiting logic |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `AccessStrategy::Vacuum`, `MgaFrameHints` | `59`, `188` | Buffer-pool surfaces consumed by GC |

## Drift and contradictions
- The old prose implied a more general page-maintenance coordinator than the current code proves.
- The reviewed code is heap-centric and reclaim-horizon aware; it is not a universal page scrubbing or maintenance plane.

## Non-blocking expansion candidates
- A unified maintenance registry across heap, index, temporary-work, and allocator page families
- A single operator evidence surface for pages blocked from cleanup by sweep or version-chain anomalies
- Stronger closure between GC state, sweep state, and operator-visible maintenance classes

## Suggestions
- Keep this spec centered on proven heap-page cleanup and buffer interaction.
- Treat broader page-maintenance orchestration as new implementation work, not current guarantee.
