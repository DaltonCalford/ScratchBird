# Parser Agent Specification

Version: 1.0
Status: Draft (Alpha IP layer)
Last Updated: January 2026

## Purpose

Define the parser agent binaries used by network listeners. Each parser agent
implements one protocol dialect and translates client SQL to SBLR for execution
by the ScratchBird engine.

## Scope

- One parser agent binary per dialect
- Socket handoff from listener
- Protocol handshake, TLS, authentication extraction
- SBLR generation and IPC calls to engine

Out of scope:
- Listener control-plane protocol (see CONTROL_PLANE_PROTOCOL_SPEC.md)
- Engine internals

## Binaries

- sb_parser_native
- sb_parser_pg
- sb_parser_mysql
- sb_parser_fb

Each binary handles exactly one dialect and is pooled by its matching listener.

## Responsibilities

Parser agents:
- Perform TLS handshake with client (listener never terminates TLS)
- Perform protocol handshake and extract auth proof
- Submit auth proof to engine for validation
- Parse SQL and emit SBLR bytecode
- Send SBLR + original SQL to engine over IPC
- Return results and errors to the client

Parser agents do NOT:
- Validate credentials locally
- Execute SQL locally
- Bypass engine authorization or bytecode validation

## Lifecycle

- Parser agent is launched by the listener pool manager.
- One agent serves one client connection at a time.
- After session end, agent is recycled or terminated per pool policy.

## Command-Line Interface

Required flags:

```
--control-socket <path>     Control-plane socket for listener handoff
--engine-endpoint <path>    Engine IPC endpoint (ScratchBird native protocol)
--log-level <level>         info|debug|warn|error
--protocol-version <n>      Protocol version for the dialect
```

Optional:

```
--tls-config <file>         TLS configuration (certs, keys, policy)
--max-requests <n>          Max sessions before recycle
--max-age-seconds <n>       Max lifetime before recycle
```

## IPC Contract (Parser <-> Engine)

Parser agents MUST follow:
- docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md

## Security

- TLS termination occurs in the parser agent.
- Parser agents are untrusted; engine enforces all permissions.
- Parser agents must propagate auth context from engine responses.

## Related Specs

- docs/specifications/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md
- docs/specifications/network/CONTROL_PLANE_PROTOCOL_SPEC.md
- docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md
- docs/specifications/wire_protocols/*.md
