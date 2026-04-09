# Checkpoint Bound Delta Reconciliation and Maintenance Markers

Status: reconstructed_required_with_current_substrate

## Purpose

Define the only admitted Beta 1 model for checkpoint-bound reconciliation of
non-commit-critical maintenance deltas. This file exists so deferred merge work
does not invent WAL-like authority or ambiguous restart behavior.

## Scope

This file owns:

- durable checkpoint-bound maintenance markers
- admitted maintenance classes for post-boundary merge
- checkpoint-time publication order for those markers
- restart behavior for incomplete merge work
- refusal rules for commit-critical structures

## Hard invariants

1. Checkpoint is not WAL checkpointing.
2. The durable page image and transaction inventory remain authoritative.
3. A checkpoint marker may certify only post-boundary maintenance work, never
   row visibility or commit truth.
4. Unique proof, exact visibility, and transaction publication are forbidden
   from this model.

## Admitted maintenance classes

Checkpoint-bound delta reconciliation is admitted only for:

- `BLOOM_REBUILD`
- `BRIN_SUMMARY_REWRITE`
- `COLD_PAGE_DELTA_MERGE`
- `RANKED_SEGMENT_SEAL`
- `RANKED_SEGMENT_MERGE`
- `ANN_GENERATION_SEAL`
- `ANN_GENERATION_MERGE`
- `COLUMNSTORE_DELTA_MERGE`

The model is forbidden for:

- unique exact indexes
- primary-key exact indexes
- exact-family commit-bound leaf or bucket modifications
- any state whose loss would change commit visibility

## Durable checkpoint maintenance marker

Checkpoint-bound maintenance state shall persist in checkpoint control as a
repeated `maintenance_marker` record with this logical shape:

```cpp
enum class MaintenanceMarkerState : uint8_t {
  kPrepared,
  kBoundaryDurable,
  kMergePending,
  kMergeActive,
  kMergeComplete,
  kFailedFence,
};

struct MaintenanceMarker {
  Uuid marker_uuid;
  Uuid logical_object_uuid;
  uint64_t checkpoint_epoch;
  MaintenanceDebtClass debt_class;
  MaintenanceMarkerState state;
  uint64_t source_generation;
  uint64_t target_generation;
  uint64_t payload_crc64;
};
```

The checkpoint control payload shall carry enough repeated marker slots to
persist every active post-boundary maintenance task admitted by this file.

## Marker lifecycle

Required lifecycle:

1. `kPrepared`
2. `kBoundaryDurable`
3. `kMergePending`
4. `kMergeActive`
5. `kMergeComplete`
6. `kFailedFence`

Rules:

1. `kPrepared` means the maintenance payload exists but the checkpoint fence has
   not completed.
2. `kBoundaryDurable` means the checkpoint fence completed and the payload is
   eligible for post-boundary merge.
3. `kMergePending` means restart or background workers may claim the work.
4. `kMergeActive` means one worker lease currently owns the merge.
5. `kMergeComplete` means the merged structure is durable and the marker can be
   retired in a later checkpoint.
6. `kFailedFence` means the marker or target structure is ambiguous and the
   object must enter fail-closed repair or rebuild.

## Checkpoint flow

Required checkpoint order:

1. prepare the family-local delta payload or sealed mutable generation
2. write `maintenance_marker` entries in `kPrepared`
3. flush dirty data and checkpoint control pages
4. execute the engine-wide forced-write fence
5. rewrite the same markers in `kBoundaryDurable`
6. close the checkpoint fence
7. schedule the corresponding debt items as `kMergePending`

No step may claim `kBoundaryDurable` before the forced-write fence succeeds.

## Restart flow

At startup:

1. scan checkpoint control for active `maintenance_marker` entries
2. if a marker is `kPrepared`, discard the associated delta payload and keep the
   pre-boundary structure authoritative
3. if a marker is `kBoundaryDurable` or `kMergePending`, recreate debt items and
   resume merge work
4. if a marker is `kMergeActive`, downgrade it to `kMergePending` and resume
5. if a marker is `kMergeComplete`, verify the merged target exists and retire
   the marker on the next successful checkpoint
6. if target state and marker state disagree, move the marker to
   `kFailedFence` and refuse ordinary write admission for that object until
   repair or rebuild

## Post-boundary merge algorithm

```cpp
void reconcileMaintenanceMarker(MaintenanceMarker& marker) {
  if (marker.state == MaintenanceMarkerState::kPrepared) {
    discardPreparedDelta(marker);
    return;
  }
  if (marker.state == MaintenanceMarkerState::kBoundaryDurable ||
      marker.state == MaintenanceMarkerState::kMergePending) {
    marker.state = MaintenanceMarkerState::kMergeActive;
    applyFamilyLocalMerge(marker);
    persistMergedTarget(marker);
    marker.state = MaintenanceMarkerState::kMergeComplete;
  }
}
```

## Failure classes

Required failure classes:

- `CHECKPOINT_MARKER_WRITE_FAILURE`
- `CHECKPOINT_MARKER_PAYLOAD_MISMATCH`
- `CHECKPOINT_MERGE_TARGET_MISSING`
- `CHECKPOINT_MERGE_TARGET_CORRUPT`
- `CHECKPOINT_MERGE_REPLAY_REFUSED`

Required handling:

1. marker-write failure prevents `kBoundaryDurable`
2. payload mismatch moves the object to `kFailedFence`
3. target corruption requires repair or full rebuild
4. replay refusal does not change transaction truth; it fences only the
   affected maintenance object

## Tunables

| Tunable | Default | Range | Reloadability |
| --- | --- | --- | --- |
| `sb.checkpoint.maintenance_marker_max` | `4096` | `256..65536` | restart required |
| `sb.checkpoint.post_boundary_merge_budget_ms` | `50` | `1..5000` | reloadable |
| `sb.checkpoint.post_boundary_merge_batch` | `64` | `1..1024` | reloadable |

## Required tests

1. `kPrepared` markers are discarded on restart
2. `kBoundaryDurable` markers resume merge work after restart
3. merged targets never become visible without a valid durable marker
4. exact-family unique or primary-key maintenance cannot enter this model
5. marker and target disagreement fences the object fail closed

## Cross-section references

- `CHECKPOINT_AND_DIRTY_STATE_MODEL.md`
- `BACKGROUND_MAINTENANCE_AND_RECOVERY_INTERACTION.md`
- `../18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md`
- `../08_Transaction_Core/MGA_TRANSACTION_PUBLICATION_AND_RESTART_SEMANTICS.md`
