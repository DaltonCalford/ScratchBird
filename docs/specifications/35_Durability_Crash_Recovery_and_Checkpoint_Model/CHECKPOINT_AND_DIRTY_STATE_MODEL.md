# Checkpoint And Dirty State Model

Status: current_authority

## 1. Scope

This file defines ordered writeback, checkpoint, dirty-frame staging, and publication sequencing for ScratchBird Alpha.

The model is MGA page-state publication with forced writes. It is not WAL checkpointing.

## 2. Dirty State Vocabulary

A page/frame must be classified into one of these effective states:

1. clean: resident image matches durable image.
2. dirty: resident image contains unpublished changes.
3. flushed_pending_fsync: bytes have been written, but the engine-wide forced-write fence has not yet completed.
4. durably_published: bytes and associated publication markers are on stable storage after the forced-write fence.

`flushed_pending_fsync` is not allowed to be treated as durable truth.

## 3. Publication Fence Entry And Exit

Transaction publication and checkpoint publication use the same durable-fence discipline.

At fence entry:

1. `buffer_pool_->beginCommitFence()` marks the fence window.
2. dirty buffers are flushed.
3. `Database::sync(...)` executes the engine-wide stable-storage fence.

At fence exit:

1. `buffer_pool_->endCommitFence()` closes the fence window.
2. `buffer_pool_->completeFsyncFence()` retires `flushed_pending_fsync` state only after `sync()` succeeds.

A page is not logically clean until the fence exit conditions are met.

## 4. Ordered Publication For Transaction-Start Inventory

`flushTransactionPublicationState(page_ids, ...)` defines the ordered publication contract for transaction-start inventory and related marker pages.

The required order is:

1. flush page-manager state when present.
2. flush page `0` first.
3. flush every listed publication page after page `0`.
4. execute `Database::sync(...)` using the same conservative all-filespace forced-write fence used for terminal durability.

Page `0` is flushed first because `next_xid`, oldest-transaction markers, and other header-visible publication state must not lag behind the durable inventory transition they certify.

## 5. Engine-Wide Sync Ordering

`Database::sync(...)` must execute the durable fence in this order:

1. sync the primary database file.
2. sync active shadow filespaces for the primary database file.
3. enumerate every registered durable tablespace.
4. sync each registered tablespace file descriptor.
5. sync active shadow filespaces for that tablespace.
6. only then retire `DirtyFlushedPendingFsync` state in the buffer pool.

This is a conservative correctness fence. It prioritizes durable ordering over foreground optimization.

## 6. Ordinary Page Write Ordering

Ordinary page writes obey this write order:

1. write the page image to the source filespace.
2. mirror the same page image to any active shadow filespace bound to that source.
3. leave the frame in a flushed-pending-fsync state until the engine-wide sync completes.

A successful page write without the subsequent sync is insufficient for safe-mode commit or checkpoint publication.

## 7. Shadow Backfill Ordering

Shadow filespace creation and backfill must:

1. copy source pages into the shadow filespace.
2. force a sync on the shadow after create/backfill.
3. only then treat the shadow as an active mirrored route.

A partially backfilled or unsynced shadow is not a valid durable participant.

## 8. Checkpoint Semantics

Checkpoint in ScratchBird Alpha means durably reducing dirty publication debt and reconciling restart markers against MGA state.

Checkpoint is not permission to discard authoritative historical state needed for visibility.

Checkpoint must preserve:

1. TIP/CLOG truth.
2. committed page images.
3. prepared transaction evidence.
4. marker consistency across page `0`, TIP horizon state, and restart metadata.

## 9. Recovery Interpretation Of Dirty State

On restart, the engine must treat:

1. durably published state as authoritative.
2. flushed-pending-fsync state as non-authoritative unless the fence completion was durably established.
3. missing terminal transaction publication as uncommitted or requiring startup reconciliation.

Restart is therefore a state-reconciliation problem, not a WAL replay problem.

## 10. Non-Guarantees

The following are not current Alpha guarantees:

1. touched-filespace-only foreground fsync narrowing.
2. WAL checkpoint LSN ordering.
3. log-authoritative redo pass.
4. background checkpoint semantics that can override TIP/CLOG/page-state truth.

## 11. Certification Requirements

Implementation is conforming only if tests prove:

1. page `0` is flushed before later transaction-publication pages in the ordered publication path.
2. safe-mode ACK is impossible until `Database::sync(...)` completes across every durability participant.
3. shadow filespace write or sync failure prevents safe-mode publication success.
4. frames are not marked fully clean before the fsync fence completes.
