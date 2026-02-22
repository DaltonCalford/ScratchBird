# ScratchBird Gap Audit Report (Implementation vs Specifications)

**Date:** 2026-02-07
**Scope:** Implementation alignment with consolidated architecture and referenced specifications.

---

## 1. Process Architecture (Listener/Parser/Engine)

**Spec expectation**
- Listener is a traffic cop: accepts sockets, manages pool, hands off to parser.
- Parser performs TLS and protocol handshake, then IPC to engine.
- Engine is protocol-agnostic and owns session state.

**Implementation**
- Listener hands off socket and passes TLS flag/config to parser.
- Parser loads TLS config and constructs TLS context.

**Gaps / Concerns**
- No direct gap found in process separation.
- Ensure no listener-side TLS or protocol handshake logic sneaks in later.

**Evidence**
- `src/network/sb_listener_main.cpp`
- `src/parser/sb_parser_main.cpp`

---

## 2. TLS Handling

**Spec expectation**
- TLS handshake is performed in the parser after socket handoff.

**Implementation**
- Listener parses TLS config and forwards `tls_active` to parser.
- Parser loads TLS config and builds TLS context.

**Gaps / Concerns**
- None found in current entry points. Verify no TLS handshake in listener path.

**Evidence**
- `src/network/sb_listener_main.cpp`
- `src/parser/sb_parser_main.cpp`

---

## 3. Multi-Dialect Parser Support

**Spec expectation**
- Parser is translation/API layer; multiple parsers can provide 1:1 emulation.

**Implementation**
- Separate dialect parser trees exist (MySQL, PostgreSQL, Firebird).

**Gaps / Concerns**
- No explicit gap found in structure, but end-to-end mapping to SBLR is not fully validated here.

**Evidence**
- `src/parser/mysql/*`, `src/parser/postgresql/*`, `src/parser/firebird/*`

---

## 4. Engine Session Ownership

**Spec expectation**
- Engine owns all session state; parsers/clients are untrusted.

**Implementation**
- Session state managed in `ServerSession` and `ConnectionContext`, stored server-side.

**Gaps / Concerns**
- Dormant reattach persists connection context in memory; good for ownership, but protocol path for secure reattach is not implemented.

**Evidence**
- `src/server/server_session.cpp`
- `src/core/connection_context.cpp`
- `src/core/database.cpp`

---

## 5. Disconnect and Recovery Semantics

**Spec expectation**
- Network loss → dormant session.
- Parser crash/kill → rollback default, no reconnection.
- Explicit DISCONNECT → normal close (no dormant preservation unless configured).

**Implementation**
- Both network failure and explicit DISCONNECT call `detachToDormant()`.

**Gaps / Concerns (HIGH)**
- Explicit DISCONNECT does not close/rollback; it preserves dormant state.
- Crash vs disconnect not distinguished in server session logic.
- Spec requires no reconnection after parser crash/kill, but code has no explicit crash path to enforce rollback only.

**Evidence**
- `src/server/server_session.cpp:481-492` (network failure → dormant)
- `src/server/server_session.cpp:755-773` (DISCONNECT → dormant)

---

## 6. Reattach / Reconnect Protocol

**Spec expectation**
- Reconnect requires same user/password/IP + session or transaction ID.
- Resume must validate session state and acknowledge transfer before resuming.

**Implementation**
- Dormant records exist; reattach exists in `Database::reattachDormant`.
- No visible protocol message or server/session path to execute reattach.

**Gaps / Concerns (HIGH)**
- No client reattach path; dormant sessions are not externally recoverable.
- No security checks in reattach; required credentials not enforced.

**Evidence**
- `src/core/database.cpp:853-889` (reattachDormant)
- No reattach references in `src/protocol/*`

---

## 7. MGA Visibility (No Snapshots)

**Spec expectation**
- TIP-based visibility only; no snapshot structures.

**Implementation**
- `TransactionManager::isVersionVisible` uses TIP; explicit MGA comments.

**Gaps / Concerns**
- Naming uses “snapshot” for statement-level XID in executor; can cause confusion but may be semantically correct.

**Evidence**
- `src/core/transaction_manager.cpp:1432-1480`
- `src/sblr/executor.cpp:1919-2014` (statement “snapshot”)

---

## 8. OIT/OAT/OST + Sweep/GC Horizon

**Spec expectation**
- Sweep trigger `(OST - OIT) > sweep_interval`.
- GC should be driven by MGA markers, not PG-style backend xmin.

**Implementation**
- GC horizon derived from `ProcArrayManager::getGcHorizon` using backend xmin/xid.

**Gaps / Concerns (HIGH)**
- GC horizon uses PG-style xmin semantics, not MGA OIT/OAT/OST markers.
- Potentially incorrect cleanup decisions for long-running read transactions.

**Evidence**
- `src/core/gc_manager.cpp:33-78`
- `src/core/proc_array.cpp:542-593`

---

## 9. MGA Recovery (No WAL Replay)

**Spec expectation**
- Direct-write MGA; no WAL replay/repair pass.
- Recovery is normal startup; older versions remain valid.

**Implementation**
- Comments indicate “no WAL” in LSM tree and “MGA-style recovery” in database startup.

**Gaps / Concerns**
- Need to confirm startup path uses MGA-only recovery and does not assume WAL replay.

**Evidence**
- `src/core/lsm_tree.cpp:16-33`
- `src/core/database.cpp:1354-1356`

---

## 10. Locking Model and Thread Safety

**Spec expectation**
- THREAD_SAFETY levels per component; lock ordering enforced.

**Implementation**
- Not audited in detail beyond ProcArray and GC.

**Gaps / Concerns**
- Requires a focused pass to verify each core structure declares thread-safety level and respects lock order.

**References**
- `docs/specifications/core/THREAD_SAFETY.md`

---

## 11. Result/Query Caching (Cross-Parser)

**Spec expectation**
- Query/result caching handled in engine for cross-parser reuse.

**Implementation**
- Executor uses `QueryResultCache` in engine.

**Gaps / Concerns**
- Verify cache invalidation is tied to MGA visibility rules.

**Evidence**
- `src/sblr/executor.cpp` (QueryResultCache usage)

---

## 12. Registry Format / Ownership

**Spec expectation**
- Registry format not ratified (alpha), expected to be small and user-editable.

**Implementation**
- No explicit registry storage module located in `src` (only internal "server registry" for catalog).

**Gaps / Concerns**
- Lack of registry storage implementation is expected but should be tracked explicitly.

**Evidence**
- `src/core/catalog_manager.cpp` (internal server_registry table; not external registry file)

---

## 13. IPC / Engine Contract

**Spec expectation**
- Parsers communicate with engine via IPC contract; SBLR is the engine interface.

**Implementation**
- IPC code exists, but full contract conformance not audited here.

**Gaps / Concerns**
- Requires a focused contract vs implementation pass.

**References**
- `docs/specifications/network/*`
- `docs/specifications/sblr/*`

---

## 14. Storage Engine Layout

**Spec expectation**
- On-disk format, page types, record format, TOAST, tablespace specs.

**Implementation**
- Storage code exists in `src/core` but not audited here.

**Gaps / Concerns**
- Needs a dedicated storage audit against `docs/specifications/storage/*`.

---

## 15. Index Maintenance

**Spec expectation**
- Stable TIDs, back-versioning, index updates only if indexed columns change.

**Implementation**
- B-tree/GiST/RTree mention MGA rules; uses xmin/xmax and logical deletion.

**Gaps / Concerns**
- Verify page split / rebalancing preserves xmin and stable TIDs per MGA rules.

**Evidence**
- `src/core/btree.cpp` (MGA comments and logic)
- `src/core/gist_index.cpp`
- `src/core/rtree.cpp`

---

## 16. Summary of High-Risk Gaps

1. **Disconnect semantics**: DISCONNECT and network failure both detach to dormant; parser crash vs network loss is not distinguished.
2. **No reattach protocol**: dormant reattach exists internally but has no external protocol path or security gate.
3. **GC horizon**: uses PG-style backend xmin; likely misaligned with MGA OIT/OAT/OST rules.

---

## 17. Recommended Next Audit Passes

1. Recovery + dormant reattach protocol + security checks.
2. GC horizon logic aligned to OIT/OAT/OST.
3. Storage engine audit vs `docs/specifications/storage/*`.
4. IPC/SBLR contract audit vs `docs/specifications/network/*` and `docs/specifications/sblr/*`.
5. Thread-safety audit vs `docs/specifications/core/THREAD_SAFETY.md`.

