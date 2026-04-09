# Listener Management IPC Channel

## Channel scope

The listener-management channel is a local, tightly scoped, untrusted-by-engine
runtime seam between:
- `ServiceController`
- the optional manager proxy
- future engine-owned admin surfaces
- a listener process

It is not a remote client protocol.
It is not a durable policy store.
It is not a substitute for engine-owned MGA diagnostics.

When the cluster-management layer is used, remote control must still terminate
into this same local seam through the server-local manager and engine-owned
admin path. The existence of remote control does not change the local authority
model of this channel.

## Transport

Current transport is:
- local control-plane framing
- request type `MANAGEMENT_COMMAND`
- response type `MANAGEMENT_RESPONSE`
- local Unix socket on non-Windows platforms

Windows status:
- listener management IPC is not currently implemented on Windows

## Response payload format

`MANAGEMENT_RESPONSE` payload format is:
- byte `0`: status flag
  - `0` = success
  - `1` = failure
- bytes `1..n`: UTF-8 text message

## Current command set

Current shipped commands are:
- `PING`
- `STATUS`
- `STOP graceful`
- `STOP force`
- `RELOAD`
- `POOL SET <min> <max>`
- `KILL <connection_id>`
- `DBBT_VALIDATE <hex_token>`
- `LPREFACE_VALIDATE <hex_preface>`

## Authority model

Every mutating command on this channel is a request to the listener runtime.

The listener may:
- validate syntax
- validate local runtime feasibility
- execute the bounded runtime action it already supports

The listener may not:
- self-authorize new emulation families
- widen its bind address or port authority
- change owner-database identity
- redefine engine policy

Engine, controller, or future admin-SQL surfaces remain the authority that
decides whether a request should be issued in the first place.

Required reconstructed consequence:
- cluster-layer remote management may request listener changes
- the server-local manager receives the remote instruction
- the engine-owned admin surface validates policy and privilege
- the controller translates approved work into a bounded listener-management
  command
- the listener executes only the local runtime action it already supports

## Required command semantics

### `PING`

Returns:
- success
- message `PONG`

### `STATUS`

Current listener-local `STATUS` is listener-owned only.

It returns a semicolon-delimited status string containing:
- `draining`
- `owner_database`
- `active_sessions`
- `warm_workers`
- `pool_min`
- `pool_max`

Required rule:
- combined engine plus listener health must be assembled outside the listener
  by querying engine-owned status separately

The listener must not invent engine-owned fields such as:
- MGA durability status
- derivative queue state
- shadow-group readiness
- restore or failback boundary identifiers

Those belong to engine or controller aggregation, not to listener-local status
truth.

### `STOP graceful`

Effects:
- set listener drain mode
- set pool drain mode
- allow current sessions to complete

Returns:
- success
- message `draining`

### `STOP force`

Effects:
- set force-shutdown flag
- set global shutdown flag

Returns:
- success
- message `force_shutdown`

### `RELOAD`

Current `RELOAD` re-applies reloadable listener runtime settings only.

It may update:
- `pool_min`
- `pool_max`
- `health_check_interval_ms`
- `max_requests`
- `max_age_seconds`
- `spawn_strategy`
- `log_level`

It may not change:
- bound port
- bind address
- control socket directory
- engine endpoint
- owner database
- listener protocol family

### `POOL SET <min> <max>`

Validation:
- `min > 0`
- `max > 0`
- `min <= max`

On success:
- parser-pool runtime bounds are updated
- listener-local runtime config copy is updated
- message is `pool_updated`

This command is a bounded runtime resize request. It does not make the listener
the authority for parser-pool policy.

### `KILL <connection_id>`

On success:
- the worker handling that connection is terminated
- worker state is forced to fault or recycled
- replacement capacity may be spawned by existing pool rules

Failure classes:
- invalid connection id
- connection not found
- kill failed
- Windows not implemented

### `DBBT_VALIDATE <hex_token>`

Validation uses:
- listener id
- current epoch millis
- configured clock skew
- replay cache
- configured or fallback key ring

Success format:
- `dbbt_valid:<dbbt_id_hex>`

Failure format:
- `dbbt_invalid`
- or `dbbt_invalid:<reason>`

This command is a manager or controller validation helper. It does not make the
listener the durable authority for database ownership or session authorization.

### `LPREFACE_VALIDATE <hex_preface>`

Validation uses:
- `LPREFACE` decode and field checks
- embedded `DBBT` validation
- listener id match
- replay policy

Result emits:
- success with encoded ack payload summary
- failure with encoded nack payload summary
- managed audit event for the decision

This command is the current manager-to-listener binding-validation path before
proxy traffic is allowed to reach the internal native listener.

## Admin surface rule

When listener control is surfaced through engine or admin-SQL workflows, those
workflows must dispatch through the same controller or management seam.

The same rule applies to cluster-layer remote management workflows.

Canonical consequence:
- no direct client-to-listener control path becomes authoritative
- listener-reported status remains observational
- listener-executed control remains bounded runtime work
- cluster management does not make the listener the source of topology or
  policy truth

## Hard boundaries

- No richer remote management transport is current authority.
- This channel is local operational control only.
- Listener-local status is not the authoritative engine health surface.
- Listener-local control is not the authoritative cluster deployment record.
