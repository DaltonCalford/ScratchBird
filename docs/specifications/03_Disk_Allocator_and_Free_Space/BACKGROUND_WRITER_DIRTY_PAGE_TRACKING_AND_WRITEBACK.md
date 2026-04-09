# Background Writer, Dirty Page Tracking, and Writeback

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The background writer and dirty-page staging model are real. Dirty generations, queue-state classification, adaptive flush thresholds, and checkpoint-bound draining are all code-backed. The old prose still overstates the maturity of a fully centralized multi-queue writeback service.

## Current implementation
- Dirty pages move through explicit `DirtyState` values.
- Queue-state attribution is tracked through `WritebackQueueState`.
- Dirty publication increments a dirty-generation clock and records checkpoint candidates.
- The background writer thread wakes on a configured interval and computes work from dirty-ratio thresholds.
- Writeback candidate ordering prefers explicit queue priority, then oldest dirty generation, then lower usage count.
- Background writer failures and foreground flush failures are both recorded in frame state.
- Checkpoint drain uses `flushDirtyCheckpointBoundary` and then `Database::sync` with checkpoint attribution.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | Writeback queue audit contract | `125` | Queue-state exists for scheduling readability, not recovery truth |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `WritebackQueueState` | `130` | Queue-state vocabulary for dirty resident pages |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `DirtyState` | `165` | Explicit dirty-state lifecycle vocabulary |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | Background-writer config fields | `270`, `271`, `272`, `273`, `274`, `275`, `276` | Threshold and batch controls |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::publishDirtyGeneration` | `4870` | Dirty-generation clock and checkpoint-candidate registration |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::startBackgroundWriter`, `BufferPool::backgroundWriterMain` | `4968`, `4999` | Thread startup and main loop |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::backgroundWriterFlush` | `5039` | Adaptive flush cycle implementation |
| ScratchBird | `src/core/buffer_pool.cpp` | Queue priority ordering lambda | `5099` | Queue-state priority determines writeback order |
| ScratchBird | `src/core/buffer_pool.cpp` | `beginFrameWriteback` during background flush | `5249` | Dirty frame enters writeback staging |
| ScratchBird | `src/core/buffer_pool.cpp` | `markFrameWritebackFailure` during background flush | `5267` | Failure path updates frame writeback state |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::flushDirtyCheckpointBoundary` | `5285` | Checkpoint-specific dirty drain and nonresident marker republish |
| ScratchBird | `include/scratchbird/core/database.h` | `write_admission_fenced`, `write_admission_status` | `524`, `525` | Database-level refusal state surfaced to durability code |
| ScratchBird | `src/core/database.cpp` | `storage.buffer.writeback.enabled` loading | `1541` | Canonical writeback enablement config |
| ScratchBird | `src/core/database.cpp` | `batch_pages`, `low_dirty_pct`, `high_dirty_pct`, `checkpoint_target_pct` loading | `1556`, `1576`, `1600`, `1624` | Batch and threshold configuration loading |
| ScratchBird | `src/core/database.cpp` | Persisted writeback incident fences new durability claims | `2780` | Writeback incident state affects admission posture |
| ScratchBird | `src/core/database.cpp` | Checkpoint lifecycle and dirty drain phases | `5665`, `5673`, `5680`, `5692` | Checkpoint drain then sync with checkpoint attribution |

## Drift and contradictions
- The code proves more real writeback staging than the old spec captured.
- The old prose still reads as if every queue family is already an operator-visible subsystem with complete rebuild and evidence semantics. This pass only proved queue-state tracking inside the buffer pool plus checkpoint drain orchestration in `Database`.

## Non-blocking expansion candidates
- A single writeback debt and queue-state operator surface
- A central incident and refusal matrix that explains writeback failure, retry, fence, and recovery posture to operators
- Direct test or gate closure for queue rebuild, background-writer failure persistence, and disk-full handling

## Suggestions
- Keep this file centered on the currently proven queue-state and dirty-generation model.
- Treat any broader queue service or operator incident plane as new implementation work rather than implied present behavior.
