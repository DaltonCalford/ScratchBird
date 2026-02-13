# Network Layer Specifications

Status: Authoritative (V3)
Last Updated: 2026-02-08

This directory contains network layer and wire protocol specifications for
ScratchBird's client-server communication. Only files listed in
`docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are normative.

## Overview

ScratchBird implements a listener + parser pool architecture for multi-protocol
support, enabling clients to connect using PostgreSQL, MySQL, Firebird, or
ScratchBird native protocols. MSSQL/TDS is not supported in V3 and MUST be
rejected.

## Specifications in this Directory

- `NETWORK_LAYER_SPEC.md` - Network layer architecture and connection lifecycle
- `NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md` - Listener startup, parser pools, socket handoff
- `CONTROL_PLANE_PROTOCOL_SPEC.md` - Listener <-> parser control-plane protocol
- `PARSER_AGENT_SPEC.md` - Parser agent binaries and lifecycle
- `ENGINE_PARSER_IPC_CONTRACT.md` - Parser <-> engine IPC runtime contract
- `DIALECT_AUTH_MAPPING_SPEC.md` - Dialect auth method mapping to SB auth providers
- `WIRE_PROTOCOL_SPECIFICATIONS.md` - Wire protocol compatibility baseline
- `Y_VALVE_DESIGN_PRINCIPLES.md` - Listener/pool design principles (legacy terminology normalized)

## Supported Wire Protocols

- PostgreSQL 16+
- MySQL 8.x
- Firebird 5.x
- ScratchBird native

## Rejected Protocols

- TDS/MSSQL (MUST reject with `ERR_FEATURE_DISABLED`)

## Related Specifications

- `docs/specifications/parser/v3/wire_protocols/` (protocol-specific wire details)
- `docs/specifications/parser/v3/operations/` (monitoring and metrics)
- `docs/specifications/parser/v3/security/` (auth/crypto references)
