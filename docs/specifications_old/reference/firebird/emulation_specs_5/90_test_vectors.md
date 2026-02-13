# Firebird 5 Protocol Test Vectors — Draft 1

These vectors provide concrete wire-level byte sequences for common flows. All packets are XDR-encoded as defined in `30_wire_protocol.md`, with field order per `appendix_protocol_fields.md` and constants per `appendix_protocol_constants.md` and `appendix_parameter_blocks.md`.

## Conventions Used In These Vectors
All integers are XDR big-endian. `xdr_short` values are encoded as 32-bit signed ints (sign-extended) as specified in `30_wire_protocol.md`. `CSTRING` values are encoded as a 32-bit length followed by bytes, then zero padding to a 4-byte boundary.

All hex dumps are full packet payloads, starting at `p_operation`.

## 1) Connect/Accept With SRP Auth (Two-Step)
The connect uses protocol version 20 (`0x8014`) and `ptype_lazy_send`. The user-id buffer is an untagged clumplet stream:
`CNCT_user`=SYSDBA, `CNCT_host`=client, `CNCT_plugin_list`=Srp, `CNCT_specific_data`=01 02 03 04.

1. Client → Server: `op_connect` with one protocol entry (`PROTOCOL_VERSION20`, `arch_generic`, `ptype_lazy_send`).
```hex
00 00 00 01 00 00 00 00 00 00 00 03 00 00 00 01 00 00 00 08 74 65 73 74 2e 66 64 62 00 00 00 01 00 00 00 1b 01 06 53 59 53 44 42 41 04 06 63 6c 69 65 6e 74 0a 03 53 72 70 07 04 01 02 03 04 00 ff ff 80 14 00 00 00 01 00 00 00 00 00 00 00 05 00 00 00 01
```

2. Server → Client: `op_accept_data` with SRP challenge and plugin name `Srp`, authenticated=0.
```hex
00 00 00 5e ff ff 80 14 00 00 00 01 00 00 00 05 00 00 00 04 11 22 33 44 00 00 00 03 53 72 70 00 00 00 00 00 00 00 00 00
```

3. Client → Server: `op_cont_auth` with SRP response data, plugin name/list `Srp`.
```hex
00 00 00 5c 00 00 00 08 53 52 50 5f 52 45 53 50 00 00 00 03 53 72 70 00 00 00 00 03 53 72 70 00 00 00 00 00
```

4. Server → Client: `op_accept_data` authenticated=1, no keys.
```hex
00 00 00 5e ff ff 80 14 00 00 00 01 00 00 00 05 00 00 00 00 00 00 00 03 53 72 70 00 00 00 00 01 00 00 00 00
```

## 2) Attach With DPB (Success)
DPB bytes used here: `isc_dpb_version1`, `isc_dpb_user_name=SYSDBA`, `isc_dpb_password=masterkey`.

1. Client → Server: `op_attach` to `test.fdb`.
```hex
00 00 00 13 00 00 00 00 00 00 00 08 74 65 73 74 2e 66 64 62 00 00 00 14 01 1c 06 53 59 53 44 42 41 1d 09 6d 61 73 74 65 72 6b 65 79
```

2. Server → Client: `op_response` success with database handle `1`, status vector `isc_arg_end`.
```hex
00 00 00 09 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## 3) Prepare/Execute/Select/Fetch Row (Single-Row Example)
This sequence shows a minimal DSQL path. It assumes the server already knows the output format for the statement from prepare/open and accepts an empty BLR in `op_fetch`.

1. Client → Server: `op_transaction` with TPB bytes `[isc_tpb_version3, isc_tpb_write, isc_tpb_nowait]`.
```hex
00 00 00 1d 00 00 00 01 00 00 00 03 03 09 07 00
```

2. Server → Client: `op_response` success with transaction handle `2`.
```hex
00 00 00 09 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

3. Client → Server: `op_allocate_statement` for database handle `1`.
```hex
00 00 00 3e 00 00 00 01
```

4. Server → Client: `op_response` success with statement handle `10`.
```hex
00 00 00 09 00 00 00 0a 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

5. Client → Server: `op_prepare_statement` for SQL `SELECT 1 FROM RDB$DATABASE`, dialect 3.
```hex
00 00 00 44 00 00 00 02 00 00 00 0a 00 00 00 03 00 00 00 1a 53 45 4c 45 43 54 20 31 20 46 52 4f 4d 20 52 44 42 24 44 41 54 41 42 41 53 45 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

6. Client → Server: `op_execute` with empty input BLR/message, timeout=0, cursor flags=0.
```hex
00 00 00 3f 00 00 00 0a 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

7. Client → Server: `op_fetch` for one row, empty output BLR (server-known format).
```hex
00 00 00 41 00 00 00 0a 00 00 00 00 00 00 00 00 00 00 00 01
```

8. Server → Client: `op_fetch_response` with one packed row, one column value=1, not null.
The packed message bytes are: bitmap `00` + 3 padding bytes + `xdr_long(1)`.
```hex
00 00 00 42 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 01
```

9. Server → Client: final `op_fetch_response` end-of-stream (`p_sqldata_status=100`, `p_sqldata_messages=0`).
```hex
00 00 00 42 00 00 00 64 00 00 00 00
```

## 4) Error Response With Warning Vector
This example returns `isc_login_error` (code `0x14000312`) with a warning `SQLWARN 100` (code `0x140E0064`). Status vector sequence is:
`isc_arg_gds`, error_code, `isc_arg_warning`, warning_code, `isc_arg_end`.

```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 14 00 03 12 00 00 00 12 14 0e 00 64 00 00 00 00
```

## 5) Blob Create/Write/Close Then Open/Read/Close
This sequence uses transaction handle `2` and a server-assigned blob ID `(0,1)`.

1. Client → Server: `op_create_blob` with transaction `2`, zero blob id.
```hex
00 00 00 22 00 00 00 02 00 00 00 00 00 00 00 00
```

2. Server → Client: `op_response` with blob handle `3` and blob id `(0,1)`.
```hex
00 00 00 09 00 00 00 03 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00
```

3. Client → Server: `op_put_segment` with blob handle `3`, segment `"hello"`.
```hex
00 00 00 25 00 00 00 03 00 00 00 05 00 00 00 05 68 65 6c 6c 6f 00 00 00
```

4. Server → Client: `op_response` success.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

5. Client → Server: `op_close_blob` for handle `3`.
```hex
00 00 00 27 00 00 00 03
```

6. Server → Client: `op_response` success.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

7. Client → Server: `op_open_blob` with transaction `2` and blob id `(0,1)`.
```hex
00 00 00 23 00 00 00 02 00 00 00 00 00 00 00 01
```

8. Server → Client: `op_response` with blob handle `4`.
```hex
00 00 00 09 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

9. Client → Server: `op_get_segment` for up to 5 bytes (empty segment data in request).
```hex
00 00 00 24 00 00 00 04 00 00 00 05 00 00 00 00
```

10. Server → Client: `op_response` with `p_resp_object=0` and `p_resp_data` containing one segment.
`p_resp_data` bytes are little-endian segment length `05 00` followed by `"hello"`.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 07 05 00 68 65 6c 6c 6f 00 00 00 00 00
```

## 6) Database Info Query (isc_info_db_sql_dialect)
Request `isc_info_db_sql_dialect` and `isc_info_end`. Response returns a single-byte dialect value (3) plus `isc_info_end`.

1. Client → Server: `op_info_database` for database handle `1`, buffer length 32.
```hex
00 00 00 28 00 00 00 01 00 00 00 00 00 00 00 02 3e 01 00 00 00 00 00 20
```

2. Server → Client: `op_response` with `p_resp_data` containing info clumplets:
`3e 01 00 03` (tag=62, len=1, value=3) followed by `01` (isc_info_end).
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 05 3e 01 00 03 01 00 00 00 00 00 00 00
```

## 7) Service Manager: Attach, Start, Info
This example attaches to the service manager, starts the display-user action, and queries `isc_info_svc_get_users`.

1. Client → Server: `op_service_attach` to `service_mgr` with SPB (user SYSDBA, password masterkey).
```hex
00 00 00 52 00 00 00 00 00 00 00 0b 73 65 72 76 69 63 65 5f 6d 67 72 00 00 00 00 14 01 1c 06 53 59 53 44 42 41 1d 09 6d 61 73 74 65 72 6b 65 79
```

2. Server → Client: `op_response` success with service handle `5`.
```hex
00 00 00 09 00 00 00 05 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

3. Client → Server: `op_service_start` with SPB start action `isc_action_svc_display_user` (7).
```hex
00 00 00 55 00 00 00 05 00 00 00 01 07 00 00 00
```

4. Server → Client: `op_response` success.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

5. Client → Server: `op_service_info` requesting `isc_info_svc_get_users` and `isc_info_end`.
```hex
00 00 00 54 00 00 00 05 00 00 00 00 00 00 00 02 44 01 00 00 00 00 00 80
```

6. Server → Client: `op_response` with SPB response clumplets.
`44 08 00 53 59 53 44 42 41 0a` is `isc_info_svc_get_users` with 2‑byte length `0x0008` and data `SYSDBA\n`, followed by `isc_info_end` (01).
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0b 44 08 00 53 59 53 44 42 41 0a 01 00 00 00 00 00
```

## 8) Event Queue and Async Event Delivery
Event block format: `EPB_version1`, then repeated `[name_length][name_bytes]`. Event response adds a 4‑byte count after each name.

1. Client → Server: `op_que_events` for database handle `1`, events `EV1` and `EV2`, request id `77`.
```hex
00 00 00 30 00 00 00 01 00 00 00 09 01 03 45 56 31 03 45 56 32 00 00 00 00 00 00 00 00 00 00 00 00 00 00 4d
```

2. Server → Client: `op_response` success.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

3. Server → Client (async on event port): `op_event` with counts `EV1=1`, `EV2=0` for request id `77`.
```hex
00 00 00 34 00 00 00 01 00 00 00 11 01 03 45 56 31 00 00 00 01 03 45 56 32 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 4d
```

## 9) Service Info: Line and To-EOF
These vectors request service output lines using `isc_info_svc_line` and `isc_info_svc_to_eof`.

1. Client → Server: `op_service_info` request `isc_info_svc_line` + `isc_info_end`.
```hex
00 00 00 54 00 00 00 05 00 00 00 00 00 00 00 02 3e 01 00 00 00 00 00 80
```

2. Server → Client: `op_response` with `isc_info_svc_line="line1\n"`, then `isc_info_end`.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0a 3e 06 00 6c 69 6e 65 31 0a 01 00 00 00 00 00 00
```

3. Client → Server: `op_service_info` request `isc_info_svc_to_eof` + `isc_info_end`.
```hex
00 00 00 54 00 00 00 05 00 00 00 00 00 00 00 02 3f 01 00 00 00 00 00 80
```

4. Server → Client: `op_response` with `isc_info_svc_to_eof="all output\n"`, then `isc_info_end`.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0f 3f 0b 00 61 6c 6c 20 6f 75 74 70 75 74 0a 01 00 00 00 00 00
```

## 10) Request Info and Transaction Info
Info responses for numeric values encode the payload as VAX little-endian integers. The length field is 2‑byte little-endian.

1. Client → Server: `op_info_request` for request handle `12`, items `isc_info_req_active`, `isc_info_end`.
```hex
00 00 00 29 00 00 00 0c 00 00 00 00 00 00 00 02 02 01 00 00 00 00 00 20
```

2. Server → Client: `op_response` with `isc_info_req_active=2` and `isc_info_end`.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 08 02 04 00 02 00 00 00 01 00 00 00 00
```

3. Client → Server: `op_info_transaction` for transaction handle `2`, items `isc_info_tra_id`, `isc_info_end`.
```hex
00 00 00 2a 00 00 00 02 00 00 00 00 00 00 00 02 04 01 00 00 00 00 00 20
```

4. Server → Client: `op_response` with `isc_info_tra_id=2` and `isc_info_end`.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 08 04 04 00 02 00 00 00 01 00 00 00 00
```

## 11) Cancel Events
Cancel a previously queued event request id.

1. Client → Server: `op_cancel_events` for database handle `1`, request id `77`.
```hex
00 00 00 31 00 00 00 01 00 00 00 4d
```

2. Server → Client: `op_response` success.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## 12) Database Info With isc_info_length
When the request begins with `isc_info_length`, the response includes a `isc_info_length` clumplet containing the total byte length of the subsequent info items (excluding the `isc_info_length` clumplet itself).

1. Client → Server: `op_info_database` items `isc_info_length`, `isc_info_page_size`, `isc_info_db_sql_dialect`, `isc_info_end`.
```hex
00 00 00 28 00 00 00 01 00 00 00 00 00 00 00 04 7e 0e 3e 01 00 00 00 40
```

2. Server → Client: `op_response` with `isc_info_length=15`, page size 4096, sql dialect 3, then `isc_info_end`.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 16 7e 04 00 0f 00 00 00 0e 04 00 00 10 00 00 3e 04 00 03 00 00 00 01 00 00 00 00 00 00
```

## 13) get_segment: Truncated and End-of-Blob
These responses demonstrate `p_resp_object` states for segment truncation and end-of-blob.

1. Client → Server: `op_get_segment` for blob handle `4`, request length 3.
```hex
00 00 00 24 00 00 00 04 00 00 00 03 00 00 00 00
```

2. Server → Client: `op_response` with `p_resp_object=1` (segment truncated), data contains one segment length `03 00` and bytes `hel`.
```hex
00 00 00 09 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 05 03 00 68 65 6c 00 00 00 00 00 00 00
```

3. Server → Client: `op_response` with `p_resp_object=2` (no more data), empty data.
```hex
00 00 00 09 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## 14) Fetch Scroll (Absolute/Relative)
Scrollable fetch using `op_fetch_scroll` with `fetch_absolute` and `fetch_relative`.

1. Client → Server: `op_fetch_scroll` absolute position 1.
```hex
00 00 00 70 00 00 00 0a 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 04 00 00 00 01
```

2. Server → Client: `op_fetch_response` with one row value 42 (packed message, one column).
```hex
00 00 00 42 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 2a
```

3. Client → Server: `op_fetch_scroll` relative position -1.
```hex
00 00 00 70 00 00 00 0a 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 05 ff ff ff ff
```

4. Server → Client: `op_fetch_response` end-of-stream.
```hex
00 00 00 42 00 00 00 64 00 00 00 00
```

## 15) Blob Info Query
Returns blob segment statistics using `op_info_blob`.

1. Client → Server: `op_info_blob` for blob handle `4`, items `num_segments`, `max_segment`, `total_length`, `end`.
```hex
00 00 00 2b 00 00 00 04 00 00 00 00 00 00 00 04 04 05 06 01 00 00 00 40
```

2. Server → Client: `op_response` with values: num_segments=1, max_segment=5, total_length=5.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 16 04 04 00 01 00 00 00 05 04 00 05 00 00 00 06 04 00 05 00 00 00 01 00 00 00 00 00 00
```

## 16) SQL Info (Statement Type and Plan)
Request statement info via `op_info_sql` for a prepared statement.

1. Client → Server: `op_info_sql` items `isc_info_sql_stmt_type`, `isc_info_sql_get_plan`, `isc_info_end`.
```hex
00 00 00 46 00 00 00 0a 00 00 00 00 00 00 00 03 15 16 01 00 00 00 01 00
```

2. Server → Client: `op_response` with stmt_type=select (1) and plan text.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 26 15 04 00 01 00 00 00 16 1b 00 50 4c 41 4e 20 28 52 44 42 24 44 41 54 41 42 41 53 45 20 4e 41 54 55 52 41 4c 29 01 00 00 00 00 00 00
```

## 17) SQL Describe Vars (SQLDA)
This example returns one output column with SQL type `SQL_LONG` and length 4. Items sequence includes `isc_info_sql_describe_end`.

1. Client → Server: `op_info_sql` items `num_variables`, `describe_vars`, `sqlda_seq`, `type`, `length`, `null_ind`, `describe_end`, `end`.
```hex
00 00 00 46 00 00 00 0a 00 00 00 00 00 00 00 08 06 07 09 0b 0e 0f 08 01 00 00 01 00
```

2. Server → Client: `op_response` with one variable and its descriptors.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 25 06 04 00 01 00 00 00 09 04 00 01 00 00 00 0b 04 00 f0 01 00 00 0e 04 00 04 00 00 00 0f 04 00 00 00 00 00 08 01 00 00 00 00 00 00 00
```

## 18) Execute2 (No Input, No Output)
`op_execute2` produces an immediate `op_sql_response` followed by `op_response`.

1. Client → Server: `op_execute2` with empty input BLR/message and empty output BLR.
```hex
00 00 00 4c 00 00 00 0a 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

2. Server → Client: `op_sql_response` with `p_sqldata_messages=0`.
```hex
00 00 00 4e 00 00 00 00
```

3. Server → Client: `op_response` success (transaction handle remains `2` in this example).
```hex
00 00 00 09 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## 19) SQL Describe Vars With Names
This expands the SQLDA example to include field, relation, owner, and alias names.

1. Client → Server: `op_info_sql` items `num_variables`, `describe_vars`, `sqlda_seq`, `type`, `length`, `null_ind`, `field`, `relation`, `owner`, `alias`, `describe_end`, `end`.
```hex
00 00 00 46 00 00 00 0a 00 00 00 00 00 00 00 0c 06 07 09 0b 0e 0f 10 11 12 13 08 01 00 00 01 00
```

2. Server → Client: `op_response` with one variable; names are `F1`, `T1`, `SYSDBA`, `A1`.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3d 06 04 00 01 00 00 00 09 04 00 01 00 00 00 0b 04 00 c4 01 00 00 0e 04 00 04 00 00 00 0f 04 00 00 00 00 00 10 02 00 46 31 11 02 00 54 31 12 06 00 53 59 53 44 42 41 13 02 00 41 31 08 01 00 00 00 00 00 00 00
```

## 20) Exec Immediate2 With Output Message
`op_exec_immediate2` sends an output message inline and receives `op_sql_response` with one row.

1. Client → Server: `op_exec_immediate2` for `SELECT 1`, output message packed (one column).
```hex
00 00 00 4b 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 03 00 00 00 08 53 45 4c 45 43 54 20 31 00 00 00 00 00 00 00 00 00 00 00 00
```

2. Server → Client: `op_sql_response` with one packed row value 1.
```hex
00 00 00 4e 00 00 00 01 00 00 00 00 00 00 00 01
```

3. Server → Client: `op_response` success (transaction handle 2).
```hex
00 00 00 09 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## 21) SQL Info Records
`isc_info_sql_records` returns a nested info block with record counts by operation.

1. Client → Server: `op_info_sql` items `isc_info_sql_records`, `isc_info_end`.
```hex
00 00 00 46 00 00 00 0a 00 00 00 00 00 00 00 02 17 01 00 00 00 00 01 00
```

2. Server → Client: `op_response` with record counts:
`update=0`, `delete=0`, `select=1`, `insert=0`.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 21 17 1d 00 0f 04 00 00 00 00 00 10 04 00 00 00 00 00 0d 04 00 01 00 00 00 0e 04 00 00 00 00 00 01 01 00 00 00 00 00 00 00
```

## 22) SQL Info: Statement Flags and Timeouts
Returns statement flags and configured timeouts.

1. Client → Server: `op_info_sql` items `isc_info_sql_stmt_flags`, `isc_info_sql_stmt_timeout_user`, `isc_info_sql_stmt_timeout_run`, `isc_info_end`.
```hex
00 00 00 46 00 00 00 0a 00 00 00 00 00 00 00 04 1b 1c 1d 01 00 00 00 80
```

2. Server → Client: `op_response` with flags=2, timeout_user=30, timeout_run=25.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 16 1b 04 00 02 00 00 00 1c 04 00 1e 00 00 00 1d 04 00 19 00 00 00 01 00 00 00 00 00 00
```

## 23) SQL Explain Plan Truncation
When output buffer is too small, the response is `isc_info_truncated`.

1. Client → Server: `op_info_sql` items `isc_info_sql_explain_plan`, `isc_info_end`, buffer length 16.
```hex
00 00 00 46 00 00 00 0a 00 00 00 00 00 00 00 02 1a 01 00 00 00 00 00 10
```

2. Server → Client: `op_response` with `isc_info_truncated`.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 02 00 00 00 00 00 00 00
```

## 24) SQL Exec Path BLR Text
Returns execution path in BLR text form.

1. Client → Server: `op_info_sql` items `isc_info_sql_exec_path_blr_text`, `isc_info_end`.
```hex
00 00 00 46 00 00 00 0a 00 00 00 00 00 00 00 02 20 01 00 00 00 00 01 00
```

2. Server → Client: `op_response` with text `blr text path\n`.
```hex
00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 12 20 0e 00 62 6c 72 20 74 65 78 74 20 70 61 74 68 0a 01 00 00 00 00 00 00
```
