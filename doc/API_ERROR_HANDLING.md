# ScratchBird API - Error Handling

## Overview

The ScratchBird Error Handling API provides comprehensive error detection, reporting, and recovery mechanisms. This covers status vectors, error codes, SQL states, exception handling, and debugging utilities for robust application development.

## Core Error Interface

### Primary Header Files
```cpp
#include <scratchbird/include/scratchbird.h>
#include <scratchbird/include/sbclient_pub.h>
#include <scratchbird/include/sb_error.h>
```

### Status Vector Structure

The status vector is the primary error reporting mechanism in ScratchBird:

```cpp
typedef SB_STATUS[20] SB_STATUS_VECTOR;

// Status vector layout:
// [0] - Error type (sb_arg_gds, sb_arg_number, sb_arg_string, etc.)
// [1] - Primary error code
// [2] - Error type for additional info
// [3] - Secondary error code or parameter
// ... (pairs continue)
// [n] - sb_arg_end (terminator)
```

### Error Information Structure

```cpp
typedef struct {
    long        error_code;         // Primary error code
    long        sql_code;           // SQL error code  
    char        error_message[512]; // Formatted error message
    char        sql_state[6];       // SQL state code
    int         line_number;        // Source line number (if available)
    char        routine_name[64];   // Function name where error occurred
    char        facility[32];       // Error facility (JRD, DSQL, etc.)
    int         severity;           // Error severity level
    void*       context_info;       // Additional context information
} SB_ERROR_INFO;
```

## Error Detection

### Status Vector Checking

Every ScratchBird API function returns an error status that should be checked:

```cpp
SB_STATUS status[20];
SB_DB_HANDLE db = 0;

if (sb_attach_database(status, strlen(db_path), db_path, 
                       &db, dpb_length, dpb_buffer)) {
    // Error occurred - status[1] contains the error code
    printf("Database connection failed with error %ld\n", status[1]);
    sb_print_status(status);
    return -1;
}

// Success - database handle is valid
printf("Database connected successfully\n");
```

### Error Code Categories

```cpp
// Error type indicators
#define sb_arg_gds          0   // ScratchBird error code
#define sb_arg_string       1   // String parameter
#define sb_arg_cstring      2   // C string parameter  
#define sb_arg_number       3   // Numeric parameter
#define sb_arg_interpreted  4   // Interpreted parameter
#define sb_arg_vms          5   // VMS specific error
#define sb_arg_unix         6   // Unix specific error
#define sb_arg_domain       7   // Domain error
#define sb_arg_dos          8   // DOS specific error
#define sb_arg_mpexl        9   // MPE/XL specific error
#define sb_arg_mpexl_ipc    10  // MPE/XL IPC error
#define sb_arg_next_mach    15  // Next machine error
#define sb_arg_netware      16  // NetWare error
#define sb_arg_win32        17  // Win32 error
#define sb_arg_warning      18  // Warning message
#define sb_arg_end          0   // End of status vector
```

## Error Codes

### Core Database Errors

```cpp
// Connection errors
#define sb_network_error            335544721L
#define sb_connection_lost          335544722L  
#define sb_connection_rejected      335544723L
#define sb_login_error             335544724L
#define sb_database_unavailable    335544725L
#define sb_server_misconfigured    335544726L

// Transaction errors
#define sb_deadlock                 335544336L
#define sb_lock_timeout            335544337L
#define sb_lock_conflict           335544338L
#define sb_no_transaction          335544339L
#define sb_transaction_in_use      335544340L

// Statement errors
#define sb_dsql_cursor_err          335544569L
#define sb_dsql_sqlda_err          335544570L
#define sb_dsql_relation_err       335544571L
#define sb_dsql_field_err          335544572L
#define sb_dsql_datatype_err       335544573L

// Schema errors (ScratchBird extensions)
#define sb_schema_not_found        335544800L
#define sb_schema_access_denied    335544801L
#define sb_schema_hierarchy_error  335544802L
#define sb_schema_circular_ref     335544803L

// Index errors
#define sb_no_segments_err         335544650L
#define sb_index_inactive          335544651L
#define sb_key_too_big            335544652L
#define sb_hash_index_error       335544850L  // ScratchBird extension
#define sb_gin_index_error        335544851L  // ScratchBird extension
#define sb_spatial_index_error    335544852L  // ScratchBird extension
```

### SQL Error Codes

SQL error codes follow standard conventions with ScratchBird extensions:

```cpp
// Standard SQL errors
#define SQLCODE_SUCCESS             0
#define SQLCODE_NO_MORE_ROWS       100
#define SQLCODE_FEATURE_NOT_IMPL  -901
#define SQLCODE_INVALID_CURSOR    -502
#define SQLCODE_DEADLOCK          -913
#define SQLCODE_LOCK_TIMEOUT      -911

// ScratchBird-specific SQL errors
#define SQLCODE_SCHEMA_ERROR      -950
#define SQLCODE_UDR_ERROR         -951
#define SQLCODE_VECTOR_ERROR      -952
#define SQLCODE_SPATIAL_ERROR     -953
#define SQLCODE_ARRAY_ERROR       -954
```

## Error Reporting Functions

### sb_print_status()
Prints formatted error information to stderr.

```cpp
void sb_print_status(SB_STATUS* status_vector);
```

**Example:**
```cpp
SB_STATUS status[20];

if (sb_attach_database(status, strlen(db_path), db_path, 
                       &db, dpb_length, dpb_buffer)) {
    printf("Database connection failed:\n");
    sb_print_status(status);
    // Output:
    // Statement failed, SQLSTATE = 08001
    // connection rejected by remote interface
    // -At block line: 0, col: 0
}
```

### sb_sqlcode()
Converts status vector to SQL error code.

```cpp
SB_SLONG sb_sqlcode(SB_STATUS* status_vector);
```

**Example:**
```cpp
SB_STATUS status[20];
SB_SLONG sql_error;

if (sb_dsql_fetch(status, stmt, 4, output_sqlda)) {
    sql_error = sb_sqlcode(status);
    
    if (sql_error == 100) {
        printf("No more rows to fetch\n");
    } else {
        printf("Fetch error: SQL code %ld\n", sql_error);
        sb_print_status(status);
    }
}
```

### sb_sql_interprete()
Converts SQL error code to message text.

```cpp
SB_SLONG sb_sql_interprete(
    short       sql_code,
    char*       buffer,
    short       buffer_length
);
```

**Example:**
```cpp
char error_msg[256];
short sql_code = -913;

sb_sql_interprete(sql_code, error_msg, sizeof(error_msg));
printf("SQL Error %d: %s\n", sql_code, error_msg);
// Output: SQL Error -913: deadlock
```

### sb_interprete()
Converts ScratchBird error code to message text.

```cpp
SB_SLONG sb_interprete(
    char*       buffer,
    short       buffer_length,
    SB_STATUS** status_vector
);
```

**Example:**
```cpp
SB_STATUS status[20];
char error_buffer[512];
SB_STATUS* status_ptr = status;

// Get first error message
if (sb_interprete(error_buffer, sizeof(error_buffer), &status_ptr)) {
    printf("Primary error: %s\n", error_buffer);
}

// Get additional error details
while (sb_interprete(error_buffer, sizeof(error_buffer), &status_ptr)) {
    printf("Additional info: %s\n", error_buffer);
}
```

## Error Information Extraction

### sb_get_error_info()
Extracts comprehensive error information from status vector.

```cpp
void sb_get_error_info(
    SB_STATUS*      status_vector,
    SB_ERROR_INFO*  error_info
);
```

**Example:**
```cpp
SB_STATUS status[20];
SB_ERROR_INFO error_info;

if (sb_dsql_prepare(status, transaction, stmt, 0, bad_sql, 4, NULL)) {
    sb_get_error_info(status, &error_info);
    
    printf("Error Details:\n");
    printf("  Error Code: %ld\n", error_info.error_code);
    printf("  SQL Code: %ld\n", error_info.sql_code);
    printf("  SQL State: %s\n", error_info.sql_state);
    printf("  Message: %s\n", error_info.error_message);
    printf("  Facility: %s\n", error_info.facility);
    printf("  Severity: %d\n", error_info.severity);
    
    if (error_info.line_number > 0) {
        printf("  Line: %d\n", error_info.line_number);
    }
    
    if (strlen(error_info.routine_name) > 0) {
        printf("  Function: %s\n", error_info.routine_name);
    }
}
```

### sb_get_sql_state()
Extracts SQL state code from status vector.

```cpp
void sb_get_sql_state(
    SB_STATUS*  status_vector,
    char*       sql_state_buffer
);
```

**SQL State Categories:**
```cpp
// Connection states
"08001"  // connection_exception
"08003"  // connection_does_not_exist  
"08006"  // connection_failure

// Transaction states  
"25000"  // invalid_transaction_state
"40001"  // serialization_failure (deadlock)
"40002"  // transaction_integrity_constraint_violation

// Statement states
"07000"  // dynamic_sql_error
"42000"  // syntax_error_or_access_rule_violation
"42S02"  // base_table_or_view_not_found

// Data states
"22001"  // string_data_right_truncation
"22003"  // numeric_value_out_of_range
"22007"  // invalid_datetime_format
"22012"  // division_by_zero

// ScratchBird extensions
"SB001"  // schema_hierarchy_error
"SB002"  // udr_execution_error
"SB003"  // vector_operation_error
"SB004"  // spatial_data_error
```

## Exception Handling Patterns

### Basic Error Handling

```cpp
int execute_operation(SB_DB_HANDLE db) {
    SB_STATUS status[20];
    SB_TR_HANDLE transaction = 0;
    SB_STMT_HANDLE stmt = 0;
    int result = -1;
    
    // Start transaction
    if (sb_start_transaction(status, &transaction, 1, &db, 
                           tpb_length, tpb_buffer)) {
        printf("Failed to start transaction:\n");
        sb_print_status(status);
        return -1;
    }
    
    // Allocate statement
    if (sb_dsql_allocate_statement(status, db, &stmt)) {
        printf("Failed to allocate statement:\n");
        sb_print_status(status);
        goto rollback;
    }
    
    // Prepare statement
    char sql[] = "INSERT INTO customers (name, email) VALUES (?, ?)";
    if (sb_dsql_prepare(status, transaction, stmt, 0, sql, 4, NULL)) {
        printf("Failed to prepare statement:\n");
        sb_print_status(status);
        goto cleanup;
    }
    
    // Execute with parameters
    if (sb_dsql_execute(status, transaction, stmt, 4, input_sqlda)) {
        printf("Failed to execute statement:\n");
        sb_print_status(status);
        goto cleanup;
    }
    
    // Commit transaction
    if (sb_commit_transaction(status, &transaction)) {
        printf("Failed to commit transaction:\n");
        sb_print_status(status);
        goto cleanup;
    }
    
    printf("Operation completed successfully\n");
    result = 0;
    goto cleanup;
    
rollback:
    if (transaction) {
        sb_rollback_transaction(status, &transaction);
    }
    
cleanup:
    if (stmt) {
        sb_dsql_free_statement(status, &stmt, SB_DROP);
    }
    
    return result;
}
```

### Specific Error Handling

```cpp
int handle_specific_errors(SB_DB_HANDLE db) {
    SB_STATUS status[20];
    SB_ERROR_INFO error_info;
    
    if (sb_dsql_execute_immediate(status, db, transaction, 0,
                                 "INSERT INTO users (email) VALUES ('duplicate@test.com')",
                                 4, NULL)) {
        
        sb_get_error_info(status, &error_info);
        
        switch (error_info.error_code) {
            case sb_unique_key_violation:
                printf("Duplicate email address detected\n");
                return handle_duplicate_email();
                
            case sb_foreign_key_violation:
                printf("Invalid foreign key reference\n");
                return handle_invalid_reference();
                
            case sb_check_constraint:
                printf("Data validation failed\n");
                return handle_validation_error();
                
            case sb_deadlock:
                printf("Deadlock detected - retrying operation\n");
                return retry_with_backoff();
                
            case sb_lock_timeout:
                printf("Lock timeout - operation taking too long\n");
                return handle_timeout();
                
            default:
                printf("Unexpected error %ld: %s\n", 
                       error_info.error_code, error_info.error_message);
                return -1;
        }
    }
    
    return 0;
}
```

### Retry Logic with Exponential Backoff

```cpp
int execute_with_retry(SB_DB_HANDLE db, 
                      int (*operation)(SB_DB_HANDLE, SB_TR_HANDLE),
                      int max_retries) {
    SB_STATUS status[20];
    SB_ERROR_INFO error_info;
    
    for (int attempt = 0; attempt < max_retries; attempt++) {
        SB_TR_HANDLE transaction = 0;
        
        // Start fresh transaction
        if (sb_start_transaction(status, &transaction, 1, &db, 
                               tpb_length, tpb_buffer)) {
            sb_print_status(status);
            return -1;
        }
        
        int result = operation(db, transaction);
        
        if (result == 0) {
            // Success
            if (sb_commit_transaction(status, &transaction) == 0) {
                return 0;
            }
        }
        
        // Check error type
        sb_get_error_info(status, &error_info);
        
        bool should_retry = false;
        int delay_ms = 100;
        
        switch (error_info.error_code) {
            case sb_deadlock:
                should_retry = true;
                delay_ms = (1 << attempt) * 100;  // Exponential backoff
                printf("Deadlock on attempt %d, retrying in %dms\n", 
                       attempt + 1, delay_ms);
                break;
                
            case sb_lock_timeout:
                should_retry = true;
                delay_ms = (1 << attempt) * 200;
                printf("Lock timeout on attempt %d, retrying in %dms\n",
                       attempt + 1, delay_ms);
                break;
                
            case sb_connection_lost:
                should_retry = true;
                delay_ms = (1 << attempt) * 500;
                printf("Connection lost on attempt %d, retrying in %dms\n",
                       attempt + 1, delay_ms);
                break;
                
            default:
                // Don't retry for other errors
                sb_rollback_transaction(status, &transaction);
                return -1;
        }
        
        sb_rollback_transaction(status, &transaction);
        
        if (!should_retry || attempt == max_retries - 1) {
            printf("Operation failed after %d attempts\n", attempt + 1);
            return -1;
        }
        
        // Wait before retry
        usleep(delay_ms * 1000);
    }
    
    return -1;
}
```

## Warning Handling

### Warning Detection

ScratchBird can return warnings without failing the operation:

```cpp
bool sb_has_warnings(SB_STATUS* status_vector);

void sb_get_warnings(
    SB_STATUS*      status_vector,
    SB_ERROR_INFO*  warning_info,
    int             max_warnings,
    int*            warning_count
);
```

**Example:**
```cpp
SB_STATUS status[20];
SB_ERROR_INFO warnings[10];
int warning_count = 0;

// Execute statement that might generate warnings
sb_dsql_execute_immediate(status, db, transaction, 0,
    "INSERT INTO customers (name, credit_limit) "
    "VALUES ('Test Customer', 999999999)", 4, NULL);

// Check for warnings even if operation succeeded
if (sb_has_warnings(status)) {
    sb_get_warnings(status, warnings, 10, &warning_count);
    
    for (int i = 0; i < warning_count; i++) {
        printf("Warning %d: %s\n", i + 1, warnings[i].error_message);
    }
}
```

## Error Context and Debugging

### Enhanced Error Context

```cpp
typedef struct {
    char        sql_text[2048];     // SQL statement that failed
    int         parameter_count;    // Number of parameters
    char        parameter_values[1024]; // Parameter values (if available)
    char        execution_plan[2048];   // Query execution plan
    SB_INT64    affected_rows;      // Rows affected before error
    char        trigger_name[64];   // Trigger name if error in trigger
    char        procedure_name[64]; // Procedure name if error in procedure
    int         call_stack_depth;  // Call stack depth
    char        call_stack[512];    // Formatted call stack
} SB_ERROR_CONTEXT;

void sb_get_error_context(
    SB_STATUS*          status_vector,
    SB_STMT_HANDLE      stmt_handle,
    SB_ERROR_CONTEXT*   context
);
```

### Debug Information

```cpp
// Enable detailed error reporting
void sb_set_debug_mode(bool enable_debug);

// Set error callback for logging
typedef void (*SB_ERROR_CALLBACK)(
    SB_ERROR_INFO*      error_info,
    SB_ERROR_CONTEXT*   context,
    void*               user_data
);

void sb_set_error_callback(
    SB_ERROR_CALLBACK   callback,
    void*               user_data
);
```

**Debug Example:**
```cpp
void error_logger(SB_ERROR_INFO* error_info, 
                 SB_ERROR_CONTEXT* context,
                 void* user_data) {
    FILE* log_file = (FILE*)user_data;
    
    fprintf(log_file, "[%s] Error %ld in %s: %s\n",
            get_timestamp(), error_info->error_code,
            error_info->routine_name, error_info->error_message);
    
    if (strlen(context->sql_text) > 0) {
        fprintf(log_file, "SQL: %s\n", context->sql_text);
    }
    
    if (context->call_stack_depth > 0) {
        fprintf(log_file, "Call stack: %s\n", context->call_stack);
    }
    
    fflush(log_file);
}

int main() {
    FILE* error_log = fopen("scratchbird_errors.log", "a");
    
    // Enable debug mode and error logging
    sb_set_debug_mode(true);
    sb_set_error_callback(error_logger, error_log);
    
    // ... application code ...
    
    fclose(error_log);
    return 0;
}
```

## Error Recovery Strategies

### Connection Recovery

```cpp
int recover_connection(SB_DB_HANDLE* db_handle) {
    SB_STATUS status[20];
    
    // Try to detach cleanly first
    if (*db_handle) {
        sb_detach_database(status, db_handle);
        *db_handle = 0;
    }
    
    // Wait before reconnection attempt
    sleep(1);
    
    // Attempt reconnection
    if (sb_attach_database(status, strlen(db_path), db_path,
                          db_handle, dpb_length, dpb_buffer)) {
        return -1;
    }
    
    printf("Connection recovered successfully\n");
    return 0;
}
```

### Transaction Recovery

```cpp
int recover_transaction(SB_DB_HANDLE db, SB_TR_HANDLE* tr_handle) {
    SB_STATUS status[20];
    
    // Rollback current transaction if active
    if (*tr_handle) {
        sb_rollback_transaction(status, tr_handle);
        *tr_handle = 0;
    }
    
    // Start new transaction
    if (sb_start_transaction(status, tr_handle, 1, &db,
                           tpb_length, tpb_buffer)) {
        return -1;
    }
    
    return 0;
}
```

## Complete Error Handling Example

```cpp
#include <scratchbird/include/scratchbird.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SB_DB_HANDLE db;
    FILE* error_log;
    int error_count;
    int warning_count;
} APP_CONTEXT;

void log_error(APP_CONTEXT* ctx, SB_STATUS* status, const char* operation) {
    SB_ERROR_INFO error_info;
    sb_get_error_info(status, &error_info);
    
    ctx->error_count++;
    
    fprintf(ctx->error_log, "[ERROR %d] %s failed:\n", ctx->error_count, operation);
    fprintf(ctx->error_log, "  Code: %ld, SQL Code: %ld\n", 
            error_info.error_code, error_info.sql_code);
    fprintf(ctx->error_log, "  State: %s\n", error_info.sql_state);
    fprintf(ctx->error_log, "  Message: %s\n", error_info.error_message);
    fprintf(ctx->error_log, "  Facility: %s\n", error_info.facility);
    
    if (error_info.line_number > 0) {
        fprintf(ctx->error_log, "  Line: %d\n", error_info.line_number);
    }
    
    fprintf(ctx->error_log, "\n");
    fflush(ctx->error_log);
}

int robust_database_operation(APP_CONTEXT* ctx) {
    SB_STATUS status[20];
    SB_TR_HANDLE transaction = 0;
    SB_STMT_HANDLE stmt = 0;
    int result = -1;
    int retry_count = 0;
    const int max_retries = 3;
    
    while (retry_count < max_retries) {
        // Start transaction
        if (sb_start_transaction(status, &transaction, 1, &ctx->db,
                               tpb_length, tpb_buffer)) {
            log_error(ctx, status, "Start Transaction");
            
            // Check if connection is lost
            SB_ERROR_INFO error_info;
            sb_get_error_info(status, &error_info);
            
            if (error_info.error_code == sb_connection_lost) {
                printf("Connection lost, attempting recovery...\n");
                if (recover_connection(&ctx->db) == 0) {
                    retry_count++;
                    continue;
                }
            }
            return -1;
        }
        
        // Allocate statement
        if (sb_dsql_allocate_statement(status, ctx->db, &stmt)) {
            log_error(ctx, status, "Allocate Statement");
            goto rollback;
        }
        
        // Prepare and execute critical operation
        char sql[] = "UPDATE inventory SET quantity = quantity - 1 "
                    "WHERE product_id = ? AND quantity > 0";
        
        if (sb_dsql_prepare(status, transaction, stmt, 0, sql, 4, NULL)) {
            log_error(ctx, status, "Prepare Statement");
            goto cleanup;
        }
        
        // Set up parameters and execute
        if (sb_dsql_execute(status, transaction, stmt, 4, input_sqlda)) {
            SB_ERROR_INFO error_info;
            sb_get_error_info(status, &error_info);
            
            if (error_info.error_code == sb_deadlock) {
                printf("Deadlock detected, retry %d of %d\n", 
                       retry_count + 1, max_retries);
                
                sb_dsql_free_statement(status, &stmt, SB_DROP);
                stmt = 0;
                sb_rollback_transaction(status, &transaction);
                transaction = 0;
                
                // Exponential backoff
                usleep((1 << retry_count) * 100000);
                retry_count++;
                continue;
            } else {
                log_error(ctx, status, "Execute Statement");
                goto cleanup;
            }
        }
        
        // Check warnings
        if (sb_has_warnings(status)) {
            SB_ERROR_INFO warnings[5];
            int warning_count = 0;
            
            sb_get_warnings(status, warnings, 5, &warning_count);
            
            for (int i = 0; i < warning_count; i++) {
                ctx->warning_count++;
                fprintf(ctx->error_log, "[WARNING %d] %s\n",
                        ctx->warning_count, warnings[i].error_message);
            }
        }
        
        // Commit transaction
        if (sb_commit_transaction(status, &transaction)) {
            log_error(ctx, status, "Commit Transaction");
            goto cleanup;
        }
        
        printf("Operation completed successfully\n");
        result = 0;
        break;
        
    cleanup:
        if (stmt) {
            sb_dsql_free_statement(status, &stmt, SB_DROP);
            stmt = 0;
        }
        
    rollback:
        if (transaction) {
            sb_rollback_transaction(status, &transaction);
            transaction = 0;
        }
        
        if (result == 0 || retry_count >= max_retries) {
            break;
        }
        
        retry_count++;
    }
    
    if (result != 0 && retry_count >= max_retries) {
        printf("Operation failed after %d retries\n", max_retries);
    }
    
    return result;
}

int main() {
    APP_CONTEXT ctx = {0};
    
    // Initialize error logging
    ctx.error_log = fopen("application.log", "a");
    if (!ctx.error_log) {
        printf("Failed to open error log\n");
        return 1;
    }
    
    // Connect to database with error handling
    SB_STATUS status[20];
    if (sb_attach_database(status, strlen("localhost:demo.sdb"), 
                          "localhost:demo.sdb", &ctx.db,
                          dpb_length, dpb_buffer)) {
        log_error(&ctx, status, "Database Connection");
        fclose(ctx.error_log);
        return 1;
    }
    
    // Perform database operations
    int result = robust_database_operation(&ctx);
    
    // Cleanup
    sb_detach_database(status, &ctx.db);
    
    printf("Application completed with %d errors and %d warnings\n",
           ctx.error_count, ctx.warning_count);
    
    fclose(ctx.error_log);
    return result;
}
```

## Implementation Files

### Core Error Handling
- `src/common/status.cpp` - Status vector management
- `src/common/StatusHolder.cpp` - Status holder utilities
- `src/common/classes/fb_exception.cpp` - Exception classes
- `src/jrd/err.cpp` - Error message formatting

### Error Message Resources
- `src/msgs/messages.sql` - Error message definitions
- `src/include/gen/msgs.h` - Generated message constants
- `src/common/msg_encode.cpp` - Message encoding utilities

### Debugging Support
- `src/jrd/trace/TraceManager.cpp` - Trace and debug support
- `src/common/utils.cpp` - Debugging utilities
- `src/jrd/stacktrace.cpp` - Stack trace generation

---

*This documentation covers the complete ScratchBird error handling API. See related API documentation for connection management, statement execution, and transaction control.*