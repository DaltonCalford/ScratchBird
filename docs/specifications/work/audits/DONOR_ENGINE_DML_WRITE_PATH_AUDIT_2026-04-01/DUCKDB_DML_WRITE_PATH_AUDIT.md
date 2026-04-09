# DuckDB DML Write-Path Audit

## Architectural Summary

DuckDB is valuable because it shows how to keep a mutable write path simple while still doing heavy durability and index work around checkpoints. The two strongest ideas are row-group-local batching and the explicit merge of index deltas after a checkpoint succeeds.

## Insert Optimizations

- Appends land in row groups with strong locality.
- Checkpointing writes a clear "checkpoint started" marker and planned root metadata before the expensive durable rewrite proceeds.
- The engine is optimized around writing coherent table and metadata batches rather than scattering random tiny updates.

## Update/Delete Optimizations

- `row_group.cpp` batches deletes by vector and row group instead of treating each row as a separate physical event.
- Actual delete application deduplicates repeated row hits and only pushes delete records into the undo buffer if real deletes occurred.
- Delete tracking stays local to version information for the row group, which keeps write-path locality strong.

## Index Maintenance Optimizations

- During checkpoint, DuckDB allows indexes to accumulate delta state.
- After the checkpoint is durably established, `checkpoint_manager.cpp` explicitly merges checkpoint deltas back into the main index structures.
- This is a powerful lesson: expensive index reconciliation does not always have to be paid before the durable barrier is known to have succeeded.

## Reliability And Publication Pattern

- The checkpoint protocol writes a flag to the WAL, records the planned root metadata, and then decides on recovery by comparing what was planned with what actually became durable.
- If the checkpoint completed, WAL replay can be shortened or avoided; if not, replay resumes from the durable boundary.
- This keeps recovery decisions explicit and cheap.

## Best Borrow Candidates For ScratchBird

- Row-group or page-local delete batching.
- Checkpoint-time temporary index deltas with post-checkpoint merge.
- Tiny explicit recovery markers that cleanly distinguish "planned" from "completed."

## Local Source Anchors

- `src/storage/checkpoint_manager.cpp`
- `src/storage/table/row_group.cpp`
- `src/storage/data_table.cpp`
- `src/execution/index/art/art.cpp`
