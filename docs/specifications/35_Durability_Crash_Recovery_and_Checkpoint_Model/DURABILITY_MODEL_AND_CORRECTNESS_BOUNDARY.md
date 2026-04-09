# Durability Model and Correctness Boundary

Status: current_authority

## 1. Scope

This file defines the authoritative durability contract for ScratchBird Alpha.

ScratchBird Alpha uses an MGA durability model. Durable correctness is derived
from durable page images, durable transaction inventory, durable catalog
evidence for prepared transactions, and durable header and control state.
Durability is not derived from WAL, redo log replay, undo log replay, or LSN
ordering.

## 2. Source of durable truth

The durable truth set is:

1. primary database page images
2. registered durable tablespace page images
3. durable transaction inventory state in TIP pages
4. durable CLOG transaction state
5. durable page-0 and control-page header state
6. durable prepared-transaction catalog rows and persisted prepared-lock rows

The following are not primary durable truth:

1. `wal_after` exports
2. remote archive database delivery
3. sweep manifests
4. shadow-capture forensic artifacts
5. debug traces or operator logs

Those may provide audit, forensic, replication, or archival value, but they do
not define commit truth.

## 3. MGA versus WAL durability boundary

ScratchBird durability must be understood in MGA terms:

1. the durable database image already contains the transaction-stamped truth
2. terminal transaction state is published through inventory pages and related
   durable state
3. restart reconciles durable page and inventory state

The following WAL-style explanation is non-canonical:

1. the authoritative commit event is a redo record
2. page state can be rebuilt later from replay
3. log position is the main correctness marker

ScratchBird does not use that model for Alpha correctness.

## 4. Forced-write durability modes

ScratchBird must interpret durability modes as follows:

1. `STRICT`: client-visible commit success requires a terminal forced-write fence before ACK
2. `GROUP_COMMIT`: client-visible commit success requires a batched terminal forced-write fence before the grouped ACKs are released
3. `DEVELOPMENT_UNSAFE`: the engine may acknowledge commits before the terminal durable fence completes

Implementation-grade authority must treat `STRICT` and `GROUP_COMMIT` as the
safe modes.

## 5. Start-transaction publication ordering

Beginning a transaction is itself a durability-sensitive publication step.

The required order is:

1. allocate `xid`
2. write TIP entry as `ACTIVE`
3. write CLOG entry as `IN_PROGRESS`
4. update page-0 `next_xid`
5. durably publish those pages with `flushTransactionPublicationState(...)`
6. only after that durable publication, publish the transaction into
   attachment-visible ProcArray state

This prevents restart from seeing a transaction that was attachment-visible but
not durably inventoried.

## 6. Commit ordering in safe modes

A safe-mode commit is not a single write. It is an ordered publication
sequence.

For non-group safe commit the required order is:

1. ensure the transaction is still `ACTIVE` in TIP
2. pre-flush dirty transactional state with `flushTransactionState(...)` before terminal TIP publication
3. write TIP terminal state as `COMMITTED` and persist the `commit_seqno`
4. write CLOG terminal state as `COMMITTED`
5. run `flushTransactionState(...)` so terminal transaction state reaches the engine-wide durable fence
6. clear ProcArray ownership only after terminal durability is established
7. update transaction markers
8. run `flushTransactionState(...)` again so marker movement is durably published
9. only after the terminal fence succeeds may the client receive commit success

For `GROUP_COMMIT`, the leader must:

1. batch terminal TIP entries
2. batch terminal CLOG state
3. execute `flushTransactionState(...)` for the batch
4. wake followers only after the batch fence succeeds

## 7. Rollback ordering in safe modes

Rollback also ends one transaction and immediately transitions the attachment
into the next transaction context. Safe-mode rollback must durably publish the
terminal rolled-back state before treating the old transaction as retired.

The required order is:

1. write terminal transaction state as `ROLLED_BACK`
2. write terminal CLOG state as `ROLLED_BACK`
3. force the terminal durable fence
4. clear ProcArray ownership
5. durably publish transaction-marker movement
6. continue in the next transaction context

`ABORTED` is reserved for non-transaction abort/cancel paths and is not the
durable terminal state name for a completed transaction rollback.

Rollback is not a return to an idle non-transactional mode.

## 8. Prepared transaction ordering

Prepared transactions require catalog-first publication.

The required order is:

1. create durable prepared-transaction catalog row
2. create durable prepared-lock snapshot rows
3. force a durable fence for those catalog rows
4. write TIP state as `PREPARED`
5. write CLOG state as `PREPARED`
6. force a durable fence for PREPARED transaction state
7. clear ProcArray ownership
8. update markers and force a final durable fence

Restart must treat `PREPARED` without durable catalog evidence as corruption.

## 9. Engine-wide forced-write fence

`Database::sync(...)` is the engine-wide forced-write fence.

When `Database::sync(...)` returns `OK` in a safe mode, the following are
required to be on stable storage:

1. the primary database file
2. every registered durable tablespace file descriptor
3. every active shadow filespace participating in durability for the primary
   database file
4. every active shadow filespace participating in durability for each
   registered tablespace

Until touched-filespace tracking exists for the foreground commit path, the
engine uses the conservative all-filespace fence.

## 10. Force-write posture rule

Firebird-style MGA durability expects forced writes and careful ordered writes.

ScratchBird must therefore treat the forced-write posture as applying to the
entire durability route:

1. main durable database file
2. registered durable tablespaces
3. active shadow durability routes

No safe-mode implementation may claim MGA-equivalent durability while allowing
terminal acknowledgement before those routes have crossed the required forced
write fence.

## 11. Shadow filespace boundary

Shadow filespaces participate in the configured durability contract as mirrored
page destinations.

Normative rules:

1. normal page writes are mirrored into active non-promoted shadow filespaces
2. a shadow-write failure blocks the caller because the configured shadow
   durability contract was not met
3. a shadow-sync failure blocks commit or publication acknowledgement for the
   same reason
4. the primary MGA page state remains the main recovery authority even when a
   shadow exists
5. a shadow can be promoted operationally, but promotion does not convert
   durability truth into WAL-style log truth

A shadow is a mirrored durable page route, not a replay log.

## 12. Dirty-frame and fsync boundary

Dirty frames are not fully durable merely because their bytes were written
earlier.

The engine uses a two-stage model:

1. page bytes may be written and staged as flushed-pending-fsync
2. those frames become fully clean only after the engine-wide forced-write
   fence completes

`buffer_pool_->completeFsyncFence()` is the boundary that retires the local
`DirtyFlushedPendingFsync` state.

## 13. Worked durability example

1. transaction `T1` creates or updates row versions in dirty pages
2. those pages may be flushed from the buffer pool
3. `T1` is still not safely committed until terminal TIP and CLOG publication is
   written and the engine-wide forced-write fence succeeds
4. only then may the client receive success

This is the careful ordered-write rule that distinguishes MGA forced-write
durability from WAL replay durability.

## 14. Write admission fail-closed rule

If writeback admission is fenced by an open writeback incident, the engine must
refuse transaction publication and terminal commit publication.

No client-visible safe commit may bypass an open writeback incident.

## 15. MGA anti-WAL rule

The following statements are false for ScratchBird Alpha and must not appear as
implementation guidance:

1. commit truth comes from a WAL record
2. restart reconstructs committed state by replaying a WAL stream
3. LSN advancement defines durable visibility
4. log durability can stand in for durable TIP, CLOG, page-image, or catalog
   publication

ScratchBird durability is MGA page-state reconciliation with forced writes and
ordered publication.

## 16. Certification requirements

Implementation is conforming only if tests prove:

1. no safe-mode commit ACK occurs before the engine-wide forced-write fence succeeds
2. restart never observes attachment-visible transactions lacking durable TIP and CLOG publication
3. prepared transactions survive only when durable catalog evidence and durable PREPARED TIP and CLOG state both exist
4. derivative `wal_after` or archival failure does not redefine commit truth
5. shadow-write or shadow-sync failure blocks safe-mode commit success when the shadow is active in the configured durability contract
