# Writeback Failure and Disk Full Policy

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The current code proves fail-closed write-admission fencing, persisted
writeback incident control state, checkpoint-bound drain with sync, and
frame-local writeback failure handling. It does not yet prove the richer
operator incident and reserve-space policy described by older prose, and that
broader plane is not part of the current Beta 1 canonical requirement.

## Current implementation
- `Database` exposes `write_admission_fenced()` and `write_admission_status()`.
- Bootstrap system-state initialization stores `WritebackIncidentControlState`.
- Checkpoint flow drains dirty pages to a boundary, then forces sync with checkpoint attribution.
- Buffer-pool writeback paths mark per-frame writeback failures.
- Conservative reserve-space handling exists in the page-manager implementation, but this pass did not promote it to a full operator-visible disk-full policy.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/database.h` | Write-admission audit contract | `522` | Commit/publication must fail closed when admission is fenced |
| ScratchBird | `include/scratchbird/core/database.h` | `write_admission_fenced`, `write_admission_status` | `524`, `525` | Database-owned refusal surface |
| ScratchBird | `src/core/database.cpp` | `Database::write_admission_fenced`, `Database::write_admission_status` | `2693`, `2699` | Runtime getters for write-admission posture |
| ScratchBird | `src/core/database.cpp` | Persisted writeback incident fences durability claims | `2780` | Writeback incident state is durable and fail-closed |
| ScratchBird | `src/core/database.cpp` | `Database::create_system_state_page` | `4103` | Bootstrap system-state page creation |
| ScratchBird | `src/core/database.cpp` | `storeWritebackIncidentControlState(state_page, ...)` | `4137` | Bootstrap persistence of writeback incident control state |
| ScratchBird | `src/core/database.cpp` | Checkpoint lifecycle phases | `5665`, `5673` | Checkpoint enters dirty-drain phases before sync |
| ScratchBird | `src/core/database.cpp` | Dirty drain and forced-write sync | `5680`, `5692` | Checkpoint failure surface ties pool drain to sync |
| ScratchBird | `src/core/database.cpp` | Checkpoint writeback attribution | `5688` | Checkpoint sync is explicitly attributed |
| ScratchBird | `src/core/buffer_pool.cpp` | `writePageToDisk` during flush-tablespace and failure marking | `3711`, `3714` | Flush failure is recorded in frame state |
| ScratchBird | `src/core/buffer_pool.cpp` | Background writer failure marking | `5267` | Background failure path updates writeback-failure state |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::flushDirtyCheckpointBoundary` | `5285` | Checkpoint-specific dirty drain surface |

## Drift and contradictions
- The current code is stronger on fail-closed write admission than the old prose captured.
- The old prose still describes a broader disk-full and operator incident plane than the reviewed code proves.

## Deferred beyond current canonical scope
- A single operator-visible writeback incident register and refusal matrix
- A proven reserve-space policy with direct evidence surfaces and tests
- A clear distinction between transient frame-local writeback failure,
  checkpoint failure, and persistent write-admission fencing

## Suggestions
- Keep this file narrowly anchored to the fail-closed durability posture the code actually proves.
- Promote broader disk-full handling only when the operator surface is code-backed and auditable.
