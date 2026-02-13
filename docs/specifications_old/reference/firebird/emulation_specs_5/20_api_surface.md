# Firebird 5 API Surface - Standalone
## 1. Object Model (Remote Protocol)
All remote objects are referenced by 16-bit `OBJCT` handles. These handles are issued by the server in responses and must be used by the client for subsequent operations. The handle namespace is per-connection.
Key object types (remote layer):
- Database attachment (`Rdb` / `IAttachment`)
- Transaction (`Rtr` / `ITransaction`)
- Request (`Rrq` / `IRequest`)
- Statement (`Rsr` / `IStatement`)
- Cursor (`IResultSet`)
- Blob (`Rbl` / `IBlob`)
- Service (`Svc` / `IService`)
- Events (`IEvents`)
Handle allocation and release semantics are defined in this document and `30_wire_protocol.md`. For each object type, a release-style opcode exists (see wire protocol) that invalidates the handle on the server and frees associated resources.
## 2. Core Lifecycles
### 2.1 Database Attach/Create
- `op_attach` / `op_create` create a database attachment (`Rdb`) and return a handle in `op_response` status.
- Required input: file/alias (`p_atch_file`) and DPB (`p_atch_dpb`).
- `op_detach` releases the attachment and implicitly releases all dependent transactions, statements, requests, and blobs.
- `op_drop_database` detaches and drops the database.
### 2.2 Transaction
- `op_transaction` starts a transaction using TPB (`p_sttr_tpb`).
- `op_commit`, `op_rollback` terminate the transaction.
- `op_commit_retaining`, `op_rollback_retaining` keep context and retain transaction handle.
- `op_prepare` / `op_prepare2` support two-phase commit (TPB + optional prepare data).
### 2.3 Request/BLR (Engine Request API)
- `op_compile` registers a BLR request and returns a request handle.
- `op_start`/`op_start_and_send` begin execution within a transaction.
- `op_send` / `op_receive` exchange messages between client and server using BLR-defined message formats.
- `op_release` frees the request.
### 2.4 DSQL
- `op_allocate_statement` allocates a statement handle.
- `op_prepare_statement` prepares SQL text and returns metadata.
- `op_execute`/`op_execute2` execute prepared statement with input message.
- `op_exec_immediate`/`op_exec_immediate2` execute SQL directly without prior prepare.
- `op_fetch` / `op_fetch_scroll` retrieve rows; server responds with `op_fetch_response`.
- `op_free_statement` releases a statement.
### 2.5 Blob
- `op_create_blob` / `op_open_blob` (and version 2 variants with BPB) create/open blobs.
- `op_get_segment`, `op_put_segment`, `op_batch_segments` read/write blob segments.
- `op_seek_blob` moves blob cursor.
- `op_close_blob`, `op_cancel_blob` close or abort blob operations.
- `op_inline_blob` transfers blob data inline for protocol >= 19.
### 2.6 Services
- `op_service_attach` attaches to service manager.
- `op_service_start` starts a service action (e.g., backup, restore, trace).
- `op_service_info` fetches service output and status.
- `op_service_detach` terminates service attachment.
### 2.7 Info Services
- `op_info_database`, `op_info_transaction`, `op_info_request`, `op_info_blob`, `op_info_sql`, `op_info_cursor` request metadata and status from corresponding handles.
- `op_info_batch` requests batch execution info.
- `op_service_info` uses a specific request/response layout with requested and received item lists.
### 2.8 Events
- `op_que_events` registers event interest list.
- `op_event` is the asynchronous event delivery packet.
- `op_cancel_events` deregisters events.
### 2.9 Authentication, Crypt, Cancel
- `op_authenticate_user` and `op_cont_auth` implement plugin-based authentication continuation.
- `op_trusted_auth` is used for trusted authentication payload.
- `op_crypt` and `op_crypt_key_callback` negotiate wire encryption.
- `op_cancel` requests asynchronous statement/attachment cancel (protocol >= 12).
## 3. Clumplet Encoding (DPB/TPB/BPB/SPB/Info)
Parameter blocks are encoded as clumplet streams. Clumplet encoding and parsing are defined by the rules in this section and the machine-readable schema in `formal/clumplet_format.json`.
### 3.1 Clumplet Kinds
- `Tagged`: 1-byte tag, 1-byte length, then `length` bytes of data.
- `WideTagged`: 1-byte tag, 4-byte **VAX little-endian** length, then data.
- `UnTagged`: same per-clumplet encoding as `Tagged`, but the buffer is not prefixed with a PB version byte.
- `WideUnTagged`: same per-clumplet encoding as `WideTagged`, but the buffer is not prefixed with a PB version byte.
- `Tpb`: TPB has single-byte tags (no length) except specific tags which use `Tagged` format.
- `SpbAttach`, `SpbStart`, `SpbSendItems`, `SpbReceiveItems`, `SpbResponse`, `InfoResponse`, `InfoItems`: SPB/Info-specific formats with tag-specific length rules.
### 3.2 DPB Versions and Encodings
- `isc_dpb_version1` uses `Tagged` format (1-byte length).
- `isc_dpb_version2` uses `WideTagged` format (4-byte length).
- DPB is a tagged clumplet stream prefixed with version byte.

All DPB tag numeric values are listed in `appendix_parameter_blocks.md`.
### 3.3 SPB Encodings
- SPB attach (`SpbAttach`) is tagged; if the first byte is `isc_spb_version`, the actual version tag is in the second byte.
- SPB start (`SpbStart`) begins with an action clumplet (tag = `isc_action_svc_*`) with no data; subsequent clumplets are interpreted based on action.

All SPB tag numeric values and service actions are listed in `appendix_parameter_blocks.md`.
### 3.4 Clumplet Length Rules
- `TraditionalDpb`: 1-byte length.
- `Wide`: 4-byte **VAX little-endian** length.
- `SingleTpb`: no length field; clumplet is single byte tag.
- `StringSpb`: 2-byte **VAX little-endian** length, then data.
- `IntSpb`: 4-byte **VAX little-endian** integer, no length.
- `BigIntSpb`: 8-byte **VAX little-endian** integer, no length.
- `ByteSpb`: 1-byte data, no length.
### 3.5 VAX Integer Encoding (Authoritative)
All clumplet integer payloads and length fields marked “VAX” are encoded **little-endian, two’s-complement** with the least-significant byte first. Supported sizes are 1, 2, 4, and 8 bytes. This is the encoding used by Firebird’s `put_vax_*` and `toVaxInteger()` helpers.

### 3.6 Parameter Blocks on the Wire
DPB, TPB, BPB, and SPB buffers are transmitted as raw clumplet byte streams inside a `CSTRING` on the wire:
- The **clumplet encoding is not XDR**; it is opaque to the XDR layer.
- All numeric fields inside clumplets use the VAX little-endian rules described above unless the clumplet type explicitly uses a single byte.

### 3.7 SPB Parameter Type Mapping by Action
Parameter type mapping is defined by `ClumpletReader::getClumpletType()` and is authoritative. Key action mappings:
- `isc_action_svc_backup`, `isc_action_svc_restore`:
  - StringSpb: `isc_spb_bkp_file`, `isc_spb_dbname`, `isc_spb_res_fix_fss_data`, `isc_spb_res_fix_fss_metadata`, `isc_spb_bkp_stat`, `isc_spb_bkp_skip_data`, `isc_spb_bkp_include_data`, `isc_spb_bkp_keyholder`, `isc_spb_bkp_keyname`, `isc_spb_bkp_crypt`
  - IntSpb: `isc_spb_bkp_factor`, `isc_spb_bkp_length`, `isc_spb_bkp_parallel_workers`, `isc_spb_res_length`, `isc_spb_res_buffers`, `isc_spb_res_page_size`, `isc_spb_options`, `isc_spb_verbint`
  - SingleTpb: `isc_spb_verbose`
  - ByteSpb: `isc_spb_res_access_mode`, `isc_spb_res_replica_mode`
- `isc_action_svc_repair`:
  - StringSpb: `isc_spb_dbname`
  - IntSpb: `isc_spb_options`, `isc_spb_rpr_commit_trans`, `isc_spb_rpr_rollback_trans`, `isc_spb_rpr_recover_two_phase`, `isc_spb_rpr_par_workers`
  - BigIntSpb: `isc_spb_rpr_commit_trans_64`, `isc_spb_rpr_rollback_trans_64`, `isc_spb_rpr_recover_two_phase_64`
- `isc_action_svc_add_user`, `isc_action_svc_delete_user`, `isc_action_svc_modify_user`, `isc_action_svc_display_user`, `isc_action_svc_display_user_adm`, `isc_action_svc_set_mapping`, `isc_action_svc_drop_mapping`:
  - StringSpb: `isc_spb_dbname`, `isc_spb_sql_role_name`, `isc_spb_sec_username`, `isc_spb_sec_password`, `isc_spb_sec_groupname`, `isc_spb_sec_firstname`, `isc_spb_sec_middlename`, `isc_spb_sec_lastname`
  - IntSpb: `isc_spb_sec_userid`, `isc_spb_sec_groupid`, `isc_spb_sec_admin`
- `isc_action_svc_properties`:
  - StringSpb: `isc_spb_dbname`
  - IntSpb: `isc_spb_prp_page_buffers`, `isc_spb_prp_sweep_interval`, `isc_spb_prp_shutdown_db`, `isc_spb_prp_deny_new_attachments`, `isc_spb_prp_deny_new_transactions`, `isc_spb_prp_set_sql_dialect`, `isc_spb_options`, `isc_spb_prp_force_shutdown`, `isc_spb_prp_attachments_shutdown`, `isc_spb_prp_transactions_shutdown`
  - ByteSpb: `isc_spb_prp_reserve_space`, `isc_spb_prp_write_mode`, `isc_spb_prp_access_mode`, `isc_spb_prp_shutdown_mode`, `isc_spb_prp_online_mode`, `isc_spb_prp_replica_mode`
- `isc_action_svc_db_stats`:
  - StringSpb: `isc_spb_dbname`, `isc_spb_command_line`, `isc_spb_sts_table`, `isc_spb_sts_schema`
  - IntSpb: `isc_spb_options`
- `isc_action_svc_nbak`, `isc_action_svc_nrest`:
  - StringSpb: `isc_spb_nbk_file`, `isc_spb_nbk_direct`, `isc_spb_dbname`, `isc_spb_nbk_guid`
  - IntSpb: `isc_spb_nbk_level`, `isc_spb_options`, `isc_spb_nbk_keep_days`, `isc_spb_nbk_keep_rows`
  - SingleTpb: `isc_spb_nbk_clean_history`
- `isc_action_svc_nfix`:
  - StringSpb: `isc_spb_dbname`
  - IntSpb: `isc_spb_options`
- `isc_action_svc_trace_start`, `isc_action_svc_trace_stop`, `isc_action_svc_trace_suspend`, `isc_action_svc_trace_resume`:
  - StringSpb: `isc_spb_trc_cfg`, `isc_spb_trc_name`, `isc_spb_trc_plugins`
  - IntSpb: `isc_spb_trc_id`
- `isc_action_svc_validate`:
  - StringSpb: `isc_spb_val_sch_incl`, `isc_spb_val_sch_excl`, `isc_spb_val_tab_incl`, `isc_spb_val_tab_excl`, `isc_spb_val_idx_incl`, `isc_spb_val_idx_excl`, `isc_spb_dbname`
  - IntSpb: `isc_spb_val_lock_timeout`

### 3.6 Unknown or Malformed Clumplets (Error Rules)
- If a parameter block is malformed (length overflows, invalid length encoding, truncated clumplet), the server MUST return a status vector with `isc_bad_dpb_form`, `isc_bad_tpb_form`, `isc_bad_bpb_form`, or `isc_bad_spb_form` as appropriate.
- If a clumplet tag is syntactically valid but unsupported for the given block type or service action, the server MUST return `isc_bad_dpb_form` / `isc_bad_tpb_form` / `isc_bad_bpb_form` / `isc_bad_spb_form` (matching the block).
## 4. Parameter Blocks (DPB / TPB / BPB / SPB)
### 4.0 Parameter Block Usage by Opcode
- DPB: `op_attach`, `op_create`, `op_service_attach` (`p_atch_dpb`)
- TPB: `op_transaction` (`p_sttr_tpb`), `op_reconnect` (`p_sttr_tpb` when present), `op_prepare2` (`p_prep_tpb`)
- BPB: `op_create_blob2`, `op_open_blob2` (`p_blob_bpb`)
- SPB (Attach): `op_service_attach` (`p_atch_dpb` interpreted as SPB attach)
- SPB (Start): `op_service_start` (`p_info_items` interpreted as SPB start)
- Info Items: `op_info_*` (`p_info_items`) and `op_service_info` (`p_info_items`)

### 4.1 DPB (Database Parameter Block) Tags
Tag values are defined in `appendix_parameter_blocks.md`. This section defines encoding rules and expected data types per tag.
```
#define isc_dpb_version1                  1
#define isc_dpb_version2                  2
#define isc_dpb_cdd_pathname              1
#define isc_dpb_allocation                2
#define isc_dpb_journal                   3
#define isc_dpb_page_size                 4
#define isc_dpb_num_buffers               5
#define isc_dpb_buffer_length             6
#define isc_dpb_debug                     7
#define isc_dpb_garbage_collect           8
#define isc_dpb_verify                    9
#define isc_dpb_sweep                     10
#define isc_dpb_enable_journal            11
#define isc_dpb_disable_journal           12
#define isc_dpb_dbkey_scope               13
#define isc_dpb_number_of_users           14
#define isc_dpb_trace                     15
#define isc_dpb_no_garbage_collect        16
#define isc_dpb_damaged                   17
#define isc_dpb_license                   18
#define isc_dpb_sys_user_name             19
#define isc_dpb_encrypt_key               20
#define isc_dpb_activate_shadow           21
#define isc_dpb_sweep_interval            22
#define isc_dpb_delete_shadow             23
#define isc_dpb_force_write               24
#define isc_dpb_begin_log                 25
#define isc_dpb_quit_log                  26
#define isc_dpb_no_reserve                27
#define isc_dpb_user_name                 28
#define isc_dpb_password                  29
#define isc_dpb_password_enc              30
#define isc_dpb_sys_user_name_enc         31
#define isc_dpb_interp                    32
#define isc_dpb_online_dump               33
#define isc_dpb_old_file_size             34
#define isc_dpb_old_num_files             35
#define isc_dpb_old_file                  36
#define isc_dpb_old_start_page            37
#define isc_dpb_old_start_seqno           38
#define isc_dpb_old_start_file            39
#define isc_dpb_drop_walfile              40
#define isc_dpb_old_dump_id               41
#define isc_dpb_wal_backup_dir            42
#define isc_dpb_wal_chkptlen              43
#define isc_dpb_wal_numbufs               44
#define isc_dpb_wal_bufsize               45
#define isc_dpb_wal_grp_cmt_wait          46
#define isc_dpb_lc_messages               47
#define isc_dpb_lc_ctype                  48
#define isc_dpb_cache_manager             49
#define isc_dpb_shutdown                  50
#define isc_dpb_online                    51
#define isc_dpb_shutdown_delay            52
#define isc_dpb_reserved                  53
#define isc_dpb_overwrite                 54
#define isc_dpb_sec_attach                55
#define isc_dpb_disable_wal               56
#define isc_dpb_connect_timeout           57
#define isc_dpb_dummy_packet_interval     58
#define isc_dpb_gbak_attach               59
#define isc_dpb_sql_role_name             60
#define isc_dpb_set_page_buffers          61
#define isc_dpb_working_directory         62
#define isc_dpb_sql_dialect               63
#define isc_dpb_set_db_readonly           64
#define isc_dpb_set_db_sql_dialect        65
#define isc_dpb_gfix_attach               66
#define isc_dpb_gstat_attach              67
#define isc_dpb_set_db_charset            68
#define isc_dpb_gsec_attach               69		/* deprecated */
#define isc_dpb_address_path              70
#define isc_dpb_process_id                71
#define isc_dpb_no_db_triggers            72
#define isc_dpb_trusted_auth			  73
#define isc_dpb_process_name              74
#define isc_dpb_trusted_role			  75
#define isc_dpb_org_filename			  76
#define isc_dpb_utf8_filename			  77
#define isc_dpb_ext_call_depth			  78
#define isc_dpb_auth_block				  79
#define isc_dpb_client_version			  80
#define isc_dpb_remote_protocol			  81
#define isc_dpb_host_name				  82
#define isc_dpb_os_user					  83
#define isc_dpb_specific_auth_data		  84
#define isc_dpb_auth_plugin_list		  85
#define isc_dpb_auth_plugin_name		  86
#define isc_dpb_config					  87
#define isc_dpb_nolinger				  88
#define isc_dpb_reset_icu				  89
#define isc_dpb_map_attach                90
#define isc_dpb_session_time_zone         91
#define isc_dpb_set_db_replica            92
#define isc_dpb_set_bind                  93
#define isc_dpb_decfloat_round            94
#define isc_dpb_decfloat_traps            95
#define isc_dpb_clear_map				  96
#define isc_dpb_upgrade_db				  97
#define isc_dpb_parallel_workers		 100
#define isc_dpb_worker_attach			 101
#define isc_dpb_owner					 102
#define isc_dpb_max_blob_cache_size		 103
#define isc_dpb_max_inline_blob_size	 104
#define isc_dpb_search_path				 105
#define isc_dpb_blr_request_search_path	 106
#define isc_dpb_gbak_restore_has_schema	 107
#define isc_dpb_address 1
#define isc_dpb_addr_protocol 1
#define isc_dpb_addr_endpoint 2
#define isc_dpb_addr_flags 3
#define isc_dpb_addr_crypt 4
#define isc_dpb_addr_flag_conn_compressed	0x01
#define isc_dpb_addr_flag_conn_encrypted	0x02
#define isc_dpb_pages                     1
#define isc_dpb_records                   2
#define isc_dpb_indices                   4
#define isc_dpb_transactions              8
#define isc_dpb_no_update                 16
#define isc_dpb_repair                    32
#define isc_dpb_ignore                    64
#define isc_dpb_shut_cache               0x1
#define isc_dpb_shut_attachment          0x2
#define isc_dpb_shut_transaction         0x4
#define isc_dpb_shut_force               0x8
#define isc_dpb_shut_mode_mask          0x70
#define isc_dpb_shut_default             0x0
#define isc_dpb_shut_normal             0x10
#define isc_dpb_shut_multi              0x20
#define isc_dpb_shut_single             0x30
#define isc_dpb_shut_full               0x40
#define isc_dpb_replica_none             0
#define isc_dpb_replica_read_only        1
#define isc_dpb_replica_read_write       2
```
### 4.2 TPB (Transaction Parameter Block) Tags
```
#define isc_tpb_version1                  1
#define isc_tpb_version3                  3
#define isc_tpb_consistency               1
#define isc_tpb_concurrency               2
#define isc_tpb_shared                    3
#define isc_tpb_protected                 4
#define isc_tpb_exclusive                 5
#define isc_tpb_wait                      6
#define isc_tpb_nowait                    7
#define isc_tpb_read                      8
#define isc_tpb_write                     9
#define isc_tpb_lock_read                 10
#define isc_tpb_lock_write                11
#define isc_tpb_verb_time                 12
#define isc_tpb_commit_time               13
#define isc_tpb_ignore_limbo              14
#define isc_tpb_read_committed	          15
#define isc_tpb_autocommit                16
#define isc_tpb_rec_version               17
#define isc_tpb_no_rec_version            18
#define isc_tpb_restart_requests          19
#define isc_tpb_no_auto_undo              20
#define isc_tpb_lock_timeout              21
#define isc_tpb_read_consistency          22
#define isc_tpb_at_snapshot_number        23
#define isc_tpb_auto_release_temp_blobid  24
#define isc_tpb_lock_table_schema         25
```
### 4.3 BPB (Blob Parameter Block) Tags
```
#define isc_bpb_version1                  1
#define isc_bpb_source_type               1
#define isc_bpb_target_type               2
#define isc_bpb_type                      3
#define isc_bpb_source_interp             4
#define isc_bpb_target_interp             5
#define isc_bpb_filter_parameter          6
#define isc_bpb_storage                   7
#define isc_bpb_type_segmented            0x0
#define isc_bpb_type_stream               0x1
#define isc_bpb_storage_main              0x0
#define isc_bpb_storage_temp              0x2
```
### 4.4 SPB (Service Parameter Block) Tags
```
#define isc_spb_version1                  1
#define isc_spb_current_version           2
#define isc_spb_version                   isc_spb_current_version
#define isc_spb_version3                  3
#define isc_spb_user_name                 isc_dpb_user_name
#define isc_spb_sys_user_name             isc_dpb_sys_user_name
#define isc_spb_sys_user_name_enc         isc_dpb_sys_user_name_enc
#define isc_spb_password                  isc_dpb_password
#define isc_spb_password_enc              isc_dpb_password_enc
#define isc_spb_command_line              105
#define isc_spb_dbname                    106
#define isc_spb_verbose                   107
#define isc_spb_options                   108
#define isc_spb_address_path              109
#define isc_spb_process_id                110
#define isc_spb_trusted_auth			  111
#define isc_spb_process_name              112
#define isc_spb_trusted_role              113
#define isc_spb_verbint                   114
#define isc_spb_auth_block                115
#define isc_spb_auth_plugin_name          116
#define isc_spb_auth_plugin_list          117
#define isc_spb_utf8_filename			  118
#define isc_spb_client_version            119
#define isc_spb_remote_protocol           120
#define isc_spb_host_name                 121
#define isc_spb_os_user                   122
#define isc_spb_config					  123
#define isc_spb_expected_db				  124
#define isc_spb_connect_timeout           isc_dpb_connect_timeout
#define isc_spb_dummy_packet_interval     isc_dpb_dummy_packet_interval
#define isc_spb_sql_role_name             isc_dpb_sql_role_name
#define isc_spb_specific_auth_data		  isc_spb_trusted_auth
#define isc_spb_sec_userid            5
#define isc_spb_sec_groupid           6
#define isc_spb_sec_username          7
#define isc_spb_sec_password          8
#define isc_spb_sec_groupname         9
#define isc_spb_sec_firstname         10
#define isc_spb_sec_middlename        11
#define isc_spb_sec_lastname          12
#define isc_spb_sec_admin             13
#define isc_spb_lic_key               5
#define isc_spb_lic_id                6
#define isc_spb_lic_desc              7
#define isc_spb_bkp_file                 5
#define isc_spb_bkp_factor               6
#define isc_spb_bkp_length               7
#define isc_spb_bkp_skip_data            8
#define isc_spb_bkp_stat                 15
#define isc_spb_bkp_keyholder			 16
#define isc_spb_bkp_keyname				 17
#define isc_spb_bkp_crypt				 18
#define isc_spb_bkp_include_data         19
#define isc_spb_bkp_parallel_workers	 21
#define isc_spb_bkp_skip_schema_data     22
#define isc_spb_bkp_include_schema_data  23
#define isc_spb_bkp_ignore_checksums     0x01
#define isc_spb_bkp_ignore_limbo         0x02
#define isc_spb_bkp_metadata_only        0x04
#define isc_spb_bkp_no_garbage_collect   0x08
#define isc_spb_bkp_old_descriptions     0x10
#define isc_spb_bkp_non_transportable    0x20
#define isc_spb_bkp_convert              0x40
#define isc_spb_bkp_expand				 0x80
#define isc_spb_bkp_no_triggers			 0x8000
#define isc_spb_bkp_zip					 0x010000
#define isc_spb_bkp_direct_io			 0x020000
#define isc_spb_prp_page_buffers		5
#define isc_spb_prp_sweep_interval		6
#define isc_spb_prp_shutdown_db			7
#define isc_spb_prp_deny_new_attachments	9
#define isc_spb_prp_deny_new_transactions	10
#define isc_spb_prp_reserve_space		11
#define isc_spb_prp_write_mode			12
#define isc_spb_prp_access_mode			13
#define isc_spb_prp_set_sql_dialect		14
#define isc_spb_prp_activate			0x0100
#define isc_spb_prp_db_online			0x0200
#define isc_spb_prp_nolinger			0x0400
#define isc_spb_prp_force_shutdown			41
#define isc_spb_prp_attachments_shutdown	42
#define isc_spb_prp_transactions_shutdown	43
#define isc_spb_prp_shutdown_mode		44
#define isc_spb_prp_online_mode			45
#define isc_spb_prp_replica_mode		46
#define isc_spb_prp_sm_normal		0
#define isc_spb_prp_sm_multi		1
#define isc_spb_prp_sm_single		2
#define isc_spb_prp_sm_full			3
#define isc_spb_prp_res_use_full	35
#define isc_spb_prp_res				36
#define isc_spb_prp_wm_async		37
#define isc_spb_prp_wm_sync			38
#define isc_spb_prp_am_readonly		39
#define isc_spb_prp_am_readwrite	40
#define isc_spb_prp_rm_none			0
#define isc_spb_prp_rm_readonly		1
#define isc_spb_prp_rm_readwrite	2
#define isc_spb_rpr_commit_trans		15
#define isc_spb_rpr_rollback_trans		34
#define isc_spb_rpr_recover_two_phase	17
#define isc_spb_tra_id					18
#define isc_spb_single_tra_id			19
#define isc_spb_multi_tra_id			20
#define isc_spb_tra_state				21
#define isc_spb_tra_state_limbo			22
#define isc_spb_tra_state_commit		23
#define isc_spb_tra_state_rollback		24
#define isc_spb_tra_state_unknown		25
#define isc_spb_tra_host_site			26
#define isc_spb_tra_remote_site			27
#define isc_spb_tra_db_path				28
#define isc_spb_tra_advise				29
#define isc_spb_tra_advise_commit		30
#define isc_spb_tra_advise_rollback		31
#define isc_spb_tra_advise_unknown		33
#define isc_spb_tra_id_64				46
#define isc_spb_single_tra_id_64		47
#define isc_spb_multi_tra_id_64			48
#define isc_spb_rpr_commit_trans_64		49
#define isc_spb_rpr_rollback_trans_64	50
#define isc_spb_rpr_recover_two_phase_64	51
#define isc_spb_rpr_par_workers			52
#define isc_spb_rpr_validate_db			0x01
#define isc_spb_rpr_sweep_db			0x02
#define isc_spb_rpr_mend_db				0x04
#define isc_spb_rpr_list_limbo_trans	0x08
#define isc_spb_rpr_check_db			0x10
#define isc_spb_rpr_ignore_checksum		0x20
#define isc_spb_rpr_kill_shadows		0x40
#define isc_spb_rpr_full				0x80
#define isc_spb_rpr_icu				  0x0800
#define isc_spb_rpr_upgrade_db		  0x1000
#define isc_spb_res_skip_data			isc_spb_bkp_skip_data
#define isc_spb_res_include_data		isc_spb_bkp_include_data
#define isc_spb_res_buffers				9
#define isc_spb_res_page_size			10
#define isc_spb_res_length				11
#define isc_spb_res_access_mode			12
#define isc_spb_res_fix_fss_data		13
#define isc_spb_res_fix_fss_metadata	14
#define isc_spb_res_skip_schema_data	isc_spb_bkp_skip_schema_data
#define isc_spb_res_include_schema_data	isc_spb_bkp_include_schema_data
#define isc_spb_res_keyholder			isc_spb_bkp_keyholder
#define isc_spb_res_keyname				isc_spb_bkp_keyname
#define isc_spb_res_crypt				isc_spb_bkp_crypt
#define isc_spb_res_stat				isc_spb_bkp_stat
#define isc_spb_res_parallel_workers	isc_spb_bkp_parallel_workers
#define isc_spb_res_metadata_only		isc_spb_bkp_metadata_only
#define isc_spb_res_deactivate_idx		0x0100
#define isc_spb_res_no_shadow			0x0200
#define isc_spb_res_no_validity			0x0400
#define isc_spb_res_one_at_a_time		0x0800
#define isc_spb_res_replace				0x1000
#define isc_spb_res_create				0x2000
#define isc_spb_res_use_all_space		0x4000
#define isc_spb_res_direct_io			isc_spb_bkp_direct_io
#define isc_spb_res_replica_mode		20
#define isc_spb_val_tab_incl		1	// include filter based on regular expression
#define isc_spb_val_tab_excl		2	// exclude filter based on regular expression
#define isc_spb_val_idx_incl		3	// regexp of indices to validate
#define isc_spb_val_idx_excl		4	// regexp of indices to NOT validate
#define isc_spb_val_lock_timeout	5	// how long to wait for table lock
#define isc_spb_val_sch_incl		6	// include schema filter based on regular expression
#define isc_spb_val_sch_excl		7	// exclude schema filter based on regular expression
#define isc_spb_res_am_readonly			isc_spb_prp_am_readonly
#define isc_spb_res_am_readwrite		isc_spb_prp_am_readwrite
#define isc_spb_res_rm_none				isc_spb_prp_rm_none
#define isc_spb_res_rm_readonly			isc_spb_prp_rm_readonly
#define isc_spb_res_rm_readwrite		isc_spb_prp_rm_readwrite
#define isc_spb_num_att			5
#define isc_spb_num_db			6
#define isc_spb_sts_table			64
#define isc_spb_sts_schema			65
#define isc_spb_sts_data_pages		0x01
#define isc_spb_sts_db_log			0x02
#define isc_spb_sts_hdr_pages		0x04
#define isc_spb_sts_idx_pages		0x08
#define isc_spb_sts_sys_relations	0x10
#define isc_spb_sts_record_versions	0x20
#define isc_spb_sts_nocreation		0x80
#define isc_spb_sts_encryption	   0x100
#define isc_spb_nbk_level			5
#define isc_spb_nbk_file			6
#define isc_spb_nbk_direct			7
#define isc_spb_nbk_guid			8
#define isc_spb_nbk_clean_history	9
#define isc_spb_nbk_keep_days		10
#define isc_spb_nbk_keep_rows		11
#define isc_spb_nbk_no_triggers		0x01
#define isc_spb_nbk_inplace			0x02
#define isc_spb_nbk_sequence		0x04
#define isc_spb_trc_id				1
#define isc_spb_trc_name			2
#define isc_spb_trc_cfg				3
#define isc_spb_trc_plugins			4
```
### 4.5 Service Actions
```
#define isc_action_svc_backup          1	/* Starts database backup process on the server */
#define isc_action_svc_restore         2	/* Starts database restore process on the server */
#define isc_action_svc_repair          3	/* Starts database repair process on the server */
#define isc_action_svc_add_user        4	/* Adds a new user to the security database */
#define isc_action_svc_delete_user     5	/* Deletes a user record from the security database */
#define isc_action_svc_modify_user     6	/* Modifies a user record in the security database */
#define isc_action_svc_display_user    7	/* Displays a user record from the security database */
#define isc_action_svc_properties      8	/* Sets database properties */
#define isc_action_svc_add_license     9	/* Adds a license to the license file */
#define isc_action_svc_remove_license 10	/* Removes a license from the license file */
#define isc_action_svc_db_stats	      11	/* Retrieves database statistics */
#define isc_action_svc_get_ib_log     12	/* Retrieves the InterBase log file from the server */
#define isc_action_svc_get_fb_log     12	/* Retrieves the Firebird log file from the server */
#define isc_action_svc_nbak           20	/* Incremental nbackup */
#define isc_action_svc_nrest          21	/* Incremental database restore */
#define isc_action_svc_trace_start    22	// Start trace session
#define isc_action_svc_trace_stop     23	// Stop trace session
#define isc_action_svc_trace_suspend  24	// Suspend trace session
#define isc_action_svc_trace_resume   25	// Resume trace session
#define isc_action_svc_trace_list     26	// List existing sessions
#define isc_action_svc_set_mapping    27	// Set auto admins mapping in security database
#define isc_action_svc_drop_mapping   28	// Drop auto admins mapping in security database
#define isc_action_svc_display_user_adm 29	// Displays user(s) from security database with admin info
#define isc_action_svc_validate		  30	// Starts database online validation
#define isc_action_svc_nfix           31	// Fixup database after file system copy
#define isc_action_svc_last			  32	// keep it last !
```
### 4.6 Service Info Items
```
#define isc_info_svc_svr_db_info		50	/* Retrieves the number of attachments and databases */
#define isc_info_svc_get_license		51	/* Retrieves all license keys and IDs from the license file */
#define isc_info_svc_get_license_mask	52	/* Retrieves a bitmask representing licensed options on the server */
#define isc_info_svc_get_config			53	/* Retrieves the parameters and values for IB_CONFIG */
#define isc_info_svc_version			54	/* Retrieves the version of the services manager */
#define isc_info_svc_server_version		55	/* Retrieves the version of the Firebird server */
#define isc_info_svc_implementation		56	/* Retrieves the implementation of the Firebird server */
#define isc_info_svc_capabilities		57	/* Retrieves a bitmask representing the server's capabilities */
#define isc_info_svc_user_dbpath		58	/* Retrieves the path to the security database in use by the server */
#define isc_info_svc_get_env			59	/* Retrieves the setting of $FIREBIRD */
#define isc_info_svc_get_env_lock		60	/* Retrieves the setting of $FIREBIRD_LOCK */
#define isc_info_svc_get_env_msg		61	/* Retrieves the setting of $FIREBIRD_MSG */
#define isc_info_svc_line				62	/* Retrieves 1 line of service output per call */
#define isc_info_svc_to_eof				63	/* Retrieves as much of the server output as will fit in the supplied buffer */
#define isc_info_svc_timeout			64	/* Sets / signifies a timeout value for reading service information */
#define isc_info_svc_get_licensed_users	65	/* Retrieves the number of users licensed for accessing the server */
#define isc_info_svc_limbo_trans		66	/* Retrieve the limbo transactions */
#define isc_info_svc_running			67	/* Checks to see if a service is running on an attachment */
#define isc_info_svc_get_users			68	/* Returns the user information from isc_action_svc_display_users */
#define isc_info_svc_stdin				78	/* Returns maximum size of data, needed as stdin for service */
```
## 5. DPB Address Path Clumplet Format
DPB address stack format and tags (from `consts_pub.h`):
```
isc_dpb_address_path <byte-clumplet-length> <address-stack>
<address-stack> ::= <address-descriptor> | <address-stack> <address-descriptor>
<address-descriptor> ::= isc_dpb_address <byte-clumplet-length> <address-elements>
<address-elements> ::= <address-element> | <address-elements> <address-element>
<address-element> ::= isc_dpb_addr_protocol | isc_dpb_addr_endpoint | isc_dpb_addr_flags | isc_dpb_addr_crypt
```
## 6. Response Semantics
- Every request is answered with `op_response` (or `op_response_piggyback`) carrying a status vector.
- For operations that return objects, the handle is returned in `p_resp_object`.
- For operations involving blobs, `p_resp_blob_id` may be populated.
- Non-error status vectors can include warnings. The status vector is authoritative.
## 7. Cursor and Fetch Semantics
- `op_fetch` returns `op_fetch_response` with `p_sqldata_status` and optional row data.
- `p_sqldata_status` is 0 for success, 100 for end-of-stream; errors are returned via status vector in `op_response` instead of `op_fetch_response`. `p_sqldata_messages` determines whether row data follows.
- `op_fetch_scroll` supports `p_sqldata_fetch_op` and `p_sqldata_fetch_pos`.
## 8. Batch Semantics
- `op_batch_create` defines input format and message length.
- `op_batch_msg` sends packed messages (NULL bitmap + non-NULL values).
- `op_batch_exec` executes pending batch on a transaction.
- `op_batch_cs` returns completion state with record counts, update counters, and per-record status vectors.
## 9. Error Handling
- Errors and warnings are always conveyed via status vectors, encoded by `xdr_status_vector`.
- `isc_arg_sql_state` entries include SQLSTATE text when available.

## 10. Authentication Block Format (isc_dpb_auth_block / isc_spb_auth_block)
- Authentication blocks are encoded as `WideUnTagged` clumplet buffers.
- The outer buffer contains a sequence of clumplets where the **tag is a sequence number** (0,1,2,...) and the **data is a nested clumplet buffer** describing one auth record.
- Each nested record is also `WideUnTagged` and may contain: 
  - `AUTH_NAME` (1): name described by its type (e.g., user name)
  - `AUTH_PLUGIN` (2): plugin that added the record
  - `AUTH_TYPE` (3): type string (e.g., USER/GROUP/ROLE)
  - `AUTH_SECURE_DB` (4): security database context (optional)
  - `AUTH_ORIG_PLUG` (5): original plugin for mapped record (informational)
- This format is defined in this document and the clumplet schema `formal/clumplet_format.json`.

## 11. Server Dispatch Mapping (Request → Handler)
The following mapping is authoritative and defines which server handler is invoked for each opcode.
```
op_connect -> accept_connection
op_compile -> port->compile
op_attach/op_create -> attach_database
op_service_attach -> attach_service
op_trusted_auth/op_cont_auth -> continue_authentication
op_service_start -> port->service_start
op_receive -> port->receive_msg
op_send -> port->send_msg
op_start/op_start_and_receive -> port->start
op_start_and_send/op_start_send_and_receive -> port->start_and_send
op_transact -> port->transact_request
op_reconnect/op_transaction -> port->start_transaction
op_prepare/op_rollback/op_commit/op_*_retaining -> port->end_transaction
op_detach -> port->end_database
op_service_detach -> port->service_end
op_drop_database -> port->drop_database
op_create_blob/op_open_blob/op_*_blob2 -> port->open_blob
op_put_segment/op_batch_segments -> port->put_segment
op_get_segment -> port->get_segment
op_seek_blob -> port->seek_blob
op_cancel_blob/op_close_blob -> port->end_blob
op_prepare2 -> port->prepare
op_release -> port->end_request
op_info_* -> port->info
op_que_events -> port->que_events
op_cancel_events -> cancel_events
op_connect_request -> aux_request
op_ddl -> port->ddl
op_get_slice -> port->get_slice
op_put_slice -> port->put_slice
op_allocate_statement -> allocate_statement
op_execute/op_execute2 -> port->execute_statement
op_exec_immediate/op_exec_immediate2 -> port->execute_immediate
op_fetch/op_fetch_scroll -> port->fetch
op_free_statement -> port->end_statement
op_prepare_statement -> port->prepare_statement
op_set_cursor -> port->set_cursor
op_dummy -> send dummy
op_cancel -> cancel_operation
op_ping -> ping_connection
op_crypt -> port->start_crypt
op_batch_create -> port->batch_create
op_batch_msg -> port->batch_msg
op_batch_exec -> port->batch_exec
op_batch_rls -> port->batch_rls
op_batch_cancel -> port->batch_cancel
op_batch_sync -> port->batch_sync
op_batch_blob_stream -> port->batch_blob_stream
op_batch_regblob -> port->batch_regblob
op_batch_set_bpb -> port->batch_bpb
op_repl_data -> port->replicate
```

## 12. Connection Flags and Negotiated State
- `PORT_symmetric` is set when client architecture matches server architecture; then `xdr_message` may use direct `xdr_opaque` without translation.
- `PORT_no_oob` is set when protocol type is not `ptype_out_of_band`.
- `PORT_lazy` is set when protocol type is `ptype_lazy_send`.
- `PORT_compressed` is set if compression was negotiated and initialized.

## 13. DPB/TPB/BPB/SPB Encoding Summary
- DPB version 1: tagged clumplets with 1-byte length.
- DPB version 2: tagged clumplets with 4-byte length.
- TPB: mostly single-byte tags; some tags (`isc_tpb_lock_write`, `isc_tpb_lock_read`, `isc_tpb_lock_timeout`, `isc_tpb_at_snapshot_number`, `isc_tpb_lock_table_schema`) are tagged with a 1-byte length and data payload.
- SPB attach/start/info use tag-specific encodings as defined by `ClumpletReader::getClumpletType()`.

## 14. Cancel Operation Types
Cancel kinds (from `consts_pub.h`) used with `op_cancel` / `fb_cancel_operation`:
- `fb_cancel_disable` = 1
- `fb_cancel_enable` = 2
- `fb_cancel_raise` = 3
- `fb_cancel_abort` = 4
