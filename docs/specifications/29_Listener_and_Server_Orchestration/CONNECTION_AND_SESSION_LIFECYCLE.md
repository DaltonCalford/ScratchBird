# Connection and Session Lifecycle

## Direct listener lifecycle

1. The listener accepts a client socket on one protocol front door.
2. If the listener or parser pool is draining, the connection is rejected
   immediately.
3. The listener acquires a parser worker from the pool.
4. If no worker is available within the queue window, the connection is
   rejected as queue saturation.
5. The accepted client socket is handed to the selected parser worker through
   the local control-plane handoff path.
6. The parser worker owns protocol handshake and all further client protocol
   exchange for that socket.

## Manager-proxy lifecycle

When the optional manager is in front of the internal native listener:

1. The client connects to the manager, not directly to the internal native
   listener.
2. The manager performs its own authentication and database-selection flow.
3. The manager limits `DB_CONNECT` to the configured manager-proxy owner
   database.
4. The manager constructs a `DBBT`.
5. The manager constructs `LPREFACE` carrying:
   - listener id
   - encoded `DBBT`
   - database selector
   - requested profile
6. The manager asks the internal native listener to validate that `LPREFACE`
   over the local listener-management channel.
7. Only on successful validation does the manager proxy the socket to the
   internal native listener.

Manager-proxy validation is a pre-parser binding stage. It does not replace the
native session attach, authentication, or MGA transaction rules inside the
engine.

## Parser worker registration lifecycle

1. Parser worker starts and connects to the listener local control socket.
2. Worker sends `HELLO`.
3. Listener validates protocol and pool capacity.
4. Listener returns `HELLO_ACK`.
5. Listener marks the worker as available for handoff.

Current worker states are:
- `IDLE`
- `BUSY`
- `DRAINING`
- `FAULT`

## Listener to parser handoff lifecycle

The listener-to-parser handoff is a bounded control-plane transfer, not a
transaction or catalog decision point.

Required current behavior:

1. The listener builds a `HANDOFF_SOCKET` message.
2. The handoff payload carries client-address and TLS state.
3. The handoff also carries binding context:
   - direct mode:
     - database UUID template
     - listener id template
   - manager-proxy mode:
     - validated database UUID
     - listener id
     - `DBBT` id
     - manager session id
4. If `require_proxy_binding = true` and no pending validated binding context
   exists, handoff fails closed.
5. The parser worker must acknowledge handoff before the listener treats the
   transfer as complete.

## Engine attach lifecycle

When the parser worker attaches an engine session:

1. It creates an engine `ATTACH`-equivalent IPC request keyed by the client
   session id.
2. The engine IPC session handler creates an `EngineSessionState`.
3. The handler either:
   - opens a fresh database connection context
   - or performs dormant reattach if the client requested it and the required
     identifiers are present
4. The connection context is initialized with:
   - `autocommit` mode enabled
   - dialect tag from application identity
   - dialect-specific session variables
   - dialect session identity
5. The handler requires a non-empty session user.
6. The handler resolves the user from the catalog.
7. Disabled or missing users fail attach.
8. The handler creates the catalog session and seeds:
   - current schema id
   - current schema path
   - search path
   - session context epochs
9. The handler synchronizes transaction state into the connection context.
10. The handler creates the SBLR executor bound to that connection context.

## Dormant reattach lifecycle at the session edge

Current native driver and adapter code support explicit dormant detach and
explicit dormant reattach.

Current attach ordering for dormant reattach is:

1. client reconnects with dormant reattach enabled and provides:
   - `dormant_id`
   - single-use reattach token
2. server parses those identifiers from `CONNECT_REQUEST`
3. authentication completes first
4. server loads the dormant transaction catalog row
5. server verifies the authenticated user matches the dormant owner or session
   user
6. server calls the database dormant-reattach path
7. if the original in-memory dormant context still exists, that live
   `ConnectionContext` is rebound
8. if the process restarted and replacement reattach is allowed, a replacement
   transaction is opened from persisted session state
9. if restart-time reattach is denied by policy, reattach fails closed and the
   dormant row remains for inspection or cleanup

Dormant reattach is explicit. Disconnect or reconnect by itself never resumes
work.

## Transaction posture at the session edge

Listener and parser session handling must respect the global transaction model:
- ScratchBird is always in a transaction
- `COMMIT` ends the current transaction and immediately starts the next one
- `ROLLBACK` ends the current transaction and immediately starts the next one
- autocommit means a successful statement auto-commits the active transaction
- statement error leaves the current transaction active

The listener and parser do not redefine these rules. They inherit them from
sections `08`, `09`, `35`, and `37`.

## Native parser READY contract

For the native parser family, the current READY response carries:
- `session_id = client_id`
- feature flags:
  - prepared statements
  - copy streaming
  - cancel

That READY surface is part of the current parser/engine edge, not a listener
policy invention.

## SQL-text ownership

The parser worker owns SQL text.

The engine IPC session handler owns execution of prepared engine payloads and
must not be treated as a general SQL parser. Engine-side SQL-text execution
paths are not the section `29` authority model.

## Detach lifecycle

Detach occurs when:
- the client disconnects
- the parser terminates the session
- listener admin kill terminates the worker handling that connection
- shutdown or drain retires the worker and attached session
- explicit dormant-detach is requested for the current transaction

Current dormant-detach rules:
- explicit dormant detach returns:
  - `dormant_id`
  - single-use reattach token
- dormant preservation is for explicit transaction work, not ordinary
  autocommit churn
- if dormant persistence fails, the runtime must fail closed and roll back or
  tear down according to current detach logic

Detached non-dormant sessions must release engine session state and stop
producing outbound IPC messages.
