# Firebird Wire Protocol Specification

## Protocol Version: 10-13 (Firebird 1.0-4.0)

## Overview

Firebird uses a packet-based protocol with XDR (External Data Representation) encoding for platform independence. The protocol operates over TCP/IP on port 3050 by default.

## XDR Encoding

Firebird uses XDR (RFC 1832) for data serialization:

```c
// XDR Basic Types
int32_t   xdr_long;      // 4 bytes, big-endian
uint32_t  xdr_u_long;    // 4 bytes, big-endian
int16_t   xdr_short;     // 2 bytes, big-endian (padded to 4)
uint16_t  xdr_u_short;   // 2 bytes, big-endian (padded to 4)
char      xdr_char;      // 1 byte (padded to 4)

// XDR String
struct xdr_string {
    uint32_t length;     // String length
    char     data[];     // String data (padded to 4-byte boundary)
    char     padding[];  // 0-3 bytes of padding
};

// XDR Opaque (byte array)
struct xdr_opaque {
    uint32_t length;     // Data length
    uint8_t  data[];     // Raw bytes (padded to 4-byte boundary)
    uint8_t  padding[];  // 0-3 bytes of padding
};
```

### XDR Padding Rule
```c
#define XDR_ALIGN(n) (((n) + 3) & ~3)  // Round up to 4-byte boundary

size_t xdr_string_size(const char* str) {
    size_t len = strlen(str);
    return 4 + XDR_ALIGN(len);  // 4 bytes for length + padded string
}
```

## Packet Structure

### Basic Packet Format

```c
struct WirePacket {
    uint32_t operation;   // Operation code (big-endian)
    uint32_t length;      // Payload length (not including header)
    uint8_t  data[];      // Operation-specific data
};
```

## Operation Codes

```c
enum WireOp {
    op_connect           = 1,    // Connect to database
    op_exit              = 2,    // Disconnect
    op_accept            = 3,    // Accept connection
    op_reject            = 4,    // Reject connection
    op_protocol          = 5,    // Protocol handshake
    op_disconnect        = 6,    // Disconnect notification
    op_credit            = 7,    // Credit check
    op_continuation      = 8,    // Continuation packet
    op_response          = 9,    // Generic response
    
    // Database operations
    op_attach            = 19,   // Attach database
    op_create            = 20,   // Create database
    op_detach            = 21,   // Detach database
    op_compile           = 22,   // Compile request
    op_start             = 23,   // Start request
    op_start_and_send    = 24,   // Start and send
    op_send              = 25,   // Send data
    op_receive           = 26,   // Receive data
    op_unwind            = 27,   // Unwind request
    op_release           = 28,   // Release object
    
    // Transaction operations
    op_transaction       = 29,   // Start transaction
    op_commit            = 30,   // Commit transaction
    op_rollback          = 31,   // Rollback transaction
    op_prepare           = 32,   // Prepare transaction
    op_reconnect         = 33,   // Reconnect to transaction
    
    // Blob operations
    op_create_blob       = 34,   // Create blob
    op_open_blob         = 35,   // Open blob
    op_get_segment       = 36,   // Get blob segment
    op_put_segment       = 37,   // Put blob segment
    op_cancel_blob       = 38,   // Cancel blob
    op_close_blob        = 39,   // Close blob
    
    // Info operations
    op_info_database     = 40,   // Database info
    op_info_request      = 41,   // Request info
    op_info_transaction  = 42,   // Transaction info
    op_info_blob         = 43,   // Blob info
    
    // Batch operations
    op_batch_segments    = 44,   // Batch segments
    op_mgr_set_affinity  = 45,   // Manager set affinity
    op_mgr_clear_affinity = 46,  // Manager clear affinity
    op_mgr_report        = 47,   // Manager report
    
    // SQL operations
    op_que_events        = 48,   // Queue events
    op_cancel_events     = 49,   // Cancel events
    op_commit_retaining  = 50,   // Commit retaining
    op_prepare2          = 51,   // Prepare (protocol 11)
    op_event             = 52,   // Event notification
    op_connect_request   = 53,   // Connection request
    op_aux_connect       = 54,   // Auxiliary connection
    op_ddl               = 55,   // DDL statement
    op_open_blob2        = 56,   // Open blob (v2)
    op_create_blob2      = 57,   // Create blob (v2)
    op_get_slice         = 58,   // Get array slice
    op_put_slice         = 59,   // Put array slice
    op_slice             = 60,   // Array slice response
    op_seek_blob         = 61,   // Seek in blob
    
    // Statement operations
    op_allocate_statement = 62,  // Allocate statement
    op_execute           = 63,   // Execute statement
    op_exec_immediate    = 64,   // Execute immediate
    op_fetch             = 65,   // Fetch rows
    op_fetch_response    = 66,   // Fetch response
    op_free_statement    = 67,   // Free statement
    op_prepare_statement = 68,   // Prepare statement
    op_set_cursor        = 69,   // Set cursor name
    op_info_sql          = 70,   // SQL info
    
    // Extended operations
    op_dummy             = 71,   // Dummy packet
    op_response_piggyback = 72,  // Piggyback response
    op_start_and_receive = 73,   // Start and receive
    op_start_send_and_receive = 74, // Start, send and receive
    op_exec_immediate2   = 75,   // Execute immediate (v2)
    op_execute2          = 76,   // Execute (v2)
    op_insert            = 77,   // Insert operation
    op_sql_response      = 78,   // SQL response
    op_transact          = 79,   // Transaction operation
    op_transact_response = 80,   // Transaction response
    op_drop_database     = 81,   // Drop database
    op_service_attach    = 82,   // Attach service
    op_service_detach    = 83,   // Detach service
    op_service_info      = 84,   // Service info
    op_service_start     = 85,   // Start service
    op_rollback_retaining = 86,  // Rollback retaining
    
    // Update operations
    op_update_account_info = 87, // Update account info
    op_authenticate_user = 88,   // Authenticate user
    op_partial           = 89,   // Partial packet
    op_trusted_auth      = 90,   // Trusted authentication
    op_cancel            = 91,   // Cancel operation
    op_cont_auth         = 92,   // Continue authentication
    op_ping              = 93,   // Ping
    op_accept_data       = 94,   // Accept data
    op_abort_aux_connection = 95, // Abort auxiliary connection
    op_crypt             = 96,   // Encryption
    op_crypt_key_callback = 97,  // Encryption key callback
    op_cond_accept       = 98    // Conditional accept
};
```

## Connection Phase

### 1. Initial Connect

```c
struct op_connect_packet {
    uint32_t op_code = op_connect;  // 1
    uint32_t op_version;             // Protocol version
    uint32_t op_architecture;        // Client architecture
    xdr_string op_file_name;         // Database path
    uint32_t op_count;               // User data count
    xdr_opaque op_user_id;           // User identification
};

// Protocol versions
#define PROTOCOL_VERSION10   10
#define PROTOCOL_VERSION11   11
#define PROTOCOL_VERSION12   12
#define PROTOCOL_VERSION13   13

// Architecture types
#define ARCH_GENERIC         1  // Generic
#define ARCH_INTEL_X86       30 // Intel x86
#define ARCH_AMD_X64         31 // AMD x64
```

Example Connect Packet:
```
00 00 00 01        // op_connect
00 00 00 0D        // Protocol version 13
00 00 00 1E        // Architecture (x86)
00 00 00 0C        // Database path length (12)
2F 74 6D 70 2F 74 65 73 74 2E 66 64 62  // "/tmp/test.fdb"
00 00              // Padding to 4-byte boundary
00 00 00 02        // User data count

// User data block
00 00 00 01        // CNCT_user (user name)
00 00 00 08        // Length
53 59 53 44 42 41 00 00  // "SYSDBA" + padding

00 00 00 04        // CNCT_host (host name)
00 00 00 09        // Length
6C 6F 63 61 6C 68 6F 73 74 00 00 00  // "localhost" + padding
```

### 2. Accept Response

```c
struct op_accept_packet {
    uint32_t op_code = op_accept;   // 3
    uint32_t op_version;             // Accepted protocol version
    uint32_t op_architecture;        // Server architecture
    uint32_t op_type;                // Accept type
};

struct op_accept_data {
    uint32_t p_acpt_version;        // Protocol version
    uint32_t p_acpt_architecture;    // Architecture
    uint32_t p_acpt_type;           // Database type
    
    // Authentication data
    uint32_t p_acpt_authenticated;  // Authentication status
    xdr_string p_acpt_plugin_name;  // Auth plugin name
    xdr_opaque p_acpt_auth_data;    // Auth data
    uint32_t p_acpt_auth_block_size; // Auth block size
};
```

### 3. Database Attachment

```c
struct op_attach_packet {
    uint32_t op_code = op_attach;   // 19
    uint32_t op_object;              // Database object ID (0 for new)
    xdr_string op_file_name;         // Database path
    uint32_t op_dpb_length;          // DPB length
    uint8_t op_dpb[];                // Database Parameter Block
};

// Database Parameter Block (DPB) items
#define isc_dpb_version1         1
#define isc_dpb_user_name        28
#define isc_dpb_password         29
#define isc_dpb_password_enc     30
#define isc_dpb_role_name        60
#define isc_dpb_sql_dialect      63
#define isc_dpb_charset          68
#define isc_dpb_session_time_zone 116
#define isc_dpb_auth_plugin_name 119
#define isc_dpb_auth_plugin_list 120
```

Example DPB Construction:
```c
uint8_t* build_dpb(const char* user, const char* password) {
    uint8_t* dpb = malloc(256);
    uint8_t* p = dpb;
    
    *p++ = isc_dpb_version1;        // Version
    
    // User name
    *p++ = isc_dpb_user_name;
    *p++ = strlen(user);
    memcpy(p, user, strlen(user));
    p += strlen(user);
    
    // Password
    *p++ = isc_dpb_password;
    *p++ = strlen(password);
    memcpy(p, password, strlen(password));
    p += strlen(password);
    
    // SQL dialect
    *p++ = isc_dpb_sql_dialect;
    *p++ = 1;
    *p++ = 3;  // Dialect 3
    
    return dpb;
}
```

## Authentication

### SRP (Secure Remote Password) Authentication

Firebird 3.0+ uses SRP for secure authentication:

```c
struct srp_client_public {
    xdr_opaque salt;        // Server salt
    xdr_opaque verifier;    // SRP verifier
};

struct srp_server_public {
    xdr_opaque public_key;  // Server public key (B)
    xdr_opaque salt;        // Salt
};

// SRP flow:
// 1. Client sends username
// 2. Server sends salt and B
// 3. Client calculates A and M1, sends both
// 4. Server verifies M1, sends M2
// 5. Client verifies M2
```

### Legacy Authentication

```c
struct legacy_auth {
    xdr_string user_name;
    xdr_string encrypted_password;
};

// Password encryption (legacy)
void encrypt_password(const char* password, const char* key, char* result) {
    // ENC_crypt algorithm (DES-based)
    for (int i = 0; i < strlen(password); i++) {
        result[i] = password[i] ^ key[i % strlen(key)];
    }
}
```

## Statement Execution

### 1. Allocate Statement

```c
struct op_allocate_statement_packet {
    uint32_t op_code = op_allocate_statement;  // 62
    uint32_t op_database;    // Database handle
};

// Response
struct op_response {
    uint32_t op_code = op_response;  // 9
    uint32_t op_handle;      // Statement handle
    uint64_t op_object_id;   // Object ID
    uint32_t op_length;      // Buffer length
    uint8_t op_buffer[];     // Response data
    
    // Status vector
    uint32_t op_status_vector[];  // ISC status codes
};
```

### 2. Prepare Statement

```c
struct op_prepare_statement_packet {
    uint32_t op_code = op_prepare_statement;  // 68
    uint32_t op_transaction; // Transaction handle
    uint32_t op_statement;   // Statement handle
    uint32_t op_dialect;     // SQL dialect
    xdr_string op_query;     // SQL query
    uint32_t op_buffer_length; // Describe buffer length
    uint8_t op_buffer[];     // Describe items
};

// Describe items
#define isc_info_sql_stmt_type    21
#define isc_info_sql_get_plan     22
#define isc_info_sql_records      23
#define isc_info_sql_batch_fetch  24

// Statement types
#define isc_info_sql_stmt_select         1
#define isc_info_sql_stmt_insert         2
#define isc_info_sql_stmt_update         3
#define isc_info_sql_stmt_delete         4
#define isc_info_sql_stmt_ddl            5
#define isc_info_sql_stmt_exec_procedure 8
```

### 3. Execute Statement

```c
struct op_execute_packet {
    uint32_t op_code = op_execute;  // 63
    uint32_t op_statement;   // Statement handle
    uint32_t op_transaction; // Transaction handle
    
    // Input parameters
    uint32_t op_format;      // Message format
    uint32_t op_length;      // Parameters length
    uint8_t op_parameters[]; // Parameter data
};

struct op_execute2_packet {
    uint32_t op_code = op_execute2;  // 76
    uint32_t op_statement;
    uint32_t op_transaction;
    
    // Input parameters
    uint32_t op_in_format;
    uint32_t op_in_length;
    uint8_t op_in_parameters[];
    
    // Output parameters
    uint32_t op_out_format;
    uint32_t op_out_length;
};
```

### 4. Fetch Rows

```c
struct op_fetch_packet {
    uint32_t op_code = op_fetch;  // 65
    uint32_t op_statement;   // Statement handle
    uint32_t op_format;      // Message format
    uint32_t op_count;       // Number of rows to fetch
};

struct op_fetch_response {
    uint32_t op_code = op_fetch_response;  // 66
    uint32_t op_status;      // 0 = data, 100 = no more data
    uint32_t op_count;       // Number of rows
    
    // For each row
    struct fetch_row {
        uint32_t length;     // Row data length
        uint8_t data[];      // Row data (formatted)
    } rows[];
};
```

## Data Representation

### SQL Data Type Encoding

```c
// Firebird SQL types
#define SQL_TEXT         452   // CHAR
#define SQL_VARYING      448   // VARCHAR
#define SQL_SHORT        500   // SMALLINT
#define SQL_LONG         496   // INTEGER
#define SQL_FLOAT        482   // FLOAT
#define SQL_DOUBLE       480   // DOUBLE
#define SQL_D_FLOAT      530   // DOUBLE PRECISION
#define SQL_TIMESTAMP    510   // TIMESTAMP
#define SQL_BLOB         520   // BLOB
#define SQL_ARRAY        540   // ARRAY
#define SQL_QUAD         550   // QUAD (internal)
#define SQL_TYPE_TIME    560   // TIME
#define SQL_TYPE_DATE    570   // DATE
#define SQL_INT64        580   // BIGINT
#define SQL_BOOLEAN      32764 // BOOLEAN (FB 3.0+)
#define SQL_NULL         32766 // NULL

// Nullable flag
#define SQL_NULLABLE     1

// Check if nullable
#define IS_NULLABLE(type) ((type) & SQL_NULLABLE)
#define GET_DTYPE(type)   ((type) & ~SQL_NULLABLE)
```

### Message Format Descriptor

```c
struct xsqlda {  // eXtended SQL Descriptor Area
    int16_t version;     // Version (SQLDA_VERSION1)
    int16_t sqln;        // Number of fields allocated
    int16_t sqld;        // Actual number of fields
    
    struct xsqlvar {
        int16_t sqltype;     // Data type + nullable flag
        int16_t sqlscale;    // Scale (for NUMERIC/DECIMAL)
        int16_t sqllen;      // Data length
        uint8_t* sqldata;    // Pointer to data
        int16_t* sqlind;     // Null indicator
        int16_t sqlname_length;
        char sqlname[32];    // Field name
        int16_t relname_length;
        char relname[32];    // Relation name
        int16_t ownname_length;
        char ownname[32];    // Owner name
        int16_t aliasname_length;
        char aliasname[32];  // Alias name
    } sqlvar[];
};
```

### Binary Data Encoding

```c
// Integer types (network byte order)
int16_t encode_smallint(int16_t value) {
    return htons(value);
}

int32_t encode_integer(int32_t value) {
    return htonl(value);
}

int64_t encode_bigint(int64_t value) {
    return htobe64(value);
}

// Floating point (IEEE 754, big-endian)
float encode_float(float value) {
    uint32_t* p = (uint32_t*)&value;
    *p = htonl(*p);
    return value;
}

double encode_double(double value) {
    uint64_t* p = (uint64_t*)&value;
    *p = htobe64(*p);
    return value;
}

// Timestamp (8 bytes)
struct fb_timestamp {
    int32_t date;  // Days since November 17, 1858
    uint32_t time; // Time in 100 microseconds since midnight
};

// Date encoding
int32_t encode_date(int year, int month, int day) {
    // Convert to Modified Julian Date
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    
    int jd = day + (153 * m + 2) / 5 + 365 * y + 
             y / 4 - y / 100 + y / 400 - 32045;
    
    return jd - 2400001;  // Convert to Firebird epoch
}
```

## BLOB Operations

### Create BLOB

```c
struct op_create_blob_packet {
    uint32_t op_code = op_create_blob;  // 34
    uint32_t op_transaction;  // Transaction handle
};

struct op_create_blob2_packet {
    uint32_t op_code = op_create_blob2;  // 57
    uint32_t op_bpb_length;   // BPB length
    uint8_t op_bpb[];         // Blob Parameter Block
    uint32_t op_transaction;
};

// Response includes blob_id
struct blob_id {
    uint32_t bid_high;
    uint32_t bid_low;
};
```

### Put Segment

```c
struct op_put_segment_packet {
    uint32_t op_code = op_put_segment;  // 37
    uint32_t op_blob;         // Blob handle
    uint32_t op_length;       // Segment length
    uint8_t op_segment[];     // Segment data
};
```

### Get Segment

```c
struct op_get_segment_packet {
    uint32_t op_code = op_get_segment;  // 36
    uint32_t op_blob;         // Blob handle
    uint32_t op_length;       // Max segment length
    uint32_t op_segment;      // Segment number (0 = next)
};
```

## Transaction Management

### Start Transaction

```c
struct op_transaction_packet {
    uint32_t op_code = op_transaction;  // 29
    uint32_t op_database;     // Database handle
    uint32_t op_tpb_length;   // TPB length
    uint8_t op_tpb[];         // Transaction Parameter Block
};

// Transaction Parameter Block items
#define isc_tpb_version1         1
#define isc_tpb_version3         3
#define isc_tpb_consistency      1   // Table-level locking
#define isc_tpb_concurrency      2   // Snapshot isolation
#define isc_tpb_shared           3   // Shared locks
#define isc_tpb_protected        4   // Protected locks
#define isc_tpb_exclusive        5   // Exclusive locks
#define isc_tpb_wait             6   // Wait on locks
#define isc_tpb_nowait           7   // No wait on locks
#define isc_tpb_read             8   // Read-only
#define isc_tpb_write            9   // Read-write
#define isc_tpb_lock_read        10  // Lock for read
#define isc_tpb_lock_write       11  // Lock for write
#define isc_tpb_verb_time        12  // Verb time
#define isc_tpb_commit_time      13  // Commit time
#define isc_tpb_ignore_limbo     14  // Ignore limbo
#define isc_tpb_read_committed   15  // Read committed
#define isc_tpb_autocommit       16  // Auto-commit
#define isc_tpb_rec_version      17  // Record version
#define isc_tpb_no_rec_version   18  // No record version
#define isc_tpb_restart_requests 19  // Restart requests
#define isc_tpb_no_auto_undo     20  // No auto undo
```

Example TPB:
```c
uint8_t* build_tpb() {
    uint8_t tpb[] = {
        isc_tpb_version3,
        isc_tpb_write,
        isc_tpb_read_committed,
        isc_tpb_rec_version,
        isc_tpb_nowait
    };
    return tpb;
}
```

### Commit/Rollback

```c
struct op_commit_packet {
    uint32_t op_code = op_commit;  // 30
    uint32_t op_transaction;  // Transaction handle
};

struct op_rollback_packet {
    uint32_t op_code = op_rollback;  // 31
    uint32_t op_transaction;
};

struct op_commit_retaining_packet {
    uint32_t op_code = op_commit_retaining;  // 50
    uint32_t op_transaction;
};
```

## Event Notification

```c
struct op_que_events_packet {
    uint32_t op_code = op_que_events;  // 48
    uint32_t op_database;     // Database handle
    uint32_t op_events_length; // EPB length
    uint8_t op_events[];      // Event Parameter Block
    uint32_t op_ast;          // AST routine address
    uint32_t op_arg;          // AST argument
    uint32_t op_event_id;     // Local event ID
};

struct op_event_packet {
    uint32_t op_code = op_event;  // 52
    uint32_t op_database;
    uint32_t op_events_length;
    uint8_t op_events[];      // Event counts
    uint32_t op_ast;
    uint32_t op_arg;
    uint32_t op_event_id;
};
```

## Status Vector

```c
// Status vector format
struct status_vector {
    uint32_t isc_arg;     // Argument type
    uint32_t isc_code;    // Error/warning code
    // ... repeated pairs ...
    uint32_t isc_arg_end; // 0 = end of vector
};

// Argument types
#define isc_arg_end          0  // End of arguments
#define isc_arg_gds          1  // Error code
#define isc_arg_string       2  // String argument
#define isc_arg_cstring      3  // C string
#define isc_arg_number       4  // Numeric argument
#define isc_arg_interpreted  5  // Interpreted status
#define isc_arg_vms          6  // VMS status
#define isc_arg_unix         7  // Unix error
#define isc_arg_domain       8  // Domain error
#define isc_arg_dos          9  // DOS error
#define isc_arg_mpexl        10 // MPE/XL error
#define isc_arg_mpexl_ipc    11 // MPE/XL IPC error
#define isc_arg_next_mach    15 // NeXT/Mach error
#define isc_arg_netware      16 // NetWare error
#define isc_arg_win32        17 // Win32 error
#define isc_arg_warning      18 // Warning
#define isc_arg_sql_state    19 // SQLSTATE
```

## Protocol State Machine

```
    ┌────────────┐
    │Disconnected│
    └─────┬──────┘
          │ op_connect
          ▼
    ┌────────────┐
    │ Connecting │
    └─────┬──────┘
          │ op_accept
          ▼
    ┌────────────┐
    │  Connected │
    └─────┬──────┘
          │ op_attach
          ▼
    ┌────────────┐
    │  Attached  │◄─────────┐
    └─────┬──────┘          │
          │                  │
          ▼                  │
    ┌────────────┐          │
    │Transaction │          │
    └─────┬──────┘          │
          │                  │
          ▼                  │
    ┌────────────┐          │
    │ Statement  │──────────┘
    └────────────┘    op_response
```

## Wire Compression

Firebird supports zlib compression:

```c
struct compressed_packet {
    uint32_t uncompressed_length;
    uint32_t compressed_length;
    uint8_t compressed_data[];  // zlib compressed
};

// Enable compression in DPB
#define isc_dpb_wire_compression 126
#define isc_dpb_wire_compression_level 127
```

## Example: Complete Query Execution

### 1. Allocate Statement
```
Client → Server:
00 00 00 3E        // op_allocate_statement (62)
00 00 00 01        // Database handle

Server → Client:
00 00 00 09        // op_response
00 00 00 01        // Statement handle
00 00 00 00 00 00 00 00  // Object ID
00 00 00 00        // Buffer length
00 00 00 00        // Status: success
```

### 2. Prepare Statement
```
Client → Server:
00 00 00 44        // op_prepare_statement (68)
00 00 00 01        // Transaction handle
00 00 00 01        // Statement handle
00 00 00 03        // SQL dialect 3
00 00 00 13        // Query length
53 45 4C 45 43 54 20 2A 20 46 52 4F 4D 20 75 73 65 72 73 00  // "SELECT * FROM users"
00                 // Padding
00 00 00 00        // Buffer length
```

### 3. Execute Statement
```
Client → Server:
00 00 00 3F        // op_execute (63)
00 00 00 01        // Statement handle
00 00 00 01        // Transaction handle
00 00 00 00        // Format
00 00 00 00        // Parameters length
```

### 4. Fetch Rows
```
Client → Server:
00 00 00 41        // op_fetch (65)
00 00 00 01        // Statement handle
00 00 00 00        // Format
00 00 00 64        // Fetch 100 rows

Server → Client:
00 00 00 42        // op_fetch_response (66)
00 00 00 00        // Status: data available
00 00 00 03        // 3 rows returned
// Row data follows...
```

## Security Considerations

1. **Use SRP authentication** instead of legacy methods
2. **Enable wire encryption** (Firebird 3.0+)
3. **Use wire compression** to reduce attack surface
4. **Validate packet lengths** to prevent buffer overflows
5. **Implement connection limits** per user
6. **Use prepared statements** to prevent SQL injection
7. **Monitor for protocol violations**
8. **Regular security updates**

## Protocol Versions

- Protocol 10: Firebird 1.0-1.5 (InterBase 6.0)
- Protocol 11: Firebird 2.0-2.5
- Protocol 12: Firebird 3.0
- Protocol 13: Firebird 4.0

## References

- Firebird Source: src/remote/protocol.h
- Firebird Wire Protocol Documentation
- XDR Specification (RFC 1832)
- Wireshark Firebird Dissector