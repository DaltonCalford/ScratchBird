# Plan 16 - Attachment + Transaction Model (Always-In-Transaction, Multiplexing)

## Scope
Implement ScratchBird's always-in-transaction model with attachment multiplexing on a single connection. Provide explicit attachment and transaction routing, transaction mapping for remote execution, and runtime monitoring views to support emulated engine catalogs.

## Priority
P0 (core behavioral contract; required for Alpha parity and future cluster work).

## References
- `docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`
- `docs/archive/2026-01-04/planning/plan_03_sblr_version2_extended_opcodes.md` (transaction payload v2)
- `docs/archive/2026-01-04/planning/plan_03_security_context_auth_audit_quorum.md` (session/authkey binding)
- `docs/archive/2026-01-04/planning/plan_05_protocol_odbc_pool.md` (protocol/pool integration)
- `docs/archive/2026-01-09/planning/plan_06_metadata_show_and_catalog.md` (monitoring metadata)
- `docs/archive/2026-01-09/planning/plan_07_emulated_protocol_compatibility.md` (dialect behavior mapping)

## Decisions / Constraints (Resolved)
- ScratchBird is **always in a transaction**. There is never a "no transaction" state.
- `START TRANSACTION` / `SET TRANSACTION` / `SET AUTOCOMMIT` never edit the current transaction in place.
  - If a conflict action is not provided, the **default action is ROLLBACK** then start a new transaction.
- Transactions are **strictly scoped to a single attachment** and can **never** be shared across attachments.
- One TCP connection can host **multiple attachments** (server-to-server multiplexing).
- Each attachment can have **multiple concurrent transactions**, and each request can target a specific transaction.
- For remote operations, the remote server uses its **local transaction IDs** and returns them for mapping/auditing.
- **Attachment IDs are UUID v7**; transaction IDs are `uint64` (MGA XID).

## Order of Implementation
1) Core attachment model (AttachmentContext, attachment registry).
2) Always-in-transaction lifecycle (attach/start, commit/rollback, autocommit).
3) Multi-transaction routing per attachment (request-level txn_id).
4) Native protocol attachment/txn routing (message flags + payloads).
5) Runtime monitoring views (attachments/transactions/mappings).
6) Tests (single/multi attachment, multi-transaction, concurrency).

## Concrete Code Touchpoints (Exact Files + Functions)
- Core:
  - `include/scratchbird/core/connection_context.h`
    - Add `AttachmentContext` struct and attachment registry APIs.
    - Add per-attachment default transaction settings and autocommit mode.
  - `src/core/connection_context.cpp`
    - Implement attachment lifecycle + transaction routing.
    - Implement conflict-action start rules and always-in-transaction behavior.
  - `include/scratchbird/core/transaction_manager.h`
    - Add APIs that accept `attachment_id` and `txn_id` explicitly.
  - `src/core/transaction_manager.cpp`
    - Track transactions per attachment; allow concurrent transactions per attachment.
  - `src/server/server_session.cpp`
    - Route every request by attachment_id and txn_id (native protocol only).
- Protocol:
  - `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`
    - Update to include attachment_id and txn_id routing (see Protocol section).
  - `src/protocol/adapters/native_adapter.cpp`
    - Encode/decode attachment_id + txn_id, create/destroy attachments.
- Catalog/Monitoring:
  - `src/catalog/virtual_catalog.cpp`
    - Add runtime views for attachments/transactions.
  - `src/catalog/firebird_catalog.cpp`
    - Map MON$ATTACHMENTS/MON$TRANSACTIONS to runtime views.

## Required Data/Schema Changes
**Runtime views (virtual, no on-disk persistence):**
```sql
-- Active attachments (runtime)
CREATE VIEW sys.runtime.attachments AS (
  attachment_id UUID,
  session_id UUID,
  user_id UUID,
  role_id UUID,
  emulation_mode TEXT,
  connection_id UUID,
  created_time BIGINT,
  last_activity_time BIGINT,
  autocommit_mode SMALLINT,
  current_txn_id BIGINT
);

-- Active transactions (runtime)
CREATE VIEW sys.runtime.transactions AS (
  txn_id BIGINT,
  attachment_id UUID,
  read_only SMALLINT,
  isolation_level SMALLINT,
  start_time BIGINT,
  state SMALLINT
);

-- Remote/local transaction mapping (runtime)
CREATE VIEW sys.runtime.txn_map AS (
  map_id UUID,
  origin_server_id UUID,
  origin_attachment_id UUID,
  origin_txn_id BIGINT,
  local_txn_id BIGINT,
  created_time BIGINT,
  last_seen_time BIGINT,
  state SMALLINT
);
```

**Per-user/role/group default transaction settings (catalog tables):**
```sql
CREATE TABLE sys.security.txn_defaults_user (
  user_id UUID PRIMARY KEY,
  isolation_level SMALLINT NOT NULL,
  access_mode SMALLINT NOT NULL,
  wait_mode SMALLINT NOT NULL,
  lock_timeout INT NOT NULL,
  autocommit_mode SMALLINT NOT NULL
);

CREATE TABLE sys.security.txn_defaults_role (
  role_id UUID PRIMARY KEY,
  isolation_level SMALLINT NOT NULL,
  access_mode SMALLINT NOT NULL,
  wait_mode SMALLINT NOT NULL,
  lock_timeout INT NOT NULL,
  autocommit_mode SMALLINT NOT NULL
);

CREATE TABLE sys.security.txn_defaults_group (
  group_id UUID PRIMARY KEY,
  isolation_level SMALLINT NOT NULL,
  access_mode SMALLINT NOT NULL,
  wait_mode SMALLINT NOT NULL,
  lock_timeout INT NOT NULL,
  autocommit_mode SMALLINT NOT NULL
);
```

Default resolution order for new attachments:
1) User default, else
2) Role default, else
3) Group default, else
4) Database default (Firebird READ COMMITTED, WAIT, read-write, autocommit OFF).

## Protocol Changes (Native ScratchBird)
**Message header routing fields:**
- Extend the native protocol header to include:
  - `attachment_id` (UUID v7, 16 bytes)
  - `txn_id` (uint64)
- After AUTH_OK, every request must include non-zero `attachment_id` and `txn_id`.
- Values are server-assigned; clients must echo them on every request.
- Bump native protocol minor version to 1; reject v1.0 headers in Alpha.
- AUTH_OK carries the initial attachment_id and txn_id in its header.

**New client->server message types:**
- `ATTACH_CREATE` (0x1E): create new attachment.
  - Payload: `authkey_id?`, `emulation_mode`, `db_name`.
  - Request: client supplies non-zero `attachment_id` and `txn_id` in the header (the caller's attachment/transaction).
  - Response: `attachment_id` + `current_txn_id`.
- `ATTACH_DETACH` (0x1F): detach an attachment.
  - Payload: `attachment_id`.
  - Server rolls back active transactions in that attachment and removes it.
- `ATTACH_LIST` (0x20): list attachments on connection.
  - Response: standard result set (`ROW_DESCRIPTION` + `DATA_ROW` + `COMMAND_COMPLETE`) with attachment_id + summary fields.

**Request routing rules:**
- Route using `attachment_id` and `txn_id` from the header.
- If required fields are missing/zero, return an error and do not execute.

## Always-In-Transaction Rules (Executor/ConnectionContext)
**Attachment creation:**
- On attach, immediately start a new transaction using resolved defaults.
- Record `current_txn_id` in attachment context.

**COMMIT/ROLLBACK:**
- Always commit/rollback the target transaction.
- Immediately start a **new** transaction for that attachment.
  - If COMMIT/ROLLBACK used `AND CHAIN`, carry forward the previous settings.
  - If `AND NO CHAIN` or unspecified, use defaults.

**START/SET TRANSACTION:**
- Apply conflict action against the attachment's current transaction:
  - `COMMIT`: commit current, then start new with requested settings.
  - `ROLLBACK`: rollback current, then start new with requested settings.
  - `ERROR`: return error and keep current transaction.
  - `KEEP`: return success but do **not** start a new transaction.
  - `DEFAULT`: use per-user/role default action (system default = ROLLBACK).

**AUTOCOMMIT:**
- `SET AUTOCOMMIT ON/OFF` behaves like START TRANSACTION with conflict action.
- Autocommit ON:
  - For each statement: commit immediately after execution, then start a new transaction.
  - For explicit BEGIN/START TRANSACTION blocks, autocommit is suspended until COMMIT/ROLLBACK.

## Remote Transaction Mapping
- When server A issues a remote operation to server B:
  - Server B starts/uses a local transaction and returns `local_txn_id`.
  - Server A records mapping: `(origin_server_id, origin_attachment_id, origin_txn_id) -> local_txn_id`.
- Mapping records are garbage collected when:
  - Both origin and local transactions are committed/rolled back, and
  - No active attachment references the mapping.

## Implementation Tasks (Detailed)
1) **AttachmentContext**
   - Add `AttachmentContext` with fields:
     - `attachment_id`, `session_id`, `user_id`, `role_id`, `emulation_mode`,
       `autocommit_mode`, `current_txn_id`, `txn_defaults`.
   - Maintain `unordered_map<UUID, AttachmentContext>` in `ConnectionContext` guarded by a mutex (multi-threaded access).

2) **Transaction Routing**
   - Update `ConnectionContext::getCurrentTransactionId()` to accept an optional attachment_id.
   - Add `ConnectionContext::getTransactionId(attachment_id, txn_id)` for explicit txn routing.
   - Ensure all executor paths resolve attachment_id + txn_id before executing DDL/DML.

3) **Conflict Actions + Defaults**
   - Add default transaction settings resolver:
     - Resolve from user -> role -> group -> DB default.
   - Implement conflict action in `startTransaction()` and autocommit toggles.

4) **Native Protocol Routing**
   - Implement attachment messages: create/detach/list.
   - Add attachment_id/txn_id routing in `NativeAdapter`.
   - Update protocol spec and tests.

5) **Runtime Monitoring Views**
   - Implement `sys.runtime.attachments`, `sys.runtime.transactions`, `sys.runtime.txn_map` in `virtual_catalog.cpp`.
   - Map Firebird `MON$ATTACHMENTS` / `MON$TRANSACTIONS` to these views.
   - Expose to MySQL/PG performance/pg_stat views as needed (Plan 13/14).

## Completion Checklist (Developer)
- [ ] AttachmentContext implemented; multiple attachments per connection supported.
- [ ] Always-in-transaction behavior enforced on attach/commit/rollback.
- [ ] START/SET TRANSACTION conflict actions implemented.
- [ ] Autocommit ON/OFF implemented as transaction-bound behavior.
- [ ] Native protocol supports attachment_id + txn_id routing.
- [ ] Runtime monitoring views implemented and queryable.

## Completion Checklist (Auditor)
- [ ] No execution path exists without an active transaction.
- [ ] Transactions never cross attachments.
- [ ] A single connection can host multiple attachments with isolated transactions.
- [ ] Autocommit ON executes each statement in its own transaction.
- [ ] Monitoring views return accurate attachment/transaction state.

## Testing Requirements
- Unit tests:
  - Attachment creation/removal.
  - Conflict-action behavior (commit/rollback/error/keep).
  - Default transaction setting resolution.
- Integration tests:
  - Two attachments on one connection; concurrent transactions do not leak.
  - Autocommit ON/OFF toggling; verify transaction boundaries.
  - Remote transaction mapping creation and cleanup.
- Protocol tests:
  - ATTACH_CREATE/ATTACH_DETACH/ATTACH_LIST flow.
  - Queries with explicit attachment_id and txn_id.

## Acceptance Criteria
- ScratchBird always has an active transaction per attachment.
- Native protocol supports multiple attachments on a single connection.
- Transaction isolation is preserved across attachments and concurrent transactions.
- Monitoring views provide required data for emulated engine catalogs.
