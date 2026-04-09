# Normative Listener Implementation Checklist

An implementation is conformant to current section `29` only if all of the
following are true.

## Server-side launch

- `ServiceController` can build listener profiles from `network` defaults.
- Explicit `listener.<name>` sections replace the legacy per-family path.
- Each launched listener is bound to an owner database and engine endpoint.
- Manager-proxy launch uses the documented manager keys only.

## Listener startup

- listener validates mode and pool bounds before admission
- managed mode requires loopback bind
- direct mode rejects proxy-binding enforcement
- local control and management sockets are started before client admission
- warm worker minimum is satisfied before listener startup completes

## Runtime control plane

- control-plane framing uses the shipped message enum set
- worker registration is `HELLO` / `HELLO_ACK`
- socket handoff uses `HANDOFF_SOCKET` / `HANDOFF_ACK`
- managed mode uses `DBBT` and `LPREFACE`
- local admin uses `MANAGEMENT_COMMAND` / `MANAGEMENT_RESPONSE`

## Pool behavior

- worker states are `IDLE`, `BUSY`, `DRAINING`, `FAULT`
- the pool never assigns draining or faulted workers new sessions
- runtime resize honors `pool_min <= pool_max`
- admin kill terminates the owning worker and replenishes minimum capacity

## Parser and engine edge

- parser workers own SQL text and protocol negotiation
- engine IPC session handler owns session attach and prepared execution
- engine attach requires a valid active user and catalog session
- attach seeds schema and search-path context from the catalog

## Unsupported features

The current implementation must not claim support for:
- live migration
- dual execution mirror
- one-way or bidirectional replication runtime
- parserless cluster fabric
