# Buffer Pool and Flush

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The reviewed implementation has a real buffer pool with pin/unpin, dirty publication, per-page and all-page flush, tablespace flush, checkpoint-bound drain, and commit-fence coordination. The live pool is a shared frame array with logical policy domains, not a family of physically separate residency pools.

## Current implementation
- `BufferPool` is the engine-owned in-memory page cache.
- Pin, unpin, mark-dirty, flush-page, flush-all, prefetch, flush-tablespace, and checkpoint-bound drain APIs are all real.
- Dirty publication records dirty generations and checkpoint candidates.
- Commit-fence begin/end surfaces are real.
- Flush logic classifies queue state, performs writeback, tracks failures, and finishes or reclassifies frame state.
- `flushDirtyCheckpointBoundary` can republish nonresident checkpoint markers before completing checkpoint drain.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `BufferPool` | `56` | Engine-owned shared-frame buffer pool type |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `Config` | `248` | Pool sizing, layout, writeback, and policy controls |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `unpinPage`, `markDirty`, `flushPage` | `425`, `446`, `456` | Legacy tablespace-0 buffer and flush surface |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `pinPageGlobal`, `unpinPageGlobal` | `482`, `494` | GPID-based shared buffer residency surface |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `flushAll`, `prefetchPagesGlobal`, `flushTablespace` | `531`, `558`, `579` | Global flush, prefetch, and per-tablespace flush surfaces |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `flushDirtyCheckpointBoundary` | `596` | Checkpoint-bound dirty drain surface |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `beginCommitFence`, `endCommitFence` | `599`, `600` | Commit-fence coordination surface |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::beginCommitFence`, `BufferPool::endCommitFence` | `2624`, `2660` | Runtime commit-fence implementation |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::unpinPage`, `BufferPool::unpinPageGlobal` | `3262`, `3270` | Runtime unpin and dirty publication interaction |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::flushPage`, `BufferPool::flushAll`, `BufferPool::flushTablespace` | `3359`, `3453`, `3661` | Flush implementations for page, whole pool, and tablespace |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::markDirty`, `BufferPool::markDirtyGlobal` | `4894`, `4902` | Dirty publication entry points |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::publishDirtyGeneration` | `4870` | Dirty-generation clock and checkpoint-candidate publication |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::flushDirtyCheckpointBoundary` | `5285` | Checkpoint-bound drain implementation |
| ScratchBird | `include/scratchbird/core/database.h` | `write_admission_fenced`, `write_admission_status` | `524`, `525` | Database-owned refusal state consumed by higher durability paths |
| ScratchBird | `src/core/database.cpp` | `buffer_pool_->flushAll`, `buffer_pool_->flushDirtyCheckpointBoundary` | `3005`, `3024` | Database-level flush orchestration |
| ScratchBird | `src/core/database.cpp` | Checkpoint drain to `flushDirtyCheckpointBoundary` and `sync` | `5680`, `5692` | Checkpoint flow uses pool drain then forced-write sync |

## Drift and contradictions
- The old prose understated how much real dirty-state and checkpoint-drain logic now exists.
- The old prose also overstated layout diversity: `segmented` is real only as logical accounting over shared frames, while `hotcold` and `tablespace` layouts are not implemented.

## Non-blocking expansion candidates
- A single operator-visible buffer-pool status and queue surface tying frame state, dirty debt, queue state, and failure status together
- Full code-backed proof for every advertised alternative layout or a spec cleanup that removes the unsupported layouts from canonical promise language
- A section-owned gate matrix for checkpoint drain, tablespace flush, and commit-fence failure behavior

## Suggestions
- Keep this file focused on shared-frame cache truth, not on speculative future layouts.
- Treat any new residency architecture as a new implementation wave with separate proof obligations.
