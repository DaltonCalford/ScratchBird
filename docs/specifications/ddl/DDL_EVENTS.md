# Event Notifications Specification

**Document:** DDL_EVENTS.md  
**Status:** Alpha (Firebird compatibility)  
**Version:** 2.0  
**Date:** 2026-01-09  
**Authority:** Chief Architect

---

## 1. Purpose

ScratchBird supports Firebird-style event notifications for push-style updates to
clients. Events are **not** scheduled jobs. Instead, they are lightweight signals
posted by SQL or PSQL that notify registered clients on commit. ScratchBird native
clients can optionally receive immediate (out-of-transaction) events.

Event notifications are used for:
- Client UI refresh or invalidation
- Out-of-band "data changed" signaling
- Simple workflow triggers outside the SQL transaction flow

---

## 2. Core Concepts

### 2.1 Event Name
- Event names are strings up to **127 bytes** (Firebird limit).
- Matching is exact byte match; the listener must register the same name.
- Recommended: include a UUID in the name for uniqueness, for example:
  `order_ready.018f6a8d-2aa5-7e2a-8a2b-0e3a4f1b2c3d`

### 2.2 Event UUID (internal)
- Each POST_EVENT generates an **event UUID** (UUIDv7) for the notification instance.
- The engine stores event UUIDs as internal IDs and only exposes UUIDs in user-facing
  replies where supported.
- Firebird protocol clients only receive event **names** and **counts**.
- ScratchBird native clients may receive the event UUID.

### 2.3 Message Payload (optional)
- ScratchBird can attach an optional message payload (text, up to 1024 bytes).
- Firebird protocol does not carry payloads; payloads are available only to
  ScratchBird-native clients.

### 2.4 Delivery Modes

**ON COMMIT (default):**
- Events are queued in the transaction.
- If the transaction commits, the event is delivered.
- If the transaction rolls back, the event is discarded.
- This is the only mode supported in Firebird emulation.

**IMMEDIATE (ScratchBird extension, native only):**
- Events are delivered at the time of posting.
- Delivery is independent of transaction commit/rollback.
- Firebird dialect rejects IMMEDIATE (syntax error).

---

## 3. Posting Events

### 3.1 POST_EVENT Statement

**Syntax (ScratchBird native):**

```sql
POST_EVENT 'event_name'
  [ON COMMIT | IMMEDIATE]
  [MESSAGE 'optional_message'];
```

**Syntax (Firebird emulation):**

```sql
POST_EVENT 'event_name';
```

**Notes:**
- `ON COMMIT` is the default and matches Firebird behavior.
- `IMMEDIATE` is a ScratchBird extension (native only).
- MESSAGE is optional and is ignored by Firebird protocol clients.
- Firebird emulation supports only the Firebird syntax shown above.

**Examples:**

```sql
-- Firebird-compatible (on commit)
POST_EVENT 'new_order';

-- Name includes UUID for uniqueness
POST_EVENT 'order_ready.018f6a8d-2aa5-7e2a-8a2b-0e3a4f1b2c3d';

-- Immediate notification (ScratchBird native only)
POST_EVENT 'cache_invalidate' IMMEDIATE;

-- Immediate with message (native clients only)
POST_EVENT 'sync_required' IMMEDIATE MESSAGE 'row=12345';
```

---

## 4. Event Registration and Delivery

### 4.1 Firebird Wire Protocol

Firebird-compatible clients register for events using:
- `op_que_events` to arm a listener
- `op_event` for notifications
- `op_cancel_events` to stop listening

Details are defined in:
`ScratchBird/docs/specifications/wire_protocols/firebird_wire_protocol.md`

Behavior:
- Each event name has a counter.
- Notifications deliver updated counts.
- Clients compute deltas and re-arm with `op_que_events`.
- Events are delivered only on commit (Firebird semantics).

### 4.2 ScratchBird Native Protocol

ScratchBird native clients may receive:
- Event name
- Count delta
- Event UUID
- Optional message payload

### 4.3 Client API Notes

See `ScratchBird/docs/specifications/future/C_API_SPECIFICATION.md` for:
- `sb_register_event`
- `sb_wait_for_event`
- `sb_post_event`
- `sb_unregister_event`

---

## 5. Security and Limits

### 5.1 Posting Rules
- `POST_EVENT` is allowed for authenticated sessions by default.
- Optional config can restrict posting to specific roles or allowlisted prefixes.

### 5.2 Listener Rules
- Event registration is per-attachment.
- Optional config can restrict which roles can register listeners.

### 5.3 Resource Limits
- Max events per registration: configurable (default 64).
- Max event name length: 127 bytes.
- Max message length: 1024 bytes (native clients only).

---

## 6. Observability

Suggested audit events:
- EVENT_POSTED
- EVENT_DELIVERED

Suggested monitoring:
- total_events_posted
- events_delivered
- listeners_active

---

## 7. Non-Goals

- Events do **not** schedule jobs.
- Events are not persistent catalog objects.
- Events are not stored in sys.jobs or sys.job_runs.

---

## 8. References

- Firebird language reference: POST_EVENT behavior and 127-byte name limit
  (`ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`)
- Firebird wire protocol event ops:
  `ScratchBird/docs/specifications/wire_protocols/firebird_wire_protocol.md`
