# ScratchBird MGA (Firebird Model) - Implementation Specification

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)



## Purpose
This document defines the MGA implementation requirements for ScratchBird.
It is derived directly from FirebirdSQL’s transaction, visibility, and GC model.
Any implementation must match Firebird semantics.

Authoritative references:
- `firebird/src/jrd/tra.cpp`
- `firebird/src/jrd/vio.cpp`
- `firebird/doc/README.read_consistency.md`
- `firebird/src/jrd/lck.cpp`, `firebird/src/jrd/lck.h`

---

## 1. Core MGA Principles (Firebird)

- Updates create new record versions and keep back-versions linked on page.
- Readers select the visible version based on TIP state and snapshot rules.
- Index entries typically point to the newest primary version and only change
  if indexed columns change.
- Recovery is implicit: uncommitted versions remain invisible and are removed
  later by GC. No WAL replay is required for visibility recovery.

---

## 2. Transaction State and Visibility

- Transaction states are stored in TIP (2-bit per transaction).
- Visibility is determined by whether the creating transaction committed before
  the current snapshot.
- Snapshot and read-committed behavior follow Firebird’s read consistency rules
  (see `README.read_consistency.md`).

---

## 3. OIT/OAT/OST Markers

- OIT: Oldest Interesting Transaction.
- OAT: Oldest Active Transaction.
- OST: Oldest Snapshot Transaction.

These markers are maintained during transaction start and sweep. They regulate
GC and sweep timing. See `firebird/src/jrd/tra.cpp`.

---

## 4. Garbage Collection

### 4.1 Cooperative GC
- Record access paths may prune back-versions when safe.
- Implemented in Firebird via `VIO_garbage_collect` and version traversal logic.

### 4.2 Background GC Thread
- Dedicated GC thread processes candidate pages.
- Attachments notify GC of candidate pages via GC bitmaps.
- Implemented in `firebird/src/jrd/vio.cpp`.

### 4.3 Sweep
- Sweep is a database-wide pass that advances OIT and cleans leftover garbage.
- Auto sweep triggers when `(oldest_active_snapshot - OIT) > sweep_interval` and
  no limbo is involved.
- Sweep uses a read-committed transaction and may run in the background.

---

## 5. Transaction Lock Data and GC Horizon

- Each transaction holds a transaction lock (`LCK_tra`).
- Lock data is set to the transaction’s oldest active snapshot.
- GC horizon is computed by querying the minimum transaction lock data.

This is the core Firebird mechanism for GC regulation. See `tra.cpp` and `lck.cpp`.

---

## 6. ScratchBird Requirements

ScratchBird must implement the following Firebird-equivalent behaviors:
- TIP-based transaction state and visibility.
- Snapshot and read-committed semantics per Firebird.
- OIT/OAT/OST maintenance identical to Firebird logic.
- GC horizon based on transaction lock data, not xmin-style.
- Cooperative GC, background GC, and sweep.

---

## 7. Implementation Pointers

- Transaction lifecycle and markers: `firebird/src/jrd/tra.cpp`
- Record visibility + GC: `firebird/src/jrd/vio.cpp`
- Read consistency and CN snapshot logic: `firebird/doc/README.read_consistency.md`
- Lock manager integration: `firebird/src/jrd/lck.cpp`, `firebird/src/jrd/lck.h`

---

## 8. Record Versioning (Detailed Firebird Behavior)

### 8.1 Update/Erase Semantics
Firebird’s MGA uses in-place update for the primary record version and stores the
previous version as a back-version on a data page. The version chain is linked
newest-to-oldest. Index entries generally point to the newest version and are
only changed when indexed columns change.

### 8.2 Visibility Check
Visibility uses TIP state and snapshot rules:
- If creator is active/dead/limbo -> not visible.
- If creator committed before snapshot -> visible.
- If committed after snapshot -> not visible.

The snapshot may be TIP-based (traditional) or CN-based (commit-order) for read
consistency. See `firebird/doc/README.read_consistency.md`.

### 8.3 Record-Level Flags
Firebird uses record header flags to control GC behavior, including “GC active”
markers to avoid concurrent GC on the same version chain. ScratchBird must
provide equivalent concurrency control to prevent double-GC of a chain.

---

## 9. Cooperative GC (Detailed Firebird Behavior)

During normal read/write operations, Firebird attempts to prune garbage:
1. Traverse back-versions to find the visible one.
2. If older versions are no longer visible to the oldest active snapshot,
   mark them as garbage.
3. Remove garbage versions and update index/blob references as needed.
4. If GC cannot safely complete, mark the page for background GC and continue.

This logic is spread across record version traversal and `VIO_garbage_collect`
in `firebird/src/jrd/vio.cpp`.

---

## 10. Background GC Thread (Detailed Firebird Behavior)

Firebird background GC uses per-relation GC bitmaps:
- Attachments notify GC when a data page contains garbageable versions.
- The GC thread scans bitmaps, selects candidate pages, and attempts cleanup.
- GC runs under a special attachment context and uses WIN_garbage_collector
  window flags to coordinate page access.

Key flow (paraphrased from `vio.cpp`):
1. Wake on semaphore when GC pending flag is set.
2. Scan relation GC bitmap for candidate pages.
3. For each candidate page, attempt to garbage collect all record chains.
4. If successful, clear bitmap bit and optionally push page to LRU tail.
5. Continue until no candidates remain or GC thread is stopped.

ScratchBird must implement equivalent candidate tracking and background cleanup.

---

## 11. Sweep (Detailed Firebird Behavior)

Sweep is distinct from GC:
- Sweep forces a database-wide pass and advances OIT safely.
- Sweep explicitly disables async GC notifications for the sweep attachment,
  performs cleanup synchronously, flushes buffers, then updates header OIT.
- Sweep must not advance OIT past any limbo transaction.

See `firebird/src/jrd/tra.cpp` for the exact ordering.

---

## 12. Index and Blob GC

When record versions are removed, Firebird triggers:
- Index cleanup (`IDX_garbage_collect`)
- Blob cleanup (`BLB_garbage_collect`)

ScratchBird must ensure record GC updates secondary structures consistently.
See `firebird/src/jrd/vio.cpp` and `firebird/src/jrd/idx.cpp` / `blb` modules.

---

## 12.1 Relation GC Bitmap Data Structures (Firebird)

Firebird uses per-relation GC tracking backed by a sparse map + bitmap:
1. Each relation has a `RelationData` structure with a `PageTranMap` that maps
   `page_number -> min_transaction_id` (lowest txn that touched that page).
2. When a page is flagged for GC, `addPage(relID, pageno, tranid)` stores or
   updates the minimum tranid for that page.
3. On a sweep request, `swept(oldest_snapshot)` walks the map and for any entry
   whose tranid is older than `oldest_snapshot`, it emits the page into a
   `PageBitmap` and removes it from the map.
4. The GC thread calls `getPages(oldest_snapshot, relID)` to obtain a bitmap
   of candidate pages for a relation, then processes those pages.

This behavior is implemented in `firebird/src/jrd/GarbageCollector.h/.cpp` and
used by `notify_garbage_collector` in `vio.cpp`.

ScratchBird must implement the same page-to-min-tran tracking and bitmap emission
to ensure GC only touches safe pages.

---

## 12.2 GC Notification Rules (Firebird)

When a reader/writer discovers garbageable versions:
1. Compute data page sequence = `record_number / max_records_per_page`.
2. Call `addPage(relID, dp_sequence, tranid)`.
3. If GC is not active and the page’s min tranid is older than the oldest
   active snapshot, signal the GC semaphore.

Large sequential scans set a window flag so page release to LRU tail is deferred
until GC can process it. ScratchBird should preserve this behavior for large scans.

---

## 13. ScratchBird Conformance Checklist

Required Firebird-equivalent behaviors:
1. TIP-based state and snapshot evaluation.
2. Record chains with back-versions and in-place update semantics.
3. Cooperative GC during visibility traversal.
4. Background GC thread with candidate page tracking.
5. Sweep flow with flush-before-OIT-advance and limbo handling.
6. GC horizon computed via transaction lock data (LCK_tra).
7. Index/blob cleanup during GC.

## ScratchBird Extensions Impact (MGA/GC)

### 1. UUID-Based Record Identity

- Version chains are linked via UUIDs instead of page/slot addresses.
- GC must follow UUID back-version pointers and maintain index/blob cleanup
  based on logical record IDs.
- Record visibility uses TIP/CN state; UUIDs only affect pointer traversal.

### 2. Large and Variable Page Sizes

- GC candidate page selection and sweep iteration must be page-size aware.
- Version chain depth thresholds should be configurable to avoid long stalls on
  large pages.

### 3. File Spaces and Tablespaces

- GC and sweep must traverse all file spaces; OIT/OAT/OST remain global per
  database instance.
- Record and version identity is independent of file space, enabling cross-space
  chain traversal.

### 4. Distributed Shard Repositories

- GC is per-shard with a cluster-safe horizon (min watermark across shards).
- Sweep must respect shard-local limbo transactions.
- UUIDs ensure global uniqueness of record/object identities across shards.
