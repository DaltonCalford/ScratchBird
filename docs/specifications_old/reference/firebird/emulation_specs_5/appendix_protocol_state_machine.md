# Appendix: Firebird 5 Protocol State Machine (Authoritative)

## States

- DISCONNECTED
- CONNECTING
- AUTHENTICATING
- READY_NO_DB
- ATTACHED
- TRANSACTION_ACTIVE
- STATEMENT_ALLOCATED
- STATEMENT_PREPARED
- EXECUTING
- FETCHING
- SERVICE_ATTACHED
- ASYNC_CHANNEL
- DETACHED
- BROKEN

## Transitions

| Op | From | To | Responses | Notes |
|---|---|---|---|---|
| op_connect | DISCONNECTED | CONNECTING | op_accept, op_accept_data, op_reject |  |
| op_cont_auth | CONNECTING | AUTHENTICATING | op_accept_data, op_response |  |
| op_trusted_auth | CONNECTING | AUTHENTICATING | op_accept_data, op_response |  |
| op_accept | CONNECTING | READY_NO_DB |  |  |
| op_accept_data | CONNECTING | READY_NO_DB |  |  |
| op_cond_accept | CONNECTING | AUTHENTICATING |  |  |
| op_attach | READY_NO_DB | ATTACHED | op_response |  |
| op_create | READY_NO_DB | ATTACHED | op_response |  |
| op_detach | ATTACHED | READY_NO_DB | op_response |  |
| op_drop_database | ATTACHED | READY_NO_DB | op_response |  |
| op_service_attach | READY_NO_DB | SERVICE_ATTACHED | op_response |  |
| op_service_start | SERVICE_ATTACHED | SERVICE_ATTACHED | op_response |  |
| op_service_info | SERVICE_ATTACHED | SERVICE_ATTACHED | op_response |  |
| op_service_detach | SERVICE_ATTACHED | READY_NO_DB | op_response |  |
| op_transaction | ATTACHED | TRANSACTION_ACTIVE | op_response |  |
| op_reconnect | ATTACHED | TRANSACTION_ACTIVE | op_response |  |
| op_commit | TRANSACTION_ACTIVE | ATTACHED | op_response |  |
| op_rollback | TRANSACTION_ACTIVE | ATTACHED | op_response |  |
| op_commit_retaining | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_rollback_retaining | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_prepare | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_prepare2 | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_allocate_statement | ATTACHED | STATEMENT_ALLOCATED | op_response |  |
| op_prepare_statement | STATEMENT_ALLOCATED | STATEMENT_PREPARED | op_response |  |
| op_exec_immediate | ATTACHED | EXECUTING | op_sql_response, op_response |  |
| op_exec_immediate2 | ATTACHED | EXECUTING | op_sql_response, op_response |  |
| op_execute | STATEMENT_PREPARED | EXECUTING | op_sql_response, op_response |  |
| op_execute2 | STATEMENT_PREPARED | EXECUTING | op_sql_response, op_response |  |
| op_fetch | EXECUTING | FETCHING | op_fetch_response, op_inline_blob? |  |
| op_fetch_scroll | EXECUTING | FETCHING | op_fetch_response, op_inline_blob? |  |
| op_free_statement | STATEMENT_ALLOCATED | ATTACHED | op_response |  |
| op_free_statement | STATEMENT_PREPARED | ATTACHED | op_response |  |
| op_compile | ATTACHED | ATTACHED | op_response |  |
| op_start | TRANSACTION_ACTIVE | EXECUTING | op_response |  |
| op_start_and_receive | TRANSACTION_ACTIVE | EXECUTING | op_response, op_response_piggyback |  |
| op_start_and_send | TRANSACTION_ACTIVE | EXECUTING | op_response |  |
| op_start_send_and_receive | TRANSACTION_ACTIVE | EXECUTING | op_response, op_response_piggyback |  |
| op_send | EXECUTING | EXECUTING | op_response |  |
| op_receive | EXECUTING | EXECUTING | op_response, op_response_piggyback |  |
| op_release | EXECUTING | ATTACHED | op_response |  |
| op_create_blob | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_open_blob | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_create_blob2 | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_open_blob2 | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_get_segment | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_put_segment | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_batch_segments | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_seek_blob | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_cancel_blob | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_close_blob | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_get_slice | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_slice, op_response |  |
| op_put_slice | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_connect_request | ATTACHED | ASYNC_CHANNEL | op_response |  |
| op_aux_connect | ASYNC_CHANNEL | ASYNC_CHANNEL |  |  |
| op_que_events | ATTACHED | ATTACHED | op_response |  |
| op_event | ASYNC_CHANNEL | ASYNC_CHANNEL |  |  |
| op_cancel_events | ATTACHED | ATTACHED | op_response |  |
| op_info_database | ATTACHED | ATTACHED | op_response |  |
| op_info_transaction | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_info_request | EXECUTING | EXECUTING | op_response |  |
| op_info_blob | TRANSACTION_ACTIVE | TRANSACTION_ACTIVE | op_response |  |
| op_info_sql | ATTACHED | ATTACHED | op_response |  |
| op_info_cursor | FETCHING | FETCHING | op_response |  |
| op_info_batch | ATTACHED | ATTACHED | op_response |  |
| op_batch_create | STATEMENT_PREPARED | STATEMENT_PREPARED | op_response |  |
| op_batch_msg | STATEMENT_PREPARED | STATEMENT_PREPARED | op_response |  |
| op_batch_exec | STATEMENT_PREPARED | STATEMENT_PREPARED | op_response, op_batch_cs |  |
| op_batch_rls | STATEMENT_PREPARED | STATEMENT_PREPARED | op_response |  |
| op_batch_cancel | STATEMENT_PREPARED | STATEMENT_PREPARED | op_response |  |
| op_batch_sync | STATEMENT_PREPARED | STATEMENT_PREPARED | op_response |  |
| op_batch_blob_stream | STATEMENT_PREPARED | STATEMENT_PREPARED | op_response |  |
| op_batch_regblob | STATEMENT_PREPARED | STATEMENT_PREPARED | op_response |  |
| op_batch_set_bpb | STATEMENT_PREPARED | STATEMENT_PREPARED | op_response |  |
| op_dummy | ATTACHED | ATTACHED | op_dummy |  |
| op_ping | ATTACHED | ATTACHED | op_response |  |
| op_cancel | ATTACHED | ATTACHED | op_response |  |
| op_crypt | CONNECTING | CONNECTING | op_response |  |
| op_crypt_key_callback | CONNECTING | CONNECTING | op_response |  |
| op_disconnect | ATTACHED | DISCONNECTED |  |  |
| op_exit | ATTACHED | DISCONNECTED |  |  |
