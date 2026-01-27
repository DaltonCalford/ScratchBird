# TDS (Tabular Data Stream) Wire Protocol Specification

## Protocol Version: TDS 7.0-7.4 (SQL Server 2000-2019)

## Overview

**Scope Note:** TDS/MSSQL support is Beta scope (UDR connector). There is no
listener/compatibility surface in Alpha.

TDS (Tabular Data Stream) is Microsoft's proprietary protocol for SQL Server and Sybase databases. It operates over TCP/IP on port 1433 by default. The protocol is packet-based with a maximum packet size of 65,536 bytes.

## Packet Structure

### TDS Packet Header

```c
struct TDSPacketHeader {
    uint8_t  type;       // Packet type
    uint8_t  status;     // Status flags
    uint16_t length;     // Total packet length (big-endian)
    uint16_t spid;       // Server Process ID (big-endian)
    uint8_t  packet_id;  // Packet ID (1 for single packet)
    uint8_t  window;     // Window (unused, always 0)
};
```

### Packet Types

```c
enum TDSPacketType {
    TDS_SQL_BATCH       = 0x01,  // SQL batch (query)
    TDS_PRE_LOGIN       = 0x02,  // Pre-login
    TDS_RPC             = 0x03,  // Remote Procedure Call
    TDS_TABULAR_RESULT  = 0x04,  // Tabular result (response)
    TDS_ATTENTION       = 0x06,  // Attention (cancel)
    TDS_BULK_LOAD       = 0x07,  // Bulk load data
    TDS_FEDAUTH_TOKEN   = 0x08,  // Federated authentication token
    TDS_TRANS_MGR_REQ   = 0x0E,  // Transaction manager request
    TDS_LOGIN           = 0x10,  // TDS 5.0 login
    TDS_SSPI            = 0x11,  // SSPI token
    TDS_PRE_LOGIN_7     = 0x12   // Pre-login (TDS 7.0+)
};
```

### Status Flags

```c
#define STATUS_NORMAL           0x00  // Normal packet
#define STATUS_EOM              0x01  // End of message
#define STATUS_IGNORE           0x02  // Ignore this event
#define STATUS_RESET            0x08  // Reset connection
#define STATUS_RESET_KEEPALIVE  0x10  // Reset connection keeping state
```

## Pre-Login Phase

### Pre-Login Packet

The pre-login packet contains option tokens:

```c
struct PreLoginOption {
    uint8_t  token;      // Option token
    uint16_t offset;     // Offset to data (big-endian)
    uint16_t length;     // Data length (big-endian)
};

enum PreLoginToken {
    PL_VERSION      = 0x00,  // Version
    PL_ENCRYPTION   = 0x01,  // Encryption
    PL_INSTOPT      = 0x02,  // Instance
    PL_THREADID     = 0x03,  // Thread ID
    PL_MARS         = 0x04,  // MARS (Multiple Active Result Sets)
    PL_TRACEID      = 0x05,  // Trace ID
    PL_FEDAUTHREQUIRED = 0x06,  // Federated Auth Required
    PL_NONCEOPT     = 0x07,  // Nonce
    PL_TERMINATOR   = 0xFF   // Terminator
};

struct PreLoginPacket {
    struct TDSPacketHeader header;
    
    // Options array
    struct PreLoginOption options[];
    
    // Terminator
    uint8_t terminator = 0xFF;
    
    // Option data follows
    uint8_t data[];
};
```

Example Pre-Login Packet:
```
12 01 00 2F 00 00 00 00  // Header: Type 0x12, Status 0x01, Length 47

// Options
00 00 15 00 06           // VERSION at offset 0x15, length 6
01 00 1B 00 01           // ENCRYPTION at offset 0x1B, length 1
02 00 1C 00 01           // INSTOPT at offset 0x1C, length 1
03 00 1D 00 04           // THREADID at offset 0x1D, length 4
04 00 21 00 01           // MARS at offset 0x21, length 1
FF                       // Terminator

// Data
0E 00 0F A0 00 00        // Version: 14.0.4000.0
02                       // Encryption: ENCRYPT_ON
00                       // Instance: 0
00 00 00 00              // Thread ID: 0
00                       // MARS: disabled
```

### Encryption Options

```c
enum EncryptionOption {
    ENCRYPT_OFF         = 0x00,  // No encryption
    ENCRYPT_ON          = 0x01,  // Encryption required
    ENCRYPT_NOT_SUP     = 0x02,  // Encryption not supported
    ENCRYPT_REQ         = 0x03   // Encryption required
};
```

## Login Phase

### Login7 Packet Structure

```c
struct Login7 {
    uint32_t length;         // Total length
    uint32_t tds_version;    // TDS version
    uint32_t packet_size;    // Packet size
    uint32_t client_prog_ver; // Client program version
    uint32_t client_pid;     // Client process ID
    uint32_t connection_id;  // Connection ID
    
    uint8_t  option_flags1;  // Option flags 1
    uint8_t  option_flags2;  // Option flags 2
    uint8_t  type_flags;     // Type flags
    uint8_t  option_flags3;  // Option flags 3
    
    int32_t  client_timezone; // Client timezone
    uint32_t client_lcid;    // Client LCID
    
    // Variable data offsets and lengths
    uint16_t hostname_offset;
    uint16_t hostname_length;
    uint16_t username_offset;
    uint16_t username_length;
    uint16_t password_offset;
    uint16_t password_length;
    uint16_t appname_offset;
    uint16_t appname_length;
    uint16_t servername_offset;
    uint16_t servername_length;
    uint16_t extension_offset;
    uint16_t extension_length;
    uint16_t interface_offset;
    uint16_t interface_length;
    uint16_t language_offset;
    uint16_t language_length;
    uint16_t database_offset;
    uint16_t database_length;
    
    uint8_t  client_mac[6];   // Client MAC address
    uint16_t sspi_offset;
    uint16_t sspi_length;
    uint16_t atch_db_file_offset;
    uint16_t atch_db_file_length;
    uint16_t change_password_offset;
    uint16_t change_password_length;
    uint32_t sspi_long;       // Long SSPI
    
    // Variable data follows (Unicode strings)
    uint8_t data[];
};
```

### Option Flags

```c
// Option Flags 1
#define BYTE_ORDER_X86      0x00  // Little-endian
#define BYTE_ORDER_68000    0x01  // Big-endian
#define FLOAT_IEEE_754      0x00  // IEEE 754
#define FLOAT_VAX           0x01  // VAX
#define FLOAT_ND5000        0x02  // ND5000
#define DUMPLOAD_ON         0x80  // Dump/load on

// Option Flags 2
#define INTEGRATED_SECURITY 0x80  // Windows authentication
#define ODBC_DRIVER         0x02  // ODBC driver

// Type Flags
#define SQL_TYPE            0x00  // SQL Server
#define OLEDB_TYPE          0x10  // OLE DB

// Option Flags 3
#define CHANGE_PASSWORD     0x01  // Change password
#define SEND_YUKON_BINARY   0x04  // Send binary XML
#define USER_INSTANCE       0x08  // User instance
#define UNKNOWN_COLLATION   0x20  // Unknown collation
#define EXTENSION_USED      0x80  // Extension used
```

### Password Encryption

SQL Server uses a simple XOR-based password obfuscation:

```c
void encrypt_password(const uint16_t* password, int len, uint8_t* output) {
    for (int i = 0; i < len; i++) {
        uint16_t ch = password[i];
        uint8_t high = (ch >> 8) & 0xFF;
        uint8_t low = ch & 0xFF;
        
        // Swap nibbles
        high = ((high & 0x0F) << 4) | ((high & 0xF0) >> 4);
        low = ((low & 0x0F) << 4) | ((low & 0xF0) >> 4);
        
        // XOR with 0xA5
        output[i * 2] = high ^ 0xA5;
        output[i * 2 + 1] = low ^ 0xA5;
    }
}
```

## Token Stream

After login, all communication uses token streams:

### Token Types

```c
enum TokenType {
    // Data Tokens
    TOKEN_ALTMETADATA   = 0x88,  // Alternate metadata
    TOKEN_ALTROW        = 0xD3,  // Alternate row
    TOKEN_COLMETADATA   = 0x81,  // Column metadata
    TOKEN_COLINFO       = 0xA5,  // Column info
    TOKEN_DONE          = 0xFD,  // Done
    TOKEN_DONEPROC      = 0xFE,  // Done procedure
    TOKEN_DONEINPROC    = 0xFF,  // Done in procedure
    TOKEN_ENVCHANGE     = 0xE3,  // Environment change
    TOKEN_ERROR         = 0xAA,  // Error
    TOKEN_FEDAUTHINFO   = 0xEE,  // Federated auth info
    TOKEN_INFO          = 0xAB,  // Info message
    TOKEN_LOGINACK      = 0xAD,  // Login acknowledgment
    TOKEN_NBCROW        = 0xD2,  // NBC row
    TOKEN_OFFSET        = 0x78,  // Offset
    TOKEN_ORDER         = 0xA9,  // Order
    TOKEN_RETURNSTATUS  = 0x79,  // Return status
    TOKEN_RETURNVALUE   = 0xAC,  // Return value
    TOKEN_ROW           = 0xD1,  // Row data
    TOKEN_SSPI          = 0xED,  // SSPI
    TOKEN_TABNAME       = 0xA4,  // Table name
    
    // Done Token Sub-types
    DONE_FINAL          = 0x00,  // Final result
    DONE_MORE           = 0x01,  // More results
    DONE_ERROR          = 0x02,  // Error occurred
    DONE_INXACT         = 0x04,  // In transaction
    DONE_COUNT          = 0x10,  // Row count valid
    DONE_ATTN           = 0x20,  // Attention acknowledged
    DONE_SRVERROR       = 0x100  // Server error
};
```

### LOGINACK Token

```c
struct LoginAckToken {
    uint8_t  token_type = 0xAD;
    uint16_t length;
    uint8_t  interface;      // Interface version
    uint32_t tds_version;    // TDS version
    uint8_t  prog_name_len;  // Program name length
    uint16_t prog_name[];    // Program name (Unicode)
    uint8_t  major_ver;      // Major version
    uint8_t  minor_ver;      // Minor version
    uint16_t build_num;      // Build number
};
```

### ENVCHANGE Token

```c
struct EnvChangeToken {
    uint8_t  token_type = 0xE3;
    uint16_t length;
    uint8_t  type;           // Environment change type
    uint8_t  new_value_len;  // New value length
    uint8_t  new_value[];    // New value
    uint8_t  old_value_len;  // Old value length
    uint8_t  old_value[];    // Old value
};

enum EnvChangeType {
    ENV_DATABASE        = 0x01,  // Database
    ENV_LANGUAGE        = 0x02,  // Language
    ENV_CHARSET         = 0x03,  // Character set
    ENV_PACKETSIZE      = 0x04,  // Packet size
    ENV_UNICODE_SORT    = 0x05,  // Unicode sorting
    ENV_UNICODE_COMP    = 0x06,  // Unicode comparison
    ENV_COLLATION       = 0x07,  // Collation
    ENV_BEGIN_TRANS     = 0x08,  // Begin transaction
    ENV_COMMIT_TRANS    = 0x09,  // Commit transaction
    ENV_ROLLBACK_TRANS  = 0x0A,  // Rollback transaction
    ENV_ENLIST_DTC      = 0x0B,  // Enlist DTC
    ENV_DEFECT_TRANS    = 0x0C,  // Defect transaction
    ENV_MIRROR_PARTNER  = 0x0D,  // Mirror partner
    ENV_RESET_COMPLETE  = 0x12,  // Reset complete
    ENV_USER_INFO       = 0x13,  // User info
    ENV_ROUTING         = 0x14   // Routing info
};
```

## Query Execution

### SQL Batch

```c
struct SQLBatch {
    struct TDSPacketHeader header;  // Type = 0x01
    uint16_t query_text[];          // Unicode query text
};
```

Example SQL Batch:
```
01 01 00 2C 00 00 00 00  // Header: Type 0x01, EOM, Length 44

// Unicode query "SELECT * FROM users"
53 00 45 00 4C 00 45 00 43 00 54 00 20 00 2A 00
20 00 46 00 52 00 4F 00 4D 00 20 00 75 00 73 00
65 00 72 00 73 00
```

### RPC (Remote Procedure Call)

```c
struct RPCRequest {
    struct TDSPacketHeader header;  // Type = 0x03
    
    uint16_t all_headers_length;
    uint32_t total_length;
    
    // Headers
    struct {
        uint32_t length;
        uint16_t type;
        uint8_t  data[];
    } headers[];
    
    // RPC data
    uint16_t proc_name_length;
    uint16_t proc_name[];    // Procedure name or ID
    uint16_t option_flags;
    
    // Parameters follow
};

// Special stored procedure IDs
#define SP_CURSOR           1   // sp_cursor
#define SP_CURSOROPEN       2   // sp_cursoropen
#define SP_CURSORPREPARE    3   // sp_cursorprepare
#define SP_CURSOREXECUTE    4   // sp_cursorexecute
#define SP_CURSORPREPEXEC   5   // sp_cursorprepexec
#define SP_CURSORUNPREPARE  6   // sp_cursorunprepare
#define SP_CURSORFETCH      7   // sp_cursorfetch
#define SP_CURSOROPTION     8   // sp_cursoroption
#define SP_CURSORCLOSE      9   // sp_cursorclose
#define SP_EXECUTESQL       10  // sp_executesql
#define SP_PREPARE          11  // sp_prepare
#define SP_EXECUTE          12  // sp_execute
#define SP_PREPEXEC         13  // sp_prepexec
#define SP_PREPEXECRPC      14  // sp_prepexecrpc
#define SP_UNPREPARE        15  // sp_unprepare
```

### sp_executesql Example

```c
struct SpExecuteSql {
    uint16_t proc_id = 0x000A;  // sp_executesql (10)
    uint16_t flags = 0x0000;
    
    // Parameter 1: SQL statement (NVARCHAR)
    struct {
        uint8_t  name_len = 0;   // No name
        uint8_t  status = 0;     // Input parameter
        uint8_t  type = 0xE7;    // NVARCHAR
        uint16_t max_len;        // Max length
        uint8_t  collation[5];   // Collation info
        uint16_t actual_len;     // Actual length
        uint16_t sql_text[];     // Unicode SQL text
    } sql_param;
    
    // Parameter 2: Parameter definition (optional)
    // Parameter 3+: Parameter values (optional)
};
```

## Result Processing

### COLMETADATA Token

```c
struct ColMetadataToken {
    uint8_t  token_type = 0x81;
    uint16_t column_count;
    
    struct ColumnData {
        uint32_t user_type;      // User-defined type
        uint16_t flags;          // Column flags
        uint8_t  type;           // Data type
        
        // Type-specific metadata
        union {
            // For fixed-length types
            struct {
                uint8_t length;
            } fixed;
            
            // For variable-length types
            struct {
                uint16_t max_length;
                uint8_t collation[5];
            } variable;
            
            // For numeric/decimal
            struct {
                uint8_t length;
                uint8_t precision;
                uint8_t scale;
            } numeric;
        } type_info;
        
        uint8_t  name_length;
        uint16_t name[];         // Column name (Unicode)
    } columns[];
};

// Column flags
#define COL_NULLABLE        0x0001
#define COL_CASE_SENSITIVE  0x0002
#define COL_UPDATEABLE      0x0004
#define COL_UPDATE_UNKNOWN  0x0008
#define COL_IDENTITY        0x0010
#define COL_COMPUTED        0x0020
#define COL_FIXED_LEN_CLR   0x1000
#define COL_SPARSE          0x2000
#define COL_ENCRYPTED       0x4000
#define COL_HIDDEN          0x8000
```

### ROW Token

```c
struct RowToken {
    uint8_t token_type = 0xD1;
    
    // For each column
    union ColumnValue {
        // For fixed-length types
        uint8_t fixed_data[];
        
        // For variable-length types
        struct {
            uint16_t length;     // 0xFFFF for NULL
            uint8_t  data[];
        } variable;
        
        // For text/image types
        struct {
            uint8_t  length;     // Pointer length
            uint8_t  pointer[];  // Text pointer
            uint8_t  timestamp[8]; // Timestamp
            uint32_t data_length; // Actual data length
            uint8_t  data[];
        } text;
    } values[];
};
```

### DONE Token

```c
struct DoneToken {
    uint8_t  token_type = 0xFD;  // DONE, DONEPROC, or DONEINPROC
    uint16_t status;         // Status flags
    uint16_t cur_cmd;        // Current command
    uint64_t row_count;      // Row count (TDS 7.2+: 8 bytes, earlier: 4 bytes)
};
```

## Data Type Encoding

### SQL Server Data Types

```c
enum TDSDataType {
    // Fixed-length types
    NULLTYPE        = 0x1F,  // Null
    INT1TYPE        = 0x30,  // TinyInt (1 byte)
    BITTYPE         = 0x32,  // Bit
    INT2TYPE        = 0x34,  // SmallInt (2 bytes)
    INT4TYPE        = 0x38,  // Int (4 bytes)
    DATETIM4TYPE    = 0x3A,  // SmallDateTime (4 bytes)
    FLT4TYPE        = 0x3B,  // Real (4 bytes)
    MONEYTYPE       = 0x3C,  // Money (8 bytes)
    DATETIMETYPE    = 0x3D,  // DateTime (8 bytes)
    FLT8TYPE        = 0x3E,  // Float (8 bytes)
    MONEY4TYPE      = 0x7A,  // SmallMoney (4 bytes)
    INT8TYPE        = 0x7F,  // BigInt (8 bytes)
    
    // Variable-length types
    GUIDTYPE        = 0x24,  // UniqueIdentifier
    INTNTYPE        = 0x26,  // Int (variable)
    DECIMALTYPE     = 0x37,  // Decimal (legacy)
    NUMERICTYPE     = 0x3F,  // Numeric (legacy)
    BITNTYPE        = 0x68,  // Bit (variable)
    DECIMALNTYPE    = 0x6A,  // Decimal
    NUMERICNTYPE    = 0x6C,  // Numeric
    FLTNTYPE        = 0x6D,  // Float (variable)
    MONEYNTYPE      = 0x6E,  // Money (variable)
    DATETIMNTYPE    = 0x6F,  // DateTime (variable)
    DATENTYPE       = 0x28,  // Date (3 bytes)
    TIMENTYPE       = 0x29,  // Time
    DATETIME2NTYPE  = 0x2A,  // DateTime2
    DATETIMEOFFSETNTYPE = 0x2B, // DateTimeOffset
    CHARTYPE        = 0x2F,  // Char
    VARCHARTYPE     = 0x27,  // VarChar
    BINARYTYPE      = 0x2D,  // Binary
    VARBINARYTYPE   = 0x25,  // VarBinary
    
    // Large types
    BIGVARBINTYPE   = 0xA5,  // VarBinary(MAX)
    BIGVARCHARTYPE  = 0xA7,  // VarChar(MAX)
    BIGBINARYTYPE   = 0xAD,  // Binary(MAX)
    BIGCHARTYPE     = 0xAF,  // Char(MAX)
    NVARCHARTYPE    = 0xE7,  // NVarChar
    NCHARTYPE       = 0xEF,  // NChar
    XMLTYPE         = 0xF1,  // XML
    UDTTYPE         = 0xF0,  // User-defined type
    
    // Text types (deprecated)
    TEXTTYPE        = 0x23,  // Text
    IMAGETYPE       = 0x22,  // Image
    NTEXTTYPE       = 0x63,  // NText
    
    // Special types
    SSVARIANTTYPE   = 0x62   // SQL_Variant
};
```

### Type-Specific Encoding

```c
// Integer types (little-endian)
int8_t   tinyint;
int16_t  smallint;
int32_t  int;
int64_t  bigint;

// Floating point (little-endian)
float    real;
double   float;

// Money (little-endian, scaled by 10000)
struct Money {
    int32_t high;
    uint32_t low;
};

struct SmallMoney {
    int32_t value;
};

// DateTime
struct DateTime {
    int32_t days;    // Days since 1900-01-01
    uint32_t time;   // Time in 1/300 second units
};

struct SmallDateTime {
    uint16_t days;   // Days since 1900-01-01
    uint16_t time;   // Minutes since midnight
};

// Date (3 bytes)
struct Date {
    uint8_t bytes[3];  // Days since 0001-01-01
};

// Time
struct Time {
    uint64_t time;     // Time units based on scale
};

// DateTime2
struct DateTime2 {
    uint64_t time;     // Time units based on scale
    uint8_t date[3];   // Days since 0001-01-01
};

// Decimal/Numeric
struct Decimal {
    uint8_t precision;
    uint8_t scale;
    uint8_t sign;      // 1 = positive, 0 = negative
    uint8_t data[16];  // Up to 16 bytes of data
};
```

## Bulk Copy Protocol (BCP)

### Bulk Load

```c
struct BulkLoadBCP {
    struct TDSPacketHeader header;  // Type = 0x07
    
    // Column metadata
    uint16_t column_count;
    struct BCPColumn {
        uint32_t user_type;
        uint16_t flags;
        uint8_t  type;
        uint8_t  type_info[];    // Type-specific info
        uint8_t  name_length;
        uint16_t name[];
    } columns[];
    
    // Row data follows
};

// BCP row format
struct BCPRow {
    uint8_t text_pointer_flag;  // 0x10 if row has text pointers
    
    // For each column
    union {
        // Fixed-length data
        uint8_t fixed_data[];
        
        // Variable-length data
        struct {
            uint64_t length;     // Length or 0xFFFFFFFFFFFFFFFF for NULL
            uint8_t  data[];
        } variable;
    } columns[];
};
```

## MARS (Multiple Active Result Sets)

MARS allows multiple batches on a single connection:

```c
struct MARSHeader {
    uint16_t header_type = 0x0002;  // MARS header
    uint16_t header_length = 0x0012;
    uint16_t smid;           // Session Multiplexing ID
    uint8_t  sequence;       // Sequence number
    uint8_t  window;         // Window size
    uint64_t ack_seqnum;     // Acknowledgment sequence
};
```

## Attention Signal

To cancel a running query:

```c
struct AttentionPacket {
    struct TDSPacketHeader header;  // Type = 0x06
    // No payload
};
```

## SSL/TLS Encryption

After pre-login negotiation, if encryption is enabled:

1. Client sends TDS packet with TLS Client Hello
2. Server responds with TLS Server Hello
3. TLS handshake completes
4. All subsequent packets are encrypted

## Example: Complete Query Flow

### 1. Pre-Login
```
Client → Server:
12 01 00 2F 00 00 00 00  // Pre-login header
[Pre-login options and data]

Server → Client:
04 01 00 25 00 00 01 00  // Response header
[Server pre-login response]
```

### 2. Login
```
Client → Server:
10 01 01 90 00 00 01 00  // Login7 header
[Login7 packet data]

Server → Client:
04 01 01 7D 00 00 01 00  // Response header
AD [LoginAck token]
E3 [EnvChange tokens]
FD [Done token]
```

### 3. SQL Batch
```
Client → Server:
01 01 00 4E 00 00 01 00  // SQL batch header
53 00 45 00 4C 00 45 00 43 00 54 00 20 00  // "SELECT "
2A 00 20 00 46 00 52 00 4F 00 4D 00 20 00  // "* FROM "
75 00 73 00 65 00 72 00 73 00              // "users"

Server → Client:
04 01 01 A3 00 00 01 00  // Response header
81 [ColMetadata token]
D1 [Row token]
D1 [Row token]
FD [Done token]
```

## Worked Hex Flow (Minimal SQL Auth)

### 1) Pre-Login (Client → Server)
```
12 01 00 2F 00 00 01 00  // Header: PRELOGIN, EOM, length=0x2F
00 00 15 00 06           // VERSION at 0x15, len 6
01 00 1B 00 01           // ENCRYPTION at 0x1B, len 1
02 00 1C 00 01           // INSTOPT at 0x1C, len 1
03 00 1D 00 04           // THREADID at 0x1D, len 4
04 00 21 00 01           // MARS at 0x21, len 1
FF                       // Terminator
0E 00 0F A0 00 00        // Version 14.0.4000.0
00                       // Encryption: ENCRYPT_OFF
00                       // Instance
00 00 00 00              // Thread ID
00                       // MARS: disabled
```

### 2) Pre-Login Response (Server → Client)
```
04 01 00 2F 00 00 01 00  // Header: TABULAR, EOM, length=0x2F
00 00 15 00 06           // VERSION
01 00 1B 00 01           // ENCRYPTION
02 00 1C 00 01           // INSTOPT
03 00 1D 00 04           // THREADID
04 00 21 00 01           // MARS
FF
0E 00 0F A0 00 00        // Version 14.0.4000.0
01                       // Encryption: ENCRYPT_ON (server requires TLS)
00
00 00 00 00
00
```

### 3) Login7 (Client → Server, SQL Auth, empty password)
```
10 01 00 AA 00 00 01 00  // Header: LOGIN7, EOM, length=0x00AA
A2 00 00 00              // Login length = 0x00A2 (162)
04 00 00 74              // TDS version 7.4
00 10 00 00              // Packet size 4096
00 00 00 00              // Client prog version
34 12 00 00              // Client PID
00 00 00 00              // Connection ID
00 00 00 00              // Option flags 1/2/type/3
00 00 00 00              // Client timezone
09 04 00 00              // Client LCID (en-US)
5E 00 06 00              // Hostname offset/len ("client")
6A 00 02 00              // Username offset/len ("sa")
6E 00 00 00              // Password offset/len (empty)
6E 00 08 00              // Appname offset/len ("sbclient")
7E 00 06 00              // Servername offset/len ("server")
8A 00 00 00              // Extension offset/len (empty)
8A 00 06 00              // Interface offset/len ("db-lib")
96 00 00 00              // Language offset/len (empty)
96 00 06 00              // Database offset/len ("master")
00 00 00 00 00 00        // Client MAC
00 00 00 00              // SSPI offset/len
00 00 00 00              // Atch DB offset/len
00 00 00 00              // Change password offset/len
00 00 00 00              // SSPI long
63 00 6C 00 69 00 65 00 6E 00 74 00  // "client"
73 00 61 00              // "sa"
73 00 62 00 63 00 6C 00 69 00 65 00 6E 00 74 00  // "sbclient"
73 00 65 00 72 00 76 00 65 00 72 00  // "server"
64 00 62 00 2D 00 6C 00 69 00 62 00  // "db-lib"
6D 00 61 00 73 00 74 00 65 00 72 00  // "master"
```

Note: non-empty passwords are obfuscated by swapping nibbles and XOR with 0xA5.

### 4) SQL Batch (Client → Server, `SELECT 1`)
```
01 01 00 18 00 00 01 00  // Header: SQL_BATCH, EOM, length=0x0018
53 00 45 00 4C 00 45 00 43 00 54 00 20 00 31 00  // "SELECT 1"
```

### 5) Result Tokens (Server → Client, single int column)
```
04 01 00 25 00 00 01 00  // Header: TABULAR, EOM, length=0x0025
81 01 00 00 00 00 00 38 01 31 00  // COLMETADATA (1 column, INT4, name "1")
D1 01 00 00 00                    // ROW (value = 1)
FD 00 00 00 00 01 00 00 00 00 00 00 00  // DONE (rowcount = 1)
```

## Protocol State Machine

```
    ┌────────────┐
    │Disconnected│
    └─────┬──────┘
          │ Connect
          ▼
    ┌────────────┐
    │ Pre-Login  │
    └─────┬──────┘
          │ TLS (optional)
          ▼
    ┌────────────┐
    │   Login    │
    └─────┬──────┘
          │ LoginAck
          ▼
    ┌────────────┐
    │   Ready    │◄─────────┐
    └─────┬──────┘          │
          │ SQL/RPC          │
          ▼                  │
    ┌────────────┐          │
    │ Executing  │          │
    └─────┬──────┘          │
          │ Done             │
          └─────────────────┘
```

## Implementation Example

### Sending a TDS Packet
```c
void send_tds_packet(int socket, uint8_t type, const void* data, size_t len) {
    struct TDSPacketHeader header = {
        .type = type,
        .status = STATUS_EOM,  // End of message
        .length = htons(sizeof(header) + len),
        .spid = 0,
        .packet_id = 1,
        .window = 0
    };
    
    send(socket, &header, sizeof(header), 0);
    if (len > 0) {
        send(socket, data, len, 0);
    }
}
```

### Reading a TDS Packet
```c
struct tds_packet {
    struct TDSPacketHeader header;
    uint8_t* data;
    size_t data_len;
};

struct tds_packet* read_tds_packet(int socket) {
    struct tds_packet* packet = malloc(sizeof(struct tds_packet));
    
    // Read header
    recv(socket, &packet->header, sizeof(packet->header), MSG_WAITALL);
    
    // Convert from network byte order
    uint16_t total_len = ntohs(packet->header.length);
    packet->data_len = total_len - sizeof(packet->header);
    
    // Read data
    if (packet->data_len > 0) {
        packet->data = malloc(packet->data_len);
        recv(socket, packet->data, packet->data_len, MSG_WAITALL);
    } else {
        packet->data = NULL;
    }
    
    return packet;
}
```

### Processing Token Stream
```c
void process_token_stream(uint8_t* data, size_t len) {
    uint8_t* ptr = data;
    uint8_t* end = data + len;
    
    while (ptr < end) {
        uint8_t token = *ptr++;
        
        switch (token) {
            case TOKEN_COLMETADATA:
                process_colmetadata(&ptr);
                break;
                
            case TOKEN_ROW:
                process_row(&ptr);
                break;
                
            case TOKEN_DONE:
            case TOKEN_DONEPROC:
            case TOKEN_DONEINPROC:
                process_done(&ptr);
                break;
                
            case TOKEN_ERROR:
                process_error(&ptr);
                break;
                
            case TOKEN_INFO:
                process_info(&ptr);
                break;
                
            default:
                // Unknown token
                break;
        }
    }
}
```

## Security Considerations

1. **Use TLS encryption** for all connections
2. **Use Windows Authentication** when possible
3. **Avoid SQL authentication** (password sent obfuscated, not encrypted)
4. **Implement connection pooling** with proper authentication
5. **Use parameterized queries** (sp_executesql)
6. **Validate packet lengths** to prevent buffer overflows
7. **Monitor for injection attempts**
8. **Regular security updates** for TDS libraries

## TDS Versions

- TDS 4.2: SQL Server 6.0
- TDS 5.0: SQL Server 6.5, Sybase 10
- TDS 7.0: SQL Server 7.0
- TDS 7.1: SQL Server 2000
- TDS 7.2: SQL Server 2005
- TDS 7.3: SQL Server 2008
- TDS 7.4: SQL Server 2012-2019

## References

- FreeTDS Documentation
- Microsoft TDS Protocol Documentation (MS-TDS)
- Wireshark TDS Dissector
- SQL Server Network Protocol Documentation
