# Gap Audit Remediation Plan (2026-02-07)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Scope:** Address all issues identified in `docs/audit/SCRATCHBIRD_GAP_AUDIT_REPORT_2026-02-07.md`.

**Primary Specs:**
- `/docs/specifications/parser/v3/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md`
- `MGA_RULES.md`
- `/docs/specifications/parser/v3/sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
- `/docs/specifications/parser/v3/core/THREAD_SAFETY.md`
- `/docs/specifications/parser/v3/storage/*`
- `/docs/specifications/parser/v3/network/*`
- `/docs/specifications/parser/v3/sblr/*`

---

## Phase 0: Baseline & Audit Prep

1. **Freeze audit baseline**
   - Record current commit hash and build config.
   - Capture current config defaults (especially dormant settings).

2. **Create working checklist**
   - Copy the “High-Risk Gaps” and section list from the audit report into a tracking checklist.

---

## Phase 1: Disconnect / Dormant Semantics (High Risk)

**Goal:** Align behavior with design: network loss → dormant; parser crash/kill → rollback/no reattach; explicit DISCONNECT → normal close.

1. **Differentiate disconnect types**
   - Update `src/server/server_session.cpp` to explicitly separate:
     - Network failure (no DISCONNECT message): preserve dormant.
     - Explicit DISCONNECT: close session and rollback (or commit if autocommit) per spec.
     - Parser crash/kill: rollback and disallow dormant.
   - References:
     - Spec: `/docs/specifications/parser/v3/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md` sections 7–9, 16–19.
     - Code: `src/server/server_session.cpp:481-492`, `src/server/server_session.cpp:755-773`.

2. **Parser crash/kill path**
   - Identify where parser termination is surfaced to engine.
   - Add explicit signaling so server can treat this as hard rollback/no reattach.
   - References:
     - Spec: `/docs/specifications/parser/v3/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md` sections 7.2, 9.
     - Code: `src/network/*`, `src/parser/*`, `src/server/*`.

3. **Update dormant catalog records**
   - Ensure dormant entries record cause (network loss vs admin kill) and enforce reattach rules.
   - References:
     - Spec: consolidated recovery sections.
     - Code: `src/core/catalog_manager.cpp` dormant transaction storage.

---

## Phase 2: Reattach / Reconnect Protocol (High Risk)

**Goal:** Implement spec-compliant reconnection path with security gating.

1. **Define protocol messages**
   - Add IPC protocol request/response for “ReattachDormant” or “ReconnectSession”.
   - Include: session/transaction ID, user identity, auth token, IP, last-row UUID/hash.
   - References:
     - Spec: `/docs/specifications/parser/v3/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md` sections 8–9.
     - Protocol specs: `/docs/specifications/parser/v3/network/*`, `/docs/specifications/parser/v3/sblr/*`.

2. **Server-side validation**
   - Enforce same user/password + IP + session/transaction ID.
   - Reject if password changed or role revoked.
   - References:
     - Spec: consolidated recovery + security sections.
     - Code: `src/core/database.cpp:853-889` (reattachDormant), `src/server/server_session.cpp`.

3. **State transfer and resume**
   - Validate resume token (row UUIDs/hash) before resuming results.
   - Resume only after engine acknowledges state transfer.
   - References:
     - Spec: consolidated result delivery and resume sections.

---

## Phase 3: GC Horizon vs MGA Markers (High Risk)

**Goal:** Align GC with MGA OIT/OAT/OST rules, remove PG-style xmin horizon.

1. **Replace GC horizon source**
   - Update `GcManager::getGcHorizon()` to use OIT/OAT/OST markers from `TransactionManager`.
   - References:
     - Spec: `MGA_RULES.md` Rule 4; consolidated MGA summary section.
     - Code: `src/core/gc_manager.cpp`, `src/core/transaction_manager.cpp`.

2. **Update ProcArray usage**
   - Remove or de-emphasize backend xmin as a GC determinant.
   - Ensure ProcArray remains for monitoring/locking but not GC horizon.
   - References:
     - Spec: MGA rules.
     - Code: `src/core/proc_array.cpp`.

3. **Update sweep trigger**
   - Ensure sweep uses `(OST - OIT) > sweep_interval`.
   - References:
     - Spec: `MGA_RULES.md`, `/docs/specifications/parser/v3/sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md`.
     - Code: `src/core/sweep_manager.cpp`.

---

## Phase 4: Terminology and Snapshot Confusion (Medium)

**Goal:** Remove snapshot terminology that conflicts with “no snapshots” rule.

1. **Rename statement snapshot terminology**
   - Replace “statement snapshot” with “statement XID” or “statement context” in `sblr/executor.cpp`.
   - References:
     - Spec: consolidated MGA rules and “no snapshots”.
     - Code: `src/sblr/executor.cpp:1919-2014`.

2. **Documentation update**
   - Ensure code comments reference TIP-based visibility, not snapshot-based semantics.

---

## Phase 5: Storage Engine Audit (Detailed)

**Goal:** Confirm storage/record/page/index behavior matches MGA rules.

1. **On-disk format**
   - Compare `src/core/*` structures with:
     - `/docs/specifications/parser/v3/storage/ON_DISK_FORMAT.md`
     - `/docs/specifications/parser/v3/storage/STORAGE_ENGINE_PAGE_MANAGEMENT.md`

2. **Record versioning**
   - Confirm back-versioning and stable TIDs for row updates.
   - Verify no forward-versioning logic.

3. **Index maintenance**
   - Verify index updates only when indexed columns change.
   - Verify stable TIDs survive page splits and rebalancing.
   - References:
     - `/docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md`
     - `/docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md`
     - Code: `src/core/btree.cpp`, `src/core/gist_index.cpp`, `src/core/rtree.cpp`, `src/core/hnsw_index.cpp`.

---

## Phase 6: IPC / SBLR Contract Audit

**Goal:** Ensure engine IPC and SBLR are aligned with spec.

1. **IPC contract verification**
   - Compare `src/ipc/*` with `/docs/specifications/parser/v3/network/*` and `/docs/specifications/parser/v3/sblr/*`.

2. **SBLR opcode alignment**
   - Validate `sblr` executor and opcode registry vs:
     - `/docs/specifications/parser/v3/sblr/SBLR_OPCODE_REGISTRY.md`
     - `/docs/specifications/parser/v3/sblr/Appendix_A_SBLR_BYTECODE.md`

---

## Phase 7: Thread Safety & Locking Model

**Goal:** Verify components meet `THREAD_SAFETY.md` requirements.

1. **Component audit**
   - Tag/confirm thread safety level for:
     - Buffer pool
     - TIP access
     - Catalog cache
     - File I/O
   - References: `/docs/specifications/parser/v3/core/THREAD_SAFETY.md`.

2. **Lock ordering**
   - Validate lock acquisition order against spec.

---

## Phase 8: Regression Tests and Validation

1. **Reconnect and dormant behavior tests**
   - Simulate network drop, verify dormant + resume.
   - Simulate parser crash/kill, verify rollback/no reattach.

2. **GC / sweep correctness tests**
   - Validate sweep trigger and cleanup based on OIT/OAT/OST.

3. **Protocol validation tests**
   - Ensure reattach handshake blocks invalid credentials or revoked roles.

---

## Deliverables

- Code changes for phases 1–7
- Updated documentation in consolidated spec where behavior changes
- Test cases and results summary

