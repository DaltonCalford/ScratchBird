# Appendix: Firebird 5 Wire Protocol Constants (Authoritative)

This appendix defines all protocol constants required to implement the Firebird 5 remote protocol. These constants are authoritative and must be used exactly as listed.

## Protocol Version Constants

```
FB_PROTOCOL_FLAG = 0x8000
FB_PROTOCOL_MASK = 0x7FFF

PROTOCOL_VERSION10 = 0x000A
PROTOCOL_VERSION11 = 0x800B
PROTOCOL_VERSION12 = 0x800C
PROTOCOL_VERSION13 = 0x800D
PROTOCOL_VERSION14 = 0x800E
PROTOCOL_VERSION15 = 0x800F
PROTOCOL_VERSION16 = 0x8010
PROTOCOL_VERSION17 = 0x8011
PROTOCOL_VERSION18 = 0x8012
PROTOCOL_VERSION19 = 0x8013
PROTOCOL_VERSION20 = 0x8014

PROTOCOL_STMT_TOUT    = PROTOCOL_VERSION16
PROTOCOL_FETCH_SCROLL = PROTOCOL_VERSION18
PROTOCOL_INLINE_BLOB  = PROTOCOL_VERSION19
PROTOCOL_PREPARE_FLAG = PROTOCOL_VERSION20
```

## Architecture IDs (P_ARCH)

```
arch_generic  = 1 
arch_sun   = 3
arch_sun4   = 8
arch_sunx86   = 9
arch_hpux   = 10
arch_rt    = 14
arch_intel_32  = 29 
arch_linux   = 36
arch_freebsd  = 37
arch_netbsd   = 38
arch_darwin_ppc  = 39
arch_winnt_64  = 40
arch_darwin_x64  = 41
arch_darwin_ppc64 = 42
arch_arm            = 43
arch_winnt_arm64 = 44
arch_max   = 45 
```

## Protocol Types (p_acpt_type)

```
ptype_batch_send = 3
ptype_out_of_band = 4
ptype_lazy_send = 5
ptype_MASK = 0x00FF

pflag_compress = 0x0100
pflag_win_sspi_nego = 0x0200
```

## Statement Flags

```
STMT_NO_BATCH = 2
STMT_DEFER_EXECUTE = 4
```

## Handle and Protocol Limits

```
MAX_CNCT_VERSIONS = 11
MAX_OBJCT_HANDLES = 65000
INVALID_OBJECT = 65535
BLOB_LENGTH = 16384
```

## Fetch Direction (P_FETCH)

```
fetch_next  = 0
fetch_prior  = 1
fetch_first  = 2
fetch_last  = 3
fetch_absolute = 4
fetch_relative = 5
fetch_execute = fetch_next
```

## Operation Codes (P_OP)

```
op_void    = 0 
op_connect   = 1 
op_exit    = 2 
op_accept   = 3 
op_reject   = 4 
op_disconnect  = 6 
op_response   = 9 
op_attach   = 19 
op_create   = 20 
op_detach   = 21 
op_compile   = 22 
op_start   = 23
op_start_and_send = 24
op_send    = 25
op_receive   = 26
op_unwind   = 27 
op_release   = 28
op_transaction  = 29 
op_commit   = 30
op_rollback   = 31
op_prepare   = 32
op_reconnect  = 33
op_create_blob  = 34 
op_open_blob  = 35
op_get_segment  = 36
op_put_segment  = 37
op_cancel_blob  = 38
op_close_blob  = 39
op_info_database = 40 
op_info_request  = 41
op_info_transaction = 42
op_info_blob  = 43
op_batch_segments = 44 
op_que_events  = 48 
op_cancel_events = 49 
op_commit_retaining = 50 
op_prepare2   = 51 
op_event   = 52 
op_connect_request = 53 
op_aux_connect  = 54 
op_ddl    = 55 
op_open_blob2  = 56
op_create_blob2  = 57
op_get_slice  = 58
op_put_slice  = 59
op_slice   = 60 
op_seek_blob  = 61 
op_allocate_statement  = 62 
op_execute    = 63 
op_exec_immediate  = 64 
op_fetch    = 65 
op_fetch_response  = 66 
op_free_statement  = 67 
op_prepare_statement  = 68 
op_set_cursor   = 69 
op_info_sql    = 70
op_dummy    = 71 
op_response_piggyback  = 72 
op_start_and_receive  = 73
op_start_send_and_receive  = 74
op_exec_immediate2  = 75 
op_execute2    = 76 
op_insert    = 77
op_sql_response   = 78 
op_transact    = 79
op_transact_response  = 80
op_drop_database  = 81
op_service_attach  = 82
op_service_detach  = 83
op_service_info   = 84
op_service_start  = 85
op_rollback_retaining = 86
op_update_account_info = 87
op_authenticate_user = 88
op_partial    = 89 
op_trusted_auth   = 90
op_cancel    = 91
op_cont_auth   = 92
op_ping     = 93
op_accept_data   = 94 
op_abort_aux_connection = 95 
op_crypt    = 96
op_crypt_key_callback = 97
op_cond_accept   = 98 
op_batch_create   = 99
op_batch_msg   = 100
op_batch_exec   = 101
op_batch_rls   = 102
op_batch_cs    = 103
op_batch_regblob  = 104
op_batch_blob_stream = 105
op_batch_set_bpb  = 106
op_repl_data   = 107
op_repl_req    = 108
op_batch_cancel   = 109
op_batch_sync   = 110
op_info_batch   = 111
op_fetch_scroll   = 112
op_info_cursor   = 113
op_inline_blob   = 114
```
