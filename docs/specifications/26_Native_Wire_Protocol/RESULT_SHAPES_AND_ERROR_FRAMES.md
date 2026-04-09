# Result Shapes and Error Frames

## Native result frames

Current native result path uses:
- `QUERY_RESULT`
- `ROW_DESCRIPTION`
- `ROW_DATA`
- `END_OF_RESULTS`
- `COMMAND_COMPLETE`

Current native non-row outcomes may also use:
- `QUERY_ERROR`
- `PORTAL_SUSPENDED`
- `NOTIFICATION`
- `QUERY_PROGRESS`

## IPC result frames

Current IPC result path uses:
- `ROW_DESCRIPTION`
- `DATA_ROW`
- `DATA_BATCH`
- `COMMAND_COMPLETE`
- `EMPTY_RESPONSE`
- `PARSE_COMPLETE`
- `BIND_COMPLETE`
- `CLOSE_COMPLETE`
- `READY_FOR_QUERY`

## Error frame authority

Current error lanes are:
- native: `QUERY_ERROR` and `PROTOCOL_ERROR`
- IPC: `ERROR_RESPONSE`
- IPC informational lane: `NOTICE`

Current section `26` authority is the transport-level existence and separation
of these frames. Semantic error domains, SQLSTATE mapping, and parser or engine
error ownership remain cross-section concerns.

## Completion frames

Current completion authority includes:
- native `COMMAND_COMPLETE`
- IPC `COMMAND_COMPLETE`
- IPC `TXN_COMPLETE`
- native and IPC copy completion frames where declared

## Negative requirements

- do not invent a single universal `ERROR_FRAME` if the current profiles do not
  use one
- do not claim every result-shape source in adjacent sections already has full
  parity across native and IPC profiles
