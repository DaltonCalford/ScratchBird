# Phase 3 — Transactions and MGA: detailed implementation plan

Status: draft

## Goals (spec → plan)
- Introduce Transaction Manager (TXN ids, begin/commit/rollback) and seed TIP usage.
- Implement Multi-Generational Architecture (MGA) semantics on heap tuples:
  - created_xid, deleted_xid, backptr_rid enforcement
  - Snapshot visibility rules (read committed; prepare Snapshot Isolation scaffolding)
  - Update as version-chain append; delete with tombstone semantics
- Conflict detection for writers; basic error paths (e.g., update/delete on invisible rows).
- Basic concurrency model (single-process; coarse latching already present); correctness-focused.
- Exit: visibility-correct reads/writes with interleavings; TIP reflects states; tests for commit/rollback; no corruption.

## Non-goals
- Full WAL/recovery of transactional state (later recovery phases).
- Global vacuum/garbage collection policy (Phase 15); only minimal space hygiene.
- Serializable isolation or predicate locks.
- Distributed transactions; cross-space ACID.

---

## ODS updates and semantics

1) Tuple header fields (activate semantics)
- created_xid: set on insert/update to the creating TXN id
- deleted_xid: set on delete or when updated (old version) by the deleter TXN; 0 means not deleted
- backptr_rid: for update chains, new version points back to previous version row RID; 0 for inserts

2) TIP (Transaction Inventory Page) usage (Phase 3)
- Per-TXN 1-byte state: 0=idle/unused, 1=active, 2=committed, 3=aborted (values reserved; exact constants in code)
- TIP page 2 is initial; additional TIP pages may be reserved but Phase 3 can assume low TXN counts for tests
- Transaction ids are 64-bit, monotonic; TIP mapping: txn_id % transPerTIP(page_size) to byte slot on current TIP page (additional TIPs to be added later)

3) Row visibility rules
- Given snapshot={own_xid, committed_before, active_set}, row version V is visible if:
  - V.created_xid is committed and committed_before V (or equal) and
  - V.deleted_xid is 0 or V.deleted_xid is active/aborted or committed after our snapshot boundary; and
  - For own_xid reads (read-your-writes): V.created_xid == own_xid visible even if not committed; and versions deleted by own_xid are invisible
- Phase 3 mode: start with Read Committed (RC): each statement obtains a fresh snapshot of committed txns; optional Snapshot Isolation (SI) scaffolding recorded in code but disabled by default.

---

## Transaction Manager (API and behavior)

Interfaces
- TransactionManager additions:
  - Transaction begin() → returns a Transaction handle {id, state}; writes TIP state=active
  - commit(Transaction&) → set TIP state=committed
  - rollback(Transaction&) → set TIP state=aborted
  - Snapshot snapshot_read_committed() → captures a view: cutoff_committed_id = last_committed_id
- ID generation: 64-bit increasing; store last issued id in memory (Phase 3; persistence later)

State transitions
- begin: TIP[tid]=active
- commit: TIP[tid]=committed
- rollback: TIP[tid]=aborted
- TIP checksum updated on write; order operations to avoid torn state writes

---

## Visibility and executor integration

Helpers
- TxnState read_txn_state(xid) → consult TIP byte; 0=idle,1=active,2=committed,3=aborted
- bool is_visible(snapshot, tuple_header) implementing RC rules

Heap changes
- Insert: set created_xid = txn.id on new version; deleted_xid = 0; backptr_rid = 0
- Update: read visible version V; create new version V' with created_xid = txn.id, backptr_rid = RID(V); set V.deleted_xid = txn.id
- Delete: set V.deleted_xid = txn.id
- Fetch: follow version chain to most recent visible version: start from slot head, ensure visible, else traverse backptr_rid
- Scan: for each slot, decode head version and traverse chain to first visible (bounded)

Conflict detection (Phase 3 scope)
- Writers check target head is visible to them at RC; if not visible (e.g., already updated by a committed concurrent txn), return error (concurrent update conflict)
- Aborted tuples: versions with created_xid aborted are invisible; versions deleted by aborted txn remain visible

---

## Isolation levels (Phase 3 scope)
- Default: Read Committed (fresh snapshot per statement)
- Scaffolding:
  - Snapshot struct includes future fields: active_txn_ids, xmin/xmax, for SI later
  - Executor still calls is_visible(snapshot, th)

---

## Tooling
- Optional: tx_inspect CLI for dumping TIP states, last txn id, and basic stats
- Extend page_dump TIP decoder (print first 64 states)

---

## Test plan

Unit tests
- TIP states: begin/commit/rollback set bytes as expected; read_txn_state returns consistent values
- Visibility function for combinations of created/deleted and committed/aborted/active txns

Integration tests (single-threaded interleavings)
- Insert + commit; reader sees row
- Insert + rollback; reader does not see row
- Update chain: T1 inserts and commits; T2 updates then commits → reader sees new version; backptr chain intact
- Update conflict: T1 updates (active), T2 attempts update on same head → conflict/error (simulate by ordering)
- Delete + commit hides row; Delete + rollback preserves visibility
- Scan returns only visible versions; overflow rows unaffected by visibility logic

Soak/robustness
- Random sequence: begin/insert/update/delete/commit/rollback for N=10k ops; at the end, perform a consistent scan and verify application-level checksum against a shadow model

---

## Milestones & exit criteria

M1: Transaction primitives & TIP
- TXN id generator, TIP byte states, begin/commit/rollback; unit tests pass

M2: Visibility and DML semantics
- Insert/update/delete set tuple headers; visibility rules enforced in fetch/scan; basic conflict detection

M3: Executor hooks and chain traversal
- Heap fetch/scan traverse backptr_rid to find visible version; tests for update/delete chains

M4: Soak and tooling
- Randomized workload passes invariants; TIP/page_dump decoders; optional tx_inspect

Exit (Phase 3 complete)
- Correct visibility under RC; TIP reflects states; update/delete produce valid chains; tests green; no checksum mismatches.

---

## Work breakdown (sequenced)

1) TIP read/write API
- Add TxnState enum and helpers; implement TransactionManager::begin/commit/rollback/read_txn_state
- Store last_txn_id in memory; TIP page write with checksum

2) Snapshot and visibility helper
- Define Snapshot for RC; implement is_visible

3) Heap DML wiring
- Thread Transaction handle through HeapRelation::insert/update/remove signatures
- Set tuple headers; implement update chaining and delete semantics
- Conflict detection against current head version

4) Fetch/Scan chaining
- Follow backptr_rid boundedly until visible or chain end; tests for chain heads/tails

5) Tests
- Unit: TIP states, visibility matrix
- Integration: insert/commit/rollback, update/delete sequences, scan correctness
- Soak: randomized sequence vs. shadow model checksum

6) Tooling
- Extend page_dump to emit TIP bytes; add tx_inspect (optional)

---

## Risks & mitigations
- Chain traversal cost: keep Phase 3 chains short in tests; later GC/vacuum will prune
- TIP growth: constrain tests to fit single TIP; future phases add multi-TIP allocation
- Concurrency gaps: single-thread interleavings only; future latching/WAL phases harden correctness
- Writer conflicts: conservative conflict detection to avoid anomalies under RC
