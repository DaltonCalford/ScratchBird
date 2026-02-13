# Firebird 5 Specification Map (Internal Only)

This file lists the authoritative inputs **within this directory**. Do not reference any external source trees.

## SQL Language & PSQL
- `10_sql_language.md` — normative SQL/PSQL behavior, rules, and error handling.
- `10_sql_grammar_full.y` — complete grammar (Bison/Yacc) embedded in the spec set.
- `formal/sql_grammar.json` — machine‑readable grammar graph for automation.
- `formal/sql_tokens.json` — token inventory and keyword list.
- `formal/sql_nonterminals.json` — nonterminal index.

## Wire Protocol (Remote API)
- `30_wire_protocol.md` — transport, handshake, opcodes, message shapes, state transitions.
- `formal/protocol_fields.json` — per‑opcode field ordering (authoritative).
- `appendix_protocol_fields.md` — per‑opcode field order (human‑readable).
- `formal/protocol_state_machine.json` — legal state transitions and responses.
- `appendix_protocol_state_machine.md` — state machine table (authoritative, human‑readable).
- `40_info_items_full.h` — full info item IDs and constants (authoritative enumeration set).
- `appendix_protocol_constants.md` — protocol versions, opcodes, flags, architectures, and fetch constants.
- `appendix_protocol_structs.md` — canonical packet field layouts (authoritative).

## API Surface (Attachment/Transaction/Statement/Blob/Service)
- `20_api_surface.md` — lifecycle rules, method semantics, and error behavior.
- `appendix_parameter_blocks.md` — DPB/TPB/BPB/SPB and service action constants.

## Responses & Data Types
- `40_response_formats_and_types.md` — type encodings, status vector format, response packets.
- `formal/datatypes.json` — datatype codes, sizes, and wire encodings.
- `appendix_time_zones.md` — time zone ID to name mapping (authoritative).
- `appendix_status_vector_constants.md` — status vector argument tags.
- `appendix_error_codes.md` — isc_* error code values.

## Catalog & Built‑ins
- `50_catalog.md` — system tables, fields, and invariants.
- `formal/catalog.json` — machine‑readable catalog schema.
- `60_builtins.md` — built‑in functions, procedures, packages.
- `formal/builtins_sysfunctions.json`
- `formal/builtins_system_packages.json`
- `appendix_catalog_bootstrap.md` — minimum system catalog image and numeric IDs.
- `appendix_security_classes.md` — security class ACL templates and generation order.
- `appendix_system_triggers.md` — system triggers and trigger messages (BLR and text).
- `appendix_ods_invariants.md` — on‑disk structure invariants and page layouts.
- `appendix_privilege_enforcement_map.md` — DDL/DML privilege enforcement and error mapping.
- `appendix_builtins_sysfunctions.md` — built‑in function list with arity/determinism.
- `appendix_builtins_system_packages.md` — system package list.
- `appendix_builtins_semantics.md` — built‑in function behavior definitions.

## Test Vectors
- `90_test_vectors.md` — required wire‑level and SQL‑level tests.
