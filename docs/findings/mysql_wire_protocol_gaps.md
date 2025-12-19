# MySQL Wire Protocol Compatibility Review (Findings)

Goal: 100% remote network compatibility for MySQL clients against ScratchBird server. This document enumerates potential gaps/unclear areas to verify in `docs/specifications/wire_protocols/mysql_wire_protocol.md`.

## Potential Gaps / Clarifications Needed
- **Full capability matrix**: capability flags list is partial; need full set and exact server behavior per flag.
- **OK packet/session state tracking**: OK packet fields for CLIENT_SESSION_TRACK and server status flags need explicit specification.
- **Auth plugin flows**: caching_sha2_password full auth (RSA key exchange) and fast auth handling details are incomplete.
- **Multi-result sets**: COM_QUERY with multiple results and CLIENT_MULTI_RESULTS behaviors need explicit framing.
- **COM_STMT_SEND_LONG_DATA**: long data parameter streaming not detailed.
- **COM_STMT_FETCH / cursor protocol**: cursor behavior and status flags need clearer definitions.
- **LOCAL INFILE**: CLIENT_LOCAL_FILES and file transfer framing not defined.
- **Change user (COM_CHANGE_USER)**: re-authentication flow and capability implications not fully described.
- **Reset connection (COM_RESET_CONNECTION)**: session state reset semantics not defined.
- **Compression edge cases**: compressed packet boundaries and error recovery need explicit rules.
- **Replication details**: binlog event coverage appears minimal; GTID-based dump (COM_BINLOG_DUMP_GTID) not described.

## Decisions / Constraints (Alpha)
- **Compatibility target**: Any native MySQL client must connect and be fully supported.\n+- **Replication**: Not required for emulated engines in Alpha; replication is deferred.\n+- **Legacy migration**: Passthrough/migration is handled by MySQL UDRs that connect to legacy databases.

## Suggested Validation Matrix
- **Handshake**: all capabilities negotiated correctly, SSL vs non-SSL.
- **Auth**: mysql_native_password, caching_sha2_password.
- **Query**: text protocol and binary protocol with prepared statements.
- **Result sets**: EOF vs OK (CLIENT_DEPRECATE_EOF), multi-results.
- **Session**: COM_CHANGE_USER, COM_RESET_CONNECTION, connection attributes.
- **Replication**: COM_BINLOG_DUMP and GTID if enabled.
