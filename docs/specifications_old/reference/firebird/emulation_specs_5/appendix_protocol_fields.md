# Appendix: Firebird 5 Protocol Field Order (Authoritative)

Each opcode is listed with its field order. Field types are defined in `appendix_protocol_structs.md` and the XDR rules in `30_wire_protocol.md`.

## op_connect

Field order:
- p_cnct_operation
- p_cnct_cversion
- p_cnct_client
- p_cnct_file
- p_cnct_count
- p_cnct_user_id
- p_cnct_versions[]

## op_accept

Field order:
- p_acpt_version
- p_acpt_architecture
- p_acpt_type

## op_accept_data

Field order:
- p_acpt_version
- p_acpt_architecture
- p_acpt_type
- p_acpt_data
- p_acpt_plugin
- p_acpt_authenticated
- p_acpt_keys

## op_cond_accept

Field order:
- p_acpt_version
- p_acpt_architecture
- p_acpt_type
- p_acpt_data
- p_acpt_plugin
- p_acpt_authenticated
- p_acpt_keys

## op_connect_request

Field order:
- p_req_type
- p_req_object
- p_req_partner

## op_aux_connect

Field order:
- p_req_type
- p_req_object
- p_req_partner

## op_attach

Field order:
- p_atch_database
- p_atch_file
- p_atch_dpb

## op_create

Field order:
- p_atch_database
- p_atch_file
- p_atch_dpb

## op_service_attach

Field order:
- p_atch_database
- p_atch_file
- p_atch_dpb

## op_compile

Field order:
- p_cmpl_database
- p_cmpl_blr

## op_receive

Field order:
- p_data_request
- p_data_incarnation
- p_data_transaction
- p_data_message_number
- p_data_messages

## op_start

Field order:
- p_data_request
- p_data_incarnation
- p_data_transaction
- p_data_message_number
- p_data_messages

## op_start_and_receive

Field order:
- p_data_request
- p_data_incarnation
- p_data_transaction
- p_data_message_number
- p_data_messages

## op_send

Field order:
- p_data_request
- p_data_incarnation
- p_data_transaction
- p_data_message_number
- p_data_messages
- xdr_request(message)

## op_start_and_send

Field order:
- p_data_request
- p_data_incarnation
- p_data_transaction
- p_data_message_number
- p_data_messages
- xdr_request(message)

## op_start_send_and_receive

Field order:
- p_data_request
- p_data_incarnation
- p_data_transaction
- p_data_message_number
- p_data_messages
- xdr_request(message)

## op_response

Field order:
- p_resp_object
- p_resp_blob_id
- p_resp_data
- p_resp_status_vector

## op_response_piggyback

Field order:
- p_resp_object
- p_resp_blob_id
- p_resp_data
- p_resp_status_vector

## op_transact

Field order:
- p_trrq_database
- p_trrq_transaction
- p_trrq_blr
- p_trrq_messages
- xdr_trrq_message(in)

## op_transact_response

Field order:
- p_data_messages
- xdr_trrq_message(out)

## op_open_blob2

Field order:
- p_blob_bpb
- p_blob_transaction
- p_blob_id

## op_create_blob2

Field order:
- p_blob_bpb
- p_blob_transaction
- p_blob_id

## op_open_blob

Field order:
- p_blob_transaction
- p_blob_id

## op_create_blob

Field order:
- p_blob_transaction
- p_blob_id

## op_get_segment

Field order:
- p_sgmt_blob
- p_sgmt_length
- p_sgmt_segment

## op_put_segment

Field order:
- p_sgmt_blob
- p_sgmt_length
- p_sgmt_segment

## op_batch_segments

Field order:
- p_sgmt_blob
- p_sgmt_length
- p_sgmt_segment

## op_seek_blob

Field order:
- p_seek_blob
- p_seek_mode
- p_seek_offset

## op_reconnect

Field order:
- p_sttr_database
- p_sttr_tpb

## op_transaction

Field order:
- p_sttr_database
- p_sttr_tpb

## op_info_blob

Field order:
- p_info_object
- p_info_incarnation
- p_info_items
- p_info_buffer_length

## op_info_database

Field order:
- p_info_object
- p_info_incarnation
- p_info_items
- p_info_buffer_length

## op_info_request

Field order:
- p_info_object
- p_info_incarnation
- p_info_items
- p_info_buffer_length

## op_info_transaction

Field order:
- p_info_object
- p_info_incarnation
- p_info_items
- p_info_buffer_length

## op_service_info

Field order:
- p_info_object
- p_info_incarnation
- p_info_items
- p_info_recv_items
- p_info_buffer_length

## op_info_sql

Field order:
- p_info_object
- p_info_incarnation
- p_info_items
- p_info_buffer_length

## op_info_batch

Field order:
- p_info_object
- p_info_incarnation
- p_info_items
- p_info_buffer_length

## op_info_cursor

Field order:
- p_info_object
- p_info_incarnation
- p_info_items
- p_info_buffer_length

## op_service_start

Field order:
- p_info_object
- p_info_incarnation
- p_info_items

## op_commit

Field order:
- p_rlse_object

## op_prepare

Field order:
- p_rlse_object

## op_rollback

Field order:
- p_rlse_object

## op_unwind

Field order:
- p_rlse_object

## op_release

Field order:
- p_rlse_object

## op_close_blob

Field order:
- p_rlse_object

## op_cancel_blob

Field order:
- p_rlse_object

## op_detach

Field order:
- p_rlse_object

## op_drop_database

Field order:
- p_rlse_object

## op_service_detach

Field order:
- p_rlse_object

## op_commit_retaining

Field order:
- p_rlse_object

## op_rollback_retaining

Field order:
- p_rlse_object

## op_allocate_statement

Field order:
- p_rlse_object

## op_batch_rls

Field order:
- p_rlse_object

## op_batch_cancel

Field order:
- p_rlse_object

## op_prepare2

Field order:
- p_prep_transaction
- p_prep_data

## op_que_events

Field order:
- p_event_database
- p_event_items
- p_event_ast
- p_event_arg
- p_event_rid

## op_event

Field order:
- p_event_database
- p_event_items
- p_event_ast
- p_event_arg
- p_event_rid

## op_cancel_events

Field order:
- p_event_database
- p_event_rid

## op_ddl

Field order:
- p_ddl_database
- p_ddl_transaction
- p_ddl_blr

## op_get_slice

Field order:
- p_slc_transaction
- p_slc_id
- p_slc_length
- p_slc_sdl
- p_slc_parameters
- p_slc_slice

## op_put_slice

Field order:
- p_slc_transaction
- p_slc_id
- p_slc_length
- p_slc_sdl
- p_slc_parameters
- p_slc_slice

## op_slice

Field order:
- p_slr_length
- p_slr_slice

## op_execute

Field order:
- p_sqldata_statement
- p_sqldata_transaction
- p_sqldata_blr
- p_sqldata_message_number
- p_sqldata_messages
- xdr_sql_message
- p_sqldata_timeout?
- p_sqldata_cursor_flags?
- p_sqldata_inline_blob_size?

## op_execute2

Field order:
- p_sqldata_statement
- p_sqldata_transaction
- p_sqldata_blr
- p_sqldata_message_number
- p_sqldata_messages
- xdr_sql_message
- p_sqldata_out_blr
- p_sqldata_out_message_number
- p_sqldata_timeout?
- p_sqldata_cursor_flags?
- p_sqldata_inline_blob_size?

## op_exec_immediate2

Field order:
- p_sqlst_blr
- p_sqlst_message_number
- p_sqlst_messages
- xdr_sql_message
- p_sqlst_out_blr
- p_sqlst_out_message_number
- p_sqlst_inline_blob_size?
- p_sqlst_transaction
- p_sqlst_statement
- p_sqlst_SQL_dialect
- p_sqlst_SQL_str
- p_sqlst_items
- p_sqlst_buffer_length
- p_sqlst_flags?

## op_exec_immediate

Field order:
- p_sqlst_transaction
- p_sqlst_statement
- p_sqlst_SQL_dialect
- p_sqlst_SQL_str
- p_sqlst_items
- p_sqlst_buffer_length
- p_sqlst_flags?

## op_prepare_statement

Field order:
- p_sqlst_transaction
- p_sqlst_statement
- p_sqlst_SQL_dialect
- p_sqlst_SQL_str
- p_sqlst_items
- p_sqlst_buffer_length
- p_sqlst_flags?

## op_fetch

Field order:
- p_sqldata_statement
- p_sqldata_blr
- p_sqldata_message_number
- p_sqldata_messages

## op_fetch_scroll

Field order:
- p_sqldata_statement
- p_sqldata_blr
- p_sqldata_message_number
- p_sqldata_messages
- p_sqldata_fetch_op
- p_sqldata_fetch_pos

## op_fetch_response

Field order:
- p_sqldata_status
- p_sqldata_messages
- xdr_sql_message?

## op_free_statement

Field order:
- p_sqlfree_statement
- p_sqlfree_option

## op_set_cursor

Field order:
- p_sqlcur_statement
- p_sqlcur_cursor_name
- p_sqlcur_type

## op_sql_response

Field order:
- p_sqldata_messages
- xdr_sql_message?

## op_update_account_info

Field order:
- p_account_database
- p_account_apb

## op_authenticate_user

Field order:
- p_auth_database
- p_auth_dpb
- p_auth_items
- p_auth_buffer_length

## op_trusted_auth

Field order:
- p_trau_data

## op_cont_auth

Field order:
- p_data
- p_name
- p_list
- p_keys

## op_cancel

Field order:
- p_co_kind

## op_crypt

Field order:
- p_plugin
- p_key

## op_crypt_key_callback

Field order:
- p_cc_data
- p_cc_reply?

## op_batch_create

Field order:
- p_batch_statement
- p_batch_blr
- p_batch_msglen
- p_batch_pb

## op_batch_msg

Field order:
- p_batch_statement
- p_batch_messages
- p_batch_data

## op_batch_exec

Field order:
- p_batch_statement
- p_batch_transaction

## op_batch_cs

Field order:
- p_batch_statement
- p_batch_reccount
- p_batch_updates
- p_batch_vectors
- p_batch_errors
- update_counts
- status_vectors

## op_batch_sync

Field order:


## op_batch_set_bpb

Field order:
- p_batch_statement
- p_batch_blob_bpb

## op_batch_regblob

Field order:
- p_batch_statement
- p_batch_exist_id
- p_batch_blob_id

## op_batch_blob_stream

Field order:
- p_batch_statement
- p_batch_blob_data

## op_repl_data

Field order:
- p_repl_database
- p_repl_data

## op_inline_blob

Field order:
- p_tran_id
- p_blob_id
- p_blob_info
- p_blob_data

