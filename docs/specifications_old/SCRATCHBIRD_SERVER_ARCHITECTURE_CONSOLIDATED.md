# ScratchBird Server Architecture, Lifecycle, and Recovery (Consolidated)


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**Version:** 1.0
**Status:** Authoritative (V3)
**Last Updated:** February 2026

---

## 1. Purpose

This document consolidates core ScratchBird server architecture, lifecycle, registry direction, security ownership, and connection recovery behavior into a single authoritative reference for the alpha phase. It replaces overlapping drafts and removes assumptions borrowed from other database systems.

---

## 1.1 AI Quick Summary (Authoritative)

- Parser-per-connection **processes**; listener is a traffic cop only.
- Parser performs **TLS and protocol handshakes** after socket handoff.
- Engine executes **SBLR only**, never raw SQL.
- Engine owns **all session state**; parsers/clients are untrusted.
- **Firebird MGA** only: TIP-based visibility, no snapshots.
- **Back-versioning** with stable TIDs; indexes change only if indexed columns change.
- **OIT/OAT/OST** markers drive sweep; trigger is `(OST - OIT) > sweep_interval`.
- Network loss → **dormant** session; work continues until first result boundary, then pauses.
- Parser crash/kill → **rollback by default**, **no reconnection**.
- Registry format is **not yet ratified**; expected to be small and user-editable for alpha.

---

## 2. Core Principles (Authoritative)

1. **Separated Parser Architecture**
   - Listeners accept network connections and hand off sockets to parsers.
   - Parsers are **separate processes** (per connection), not threads.
   - Parsers translate protocol SQL into **SBLR** and communicate with the engine via IPC.

2. **Engine Is Protocol-Agnostic**
   - The engine executes **SBLR**, never raw SQL.
   - PostgreSQL/MySQL/Firebird compatibility is implemented in parser processes only.

3. **Engine Owns Session State**
   - The engine is the final authority for security and session state.
   - Parsers and clients are untrusted.

4. **Cross-Parser Optimization Happens in the Engine**
   - Query/result caching is handled in the engine, not in parsers.
   - This allows cache reuse across all protocols and parser types.

5. **MGA (Multi-Generation Architecture)**
   - Readers never acquire locks.
   - Lock manager coordinates write-write conflicts only.

6. **Parser Isolation**
   - Each connection runs in a separate parser process.
   - A parser crash cannot corrupt engine memory.

---

## 3. Process and Thread Model

### 3.1 Processes

- **sb_server** (main engine process)
  - Owns all shared state (buffer pool, lock manager, transaction manager)
  - Runs background threads
- **sb_listener_* (per protocol)**
  - Accepts sockets, manages parser pool, reports load
- **sb_parser_* (per connection)**
  - Performs TLS and protocol handshake
  - Parses SQL and produces SBLR
  - Communicates with engine via IPC
  - Acts as the translation/API layer between a wire protocol and engine internals

### 3.3 Multi-Dialect Parser Support

- Each parser is a **protocol-specific translation layer**.
- Multiple parsers can be implemented with **1:1 functionality** to their emulated engines.
- The engine remains unchanged; only the parser layer adapts dialect and wire protocol.

### 3.2 Threads (inside sb_server)

- Lock Manager (per open database)
- MGA Garbage Collector (per open database)
- Job Scheduler
- Statistics Collector
- Cluster Manager (optional)
- Health Monitor

---

## 4. Startup Lifecycle

### Phase 1: Pre-Initialization (single-threaded)
- Parse CLI arguments
- Emergency logging
- PID file lock
- Validate configuration
- Signal handlers

### Phase 2: Core Initialization
- Full logging
- Network subsystem
- TLS/OpenSSL
- Open registry (format TBD)
- Open security database (if shared/cluster mode)

### Phase 3: Startup Databases (optional)
- Open configured startup databases
- Recovery (if required)
- Execute startup scripts
- Fire startup triggers

### Phase 4: Background Threads
- Start Lock Manager (per open DB)
- Start MGA GC (per open DB)
- Start Job Scheduler, Statistics Collector
- Start Cluster Manager (if enabled)
- Start Health Monitor

### Phase 5: Listeners
- Create control socket directory
- Spawn listener processes
- Wait for listener readiness

### Phase 6: Parser Pools
- Accept parser HELLO messages
- Validate protocol
- Send HELLO_ACK
- Register idle parser workers

### Phase 7: Ready
- Log ready state
- Notify systemd
- Enter main event loop

---

## 5. Connection Lifecycle (Canonical)

1. Client connects to listener port (TCP or Unix socket).
2. Listener selects idle parser and hands off socket.
3. **Parser performs TLS handshake** (if enabled).
4. Parser connects to engine over IPC.
5. Parser performs protocol handshake and authentication.
6. Parser sends SBLR requests to engine.
7. Engine executes and returns results to parser.
8. Parser formats results for client protocol.
9. On disconnect or error, parser exits or returns to idle.

**Key Point:** TLS is performed **in the parser**, not in the listener.

**Why this design:** The listener stays minimal (traffic cop), while the parser handles protocol complexity and security handshakes, keeping the engine isolated and protocol-agnostic.

---

## 6. Session State and Connection States

### 6.1 State Ownership

- Engine owns all session state and is the security arbiter.
- Parsers do not own authoritative state.

### 6.2 Connection States (Authoritative)

- **connected**: connected but idle (not doing work)
- **active**: connected and doing work
- **dormant**: client lost connectivity, parser was healthy; parser signals and engine records dormant state, then parser exits
- **disconnected**: connection/session/transaction gone; UUID mapping freed (rarely used)
- **terminated**: logging state only

### 6.3 Dormant Cleanup

- Dormant sessions are cleaned up by a server process using OAT/OIT statistics and configuration settings.

---

## 7. Disconnect and Recovery Behavior

### 7.1 Network Failure (Client Drops)

- Session becomes **dormant**.
- Transaction **continues**.
- Server-side work continues **for all actions** (SELECT, DML, DDL, stored procedures, triggers).
- The server **pauses at the first result boundary** (see section 8).

**Why this design:** Long-running analytical work should not be lost due to transient connectivity; the engine can finish work and resume delivery once the session is restored.

### 7.2 Parser Crash or Kill

- Processing stops immediately.
- **Rollback is the default** if no commit occurred.
- **No reconnection** is allowed after parser crash/kill.

**Why this design:** A crashed or killed parser cannot be trusted to preserve a valid protocol or execution context; the safest default is rollback and a fresh session.

---

## 8. Result Delivery and Resume

### 8.1 Pause Boundary

- The server pauses **the moment a result must be sent** to the client.
- A row is a full result.
- A partial tuple (large field, streamed COPY, etc.) is **not** a valid break and must be resent.
- The **first result** triggers the pause.

### 8.2 Buffering

- Buffering is in the server.
- If a result is waiting to be sent, server-side processes pause until it is sent.

### 8.3 Resume Token

- Resume token is per-row tuple, using **row UUIDs**.
- For single-return procedures/functions, the return itself is the resume boundary.
- Reconnection does not simply resume: connection/user/session/transaction are verified, state transfer acknowledged, then resume continues.

---

## 9. Reconnection Security

- Reconnect requires the **same user/password**, **same IP**, and **session or transaction ID**.
- Client prompts user once connectivity is reestablished.
- Reconnect is **not allowed** after password change or role revocation.
- Max reconnect attempts are configurable and should alert monitoring.
- Timeouts default to OAT/OIT cleanup settings.

---

## 10. Registry (Alpha Direction)

### 10.1 Status

Registry format and ownership are **not yet ratified**. The goal is:

- Easiest for end users to manually edit
- Small footprint
- Fast and simple to parse

### 10.2 Options Under Consideration

- **Per-database small text files**
  - Created on CREATE DATABASE
  - Deleted on DROP DATABASE
  - Assumes single-user admin operations

- **Single registry file**
  - Typical DB approach where create/drop is rare

The final decision is deferred until usage statistics are available.

---

## 11. Configuration Knobs (Status)

Recovery/timeouts/strictness settings exist conceptually, but are **not formalized**. They will be added as specific use cases emerge.

---

## 12. Reference Notes

- Any document that claims the registry is SQLite or a ScratchBird embedded database is **obsolete** for alpha.
- Any flow showing TLS handled by the listener is **obsolete**.
- Parser crash/kill and network disconnect are **distinct** behaviors.

---

## 13. Performance and Concurrency Advantages (Authoritative)

ScratchBird's transaction model and MGA design provide concrete performance and operational benefits:

1. **Readers never lock**
   - Read-only operations do not acquire locks.
   - Locking is only for write-write contention.

2. **Direct-write MGA (no WAL stream for core writes)**
   - Firebird-style MGA writes directly to the database record versions.
   - This avoids the WAL pattern of streaming changes to a delta log and then replaying them into the database.
   - Net effect: one primary data write path rather than WAL write + replay.

**Why this design:** The MGA approach removes WAL replay overhead and aligns with Firebird’s proven direct-write recovery semantics.

3. **Fast commit/rollback**
   - Deletes, rollbacks, and commits are transaction state changes (flags), not immediate physical rewrites.
   - This makes transactional control operations very fast.

4. **No practical transaction size limit**
   - Large inserts/deletes do not degrade performance due to transaction size alone.
   - Transactions can span billions of rows without intrinsic slowdown.

5. **Always-in-transaction ACID**
   - There is no “non-transaction” mode.
   - All operations are ACID-compliant by default.

This recovery model is solid and time-tested, proven over decades in InterBase and Firebird.

---

## 14. MGA Transaction Model Summary (Authoritative)

ScratchBird adopts Firebird's Multi-Generational Architecture (MGA) transaction model:

1. **No snapshots (PostgreSQL MVCC is forbidden)**
   - Visibility is determined by TIP state, not by snapshot structures.

2. **TIP-based visibility**
   - Transaction states are stored in Transaction Inventory Pages (2 bits per transaction).
   - Visibility checks are based on `TX_COMMITTED`, `TX_ACTIVE`, `TX_ABORTED`, `TX_LIMBO`.

3. **OIT/OAT/OST markers**
   - Stored in the database header and used for garbage collection and sweep decisions.
   - Sweep is triggered by `(OST - OIT) > sweep_interval`.

4. **Back-versioning with stable TIDs**
   - Updates modify the primary record in-place and chain back to older versions.
   - Index entries remain stable unless indexed columns change.

5. **Newest-to-oldest version chains**
   - Version traversal follows back pointers from newest to oldest.

6. **MGA recovery behavior**
   - Each write creates a new version with a back pointer to the prior record.
   - Commit advances transaction markers (OIT/OAT/OST as applicable).
   - On crash/no-commit, older versions remain valid; partial writes are cleaned up by GC.
   - No WAL replay or repair pass is required; recovery is effectively the normal startup path.

---

## 15. Risk Mitigations (Authoritative)

1. **GC pressure from long-running read transactions**
   - Dedicated GC threads reduce the operational impact of OST/OIT gaps.

2. **Dormant execution risk**
   - Management tools and statistics provide visibility and controls for dormant session behavior.

3. **Parser process overhead**
   - Modern multi-core CPUs and large RAM capacities reduce per-process overhead concerns.
   - Vertical and horizontal scaling are part of the design strategy.

---

## 16. AI Misconceptions (Authoritative)

1. **“Use WAL + replay”**
   - Wrong: ScratchBird uses Firebird MGA with direct-write versioning and TIP-based visibility.

2. **“Readers need locks”**
   - Wrong: Readers never lock; locks are only for write-write contention.

3. **“Snapshots are required”**
   - Wrong: Snapshot structures are forbidden. Visibility uses TIP state and OIT/OAT/OST markers.

4. **“Parser is a thread inside the server”**
   - Wrong: Parsers are separate processes, per connection.

5. **“Listener performs TLS and protocol handshake”**
   - Wrong: Listener only hands off sockets; parser performs TLS and protocol handshake.

6. **“Connection loss implies rollback”**
   - Wrong: Network loss creates a dormant session; transaction can continue. Rollback is default only on parser crash/kill without commit.

7. **“Registry must be a database”**
   - Wrong for alpha: registry format is not yet ratified and is expected to be a small, user-editable file.

---

## 17. What Not to Assume (Authoritative)

- There is no WAL replay stream for core writes.
- There are no snapshot structures; TIP-based visibility is mandatory.
- The listener never performs TLS or protocol handshakes.
- The registry format is not yet finalized and is intentionally simple for alpha.

---

## 18. Assumption Overrides (Explicit Map)

| Typical DB Assumption | ScratchBird Reality |
|---|---|
| WAL + replay is required | Firebird MGA direct-write; no WAL replay/repair pass |
| Snapshot structures define visibility | TIP-based visibility only; snapshots are forbidden |
| Listener performs TLS/handshake | Parser performs TLS and protocol handshake |
| Parser is a server thread | Parser is a separate process per connection |
| Read locks are needed | Readers never lock; locks only for write-write contention |
| Connection loss implies rollback | Network loss → dormant; rollback is default only on parser crash/kill |
| Registry is a database | Registry format is not ratified for alpha; expected to be small, user-editable |

---

## 19. Required Questions Before Proposing Changes

Use this checklist before suggesting architectural changes:

1. Does this introduce **snapshots** or any PostgreSQL MVCC semantics?
2. Does this assume **WAL replay/repair** for core recovery?
3. Does this violate **parser/engine trust boundaries**?
4. Does this move **TLS or protocol handshake** into the listener?
5. Does this require **read locks** for MGA readers?
6. Does this change **direct-write back-versioning** or stable TIDs?
7. Does this alter **dormant recovery semantics** (network loss vs parser crash)?

---

## 18. Engine Core References (Pointers)

Use the following specifications for engine-level details beyond server architecture:

- **Storage layout and on-disk format**: `docs/specifications/parser/v3/storage/ON_DISK_FORMAT.md`
- **Storage engine overview**: `docs/specifications/parser/v3/storage/STORAGE_ENGINE_MAIN.md`
- **Page management**: `docs/specifications/parser/v3/storage/STORAGE_ENGINE_PAGE_MANAGEMENT.md`
- **Buffer pool and cache policy**: `docs/specifications/parser/v3/storage/STORAGE_ENGINE_BUFFER_POOL.md`, `docs/specifications/parser/v3/core/CACHE_AND_BUFFER_ARCHITECTURE.md`
- **MGA internals and rules**: `MGA_RULES.md`, `docs/specifications/parser/v3/storage/MGA_IMPLEMENTATION.md`
- **Transaction model**: `docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md`, `docs/specifications/parser/v3/transaction/TRANSACTION_MAIN.md`
- **Locking and thread safety**: `docs/specifications/parser/v3/core/THREAD_SAFETY.md`
- **Index architecture and maintenance**: `docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md`, `docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md`
- **Catalog and system tables**: `docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_STRUCTURE.md`
- **DDL semantics**: `docs/specifications/parser/v3/ddl/02_DDL_STATEMENTS_OVERVIEW.md`
- **Query optimizer and execution**: `docs/specifications/parser/v3/query/QUERY_OPTIMIZER_SPEC.md`
- **SBLR bytecode and engine IPC contract**: `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`, `docs/specifications/parser/v3/SBLR_V3_BYTECODE_CONTAINER.md`
- **Wire protocol details**: `docs/specifications/parser/v3/wire_protocols/README.md`
- **Replication/cluster direction**: `docs/specifications/parser/v3/beta_requirements/replication/README.md`

**Terminology note:** ScratchBird uses Firebird MGA. Any MGA references in this file are legacy shorthand and must be interpreted as MGA per the authoritative references above.
