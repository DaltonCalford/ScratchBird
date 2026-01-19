# Parser <-> Engine IPC Runtime Contract

Version: 1.0
Status: Draft (Alpha IP layer)
Last Updated: January 2026

## Purpose

Define the runtime contract between protocol parsers and the ScratchBird engine
when operating over IPC/IP. This contract ensures:
- The engine validates all bytecode before execution
- The engine receives original SQL for DDL
- Security context is enforced by the engine (parser is untrusted)

## Scope

In scope:
- IPC framing for requests/responses
- Required metadata for every execution request
- DDL persistence rules (original SQL + bytecode storage)
- Error mapping responsibilities

Out of scope:
- Listener/pool control-plane (see CONTROL_PLANE_PROTOCOL_SPEC.md)
- Client wire protocol details (see wire_protocols/)

## Trust Model

- Parser is untrusted.
- Engine is the sole authority for authentication, authorization,
  SBLR validation, and execution.

## Framing

All parser->engine messages use a fixed header + payload.
All integer fields are little-endian.

Header (fixed 32 bytes):
- magic[4]        : "SBIP"
- version_u16     : protocol version (start at 1)
- msg_u16         : message type
- flags_u32       : flags
- request_id_u64  : correlation id
- payload_len_u64 : bytes

### Message Types

0x0001 AUTH_REQUEST
0x0002 AUTH_RESPONSE
0x0010 EXECUTE_SBLR
0x0011 EXECUTE_RESULT
0x0012 EXECUTE_ERROR
0x0020 METADATA_LOOKUP
0x0021 METADATA_RESPONSE
0x0030 SESSION_CLOSE

## AUTH_REQUEST

Parser sends authentication proof (from wire protocol) to engine for validation.
Payload (binary or JSON):
- protocol: scratchbird|postgresql|mysql|firebird
- user_name
- auth_method
- auth_payload (method-specific bytes)
- client_addr
- tls_info (optional)

Engine validates against SB auth providers and returns AUTH_RESPONSE.

## EXECUTE_SBLR

Payload includes:
- session_id
- protocol
- user_id / role_id (from engine AUTH_RESPONSE)
- original_sql (required for all statements)
- statement_type (DDL/DML/PSQL/UTILITY)
- sblr_bytecode (required)
- execution_flags (autocommit, read_only, etc)

Rules:
- original_sql MUST be present for all statements (used for debugging and SQL<->SBLR mapping).
- sblr_bytecode MUST be present for all executable statements.
- Engine validates bytecode before execution and rejects invalid payloads.

## EXECUTE_RESULT

Payload includes:
- status
- result sets or row counts
- notices/warnings
- server-side timing metadata

## DDL Persistence Rules

For CREATE/ALTER/DROP and all object definitions:
- Engine stores original SQL in catalog (for auditing, reproduction, and Git tools).
- Engine stores validated bytecode in catalog (for verification and replay).
- Applies to all object types (tables, views, functions, triggers, sequences, domains, etc).
- Parser is NOT allowed to bypass persistence.

## Error Mapping

- Engine returns SB error codes and messages.
- Parser is responsible for mapping errors to protocol-specific formats.
- Engine errors must include SQLSTATE or SB error code for mapping.

## Validation Requirements

Engine MUST:
- Validate bytecode length and structure.
- Reject unknown opcodes.
- Validate object access against security context.
- Enforce RLS/CLS and permission checks.

Parser MUST:
- Provide original SQL for every statement.
- Pass through client auth proof (do not attempt local auth).
- Not execute any SQL locally.

## Related Specs

- docs/specifications/network/CONTROL_PLANE_PROTOCOL_SPEC.md
- docs/specifications/sblr/SBLR_OPCODE_REGISTRY.md
- docs/specifications/Security Design Specification/05.A_IPC_WIRE_FORMAT_AND_EXAMPLES.md
