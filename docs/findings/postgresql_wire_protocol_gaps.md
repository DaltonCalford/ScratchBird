# PostgreSQL Wire Protocol Compatibility Review (Findings)

Goal: 100% remote network compatibility for PostgreSQL clients against ScratchBird server. This document enumerates potential gaps/unclear areas to verify in `docs/specifications/wire_protocols/postgresql_wire_protocol.md`.

## Potential Gaps / Clarifications Needed
- **ParameterStatus set**: server must send all required runtime parameters (server_version, client_encoding, DateStyle, TimeZone, etc.). Not enumerated.
- **BackendKeyData + CancelRequest**: CancelRequest flow is listed, but required BackendKeyData message handling should be explicit.
- **ErrorResponse fields**: Error/Notice fields and severity codes need full mapping for client compatibility.
- **Authentication methods**: SCRAM-SHA-256 is listed, but channel binding (SCRAM-PLUS) details and SASL extensions need coverage.
- **Extended query protocol edge cases**: portal lifecycle, unnamed statement behavior, parameter format handling, and Describe semantics.
- **COPY protocol**: COPY IN/OUT/BOTH framing must cover formats, header/trailer, and error recovery.
- **Type OIDs**: list of common OIDs exists, but server-side mapping of custom types, arrays, domains needs clarity.
- **Replication protocol**: logical replication commands, replication slots, timeline handling not covered.
- **Startup parameters**: full set of supported startup parameters and reject/ignore rules not defined.

## Decisions / Constraints (Alpha)
- **Compatibility target**: Any native PostgreSQL client must connect and be fully supported.\n+- **Replication**: Not required for emulated engines in Alpha; replication is deferred.\n+- **Legacy migration**: Passthrough/migration is handled by PostgreSQL UDRs that connect to legacy databases.

## Suggested Validation Matrix
- **Startup**: SSLRequest, startup parameters, ParameterStatus.
- **Auth**: MD5, SCRAM, optional GSS/SSPI if enabled.
- **Simple query**: full message flow and error recovery.
- **Extended query**: Parse/Bind/Execute/Describe with multiple portals.
- **COPY**: IN/OUT/BOTH with large payloads.
- **Cancel**: BackendKeyData + CancelRequest behavior.
- **Replication**: streaming protocol (if enabled).
