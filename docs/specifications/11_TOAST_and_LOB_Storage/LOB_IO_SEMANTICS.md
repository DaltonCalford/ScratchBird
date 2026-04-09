# LOB I/O Semantics

## Purpose

This file defines the current oversized-value I/O contract. The authoritative runtime is `ToastManager`. ScratchBird does not currently define a generic standalone streaming LOB API.

## Public Operations

`initialize(...)`

`createToastTable(...)`

`toastValue(...)`

`detoastValue(...)`

`deleteToastValue(...)`

`retireTupleToastValue(...)`

`deleteToastValueHeapScan(...)`

`detoastIfNeeded(...)`

## Strategy Selection Rules

`chooseStrategy(...)` returns `PLAIN` when the value size is less than or equal to the TOAST threshold for the current page size.

If compression is disabled and the value must be externalized, `chooseStrategy(...)` returns `EXTENDED`.

If compression is enabled and the value size is greater than `2 * ToastSettings::getTarget(page_size)`, `chooseStrategy(...)` returns `EXTERNAL`.

All other externalized cases use `EXTENDED`.

## Write Semantics

`toastValue(...)` rejects `PLAIN`.

`toastValue(...)` rejects `COMPRESSED` as an invalid strategy for TOAST storage.

`toastValue(...)` with `EXTENDED` writes out-of-line uncompressed chunks.

`toastValue(...)` with `EXTERNAL` first attempts compression. If compression succeeds, the stored payload is compressed and the pointer marks `TOAST_COMPRESSED`. If compression fails, the manager falls back to uncompressed chunk storage and clears the compressed flag.

Chunk writes split the payload into fragments of at most `ToastSettings::getMaxChunkSize(db_->page_size())`.

Chunk rows are inserted in ascending `chunk_seq` order starting at `0`.

If chunk insertion fails after earlier chunks were inserted, the manager deletes the already inserted chunk tuples before returning failure.

## Read Semantics

`detoastValue(...)` rejects null pointers and invalid TOAST pointers.

`detoastValue(...)` reads chunk payloads through `readToastChunks(...)`.

If the pointer is not compressed, `detoastValue(...)` returns the stored payload directly.

If the pointer is compressed, `detoastValue(...)` validates `total_len` and decompresses the stored payload into the requested output buffer.

`detoastIfNeeded(...)` detoasts only when the supplied payload passes the canonical TOAST-pointer check. Otherwise it returns the original bytes unchanged.

## Delete and Retire Semantics

`deleteToastValue(...)` prefers indexed lookup through the `(chunk_id, chunk_seq)` index.

If index lookup is unavailable or ineffective, `deleteToastValue(...)` falls back to a heap scan.

Delete is MGA-safe soft delete. The manager updates `xmax` on each owning chunk row and does not mark the item pointer deleted.

`retireTupleToastValue(...)` extracts the referenced TOAST value ID from a tuple image and routes deletion through `deleteToastValue(...)`.

## Visibility Rules

Chunk visibility uses `ToastVisibility::evaluateChunkLifecycle(...)`.

Read paths may skip invisible chunks for the requesting transaction.

Maintenance or GC callers may request lifecycle evaluation with reclaim-horizon semantics that differ from ordinary read visibility.

## Unsupported Surfaces

This file does not authorize a generic standalone LOB handle API.

This file does not authorize seek-based or partial-read LOB session semantics.

This file does not authorize standalone parser or admin commands for large-object session management.
