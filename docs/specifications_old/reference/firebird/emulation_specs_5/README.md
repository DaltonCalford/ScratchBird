# Firebird 5 Emulation Specification (Standalone)

## Scope
Authoritative, no-ambiguity technical specification for server-side emulation of Firebird 5.x, sufficient for an implementation to interoperate with existing clients and drivers.

This spec is self-contained and intended for emulator implementation, not for end-user guidance.

## Version Pinning
- Target line: Firebird 5.0.x (current stable minor 5.0.3 as of July 11, 2025)
- Wire protocol: Firebird protocol versions 10–20 (current protocol version 20)

## Deliverables
1. SQL grammar and behavior (DDL/DML/PSQL, dialect specifics, edge cases)
2. API surface (service manager, attachment, transaction, statement, cursor, blob, info, events)
3. Wire protocol (packet types, encoding, negotiation, auth, batching, compression, crypt)
4. Response protocol (message formats, row encoding, data type representations, errors/warnings)

## Structure
- `01_source_map.md` — internal specification map
- `10_sql_language.md` — full SQL grammar and behavior
- `20_api_surface.md` — API/operation semantics and lifecycles
- `30_wire_protocol.md` — byte-level protocol specification
- `40_response_formats_and_types.md` — data types, responses, status vectors
- `90_test_vectors.md` — wire-level examples and expected outcomes
- `appendix_protocol_constants.md` — protocol versions, opcodes, flags, architectures, fetch constants
- `appendix_protocol_structs.md` — canonical packet field layouts
- `appendix_protocol_fields.md` — per‑opcode field order
- `appendix_protocol_state_machine.md` — protocol state transitions
- `appendix_time_zones.md` — time zone ID to name mapping
- `appendix_parameter_blocks.md` — DPB/TPB/BPB/SPB and service action constants
- `appendix_status_vector_constants.md` — status vector argument tags
- `appendix_error_codes.md` — isc_* error code values
- `appendix_blr_message_layout.md` — BLR message parsing and rem_fmt layout
- `appendix_transport_framing.md` — TCP/XDR stream framing rules
- `appendix_catalog_bootstrap.md` — minimum system catalog image and numeric IDs
- `appendix_security_classes.md` — security class ACL templates and generation order
- `appendix_system_triggers.md` — system triggers and trigger messages (BLR and text)
- `appendix_ods_invariants.md` — on‑disk structure invariants and page layouts
- `appendix_privilege_enforcement_map.md` — DDL/DML privilege enforcement and error mapping
- `appendix_builtins_sysfunctions.md` — built‑in function list with arity/determinism
- `appendix_builtins_system_packages.md` — system package list
- `appendix_builtins_semantics.md` — built‑in function behavior definitions

## Status
Standalone and authoritative. Any remaining ambiguities should be resolved by expanding the local spec files and formal schemas.
