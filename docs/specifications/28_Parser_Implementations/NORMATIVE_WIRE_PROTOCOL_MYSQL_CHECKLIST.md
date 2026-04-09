# Normative Checklist: MySQL Wire Protocol Adapter (Alpha)

## Purpose
Define deterministic implementation requirements for MySQL protocol emulation in the parser layer for MySQL 8.x compatibility profiles.

## Scope
- Connection phase handshake and authentication packets.
- Command phase packet handling.
- Prepared statement and resultset flow mapping.

## Hard Invariants
1. Engine never speaks MySQL wire protocol.
2. Parser performs all packet framing, capability negotiation, and auth exchange.
3. Parser converts commands to canonical AST/SBLR with UUID binding.
4. Parser profile controls feature exposure and reject paths.
5. Parser never executes SQL semantics locally.

## Alpha Wire Profile
- Packet header:
  - payload length: 3-byte little-endian
  - sequence id: 1 byte
- Max packet payload: `MAX_PACKET_LENGTH = 16,777,215` bytes.
- Handshake protocol:
  - server handshake `Protocol::HandshakeV10` (protocol version 10).
  - client response `Protocol::HandshakeResponse41`.
- Required capability handling includes:
  - `CLIENT_PROTOCOL_41`
  - `CLIENT_SSL` (when enabled)
  - `CLIENT_PLUGIN_AUTH`
  - `CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA`
  - `CLIENT_CONNECT_ATTRS`
  - `CLIENT_COMPRESS` / compression profile settings
  - `CLIENT_DEPRECATE_EOF` behavior.

## Connection Phase Contract
1. Parser sends handshake v10 packet with server capabilities and auth plugin data.
2. Client may send SSL request packet before handshake response when `CLIENT_SSL` is set.
3. Parser performs plugin-auth exchange until terminal packet:
   - success: OK packet
   - failure: ERR packet and close.
4. Parser initializes session context only after auth success.

## Command Phase Contract

### Required Command Coverage
- `COM_QUERY`
- `COM_INIT_DB`
- `COM_PING`
- `COM_CHANGE_USER`
- `COM_RESET_CONNECTION`
- `COM_STMT_PREPARE`
- `COM_STMT_EXECUTE`
- `COM_STMT_SEND_LONG_DATA`
- `COM_STMT_CLOSE`
- `COM_STMT_RESET`
- `COM_STMT_FETCH`

### Command Mapping Rules
- `COM_QUERY`:
  - parse text SQL
  - capability gate
  - UUID bind
  - emit SBLR
- `COM_STMT_*`:
  - maintain parser-side prepared-statement id map
  - bind parameters deterministically using type metadata
  - map execute/fetch results to binary protocol responses.
- `COM_CHANGE_USER` and `COM_RESET_CONNECTION`:
  - reinitialize session-scoped parser caches as required by profile.

## Response Contract
- Parser MUST return one of deterministic packet families:
  - OK packet
  - ERR packet
  - Resultset packet sequence
- Resultset sequence MUST include deterministic metadata and row ordering.
- EOF deprecation behavior MUST follow negotiated capabilities.
- Server status flags MUST be mapped consistently for transaction and cursor state.

## Error Mapping Contract
- Parser and engine failures map to MySQL error code + SQLSTATE + message tuple.
- Unmapped failures MUST use deterministic generic MySQL mapping.
- Security and discoverability policy MUST hide protected object existence.

## Implementation Checklist

### MYW00 Packet Framing
- [ ] Implement 4-byte MySQL packet header decode/encode.
- [ ] Validate sequence-id ordering and reject out-of-order packets.
- [ ] Enforce max packet payload size limits.

Pass condition:
- Packet framing is deterministic and desynchronization-safe.

### MYW01 Handshake and Auth
- [ ] Implement handshake v10 packet emission.
- [ ] Implement SSL request transition when negotiated.
- [ ] Implement plugin-auth exchange and terminal OK/ERR handling.

Pass condition:
- Connection phase is protocol-correct for success and failure paths.

### MYW02 Capability Negotiation
- [ ] Compute effective capabilities from parser profile and client flags intersection.
- [ ] Persist effective capability bitmap in immutable session context.
- [ ] Reject commands requiring unavailable capabilities.

Pass condition:
- Capability behavior is deterministic and auditable.

### MYW03 Text Query Path
- [ ] Implement `COM_QUERY` to canonical parse -> UUID bind -> SBLR emit path.
- [ ] Map engine results to MySQL text resultset packet sequence.
- [ ] Map engine errors to MySQL ERR packet format.

Pass condition:
- Query path is deterministic and profile-compliant.

### MYW04 Prepared Statement Path
- [ ] Implement `COM_STMT_PREPARE` and statement id allocation.
- [ ] Implement parameter and column metadata mapping.
- [ ] Implement `COM_STMT_EXECUTE`, `COM_STMT_FETCH`, and long-data path.
- [ ] Implement `COM_STMT_CLOSE` and `COM_STMT_RESET` lifecycle rules.

Pass condition:
- Prepared-statement protocol is state-correct and deterministic.

### MYW05 Session Reset/Change User
- [ ] Implement `COM_CHANGE_USER` re-auth flow and context replacement.
- [ ] Implement `COM_RESET_CONNECTION` session reset semantics.
- [ ] Ensure parser caches and prepared statements are reset according to profile rules.

Pass condition:
- Session transitions are safe and deterministic.

### MYW06 Response and Status Flags
- [ ] Implement OK/ERR/resultset encoders.
- [ ] Populate server status flags consistently.
- [ ] Support EOF-deprecation behavior based on negotiated capability.

Pass condition:
- Response packet family and status flags are protocol-correct for all command classes.

## Negative Requirements
- Parser MUST NOT bypass engine privilege checks.
- Parser MUST NOT emit native ScratchBird-only protocol concepts on MySQL wire.
- Parser MUST NOT rely on heuristic text matching for error mapping.

## Conformance Gates
- `P28-MYW-GATE-01`: framing/sequence and packet-size tests pass.
- `P28-MYW-GATE-02`: handshake/auth/capability negotiation tests pass.
- `P28-MYW-GATE-03`: query and prepared statement lifecycle tests pass.
- `P28-MYW-GATE-04`: response/status/error mapping tests pass.

## Evidence Artifacts
- `docs/specifications/work/conformance/wire/mysql/HANDSHAKE_AND_CAPABILITY_RESULTS.json`
- `docs/specifications/work/conformance/wire/mysql/COMMAND_MAPPING_RESULTS.csv`
- `docs/specifications/work/conformance/wire/mysql/PREPARED_STATEMENT_LIFECYCLE_RESULTS.csv`
- `docs/specifications/work/conformance/wire/mysql/ERROR_MAPPING_RESULTS.csv`

## Cross-Section Links
- `docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`
- `docs/specifications/30_Client_Tooling/README.md`

## Audit normalization note (2026-03-28)
- This file is retained as a checklist, not as stand-alone proof of full protocol parity.
- Current code-backed evidence in section `28` is limited to the shipped parser-agent and listener subset for this family.
- Exhaustive protocol fidelity, complete message-family coverage, and full production parity remain bounded and require family-local runtime evidence beyond this section-level checklist.

## Hardening promotion note (2026-03-28)
- section `28` now carries explicit capability-state vocabulary for parser implementation proof lanes:
  - `supported_native_v3`
  - `supported_emulated_sql_family`
  - `supported_scaffold_or_udr_boundary`
  - `bounded_shipped_front_door`
  - `checklist_only`
  - `target_state_only`
  - `fail_closed`
- dedicated parser-family proof must be anchored to live parser code plus shipped parser-agent or listener/runtime seams, not to checklist presence alone
- native-V3 internal feature vocabulary must not be promoted into dedicated external parser-family parity without family-local source proof
- universal capability-profile generation, universal corpus cardinality, and universal wire parity claims remain non-authoritative unless backed by generated or runtime evidence
