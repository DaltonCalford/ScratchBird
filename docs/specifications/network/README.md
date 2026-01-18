# Network Layer Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains network layer and wire protocol specifications for ScratchBird's client-server communication.

## Overview

ScratchBird implements a sophisticated network layer with the Y-Valve architecture for multi-protocol support, enabling clients to connect using PostgreSQL, MySQL, Firebird, or ScratchBird native protocols (MSSQL/TDS post-gold).

## Specifications in this Directory

- **[NETWORK_LAYER_SPEC.md](NETWORK_LAYER_SPEC.md)** - Network layer architecture and implementation
- **[NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md](NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md)** - Listener startup, parser pools, and socket handoff
- **[WIRE_PROTOCOL_SPECIFICATIONS.md](WIRE_PROTOCOL_SPECIFICATIONS.md)** - Wire protocol specifications overview
- **[Y_VALVE_DESIGN_PRINCIPLES.md](Y_VALVE_DESIGN_PRINCIPLES.md)** - Y-Valve multi-protocol routing design

## Key Concepts

### Y-Valve Architecture

The Y-Valve is ScratchBird's protocol multiplexer:

1. **Protocol Detection** - Auto-detect client protocol on connection
2. **Protocol Routing** - Route to appropriate protocol handler
3. **Unified Backend** - All protocols map to common SBLR bytecode

### Supported Wire Protocols

- **PostgreSQL Wire Protocol** - Full libpq compatibility
- **MySQL Wire Protocol** - MySQL client/server protocol
- **Firebird Wire Protocol** - Firebird remote protocol
- **TDS (MSSQL) Protocol** - SQL Server TDS protocol (post-gold)
- **ScratchBird Native** - Optimized native protocol

See [Wire Protocols](../wire_protocols/) for detailed protocol specifications.

## Related Specifications

- [Wire Protocols](../wire_protocols/) - Detailed wire protocol specifications
- [Parser](../parser/) - SQL dialect parsing
- [Security](../Security%20Design%20Specification/) - Authentication and encryption
- [Core](../core/) - Y-Valve implementation

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Related:** [Wire Protocols](../wire_protocols/README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
