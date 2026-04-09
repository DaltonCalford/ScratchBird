# Listener Parser Handoff Protocol

## Scope

This document defines the current shipped control-plane contract between the
listener runtime and parser workers.

It is a bounded internal runtime seam. It is not a stable public wire protocol,
driver ABI, or plugin ABI.

## Message framing

Current control-plane framing uses:
- `CONTROL_PLANE_MAGIC = 0x54434253`
- `CONTROL_PLANE_VERSION = 1`
- `CONTROL_PLANE_FLAG_HAS_HANDLE = 0x0001`

Current header fields are:
- `magic`
- `version`
- `message_type`
- `flags`
- `reserved`
- `request_id`
- `payload_len`

## Current message families

Current shipped `ControlPlaneMessageType` values are:
- `HELLO = 0x0001`
- `HELLO_ACK = 0x0002`
- `SPAWN_REQUEST = 0x0010`
- `SPAWN_READY = 0x0011`
- `HANDOFF_SOCKET = 0x0020`
- `HANDOFF_ACK = 0x0021`
- `LPREFACE = 0x0022`
- `LPREFACE_ACK = 0x0023`
- `LPREFACE_NACK = 0x0024`
- `HEALTH_CHECK = 0x0030`
- `HEALTH_REPORT = 0x0031`
- `POOL_STATS = 0x0040`
- `RECYCLE = 0x0050`
- `SHUTDOWN = 0x0051`
- `MANAGEMENT_COMMAND = 0x0060`
- `MANAGEMENT_RESPONSE = 0x0061`
- `ERROR_MESSAGE = 0x00FF`

## Current semantic use

- `HELLO` and `HELLO_ACK`
  - parser worker registration and admission
- `HANDOFF_SOCKET` and `HANDOFF_ACK`
  - transfer of an accepted client socket plus validated binding context to a
    worker
- `LPREFACE`, `LPREFACE_ACK`, `LPREFACE_NACK`
  - manager-to-listener managed-mode preface validation
- `HEALTH_CHECK` and `HEALTH_REPORT`
  - worker liveness and health probing
- `RECYCLE`
  - worker retirement request
- `SHUTDOWN`
  - controlled worker shutdown
- `MANAGEMENT_COMMAND` and `MANAGEMENT_RESPONSE`
  - local admin channel, not client-facing protocol traffic

## `DBBT` contract

Current `DBBT` fields are:
- `version`
- `db_uuid[16]`
- `listener_id`
- `issued_at_ms`
- `expires_at_ms`
- `manager_session_id[16]`
- `client_nonce`
- `server_nonce`
- `flags`
- `mac`

Validation enforces:
- listener id match
- issuance and expiry window
- clock skew bound
- MAC verification
- replay protection when enabled

Defaults:
- `DBBT_VERSION = 1`
- `DBBT_MAC_BYTES = 32`
- `DBBT_DEFAULT_CLOCK_SKEW_MS = 2000`

## `LPREFACE` contract

Current `LPREFACE` fields are:
- `magic = 0x504C4253`
- `version = 1`
- `reserved`
- `listener_id`
- `dbbt`
- `db_selector`
- `requested_profile`
- `flags`

Current bounds:
- `LPREFACE_MAX_DBBT_BYTES = 8192`
- `LPREFACE_MAX_TEXT_BYTES = 1024`

Current `LPREFACE` nack classes are:
- `NONE`
- `INVALID_FORMAT`
- `INVALID_DBBT`
- `LISTENER_MISMATCH`
- `DB_NOT_READY`
- `POLICY_DENIED`
- `RESOURCE_EXHAUSTED`
- `INTERNAL_ERROR`

`LPREFACE` validation must fail closed on:
- invalid magic or version
- invalid DBBT length
- missing listener id
- listener mismatch
- DBBT validation failure

## Managed-mode binding context

The listener queues managed binding context for handoff:
- derived database UUID
- `DBBT` id
- manager session id
- listener id

This context binds the accepted socket to the validated owner database before
parser work begins.

## `HANDOFF_SOCKET` admission rules

The handoff contract is:

1. listener chooses one parser worker
2. listener allocates a request id
3. listener attaches client address, TLS state, and binding context
4. listener sends `HANDOFF_SOCKET`
5. worker returns `HANDOFF_ACK`
6. listener treats the transfer as complete only after the ack

Required fail-closed rule:
- if the listener is in `require_proxy_binding` mode and no pending validated
  binding context exists, the handoff must be rejected

Required authority rule:
- the parser worker consumes binding context
- it does not mint or widen binding context itself

## Manager versus parser boundary

Managed-mode `LPREFACE` validation is not parser admission by itself.

The current order is:
1. manager obtains `LPREFACE` acceptance from the listener
2. listener records or queues the validated binding context
3. ordinary listener-to-parser `HANDOFF_SOCKET` still occurs
4. parser worker still owns protocol handshake and downstream engine attach

This preserves a strict separation between:
- manager-to-listener validation
- listener-to-parser socket transfer
- parser-to-engine session attach

## Hard boundaries

- The older abstract `LPH-*` transcript is not the current shipped authority.
- Control-plane semantics are owned by current source and this document only.
- No additional control-plane families may be invented by downstream agents
  without extending both the enum set and the listener/parser implementation.
- Manager validation traffic and parser handoff traffic share framing but not
  ownership; do not collapse them into one trust boundary.
