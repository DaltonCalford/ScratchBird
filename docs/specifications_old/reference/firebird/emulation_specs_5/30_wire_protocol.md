# Firebird 5 Wire Protocol (XDR) - Standalone

## 1. Transport & Encoding
- Transport: TCP (default port 3050).
- Encoding: XDR (big-endian, 4-byte alignment).
- Record framing: XDR records over TCP; each packet is encoded according to the field order in `formal/protocol_fields.json` and the type rules in this document.
 - Transport framing rules are defined in `appendix_transport_framing.md` and are authoritative.

### 1.1 XDR Primitive Encodings (Authoritative)
These rules apply to all protocol structs and data types.

- `xdr_long` / `SLONG` / `ULONG`: 4 bytes, big-endian (network order).
- `xdr_short` / `SSHORT` / `USHORT`: encoded as a 4-byte big-endian `xdr_long`, then truncated to 16 bits on decode.
- `xdr_hyper` / `SINT64` / `UINT64`: 8 bytes as two `xdr_long` values: **high 32 bits first**, then low 32 bits.
- `xdr_quad` / `SQUAD`: two `xdr_long` values: `gds_quad_high` then `gds_quad_low`.
- `xdr_opaque(p, len)`: exactly `len` bytes followed by 0-3 padding bytes so total is a multiple of 4.
- `xdr_string`: 4-byte length (`xdr_long`), then bytes, then 0-3 padding bytes.

All padding bytes are `0x00`.

## 2. Core Wire Types
### 2.1 `CSTRING` / `CSTRING_CONST`
Layout (XDR):
1. `cstr_length` as 32-bit signed (`xdr_long`)
2. `cstr_address` raw bytes, length `cstr_length`
3. Padding to 4-byte boundary (0-3 zero bytes)

Notes:
- Length fields were USHORT in older versions; `fixupLength()` masks sign extension if the high 16 bits are `0xFFFF`.
- `CSTRING_CONST` shares the same on-wire layout.

### 2.2 `xdr_response(CSTRING)`
Used for `p_resp_data` and inline blob info. If decoding on client and `cstr_allocated` is set, `xdr_response` enforces a maximum length of `cstr_allocated`.

### 2.3 `xdr_longs(CSTRING)`
Encodes a vector of `SLONG` values. `cstr_length` is the byte length (not count).

### 2.4 Handle and Buffer Limits
Limits are defined in `appendix_protocol_constants.md`:
- `MAX_CNCT_VERSIONS`
- `MAX_OBJCT_HANDLES`
- `INVALID_OBJECT`
- `BLOB_LENGTH`

## 3. Protocol Versions
Negotiated during connect. Firebird 5 supports protocol versions 10-20.

Protocol version constants are defined in `appendix_protocol_constants.md`.
- `PROTOCOL_VERSION10` = 10
- `PROTOCOL_VERSION11` = 0x800B
- `PROTOCOL_VERSION12` = 0x800C
- `PROTOCOL_VERSION13` = 0x800D
- `PROTOCOL_VERSION14` = 0x800E
- `PROTOCOL_VERSION15` = 0x800F
- `PROTOCOL_VERSION16` = 0x8010
- `PROTOCOL_VERSION17` = 0x8011
- `PROTOCOL_VERSION18` = 0x8012
- `PROTOCOL_VERSION19` = 0x8013
- `PROTOCOL_VERSION20` = 0x8014

Notes:
- Versions 11+ set `FB_PROTOCOL_FLAG` (0x8000) to disambiguate from InterBase.
- Version 13 introduces packed SQL messages (NULL bitmap).
- Version 16 introduces statement timeouts.
- Version 18 adds scrollable fetch.
- Version 19 adds inline blobs.
- Version 20 adds statement prepare flags.

## 4. Connect/Accept Handshake
### 4.1 Client Connect Block (`P_CNCT`)
XDR field order:
1. `p_cnct_operation` (enum)
2. `p_cnct_cversion` (short) must be 3
3. `p_cnct_client` (enum)
4. `p_cnct_file` (CSTRING_CONST)
5. `p_cnct_count` (short)
6. `p_cnct_user_id` (CSTRING_CONST)
7. `p_cnct_versions[i]` repeated `p_cnct_count` times:
   - `p_cnct_version` (short)
   - `p_cnct_architecture` (enum)
   - `p_cnct_min_type` (unsigned short)
   - `p_cnct_max_type` (unsigned short)
   - `p_cnct_weight` (short)

If `p_cnct_count` exceeds `MAX_CNCT_VERSIONS`, extra versions are ignored; count is truncated to `MAX_CNCT_VERSIONS`.

### 4.2 Server Accept (`P_ACPT`) / Accept-with-Data (`P_ACPD`)
`op_accept` → `P_ACPT` fields:
1. `p_acpt_version` (short)
2. `p_acpt_architecture` (enum)
3. `p_acpt_type` (unsigned short)

`op_accept_data` / `op_cond_accept` → `P_ACPD` fields:
1. `p_acpt_version` (short)
2. `p_acpt_architecture` (enum)
3. `p_acpt_type` (unsigned short)
4. `p_acpt_data` (CSTRING)
5. `p_acpt_plugin` (CSTRING)
6. `p_acpt_authenticated` (unsigned short)
7. `p_acpt_keys` (CSTRING)

### 4.3 User-Id Tags (CNCT_*)
Tags used in `p_cnct_user_id` include:
- `CNCT_user` (1) user name
- `CNCT_passwd` (2) password
- `CNCT_host` (4) host name
- `CNCT_group` (5) effective Unix group id
- `CNCT_user_verification` (6) request user verification
- `CNCT_specific_data` (7) auth plugin data
- `CNCT_plugin_name` (8) auth plugin name
- `CNCT_login` (9) login/user (same as `isc_dpb_user_name`)
- `CNCT_plugin_list` (10) client available plugins
- `CNCT_client_crypt` (11) client encryption preference

## 5. Protocol Types and Flags
`ptype_*` determines send mode and behavior:
- `ptype_batch_send` = 3
- `ptype_out_of_band` = 4
- `ptype_lazy_send` = 5

Flags in upper byte:
- `pflag_compress` = 0x100 (request compression)
- `pflag_win_sspi_nego` = 0x200 (Windows SSPI negotiate)

## 6. Operation Codes (P_OP)
All packets include `p_operation` as one of these opcodes. The authoritative numeric list is in `appendix_protocol_constants.md`.

## 7. Packet Struct Definitions
Canonical packet field ordering and nesting is defined in `appendix_protocol_structs.md`. These struct definitions are authoritative for all packet layouts.

## 7.1 Field Order by Opcode
The authoritative field order for each opcode is listed in `appendix_protocol_fields.md`.

## 8. State Machine (Request/Response Legality)
The authoritative protocol state machine is defined in `appendix_protocol_state_machine.md`.

### 6.1 Connection and Session
- `op_connect` = 1
- `op_exit` = 2
- `op_accept` = 3
- `op_reject` = 4
- `op_disconnect` = 6
- `op_response` = 9
- `op_dummy` = 71
- `op_response_piggyback` = 72
- `op_ping` = 93
- `op_accept_data` = 94
- `op_abort_aux_connection` = 95
- `op_cond_accept` = 98

### 6.2 Database Attach/Create/Detach
- `op_attach` = 19
- `op_create` = 20
- `op_detach` = 21
- `op_drop_database` = 81

### 6.3 Request/BLR Execution
- `op_compile` = 22
- `op_start` = 23
- `op_start_and_send` = 24
- `op_send` = 25
- `op_receive` = 26
- `op_unwind` = 27
- `op_release` = 28

### 6.4 Transactions
- `op_transaction` = 29
- `op_commit` = 30
- `op_rollback` = 31
- `op_prepare` = 32
- `op_reconnect` = 33
- `op_commit_retaining` = 50
- `op_prepare2` = 51
- `op_rollback_retaining` = 86
- `op_transact` = 79
- `op_transact_response` = 80

### 6.5 Blob and Slice
- `op_create_blob` = 34
- `op_open_blob` = 35
- `op_get_segment` = 36
- `op_put_segment` = 37
- `op_cancel_blob` = 38
- `op_close_blob` = 39
- `op_batch_segments` = 44
- `op_open_blob2` = 56
- `op_create_blob2` = 57
- `op_get_slice` = 58
- `op_put_slice` = 59
- `op_slice` = 60
- `op_seek_blob` = 61
- `op_inline_blob` = 114

### 6.6 DSQL
- `op_allocate_statement` = 62
- `op_execute` = 63
- `op_exec_immediate` = 64
- `op_fetch` = 65
- `op_fetch_response` = 66
- `op_free_statement` = 67
- `op_prepare_statement` = 68
- `op_set_cursor` = 69
- `op_info_sql` = 70
- `op_start_and_receive` = 73
- `op_start_send_and_receive` = 74
- `op_exec_immediate2` = 75
- `op_execute2` = 76
- `op_insert` = 77
- `op_sql_response` = 78
- `op_fetch_scroll` = 112
- `op_info_cursor` = 113

### 6.7 Events
- `op_que_events` = 48
- `op_cancel_events` = 49
- `op_event` = 52

### 6.8 Info Requests
- `op_info_database` = 40
- `op_info_request` = 41
- `op_info_transaction` = 42
- `op_info_blob` = 43
- `op_service_info` = 84
- `op_info_batch` = 111
- `op_info_cursor` = 113

### 6.9 Services
- `op_service_attach` = 82
- `op_service_detach` = 83
- `op_service_start` = 85

### 6.10 Auth/Crypt/Cancel
- `op_update_account_info` = 87
- `op_authenticate_user` = 88
- `op_trusted_auth` = 90
- `op_cancel` = 91
- `op_cont_auth` = 92
- `op_crypt` = 96
- `op_crypt_key_callback` = 97

### 6.11 Batch API
- `op_batch_create` = 99
- `op_batch_msg` = 100
- `op_batch_exec` = 101
- `op_batch_rls` = 102
- `op_batch_cs` = 103
- `op_batch_regblob` = 104
- `op_batch_blob_stream` = 105
- `op_batch_set_bpb` = 106
- `op_batch_cancel` = 109
- `op_batch_sync` = 110

### 6.12 Replication
- `op_repl_data` = 107
- `op_repl_req` = 108

## 7. Per-Opcode Field Layouts (Authoritative XDR Order)
These layouts are encoded by the field orders in `formal/protocol_fields.json`.

### 7.1 No-Payload Ops
`op_reject`, `op_disconnect`, `op_dummy`, `op_ping`, `op_abort_aux_connection` have no payload beyond `p_operation`.

### 7.2 `op_connect`
`P_CNCT` fields, in order as defined in section 4.1.

### 7.3 `op_accept`
`P_ACPT` fields, in order as defined in section 4.2.

### 7.4 `op_accept_data`, `op_cond_accept`
`P_ACPD` fields, in order as defined in section 4.2.

### 7.5 `op_connect_request`, `op_aux_connect`
`P_REQ` fields:
- `p_req_type` (short)
- `p_req_object` (short)
- `p_req_partner` (long)

### 7.6 `op_attach`, `op_create`, `op_service_attach`
`P_ATCH` fields:
- `p_atch_database` (short)
- `p_atch_file` (CSTRING_CONST)
- `p_atch_dpb` (CSTRING_CONST)

### 7.7 `op_compile`
`P_CMPL` fields:
- `p_cmpl_database` (short)
- `p_cmpl_blr` (CSTRING_CONST)

### 7.8 `op_receive`, `op_start`, `op_start_and_receive`
`P_DATA` fields:
- `p_data_request` (short)
- `p_data_incarnation` (short)
- `p_data_transaction` (short)
- `p_data_message_number` (short)
- `p_data_messages` (short)

### 7.9 `op_send`, `op_start_and_send`, `op_start_send_and_receive`
Same `P_DATA` fields as 7.8, then `xdr_request()` payload:
- Message data in request format (`xdr_message`) for the given request ID, message number, and incarnation.

### 7.10 `op_response`, `op_response_piggyback`
`P_RESP` fields:
- `p_resp_object` (short)
- `p_resp_blob_id` (quad)
- `p_resp_data` (xdr_response)
- `p_resp_status_vector` (xdr_status_vector)

### 7.11 `op_transact`
`P_TRRQ` fields:
- `p_trrq_database` (short)
- `p_trrq_transaction` (short)
- `p_trrq_blr` (CSTRING)
- `p_trrq_messages` (short)
- If `p_trrq_messages > 0`: `xdr_trrq_message(msg_type=0)`

### 7.12 `op_transact_response`
Fields:
- `p_data_messages` (short)
- If `p_data_messages > 0`: `xdr_trrq_message(msg_type=1)`

### 7.13 `op_open_blob2`, `op_create_blob2`
Fields:
- `p_blob_bpb` (CSTRING_CONST)
- `p_blob_transaction` (short)
- `p_blob_id` (quad)

### 7.14 `op_open_blob`, `op_create_blob`
Fields:
- `p_blob_transaction` (short)
- `p_blob_id` (quad)

### 7.15 `op_get_segment`, `op_put_segment`, `op_batch_segments`
`P_SGMT` fields:
- `p_sgmt_blob` (short)
- `p_sgmt_length` (short)
- `p_sgmt_segment` (CSTRING_CONST)

### 7.16 `op_seek_blob`
`P_SEEK` fields:
- `p_seek_blob` (short)
- `p_seek_mode` (short)
- `p_seek_offset` (long)

### 7.17 `op_reconnect`, `op_transaction`
`P_STTR` fields:
- `p_sttr_database` (short)
- `p_sttr_tpb` (CSTRING_CONST)

### 7.18 `op_info_blob`, `op_info_database`, `op_info_request`, `op_info_transaction`, `op_service_info`, `op_info_sql`, `op_info_batch`, `op_info_cursor`
`P_INFO` fields:
- `p_info_object` (short)
- `p_info_incarnation` (short)
- `p_info_items` (CSTRING_CONST)
- If `op_service_info`: `p_info_recv_items` (CSTRING_CONST)
- `p_info_buffer_length` (long). If decoded length has high 16 bits `0xFFFF`, mask to 16-bit.

### 7.19 `op_service_start`
`P_INFO` fields (subset):
- `p_info_object` (short)
- `p_info_incarnation` (short)
- `p_info_items` (CSTRING_CONST)

### 7.20 Release-Style Ops
`op_commit`, `op_prepare`, `op_rollback`, `op_unwind`, `op_release`, `op_close_blob`, `op_cancel_blob`,
`op_detach`, `op_drop_database`, `op_service_detach`, `op_commit_retaining`, `op_rollback_retaining`,
`op_allocate_statement`, `op_batch_rls`, `op_batch_cancel`:
- `p_rlse_object` (short)

### 7.21 `op_prepare2`
`P_PREP` fields:
- `p_prep_transaction` (short)
- `p_prep_data` (CSTRING_CONST)

### 7.22 `op_que_events`, `op_event`
`P_EVENT` fields:
- `p_event_database` (short)
- `p_event_items` (CSTRING_CONST)
- `p_event_ast` (long)
- `p_event_arg` (long)
- `p_event_rid` (long)

### 7.23 `op_cancel_events`
`P_EVENT` fields (subset):
- `p_event_database` (short)
- `p_event_rid` (long)

### 7.24 `op_ddl`
`P_DDL` fields:
- `p_ddl_database` (short)
- `p_ddl_transaction` (short)
- `p_ddl_blr` (CSTRING_CONST)

### 7.25 `op_get_slice`, `op_put_slice`
`P_SLC` fields:
- `p_slc_transaction` (short)
- `p_slc_id` (quad)
- `p_slc_length` (long)
- `p_slc_sdl` (CSTRING)
- `p_slc_parameters` (xdr_longs)
- `p_slc_slice` (xdr_slice)

### 7.26 `op_slice`
`P_SLR` fields:
- `p_slr_length` (long)
- `p_slr_slice` (xdr_slice)

### 7.27 `op_execute`, `op_execute2`
`P_SQLDATA` fields:
- `p_sqldata_statement` (short)
- `p_sqldata_transaction` (short)
- `p_sqldata_blr` (xdr_sql_blr, prepared)
- `p_sqldata_message_number` (short)
- `p_sqldata_messages` (short)
- If `p_sqldata_messages > 0`: `xdr_sql_message(statement_id)`
- If `op_execute2`: `p_sqldata_out_blr` (xdr_sql_blr, output), `p_sqldata_out_message_number` (short)
- If protocol >= 16: `p_sqldata_timeout` (ulong)
- If protocol >= 18: `p_sqldata_cursor_flags` (ulong)
- If protocol >= 19: `p_sqldata_inline_blob_size` (ulong)

### 7.28 `op_exec_immediate2`
Fields:
- `p_sqlst_blr` (xdr_sql_blr, immediate)
- `p_sqlst_message_number` (short)
- `p_sqlst_messages` (short)
- If `p_sqlst_messages > 0`: `xdr_sql_message(statement_id=-1)`
- `p_sqlst_out_blr` (xdr_sql_blr, output)
- `p_sqlst_out_message_number` (short)
- If protocol >= 19: `p_sqlst_inline_blob_size` (ulong)
- Then fall-through to `op_exec_immediate`/`op_prepare_statement` fields (section 7.29)

### 7.29 `op_exec_immediate`, `op_prepare_statement`
`P_SQLST` fields:
- `p_sqlst_transaction` (short)
- `p_sqlst_statement` (short)
- `p_sqlst_SQL_dialect` (short)
- `p_sqlst_SQL_str` (CSTRING_CONST)
- `p_sqlst_items` (CSTRING_CONST)
- `p_sqlst_buffer_length` (long). If decoded length has high 16 bits `0xFFFF`, mask to 16-bit.
- If protocol >= 20: `p_sqlst_flags` (short)

### 7.30 `op_fetch`, `op_fetch_scroll`
`P_SQLDATA` fields:
- `p_sqldata_statement` (short)
- `p_sqldata_blr` (xdr_sql_blr, output)
- `p_sqldata_message_number` (short)
- `p_sqldata_messages` (short)
- If `op_fetch_scroll`: `p_sqldata_fetch_op` (short), `p_sqldata_fetch_pos` (long)

### 7.31 `op_fetch_response`
Fields:
- `p_sqldata_status` (long)
- `p_sqldata_messages` (short)
- If `p_sqldata_messages > 0`: `xdr_sql_message(statement_id)`

### 7.32 `op_free_statement`
`P_SQLFREE` fields:
- `p_sqlfree_statement` (short)
- `p_sqlfree_option` (short)

### 7.33 `op_set_cursor`
`P_SQLCUR` fields:
- `p_sqlcur_statement` (short)
- `p_sqlcur_cursor_name` (CSTRING_CONST)
- `p_sqlcur_type` (short)

### 7.34 `op_sql_response`
Fields:
- `p_sqldata_messages` (short)
- If `p_sqldata_messages > 0`: `xdr_sql_message(statement_id=-1)`

### 7.35 `op_update_account_info`
`p_update_account` fields:
- `p_account_database` (short)
- `p_account_apb` (CSTRING_CONST)

### 7.36 `op_authenticate_user`
`p_authenticate` fields:
- `p_auth_database` (short)
- `p_auth_dpb` (CSTRING_CONST)
- `p_auth_items` (CSTRING)
- `p_auth_buffer_length` (short)

### 7.37 `op_trusted_auth`
`P_TRAU` fields:
- `p_trau_data` (CSTRING)

### 7.38 `op_cont_auth`
`P_AUTH_CONT` fields:
- `p_data` (CSTRING)
- `p_name` (CSTRING)
- `p_list` (CSTRING)
- `p_keys` (CSTRING)

### 7.39 `op_cancel`
`P_CANCEL_OP` fields:
- `p_co_kind` (short)

### 7.40 `op_crypt`
`P_CRYPT` fields:
- `p_plugin` (CSTRING)
- `p_key` (CSTRING)

### 7.41 `op_crypt_key_callback`
`P_CRYPT_CALLBACK` fields:
- `p_cc_data` (CSTRING)
- If protocol >= 14 or protocol is 0 (connect phase): `p_cc_reply` (short)

### 7.42 `op_batch_create`
`P_BATCH_CREATE` fields:
- `p_batch_statement` (short)
- `p_batch_blr` (CSTRING_CONST)
- `p_batch_msglen` (ulong)
- `p_batch_pb` (CSTRING_CONST)

### 7.43 `op_batch_msg`
`P_BATCH_MSG` fields:
- `p_batch_statement` (short)
- `p_batch_messages` (ulong)
- `p_batch_data` (packed message buffer). Encoding uses `xdr_packed_message` repeatedly against the statement’s bind format. Each message is aligned to `FB_ALIGNMENT` and size is `FB_ALIGN(fmt_length, FB_ALIGNMENT)`.

### 7.44 `op_batch_exec`
`P_BATCH_EXEC` fields:
- `p_batch_statement` (short)
- `p_batch_transaction` (short)

### 7.45 `op_batch_cs`
`P_BATCH_CS` fields:
- `p_batch_statement` (short)
- `p_batch_reccount` (ulong)
- `p_batch_updates` (ulong)
- `p_batch_vectors` (ulong)
- `p_batch_errors` (ulong)
- Then a variable number of update counts and status vectors as serialized by `xdr_status_vector` and indexed positions.

### 7.46 `op_batch_sync`
No payload.

### 7.47 `op_batch_set_bpb`
`P_BATCH_SETBPB` fields:
- `p_batch_statement` (short)
- `p_batch_blob_bpb` (CSTRING_CONST)

### 7.48 `op_batch_regblob`
`P_BATCH_REGBLOB` fields:
- `p_batch_statement` (short)
- `p_batch_exist_id` (quad)
- `p_batch_blob_id` (quad)

### 7.49 `op_batch_blob_stream`
`P_BATCH_BLOB` fields:
- `p_batch_statement` (short)
- `p_batch_blob_data` (custom blob stream). Encoded by `xdr_blob_stream`, which prefixes a `ulong` length and then transfers a packed stream aligned to `Rsr::BatchStream::alignment`.

### 7.50 `op_repl_data`
`P_REPLICATE` fields:
- `p_repl_database` (short)
- `p_repl_data` (CSTRING_CONST)

### 7.51 `op_inline_blob`
`P_INLINE_BLOB` fields:
- `p_tran_id` (short)
- `p_blob_id` (quad)
- `p_blob_info` (xdr_response)
- `p_blob_data` (blob buffer via `xdr_blobBuffer`)

## 8. SQL Message Encoding
### 8.1 `xdr_sql_blr`
Encodes a BLR message description as a `CSTRING`. On decode, BLR is parsed into a `rem_fmt` and stored as the statement’s bind or select format.

### 8.2 `xdr_sql_message`
For protocol >= 13, SQL messages use `xdr_packed_message`; for protocol < 13, use `xdr_message`.

### 8.3 `xdr_packed_message`
Packed message uses a NULL bitmap:
- For a message with `N` columns, there are `N` value descriptors and `N` NULL-indicator descriptors; the bitmap size is `ceil(N/8)` bytes.
- Encode:
  1. Build bitmap from NULL indicators (odd descriptors).
  2. Send bitmap bytes.
  3. Send only non-NULL values (even descriptors) via `xdr_datum`.
- Decode:
  1. Receive bitmap.
  2. Populate NULL indicators.
  3. Receive only non-NULL values.

## 9. Status Vector Encoding
`xdr_status_vector` sends a sequence of ISC_STATUS items:
- Each item starts with a `SLONG` tag.
- For `isc_arg_interpreted`, `isc_arg_string`, `isc_arg_sql_state`, the next field is a string via `xdr_wrapstring`.
- For `isc_arg_number` and defaults, the next field is a `SLONG`.
- `isc_arg_end` terminates the vector.

## 10. Next Sections to Expand
This document will be expanded with:
- Authentication flows (SRP, plugins, trusted auth)
- Compression negotiation and record layering
- Full blob-stream layout for batch and inline blobs

## 11. Authentication Flows (Overview)
Firebird wire authentication is plugin-based. The wire protocol defines the packet exchange; the authentication algorithm is implemented by plugins (e.g., SRP).

### 11.1 Connect Phase (Plugin Negotiation)
- Client sends `op_connect` with `p_cnct_user_id` containing CNCT tags, notably `CNCT_plugin_list` (available plugins), `CNCT_plugin_name`, `CNCT_specific_data` (plugin data), and credentials.
- Server responds with `op_accept` (auth complete) or `op_accept_data` / `op_cond_accept` if additional auth data or continuation is required.
- If continuation is required, server includes:
  - `p_acpt_data` (opaque plugin data)
  - `p_acpt_plugin` (selected plugin name)
  - `p_acpt_authenticated` flag (0/1)
  - `p_acpt_keys` (server known keys)
- Client then continues with `op_cont_auth` (P_AUTH_CONT) containing plugin data and selected plugin list/name.

### 11.2 SRP Plugins
- Firebird uses SRP authentication via plugins; supported names include `Srp` (SHA-1 proof) and `Srp256` (SHA-256 proof).
- Client and server must have a common SRP plugin; the plugin name is exchanged during connect/auth continuation.

### 11.3 Trusted Authentication
- `op_trusted_auth` carries `P_TRAU` data for trusted auth flows (platform dependent). This is used when server accepts external authentication without password data in the DPB.

## 12. Wire Compression
- Compression is negotiated at connect time and enabled by the client using the protocol type flags (`pflag_compress`).
- Compression is supported for protocol >= 13 and requires `WireCompression=true` on the client configuration.
- When active, server and client compress payload data using zlib at the remote layer. Compression is indicated by `Z` in version strings.

## 13. Wire Encryption (Crypt)
- `op_crypt` initiates encryption negotiation; `P_CRYPT` contains plugin name and key selector.
- `op_crypt_key_callback` can be used during connect and later to obtain or confirm cryptographic keys.
- Encrypted connections are indicated in address path flags (`isc_dpb_addr_flag_conn_encrypted`) in DPB address clumplets.

## 14. Connect Negotiation Algorithm (Authoritative)
Server-side selection behavior (authoritative):
- Accepts only protocol versions 10 or 11-20 inclusive.
- Chooses the **highest weight** entry that matches architecture (client arch is `arch_generic` or server `ARCHITECTURE`).
- Selected `ptype` is `min(p_cnct_max_type & ptype_MASK, ptype_lazy_send)`.
- Compression is enabled if `p_cnct_max_type` has `pflag_compress` set; server returns `pflag_compress` in `p_acpt_type`.
- `p_acpt_architecture` is set to selected architecture; `p_acpt_version` is selected protocol version.
- If chosen architecture equals server `ARCHITECTURE`, server sets `PORT_symmetric`.
- If `ptype` is not `ptype_out_of_band`, server sets `PORT_no_oob`.
- If `ptype` is `ptype_lazy_send`, server sets `PORT_lazy`.
- For `CONNECT_VERSION3` or higher, user-id strings and database name in the connect packet are UTF-8; server converts to system encoding.

Compression activation:
- After `op_accept`/`op_accept_data`, server calls `initCompression()` if `pflag_compress` is set and sets `PORT_compressed`.

## 15. Authentication Handshake (Detailed)
Server-side behavior (protocol >= 13) from `accept_connection()` and `continue_authentication()`:
1. Client sends `op_connect` with `p_cnct_user_id` (untagged clumplet stream).
2. Server loads client auth data into `SrvAuthBlock` and attempts to select plugin:
   - If client-provided plugin name differs from server-selected plugin, server responds with `op_accept_data` and `p_acpt_plugin` containing server plugin; client must continue with `op_cont_auth`.
   - If plugin authenticate() returns `AUTH_SUCCESS`, server sets `p_acpt_authenticated = 1` and returns `op_accept_data`.
   - If `AUTH_MORE_DATA`, server returns `op_accept_data` with `p_acpt_data` and `p_acpt_plugin`.
   - If `AUTH_CONTINUE`, server advances to next plugin; if none, authentication fails.
   - If `AUTH_FAILED`, authentication fails and server sends login error.
3. Client continues using `op_cont_auth` (protocol >= 13) or `op_trusted_auth` (legacy / trusted auth). Server accepts only the matching op for the negotiated protocol.
4. On `op_cont_auth`, server extracts `P_AUTH_CONT` data and continues the plugin sequence.
5. On success, server may return `op_accept_data` with `p_acpt_keys` containing known encryption keys and `p_acpt_authenticated=1`.
6. On failure, server sends an `op_response` with login error status and disconnects.

`ConnectAuth::accept()` behavior:
- If `useResponse` is set, server responds with `op_response` and `p_resp_data` containing new keys; status vector success.
- Otherwise, server responds `op_accept_data` with `p_acpt_keys` and `p_acpt_authenticated=1`.

## 16. Encryption Negotiation
- If `wireEncryption()` requires crypt and protocol >= 13, server initializes `ConnectAuth` and may require conditional accept (`op_cond_accept`).
- If `WIRECRYPT_REQUIRED` and client does not support encryption, server rejects the connection.
- `op_crypt` / `op_crypt_key_callback` are used for wire encryption setup during and after connect.

## 17. Blob Segment Batch Encoding (op_batch_segments)
- `op_batch_segments` uses `P_SGMT` with `p_sgmt_segment` containing a concatenated series of segments.
- Each segment is prefixed by a 2-byte little-endian length (low byte then high byte), followed by that many bytes of segment data.
- Server iterates through the buffer and calls `putSegment(length, data)` for each segment.

## 18. Blob Defaults and Limits
- Default blob segment buffer length: `BLOB_LENGTH = 16384` bytes.
- Inline blob max size defaults to `DEFAULT_INLINE_BLOB_SIZE = MAX_USHORT`.

## 19. op_get_segment Response Semantics
Server response to `op_get_segment` is `op_response` with:
- `p_resp_object` = state code:
  - `0` = success, more data may remain
  - `1` = segment truncated (buffer filled before end of segment)
  - `2` = no more data
- `p_resp_data` = buffer containing one or more segments; each segment is encoded as: 2-byte little-endian length followed by segment bytes. Total response length is `p_resp_data.cstr_length`.

This behavior is defined by this document.

## 20. op_get_slice Response Semantics
- On success, server sends `op_slice` with `P_SLR` (length + slice data encoded by `xdr_slice`).
- On error, server sends `op_response` with status vector.

## 21. Fetch Response Semantics
- `op_fetch_response` is emitted by server after fetch operations.
- The server may send **multiple `op_fetch_response` packets** with row data via `send_partial()` before sending the final `op_fetch_response` with `p_sqldata_messages=0` and `p_sqldata_status` set to 0 (success) or 100 (no data). This implements prefetch batching.
- Each partial response carries one message (`p_sqldata_messages=1`) and uses the statement's select format for the row.
- The final response carries no row data (`p_sqldata_messages=0`) and communicates stream end via `p_sqldata_status=100`.
- Inline blob data may be transmitted between rows using `op_inline_blob` when `rsr_inline_blob_size` is set and protocol >= 19.

## 22. op_response Semantics
- `op_response` is the generic completion response for most operations.
- `p_resp_object` may carry a handle (object id) or other numeric value specific to the op.
- `p_resp_data` carries optional data; `p_resp_data.cstr_length` is set explicitly by the server.
- A status vector is always present; errors are reported here (not via `p_sqldata_status`).
- When `PORT_lazy` is set and `defer_flag` is true, server may send `op_response` via `send_partial()`.

## 23. op_response_piggyback Semantics
- Used by `receive_after_start` to inform client about message number while continuing to receive data.
- `p_resp_object` contains the message number. `p_resp_data` is empty.
- Status vector contains the current status of receive operation.

## 24. Asynchronous Event Channel (op_connect_request / op_aux_connect)
- The client requests an auxiliary asynchronous connection by sending `op_connect_request` on the main port.
- Server handles `op_connect_request` via `aux_request()` and responds with `op_response` containing:
  - `p_resp_object` = database attachment id (`rdb_id`)
  - `p_resp_data` = transport-specific server identification string
- Server then attempts to establish the auxiliary port (`op_aux_connect`) and sets `PORT_async` on the new port.
- Event notifications are delivered on the async port as `op_event` packets.

This behavior is defined by this document.

## 25. Inline Blob Buffer Encoding
`op_inline_blob` uses `xdr_blobBuffer` for `p_blob_data`:
- `len` (SLONG) followed by `len` bytes of blob buffer.
- Padding to 4-byte boundary with zero bytes.
- This encoding matches `CSTRING` padding rules but is a raw byte array (no max length enforcement).

## 26. Wire Compression (Detailed)
Compression is applied to the XDR stream at the port layer as defined in this document.
- Compression uses zlib `deflate`/`inflate`.
- When enabled, outgoing data is compressed by `REMOTE_deflate()`; incoming data is decompressed by `REMOTE_inflate()`.
- Flush behavior: `deflate()` is called with `Z_SYNC_FLUSH` when the protocol layer requests flush; otherwise `Z_NO_FLUSH`.
- Compressed bytes are sent using the same packet framing as uncompressed bytes (no additional wire framing beyond the transport’s packet boundaries).
- Compression is only available for protocol >= 13.
