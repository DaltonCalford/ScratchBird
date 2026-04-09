# Embedded and Linked Library API (Alpha)

## Purpose
Define deterministic native API for:
- embedded engine mode
- shared-local IPC mode
- network listener mode

This document defines the canonical host/runtime API model. It does not require
every maintained driver lane to implement every transport profile directly.

## Scope
- C ABI and handle model.
- session/transaction API semantics.
- request execution and result access.
- memory ownership and threading rules.
- transport-family applicability notes for maintained driver lanes.

## API Language and ABI Rules
1. Primary ABI: stable C ABI.
2. Calling convention:
   - Linux/macOS: `cdecl`
   - Windows: `cdecl`
3. Struct layout packed to natural alignment; no compiler-specific packing pragmas in public structs.
4. Versioned symbol namespace prefix: `sb_`.

## Public Handles
Opaque pointers:
- `sb_env_t*`
- `sb_db_t*`
- `sb_conn_t*`
- `sb_session_t*`
- `sb_stmt_t*`
- `sb_result_t*`
- `sb_error_t*`

## Core Enums
```c
typedef enum {
  SB_MODE_EMBEDDED = 1,
  SB_MODE_IPC = 2,
  SB_MODE_INET = 3
} sb_connect_mode_t;

typedef enum {
  SB_STATUS_OK = 0,
  SB_STATUS_WARN = 1,
  SB_STATUS_ERROR = 2
} sb_status_t;
```

## Configuration Structs
```c
typedef struct {
  uint32_t struct_size;
  uint32_t api_version_major;
  uint32_t api_version_minor;
  const char* app_name;
  const char* app_version;
} sb_env_config_t;

typedef struct {
  uint32_t struct_size;
  sb_connect_mode_t mode;
  const char* target;             // file path for embedded, socket path for IPC, host:port for INET
  const char* database;
  const char* username;
  const char* password;
  const char* role;
  const char* group_name;
  const char* language;
  uint32_t connect_timeout_ms;
  uint32_t request_timeout_ms;
  uint32_t flags;
} sb_connect_config_t;
```

## Flags
Connection flags:
- `SB_CONN_FLAG_READ_ONLY`
- `SB_CONN_FLAG_AUTOCOMMIT`
- `SB_CONN_FLAG_TLS_REQUIRED`
- `SB_CONN_FLAG_MGMT_CHANNEL`

## Required API Functions

### Environment
```c
sb_status_t sb_env_create(const sb_env_config_t* cfg, sb_env_t** out_env, sb_error_t** out_err);
sb_status_t sb_env_destroy(sb_env_t* env, sb_error_t** out_err);
```

### Database Open/Close (Embedded)
```c
sb_status_t sb_db_open(sb_env_t* env, const char* db_path, sb_db_t** out_db, sb_error_t** out_err);
sb_status_t sb_db_close(sb_db_t* db, sb_error_t** out_err);
```

### Connection
```c
sb_status_t sb_connect(sb_env_t* env, const sb_connect_config_t* cfg, sb_conn_t** out_conn, sb_error_t** out_err);
sb_status_t sb_disconnect(sb_conn_t* conn, sb_error_t** out_err);
```

### Session
```c
sb_status_t sb_session_open(sb_conn_t* conn, const char* role, const char* group_name, const char* schema, sb_session_t** out_session, sb_error_t** out_err);
sb_status_t sb_session_close(sb_session_t* session, sb_error_t** out_err);
```

### Transaction
```c
sb_status_t sb_tx_begin(sb_session_t* session, const char* isolation_level, uint32_t flags, sb_error_t** out_err);
sb_status_t sb_tx_commit(sb_session_t* session, uint32_t flags, sb_error_t** out_err);
sb_status_t sb_tx_rollback(sb_session_t* session, uint32_t flags, sb_error_t** out_err);
sb_status_t sb_tx_savepoint(sb_session_t* session, const char* name, sb_error_t** out_err);
```

### Statement Execution
```c
sb_status_t sb_stmt_prepare(sb_session_t* session, const char* sql_or_command, sb_stmt_t** out_stmt, sb_error_t** out_err);
sb_status_t sb_stmt_bind_param(sb_stmt_t* stmt, uint32_t index, const void* value, uint32_t value_len, uint32_t type_code, sb_error_t** out_err);
sb_status_t sb_stmt_execute(sb_stmt_t* stmt, sb_result_t** out_result, sb_error_t** out_err);
sb_status_t sb_stmt_close(sb_stmt_t* stmt, sb_error_t** out_err);
```

### Result Access
```c
sb_status_t sb_result_next(sb_result_t* result, uint32_t* out_has_row, sb_error_t** out_err);
sb_status_t sb_result_get_col_count(sb_result_t* result, uint32_t* out_count, sb_error_t** out_err);
sb_status_t sb_result_get_col_name(sb_result_t* result, uint32_t col, const char** out_name, sb_error_t** out_err);
sb_status_t sb_result_get_value(sb_result_t* result, uint32_t col, const void** out_ptr, uint32_t* out_len, uint32_t* out_type_code, sb_error_t** out_err);
sb_status_t sb_result_close(sb_result_t* result, sb_error_t** out_err);
```

### Service Channels
```c
sb_status_t sb_service_subscribe(sb_session_t* session, uint32_t service_kind, const void* filter_payload, uint32_t filter_len, uint32_t* out_stream_id, sb_error_t** out_err);
sb_status_t sb_service_poll(sb_session_t* session, uint32_t stream_id, void* out_buf, uint32_t out_buf_len, uint32_t* out_written, sb_error_t** out_err);
sb_status_t sb_service_unsubscribe(sb_session_t* session, uint32_t stream_id, sb_error_t** out_err);
```

## Memory Ownership Rules
1. API-owned strings/pointers from getters remain valid until next fetch call or result close.
2. Caller buffers passed for input are copied before function returns.
3. `sb_error_t` objects are caller-freed via `sb_error_destroy`.

```c
void sb_error_destroy(sb_error_t* err);
```

## Threading Rules
1. `sb_env_t` is thread-safe for independent connection creation.
2. `sb_conn_t` is not concurrent-mutation safe; one thread at a time for control operations.
3. `sb_session_t` can execute one active statement at a time unless explicit async API extension is enabled.
4. `sb_result_t` is single-consumer.

## Error Handling Contract
- all API functions return `sb_status_t` and optionally `sb_error_t`.
- no exceptions in C ABI.
- deterministic error codes map to protocol/domain codes.

## Mode-Specific Behavior

### Embedded Mode
- bypasses listener and IPC protocol stack.
- uses in-process engine execution path.

### IPC Mode
- client library is IPC protocol client to `sb_ipc_server`.

### INET Mode
- client library speaks SBWP to listener/parser path.
- direct vs manager-proxy is a front-door policy distinction within INET mode.
- the current ScratchBird-driver C/C++ lane implements only INET direct/managed;
  embedded and IPC are delegated to ScratchBird runtime/server layers.

## Compatibility and Versioning
- breaking ABI changes require major version increment.
- additive struct fields require `struct_size` guards.

## 2026-03-28 Audit Normalization Update

- Section `30` is normalized to the code-backed `partial` standard.
- Current authority is bounded to the shipped `ScratchBird-driver` surfaces, especially `tracks/p3/drivers/*`, shared connectivity docs, and the concrete CLI/runtime seams.
- Direct native and manager-proxy are the current portable client contract.
- Local runtime modes such as `embedded` and `local-ipc` are bounded tooling/runtime surfaces, not universal parity claims for every maintained language driver.
- The C/C++ lane in the current driver repo is intentionally IP-only; current CLI `embedded` mode is routed through local IPC in the present beta C++ runtime.
- Tool command truth is bounded to the shipped `sb_isql`, `sb_admin`, `sb_backup`, `sb_security`, `sb_verify`, and `sbdriver-conformance` surfaces.
- Recovery language follows MGA/session-repair rules and explicitly excludes WAL-style transaction replay.
- Forensic replay, migration/passthrough, and replication control narratives remain bounded, checklist-only, or target-state-only unless a shipped lane-local control surface is proven.
- Driver-lane claims must stay tied to the current maintained lane set and must not assume universal cross-language parity from section-outline text alone.

## 2026-03-28 Hardening Promotion Update

- Section `30` now carries explicit bounded authority for current maintained `ScratchBird-driver` `p3` lanes.
- Embedded and linked-library language is bounded by the current IP-only C/C++ lane plus tool-local `embedded` or `local-ipc` seams.
- Direct native and manager-proxy remain the current portable client baseline.
- CLI authority is bounded to shipped `sb_isql`, `sb_admin`, `sb_backup`, `sb_security`, `sb_verify`, and `sbdriver-conformance`.
- Error and reconnect language is bounded to deterministic MGA/session repair and explicitly excludes whole-transaction replay.
- Installer, replay, migration, passthrough, and replication client-control claims remain bounded or `target_state_only` unless maintained lane-local proof is promoted.
