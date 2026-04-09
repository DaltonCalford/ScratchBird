# Buffer Write Shaping and Fragmentation Control

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
Write shaping is real through dirty-ratio thresholds, queue-state prioritization, usage-count ordering, and checkpoint-bound drain. Fragmentation control is only partially proven through allocator growth, preallocation, and page-family-local compaction behavior elsewhere; there is no single section `03` fragmentation controller.

## Current implementation
- Adaptive write shaping exists in the background writer.
- Queue-state priority influences writeback order.
- Dirty generation and usage count influence candidate ordering.
- Tablespace preallocation and conservative growth exist in `PageManager`.
- Checkpoint drain prevents unbounded dirty carryover past a durability boundary.

## Implementation code map
- `ScratchBird/src/core/buffer_pool.cpp:5039,5099,5192,5243,5285`
- `ScratchBird/src/core/page_manager.cpp:1916,2308`
- `ScratchBird/src/core/database.cpp:5680,5692`

## Drift and contradictions
- Older prose described a richer fragmentation control plane than the code proves.
- The current implementation proves write shaping more strongly than fragmentation governance.

## Non-blocking expansion candidates
- A canonical fragmentation metric and repair surface
- An operator-visible distinction between allocator pressure, logical page fragmentation, and background write debt
- A section-owned test or gate matrix for write-shaping fairness and fragmentation claims

## Suggestions
- Keep write shaping and fragmentation as separate concerns in future planning.
- Do not present allocator preallocation as proof of a full fragmentation-control subsystem.
