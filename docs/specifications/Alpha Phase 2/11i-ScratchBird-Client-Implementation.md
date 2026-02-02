# ScratchBird Client Implementation

ScratchBird wire protocol client adapter for Remote Database UDR.

## 1. Scope
- Protocol: ScratchBird native wire protocol (SBWP v1.1)
- Transport: TCP with TLS
- Untrusted connector: cannot use cluster PKI or federation features

## 2. Connection and Authentication
- Use AUTH_PASSWORD or configured auth method from SB protocol spec
- Require TLS unless explicitly disabled for local dev
- Use standard ScratchBird startup and parameter negotiation

## 3. Command Execution
- Use prepared statements and portal paging per SBWP
- Support COPY/streaming per SBWP

## 4. Cancellation
- Use SBWP cancel message (see wire protocol spec)

## 5. Type Mapping
- SB types map 1:1 to ScratchBird types

## 6. Schema Introspection
- Use sys.* catalogs (sys.tables, sys.columns, sys.indexes, sys.foreign_keys)

## 7. References
- docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md

