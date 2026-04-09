# Message Catalog and Schemas

## Native wire message catalog

Current native `MessageType` families are:

### Connection lifecycle
- `CONNECT_REQUEST`
- `CONNECT_RESPONSE`
- `DISCONNECT`

### Authentication
- `AUTH_REQUEST`
- `AUTH_RESPONSE`
- `AUTH_CHALLENGE`
- `SUBSCRIBE`
- `UNSUBSCRIBE`

### Query execution
- `QUERY`
- `QUERY_RESULT`
- `QUERY_ERROR`
- `QUERY_CANCEL`

### Prepared statements
- `PREPARE`
- `PREPARE_RESPONSE`
- `EXECUTE`
- `CLOSE_STATEMENT`
- `DESCRIBE`
- `DESCRIBE_RESPONSE`

### Transactions
- `BEGIN_TRANSACTION`
- `COMMIT`
- `ROLLBACK`
- `SAVEPOINT`
- `RELEASE_SAVEPOINT`
- `ROLLBACK_TO`
- `TRANSACTION_STATUS`

### Result streaming
- `ROW_DESCRIPTION`
- `ROW_DATA`
- `END_OF_RESULTS`
- `COMMAND_COMPLETE`
- `PORTAL_SUSPENDED`
- `NOTIFICATION`
- `QUERY_PROGRESS`

### Administrative
- `SHUTDOWN`
- `PING`
- `PONG`
- `STATUS_REQUEST`
- `STATUS_RESPONSE`
- `MCP_HELLO`
- `MCP_AUTH_START`
- `MCP_AUTH_CONTINUE`
- `MCP_DB_LIST`
- `MCP_DB_CONNECT`
- `MCP_DB_INFO`
- `DORMANT_DETACH`
- `DORMANT_DETACH_RESULT`

### Streaming and copy
- `COPY_DATA`
- `COPY_DONE`
- `COPY_FAIL`
- `COPY_IN_RESPONSE`
- `COPY_OUT_RESPONSE`
- `COPY_BOTH_RESPONSE`
- `STREAM_CONTROL`
- `STREAM_READY`
- `STREAM_DATA`
- `STREAM_END`

### Internal and fatal
- `DEBUG_MESSAGE`
- `PROTOCOL_ERROR`

## Current native payload authorities

The native header file is the current payload schema authority for:
- `CONNECT_REQUEST`
- `CONNECT_RESPONSE`
- `QUERY_RESULT`
- `ROW_DESCRIPTION`
- `ROW_DATA`
- `COMMAND_COMPLETE`

Administrative inspection of derivative queues, shadow groups, restore
boundaries, and failback boundaries uses existing status or ordinary result
payload families. Under current authority it does not introduce a new
message-type family.

When surfaced through `STATUS_RESPONSE`, the payload MUST preserve deterministic
field names for:
- derivative backpressure class
- derivative queue profile count
- shadow-group state
- shadow-group ready-member count
- shadow-group required-member count
- last restore boundary identifier when present
- last failback boundary identifier when present

When surfaced through ordinary query/admin result sets, the payload MUST use the
fixed rowset contracts defined by section `30`.

The native adapter and client tooling are the current profile-specific behavior
authorities for how these payloads are emitted and consumed.

## IPC message catalog

Current `IPCMessageType` families are:

### Connection management
- `STARTUP`
- `READY`
- `FEATURE_NEGOTIATE`
- `TERMINATE`
- `PING`
- `PONG`

### Session management
- `ATTACH`
- `DETACH`
- `ATTACHED`
- `DETACHED`

### Query execution
- `SIMPLE_QUERY`
- `PARSE`
- `BIND`
- `DESCRIBE`
- `EXECUTE`
- `CLOSE`
- `SYNC`
- `COMPILED_QUERY`
- `COMPILED_PARSE`

### Results
- `ROW_DESCRIPTION`
- `DATA_ROW`
- `DATA_BATCH`
- `COMMAND_COMPLETE`
- `EMPTY_RESPONSE`
- `PARSE_COMPLETE`
- `BIND_COMPLETE`
- `CLOSE_COMPLETE`
- `READY_FOR_QUERY`

### Copy and streaming
- `COPY_IN_REQUEST`
- `COPY_OUT_RESPONSE`
- `COPY_DATA`
- `COPY_DONE`
- `COPY_FAIL`
- `COPY_COMPLETE`
- `STREAM_CONTROL`

### Transactions
- `TXN_BEGIN`
- `TXN_COMMIT`
- `TXN_ROLLBACK`
- `SAVEPOINT`
- `RELEASE`
- `ROLLBACK_TO`
- `TXN_COMPLETE`

### Async and error lanes
- `NOTIFY_SUBSCRIBE`
- `NOTIFY_UNSUBSCRIBE`
- `NOTIFY_DELIVER`
- `CANCEL_REQUEST`
- `CANCEL_ACK`
- `ERROR_RESPONSE`
- `NOTICE`
- `HEARTBEAT`
- `SHUTDOWN`

## Current IPC payload-schema authority

Current `ipc_contract_v1_1.h` is the active schema authority for:
- startup payloads
- ready payloads
- feature negotiation payloads
- parse, bind, execute, and close payloads
- row-description and data-row payloads
- copy payloads
- stream-control payloads
- error-response payloads

Under current authority, derivative queue-state and shadow-group inspection in
IPC also ride through existing result or status payload paths rather than a new
IPC message family.

## Hard boundaries

- current section `26` does not define cluster-fabric `0x08xx` runtime messages
- current section `26` does not define replay-session transport payloads
- current section `26` does not define a complete distributed telemetry message
  family
- current section `26` does not define a dedicated derivative-delivery transport
  family
- current section `26` does not define a dedicated shadow-group event stream
