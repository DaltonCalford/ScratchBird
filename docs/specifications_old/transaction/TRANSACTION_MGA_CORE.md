# ScratchBird Transaction Management - MGA Core (Firebird Model)


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](FIREBIRD_CONSTANTS_REFERENCE.md)


## Part 1 of Transaction and Lock Management Specification

## Purpose
This specification defines the ScratchBird transaction, snapshot, and garbage collection model.
It is intentionally derived from FirebirdSQL’s implementation. ScratchBird follows Firebird’s MGA
(Multi-Generational Architecture) semantics, not PostgreSQL MVCC. Any design or implementation
must conform to this model.

## Authority and Sources
Authoritative reference implementation is FirebirdSQL.
Primary implementation pointers:
- Firebird transactions: `firebird/src/jrd/tra.cpp`
- Record versioning and GC: `firebird/src/jrd/vio.cpp`
- TIP cache and read consistency notes: `firebird/doc/README.read_consistency.md`
- Lock manager data used by transactions: `firebird/src/jrd/lck.cpp`

ScratchBird differences are limited to identifiers (UUID usage) and 64-bit transaction IDs.
All behavior and sequencing below must match Firebird semantics.

---

## 1. Transaction Identity and States

### 1.1 Transaction IDs
- Firebird uses monotonically increasing transaction IDs tracked in TIP pages.
- ScratchBird uses 64-bit IDs to avoid wraparound. Behavior is otherwise the same.

Special IDs:
- `0` invalid
- `1` system/bootstrap
- `2` frozen/reserved
- First normal transaction starts at `3`

### 1.2 Transaction States (TIP)
Firebird stores 2-bit states per transaction in TIP (transaction inventory pages).
States:
- `active`
- `committed`
- `dead` (aborted/rolled back)
- `limbo` (two-phase commit prepared)

TIP rules:
- TIP is the source of truth for transaction visibility.
- TIP pages are extended as transaction IDs grow.
- In read-only databases, TIP updates are minimized; state is tracked in cache.

---

## 2. Database Transaction Markers

The database header maintains three critical markers:
- **OIT** (Oldest Interesting Transaction)
- **OAT** (Oldest Active Transaction)
- **OST** (Oldest Snapshot Transaction)

Definitions, per Firebird:
- **OIT** is the oldest transaction whose state is still relevant for visibility.
- **OAT** is the oldest currently active transaction.
- **OST** is the oldest snapshot in use. It is derived from active transactions’ snapshots and
  is used to regulate garbage collection.

Marker maintenance occurs during transaction start and during sweep. See
`firebird/src/jrd/tra.cpp` (transaction startup and sweep sections).

---

## 3. Isolation Levels and Snapshots

### 3.1 Snapshot (Concurrency) Transactions
- A snapshot transaction sees the database as of its start.
- Firebird historically used a private TIP copy; modern Firebird uses commit-order snapshot logic
  backed by TIP cache and commit numbers (CN) as described in
  `firebird/doc/README.read_consistency.md`.

### 3.2 Read Committed Transactions
Firebird supports three read committed modes:
- **READ COMMITTED READ CONSISTENCY**
- **READ COMMITTED RECORD VERSION**
- **READ COMMITTED NO RECORD VERSION**

Read committed *read consistency* uses a statement-level snapshot (CN-based) to avoid
inconsistent reads within a statement. This is the modern Firebird default and should be the
ScratchBird default.

Behavioral implications:
- For READ CONSISTENCY, conflicts trigger a statement restart algorithm
  (see `README.read_consistency.md`).
- For RECORD VERSION, readers traverse back-versions and do not wait.
- For NO RECORD VERSION, readers may wait on active writers.

### 3.3 Precommitted Read-Only
- Firebird treats read-only read-committed transactions as precommitted.
- This reduces GC inhibition but has nuanced interaction with OST.

---

## 4. Record Versioning and Visibility

### 4.1 Version Chain
- Updates create a new primary version and a back-version chain.
- The index entry typically points to the newest primary version.
- Back-versions are linked in a chain by record headers.

### 4.2 Visibility Rule
A record version is visible to a transaction if the creating transaction:
- is committed, and
- committed before the transaction’s snapshot (CN or TIP-based snapshot view).

If the creating transaction is active, dead, or in limbo, the version is not visible.

Implementation source: `firebird/src/jrd/vio.cpp` and
`firebird/doc/README.read_consistency.md`.

---

## 5. Garbage Collection Model

Firebird garbage collection is multi-layered:
1. **Cooperative GC** during normal reads and updates
2. **Background GC** via a dedicated garbage collector thread
3. **Sweep** (database-wide pass)

### 5.1 GC Rule (Core)
If the **oldest active snapshot** can see a record version, then all older versions of that
record are garbage and can be removed. This is the Firebird MGA rule derived in
`README.read_consistency.md`.

### 5.2 Cooperative GC
- Readers and writers opportunistically prune version chains they encounter.
- This reduces pressure on background GC and sweep.
- Implemented in `firebird/src/jrd/vio.cpp` via `VIO_garbage_collect` and
  version chain processing.

### 5.3 Background GC Thread
- A dedicated GC thread processes candidate pages (per-relation GC bitmaps).
- Attachments notify GC when they encounter garbageable pages.
- GC uses special attachment flags and window flags to mark pages for GC.
- Implemented in `firebird/src/jrd/vio.cpp` (GC thread, notify mechanisms).

### 5.4 Sweep
Sweep is a database-wide scan that:
- advances OIT when possible
- cleans remaining obsolete versions that cooperative/background GC did not reclaim

Sweep trigger (Firebird semantics):
- automatic sweep runs when `(oldest_active_snapshot - OIT) > sweep_interval`
  and the oldest candidate is not limbo
- see `firebird/src/jrd/tra.cpp` for exact logic

Sweep behavior:
- uses a read-committed transaction internally
- saves transaction’s oldest snapshot to compute safe OIT advance
- scans TIP to determine new OIT and update header
- can run in background thread

---

## 6. Transaction Start and OIT/OAT/OST Maintenance

At transaction start, Firebird performs:
- snapshot setup (TIP/CN based)
- calculation of attachment-local oldest active/snapshot
- write of transaction lock data (LCK_tra) for GC regulation
- recomputation of OIT/OAT/OST candidates

Important rule:
- Transaction lock data stores the oldest active snapshot for GC queries. This is queried
  via `LCK_query_data` in `tra.cpp` to compute `tra_oldest_active`.

This interaction between transaction locks and GC is a key MGA mechanism and must be preserved.

---

## 7. Recovery Behavior (MGA)

Firebird MGA recovery is built-in:
- Each update writes the new version and links to the prior version.
- If a transaction crashes without commit, the previous visible version remains intact.
- No WAL replay is required for visibility recovery.
- GC later removes dead versions.

This is the core rationale for “fast recovery” in MGA systems.

---

## 8. Implementation Requirements for ScratchBird

The following must be true in ScratchBird:
- TIP-based transaction state is authoritative.
- OIT/OAT/OST semantics match Firebird.
- Snapshot and read-committed behavior match Firebird modes.
- GC rule is based on oldest active snapshot, not xmin-style horizon.
- Sweep trigger and OIT advancement follow Firebird logic.
- Cooperative GC and background GC are both supported.
- Transaction lock data (`LCK_tra`) is used to regulate GC horizon.

---

## 9. Implementation Pointers (FirebirdSQL)

- Transaction lifecycle: `firebird/src/jrd/tra.cpp`
- TIP operations: `firebird/src/jrd/tra.cpp`
- Record visibility + GC: `firebird/src/jrd/vio.cpp`
- Read consistency: `firebird/doc/README.read_consistency.md`
- Locking integration for GC: `firebird/src/jrd/lck.cpp`

---

## 10. Firebird-Accurate Transaction Lifecycle (Detailed)

This section paraphrases Firebird’s transaction lifecycle and ties it to the
required ScratchBird behavior. See `firebird/src/jrd/tra.cpp` for the exact
sequence.

### 10.1 Start Transaction (TRA_start / transaction_start)

Core steps in Firebird:
1. Validate database state (shutdown flags, attachment permissions).
2. Allocate transaction context and pool; initialize flags and timeouts.
3. Acquire the **transaction lock** (`LCK_tra`) and set its data to the
   **oldest active snapshot** for this transaction (read committed uses its
   own transaction number to avoid GC inhibition).
4. Read/update header page markers if necessary: `hdr_oldest_transaction` (OIT),
   `hdr_oldest_active` (OAT), `hdr_oldest_snapshot` (OST).
5. Capture snapshot state (TIP or commit-order snapshot) based on isolation.
6. Compute **attachment-local** oldest active/snapshot for GTT and local GC.
7. Compute global oldest active snapshot via `LCK_query_data(LCK_tra, MIN)`.
8. Update database-level OIT/OAT/OST cached values (in-memory) and TIP cache.
9. If sweep interval threshold is exceeded, start a sweeper thread.
10. Create a transaction savepoint (unless system/no-auto-undo).

ScratchBird must preserve the above ordering constraints, especially the
transaction lock data update before GC horizon computation.

### 10.2 Commit Transaction (TRA_commit)

Firebird commit behavior:
- Commit updates TIP state to committed.
- Releases transaction locks, savepoints, and attachment state.
- Read-only read-committed may be precommitted (minimal TIP writes).

ScratchBird must ensure commit updates are durable and visible to TIP readers
before any GC can reclaim versions.

### 10.3 Rollback Transaction (TRA_rollback)

Firebird rollback behavior:
- Marks TIP state as dead (aborted).
- Backversions created by the aborted transaction are left in place and later
  reclaimed by GC/sweep.

ScratchBird must avoid immediate physical cleanup during rollback beyond
transaction-local undo, leaving GC to reclaim physical versions.

### 10.4 Limbo (Two-Phase Commit)

Firebird uses TIP limbo state for prepared transactions. GC and sweep must
preserve limbo versions until they resolve. Sweep must detect oldest limbo
transaction and avoid advancing OIT past limbo.

---

## 11. Firebird-Accurate Sweep Sequence (Detailed)

Firebird sweep (`TRA_sweep`) flow (paraphrased from `tra.cpp`):
1. Mark sweep in progress and acquire sweep lock.
2. Start a sweep transaction (read-committed, special tpb).
3. Save sweep transaction’s `tra_oldest_active` before it changes.
4. Disable async GC notification for the sweep attachment.
5. Run `VIO_sweep` to ensure dead versions are removed.
6. Scan TIP cache to find the oldest limbo transaction in range.
7. Flush page buffers **before** advancing OIT.
8. Update header page `hdr_oldest_transaction` to the minimum of
   `transaction_oldest_active` and oldest limbo (if present).
9. Commit the sweep transaction and clear sweep flags.

ScratchBird must preserve the ordering (especially flush-before-OIT-advance),
and must not advance OIT beyond the oldest limbo transaction.

---

## 12. Firebird-Accurate GC Integration (Detailed)

Key Firebird GC integration points:
- Transaction locks (`LCK_tra`) carry the oldest active snapshot as lock data.
- GC horizon is the minimum `LCK_tra` data (not xmin style).
- Read-committed transactions may set lock data to their own transaction ID to
  avoid unnecessarily blocking GC.
- Attachment-local oldest snapshot is tracked to manage temporary table GC.

ScratchBird must implement this exact coupling between lock manager and GC.

---

## 13. Read Consistency (Commit-Order Snapshot) Details

Firebird’s modern read consistency model uses commit order (Commit Number, CN)
to define snapshots without copying TIP. Core concepts from
`firebird/doc/README.read_consistency.md`:
1. A per-database commit counter (CN) increments on commit.
2. Each committed transaction is assigned its CN.
3. A snapshot is defined by the current CN value (snapshot CN).
4. A version is visible if its creator is committed and its CN <= snapshot CN.
5. Active/limbo/dead transactions are never visible.

For READ COMMITTED READ CONSISTENCY:
- Each top-level statement uses a statement-level snapshot CN.
- Nested statements reuse the same snapshot.
- Update conflicts trigger a statement restart algorithm:
  1. Switch temporarily to NO RECORD VERSION mode.
  2. Acquire write locks on conflicting rows and remaining target rows.
  3. Undo statement effects while keeping write locks.
  4. Create a new statement-level snapshot and restart.
  5. Abort after a bounded number of restarts (Firebird uses 10).

ScratchBird must implement the above behavior and avoid “moving snapshot”
reads within a single statement in read consistency mode.

---

## 14. Transaction Start Algorithm (Expanded Step-by-Step)

Full Firebird order (paraphrased and made explicit):
1. Precheck shutdown flags and attachment permissions.
2. Allocate transaction context and pool.
3. Acquire temporary relation locks if required.
4. Assign transaction ID from TIP.
5. Enqueue `LCK_tra` and set lock data to oldest active snapshot (or own ID for
   read-committed non-read-consistency transactions).
6. Snapshot setup (TIP/CN depending on isolation).
7. Compute attachment-local oldest active and oldest snapshot.
8. Compute global oldest active snapshot via lock data min.
9. Update in-memory OIT/OAT/OST and TIP cache.
10. Trigger sweep if sweep interval exceeded.
11. Start transaction savepoint (unless system/no-auto-undo).
12. Emit trace hooks.

---

## 15. Visibility and State Algorithms (Explicit)

### 15.1 Transaction State Lookup (TIP)
Algorithm to fetch a transaction state from TIP:
1. Compute TIP page number = `xid / trans_per_tip`.
2. Compute offset = `xid % trans_per_tip`.
3. Locate byte and 2-bit slot.
4. Return 2-bit state (active/committed/dead/limbo).

### 15.2 Visibility Decision
Given record version created by transaction `T` and snapshot `S`:
1. If `T` is active/dead/limbo -> not visible.
2. If `T` committed and `commit_number(T) <= snapshot_number(S)` -> visible.
3. Else not visible.

---

## 16. TIP Cache (Commit-Order Snapshot) Algorithms

These details are derived from `firebird/src/jrd/tpc.cpp`, `tpc_proto.h`, and
`README.read_consistency.md`. ScratchBird must implement equivalent behavior.

### 16.1 TIP Cache Global Header and Block Allocation

Firebird stores commit numbers and transaction status in a shared memory cache:
1. A global shared header holds `latest_commit_number`, `latest_transaction_id`,
   `latest_attachment_id`, and `oldest_transaction`.
2. The cache is divided into fixed-size blocks (TipCacheBlockSize), each block
   covering a contiguous range of transaction IDs.
3. Block size is configured via `TipCacheBlockSize` (default per Firebird).
4. On initialization, header initializes block size and maps existing inventory
   pages. On first use, it loads inventory pages into the cache.

ScratchBird must keep block sizing configurable and map/unmap blocks based on
OIT movement (see §16.4).

### 16.2 Commit Number (CN) Assignment and State

Firebird associates a commit number (CN) to each committed transaction:
1. `CN_ACTIVE = 0`, `CN_PREHISTORIC = 1`, `CN_DEAD = MAX-2`, `CN_LIMBO = MAX-1`.
2. On commit, `TipCache::setState` assigns a new CN by incrementing
   `latest_commit_number` in the global header.
3. On rollback, the state becomes `CN_DEAD` (no new CN).
4. On limbo (prepared), state becomes `CN_LIMBO`.

ScratchBird must preserve these CN state values and transitions.

### 16.3 Snapshot State Lookup

`snapshotState(number)` returns a CN for a given transaction:
1. If transaction is older than OIT, return `CN_PREHISTORIC`.
2. If cache has committed CN value, return it.
3. If cache says active/limbo, fall back to TIP/TPC and possibly update.
4. If transaction is found dead, return `CN_DEAD` and update TIP cache.

ScratchBird must preserve the distinction between “active/limbo” vs “committed”
in snapshot evaluation.

### 16.4 TIP Cache Block Release

When OIT advances, old cache blocks can be released:
1. Compute `lastInterestingBlock = oldest_new / transactions_per_block`.
2. If OIT crosses a block boundary, scan backwards to find existing blocks.
3. Unmap shared memory blocks safely and delete backing files.
4. Use a per-block lock (`LCK_tpc_block`) to serialize cleanup.

ScratchBird must implement safe multi-process block cleanup with locking.

---

## 17. Snapshot Slot Allocation and Active Snapshots

### 17.1 Snapshot Slot Allocation

Snapshot slots are managed in a shared snapshot list:
1. Find a free slot by scanning from `min_free_slot` to `slots_used`.
2. If no free slot exists, extend (remap) the shared snapshot list.
3. Store snapshot CN and then set `attachment_id` to claim the slot.
4. Update `min_free_slot` watermark.

This ordering (snapshot first, attachment_id last) prevents readers from seeing
partially initialized slots.

### 17.2 Snapshot Slot Deallocation

Deallocation rules:
1. Clear snapshot and attachment_id.
2. Reduce `slots_used` only if freeing the highest-indexed slot.
3. Update `min_free_slot` to the freed slot if lower than current value.

This keeps allocation efficient without scanning from slot 0 every time.

### 17.3 Active Snapshot Tracking

Firebird maintains a per-attachment ActiveSnapshots list:
1. The “slow path” initializes by scanning all slots and building a bitmap of
   active snapshot CN values.
2. The “fast path” updates based on `snapshot_release_count` and `slots_used`.
3. Stalled lists are avoided by always updating `m_lastCommit` even when no
   snapshots were added/removed.

ScratchBird must ensure active snapshots are refreshed often enough to avoid
GC blocking due to stale oldest snapshot values.

---

## 18. TIP Cache Block Locks and AST Cleanup

Firebird protects TPC blocks with per-block existence locks:
1. Each TIP cache block is associated with a lock (`LCK_tpc_block`).
2. When a block becomes obsolete (OIT moved past), cleanup takes EX lock on the
   block before unlinking the backing shared memory file.
3. A blocking AST for the block lock (`tpc_block_blocking_ast`) can release
   mapped memory for old blocks when another process requests exclusive access.

ScratchBird must implement per-block locking and AST-based release to prevent
use-after-free across processes.

---

## 19. Transaction Wait Algorithm (Stable State Wait)

Firebird waits for another transaction to become stable (non-active):
1. Wait on the target transaction lock and TIP state transitions.
2. If the transaction remains active, the waiter may sleep or rescan depending
   on wait mode and timeout.
3. For read consistency mode, statement restart logic is preferred to waiting.

ScratchBird must preserve the principle: waiting is only for no-record-version
semantics, while record-version and read-consistency avoid waiting when possible.

---

## Appendix A. TIP Cache Block Size, File Naming, and Remap Policy

This appendix captures Firebird’s precise policies from `firebird/src/jrd/tpc.cpp`.

### A.1 Block Size Policy
1. TIP cache block size is controlled by `TipCacheBlockSize` (configurable).
2. The global TPC header stores `tpc_block_size` as the authoritative block size.
3. `initTransactionsPerBlock()` derives the transaction count per block based on
   the configured block size.

ScratchBird must keep block size configurable and stored in the global cache header.

Firebird default:
- `TipCacheBlockSize = 4194304` bytes (4 MiB) in `firebird/src/common/config/config.h`.

### A.2 File Naming Policy
1. Each block is stored as a shared memory file named from the database unique ID
   plus a block index (`TPC_BLOCK_FILE` format).
2. If `fullPath` is requested, the name is passed through the lock directory
   prefix logic (Firebird uses `iscPrefixLock`).

ScratchBird must use stable, unique naming per database and block index.

Firebird literal format string:
- `TPC_BLOCK_FILE = "fb_tpc_%s_%" UQUADFORMAT` in `firebird/src/common/file_params.h`.

### A.3 Shared Memory Remap Policy (Snapshots)
1. Snapshot list is a shared memory block containing `SnapshotData` array.
2. Allocation scans for free slots; if full, it remaps the block to a larger size.
3. Remap doubles the mapped size when expanding (`sh_mem_length_mapped * 2`).
4. After remap, `slots_allocated` is recalculated based on new mapped size.
5. Remap is done under a shared memory mutex; if `sync` is requested, it locks
   the mutex before remapping to avoid races.

ScratchBird must implement equivalent remap semantics for snapshot storage.

### A.4 Shared Memory Remap Policy (TPC Blocks)
1. TIP cache blocks are allocated on demand per block index.
2. Allocation is guarded by a double-checked locking pattern:
   - shared lookup first; if missing, upgrade to exclusive and re-check.
3. Old blocks are released only when OIT crosses their range.
4. Cleanup uses a per-block lock (`LCK_tpc_block`) and deletes the backing file
   only after acquiring EX lock, to serialize cross-process cleanup.

ScratchBird must replicate the double-checked allocation pattern and per-block
cleanup locking to avoid races or premature file deletion.

**Terminology note:** ScratchBird uses Firebird MGA. Any MGA references in this file are legacy shorthand and must be interpreted as MGA per the authoritative references above.

## ScratchBird Extensions Impact (Transactions, GC, Locking)

This section overlays ScratchBird’s extensions onto the Firebird MGA baseline. These changes
are mandatory and must be reflected in implementation.

### 1. UUID-Based Object and Tuple Identifiers

ScratchBird uses UUIDs as first-class identifiers for objects and tuple versions:
- **Record identity**: record headers store creator transaction and a UUID-based back-version
  pointer. There is no PostgreSQL-style `xmin/xmax` tuple header.
- **Index entries**: index keys reference UUID-based record identifiers; index entries remain
  stable across versions unless indexed columns change. This preserves Firebird’s “index change
  only on indexed-column change” rule.
- **Visibility checks**: visibility is driven by TIP/CN and record header fields (e.g.,
  `rhd_transaction`, `rhd_back_version`). UUIDs replace page/slot addresses as logical record IDs.
- **GC traversal**: garbage collection walks UUID-linked version chains. The GC horizon still
  comes from `LCK_tra` min data; UUIDs do not change the horizon calculation, only the pointer
  format.
- **Locks**: lock keys use UUIDs for object identity (relations, indexes, and record-level GC
  lock scopes). The lock manager still uses Firebird compatibility semantics.

### 2. Large and Variable Block Sizes

ScratchBird supports extended page sizes (8K–128K). Impacts:
- **TIP layout**: transactions-per-TIP page and TIP cache block sizing must be computed based
  on actual page size; larger pages reduce TIP page count but increase update granularity.
- **GC scan stride**: sweep and background GC must use page-size-aware iteration and candidate
  page identification.
- **Version chain density**: larger pages can hold longer version chains; cooperative GC and
  sweep should include back-version depth caps or throttling to avoid long per-page stalls.
- **Locking granularity**: page locks or buffer locks remain at physical page granularity; larger
  pages increase the contention domain and should be reflected in lock acquisition strategy.

### 3. File Spaces and Tablespaces

ScratchBird’s tablespaces and file spaces affect transaction visibility and GC:
- **Record identity** remains UUID-based and independent of file space.
- **Version chains** may span file spaces if configured; GC must traverse chains across spaces.
- **Sweep and GC** must iterate all file spaces and maintain consistent OIT/OAT/OST behavior
  across them.
- **Locking**: locks should be keyed by object UUIDs, not by file-space-specific page IDs, to
  avoid cross-space ambiguity. Physical page locks still exist but are subordinate to logical
  object identity.

### 4. Distributed Shard Repositories (Required)

ScratchBird’s shard repositories extend MGA across a cluster:
- **Transaction identity**: transaction IDs remain globally unique (64-bit + node UUID or
  shard prefix). UUID-based object IDs are globally unique across shards.
- **OIT/OAT/OST tracking**: each shard maintains local OIT/OAT/OST; global coordination must
  compute a safe cluster-wide GC horizon (e.g., min of shard OIT/OST or a negotiated watermark).
- **GC and sweep**: GC is performed per-shard with optional cross-shard coordination for
  shared objects. Sweep must not advance OIT past any unresolved limbo transactions in the shard.
- **Locking**: lock ownership remains per-shard for physical resources; logical locks on shared
  objects require a shard-aware lock namespace or a control-plane arbitration layer.
