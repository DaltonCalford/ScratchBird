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

---

# Version 1.1 Additions (Alpha)

**Status:** Draft (Target)  
**Goal:** Add prepared statements, COPY/streaming, notifications, cancel
support, and explicit attachment/txn mapping with a fully specified payload
format and SBWP-aligned names.

This section extends the v1.0 contract above. v1.0 remains valid but does not
support the features listed here.

## 1. Encoding and Primitive Types

All v1.1 payloads are **little-endian**. The following primitive types are
used in all message definitions.

```
u8   = 1 byte unsigned
u16  = 2 bytes unsigned
u32  = 4 bytes unsigned
u64  = 8 bytes unsigned
i32  = 4 bytes signed
uuid = 16 bytes (RFC4122 binary, big-endian byte order)
```

Strings are encoded as:
```
u32 byte_len
u8  bytes[byte_len]  // UTF-8, no null terminator
```

Arrays are encoded as:
```
u32 count
T   items[count]
```

Parameter values use:
```
i32 value_len  // -1 means NULL, otherwise number of bytes
u8  value[value_len]
```

## 2. Header Extension (v1.1)

The fixed 32-byte header remains unchanged. v1.1 introduces an **optional**
extended header that follows the fixed header when `flags` includes
`SBIP_FLAG_EXTENDED_HEADER`.

```
struct IpcHeaderV1 {
  char  magic[4];      // "SBIP"
  u16   version;       // 0x0001 (v1.0) or 0x0002 (v1.1)
  u16   msg_type;
  u32   flags;
  u64   request_id;
  u64   payload_len;
  u32   reserved;      // must be 0 (padding to 32 bytes)
}

struct IpcHeaderV1_1_Ext {
  u32   feature_flags; // negotiated features bitset
  u32   checksum;      // CRC32C of payload if enabled
  u64   reserved;      // must be 0
}
```

Rules:
1. v1.1 sets `version = 0x0002`.
2. If `SBIP_FLAG_EXTENDED_HEADER` is set, the 16-byte extension is appended
   immediately after the fixed header and before the payload.
3. `feature_flags` MUST match negotiated results from STARTUP/NEGOTIATE.
4. `checksum` is valid only if `SBIP_FLAG_CHECKSUM` is set; otherwise 0.
5. `reserved` MUST be 0.

### 2.1 Flags (v1.1)
```
SBIP_FLAG_EXTENDED_HEADER = 0x00000001
SBIP_FLAG_COMPRESSED      = 0x00000002
SBIP_FLAG_CHECKSUM        = 0x00000004
SBIP_FLAG_FRAGMENTED      = 0x00000008
```

## 3. Feature Negotiation

### 3.1 Feature Flags
```
FEATURE_PREPARED_STMTS = 0x00000001
FEATURE_COPY           = 0x00000002
FEATURE_STREAMING      = 0x00000004
FEATURE_NOTIFICATIONS  = 0x00000008
FEATURE_CANCEL         = 0x00000010
FEATURE_COMPRESSION    = 0x00000020
FEATURE_CHECKSUM       = 0x00000040
```

### 3.2 STARTUP / NEGOTIATE Messages (SBWP-aligned)

Message types:
```
0x0003 STARTUP_REQUEST
0x0004 STARTUP_RESPONSE
0x0005 FEATURE_NEGOTIATE
```

STARTUP_REQUEST payload:
```
u16 sbwp_major
u16 sbwp_minor
u32 client_features
uuid client_id
u8  dialect   // 0=scratchbird, 1=postgresql, 2=mysql, 3=firebird
u32 process_id
```

STARTUP_RESPONSE payload:
```
u16 accepted_major
u16 accepted_minor
u32 server_features
uuid engine_id
uuid session_id
u32 message_len
u8  message[message_len]
```

FEATURE_NEGOTIATE payload:
```
u32 requested_features
u32 granted_features
```

Rules:
1. Parser MUST send STARTUP_REQUEST before AUTH_REQUEST in v1.1.
2. Engine replies STARTUP_RESPONSE and MAY send FEATURE_NEGOTIATE.
3. Negotiated features MUST be echoed in the extended header on all subsequent
   messages when `SBIP_FLAG_EXTENDED_HEADER` is set.

## 4. Message Type Registry (v1.1)

These names align with SBWP v1.1 message names.

```
0x0001 AUTH_REQUEST
0x0002 AUTH_RESPONSE
0x0010 EXECUTE_SBLR
0x0011 EXECUTE_RESULT
0x0012 EXECUTE_ERROR
0x0020 METADATA_LOOKUP
0x0021 METADATA_RESPONSE
0x0030 SESSION_CLOSE

0x0040 PARSE
0x0041 PARSE_OK
0x0042 BIND
0x0043 BIND_OK
0x0044 DESCRIBE
0x0045 DESCRIBE_RESULT
0x0046 EXECUTE
0x0047 EXECUTE_RESULT
0x0048 CLOSE
0x0049 CLOSE_OK
0x004A SYNC

0x0050 COPY_BEGIN
0x0051 COPY_DATA
0x0052 COPY_DONE
0x0053 COPY_FAIL
0x0054 STREAM_READY
0x0055 STREAM_DATA
0x0056 STREAM_END
0x0057 STREAM_CONTROL

0x0060 SUBSCRIBE
0x0061 UNSUBSCRIBE
0x0062 NOTIFICATION

0x0070 CANCEL
0x0071 CANCEL_ACK
0x0072 TXN_BEGIN
0x0073 TXN_COMMIT
0x0074 TXN_ROLLBACK
0x0075 TXN_SAVEPOINT
0x0076 TXN_RELEASE
0x0077 TXN_ROLLBACK_TO
0x0078 TXN_STATUS
```

## 5. Prepared Statement Lifecycle (PARSE/BIND/DESCRIBE/EXECUTE/CLOSE/SYNC)

PARSE payload:
```
uuid session_id
uuid attachment_id
u64 txn_id
u32 statement_name_len
u8  statement_name[statement_name_len]
u32 sql_len
u8  sql[sql_len]
```

BIND payload:
```
uuid session_id
uuid attachment_id
u64 txn_id
u32 statement_name_len
u8  statement_name[statement_name_len]
u32 portal_name_len
u8  portal_name[portal_name_len]
u32 param_format_count
u16 param_formats[param_format_count]  // 0=text, 1=binary
u32 param_value_count
i32 param_len[param_value_count]
u8  param_bytes[sum(param_len>=0)]
u32 result_format_count
u16 result_formats[result_format_count] // 0=text, 1=binary
```

DESCRIBE payload:
```
u8  target_type  // 0=statement, 1=portal
u32 name_len
u8  name[name_len]
```

EXECUTE payload:
```
uuid attachment_id
u64 txn_id
u32 portal_name_len
u8  portal_name[portal_name_len]
u32 max_rows
u32 timeout_ms
```

CLOSE payload:
```
u8  target_type  // 0=statement, 1=portal
u32 name_len
u8  name[name_len]
```

SYNC payload:
```
u32 reserved_zero  // must be 0
```

Rules:
1. Engine owns the statement cache; parser must not execute locally.
2. All PARSE/BIND/EXECUTE messages MUST include attachment_id and txn_id.

## 6. COPY and Streaming

COPY_BEGIN payload:
```
uuid attachment_id
u64 txn_id
u64 copy_id
u8  direction   // 0=in, 1=out, 2=both
u8  format      // 0=text, 1=csv, 2=binary
```

COPY_DATA payload:
```
u64 copy_id
u32 data_len
u8  data[data_len]
```

COPY_DONE payload:
```
u64 copy_id
```

COPY_FAIL payload:
```
u64 copy_id
u32 error_len
u8  error[error_len]
```

STREAM_READY payload:
```
u64 stream_id
```

STREAM_DATA payload:
```
u64 stream_id
u32 data_len
u8  data[data_len]
```

STREAM_END payload:
```
u64 stream_id
u32 status_code
u32 message_len
u8  message[message_len]
```

STREAM_CONTROL payload:
```
u64 stream_id
u32 window_bytes
u32 window_rows
```

Rules:
1. COPY messages require FEATURE_COPY.
2. STREAM_* messages require FEATURE_STREAMING.
3. STREAM_CONTROL must be enforced for backpressure.

## 7. Notifications

SUBSCRIBE payload:
```
uuid attachment_id
u64 txn_id
u32 channel_len
u8  channel[channel_len]
```

UNSUBSCRIBE payload:
```
uuid attachment_id
u64 txn_id
u32 channel_len
u8  channel[channel_len]
```

NOTIFICATION payload:
```
u32 channel_len
u8  channel[channel_len]
u32 payload_len
u8  payload[payload_len]
uuid origin_session
```

Rules:
1. Notifications require FEATURE_NOTIFICATIONS.

## 8. Cancel and Transaction Lifecycle

CANCEL payload:
```
u64 target_request_id
u32 reason_len
u8  reason[reason_len]
```

CANCEL_ACK payload:
```
u64 target_request_id
u8  status  // 0=accepted, 1=ignored, 2=not_found
```

TXN_BEGIN payload:
```
uuid attachment_id
u64 txn_id
```

TXN_COMMIT payload:
```
uuid attachment_id
u64 txn_id
```

TXN_ROLLBACK payload:
```
uuid attachment_id
u64 txn_id
```

TXN_SAVEPOINT payload:
```
uuid attachment_id
u64 txn_id
u32 name_len
u8  name[name_len]
```

TXN_RELEASE payload:
```
uuid attachment_id
u64 txn_id
u32 name_len
u8  name[name_len]
```

TXN_ROLLBACK_TO payload:
```
uuid attachment_id
u64 txn_id
u32 name_len
u8  name[name_len]
```

TXN_STATUS payload:
```
uuid attachment_id
u64 txn_id
u8  state  // 0=idle,1=in_txn,2=failed
```

Rules:
1. CANCEL requires FEATURE_CANCEL.
2. Engine must propagate cancel to execution context.

## 9. Attachment / Transaction Mapping

Mapping rules:
1. `attachment_id` is issued by the engine after successful auth.
2. `txn_id` is issued by the engine; parser MUST echo it on every execution request.
3. If missing or zero, engine must reject with protocol error.

## 10. Compression / Checksum

Rules:
1. If FEATURE_COMPRESSION is granted and `SBIP_FLAG_COMPRESSED` is set, payload
   is zstd-compressed.
2. If FEATURE_CHECKSUM is granted and `SBIP_FLAG_CHECKSUM` is set, CRC32C is
   included in the extended header and verified by the receiver.

---

# Appendix A: IPC v1.1 ↔ SBWP v1.1 Cross-Reference

This appendix maps IPC v1.1 message types to the canonical SBWP v1.1 message
types defined in `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`
and provides field-by-field alignment rules. The goal is zero ambiguity.

## A.1 Message Type Mapping

Each IPC message type maps to an SBWP message type by name (and SBWP code).
IPC codes are **internal** and do not need to match SBWP numeric codes.

| IPC msg_type | IPC name | SBWP msg_type | SBWP code | Notes |
| --- | --- | --- | --- | --- |
| 0x0003 | STARTUP_REQUEST | STARTUP | 0x01 | IPC STARTUP_REQUEST is the parser→engine analog of client STARTUP. |
| 0x0004 | STARTUP_RESPONSE | NEGOTIATE_VERSION | 0x56 | IPC STARTUP_RESPONSE carries accepted version/features. |
| 0x0001 | AUTH_REQUEST | AUTH_REQUEST | 0x40 | Engine→parser auth challenge. |
| 0x0002 | AUTH_RESPONSE | AUTH_RESPONSE | 0x02 | Parser→engine auth response. |
| 0x0010 | EXECUTE_SBLR | SBLR_EXECUTE | 0x10 | Direct SBLR execution. |
| 0x0011 | EXECUTE_RESULT | COMMAND_COMPLETE | 0x46 | Result status or row count. |
| 0x0012 | EXECUTE_ERROR | ERROR | 0x48 | Error mapping required. |
| 0x0040 | PARSE | PARSE | 0x04 | Prepared statement parse. |
| 0x0041 | PARSE_OK | PARSE_COMPLETE | 0x4A | Parse completion. |
| 0x0042 | BIND | BIND | 0x05 | Bind parameters to portal. |
| 0x0043 | BIND_OK | BIND_COMPLETE | 0x4B | Bind completion. |
| 0x0044 | DESCRIBE | DESCRIBE | 0x06 | Describe statement/portal. |
| 0x0045 | DESCRIBE_RESULT | PARAMETER_DESCRIPTION / ROW_DESCRIPTION | 0x50 / 0x44 | IPC may return one or both. |
| 0x0046 | EXECUTE | EXECUTE | 0x07 | Execute portal. |
| 0x0047 | EXECUTE_RESULT | DATA_ROW / COMMAND_COMPLETE | 0x45 / 0x46 | Streaming of rows then completion. |
| 0x0048 | CLOSE | CLOSE | 0x08 | Close statement or portal. |
| 0x0049 | CLOSE_OK | CLOSE_COMPLETE | 0x4C | Close completion. |
| 0x004A | SYNC | SYNC | 0x09 | Sync point. |
| 0x0050 | COPY_BEGIN | COPY_IN_RESPONSE / COPY_OUT_RESPONSE / COPY_BOTH_RESPONSE | 0x51 / 0x52 / 0x53 | Direction selects response type. |
| 0x0051 | COPY_DATA | COPY_DATA | 0x0D | Copy data chunk. |
| 0x0052 | COPY_DONE | COPY_DONE | 0x0E | Copy complete. |
| 0x0053 | COPY_FAIL | COPY_FAIL | 0x0F | Copy failure. |
| 0x0054 | STREAM_READY | STREAM_READY | 0x59 | Stream ready. |
| 0x0055 | STREAM_DATA | STREAM_DATA | 0x5A | Stream data chunk. |
| 0x0056 | STREAM_END | STREAM_END | 0x5B | Stream complete. |
| 0x0057 | STREAM_CONTROL | STREAM_CONTROL | 0x14 | Backpressure/flow control. |
| 0x0060 | SUBSCRIBE | SUBSCRIBE | 0x11 | Subscribe to notifications. |
| 0x0061 | UNSUBSCRIBE | UNSUBSCRIBE | 0x12 | Unsubscribe. |
| 0x0062 | NOTIFICATION | NOTIFICATION | 0x54 | Notification delivery. |
| 0x0070 | CANCEL | CANCEL | 0x0B | Cancel request. |
| 0x0071 | CANCEL_ACK | ERROR / NOTICE / READY | 0x48 / 0x49 / 0x43 | IPC explicit ack; SBWP uses ERROR/READY semantics. |
| 0x0072 | TXN_BEGIN | TXN_BEGIN | 0x15 | Explicit txn begin. |
| 0x0073 | TXN_COMMIT | TXN_COMMIT | 0x16 | Commit txn. |
| 0x0074 | TXN_ROLLBACK | TXN_ROLLBACK | 0x17 | Rollback txn. |
| 0x0075 | TXN_SAVEPOINT | TXN_SAVEPOINT | 0x18 | Savepoint. |
| 0x0076 | TXN_RELEASE | TXN_RELEASE | 0x19 | Release savepoint. |
| 0x0077 | TXN_ROLLBACK_TO | TXN_ROLLBACK_TO | 0x1A | Rollback to savepoint. |
| 0x0078 | TXN_STATUS | TXN_STATUS | 0x5C | Transaction status. |

## A.2 Field-by-Field Mapping Rules

This section specifies exact field alignment between IPC v1.1 payloads and
SBWP v1.1 message bodies and headers.

### A.2.1 Attachment and Transaction IDs

SBWP places `attachment_id` (uuid) and `txn_id` (u64) in the **SBWP message header**.
IPC v1.1 places them in the **IPC payload** for all execution messages.

Mapping rule:
- IPC `attachment_id` MUST be copied into SBWP header `attachment_id`.
- IPC `txn_id` MUST be copied into SBWP header `txn_id`.
- If IPC payload omits these, the engine MUST reject with protocol error.

### A.2.2 STARTUP_REQUEST ↔ SBWP STARTUP

IPC STARTUP_REQUEST payload:
```
u16 sbwp_major
u16 sbwp_minor
u32 client_features
uuid client_id
u8  dialect
u32 process_id
```

SBWP STARTUP body:
```
u16 version_major
u16 version_minor
u32 flags/feature_bits
uuid client_id
u8  dialect
u32 process_id
```

Mapping rule:
- Fields map 1:1 by order and size.
- `client_features` maps to SBWP feature flags (same bit positions).

### A.2.3 STARTUP_RESPONSE ↔ SBWP NEGOTIATE_VERSION + READY

IPC STARTUP_RESPONSE payload:
```
u16 accepted_major
u16 accepted_minor
u32 server_features
uuid engine_id
uuid session_id
u32 message_len
u8  message[message_len]
```

SBWP response sequence:
- `NEGOTIATE_VERSION` carries accepted version and server feature flags.
- `READY` indicates session is ready after auth.

Mapping rule:
- IPC `accepted_*` and `server_features` map to SBWP NEGOTIATE_VERSION body.
- IPC `engine_id` and `session_id` are emitted as `PARAMETER_STATUS` pairs:
  `engine_id`, `session_id`.
- `message` maps to SBWP NOTICE (if non-empty) before READY.

### A.2.4 AUTH_REQUEST / AUTH_RESPONSE

IPC AUTH_REQUEST payload mirrors SBWP AUTH_REQUEST:
- Auth method enum
- Auth payload bytes

IPC AUTH_RESPONSE payload mirrors SBWP AUTH_RESPONSE:
- Auth payload bytes
- Auth continuation flag (if used)

Mapping rule:
- Payload bytes are passed through unchanged.
- Auth method enum values must match SBWP AuthMethod values.

### A.2.5 PARSE ↔ SBWP PARSE

IPC PARSE payload:
```
uuid session_id
uuid attachment_id
u64 txn_id
u32 statement_name_len
u8  statement_name[statement_name_len]
u32 sql_len
u8  sql[sql_len]
```

SBWP PARSE body:
```
u32 name_length
u8  statement_name[name_length]
u32 query_length
u8  query[query_length]
u16 num_param_types
u16 reserved
u32 param_type_oids[num_param_types]
```

Mapping rule:
- IPC `statement_name` and `sql` map directly.
- IPC does not carry `param_type_oids`; SBWP uses 0 and num_param_types=0.
- Attachment/txn are mapped via header (see A.2.1).

### A.2.6 BIND ↔ SBWP BIND

IPC BIND payload:
```
uuid session_id
uuid attachment_id
u64 txn_id
u32 statement_name_len
u8  statement_name[]
u32 portal_name_len
u8  portal_name[]
u32 param_format_count
u16 param_formats[]
u32 param_value_count
i32 param_len[]
u8  param_bytes[]
u32 result_format_count
u16 result_formats[]
```

SBWP BIND body:
```
u32 portal_name_length
u8  portal_name[]
u32 statement_name_length
u8  statement_name[]
u16 num_param_formats
u16 param_formats[]
u16 num_param_values
u16 reserved
ParamValue params[]
u16 num_result_formats
u16 result_formats[]
```

Mapping rule:
- IPC `param_format_count` → SBWP `num_param_formats` (u16).
- IPC `param_value_count` → SBWP `num_param_values` (u16).
- IPC `param_len[]`/`param_bytes[]` are re-packed into SBWP ParamValue format.
- IPC `result_format_count` → SBWP `num_result_formats` (u16).

### A.2.7 DESCRIBE ↔ SBWP DESCRIBE

IPC DESCRIBE payload:
```
u8  target_type  // 0=statement, 1=portal
u32 name_len
u8  name[]
```

SBWP DESCRIBE body:
```
u8  describe_type  // 'S' or 'P'
u8  reserved[3]
u32 name_length
u8  name[]
```

Mapping rule:
- IPC target_type 0 → 'S', 1 → 'P'.
- name_len and name map directly.

### A.2.8 EXECUTE ↔ SBWP EXECUTE

IPC EXECUTE payload:
```
uuid attachment_id
u64 txn_id
u32 portal_name_len
u8  portal_name[]
u32 max_rows
u32 timeout_ms
```

SBWP EXECUTE body:
```
u32 portal_name_length
u8  portal_name[]
u32 max_rows
u8  fetch_backward
```

Mapping rule:
- portal_name and max_rows map directly.
- IPC `timeout_ms` maps to SBWP message flag or execution context; if unsupported, ignore.
- SBWP `fetch_backward` is 0 (forward-only) for IPC.

### A.2.9 CLOSE ↔ SBWP CLOSE

IPC CLOSE payload:
```
u8  target_type  // 0=statement, 1=portal
u32 name_len
u8  name[]
```

SBWP CLOSE body:
```
u8  close_type  // 'S' or 'P'
u8  reserved[3]
u32 name_length
u8  name[]
```

Mapping rule:
- IPC target_type 0 → 'S', 1 → 'P'.
- name_len and name map directly.

### A.2.10 SYNC ↔ SBWP SYNC

IPC SYNC payload:
```
u32 reserved_zero
```

SBWP SYNC body:
- empty

Mapping rule:
- IPC reserved_zero must be 0; ignored by SBWP.

### A.2.11 COPY ↔ SBWP COPY_*

IPC COPY_BEGIN payload:
```
uuid attachment_id
u64 txn_id
u64 copy_id
u8  direction   // 0=in, 1=out, 2=both
u8  format      // 0=text, 1=csv, 2=binary
```

SBWP COPY response:
- COPY_IN_RESPONSE / COPY_OUT_RESPONSE / COPY_BOTH_RESPONSE

Mapping rule:
- IPC direction selects which SBWP COPY_*_RESPONSE to emit.
- IPC copy_id maps to internal copy handle.
- IPC format maps to SBWP format flag (text/csv/binary).

IPC COPY_DATA/COPY_DONE/COPY_FAIL map 1:1 to SBWP of same name.

### A.2.12 STREAM_* ↔ SBWP STREAM_*

IPC STREAM_READY:
```
u64 stream_id
```

SBWP STREAM_READY:
```
u64 stream_id
u64 total_rows
u64 estimated_bytes
```

Mapping rule:
- If IPC does not provide totals, SBWP fields are 0.

IPC STREAM_DATA:
```
u64 stream_id
u32 data_len
u8  data[]
```

SBWP STREAM_DATA:
```
u64 stream_id
u32 chunk_rows
u32 chunk_bytes
u8  data[]
```

Mapping rule:
- IPC data_len maps to SBWP chunk_bytes.
- chunk_rows is 0 unless known.

IPC STREAM_END:
```
u64 stream_id
u32 status_code
u32 message_len
u8  message[]
```

SBWP STREAM_END:
```
u64 stream_id
u64 total_rows
u64 total_bytes
```

Mapping rule:
- IPC status_code != 0 should emit SBWP ERROR before STREAM_END.
- total_rows/total_bytes set to 0 if unknown.

IPC STREAM_CONTROL:
```
u64 stream_id
u32 window_bytes
u32 window_rows
```

SBWP STREAM_CONTROL:
```
u8  control_type
u8  reserved[3]
u32 window_size
u32 timeout_ms
```

Mapping rule:
- IPC window_rows maps to SBWP window_size (rows).
- If IPC window_bytes is set, apply to engine backpressure window in bytes.
- control_type is STREAM_ACK unless engine is pausing/resuming.

### A.2.13 NOTIFICATION ↔ SBWP NOTIFICATION

IPC NOTIFICATION:
```
u32 channel_len
u8  channel[]
u32 payload_len
u8  payload[]
uuid origin_session
```

SBWP NOTIFICATION:
```
u32 process_id
u32 channel_length
u8  channel[]
u32 payload_length
u8  payload[]
u8  change_type
u64 row_id
```

Mapping rule:
- IPC origin_session maps to SBWP process_id via session→pid lookup or 0.
- change_type/row_id are 0 unless table notifications are enabled.

### A.2.14 CANCEL ↔ SBWP CANCEL

IPC CANCEL:
```
u64 target_request_id
u32 reason_len
u8  reason[]
```

SBWP CANCEL:
```
u32 cancel_type
u32 target_sequence
```

Mapping rule:
- IPC target_request_id must map to SBWP target_sequence (sequence lookup).
- cancel_type is 1 (by_sequence) when target is known; otherwise 0 (current).

### A.2.15 TXN_* ↔ SBWP TXN_*

IPC TXN_BEGIN/COMMIT/ROLLBACK/...
payloads map directly by name to SBWP TXN_* messages with attachment/txn
carried in the SBWP header.

## A.3 Unsupported SBWP Fields (Must Be Zero)

When translating IPC v1.1 to SBWP v1.1, any SBWP fields not represented in the
IPC payloads MUST be set to zero (or empty) to avoid undefined behavior.

- **PARSE**: `num_param_types = 0`, `param_types[]` omitted (infer from context).
- **BIND**: `reserved` (u16) = 0.
- **DESCRIBE**: `reserved[3]` = 0.
- **EXECUTE**: `fetch_backward = 0` (forward-only).
- **CLOSE**: `reserved[3]` = 0.
- **SYNC / FLUSH**: no payload (ignore IPC reserved_zero).
- **STREAM_READY**: `total_rows = 0`, `estimated_bytes = 0` if unknown.
- **STREAM_DATA**: `chunk_rows = 0` if unknown.
- **STREAM_END**: `total_rows = 0`, `total_bytes = 0` if unknown.
- **NOTIFICATION**: `change_type = 0`, `row_id = 0` unless table notifications are enabled.
- **CANCEL**: if no sequence mapping exists, use `cancel_type = 0` and `target_sequence = 0`.

Additionally, SBWP header fields must follow SBWP v1.1 requirements:
- `attachment_id` and `txn_id` are **zero** until AUTH_OK is sent.
- All reserved flag bits in SBWP must be 0.

## A.4 IPC Payload Binary Layout Examples (Hex + Offsets)

Examples below show **IPC payloads only** (IPC header not included).
All values are **little-endian**. Strings are `u32 length` + bytes.
UUIDs shown as 16 bytes. Hex values are spaced by byte.

For compactness, most examples use:
- `session_id = 11111111-2222-3333-4444-555555555555`
- `attachment_id = aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee`
- `txn_id = 0x0000000000000001`
- empty strings (length = 0) unless noted

### STARTUP_REQUEST
```
0000: 01 00 01 00 20 00 00 00 11 11 11 11 22 22 33 33
0010: 44 44 55 55 55 55 55 55 00 12 00 00 00
```
Fields:
- sbwp_major=1, sbwp_minor=1
- client_features=0x20
- client_id (16 bytes)
- dialect=0 (scratchbird)
- process_id=0x00001200

### STARTUP_RESPONSE
```
0000: 01 00 01 00 20 00 00 00 11 11 11 11 22 22 33 33
0010: 44 44 55 55 55 55 55 55 66 66 66 66 77 77 88 88
0020: 99 99 aa aa aa aa aa aa 00 00 00 00
```
Fields:
- accepted_major=1, accepted_minor=1
- server_features=0x20
- engine_id (16 bytes)
- session_id (16 bytes)
- message_len=0

### FEATURE_NEGOTIATE
```
0000: 20 00 00 00 20 00 00 00
```
Fields:
- requested_features=0x20
- granted_features=0x20

### AUTH_REQUEST (payload bytes example)
```
0000: 02 00 00 00 01 02
```
Fields:
- auth_method=2 (example)
- auth_payload bytes: 0x01 0x02

### AUTH_RESPONSE (payload bytes example)
```
0000: 01 02 03 04
```
Fields:
- auth_payload bytes: 0x01 0x02 0x03 0x04

### PARSE
```
0000: 11 11 11 11 22 22 33 33 44 44 55 55 55 55 55 55
0010: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0020: 01 00 00 00 00 00 00 00
0028: 00 00 00 00 00 00 00 00
```
Fields:
- session_id (16)
- attachment_id (16)
- txn_id (u64 = 1)
- statement_name_len=0
- sql_len=0

### BIND
```
0000: 11 11 11 11 22 22 33 33 44 44 55 55 55 55 55 55
0010: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0020: 01 00 00 00 00 00 00 00
0028: 00 00 00 00 00 00 00 00
0030: 00 00 00 00 00 00 00 00
0038: 00 00 00 00
```
Fields:
- session_id (16)
- attachment_id (16)
- txn_id (u64 = 1)
- statement_name_len=0
- portal_name_len=0
- param_format_count=0
- param_value_count=0
- result_format_count=0

### DESCRIBE
```
0000: 00 00 00 00 00
```
Fields:
- target_type=0 (statement)
- name_len=0

### EXECUTE
```
0000: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0010: 01 00 00 00 00 00 00 00
0018: 00 00 00 00 00 00 00 00 00 00 00 00
```
Fields:
- attachment_id (16)
- txn_id (u64 = 1)
- portal_name_len=0
- max_rows=0
- timeout_ms=0

### CLOSE
```
0000: 00 00 00 00 00
```
Fields:
- target_type=0
- name_len=0

### SYNC
```
0000: 00 00 00 00
```
Fields:
- reserved_zero=0

### COPY_BEGIN
```
0000: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0010: 01 00 00 00 00 00 00 00
0018: 10 00 00 00 00 00 00 00
0020: 00 00
```
Fields:
- attachment_id (16)
- txn_id (u64 = 1)
- copy_id (u64 = 0x10)
- direction=0 (in)
- format=0 (text)

### COPY_DATA
```
0000: 10 00 00 00 00 00 00 00
0008: 03 00 00 00 61 62 63
```
Fields:
- copy_id=0x10
- data_len=3
- data="abc"

### COPY_DONE
```
0000: 10 00 00 00 00 00 00 00
```
Fields:
- copy_id=0x10

### COPY_FAIL
```
0000: 10 00 00 00 00 00 00 00
0008: 05 00 00 00 65 72 72 6f 72
```
Fields:
- copy_id=0x10
- error_len=5
- error="error"

### STREAM_READY
```
0000: 20 00 00 00 00 00 00 00
```
Fields:
- stream_id=0x20

### STREAM_DATA
```
0000: 20 00 00 00 00 00 00 00
0008: 04 00 00 00 64 61 74 61
```
Fields:
- stream_id=0x20
- data_len=4
- data="data"

### STREAM_END
```
0000: 20 00 00 00 00 00 00 00
0008: 00 00 00 00 00 00 00 00
```
Fields:
- stream_id=0x20
- status_code=0
- message_len=0

### STREAM_CONTROL
```
0000: 20 00 00 00 00 00 00 00
0008: 00 10 00 00 00 00 00 00
```
Fields:
- stream_id=0x20
- window_bytes=0x1000
- window_rows=0

### SUBSCRIBE
```
0000: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0010: 01 00 00 00 00 00 00 00
0018: 00 00 00 00
```
Fields:
- attachment_id (16)
- txn_id (u64 = 1)
- channel_len=0

### UNSUBSCRIBE
```
0000: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0010: 01 00 00 00 00 00 00 00
0018: 00 00 00 00
```
Fields:
- attachment_id (16)
- txn_id (u64 = 1)
- channel_len=0

### NOTIFICATION
```
0000: 00 00 00 00
0004: 00 00 00 00
0008: 11 11 11 11 22 22 33 33 44 44 55 55 55 55 55 55
```
Fields:
- channel_len=0
- payload_len=0
- origin_session (16)

### CANCEL
```
0000: 2a 00 00 00 00 00 00 00
0008: 00 00 00 00
```
Fields:
- target_request_id=0x2a
- reason_len=0

### CANCEL_ACK
```
0000: 2a 00 00 00 00 00 00 00
0008: 00
```
Fields:
- target_request_id=0x2a
- status=0 (accepted)

### TXN_BEGIN / TXN_COMMIT / TXN_ROLLBACK
```
0000: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0010: 01 00 00 00 00 00 00 00
```
Fields:
- attachment_id (16)
- txn_id (u64 = 1)

### TXN_SAVEPOINT / TXN_RELEASE / TXN_ROLLBACK_TO
```
0000: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0010: 01 00 00 00 00 00 00 00
0018: 00 00 00 00
```
Fields:
- attachment_id (16)
- txn_id (u64 = 1)
- name_len=0

### TXN_STATUS
```
0000: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0010: 01 00 00 00 00 00 00 00
0018: 00
```
Fields:
- attachment_id (16)
- txn_id (u64 = 1)
- state=0 (idle)

## A.5 Header-Inclusive IPC Examples (Header + Ext + Payload)

All examples below include:
- 32-byte IPC header (v1.1 with reserved padding)
- 16-byte extended header (feature_flags=0x20, checksum=0)
- Payload from A.4

Header fields used in all examples:
- magic = "SBIP" (53 42 49 50)
- version = 0x0002
- flags = 0x00000001 (SBIP_FLAG_EXTENDED_HEADER)
- request_id = 0x0000000000000001
- reserved (u32) = 0

### A.5.1 STARTUP_REQUEST
```
0000: 53 42 49 50 02 00 03 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 1d 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 01 00 01 00 20 00 00 00 11 11 11 11 22 22 33 33
0040: 44 44 55 55 55 55 55 55 00 12 00 00 00
```

### A.5.2 STARTUP_RESPONSE
```
0000: 53 42 49 50 02 00 04 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 2c 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 01 00 01 00 20 00 00 00 11 11 11 11 22 22 33 33
0040: 44 44 55 55 55 55 55 55 66 66 66 66 77 77 88 88
0050: 99 99 aa aa aa aa aa aa 00 00 00 00
```

### A.5.3 FEATURE_NEGOTIATE
```
0000: 53 42 49 50 02 00 05 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 20 00 00 00 20 00 00 00
```

### A.5.4 AUTH_REQUEST
```
0000: 53 42 49 50 02 00 01 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 06 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 02 00 00 00 01 02
```

### A.5.5 AUTH_RESPONSE
```
0000: 53 42 49 50 02 00 02 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 01 02 03 04
```

### A.5.6 PARSE
```
0000: 53 42 49 50 02 00 40 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 30 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 11 11 11 11 22 22 33 33 44 44 55 55 55 55 55 55
0040: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0050: 01 00 00 00 00 00 00 00
0058: 00 00 00 00 00 00 00 00
```

### A.5.7 BIND
```
0000: 53 42 49 50 02 00 42 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 3c 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 11 11 11 11 22 22 33 33 44 44 55 55 55 55 55 55
0040: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0050: 01 00 00 00 00 00 00 00
0058: 00 00 00 00 00 00 00 00
0060: 00 00 00 00 00 00 00 00
0068: 00 00 00 00
```

### A.5.8 DESCRIBE
```
0000: 53 42 49 50 02 00 44 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 05 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 00 00 00 00 00
```

### A.5.9 EXECUTE
```
0000: 53 42 49 50 02 00 46 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 24 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
0048: 00 00 00 00 00 00 00 00 00 00 00 00
```

### A.5.10 CLOSE
```
0000: 53 42 49 50 02 00 48 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 05 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 00 00 00 00 00
```

### A.5.11 SYNC
```
0000: 53 42 49 50 02 00 4a 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 00 00 00 00
```

### A.5.12 COPY_BEGIN
```
0000: 53 42 49 50 02 00 50 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 22 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
0048: 10 00 00 00 00 00 00 00
0050: 00 00
```

### A.5.13 COPY_DATA
```
0000: 53 42 49 50 02 00 51 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 0f 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 10 00 00 00 00 00 00 00
0038: 03 00 00 00 61 62 63
```

### A.5.14 COPY_DONE
```
0000: 53 42 49 50 02 00 52 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 10 00 00 00 00 00 00 00
```

### A.5.15 COPY_FAIL
```
0000: 53 42 49 50 02 00 53 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 11 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 10 00 00 00 00 00 00 00
0038: 05 00 00 00 65 72 72 6f 72
```

### A.5.16 STREAM_READY
```
0000: 53 42 49 50 02 00 54 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 20 00 00 00 00 00 00 00
```

### A.5.17 STREAM_DATA
```
0000: 53 42 49 50 02 00 55 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 10 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 20 00 00 00 00 00 00 00
0038: 04 00 00 00 64 61 74 61
```

### A.5.18 STREAM_END
```
0000: 53 42 49 50 02 00 56 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 10 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 20 00 00 00 00 00 00 00
0038: 00 00 00 00 00 00 00 00
```

### A.5.19 STREAM_CONTROL
```
0000: 53 42 49 50 02 00 57 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 10 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 20 00 00 00 00 00 00 00
0038: 00 10 00 00 00 00 00 00
```

### A.5.20 SUBSCRIBE
```
0000: 53 42 49 50 02 00 60 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 1c 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
0048: 00 00 00 00
```

### A.5.21 UNSUBSCRIBE
```
0000: 53 42 49 50 02 00 61 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 1c 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
0048: 00 00 00 00
```

### A.5.22 NOTIFICATION
```
0000: 53 42 49 50 02 00 62 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 18 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 00 00 00 00 00 00 00 00
0038: 11 11 11 11 22 22 33 33 44 44 55 55 55 55 55 55
```

### A.5.23 CANCEL
```
0000: 53 42 49 50 02 00 70 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 0c 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 2a 00 00 00 00 00 00 00
0038: 00 00 00 00
```

### A.5.24 CANCEL_ACK
```
0000: 53 42 49 50 02 00 71 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 09 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: 2a 00 00 00 00 00 00 00
0038: 00
```

### A.5.25 TXN_BEGIN
```
0000: 53 42 49 50 02 00 72 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 18 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
```

### A.5.26 TXN_COMMIT
```
0000: 53 42 49 50 02 00 73 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 18 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
```

### A.5.27 TXN_ROLLBACK
```
0000: 53 42 49 50 02 00 74 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 18 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
```

### A.5.28 TXN_SAVEPOINT
```
0000: 53 42 49 50 02 00 75 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 1c 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
0048: 00 00 00 00
```

### A.5.29 TXN_RELEASE
```
0000: 53 42 49 50 02 00 76 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 1c 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
0048: 00 00 00 00
```

### A.5.30 TXN_ROLLBACK_TO
```
0000: 53 42 49 50 02 00 77 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 1c 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
0048: 00 00 00 00
```

### A.5.31 TXN_STATUS
```
0000: 53 42 49 50 02 00 78 00 01 00 00 00 01 00 00 00
0010: 00 00 00 00 19 00 00 00 00 00 00 00 00 00 00 00
0020: 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030: aa aa aa aa bb bb cc cc dd dd ee ee ee ee ee ee
0040: 01 00 00 00 00 00 00 00
0048: 00
```

## A.6 Machine-Readable Message Tables

The complete IPC v1.1 message tables with offsets and sizes are provided as
machine-readable artifacts:
- `ScratchBird/docs/specifications/network/ipc_message_table.json`
- `ScratchBird/docs/specifications/network/ipc_message_table.yaml`

Usage note:
Use these tables as the authoritative source for offset/size validation in
tests and packet decoders. Parsers should reject payloads that do not match the
declared field lengths or violate `must` constraints (e.g., reserved fields).
