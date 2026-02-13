# MySQL 8.4 LTS Emulation Specification (Work Tree)

## Scope
Authoritative, no-ambiguity technical specification for server-side emulation of MySQL 8.4 LTS, sufficient for an implementation to interoperate with existing clients and drivers.

## Version Pin
- Target line: MySQL 8.4 LTS (current stable 8.4.x)
- Protocol: MySQL Classic Protocol 4.1+ (protocol v10), prepared statement protocol, binary protocol

## Deliverables
1. SQL grammar and behavior (DDL/DML, stored programs, SQL mode impacts)
2. API surface (client/server commands, capabilities, authentication, session state)
3. Wire protocol (handshake, auth, command packets, prepared statements, compression)
4. Response protocol (OK/ERR/EOF/resultsets, type encodings)

## Structure
- `00_scope_and_compliance.md`
- `01_source_map.md`
- `10_sql_language.md`
- `20_api_surface.md`
- `30_wire_protocol.md`
- `40_response_formats_and_types.md`
- `90_test_vectors.md`
