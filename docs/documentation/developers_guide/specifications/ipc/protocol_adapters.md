# Specification: Protocol Adapters

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | protocol/adapters |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 1.0.0-alpha1 |
| **Authors** | ScratchBird Engineering |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/protocol/adapters/postgresql_adapter.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/protocol/adapters/mysql_adapter.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/protocol/adapters/firebird_adapter.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/protocol/adapters/protocol_adapter.cpp`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/adapters/postgresql_adapter.h`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/adapters/mysql_adapter.h`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/adapters/firebird_adapter.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_*.cpp`

## Synopsis

This specification defines the protocol adapters that enable ScratchBird to emulate PostgreSQL, MySQL, and Firebird wire protocols. Each adapter translates native database client requests into ScratchBird IPC messages and converts engine responses back to the emulated protocol format. This allows existing database clients to connect to ScratchBird without modification.

## Scope

### In Scope

- PostgreSQL v3 wire protocol adapter
- MySQL wire protocol adapter (4.1+)
- Firebird wire protocol adapter
- Protocol state machines
- Message format translations
- Authentication method mappings
- Error code translations

### Out of Scope

- SQL dialect parsing (see parser specifications)
- SBLR generation (see SBLR specifications)
- IPC message handling (see parser_agent_contract.md)
- TLS/SSL implementation details (see security specifications)

## Background

Protocol adapters sit between the network layer and the parser agent, implementing the wire protocol surface of emulated databases. They maintain protocol-specific state while delegating SQL execution to the engine via IPC.

Key responsibilities:
1. **Wire Format Parsing**: Decode protocol-specific message formats
2. **SQL Translation**: Pass SQL text to the engine's parser
3. **Result Encoding**: Convert engine results to protocol-specific formats
4. **State Management**: Maintain prepared statements, portals, and transactions
5. **Error Translation**: Map engine errors to protocol-specific error codes

## Specification

### Data Structures

#### ProtocolAdapter Base Class

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/adapters/protocol_adapter.h

class ProtocolAdapter {
public:
    explicit ProtocolAdapter(const ProtocolAdapterConfig& config);
    virtual ~ProtocolAdapter();
    
    // Core interface
    virtual network::ProtocolType getProtocolType() const = 0;
    virtual core::Status parseMessage(network::Connection* conn) = 0;
    virtual core::Status processMessage(network::Connection* conn) = 0;
    virtual core::Status sendGreeting(network::Connection* conn) = 0;
    virtual core::Status processAuthentication(network::Connection* conn) = 0;
    virtual core::Status sendQueryResult(network::Connection* conn, 
                                         const ResultContext& result) = 0;
    
protected:
    ProtocolAdapterConfig config_;
    std::string database_name_;
    std::string username_;
    std::string remote_password_;
    std::unique_ptr<client::Connection> client_;
};
```

#### ProtocolAdapterConfig Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/adapters/protocol_adapter.h

struct ProtocolAdapterConfig {
    std::string default_database;
    std::string engine_endpoint;
    uint32_t read_timeout_ms = 30000;
    uint32_t write_timeout_ms = 30000;
    bool enable_compression = false;
    bool enforce_bound_database = true;
    // ... additional fields
};
```

#### ResultContext Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/adapters/protocol_adapter.h

struct ResultContext {
    std::vector<ProtocolCodec::ColumnInfo> columns;
    std::vector<std::vector<ProtocolCodec::ColumnValue>> rows;
    uint64_t rows_affected = 0;
    std::string command_tag;
    bool has_error = false;
    uint32_t error_code = 0;
    std::string error_message;
    std::vector<std::string> notices;
};
```

### PostgreSQL Adapter

#### PostgreSQL Protocol Constants

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/adapters/postgresql_adapter.h:43

namespace pg {
    constexpr int32_t PROTOCOL_VERSION_3 = 196608;  // (3 << 16)
    constexpr int32_t SSL_REQUEST = 80877103;
    constexpr int32_t GSSENC_REQUEST = 80877104;
    constexpr int32_t CANCEL_REQUEST = 80877102;
    
    // Frontend message types
    namespace FrontendMsg {
        constexpr char BIND = 'B';
        constexpr char CLOSE = 'C';
        constexpr char COPY_DATA = 'd';
        constexpr char COPY_DONE = 'c';
        constexpr char COPY_FAIL = 'f';
        constexpr char DESCRIBE = 'D';
        constexpr char EXECUTE = 'E';
        constexpr char PARSE = 'P';
        constexpr char QUERY = 'Q';
        constexpr char SYNC = 'S';
        constexpr char TERMINATE = 'X';
    }
    
    // Backend message types
    namespace BackendMsg {
        constexpr char AUTHENTICATION = 'R';
        constexpr char BACKEND_KEY_DATA = 'K';
        constexpr char BIND_COMPLETE = '2';
        constexpr char CLOSE_COMPLETE = '3';
        constexpr char COMMAND_COMPLETE = 'C';
        constexpr char COPY_DATA = 'd';
        constexpr char DATA_ROW = 'D';
        constexpr char ERROR_RESPONSE = 'E';
        constexpr char PARAMETER_STATUS = 'S';
        constexpr char PARSE_COMPLETE = '1';
        constexpr char READY_FOR_QUERY = 'Z';
        constexpr char ROW_DESCRIPTION = 'T';
    }
    
    // Authentication types
    namespace AuthType {
        constexpr int32_t OK = 0;
        constexpr int32_t CLEARTEXT_PASSWORD = 3;
        constexpr int32_t MD5_PASSWORD = 5;
        constexpr int32_t SASL = 10;
    }
}
```

#### PostgreSQL State Machine

```
┌─────────────────────────────────────────────────────────────────┐
│                    PostgreSQL Protocol States                    │
└─────────────────────────────────────────────────────────────────┘

                         ┌─────────────┐
                    ┌────┤   STARTUP   │
                    │    └──────┬──────┘
                    │           │ Startup message received
                    │           ▼
                    │    ┌─────────────┐     SSL request
                    │    │ AUTH_REQUEST├────────────────┐
                    │    └──────┬──────┘                │
                    │           │ Password message      │
                    │           ▼                       │
                    │    ┌─────────────┐              ┌─┴──────────┐
                    │    │AUTH_MD5/    │              │   SSL      │
                    │    │AUTH_SCRAM   │              │  REQUEST   │
                    │    └──────┬──────┘              └─────┬──────┘
                    │           │ Auth success              │
                    │           ▼                           │
                    │    ┌─────────────┐              ┌─────┴──────┐
                    └────┤ AUTHENTICATED              │ TLS HANDSHAKE
                         └──────┬──────┘              └────────────┘
                                │
                                ▼
                         ┌─────────────┐
                         │    READY    │◄──────────────┐
                         └──────┬──────┘               │
                                │ Query/Execute        │
              ┌─────────────────┼─────────────────┐    │
              ▼                 ▼                 ▼    │
        ┌──────────┐     ┌──────────┐      ┌──────────┐│
        │SIMPLE_   │     │EXTENDED_ │      │   COPY   ││
        │QUERY     │     │QUERY     │      │          ││
        └────┬─────┘     └────┬─────┘      └────┬─────┘│
             │                │                 │      │
             └────────────────┴─────────────────┘      │
                                │                      │
                                ▼                      │
                         ┌─────────────┐    Complete  │
                         │  EXECUTING  │──────────────┘
                         └──────┬──────┘
                                │ Error
                                ▼
                         ┌─────────────┐
                         │    ERROR    │
                         └──────┬──────┘
                                │ Terminate
                                ▼
                         ┌─────────────┐
                         │   CLOSING   │
                         └─────────────┘
```

#### PostgreSQL Message Formats

**Startup Message:**
```
Length (4 bytes) + Protocol Version (4 bytes) + Parameters (null-terminated pairs)
```

**Regular Message:**
```
Type (1 byte) + Length (4 bytes, includes self) + Payload (Length - 4 bytes)
```

**Authentication Messages:**
```
Type: 'R'
Length: 4 + payload
Payload:
  - Auth type (4 bytes): 0=OK, 3=Cleartext, 5=MD5, 10=SASL
  - For MD5: 4-byte salt
  - For SASL: Mechanism name(s)
```

### MySQL Adapter

#### MySQL Protocol Constants

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/adapters/mysql_adapter.cpp

namespace mysql {
    // Capabilities flags
    enum class Capability : uint32_t {
        LONG_PASSWORD     = 0x00000001,
        FOUND_ROWS        = 0x00000002,
        LONG_FLAG         = 0x00000004,
        CONNECT_WITH_DB   = 0x00000008,
        NO_SCHEMA         = 0x00000010,
        COMPRESS          = 0x00000020,
        ODBC              = 0x00000040,
        LOCAL_FILES       = 0x00000080,
        IGNORE_SPACE      = 0x00000100,
        PROTOCOL_41       = 0x00000200,
        INTERACTIVE       = 0x00000400,
        SSL               = 0x00000800,
        IGNORE_SIGPIPE    = 0x00001000,
        TRANSACTIONS      = 0x00002000,
        RESERVED          = 0x00004000,
        SECURE_CONNECTION = 0x00008000,
        MULTI_STATEMENTS  = 0x00010000,
        MULTI_RESULTS     = 0x00020000,
        PLUGIN_AUTH       = 0x00080000,
        CONNECT_ATTRS     = 0x00100000,
        PLUGIN_AUTH_LENENC= 0x00200000,
        SESSION_TRACK     = 0x00800000,
        DEPRECATE_EOF     = 0x01000000,
    };
    
    // Command types
    constexpr uint8_t COM_QUIT = 0x01;
    constexpr uint8_t COM_INIT_DB = 0x02;
    constexpr uint8_t COM_QUERY = 0x03;
    constexpr uint8_t COM_FIELD_LIST = 0x04;
    constexpr uint8_t COM_STMT_PREPARE = 0x16;
    constexpr uint8_t COM_STMT_EXECUTE = 0x17;
    constexpr uint8_t COM_STMT_CLOSE = 0x19;
    constexpr uint8_t COM_STMT_RESET = 0x1A;
    constexpr uint8_t COM_RESET_CONNECTION = 0x1F;
}
```

#### MySQL Handshake Flow

```
1. Server sends Handshake Initialization Packet:
   - Protocol version (1 byte)
   - Server version (null-terminated string)
   - Connection ID (4 bytes)
   - Auth plugin data part 1 (8 bytes)
   - Filler (1 byte)
   - Server capabilities (4 bytes)
   - Character set (1 byte)
   - Status flags (2 bytes)
   - Capabilities upper (2 bytes)
   - Auth plugin data length (1 byte)
   - Reserved (10 bytes)
   - Auth plugin data part 2 (variable)
   - Auth plugin name (null-terminated)

2. Client sends Handshake Response Packet:
   - Client capabilities (4 bytes)
   - Max packet size (4 bytes)
   - Character set (1 byte)
   - Reserved (23 bytes)
   - Username (null-terminated)
   - Auth response (length-encoded)
   - Database (null-terminated, optional)
   - Auth plugin name (null-terminated)
   - Connect attributes (optional)

3. Server sends OK or ERR packet
```

#### MySQL OK Packet Format

```
Header: 0x00 or 0xFE (OK header or EOF in old protocol)
Affected rows: length-encoded integer
Last insert ID: length-encoded integer
Status flags: 2 bytes
Warnings: 2 bytes
Info: string (optional, protocol 4.1+)
Session state: string (optional)
```

#### MySQL ERR Packet Format

```
Header: 0xFF
Error code: 2 bytes
SQL state marker: '#' (if CLIENT_PROTOCOL_41)
SQL state: 5 bytes
Error message: string
```

### Firebird Adapter

#### Firebird Protocol Constants

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/adapters/firebird_adapter.cpp

namespace firebird {
    // Operation codes (op_)
    constexpr int32_t op_attach = 19;
    constexpr int32_t op_create = 20;
    constexpr int32_t op_detach = 21;
    constexpr int32_t op_transaction = 22;
    constexpr int32_t op_commit = 23;
    constexpr int32_t op_rollback = 24;
    constexpr int32_t op_open_blob = 36;
    constexpr int32_t op_get_segment = 37;
    constexpr int32_t op_put_segment = 38;
    constexpr int32_t op_close_blob = 39;
    constexpr int32_t op_info_database = 48;
    constexpr int32_t op_info_transaction = 49;
    constexpr int32_t op_allocate_statement = 52;
    constexpr int32_t op_execute = 53;
    constexpr int32_t op_exec_immediate = 54;
    constexpr int32_t op_fetch = 56;
    constexpr int32_t op_prepare_statement = 60;
    constexpr int32_t op_free_statement = 61;
    constexpr int32_t op_insert = 62;
    constexpr int32_t op_connect_request = 81;
    constexpr int32_t op_accept = 86;
    
    // Error codes
    namespace ErrorCode {
        constexpr int32_t isc_sqlerr = 335544436;
        constexpr int32_t isc_dsql_error = 335544569;
        constexpr int32_t isc_login = 335544472;
        constexpr int32_t isc_unavailable = 335544375;
    }
}
```

#### Firebird XDR Encoding

Firebird uses XDR (External Data Representation) for wire format:

```cpp
// XDR helpers from firebird_adapter.cpp

void appendXdrUInt32(std::vector<uint8_t>& out, uint32_t value);
void appendXdrInt32(std::vector<uint8_t>& out, int32_t value);
void appendXdrUInt64(std::vector<uint8_t>& out, uint64_t value);
void appendXdrInt64(std::vector<uint8_t>& out, int64_t value);
void appendXdrOpaque(std::vector<uint8_t>& out, const uint8_t* data, size_t len);
```

**XDR Format Rules:**
- All integers are big-endian
- Strings are length-prefixed and padded to 4-byte boundaries
- Opaque data is padded to 4-byte boundaries

#### Firebird BLR (Binary Language Representation)

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/adapters/firebird_adapter.cpp:306

// BLR opcodes for SQLDA description
constexpr uint8_t blr_version5 = 5;
constexpr uint8_t blr_begin = 2;
constexpr uint8_t blr_message = 4;
constexpr uint8_t blr_short = 7;
constexpr uint8_t blr_long = 8;
constexpr uint8_t blr_float = 10;
constexpr uint8_t blr_sql_date = 12;
constexpr uint8_t blr_sql_time = 13;
constexpr uint8_t blr_int64 = 16;
constexpr uint8_t blr_bool = 23;
constexpr uint8_t blr_text = 14;
constexpr uint8_t blr_varying = 37;
constexpr uint8_t blr_varying2 = 38;
constexpr uint8_t blr_double = 27;
constexpr uint8_t blr_timestamp = 35;
constexpr uint8_t blr_end = 255;
```

### Interface Contracts

#### Function: `PostgresqlAdapter::parseMessage()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/adapters/postgresql_adapter.cpp:920

core::Status PostgresqlAdapter::parseMessage(network::Connection* conn);
```

**Preconditions:**
- Connection has data available in read buffer
- pg_state_ indicates expected message type

**Postconditions:**
- Message type stored in current_msg_type_
- Message length stored in current_msg_length_
- Message payload stored in current_msg_data_

**Process Flow:**
1. Check if in STARTUP state (different header format)
2. For startup: read 4-byte length, validate, read full message
3. For regular: read 1-byte type, 4-byte length, validate, read payload
4. Consume bytes from read buffer

#### Function: `PostgresqlAdapter::processMessage()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/adapters/postgresql_adapter.cpp:999

core::Status PostgresqlAdapter::processMessage(network::Connection* conn);
```

**Preconditions:**
- Message has been parsed via parseMessage()
- current_msg_type_ is valid

**Postconditions:**
- Message is handled according to type
- Response sent to client if needed
- State updated appropriately

**Message Type Dispatch:**
```cpp
switch (current_msg_type_) {
    case 'Q': return handleQuery(conn);        // Simple query
    case 'P': return handleParse(conn);        // Parse
    case 'B': return handleBind(conn);         // Bind
    case 'E': return handleExecute(conn);      // Execute
    case 'D': return handleDescribe(conn);     // Describe
    case 'C': return handleClose(conn);        // Close
    case 'S': return handleSync(conn);         // Sync
    case 'd': return handleCopyData(conn);     // COPY data
    case 'c': return handleCopyDone(conn);     // COPY done
    case 'f': return handleCopyFail(conn);     // COPY fail
    case 'X': return handleTerminate(conn);    // Terminate
    default:  return sendErrorResponse(...);   // Unknown type
}
```

#### Function: `MySqlAdapter::processCommand()`

```cpp
// MySQL command dispatch

switch (command) {
    case COM_QUIT: return handleQuit(conn);
    case COM_INIT_DB: return handleInitDb(conn);
    case COM_QUERY: return handleQuery(conn);
    case COM_FIELD_LIST: return handleFieldList(conn);
    case COM_STMT_PREPARE: return handleStmtPrepare(conn);
    case COM_STMT_EXECUTE: return handleStmtExecute(conn);
    case COM_STMT_CLOSE: return handleStmtClose(conn);
    case COM_STMT_RESET: return handleStmtReset(conn);
    case COM_RESET_CONNECTION: return handleResetConnection(conn);
    default: return sendErrorPacket(conn, ER_UNKNOWN_COM_ERROR);
}
```

#### Function: `FirebirdAdapter::handleOpAccept()`

```cpp
// Firebird connection acceptance

core::Status FirebirdAdapter::handleOpAccept(network::Connection* conn);
```

**Process Flow:**
1. Send op_accept packet with protocol version
2. Wait for op_attach or op_create
3. Validate database path
4. Connect to engine via IPC
5. Send success response

### State Machines

#### PostgreSQL Extended Query State Machine

```
                         ┌─────────────┐
                         │    READY    │
                         └──────┬──────┘
                                │
            ┌───────────────────┼───────────────────┐
            │ Parse             │ Bind              │ Execute
            ▼                   ▼                   ▼
    ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
    │   PARSING    │   │   BINDING    │   │  EXECUTING   │
    └──────┬───────┘   └──────┬───────┘   └──────┬───────┘
           │                  │                  │
           │ ParseComplete    │ BindComplete     │ CommandComplete
           │ or Error         │ or Error         │ or Error
           ▼                  ▼                  ▼
    ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
    │    READY     │   │    READY     │   │    READY     │
    └──────────────┘   └──────────────┘   └──────────────┘
```

**Sync Point Rule:**
- All extended query operations must end with SYNC
- SYNC flushes the operation pipeline
- Errors are reported at the sync point

### Error Translation

#### PostgreSQL SQLSTATE Mapping

| Engine Status | PostgreSQL SQLSTATE | Description |
|---------------|---------------------|-------------|
| OK | 00000 | Successful completion |
| INVALID_ARGUMENT | 22023 | Invalid parameter value |
| SYNTAX_ERROR | 42601 | Syntax error |
| UNDEFINED_TABLE | 42P01 | Undefined table |
| UNDEFINED_COLUMN | 42703 | Undefined column |
| CONNECTION_FAILURE | 08001 | Connection failure |
| AUTH_FAILURE | 28000 | Invalid authorization |
| TIMEOUT | 57014 | Query canceled |

#### MySQL Error Code Mapping

| Engine Status | MySQL Error Code | Description |
|---------------|------------------|-------------|
| OK | 0 | Success |
| INVALID_ARGUMENT | 1048 | Column cannot be null |
| SYNTAX_ERROR | 1064 | Syntax error |
| UNDEFINED_TABLE | 1146 | Table doesn't exist |
| CONNECTION_FAILURE | 2003 | Can't connect |
| AUTH_FAILURE | 1045 | Access denied |

#### Firebird GDS Code Mapping

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/adapters/firebird_adapter.cpp:472

int32_t mapStatusToFirebird(core::Status st) {
    switch (st) {
        case Status::OK:
            return firebird::ErrorCode::isc_sqlerr;
        case Status::INVALID_ARGUMENT:
        case Status::SYNTAX_ERROR:
            return firebird::ErrorCode::isc_dsql_error;
        case Status::CONNECTION_FAILURE:
            return firebird::ErrorCode::isc_unavailable;
        case Status::PERMISSION_DENIED:
        case Status::INVALID_PASSWORD:
            return firebird::ErrorCode::isc_login;
        default:
            return firebird::ErrorCode::isc_dsql_error;
    }
}
```

## Invariants

1. **Protocol Version Invariant**: Each adapter MUST negotiate and enforce its protocol version
   - PostgreSQL: PROTOCOL_VERSION_3 (196608)
   - MySQL: Capabilities exchange required
   - Firebird: XDR wire protocol version 10+

2. **State Consistency**: Adapter state MUST match engine session state
   - Transaction status synchronized via READY_FOR_QUERY/OK packets
   - Prepared statements tracked in adapter cache

3. **Error Propagation**: All engine errors MUST be translated to protocol-specific formats
   - PostgreSQL: SQLSTATE + message fields
   - MySQL: Error code + SQLSTATE + message
   - Firebird: GDS error code + message

4. **Connection Binding**: Each adapter connection MUST map to exactly one IPC session
   - Session ID stored in client configuration
   - Reconnection creates new session

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_protocol_postgresql.cpp` | PostgreSQL wire protocol |
| `tests/unit/test_protocol_mysql.cpp` | MySQL wire protocol |
| `tests/unit/test_protocol_firebird.cpp` | Firebird wire protocol |

## Migration Notes

- Protocol adapters were previously in-process; now run as separate parser agents
- Configuration moved from `protocol_adapters` section to `parser_agents`
- TLS configuration now centralized in security manager

## Related Specifications

- [wire_protocol.md](./wire_protocol.md) - SBWP frame format
- [ipc_session_lifecycle.md](./ipc_session_lifecycle.md) - Session lifecycle
- [parser_agent_contract.md](./parser_agent_contract.md) - Parser agent protocol

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Wire Protocol | Native database network protocol |
| SQLSTATE | Standard SQL error code (5 characters) |
| GDS | Firebird Global Database System |
| BLR | Binary Language Representation (Firebird) |
| XDR | External Data Representation (big-endian encoding) |
| Capabilities | Bitmask of supported protocol features |
| Portal | PostgreSQL cursor name |
| SQLDA | SQL Descriptor Area (Firebird) |

### References

- `/home/dcalford/CliWork/ScratchBird/src/protocol/adapters/postgresql_adapter.cpp` - PostgreSQL adapter
- `/home/dcalford/CliWork/ScratchBird/src/protocol/adapters/mysql_adapter.cpp` - MySQL adapter
- `/home/dcalford/CliWork/ScratchBird/src/protocol/adapters/firebird_adapter.cpp` - Firebird adapter
- PostgreSQL Protocol Documentation: https://www.postgresql.org/docs/current/protocol.html
- MySQL Protocol Documentation: https://dev.mysql.com/doc/dev/mysql-server/latest/
- Firebird Wire Protocol Documentation: https://firebirdsql.org/file/documentation/drivers_documentation/

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Engineering |
