# ScratchBird C API Specification

---

## IMPLEMENTATION STATUS: NOT IMPLEMENTED - FUTURE/DESIGN ONLY

**IMPORTANT:** This specification is for a **FUTURE C API** that does **NOT CURRENTLY EXIST** in the ScratchBird codebase.

**Current Implementation:**
- ScratchBird currently uses C++ classes and APIs
- No C API wrapper exists
- All types/functions documented here are aspirational
- Implementation estimated at 40-80 hours of work

**This document has been moved to `docs/specifications/future/` to clearly indicate it is a design specification, not current functionality.**

**For current API documentation, see the C++ header files in `include/scratchbird/`**

---

## Overview

The ScratchBird C API (PLANNED) will provide low-level access to the database engine, designed for:
- Direct embedding (no network overhead)
- Language binding development (Python, Java, etc.)
- High-performance applications
- Tool development (backup, restore, monitoring)

## Design Principles

1. **Thread-Safe**: All functions are thread-safe unless explicitly noted
2. **Zero-Copy**: Minimize data copying where possible
3. **Async-First**: Support both synchronous and asynchronous operations
4. **Resource-Safe**: RAII-style resource management with explicit cleanup
5. **Error-Rich**: Detailed error information with error chains
6. **Version-Stable**: ABI stability within major versions

## Core Handle Types

```c
// Opaque handle types for safety
typedef struct sb_environment*     sb_environment_t;
typedef struct sb_database*        sb_database_t;
typedef struct sb_connection*      sb_connection_t;
typedef struct sb_transaction*     sb_transaction_t;
typedef struct sb_statement*       sb_statement_t;
typedef struct sb_cursor*          sb_cursor_t;
typedef struct sb_result*          sb_result_t;
typedef struct sb_blob*            sb_blob_t;
typedef struct sb_event*           sb_event_t;
typedef struct sb_backup*          sb_backup_t;

// Error handling
typedef struct sb_error*           sb_error_t;
typedef struct sb_error_info {
    int32_t     code;               // Error code
    const char* sqlstate;           // SQLSTATE
    const char* message;            // Error message
    const char* detail;             // Detailed information
    const char* hint;               // Hint for resolution
    const char* file;               // Source file
    int32_t     line;               // Source line
    sb_error_t  cause;              // Chained error (if any)
} sb_error_info_t;
```

## Environment Management

```c
// Initialize ScratchBird environment (call once per process)
sb_status_t sb_init(
    sb_environment_t* env,
    const sb_config_t* config       // Optional configuration
);

// Environment configuration
typedef struct sb_config {
    // Memory management
    size_t      page_cache_size;    // Page cache size in bytes
    size_t      sort_buffer_size;   // Sort buffer size
    size_t      work_mem;           // Working memory per operation
    
    // Threading
    uint32_t    max_connections;    // Maximum connections
    uint32_t    max_threads;        // Thread pool size
    
    // Paths
    const char* data_directory;     // Data directory path
    const char* temp_directory;     // Temporary files path
    const char* plugin_directory;   // Plugin directory
    
    // Logging
    sb_log_level_t log_level;       // Log level
    sb_log_callback_t log_callback; // Custom log handler
    
    // Features
    bool        enable_jit;         // Enable JIT compilation
    bool        enable_parallel;    // Enable parallel execution
    bool        enable_compression; // Enable compression
    
} sb_config_t;

// Shutdown environment
sb_status_t sb_shutdown(
    sb_environment_t env
);

// Get version information
const char* sb_version(void);
uint32_t    sb_version_number(void);
const char* sb_build_info(void);
```

## Database Operations

```c
// Create a new database
sb_status_t sb_create_database(
    sb_environment_t env,
    const char* path,
    const sb_create_options_t* options,
    sb_database_t* database
);

typedef struct sb_create_options {
    sb_page_size_t  page_size;      // 8K, 16K, 32K, 64K, 128K
    sb_charset_t    charset;        // Default character set
    sb_collation_t  collation;      // Default collation
    const char*     password;       // Encryption password
    bool            overwrite;      // Overwrite if exists
    
    // Advanced options
    uint32_t        initial_pages;  // Initial file size
    uint32_t        extend_pages;   // Extension size
    bool            auto_sweep;     // Enable auto sweep
    uint32_t        sweep_interval; // Sweep interval
    
} sb_create_options_t;

// Open existing database
sb_status_t sb_open_database(
    sb_environment_t env,
    const char* path,
    const sb_open_options_t* options,
    sb_database_t* database
);

typedef struct sb_open_options {
    sb_access_mode_t access_mode;   // READ_ONLY, READ_WRITE
    bool            exclusive;      // Exclusive access
    const char*     password;       // Decryption password
    uint32_t        cache_pages;    // Cache size for this database
    
} sb_open_options_t;

// Close database
sb_status_t sb_close_database(
    sb_database_t database
);

// Database information
sb_status_t sb_database_info(
    sb_database_t database,
    sb_info_type_t info_type,
    void* buffer,
    size_t* buffer_size
);
```

## Connection Management

```c
// Create connection to database
sb_status_t sb_connect(
    sb_database_t database,
    const sb_connect_options_t* options,
    sb_connection_t* connection
);

typedef struct sb_connect_options {
    const char*     username;       // Username
    const char*     password;       // Password
    const char*     role;           // Role name
    sb_charset_t    charset;        // Connection charset
    sb_protocol_t   protocol;       // Protocol type (for Y-Valve)
    uint32_t        timeout;        // Connection timeout (ms)
    
    // Session parameters
    const char*     application_name;
    const char*     client_encoding;
    const char*     search_path;    // Schema search path
    
} sb_connect_options_t;

// Disconnect
sb_status_t sb_disconnect(
    sb_connection_t connection
);

// Connection state
typedef enum sb_connection_state {
    SB_CONN_IDLE,                   // Ready for query
    SB_CONN_ACTIVE,                 // Query in progress
    SB_CONN_IN_TRANSACTION,        // In transaction block
    SB_CONN_ERROR,                  // In error state
    SB_CONN_CLOSED                  // Connection closed
} sb_connection_state_t;

sb_connection_state_t sb_connection_state(
    sb_connection_t connection
);

// Reset connection state
sb_status_t sb_reset_connection(
    sb_connection_t connection
);
```

## Transaction Management

```c
// Start transaction
sb_status_t sb_begin_transaction(
    sb_connection_t connection,
    const sb_transaction_options_t* options,
    sb_transaction_t* transaction
);

typedef struct sb_transaction_options {
    sb_isolation_level_t isolation_level;
    sb_access_mode_t    access_mode;    // READ_ONLY, READ_WRITE
    bool                deferrable;     // Deferrable transaction
    uint32_t            timeout_ms;     // Transaction timeout
    const char*         name;           // Transaction name
    
} sb_transaction_options_t;

// Isolation levels
typedef enum sb_isolation_level {
    SB_ISOLATION_READ_COMMITTED,
    SB_ISOLATION_REPEATABLE_READ,
    SB_ISOLATION_SERIALIZABLE
} sb_isolation_level_t;

// Commit transaction
sb_status_t sb_commit(
    sb_transaction_t transaction
);

// Rollback transaction
sb_status_t sb_rollback(
    sb_transaction_t transaction
);

// Savepoint management
sb_status_t sb_savepoint(
    sb_transaction_t transaction,
    const char* name
);

sb_status_t sb_release_savepoint(
    sb_transaction_t transaction,
    const char* name
);

sb_status_t sb_rollback_to_savepoint(
    sb_transaction_t transaction,
    const char* name
);

// Two-phase commit
sb_status_t sb_prepare_transaction(
    sb_transaction_t transaction,
    const char* gid                 // Global transaction ID
);

sb_status_t sb_commit_prepared(
    sb_connection_t connection,
    const char* gid
);

sb_status_t sb_rollback_prepared(
    sb_connection_t connection,
    const char* gid
);
```

## Statement Execution

```c
// Prepare statement
sb_status_t sb_prepare(
    sb_connection_t connection,
    const char* sql,
    sb_statement_t* statement
);

// Prepare with BLR (bypass SQL parsing)
sb_status_t sb_prepare_blr(
    sb_connection_t connection,
    const uint8_t* blr,
    size_t blr_length,
    sb_statement_t* statement
);

// Bind parameters
sb_status_t sb_bind_null(
    sb_statement_t statement,
    uint32_t param_index
);

sb_status_t sb_bind_boolean(
    sb_statement_t statement,
    uint32_t param_index,
    bool value
);

sb_status_t sb_bind_int32(
    sb_statement_t statement,
    uint32_t param_index,
    int32_t value
);

sb_status_t sb_bind_int64(
    sb_statement_t statement,
    uint32_t param_index,
    int64_t value
);

sb_status_t sb_bind_int128(
    sb_statement_t statement,
    uint32_t param_index,
    const sb_int128_t* value
);

sb_status_t sb_bind_float(
    sb_statement_t statement,
    uint32_t param_index,
    float value
);

sb_status_t sb_bind_double(
    sb_statement_t statement,
    uint32_t param_index,
    double value
);

sb_status_t sb_bind_decimal(
    sb_statement_t statement,
    uint32_t param_index,
    const sb_decimal_t* value
);

sb_status_t sb_bind_string(
    sb_statement_t statement,
    uint32_t param_index,
    const char* value,
    size_t length                   // -1 for null-terminated
);

sb_status_t sb_bind_blob(
    sb_statement_t statement,
    uint32_t param_index,
    const void* data,
    size_t length
);

sb_status_t sb_bind_uuid(
    sb_statement_t statement,
    uint32_t param_index,
    const sb_uuid_t* value
);

sb_status_t sb_bind_timestamp(
    sb_statement_t statement,
    uint32_t param_index,
    const sb_timestamp_t* value
);

// Bind arrays
sb_status_t sb_bind_array(
    sb_statement_t statement,
    uint32_t param_index,
    sb_type_t element_type,
    const void* data,
    size_t count
);

// Execute statement
sb_status_t sb_execute(
    sb_statement_t statement,
    sb_result_t* result
);

// Execute with parameters (convenience)
sb_status_t sb_execute_params(
    sb_connection_t connection,
    const char* sql,
    const sb_param_t* params,
    uint32_t param_count,
    sb_result_t* result
);

// Execute batch
sb_status_t sb_execute_batch(
    sb_statement_t statement,
    const sb_batch_params_t* batch,
    sb_result_t* result
);

// Get statement info
sb_status_t sb_statement_info(
    sb_statement_t statement,
    sb_statement_info_t* info
);

typedef struct sb_statement_info {
    uint32_t    param_count;        // Number of parameters
    uint32_t    column_count;       // Number of result columns
    sb_type_t*  param_types;        // Parameter types
    sb_type_t*  column_types;       // Column types
    char**      column_names;       // Column names
    
} sb_statement_info_t;

// Release statement
sb_status_t sb_release_statement(
    sb_statement_t statement
);
```

## Result Set Handling

```c
// Result set navigation
sb_status_t sb_fetch(
    sb_result_t result
);

sb_status_t sb_fetch_many(
    sb_result_t result,
    uint32_t count
);

sb_status_t sb_fetch_all(
    sb_result_t result
);

// Get values
bool sb_is_null(
    sb_result_t result,
    uint32_t column_index
);

sb_status_t sb_get_boolean(
    sb_result_t result,
    uint32_t column_index,
    bool* value
);

sb_status_t sb_get_int32(
    sb_result_t result,
    uint32_t column_index,
    int32_t* value
);

sb_status_t sb_get_int64(
    sb_result_t result,
    uint32_t column_index,
    int64_t* value
);

sb_status_t sb_get_int128(
    sb_result_t result,
    uint32_t column_index,
    sb_int128_t* value
);

sb_status_t sb_get_float(
    sb_result_t result,
    uint32_t column_index,
    float* value
);

sb_status_t sb_get_double(
    sb_result_t result,
    uint32_t column_index,
    double* value
);

sb_status_t sb_get_decimal(
    sb_result_t result,
    uint32_t column_index,
    sb_decimal_t* value
);

sb_status_t sb_get_string(
    sb_result_t result,
    uint32_t column_index,
    const char** value,
    size_t* length
);

sb_status_t sb_get_blob(
    sb_result_t result,
    uint32_t column_index,
    const void** data,
    size_t* length
);

sb_status_t sb_get_uuid(
    sb_result_t result,
    uint32_t column_index,
    sb_uuid_t* value
);

sb_status_t sb_get_timestamp(
    sb_result_t result,
    uint32_t column_index,
    sb_timestamp_t* value
);

// Get by column name
sb_status_t sb_get_by_name(
    sb_result_t result,
    const char* column_name,
    sb_value_t* value
);

// Result metadata
uint32_t sb_column_count(
    sb_result_t result
);

const char* sb_column_name(
    sb_result_t result,
    uint32_t column_index
);

sb_type_t sb_column_type(
    sb_result_t result,
    uint32_t column_index
);

uint64_t sb_row_count(
    sb_result_t result
);

uint64_t sb_affected_rows(
    sb_result_t result
);

// Release result
sb_status_t sb_release_result(
    sb_result_t result
);
```

## Cursor Operations

```c
// Create cursor
sb_status_t sb_declare_cursor(
    sb_connection_t connection,
    const char* name,
    const char* sql,
    const sb_cursor_options_t* options,
    sb_cursor_t* cursor
);

typedef struct sb_cursor_options {
    bool        scrollable;          // Scrollable cursor
    bool        updatable;           // FOR UPDATE
    bool        hold;                // WITH HOLD
    uint32_t    fetch_size;          // Rows per fetch
    
} sb_cursor_options_t;

// Open cursor
sb_status_t sb_open_cursor(
    sb_cursor_t cursor
);

// Fetch from cursor
sb_status_t sb_fetch_cursor(
    sb_cursor_t cursor,
    sb_fetch_direction_t direction,
    int64_t offset,
    sb_result_t* result
);

typedef enum sb_fetch_direction {
    SB_FETCH_NEXT,
    SB_FETCH_PRIOR,
    SB_FETCH_FIRST,
    SB_FETCH_LAST,
    SB_FETCH_ABSOLUTE,
    SB_FETCH_RELATIVE
} sb_fetch_direction_t;

// Update current row
sb_status_t sb_update_cursor(
    sb_cursor_t cursor,
    const char* sql
);

// Delete current row
sb_status_t sb_delete_cursor(
    sb_cursor_t cursor
);

// Close cursor
sb_status_t sb_close_cursor(
    sb_cursor_t cursor
);
```

## BLOB Operations

```c
// Create BLOB
sb_status_t sb_create_blob(
    sb_connection_t connection,
    sb_blob_t* blob
);

// Open existing BLOB
sb_status_t sb_open_blob(
    sb_connection_t connection,
    const sb_blob_id_t* blob_id,
    sb_blob_t* blob
);

// BLOB I/O
sb_status_t sb_blob_read(
    sb_blob_t blob,
    void* buffer,
    size_t buffer_size,
    size_t* bytes_read
);

sb_status_t sb_blob_write(
    sb_blob_t blob,
    const void* data,
    size_t data_size,
    size_t* bytes_written
);

sb_status_t sb_blob_seek(
    sb_blob_t blob,
    int64_t offset,
    sb_seek_origin_t origin
);

sb_status_t sb_blob_truncate(
    sb_blob_t blob,
    size_t new_size
);

// Get BLOB info
sb_status_t sb_blob_info(
    sb_blob_t blob,
    sb_blob_info_t* info
);

typedef struct sb_blob_info {
    sb_blob_id_t    id;             // BLOB ID
    size_t          size;            // Total size
    uint32_t        segment_size;    // Segment size
    sb_type_t       subtype;         // BLOB subtype
    
} sb_blob_info_t;

// Close BLOB
sb_status_t sb_close_blob(
    sb_blob_t blob
);
```

## Event Handling

```c
// Register for events
sb_status_t sb_register_event(
    sb_connection_t connection,
    const char* event_name,
    sb_event_callback_t callback,
    void* user_data,
    sb_event_t* event
);

typedef void (*sb_event_callback_t)(
    sb_event_t event,
    const char* event_name,
    const void* event_data,
    size_t data_size,
    void* user_data
);

// Wait for events
sb_status_t sb_wait_for_event(
    sb_connection_t connection,
    sb_event_t* events,
    uint32_t event_count,
    uint32_t timeout_ms,
    uint32_t* triggered_index
);

// Post event
sb_status_t sb_post_event(
    sb_connection_t connection,
    const char* event_name,
    const void* event_data,
    size_t data_size
);

// Unregister event
sb_status_t sb_unregister_event(
    sb_event_t event
);
```

## Backup and Restore

```c
// Create backup
sb_status_t sb_backup_start(
    sb_database_t database,
    const char* backup_path,
    const sb_backup_options_t* options,
    sb_backup_t* backup
);

typedef struct sb_backup_options {
    bool        incremental;         // Incremental backup
    bool        compressed;          // Compress backup
    bool        encrypted;           // Encrypt backup
    const char* password;           // Encryption password
    uint32_t    parallel_streams;    // Parallel backup
    
    // Callback for progress
    sb_backup_progress_callback_t progress_callback;
    void*       user_data;
    
} sb_backup_options_t;

// Backup progress
sb_status_t sb_backup_step(
    sb_backup_t backup,
    uint32_t pages                  // Pages to backup (-1 for all)
);

// Finish backup
sb_status_t sb_backup_finish(
    sb_backup_t backup
);

// Restore database
sb_status_t sb_restore(
    sb_environment_t env,
    const char* backup_path,
    const char* database_path,
    const sb_restore_options_t* options
);
```

## Async Operations

```c
// Async execution
sb_status_t sb_execute_async(
    sb_statement_t statement,
    sb_async_callback_t callback,
    void* user_data,
    sb_async_handle_t* handle
);

typedef void (*sb_async_callback_t)(
    sb_async_handle_t handle,
    sb_status_t status,
    sb_result_t result,
    void* user_data
);

// Poll async operation
sb_status_t sb_async_poll(
    sb_async_handle_t handle,
    sb_async_state_t* state
);

typedef enum sb_async_state {
    SB_ASYNC_PENDING,
    SB_ASYNC_READY,
    SB_ASYNC_ERROR,
    SB_ASYNC_CANCELLED
} sb_async_state_t;

// Wait for async completion
sb_status_t sb_async_wait(
    sb_async_handle_t handle,
    uint32_t timeout_ms
);

// Cancel async operation
sb_status_t sb_async_cancel(
    sb_async_handle_t handle
);
```

## Error Handling

```c
// Get last error
sb_error_t sb_last_error(
    sb_connection_t connection
);

// Error details
int32_t sb_error_code(
    sb_error_t error
);

const char* sb_error_message(
    sb_error_t error
);

const char* sb_error_sqlstate(
    sb_error_t error
);

const char* sb_error_detail(
    sb_error_t error
);

const char* sb_error_hint(
    sb_error_t error
);

// Error chain
sb_error_t sb_error_cause(
    sb_error_t error
);

// Clear error
void sb_clear_error(
    sb_connection_t connection
);

// Error callback
typedef void (*sb_error_callback_t)(
    sb_connection_t connection,
    sb_error_t error,
    void* user_data
);

sb_status_t sb_set_error_callback(
    sb_connection_t connection,
    sb_error_callback_t callback,
    void* user_data
);
```

## Utility Functions

```c
// UUID operations
void sb_uuid_generate(
    sb_uuid_t* uuid
);

void sb_uuid_to_string(
    const sb_uuid_t* uuid,
    char* buffer                    // Must be 37 bytes
);

sb_status_t sb_uuid_from_string(
    const char* string,
    sb_uuid_t* uuid
);

// Type conversions
const char* sb_type_name(
    sb_type_t type
);

size_t sb_type_size(
    sb_type_t type
);

// Memory management
void* sb_malloc(size_t size);
void* sb_realloc(void* ptr, size_t size);
void  sb_free(void* ptr);

// Logging
typedef enum sb_log_level {
    SB_LOG_DEBUG,
    SB_LOG_INFO,
    SB_LOG_WARNING,
    SB_LOG_ERROR,
    SB_LOG_FATAL
} sb_log_level_t;

typedef void (*sb_log_callback_t)(
    sb_log_level_t level,
    const char* message,
    void* user_data
);

void sb_set_log_callback(
    sb_log_callback_t callback,
    void* user_data
);
```

## Thread Safety

```c
// Thread-local storage
sb_status_t sb_tls_set(
    const char* key,
    void* value
);

void* sb_tls_get(
    const char* key
);

// Connection pooling
typedef struct sb_pool* sb_pool_t;

sb_status_t sb_pool_create(
    sb_database_t database,
    const sb_pool_options_t* options,
    sb_pool_t* pool
);

typedef struct sb_pool_options {
    uint32_t    min_connections;
    uint32_t    max_connections;
    uint32_t    idle_timeout;
    bool        test_on_borrow;
    
} sb_pool_options_t;

sb_status_t sb_pool_acquire(
    sb_pool_t pool,
    sb_connection_t* connection
);

sb_status_t sb_pool_release(
    sb_pool_t pool,
    sb_connection_t connection
);

sb_status_t sb_pool_destroy(
    sb_pool_t pool
);
```

## Header File Structure

```c
// scratchbird.h - Main header file

#ifndef SCRATCHBIRD_H
#define SCRATCHBIRD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Version macros
#define SB_VERSION_MAJOR    1
#define SB_VERSION_MINOR    0
#define SB_VERSION_PATCH    0
#define SB_VERSION_STRING   "1.0.0"

// Export macros
#ifdef _WIN32
    #ifdef SB_BUILDING_DLL
        #define SB_API __declspec(dllexport)
    #else
        #define SB_API __declspec(dllimport)
    #endif
#else
    #define SB_API __attribute__((visibility("default")))
#endif

// Status codes
typedef enum sb_status {
    SB_SUCCESS = 0,
    SB_ERROR = -1,
    SB_INVALID_PARAMETER = -2,
    SB_OUT_OF_MEMORY = -3,
    SB_NOT_FOUND = -4,
    SB_ALREADY_EXISTS = -5,
    SB_PERMISSION_DENIED = -6,
    SB_CONNECTION_FAILED = -7,
    SB_TIMEOUT = -8,
    SB_CANCELLED = -9,
    SB_NOT_SUPPORTED = -10,
    SB_BUSY = -11,
    SB_DEADLOCK = -12,
    SB_CONSTRAINT_VIOLATION = -13,
    SB_TYPE_MISMATCH = -14,
    SB_PROTOCOL_ERROR = -15,
    // ... more status codes
} sb_status_t;

// All function declarations...

#ifdef __cplusplus
}
#endif

#endif // SCRATCHBIRD_H
```

## Usage Examples

### Basic Usage
```c
#include <scratchbird.h>

int main() {
    sb_environment_t env;
    sb_database_t db;
    sb_connection_t conn;
    sb_result_t result;
    
    // Initialize environment
    if (sb_init(&env, NULL) != SB_SUCCESS) {
        fprintf(stderr, "Failed to initialize\n");
        return 1;
    }
    
    // Open database
    sb_open_options_t opts = {
        .access_mode = SB_ACCESS_READ_WRITE,
        .exclusive = false
    };
    
    if (sb_open_database(env, "mydb.sdb", &opts, &db) != SB_SUCCESS) {
        fprintf(stderr, "Failed to open database\n");
        return 1;
    }
    
    // Connect
    sb_connect_options_t conn_opts = {
        .username = "admin",
        .password = "secret"
    };
    
    if (sb_connect(db, &conn_opts, &conn) != SB_SUCCESS) {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }
    
    // Execute query
    if (sb_execute_params(conn, 
                         "SELECT * FROM users WHERE age > ?",
                         &(sb_param_t){.type = SB_TYPE_INT32, .value.i32 = 18},
                         1,
                         &result) == SB_SUCCESS) {
        
        // Process results
        while (sb_fetch(result) == SB_SUCCESS) {
            const char* name;
            size_t name_len;
            sb_get_string(result, 0, &name, &name_len);
            printf("User: %.*s\n", (int)name_len, name);
        }
        
        sb_release_result(result);
    }
    
    // Cleanup
    sb_disconnect(conn);
    sb_close_database(db);
    sb_shutdown(env);
    
    return 0;
}
```

### Prepared Statements
```c
sb_statement_t stmt;
sb_prepare(conn, "INSERT INTO users (name, age) VALUES (?, ?)", &stmt);

sb_bind_string(stmt, 0, "Alice", -1);
sb_bind_int32(stmt, 1, 25);
sb_execute(stmt, NULL);

sb_bind_string(stmt, 0, "Bob", -1);
sb_bind_int32(stmt, 1, 30);
sb_execute(stmt, NULL);

sb_release_statement(stmt);
```

### Transactions
```c
sb_transaction_t txn;
sb_transaction_options_t txn_opts = {
    .isolation_level = SB_ISOLATION_SERIALIZABLE,
    .access_mode = SB_ACCESS_READ_WRITE
};

sb_begin_transaction(conn, &txn_opts, &txn);

// Do work...
sb_execute_params(conn, "UPDATE accounts SET balance = balance - 100 WHERE id = 1", NULL, 0, NULL);
sb_execute_params(conn, "UPDATE accounts SET balance = balance + 100 WHERE id = 2", NULL, 0, NULL);

// Commit or rollback
if (everything_ok) {
    sb_commit(txn);
} else {
    sb_rollback(txn);
}
```

## Language Bindings

This C API is designed to be easily wrapped for other languages:

- **Python**: Using ctypes or CFFI
- **Java**: Via JNI
- **Go**: Using CGO
- **Rust**: Using bindgen
- **Node.js**: Using N-API
- **.NET**: Using P/Invoke

## Performance Considerations

1. **Zero-Copy Results**: String and BLOB data returned as pointers
2. **Prepared Statements**: Reuse for better performance
3. **Batch Operations**: Use batch API for bulk inserts
4. **Async Operations**: For concurrent workloads
5. **Connection Pooling**: Built-in pool support
6. **BLR Direct**: Bypass SQL parsing with pre-compiled BLR