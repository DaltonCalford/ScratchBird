# MCP Manager Control and Database Binding Model

## Scope

This file defines the current code-backed MCP control and binding messages used by the optional ScratchBird manager.

This file is authoritative for:

- bounded MCP manager message families
- authentication sequencing
- database binding through DBBT
- listener binding through LPREFACE
- downstream connect flagging for manager-bound admission

## Protocol family

MCP is a bounded ScratchBird-native management control family. It is not the emulated listener dialect protocol.

The current code and tests prove the following message family:

- `MCP_HELLO`
- `MCP_AUTH_START`
- `MCP_AUTH_CONTINUE`
- `MCP_DB_LIST`
- `MCP_DB_INFO`
- `MCP_DB_CONNECT`

## Sequencing

### Required sequence

The current code-backed control flow is:

1. `MCP_HELLO`
2. `MCP_AUTH_START`
3. `MCP_AUTH_CONTINUE`
4. database discovery or info verbs
5. `MCP_DB_CONNECT`
6. downstream connect carrying manager-bound admission material

### Refusal rules

The current code refuses:

- `MCP_AUTH_CONTINUE` before `MCP_AUTH_START`
- empty continuation payload
- method mismatch across auth steps
- info or connect verbs before authentication
- unsupported client intent on `MCP_DB_CONNECT`
- invalid `client_nonce` length on `MCP_DB_CONNECT`

## Authentication model

The current code-backed MCP path uses:

- username on auth start
- `TOKEN` auth method

This file does not authorize generic alternative manager auth methods unless a later canonical spec adds them.

## Database binding model

### Manager-issued binding

Before downstream connection establishment, the manager issues a DBBT and constructs LPREFACE.

### Listener-side validation

The listener validates LPREFACE and DBBT before it accepts the manager-bound connect path.

### Downstream connect flag

The downstream connect path marks manager-bound admission through `CONNECT_FLAG_MANAGER_DBBT`.

Canonical rule:

- a manager-bound connect must remain distinguishable from an ordinary listener-direct connect

## Message and payload contract

### `MCP_HELLO`

The hello message negotiates manager-protocol version and capability bits for the MCP session.

### `MCP_AUTH_START`

Must carry:

- username
- `TOKEN` auth method

### `MCP_AUTH_CONTINUE`

Must carry:

- non-empty continuation payload

### `MCP_DB_LIST`

Returns bounded database inventory allowed under the authenticated manager context.

### `MCP_DB_INFO`

Returns bounded database metadata allowed under the authenticated manager context.

### `MCP_DB_CONNECT`

Must carry enough identity to bind:

- target database intent
- client intent
- bounded client nonce

The manager then derives DBBT and LPREFACE and validates them against the selected listener.

## Boundary with other sections

This file does not redefine:

- listener-management IPC
- engine internal control-plane ABI
- remote deployment queue semantics
- manager heartbeat bus semantics

Those live in sections `25`, `29`, and `24`.

This file only defines the manager-side public control-message family and the database-binding sequence the current code already proves.
