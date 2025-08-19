# ScratchBird API - Connection Management

## Overview

The ScratchBird Client API provides comprehensive connection management capabilities through the `libsbclient` library. This documentation covers database connection establishment, configuration, pooling, and lifecycle management.

## Core Connection Interface

### Primary Header Files
```cpp
#include <scratchbird/include/scratchbird.h>
#include <scratchbird/include/sbclient_pub.h>
#include <scratchbird/include/sb_api.h>
```

### Connection Handle Structure

```cpp
typedef struct {
    void* db_handle;
    void* tr_handle;
    char* database_path;
    char* user_name;
    char* password;
    char* role_name;
    char* character_set;
    int dialect;
    int connection_timeout;
    int statement_timeout;
    bool auto_commit;
    SB_CONNECTION_STATE state;
    SB_ERROR_INFO last_error;
} SB_CONNECTION;
```

## Connection Functions

### sb_attach_database()
Establishes a connection to a ScratchBird database.

```cpp
SB_STATUS sb_attach_database(
    SB_STATUS*      status_vector,
    short           db_name_length,
    const char*     db_name,
    SB_DB_HANDLE*   db_handle,
    short           dpb_length,
    const char*     dpb_buffer
);
```

**Parameters:**
- `status_vector`: Error status array
- `db_name_length`: Length of database name string
- `db_name`: Database connection string
- `db_handle`: Output database handle
- `dpb_length`: Database parameter buffer length
- `dpb_buffer`: Database parameter buffer

**Database Connection Strings:**
```cpp
// Local database
"localhost:employee.sdb"

// Remote database with port
"server.company.com/3050:employee.sdb"

// Embedded database
"employee.sdb"

// Service connection
"service_mgr"

// Hierarchical schema connection
"localhost:finance.sdb?schema=accounting.reports"
```

**Example Usage:**
```cpp
SB_STATUS status[20];
SB_DB_HANDLE db = 0;
char dpb_buffer[256];
short dpb_length = 0;

// Build database parameter buffer
sb_expand_dpb(&dpb_buffer, &dpb_length,
    sb_dpb_user_name, "SYSDBA",
    sb_dpb_password, "masterkey",
    sb_dpb_sql_dialect, &dialect,
    sb_dpb_lc_ctype, "UTF8",
    sb_dpb_connect_timeout, &timeout,
    0);

// Connect to database
if (sb_attach_database(status, strlen(db_path), db_path, 
                       &db, dpb_length, dpb_buffer)) {
    // Handle connection error
    sb_print_status(status);
    return;
}

printf("Connected successfully to %s\n", db_path);
```

### sb_detach_database()
Closes a database connection and releases resources.

```cpp
SB_STATUS sb_detach_database(
    SB_STATUS*      status_vector,
    SB_DB_HANDLE*   db_handle
);
```

**Example:**
```cpp
SB_STATUS status[20];

if (sb_detach_database(status, &db)) {
    sb_print_status(status);
} else {
    printf("Database disconnected successfully\n");
}
```

## Database Parameter Buffer (DPB)

### Core DPB Parameters

```cpp
// Authentication parameters
sb_dpb_user_name            // User account name
sb_dpb_password             // User password
sb_dpb_password_enc         // Encrypted password
sb_dpb_sys_user_name        // System user name
sb_dpb_sys_user_name_enc    // Encrypted system user name

// Connection parameters
sb_dpb_sql_dialect          // SQL dialect (1-4)
sb_dpb_lc_ctype             // Character set
sb_dpb_connect_timeout      // Connection timeout (seconds)
sb_dpb_dummy_packet_interval // Keep-alive interval

// Security parameters
sb_dpb_trusted_auth         // Use trusted authentication
sb_dpb_process_id           // Process ID for tracking
sb_dpb_process_name         // Process name for tracking
sb_dpb_role_name           // Database role to assume

// ScratchBird-specific parameters
sb_dpb_schema_name          // Default schema
sb_dpb_home_schema          // User home schema
sb_dpb_compression          // Enable compression
sb_dpb_wire_crypt          // Wire protocol encryption
```

### DPB Builder Helper Function

```cpp
void sb_expand_dpb(char** dpb, short* dpb_length, ...);
```

**Example:**
```cpp
char dpb_buffer[512];
short dpb_length = 0;
int dialect = 4;
int timeout = 30;
bool compression = true;

sb_expand_dpb(&dpb_buffer, &dpb_length,
    sb_dpb_user_name, "FINANCE_USER",
    sb_dpb_password, "secure_password",
    sb_dpb_role_name, "FINANCE_MANAGER", 
    sb_dpb_sql_dialect, &dialect,
    sb_dpb_lc_ctype, "UTF8",
    sb_dpb_connect_timeout, &timeout,
    sb_dpb_schema_name, "finance.accounting",
    sb_dpb_compression, &compression,
    0);  // Terminator
```

## Connection Pool Management

### Pool Configuration Structure

```cpp
typedef struct {
    int min_connections;        // Minimum pool size
    int max_connections;        // Maximum pool size
    int connection_timeout;     // Connection establishment timeout
    int idle_timeout;          // Idle connection timeout
    int cleanup_interval;      // Pool cleanup frequency
    bool validate_connections; // Validate connections on checkout
    char* database_path;       // Database connection string
    char* default_user;        // Default username
    char* default_password;    // Default password
    char* default_role;        // Default role
} SB_POOL_CONFIG;
```

### Pool Management Functions

```cpp
// Create connection pool
SB_STATUS sb_pool_create(
    SB_STATUS*          status_vector,
    SB_POOL_HANDLE*     pool_handle,
    SB_POOL_CONFIG*     config
);

// Get connection from pool
SB_STATUS sb_pool_get_connection(
    SB_STATUS*          status_vector,
    SB_POOL_HANDLE      pool_handle,
    SB_DB_HANDLE*       db_handle,
    int                 timeout_ms
);

// Return connection to pool
SB_STATUS sb_pool_return_connection(
    SB_STATUS*          status_vector,
    SB_POOL_HANDLE      pool_handle,
    SB_DB_HANDLE        db_handle
);

// Destroy connection pool
SB_STATUS sb_pool_destroy(
    SB_STATUS*          status_vector,
    SB_POOL_HANDLE*     pool_handle
);
```

**Pool Usage Example:**
```cpp
SB_STATUS status[20];
SB_POOL_HANDLE pool = 0;
SB_DB_HANDLE db = 0;

// Configure pool
SB_POOL_CONFIG config = {
    .min_connections = 2,
    .max_connections = 10,
    .connection_timeout = 30,
    .idle_timeout = 300,
    .cleanup_interval = 60,
    .validate_connections = true,
    .database_path = "localhost:employee.sdb",
    .default_user = "APP_USER",
    .default_password = "app_password",
    .default_role = "APPLICATION"
};

// Create pool
if (sb_pool_create(status, &pool, &config)) {
    sb_print_status(status);
    return;
}

// Get connection from pool
if (sb_pool_get_connection(status, pool, &db, 5000)) {
    sb_print_status(status);
    return;
}

// Use connection for database operations
// ... perform queries ...

// Return to pool
sb_pool_return_connection(status, pool, db);

// Cleanup
sb_pool_destroy(status, &pool);
```

## Advanced Connection Features

### Connection Events and Callbacks

```cpp
typedef enum {
    SB_EVENT_CONNECT,
    SB_EVENT_DISCONNECT,
    SB_EVENT_TRANSACTION_START,
    SB_EVENT_TRANSACTION_COMMIT,
    SB_EVENT_TRANSACTION_ROLLBACK,
    SB_EVENT_ERROR
} SB_CONNECTION_EVENT;

typedef void (*SB_EVENT_CALLBACK)(
    SB_DB_HANDLE        db_handle,
    SB_CONNECTION_EVENT event,
    void*               user_data
);

// Register event callback
SB_STATUS sb_register_event_callback(
    SB_STATUS*          status_vector,
    SB_DB_HANDLE        db_handle,
    SB_EVENT_CALLBACK   callback,
    void*               user_data
);
```

### Connection Information

```cpp
// Get connection information
SB_STATUS sb_database_info(
    SB_STATUS*      status_vector,
    SB_DB_HANDLE    db_handle,
    short           item_list_length,
    const char*     item_list,
    short           buffer_length,
    char*           buffer
);
```

**Information Items:**
```cpp
sb_info_db_id               // Database ID string
sb_info_reads               // Page reads count
sb_info_writes              // Page writes count
sb_info_fetches             // Record fetches count
sb_info_marks               // Record marks count
sb_info_implementation      // Implementation string
sb_info_version             // Version string
sb_info_base_level          // ODS base level
sb_info_page_size           // Database page size
sb_info_num_buffers         // Buffer pool size
sb_info_limbo               // Limbo transaction count
sb_info_current_memory      // Current memory usage
sb_info_max_memory          // Maximum memory usage
sb_info_allocation          // Page allocation count
sb_info_attachment_id       // Attachment ID
sb_info_read_seq_count      // Sequential reads
sb_info_read_idx_count      // Index reads
sb_info_insert_count        // Insert operations
sb_info_update_count        // Update operations
sb_info_delete_count        // Delete operations
sb_info_backout_count       // Backout operations
sb_info_purge_count         // Purge operations
sb_info_expunge_count       // Expunge operations
```

**Example:**
```cpp
SB_STATUS status[20];
char info_buffer[1024];
char request[] = {
    sb_info_db_id, 
    sb_info_implementation,
    sb_info_version,
    sb_info_page_size,
    sb_info_end
};

if (sb_database_info(status, db, sizeof(request), request,
                     sizeof(info_buffer), info_buffer)) {
    sb_print_status(status);
} else {
    // Parse info buffer
    sb_parse_database_info(info_buffer, sizeof(info_buffer));
}
```

## Error Handling

### Connection Error Codes

```cpp
// Connection-specific error codes
#define sb_connection_lost          335544721L
#define sb_connection_rejected      335544722L
#define sb_connection_timeout       335544723L
#define sb_connection_broken        335544724L
#define sb_network_error           335544725L
#define sb_login_error             335544726L
#define sb_database_unavailable    335544727L
#define sb_server_misconfigured    335544728L
#define sb_network_protocol_error  335544729L
#define sb_connection_pool_full    335544730L
```

### Error Information Structure

```cpp
typedef struct {
    long        error_code;
    long        sql_code;
    char        error_message[512];
    char        sql_state[6];
    int         line_number;
    char        routine_name[64];
} SB_ERROR_INFO;

// Get detailed error information
void sb_get_error_info(
    SB_STATUS*      status_vector,
    SB_ERROR_INFO*  error_info
);
```

## Connection Security

### SSL/TLS Configuration

```cpp
// SSL DPB parameters
sb_dpb_wire_crypt           // Enable wire encryption
sb_dpb_certificate_file     // Client certificate file
sb_dpb_private_key_file     // Private key file
sb_dpb_ca_file             // Certificate authority file
sb_dpb_cipher_suite        // SSL cipher suite
sb_dpb_ssl_verify_mode     // Certificate verification mode
```

**SSL Example:**
```cpp
char dpb_buffer[512];
short dpb_length = 0;
bool wire_crypt = true;

sb_expand_dpb(&dpb_buffer, &dpb_length,
    sb_dpb_user_name, "SECURE_USER",
    sb_dpb_password, "secure_password",
    sb_dpb_wire_crypt, &wire_crypt,
    sb_dpb_certificate_file, "/path/to/client.crt",
    sb_dpb_private_key_file, "/path/to/client.key",
    sb_dpb_ca_file, "/path/to/ca.crt",
    0);
```

## Platform-Specific Considerations

### Windows Named Pipes
```cpp
// Named pipe connection
"\\\\server\\pipe\\scratchbird\\database.sdb"
```

### Linux/Unix Domain Sockets
```cpp
// Unix socket connection
"/var/run/scratchbird/database.sdb"
```

### Embedded Mode
```cpp
// Embedded database (no server)
"/path/to/database.sdb"
```

## Performance Optimization

### Connection Pooling Best Practices

1. **Pool Sizing**
   - Set `min_connections` to handle base load
   - Set `max_connections` based on system resources
   - Monitor pool utilization metrics

2. **Timeout Configuration**
   - `connection_timeout`: 30-60 seconds for WAN, 5-10 for LAN
   - `idle_timeout`: 300-600 seconds to balance resources
   - `cleanup_interval`: 60-120 seconds for optimal performance

3. **Connection Validation**
   - Enable `validate_connections` for critical applications
   - Use lightweight validation queries
   - Monitor validation failure rates

### Memory Management

```cpp
// Configure memory allocation
sb_dpb_num_buffers          // Buffer pool size
sb_dpb_dbkey_scope          // Database key scope
sb_dpb_no_garbage_collect   // Disable garbage collection
sb_dpb_gc_policy           // Garbage collection policy
```

## Migration from Firebird

### API Compatibility Layer

Most Firebird client applications can be migrated by:

1. **Header Changes**
   ```cpp
   // Replace
   #include <ibase.h>
   
   // With
   #include <scratchbird/include/scratchbird.h>
   ```

2. **Function Prefix Changes**
   ```cpp
   // Replace isc_ with sb_
   isc_attach_database()  →  sb_attach_database()
   isc_detach_database()  →  sb_detach_database()
   isc_database_info()    →  sb_database_info()
   ```

3. **Library Linking**
   ```bash
   # Replace
   -lfbclient
   
   # With
   -lsbclient
   ```

## Complete Connection Example

```cpp
#include <scratchbird/include/scratchbird.h>
#include <stdio.h>
#include <string.h>

int main() {
    SB_STATUS status[20];
    SB_DB_HANDLE db = 0;
    char dpb_buffer[256];
    short dpb_length = 0;
    int dialect = 4;
    int timeout = 30;
    
    // Build connection parameters
    sb_expand_dpb(&dpb_buffer, &dpb_length,
        sb_dpb_user_name, "DEMO_USER",
        sb_dpb_password, "demo_password",
        sb_dpb_sql_dialect, &dialect,
        sb_dpb_lc_ctype, "UTF8",
        sb_dpb_connect_timeout, &timeout,
        sb_dpb_schema_name, "demo.application",
        0);
    
    // Connect to database
    if (sb_attach_database(status, 
                          strlen("localhost:demo.sdb"), 
                          "localhost:demo.sdb",
                          &db, dpb_length, dpb_buffer)) {
        printf("Connection failed:\n");
        sb_print_status(status);
        return 1;
    }
    
    printf("Connected successfully to demo database\n");
    
    // Get database information
    char info_buffer[256];
    char request[] = {sb_info_version, sb_info_page_size, sb_info_end};
    
    if (!sb_database_info(status, db, sizeof(request), request,
                         sizeof(info_buffer), info_buffer)) {
        printf("Database version and page size retrieved\n");
    }
    
    // Disconnect
    if (sb_detach_database(status, &db)) {
        printf("Disconnect failed:\n");
        sb_print_status(status);
    } else {
        printf("Disconnected successfully\n");
    }
    
    return 0;
}
```

## Implementation Files

### Core Connection Management
- `src/remote/client.cpp` - Remote client connection handling
- `src/remote/inet.cpp` - TCP/IP protocol implementation  
- `src/remote/wnet.cpp` - Named pipes (Windows)
- `src/remote/xnet.cpp` - Local protocol (Windows)
- `src/common/classes/ClumpletWriter.cpp` - DPB building
- `src/common/isc_sync.cpp` - Connection synchronization

### Connection Pool Implementation
- `src/common/classes/fb_queue.h` - Connection queue management
- `src/jrd/Attachment.cpp` - Database attachment management
- `src/common/ThreadStart.cpp` - Connection threading

### Security and Authentication
- `src/auth/SecureRemotePassword.cpp` - SRP authentication
- `src/auth/trusted.cpp` - Trusted authentication
- `src/plugins/crypt.cpp` - Wire protocol encryption

---

*This documentation covers the complete ScratchBird connection management API. See related API documentation for statement execution, transaction management, and error handling.*