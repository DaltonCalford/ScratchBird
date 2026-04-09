# Service Channels and Streaming

## Current native service or streaming families

Current native header declares:
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

Current native capability bits also declare:
- copy support
- LOB stream support
- portal paging
- notifications
- progress

## Current IPC service or streaming families

Current IPC contract declares:
- `COPY_IN_REQUEST`
- `COPY_OUT_RESPONSE`
- `COPY_DATA`
- `COPY_DONE`
- `COPY_FAIL`
- `COPY_COMPLETE`
- `STREAM_CONTROL`

## Current authority boundary

This section certifies:
- these message families exist in the current transport contracts
- their message ids and payload ownership boundaries are fixed by the current
  headers
- `STREAM_CONTROL` is the only current flow-control surface explicitly declared
  by both current transport profiles
- derivative queue, shadow-group, restore-boundary, and failback-boundary
  inspection do not create a new streaming family under current authority;
  they ride on existing administrative or ordinary result paths

This section does not certify:
- full native and IPC parity for every copy mode
- complete service-channel equivalence across every adapter
- replay, distributed-read, or telemetry stream families
- multi-stream fairness, credit windows, or generalized backpressure contracts
  beyond the declared `STREAM_CONTROL` surface
- dedicated derivative-delivery or shadow-group push streams

## Negative requirements

- do not infer additional service-channel families from capability bits alone
- do not claim replay or telemetry stream support under current section `26`
  authority
- do not invent richer flow-control guarantees than the current declared
  `STREAM_CONTROL` interaction
- do not invent a dedicated wire stream for derivative queue-state, shadow-group
  state, or restore-boundary status under current authority
