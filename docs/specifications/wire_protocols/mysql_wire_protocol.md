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

### Packet Framing and Sequencing

- The maximum payload length per packet is `0xFFFFFF` (16,777,215 bytes).
- If a logical payload exceeds this size, it is split across multiple packets:
  - Each packet has `length == 0xFFFFFF`, except the final packet which is `< 0xFFFFFF`.
  - The receiver must concatenate payloads in order to reconstruct the logical message.
- `sequence_id` starts at `0` for the first packet of each command phase (including the initial handshake) and increments by 1 for each subsequent packet in that phase (wrapping at 255).


### Length Encoding

MySQL uses length-encoded integers (also called packed integers):

```c
// Length Encoded Integer
if (value < 0xFB) {
    // 1 byte: value as-is
    uint8 value;
} else if (value == 0xFB) {
    // NULL (only valid for length-encoded values)
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
    uint8  auth_plugin_data_len; // Total auth data length (0 if no CLIENT_PLUGIN_AUTH)
    uint8  reserved[10];         // All 0x00
    
    // If capabilities & CLIENT_SECURE_CONNECTION
    uint8  auth_plugin_data_2[]; // Rest of auth data (len = max(12, auth_plugin_data_len - 9))
    
    // If capabilities & CLIENT_PLUGIN_AUTH
    char   auth_plugin_name[];   // Null-terminated plugin name

    // MariaDB-specific: if CLIENT_MYSQL is not set, a 3rd 32-bit capabilities
    // word may appear after the 10-byte reserved block.
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

##### Example Handshake Flow (Client Response + OK)

Client HandshakeResponse41 (user=root, plugin=mysql_native_password, password=secret):
```
50 00 00 01  // packet header (length=0x50, seq=1)
00 82 08 00  // capability flags (CLIENT_PROTOCOL_41|CLIENT_SECURE_CONNECTION|CLIENT_PLUGIN_AUTH)
00 00 00 01  // max packet size
21           // character set
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  // reserved
72 6f 6f 74 00  // "root\0"
14 28 44 15 90 67 42 85 e7 d0 3c ae 7a f2 37 50 47 97 f7 0e 91  // auth response (20 bytes)
6d 79 73 71 6c 5f 6e 61 74 69 76 65 5f 70 61 73 73 77 6f 72 64 00  // "mysql_native_password\0"
```

Server OK packet:
```
07 00 00 02 00 00 00 02 00 00
```

##### Worked Hex Flow (COM_QUERY SELECT 1)

Client → Server: COM_QUERY (`SELECT 1`)
```
09 00 00 00  // length=9, seq=0
03 53 45 4C 45 43 54 20 31  // COM_QUERY + "SELECT 1"
```

Server → Client: Column count (1)
```
01 00 00 01  // length=1, seq=1
01           // column count
```

Server → Client: Column definition (single INT column)
```
1C 00 00 02  // length=28, seq=2
03 64 65 66              // catalog = "def"
04 74 65 73 74           // schema = "test"
00                       // table = ""
00                       // org_table = ""
01 31                    // name = "1"
01 31                    // org_name = "1"
0C                       // length of fixed fields
21 00                    // character set = utf8_general_ci
01 00 00 00              // column length
03                       // type = LONG
00 00                    // flags
00                       // decimals
00 00                    // filler
```

Server → Client: EOF (end of column definitions)
```
05 00 00 03  // length=5, seq=3
FE 00 00 02 00
```

Server → Client: Row data ("1")
```
02 00 00 04  // length=2, seq=4
01 31        // len=1, value="1"
```

Server → Client: EOF (end of rows)
```
05 00 00 05  // length=5, seq=5
FE 00 00 02 00
```

### 2. Client Authentication

#### Client → Server: Handshake Response (Protocol::HandshakeResponse)

If `CLIENT_SSL` is set in the response capabilities, the client **first** sends an `SSLRequest` packet (same layout as the first part of `HandshakeResponse41`, without username/auth/database/plugin/attributes), performs the TLS handshake, then sends the full `HandshakeResponse41` over the encrypted channel.

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

##### Capability Flags (Full Matrix)
```c
#define CLIENT_LONG_PASSWORD             0x00000001
#define CLIENT_FOUND_ROWS                0x00000002
#define CLIENT_LONG_FLAG                 0x00000004
#define CLIENT_CONNECT_WITH_DB           0x00000008
#define CLIENT_NO_SCHEMA                 0x00000010
#define CLIENT_COMPRESS                  0x00000020
#define CLIENT_ODBC                      0x00000040
#define CLIENT_LOCAL_FILES               0x00000080
#define CLIENT_IGNORE_SPACE              0x00000100
#define CLIENT_PROTOCOL_41               0x00000200
#define CLIENT_INTERACTIVE               0x00000400
#define CLIENT_SSL                       0x00000800
#define CLIENT_IGNORE_SIGPIPE            0x00001000
#define CLIENT_TRANSACTIONS              0x00002000
#define CLIENT_RESERVED                  0x00004000
#define CLIENT_SECURE_CONNECTION         0x00008000
#define CLIENT_MULTI_STATEMENTS          0x00010000
#define CLIENT_MULTI_RESULTS             0x00020000
#define CLIENT_PS_MULTI_RESULTS          0x00040000
#define CLIENT_PLUGIN_AUTH               0x00080000
#define CLIENT_CONNECT_ATTRS             0x00100000
#define CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA 0x00200000
#define CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS   0x00400000
#define CLIENT_SESSION_TRACK             0x00800000
#define CLIENT_DEPRECATE_EOF             0x01000000
#define CLIENT_OPTIONAL_RESULTSET_METADATA    0x02000000
#define CLIENT_ZSTD_COMPRESSION          0x04000000
#define CLIENT_QUERY_ATTRIBUTES          0x08000000
#define CLIENT_MULTI_FACTOR_AUTH         0x10000000
```

Server behavior rules:
- The server must echo back the negotiated capability flags in OK packets and
  during `HandshakeResponse41` handling.
- If `CLIENT_MULTI_RESULTS` or `CLIENT_PS_MULTI_RESULTS` is set, the server
  must set `SERVER_MORE_RESULTS_EXISTS` in status flags when more result sets
  follow.
- If `CLIENT_SESSION_TRACK` is set, OK packets include session state tracking.

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

Caching SHA-256 authentication is plugin-driven and may require multiple rounds:

**Initial auth response (in HandshakeResponse41 or AuthSwitchResponse):**
```
scramble = XOR(SHA256(password),
               SHA256(seed || SHA256(SHA256(password))))
```

**Server fast-auth result (AuthMoreData):**
- 0x03 = success, authentication complete
- 0x04 = continue full authentication

**Full auth when `0x04` returned:**
- If the connection is encrypted (TLS), client sends cleartext password (null-terminated).
- Otherwise:
  1) Client sends `0x02` to request server RSA public key.
  2) Server responds with `0x01` + public key bytes.
  3) Client sends RSA-encrypted password (`RSA_PKCS1_OAEP_PADDING`) over `XOR(password, seed)`.

#### SHA256 Password (sha256_password)

Flow is the same as full auth above, except the public key request byte is `0x01`.

#### Authentication Switch / More Data Packets

```c
struct AuthSwitchRequest {
    uint8 header = 0xFE;
    char  plugin_name[];  // Null-terminated
    uint8 plugin_data[];  // EOF-length
};

struct AuthMoreData {
    uint8 header = 0x01;
    uint8 data[];         // EOF-length
};
```

If the server sends plugin data starting with `0x00`, `0xFE`, or `0xFF`, it may prepend `0x01` to escape it. Clients must ignore the optional leading `0x01` when present.

Client response to `AuthSwitchRequest` is the raw auth response data for the named plugin (payload only, length-encoded by the packet header).

AuthMoreData payloads for `caching_sha2_password`:
- `0x03` = fast auth success (server will send OK next)
- `0x04` = full auth required
- `0x01` + PEM bytes = server public key (after client request)

Client full-auth steps when `0x04` received:
- If TLS is active, send cleartext password (null-terminated).
- Otherwise send `0x02` (public key request), receive key, then send RSA-OAEP
  encrypted password (XOR with seed as required by plugin).

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

### COM_CHANGE_USER
```c
struct ComChangeUser {
    uint8  command = 0x11;
    char   user[];                // Null-terminated
    lenenc auth_response_length;  // If CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
    uint8  auth_response[];       // Auth response
    char   database[];            // Null-terminated (optional)
    char   auth_plugin_name[];    // Null-terminated (optional)
    lenenc attrs_length;          // Optional attributes
    uint8  attrs[];               // Key/value attributes
};
```

Behavior:
- Resets session state (autocommit, temp tables, user variables) and re-authenticates.
- Server may respond with `AuthSwitchRequest` / `AuthMoreData` during re-auth.
- Capabilities are preserved; the new user inherits capability limits.

### COM_RESET_CONNECTION
```c
struct ComResetConnection {
    uint8 command = 0x1F;
};
```

Behavior:
- Resets session state to defaults while keeping the authenticated user.
- Cleans up open prepared statements, temp tables, and session variables.
- Returns OK or ERR; does not renegotiate capabilities.

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
- Local Infile Request (0xFB) for `LOAD DATA LOCAL INFILE`

When the server sends `0xFB` as the first byte of the payload, the packet is a Local Infile Request. The client must send the file contents as raw data packets, then a zero-length packet to terminate.

Local Infile Request payload:
```c
uint8  header = 0xFB;
char   filename[];  // Null-terminated path or identifier
```

Rules:
- Requires `CLIENT_LOCAL_FILES` capability on both sides.
- Client streams file data in packet payloads (no command byte) and ends with
  a zero-length packet.
- Server replies with OK or ERR after ingesting the stream.

##### OK Packet
```c
struct OKPacket {
    uint8  header;          // 0x00, or 0xFE if CLIENT_DEPRECATE_EOF is set
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

##### Server Status Flags (Common)
```c
#define SERVER_STATUS_IN_TRANS             0x0001  // Transaction active
#define SERVER_STATUS_AUTOCOMMIT           0x0002  // Autocommit enabled
#define SERVER_MORE_RESULTS_EXISTS         0x0008  // More result sets follow
#define SERVER_STATUS_NO_GOOD_INDEX_USED   0x0010
#define SERVER_STATUS_NO_INDEX_USED        0x0020
#define SERVER_STATUS_CURSOR_EXISTS        0x0040
#define SERVER_STATUS_LAST_ROW_SENT        0x0080
#define SERVER_STATUS_DB_DROPPED           0x0100
#define SERVER_STATUS_NO_BACKSLASH_ESCAPES 0x0200
#define SERVER_STATUS_METADATA_CHANGED     0x0400
#define SERVER_SESSION_STATE_CHANGED       0x4000
```

##### Session State Tracking (CLIENT_SESSION_TRACK)
When `CLIENT_SESSION_TRACK` is enabled and `SERVER_SESSION_STATE_CHANGED` is set,
the OK packet includes session state items:

```c
// Session state types
#define SESSION_TRACK_SYSTEM_VARIABLES 0x00
#define SESSION_TRACK_SCHEMA           0x01
#define SESSION_TRACK_STATE_CHANGE     0x02
#define SESSION_TRACK_GTIDS            0x03
```

Each item contains type-specific payload:
- `SYSTEM_VARIABLES`: key/value pairs for updated session variables.
- `SCHEMA`: the current default schema name.
- `STATE_CHANGE`: a single byte (0x00 or 0x01).
- `GTIDS`: GTID set string.

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

EOF packets are only used when `CLIENT_DEPRECATE_EOF` is **not** set. An EOF packet is identified by `header == 0xFE` **and** payload length `< 9` bytes (to disambiguate from OK packets that may also use 0xFE).

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

#### Multi-Result Sets

When `CLIENT_MULTI_RESULTS` (text protocol) or `CLIENT_PS_MULTI_RESULTS`
(binary protocol) is enabled, the server may return multiple result sets:
- Each result set uses the standard framing: column count, column definitions,
  EOF/OK terminator (depending on `CLIENT_DEPRECATE_EOF`), then row packets,
  and a final EOF/OK for that result set.
- The `SERVER_MORE_RESULTS_EXISTS` flag is set in the status flags of the
  terminating EOF/OK packet when additional result sets follow.
- The final result set terminator clears `SERVER_MORE_RESULTS_EXISTS`.

For `CLIENT_MULTI_STATEMENTS`, a single `COM_QUERY` may produce multiple
result sets in sequence.

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
    // - EOF/OK terminators depending on CLIENT_DEPRECATE_EOF
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

#### COM_STMT_SEND_LONG_DATA
```c
struct ComStmtSendLongData {
    uint8  command = 0x18;   // COM_STMT_SEND_LONG_DATA
    uint32 statement_id;
    uint16 param_id;         // 0-based parameter index
    uint8  data[];           // Raw chunk
};
```

Semantics:
- Allows streaming large parameter values in multiple chunks.
- Chunks are appended in order for the parameter until COM_STMT_EXECUTE.
- Data is cleared after execution or COM_STMT_RESET.

#### COM_STMT_FETCH
```c
struct ComStmtFetch {
    uint8  command = 0x1C;   // COM_STMT_FETCH
    uint32 statement_id;
    uint32 num_rows;         // Requested rows
};
```

Semantics:
- Requires server-side cursor (COM_STMT_EXECUTE flags).
- Server returns up to `num_rows` rows using the binary protocol.
- Uses `SERVER_STATUS_CURSOR_EXISTS` and `SERVER_STATUS_LAST_ROW_SENT` to
  signal cursor state.

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
    uint8  length;      // 0, 4 (date), 7 (datetime), or 11 (datetime with micros)
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
    uint8  null_bitmap[(column_count + 7 + 2) / 8]; // 2-bit offset, then 1 bit per column
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

The SSLRequest is the same as the initial fixed-length portion of `HandshakeResponse41` (no username/auth/database/plugin/attrs). It uses the next `sequence_id` after the server handshake (typically 1).

2. After sending SSL request, client initiates TLS handshake
3. Client then sends the full `HandshakeResponse41` over TLS (sequence_id continues)
4. All subsequent communication is encrypted

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

If `uncompressed_length` is 0, the payload is uncompressed and can be treated as normal packets.

Compression edge cases:
- Large payloads may be split into multiple compressed packets; each compressed
  packet contains one or more full MySQL packets after decompression.
- `compressed_seq_id` is independent of the inner packet sequence ids.
- If decompression fails, the server should return an error and close the
  connection (no resynchronization is possible).

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

### COM_BINLOG_DUMP_GTID
```c
struct ComBinlogDumpGtid {
    uint8  command = 0x1E;      // COM_BINLOG_DUMP_GTID
    uint16 flags;
    uint32 server_id;
    uint32 binlog_filename_length;
    char   binlog_filename[];
    uint64 binlog_pos;
    uint32 data_size;           // Length of following GTID set
    uint8  gtid_set[];          // Encoded GTID set
};
```

Alpha note: replication is deferred; these commands may return a clear error
until replication support is enabled.

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
