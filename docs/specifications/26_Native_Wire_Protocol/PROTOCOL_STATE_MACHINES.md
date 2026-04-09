# Protocol State Machines

## Native wire connection state machine

Current native wire ordering is:
1. client sends `CONNECT_REQUEST`
   - request may carry:
     - database name
     - client identity
     - connect flags
     - optional bound database UUID
     - optional dormant reattach identifiers when dormant reattach is requested
2. server returns `CONNECT_RESPONSE`
3. if authentication is required, auth exchange proceeds through:
   - `AUTH_REQUEST`
   - `AUTH_RESPONSE`
   - optional `AUTH_CHALLENGE`
4. after authentication the server either:
   - opens a fresh connection context
   - or executes dormant reattach if the connect flags requested it
5. authenticated session may issue query, prepare, execute, transaction, and
   administrative messages
6. session ends with `DISCONNECT` or fatal protocol close

### Dormant reattach branch

When `CONNECT_FLAG_DORMANT_REATTACH` is present:

1. `CONNECT_REQUEST` must carry:
   - `dormant_id`
   - single-use dormant reattach authkey
2. the server validates the connect framing first
3. authentication still runs before reattach
4. the server verifies the dormant row belongs to the authenticated principal
5. the server attempts dormant reattach
6. current outcomes are:
   - rebind live dormant context
   - replacement reattach after restart when policy allows
   - fail closed

Disconnect or reconnect without that explicit flag and identifiers never resumes
the dormant transaction.

### Manager-proxy boundary

`DBBT` and `LPREFACE` validation are not part of the public native wire
transcript. They are pre-connect control-plane steps between the optional
manager and the internal native listener.

Fatal close conditions include:
- invalid header
- unsupported version
- oversized payload
- malformed or unknown fatal message handling path

## IPC session state machine

Current IPC ordering is:
1. `STARTUP`
2. `READY`
3. optional `FEATURE_NEGOTIATE`
4. `ATTACH`
5. `ATTACHED`
6. query and statement lifecycle:
   - `SIMPLE_QUERY`
   - or `PARSE -> BIND -> EXECUTE -> CLOSE`
   - or compiled-query message path
7. result or completion messages
8. `DETACH` or `TERMINATE`

## IPC transaction state ordering

Current IPC transaction messages are:
- `TXN_BEGIN`
- `TXN_COMMIT`
- `TXN_ROLLBACK`
- `SAVEPOINT`
- `RELEASE`
- `ROLLBACK_TO`
- `TXN_COMPLETE`

These transport messages do not redefine transaction semantics. The always-in-
transaction MGA lifecycle is owned by sections `08`, `09`, and `35`.

## Result sequencing

Current native result sequencing uses:
- `QUERY_RESULT`
- `ROW_DESCRIPTION`
- zero or more `ROW_DATA`
- `END_OF_RESULTS`
- `COMMAND_COMPLETE`

Current IPC result sequencing uses:
- `ROW_DESCRIPTION`
- zero or more `DATA_ROW` or `DATA_BATCH`
- `COMMAND_COMPLETE`
- optional `READY_FOR_QUERY`

Error responses terminate or short-circuit the active request path according to
current profile behavior.

Administrative status inspection for derivative queues, shadow groups, restore
boundaries, and failback boundaries follows the same ordinary admin or result
sequencing model:
- request message or command
- `ROW_DESCRIPTION` or status payload metadata
- zero or more data rows or one bounded status payload
- `COMMAND_COMPLETE` or equivalent success response

These inspections do not create a separate recovery or derivative stream state
machine.

Dormant-detach and dormant-reattach administrative flows also stay inside the
ordinary protocol family:
- no separate reattach transport profile exists
- no replay-session stream is created
- dormant identifiers ride inside the existing connect or result payload shapes

## Copy and stream sequencing

Current copy and stream sequencing is limited to the exact native and IPC
message families declared in the current headers and message catalog.

This section certifies:
- the declared message families exist
- the documented ordering constraints for those families exist
- unsupported families or parity claims must fail closed

This section does not certify:
- full native and IPC parity for every copy mode
- replay-session streaming
- parserless cluster-fabric streaming
- distributed-read or telemetry stream sequencing
- dedicated derivative-status or shadow-status streaming

## Hard boundaries

- no replay-session state machine is current authority here
- no parserless cluster-fabric state machine is current authority here
- no distributed read or telemetry transport state machine is current authority
  here
