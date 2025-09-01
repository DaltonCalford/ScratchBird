# MySQL Wire Protocol Specification

## Protocol Version: MySQL 4.1+ (Protocol Version 10)

## Overview

The MySQL wire protocol is a packet-based protocol used for communication between MySQL clients and servers. All data is sent in packets with a maximum size of 16MB.

## Packet Structure

### Basic Packet Format

```
+-------------------+
| 3 bytes | 1 byte  |
+---------+---------+
| Length  | Seq ID  | Payload
+---------+---------+
|      Payload      |
|       ...         |
+-------------------+
```

#### Byte-Level Structure:
```c
struct MySQLPacket {
    uint24_le length;      // 3 bytes: Payload length (little-endian)
    uint8     sequence_id; // 1 byte: Sequence number
    uint8     payload[];   // Variable: Actual data
};
```

### Length Encoding

MySQL uses length-encoded integers (also called packed integers):

```c
// Length Encoded Integer
if (value < 0xFB) {
    // 1 byte: value as-is
    uint8 value;
} else if (value < 0x10000) {
    // 3 bytes: 0xFC followed by 2-byte value
    uint8  marker = 0xFC;
    uint16_le value;
} else if (value < 0x1000000) {
    // 4 bytes: 0xFD followed by 3-byte value
    uint8  marker = 0xFD;
    uint24_le value;
} else {
    // 9 bytes: 0xFE followed by 8-byte value
    uint8  marker = 0xFE;
    uint64_le value;
}
```

## Connection Phase

### 1. Initial Handshake

#### Server → Client: Initial Handshake Packet (Protocol::Handshake)

```c
struct HandshakeV10 {
    uint8  protocol_version;     // Always 10 for MySQL 4.1+
    char   server_version[];     // Null-terminated string
    uint32 connection_id;        // Thread ID
    uint8  auth_plugin_data[8];  // First 8 bytes of auth data
    uint8  filler;              // Always 0x00
    uint16 capability_flags_1;   // Lower 2 bytes of capability flags
    uint8  character_set;        // Server character set
    uint16 status_flags;         // Server status
    uint16 capability_flags_2;   // Upper 2 bytes of capability flags
    uint8  auth_plugin_data_len; // Length of auth data (usually 21)
    uint8  reserved[10];         // All 0x00
    
    // If capabilities & CLIENT_SECURE_CONNECTION
    uint8  auth_plugin_data_2[]; // Rest of auth data (max 13 bytes)
    
    // If capabilities & CLIENT_PLUGIN_AUTH
    char   auth_plugin_name[];   // Null-terminated plugin name
};
```

##### Example Handshake Packet (Hex):
```
4a 00 00 00    // Length: 74, Sequence: 0
0a             // Protocol version 10
35 2e 37 2e 33 31 2d 30 75 62 75 6e 74 75 30 2e // "5.7.31-0ubuntu0."
32 30 2e 30 34 2e 31 00                         // "20.04.1\0"
08 00 00 00    // Connection ID: 8
41 42 43 44 45 46 47 48 00  // Auth data part 1 + filler
ff f7          // Capability flags lower
21             // Character set (utf8_general_ci)
02 00          // Status flags
ff 81          // Capability flags upper
15             // Auth plugin data length: 21
00 00 00 00 00 00 00 00 00 00  // Reserved
49 4a 4b 4c 4d 4e 4f 50 51 52 53 54 00  // Auth data part 2
6d 79 73 71 6c 5f 6e 61 74 69 76 65 5f   // "mysql_native_"
70 61 73 73 77 6f 72 64 00              // "password\0"
```

### 2. Client Authentication

#### Client → Server: Handshake Response (Protocol::HandshakeResponse)

```c
struct HandshakeResponse41 {
    uint32 capability_flags;      // Client capabilities
    uint32 max_packet_size;       // Max packet size
    uint8  character_set;          // Client character set
    uint8  reserved[23];           // All 0x00
    char   username[];             // Null-terminated username
    
    // If capabilities & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
    lenenc auth_response_length;
    uint8  auth_response[];
    // else if capabilities & CLIENT_SECURE_CONNECTION
    uint8  auth_response_length;
    uint8  auth_response[];
    // else
    char   auth_response[];        // Null-terminated
    
    // If capabilities & CLIENT_CONNECT_WITH_DB
    char   database[];             // Null-terminated database name
    
    // If capabilities & CLIENT_PLUGIN_AUTH
    char   auth_plugin_name[];    // Null-terminated plugin name
    
    // If capabilities & CLIENT_CONNECT_ATTRS
    lenenc attrs_length;
    struct {
        lenenc key_length;
        uint8  key[];
        lenenc value_length;
        uint8  value[];
    } attributes[];
};
```

##### Capability Flags (Important ones):
```c
#define CLIENT_LONG_PASSWORD     0x00000001  // Use new auth protocol
#define CLIENT_FOUND_ROWS        0x00000002  // Return found rows
#define CLIENT_LONG_FLAG         0x00000004  // Get all column flags
#define CLIENT_CONNECT_WITH_DB   0x00000008  // Can specify database
#define CLIENT_COMPRESS          0x00000020  // Compression protocol
#define CLIENT_LOCAL_FILES       0x00000080  // Can use LOAD DATA LOCAL
#define CLIENT_PROTOCOL_41       0x00000200  // New 4.1 protocol
#define CLIENT_SSL               0x00000800  // SSL encryption
#define CLIENT_TRANSACTIONS      0x00002000  // Client knows transactions
#define CLIENT_SECURE_CONNECTION 0x00008000  // New 4.1 authentication
#define CLIENT_PLUGIN_AUTH       0x00080000  // Plugin authentication
#define CLIENT_CONNECT_ATTRS     0x00100000  // Connection attributes
#define CLIENT_DEPRECATE_EOF     0x01000000  // No EOF packets
```

### 3. Authentication Methods

#### MySQL Native Password (mysql_native_password)

```c
// Password hashing algorithm:
// SHA1(password) XOR SHA1("20-bytes random data from server" + SHA1(SHA1(password)))

uint8_t* scramble_native_password(const char* password, const uint8_t* salt) {
    uint8_t stage1[20];  // SHA1(password)
    uint8_t stage2[20];  // SHA1(SHA1(password))
    uint8_t result[20];
    
    // Stage 1: SHA1(password)
    SHA1(password, strlen(password), stage1);
    
    // Stage 2: SHA1(stage1)
    SHA1(stage1, 20, stage2);
    
    // Combine with salt: SHA1(salt + stage2)
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, salt, 20);
    SHA1_Update(&ctx, stage2, 20);
    SHA1_Final(result, &ctx);
    
    // XOR with stage1
    for (int i = 0; i < 20; i++) {
        result[i] ^= stage1[i];
    }
    
    return result;
}
```

#### Caching SHA2 Password (caching_sha2_password)

```c
// More secure authentication using SHA256
// Fast auth: SHA256(password) XOR SHA256(SHA256(SHA256(password)) + salt)
// Full auth: Requires SSL or RSA key exchange

struct CachingSha2Response {
    uint8 status;  // 0x03 = fast auth ok, 0x04 = need full auth
    
    // If status == 0x04
    uint8 auth_method;  // 0x02 = request public key, 0x04 = fast auth
};
```

## Command Phase

### Command Packet Structure

```c
enum CommandType {
    COM_SLEEP           = 0x00,  // Internal thread state
    COM_QUIT            = 0x01,  // Close connection
    COM_INIT_DB         = 0x02,  // Select database
    COM_QUERY           = 0x03,  // Text protocol query
    COM_FIELD_LIST      = 0x04,  // Get field list (deprecated)
    COM_CREATE_DB       = 0x05,  // Create database (deprecated)
    COM_DROP_DB         = 0x06,  // Drop database (deprecated)
    COM_REFRESH         = 0x07,  // Refresh
    COM_SHUTDOWN        = 0x08,  // Shutdown server
    COM_STATISTICS      = 0x09,  // Get statistics
    COM_PROCESS_INFO    = 0x0A,  // Get process list
    COM_CONNECT         = 0x0B,  // Internal
    COM_PROCESS_KILL    = 0x0C,  // Kill connection
    COM_DEBUG           = 0x0D,  // Dump debug info
    COM_PING            = 0x0E,  // Ping server
    COM_TIME            = 0x0F,  // Internal
    COM_DELAYED_INSERT  = 0x10,  // Internal
    COM_CHANGE_USER     = 0x11,  // Change user
    COM_BINLOG_DUMP     = 0x12,  // Request binlog
    COM_TABLE_DUMP      = 0x13,  // Internal
    COM_CONNECT_OUT     = 0x14,  // Internal
    COM_REGISTER_SLAVE  = 0x15,  // Register slave
    COM_STMT_PREPARE    = 0x16,  // Prepare statement
    COM_STMT_EXECUTE    = 0x17,  // Execute prepared statement
    COM_STMT_SEND_LONG_DATA = 0x18,  // Send long data
    COM_STMT_CLOSE      = 0x19,  // Close statement
    COM_STMT_RESET      = 0x1A,  // Reset statement
    COM_SET_OPTION      = 0x1B,  // Set options
    COM_STMT_FETCH      = 0x1C,  // Fetch rows
    COM_DAEMON          = 0x1D,  // Internal
    COM_BINLOG_DUMP_GTID = 0x1E, // Binlog dump with GTID
    COM_RESET_CONNECTION = 0x1F  // Reset connection
};

struct CommandPacket {
    uint8  command;      // Command type
    uint8  payload[];    // Command-specific data
};
```

### Text Protocol (COM_QUERY)

#### Query Request
```c
struct ComQuery {
    uint8  command = 0x03;  // COM_QUERY
    char   query[];         // SQL query (not null-terminated)
};
```

#### Query Response

The server responds with one of:
- OK Packet
- Error Packet
- Result Set

##### OK Packet
```c
struct OKPacket {
    uint8  header;          // 0x00 or 0xFE (if < MySQL 5.7.5)
    lenenc affected_rows;   // Number of affected rows
    lenenc last_insert_id;  // Last INSERT id
    
    // If capabilities & CLIENT_PROTOCOL_41
    uint16 status_flags;    // Server status
    uint16 warnings;        // Number of warnings
    
    // If capabilities & CLIENT_SESSION_TRACK
    lenenc info_length;
    char   info[];          // Human readable info
    
    // If status_flags & SERVER_SESSION_STATE_CHANGED
    lenenc session_state_length;
    struct SessionState {
        uint8  type;
        lenenc data_length;
        uint8  data[];
    } session_states[];
};
```

##### Error Packet
```c
struct ErrorPacket {
    uint8  header = 0xFF;   // Error packet marker
    uint16 error_code;      // Error number
    
    // If capabilities & CLIENT_PROTOCOL_41
    char   sql_state_marker = '#';
    char   sql_state[5];    // SQLSTATE value
    
    char   error_message[]; // Human readable error
};
```

##### Result Set

Result sets consist of multiple packets:

1. **Column Count Packet**
```c
struct ColumnCount {
    lenenc column_count;    // Number of columns
};
```

2. **Column Definition Packets** (one per column)
```c
struct ColumnDefinition41 {
    lenenc catalog_length;
    char   catalog[];       // Always "def"
    lenenc schema_length;
    char   schema[];        // Database name
    lenenc table_length;
    char   table[];         // Virtual table name
    lenenc org_table_length;
    char   org_table[];     // Physical table name
    lenenc name_length;
    char   name[];          // Virtual column name
    lenenc org_name_length;
    char   org_name[];      // Physical column name
    lenenc fixed_length = 0x0C;
    uint16 character_set;   // Column character set
    uint32 column_length;   // Maximum length
    uint8  type;           // Field type (see enum_field_types)
    uint16 flags;          // Field flags
    uint8  decimals;       // Decimal places
    uint16 filler = 0x0000; // Reserved
    
    // If command was COM_FIELD_LIST
    lenenc default_length;
    char   default_value[];
};
```

3. **EOF Packet** (if not using CLIENT_DEPRECATE_EOF)
```c
struct EOFPacket {
    uint8  header = 0xFE;   // EOF marker
    
    // If capabilities & CLIENT_PROTOCOL_41
    uint16 warnings;        // Number of warnings
    uint16 status_flags;    // Server status
};
```

4. **Row Data Packets**
```c
struct TextResultRow {
    // Each field is length-encoded string or NULL
    union Field {
        uint8  null_marker = 0xFB;  // NULL value
        lenenc string_length;
        char   string_data[];
    } fields[];
};
```

### Binary Protocol (Prepared Statements)

#### COM_STMT_PREPARE
```c
struct ComStmtPrepare {
    uint8  command = 0x16;  // COM_STMT_PREPARE
    char   query[];         // SQL query with ? placeholders
};

struct ComStmtPrepareOK {
    uint8  status = 0x00;   // OK packet
    uint32 statement_id;    // Statement handle
    uint16 num_columns;     // Number of columns in result
    uint16 num_params;      // Number of parameters
    uint8  filler = 0x00;   // Reserved
    uint16 warning_count;   // Number of warnings
    
    // Followed by:
    // - Parameter definitions (if num_params > 0)
    // - Column definitions (if num_columns > 0)
};
```

#### COM_STMT_EXECUTE
```c
struct ComStmtExecute {
    uint8  command = 0x17;      // COM_STMT_EXECUTE
    uint32 statement_id;         // Statement handle
    uint8  flags;               // Cursor type flags
    uint32 iteration_count = 1; // Always 1
    
    // If num_params > 0
    uint8  null_bitmap[(num_params + 7) / 8];  // NULL bitmap
    uint8  new_params_bound = 1; // Always 1 in 4.1+
    
    // For each parameter (if new_params_bound == 1)
    struct ParamType {
        uint8  type;            // Field type
        uint8  unsigned_flag;   // 0x80 if unsigned
    } param_types[];
    
    // Parameter values (non-NULL parameters only)
    uint8  param_values[];      // Binary encoded based on type
};
```

#### Binary Protocol Value Encoding

```c
// Integer types
int8_t    MYSQL_TYPE_TINY;      // 1 byte
int16_le  MYSQL_TYPE_SHORT;     // 2 bytes
int32_le  MYSQL_TYPE_LONG;      // 4 bytes
int64_le  MYSQL_TYPE_LONGLONG;  // 8 bytes

// Floating point
float32_le MYSQL_TYPE_FLOAT;    // 4 bytes
float64_le MYSQL_TYPE_DOUBLE;   // 8 bytes

// String types (length-encoded)
struct String {
    lenenc length;
    char   data[];
};

// Date/Time types
struct MYSQL_TIME {
    uint8  length;      // 0, 4, 7, or 11
    uint16 year;        // If length >= 4
    uint8  month;       // If length >= 4
    uint8  day;         // If length >= 4
    uint8  hour;        // If length >= 7
    uint8  minute;      // If length >= 7
    uint8  second;      // If length >= 7
    uint32 microsecond; // If length >= 11
};
```

#### Binary Result Set Row
```c
struct BinaryResultRow {
    uint8  header = 0x00;   // Binary row marker
    uint8  null_bitmap[(column_count + 7 + 2) / 8];
    // Followed by non-NULL column values in binary format
    uint8  column_values[];
};
```

## SSL/TLS Negotiation

After receiving the initial handshake, if both client and server support SSL:

1. Client sends SSL Request Packet:
```c
struct SSLRequest {
    uint32 capability_flags;  // Must include CLIENT_SSL
    uint32 max_packet_size;
    uint8  character_set;
    uint8  filler[23];        // All zeros
};
```

2. After sending SSL request, client initiates TLS handshake
3. All subsequent communication is encrypted

## Compression Protocol

When CLIENT_COMPRESS is enabled:

```c
struct CompressedPacket {
    uint24_le compressed_length;   // Length of compressed payload
    uint8     compressed_seq_id;   // Compressed packet sequence
    uint24_le uncompressed_length; // Original length before compression
    uint8     compressed_payload[]; // zlib compressed data
};
```

## Replication Protocol

### COM_BINLOG_DUMP
```c
struct ComBinlogDump {
    uint8  command = 0x12;      // COM_BINLOG_DUMP
    uint32 binlog_pos;          // Start position
    uint16 flags;               // Dump flags
    uint32 server_id;           // Slave server ID
    char   binlog_filename[];   // Binlog file name
};
```

### Binlog Event Structure
```c
struct BinlogEvent {
    uint8  header = 0x00;       // OK packet header
    uint32 timestamp;           // Event timestamp
    uint8  event_type;          // Event type code
    uint32 server_id;           // Server ID
    uint32 event_size;          // Total event size
    uint32 log_pos;             // Position after event
    uint16 flags;               // Event flags
    uint8  event_data[];        // Event-specific data
};
```

## Protocol State Machine

```
    ┌─────────────┐
    │ Disconnected│
    └──────┬──────┘
           │ Connect
           ▼
    ┌─────────────┐
    │  Connecting │
    └──────┬──────┘
           │ Handshake
           ▼
    ┌─────────────┐
    │Authenticating│
    └──────┬──────┘
           │ Auth OK
           ▼
    ┌─────────────┐
    │  Connected  │◄────┐
    └──────┬──────┘     │
           │             │
           ▼             │
    ┌─────────────┐     │
    │   Command   │─────┘
    └─────────────┘   Response
```

## Example: Complete Query Flow

### 1. Client sends COM_QUERY
```
Packet: 21 00 00 00 03 53 45 4C 45 43 54 20 2A 20 46 52 4F 4D 20 75 73 65 72 73
Length: 21 bytes
Sequence: 0
Command: 0x03 (COM_QUERY)
Query: "SELECT * FROM users"
```

### 2. Server responds with Column Count
```
Packet: 01 00 00 01 03
Length: 1 byte
Sequence: 1
Column Count: 3
```

### 3. Server sends Column Definitions
```
Column 1: id (INT)
Column 2: name (VARCHAR)
Column 3: email (VARCHAR)
```

### 4. Server sends EOF (or OK if CLIENT_DEPRECATE_EOF)
```
Packet: 05 00 00 04 FE 00 00 02 00
EOF marker, 0 warnings, status 0x0002
```

### 5. Server sends Row Data
```
Row 1: 01 00 00 05 01 31 04 4A 6F 68 6E 0E 6A 6F 68 6E 40 65 78 61 6D 70 6C 65 2E 63 6F 6D
Fields: "1", "John", "john@example.com"
```

### 6. Server sends Final EOF/OK
```
Packet: 05 00 00 07 FE 00 00 02 00
EOF marker, end of result set
```

## Common Operations Implementation

### Sending a Query
```c
void send_query(int socket, const char* query, uint8_t seq_id) {
    size_t query_len = strlen(query);
    size_t packet_len = 1 + query_len;  // 1 byte for command
    
    uint8_t header[4];
    header[0] = packet_len & 0xFF;
    header[1] = (packet_len >> 8) & 0xFF;
    header[2] = (packet_len >> 16) & 0xFF;
    header[3] = seq_id;
    
    send(socket, header, 4, 0);
    
    uint8_t command = COM_QUERY;
    send(socket, &command, 1, 0);
    send(socket, query, query_len, 0);
}
```

### Reading a Packet
```c
struct mysql_packet* read_packet(int socket) {
    uint8_t header[4];
    recv(socket, header, 4, MSG_WAITALL);
    
    uint32_t length = header[0] | (header[1] << 8) | (header[2] << 16);
    uint8_t seq_id = header[3];
    
    uint8_t* payload = malloc(length);
    recv(socket, payload, length, MSG_WAITALL);
    
    struct mysql_packet* packet = malloc(sizeof(struct mysql_packet) + length);
    packet->length = length;
    packet->sequence_id = seq_id;
    memcpy(packet->payload, payload, length);
    
    free(payload);
    return packet;
}
```

## Security Considerations

1. **Always use SSL/TLS** for production connections
2. **Never send passwords in plain text** 
3. **Implement connection rate limiting**
4. **Validate packet lengths** to prevent buffer overflows
5. **Use prepared statements** to prevent SQL injection
6. **Implement query timeouts**
7. **Monitor for protocol violations**

## Protocol Versions

- Protocol 9: MySQL 3.22 and earlier
- Protocol 10: MySQL 3.23 and later (current)
- X Protocol: MySQL 5.7.12+ (document store, port 33060)

## References

- MySQL Source Code: sql/protocol.cc
- MySQL Internals Manual
- MariaDB Protocol Documentation
- Wireshark MySQL Dissector