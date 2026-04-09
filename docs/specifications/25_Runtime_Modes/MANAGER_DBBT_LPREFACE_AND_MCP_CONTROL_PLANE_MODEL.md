# Manager DBBT, LPREFACE, and MCP Control Plane Model

## Scope

This file defines the current code-backed manager front-door control plane and the required reconstructed expansion boundary for cluster-managed remote control.

This file is authoritative for:

- optional manager front-door behavior
- manager-issued DB binding tokens
- listener preface validation
- MCP control verbs currently present in code
- the separation between current shipped manager control and the larger reconstructed remote-management queue

## Architectural role

The manager is optional and ScratchBird-native.

When enabled, it sits in front of listeners and acts as a server-local proxy and admission point. It is not an emulated-engine listener.

The current code-backed role of the manager is:

- authenticate manager clients
- expose MCP control verbs
- enumerate databases and database info
- issue DBBT
- construct LPREFACE
- validate LPREFACE with the target listener
- proxy or authorize downstream connection establishment

The manager is the correct future home for server-local heartbeat and cluster-control integration. The listener is not.

## Listener relationship

The listener remains:

- local
- bounded
- controlled by the database and controller path
- untrusted by the engine

The manager does not grant the listener independent authority. It binds client intent to a specific database and listener target through DBBT plus LPREFACE.

## MCP current code-backed control verbs

The current code and wire tests prove a bounded MCP message family:

- `MCP_HELLO`
- `MCP_AUTH_START`
- `MCP_AUTH_CONTINUE`
- `MCP_DB_LIST`
- `MCP_DB_INFO`
- `MCP_DB_CONNECT`

### Authentication model

The current manager implementation requires:

- username on `MCP_AUTH_START`
- `auth_method=TOKEN`
- non-empty continuation payload on `MCP_AUTH_CONTINUE`
- method continuity across start and continue

Authentication must complete before database-info or database-connect verbs are accepted.

### Database connect model

`MCP_DB_CONNECT` currently enforces:

- authenticated session
- supported client intent only
- `client_nonce` length in the range `16..32` bytes
- manager-issued DBBT before downstream connect
- LPREFACE construction and listener validation before downstream connect is accepted

## DBBT model

### Purpose

DBBT is the manager-issued database binding token used to bind a manager-authorized connection attempt to a specific listener and database target.

### Current wire body

The current code-backed DBBT body carries:

- `version`
- `db_uuid`
- `listener_id`
- `issued_at_ms`
- `expires_at_ms`
- `manager_session_id`
- `client_nonce`
- `server_nonce`
- `flags`
- `mac`

### Current validation rules

The current code and tests prove all of the following:

- token size is bounded
- nonce sizes are bounded
- HMAC-SHA256 signing is used
- `listener_id` match is mandatory
- expiry and not-yet-valid windows are enforced
- replay detection can be enabled through a replay cache
- forged payload mutation fails verification
- keyring loading supports active-key designation

### Canonical meaning

DBBT is authorization evidence for listener binding. It is not a general session continuation token, not a user-rights grant, and not a transaction resurrection token.

## LPREFACE model

### Purpose

LPREFACE is the bounded listener-preface frame that packages manager-issued DBBT and requested listener binding context for listener-side validation.

### Current wire shape

The current code-backed LPREFACE carries:

- magic
- version
- reserved
- `listener_id`
- encoded DBBT payload
- `db_selector`
- `requested_profile`
- `flags`

### Current validation rules

The current code and tests prove:

- `listener_id` is mandatory
- DBBT payload is mandatory
- text fields are bounded
- version and magic are checked
- listener-id mismatch is rejected
- DBBT is validated under listener options and replay policy
- LPREFACE ack is a distinct bounded response frame

### Canonical meaning

LPREFACE is the listener admission seam for manager-bound connects. It is not public SQL protocol state and not a general extension ABI.

## Engine and listener control path

The canonical control path remains:

- database-admin surface
- controller
- listener-management IPC
- listener runtime

The manager sits in front of that path for optional ScratchBird-native admission, proxying, and later cluster heartbeat/control.

Listener-local status remains listener-owned. Combined engine plus listener plus manager health is an aggregated management view and must not be fabricated by the listener alone.

## Current code-backed versus reconstructed-required behavior

### Current code-backed

The current code proves:

- manager process
- bounded MCP verbs
- token authentication
- DBBT issuance
- LPREFACE encoding and validation
- listener validation commands
- downstream connect binding with manager DBBT flagging

### Required reconstructed behavior

The lost-spec rebuild requires that the manager become the server-local cluster/control agent for:

- heartbeat publication
- remote instruction assessment
- queued deployment execution
- drift reporting
- remote inspection

That larger control plane must be rebuilt on top of the current manager and cluster-fabric catalog substrate. It must not replace DBBT/LPREFACE with a looser bindless listener admission model.

### Drift boundary

If the current code lacks the full remote instruction queue, the canonical requirement still stands:

- the manager owns the cluster-facing heartbeat and control seam
- the listener does not become the cluster heartbeat bus
- the database remains the ultimate authority for listener topology and policy

## Security boundary

Manager authentication alone does not imply full mutation authority.

The canonical security split remains:

- inspection privilege
- listener/runtime mutation privilege
- remote deployment privilege
- security-policy mutation privilege

Any future cluster-control expansion must preserve that split.

## MGA boundary

Manager control and cluster-control state do not bypass MGA. Any persisted instruction, drift, heartbeat, or deployment record remains ordinary MGA-governed database state.
