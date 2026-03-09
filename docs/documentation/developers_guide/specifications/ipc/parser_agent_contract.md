# Specification: Parser Agent Contract

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | ipc/parser |
| **Spec Version** | 1.1.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 1.0.0-alpha1 |
| **Authors** | ScratchBird Engineering |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/ipc/external_agents/postgresql_parser_agent.cpp`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/ipc/external_agents/mysql_parser_agent.cpp`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/ipc/external_agents/firebird_parser_agent.cpp`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/parser_agent.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_*.cpp`

## Synopsis

This specification defines the complete contract between parser agents and the ScratchBird engine. Parser agents act as protocol translators, converting emulated database wire protocols (PostgreSQL, MySQL, Firebird) into native IPC messages for the engine. The contract specifies message formats, lifecycle management, protocol translation, and error handling.

## Scope

### In Scope

- Parser agent base class interface
- Protocol-specific agent implementations
- IPC message translation protocol
- Connection lifecycle management
- Query execution flow (simple and extended)
- Prepared statement handling
- Error propagation from engine to client
- Flow control for COPY operations
- Capability negotiation

### Out of Scope

- SQL parsing and SBLR generation (see SBLR specifications)
- Specific wire protocol implementations (see protocol_adapters.md)
- Authentication mechanisms (see security specifications)
- Network transport layer details (see ipc_channels.md)

## Background

Parser agents enable ScratchBird to emulate multiple database protocols by:

1. **Protocol Translation**: Converting PostgreSQL, MySQL, Firebird wire formats to IPC messages
2. **SQL Normalization**: Translating dialect-specific SQL to canonical form
3. **Result Transformation**: Mapping engine results back to emulated protocol formats
4. **Session Management**: Maintaining connection state between client and engine
5. **Type Mapping**: Converting between protocol-specific and engine types

Each parser agent runs as an independent process that accepts client connections on a protocol-specific port and communicates with the engine via IPC channels.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Parser Agent Architecture                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────┐    Wire Protocol    ┌──────────────┐             │
│  │   Client     │◄───────────────────►│ Parser Agent │             │
│  │  (psql/mysql)│   (PostgreSQL/      │  Process     │             │
│  └──────────────┘    MySQL/Firebird)  └──────┬───────┘             │
│                                              │                      │
│                                              │ IPC (SBWP)           │
│                                              │                      │
│                                              ▼                      │
│                                       ┌──────────────┐             │
│                                       │    Engine    │             │
│                                       │   Process    │             │
│                                       └──────────────┘             │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

## Specification

### Data Structures

#### ParserAgentConfig Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/parser_agent.h

struct ParserAgentConfig {
    std::string protocol;              // "native", "postgresql", "mysql", "firebird"
    std::string listen_endpoint;       // Host:port or socket path for client connections
    std::string ipc_endpoint;          // Engine IPC socket path
    uint32_t max_connections = 100;    // Maximum concurrent client connections
    uint32_t io_threads = 4;           // Number of I/O worker threads
    uint32_t idle_timeout_ms = 300000; // 5 minute idle timeout
    bool enable_ssl = true;            // Enable SSL/TLS
    uint32_t max_query_size = 1024 * 1024;  // 1MB max query size
};
```

#### ClientConnection Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/parser_agent.h

struct ClientConnection {
    uint32_t client_id;              // Unique client identifier
    int socket_fd;                   // Client socket file descriptor
    uint64_t connect_time_ms;        // Connection timestamp
    uint64_t last_activity_ms;       // Last activity timestamp
    std::string user;                // Authenticated username
    std::string database;            // Connected database
    std::string application;         // Application name
    uint32_t session_id;             // Engine session ID
    bool authenticated;              // Authentication status
    std::unique_ptr<IPCChannel> ipc_channel;  // Engine IPC channel
    
    // Protocol-specific state
    std::unordered_map<std::string, PreparedStatement> prepared_stmts;
    std::unordered_map<std::string, Portal> portals;
    std::vector<std::string> parameter_status;
};
```

#### Parser Agent Statistics

```cpp
struct Stats {
    uint32_t connections_accepted;
    uint32_t connections_closed;
    uint32_t active_connections;
    uint32_t queries_processed;
    uint32_t errors;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t uptime_ms;
};
```

### Interface Contracts

#### Function: `ParserAgent::start()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp:50

core::Status ParserAgent::start(core::ErrorContext* ctx);
```

**Preconditions:**
- Config must specify valid protocol and endpoints
- No existing listener on the configured endpoint
- Sufficient resources available

**Postconditions:**
- Socket listener is bound and listening
- Accept thread is running
- I/O threads are started
- running_ flag is true

**Error Handling:**
- Returns IO_ERROR if socket creation fails
- Returns ALREADY_EXISTS if endpoint is in use
- Returns NOT_IMPLEMENTED on unsupported platforms

**Thread Safety:**
- Safe to call from any thread
- Not safe to call multiple times without stop()

#### Function: `ParserAgent::stop()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp:73

core::Status ParserAgent::stop(core::ErrorContext* ctx);
```

**Preconditions:**
- start() was previously called

**Postconditions:**
- Listener is closed
- All threads are stopped
- All client connections are closed
- All IPC channels are released
- running_ flag is false

**Error Handling:**
- Returns error if cleanup fails (but continues)

#### Function: `ParserAgent::handleClient()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp:416

core::Status ParserAgent::handleClient(int client_fd, core::ErrorContext* ctx);
```

**Preconditions:**
- client_fd is a valid connected socket
- Connection count is below max_connections

**Postconditions:**
- ClientConnection record is created
- Client thread is spawned for message handling
- Stats are updated (connections_accepted, active_connections)

**Process Flow:**
1. Create ClientConnection with unique client_id
2. Store in connections_ map
3. Spawn client handling thread
4. Return immediately (asynchronous handling)

**Thread Safety:**
- Thread-safe via connections_mutex_

#### Function: `ParserAgent::sendToEngine()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp:294

core::Status ParserAgent::sendToEngine(uint32_t client_id, 
                                       const IPCMessage& msg,
                                       core::ErrorContext* ctx);
```

**Preconditions:**
- Client exists in connections_ map
- Client has an active IPC channel
- Message is valid

**Postconditions:**
- Message is serialized and sent to engine
- Return status indicates send success/failure

**Thread Safety:**
- Thread-safe via connections_mutex_ shared_lock

**Error Handling:**
- Returns NOT_FOUND if client doesn't exist
- Returns CONNECTION_FAILURE if IPC channel disconnected

#### Function: `ParserAgent::receiveFromEngine()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp:320

core::Status ParserAgent::receiveFromEngine(uint32_t client_id, 
                                            IPCMessage& msg,
                                            core::ErrorContext* ctx,
                                            uint32_t timeout_ms = 0);
```

**Preconditions:**
- Client exists with active IPC channel

**Postconditions:**
- msg is populated with received message
- Returns OK if message received within timeout

**Parameters:**
- timeout_ms: 0 = blocking receive, >0 = timeout in milliseconds

**Error Handling:**
- Returns NOT_FOUND if client doesn't exist
- Returns TIMEOUT if timeout expires
- Returns CONNECTION_CLOSED if channel disconnected

### Protocol-Specific Agents

#### NativeSBParserAgent

Handles native ScratchBird wire protocol.

```cpp
class NativeSBParserAgent : public ParserAgent {
public:
    core::Status handleClient(int client_fd, core::ErrorContext* ctx) override;
    core::Status handleStartup(ClientConnection& client, core::ErrorContext* ctx);
    core::Status handleQuery(ClientConnection& client, 
                             const std::string& sql,
                             core::ErrorContext* ctx);
    core::Status handleParse(ClientConnection& client,
                             const std::string& stmt_name,
                             const std::string& sql,
                             core::ErrorContext* ctx);
    core::Status handleBind(ClientConnection& client,
                            const std::string& portal_name,
                            const std::string& stmt_name,
                            core::ErrorContext* ctx);
    core::Status handleExecute(ClientConnection& client,
                               const std::string& portal_name,
                               uint32_t max_rows,
                               core::ErrorContext* ctx);
    core::Status handleClose(ClientConnection& client, char type,
                             const std::string& name,
                             core::ErrorContext* ctx);
    core::Status handleSync(ClientConnection& client, core::ErrorContext* ctx);
    core::Status handleTerminate(ClientConnection& client, core::ErrorContext* ctx);
    
private:
    core::Status sendReady(ClientConnection& client, uint32_t features);
    core::Status sendParseComplete(ClientConnection& client);
    core::Status sendBindComplete(ClientConnection& client);
    core::Status sendCloseComplete(ClientConnection& client);
    core::Status sendCommandComplete(ClientConnection& client, const std::string& tag);
    core::Status sendRowDescription(ClientConnection& client,
                                    const std::vector<IPCFieldDesc>& fields);
    core::Status sendDataRow(ClientConnection& client,
                             const std::vector<std::optional<std::string>>& values);
    core::Status sendError(ClientConnection& client,
                           const char* sqlstate,
                           const std::string& message);
    core::Status forwardResponseToClient(ClientConnection& client,
                                         const IPCMessage& response,
                                         core::ErrorContext* ctx);
};
```

#### PostgreSQLParserAgent

Handles PostgreSQL wire protocol v3.

```cpp
class PostgreSQLParserAgent : public ParserAgent {
public:
    core::Status handleClient(int client_fd, core::ErrorContext* ctx) override;
    
private:
    // PostgreSQL-specific handlers
    core::Status handleStartupMessage(ClientConnection& client);
    core::Status handleSSLRequest(ClientConnection& client);
    core::Status handleQuery(ClientConnection& client, const std::string& sql);
    core::Status handleParse(ClientConnection& client);      // 'P' message
    core::Status handleBind(ClientConnection& client);       // 'B' message
    core::Status handleExecute(ClientConnection& client);    // 'E' message
    core::Status handleDescribe(ClientConnection& client);   // 'D' message
    core::Status handleClose(ClientConnection& client);      // 'C' message
    core::Status handleSync(ClientConnection& client);       // 'S' message
    core::Status handleTerminate(ClientConnection& client);  // 'X' message
    core::Status handleCopyData(ClientConnection& client);   // 'd' message
    core::Status handleCopyDone(ClientConnection& client);   // 'c' message
    core::Status handleCopyFail(ClientConnection& client);   // 'f' message
    
    // Response builders
    core::Status sendAuthenticationOk(ClientConnection& client);
    core::Status sendAuthenticationMD5Password(ClientConnection& client, 
                                                const uint8_t salt[4]);
    core::Status sendParameterStatus(ClientConnection& client,
                                     const std::string& name,
                                     const std::string& value);
    core::Status sendBackendKeyData(ClientConnection& client);
    core::Status sendReadyForQuery(ClientConnection& client, char status);
    core::Status sendRowDescriptionPG(ClientConnection& client,
                                      const std::vector<FieldInfo>& fields);
    core::Status sendDataRowPG(ClientConnection& client,
                               const std::vector<Value>& values);
    core::Status sendCommandCompletePG(ClientConnection& client,
                                       const std::string& tag);
    core::Status sendErrorResponsePG(ClientConnection& client,
                                     const char* sqlstate,
                                     const std::string& message);
    core::Status sendNoticeResponsePG(ClientConnection& client,
                                      const std::string& message);
    core::Status sendParseCompletePG(ClientConnection& client);
    core::Status sendBindCompletePG(ClientConnection& client);
    core::Status sendCloseCompletePG(ClientConnection& client);
    core::Status sendCopyInResponsePG(ClientConnection& client);
    core::Status sendCopyOutResponsePG(ClientConnection& client);
    core::Status sendCopyDataPG(ClientConnection& client, const uint8_t* data, size_t len);
    core::Status sendCopyDonePG(ClientConnection& client);
};
```

### Algorithms

#### Algorithm: Query Execution Flow

```
Input:  Client query via wire protocol
Output: Query results to client

1. Parse wire protocol message (PostgreSQL/MySQL/Firebird)
2. Extract SQL text and parameters
3. Normalize SQL dialect to canonical form
4. Create IPCMessage with type SIMPLE_QUERY or PARSE/BIND/EXECUTE
5. Copy SQL text into payload
6. Send message to engine via IPC channel
7. While not complete:
   a. Receive response from engine
   b. Translate response to wire protocol format
   c. Send to client
   d. If COMMAND_COMPLETE or ERROR_RESPONSE, break
8. Return success
```

**Complexity:**
- Time: O(n) where n is result set size
- Space: O(1) for streaming (O(n) if buffering)

#### Algorithm: Extended Query Protocol

```
Input:  PARSE, BIND, EXECUTE, SYNC sequence
Output: Prepared statement execution results

1. PARSE phase:
   a. Receive PARSE message with SQL and statement name
   b. Create IPCMessage(IPCMessageType::PARSE)
   c. Send to engine
   d. Wait for PARSE_COMPLETE or ERROR_RESPONSE
   e. Store prepared statement info locally
   f. Send ParseComplete to client

2. BIND phase:
   a. Receive BIND message with portal, statement, parameters
   b. Create IPCMessage(IPCMessageType::BIND)
   c. Send to engine
   d. Wait for BIND_COMPLETE or ERROR_RESPONSE
   e. Store portal info locally
   f. Send BindComplete to client

3. DESCRIBE phase (optional):
   a. Create IPCMessage(IPCMessageType::DESCRIBE)
   b. Receive ROW_DESCRIPTION or PARAMETER_DESCRIPTION
   c. Forward to client

4. EXECUTE phase:
   a. Create IPCMessage(IPCMessageType::EXECUTE)
   b. Send to engine
   c. While receiving DATA_ROW:
      - Translate and forward to client
   d. Send CommandComplete to client

5. SYNC phase:
   a. Create IPCMessage(IPCMessageType::SYNC)
   b. Send to engine
   c. Wait for READY_FOR_QUERY
   d. Forward to client
```

## Invariants

1. **IPC Channel Availability**: Active sessions MUST have a valid IPC channel
   - Verification: `client.ipc_channel != nullptr && client.ipc_channel->isConnected()`

2. **Session ID Assignment**: Authenticated clients MUST have a valid session_id
   - Verification: `client.session_id != 0` after successful handshake

3. **Message Type Validity**: Only valid IPCMessageType values may be sent to engine
   - Verification: `static_cast<uint16_t>(type) >= 0x01 && type <= 0xFF`

4. **Payload Size Limit**: IPC message payloads MUST NOT exceed 1MB
   - Verification: `payload.size() <= IPC_MAX_PAYLOAD_SIZE`
   - Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:40`

5. **Thread Safety**: All connection map access MUST be protected by connections_mutex_
   - Verification: Use `std::shared_lock<std::shared_mutex>` for reads
   - Verification: Use `std::unique_lock<std::shared_mutex>` for writes

6. **Prepared Statement Tracking**: All prepared statements MUST be tracked
   - Verification: Statement in local map before sending PARSE_COMPLETE

7. **Portal Tracking**: All portals MUST be tracked
   - Verification: Portal in local map before sending BIND_COMPLETE

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `NOT_FOUND` | Client ID not in connections map | Log error, ignore message |
| `CONNECTION_FAILURE` | IPC channel disconnected | Attempt reconnect, else close client |
| `INVALID_ARGUMENT` | Malformed IPC message payload | Send ERROR_RESPONSE to client |
| `TIMEOUT` | Engine response timeout | Send timeout error to client |
| `IO_ERROR` | Socket read/write failure | Close connection, cleanup |
| `PROTOCOL_VIOLATION` | Unexpected message type | Log error, close connection |
| `NOT_SUPPORTED` | Unsupported feature | Send feature_not_supported error |

## Configuration

### Parser Agent Factory

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp:381

class ParserAgentFactory {
public:
    static std::unique_ptr<ParserAgent> create(const ParserAgentConfig& config);
    static std::unique_ptr<ParserAgent> create(const std::string& protocol,
                                               const std::string& listen_endpoint,
                                               const std::string& ipc_endpoint);
};
```

**Supported Protocols:**
- "native" or "scratchbird" - Native SBWP
- "postgresql" - PostgreSQL v3 wire protocol
- "mysql" - MySQL protocol 4.1+
- "firebird" - Firebird wire protocol

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_parser_agent.cpp` | Base parser agent functionality |
| `tests/unit/test_parser_postgresql.cpp` | PostgreSQL protocol handling |
| `tests/unit/test_parser_mysql.cpp` | MySQL protocol handling |
| `tests/unit/test_parser_firebird.cpp` | Firebird protocol handling |
| `tests/unit/test_ipc_contract.cpp` | IPC message serialization |

## Migration Notes

- Parser agents are a new architecture in 1.0.0-alpha1
- Previous versions used in-process protocol adapters
- Migration requires configuring separate parser agent processes
- Configuration moved from `protocol_adapters` section to `parser_agents`

## Related Specifications

- [parser_requests.md](./parser_requests.md) - Request handling details
- [parser_responses.md](./parser_responses.md) - Response generation
- [parser_capabilities.md](./parser_capabilities.md) - Capability negotiation
- [wire_protocol.md](./wire_protocol.md) - SBWP frame format
- [ipc_session_lifecycle.md](./ipc_session_lifecycle.md) - Session lifecycle
- [protocol_adapters.md](./protocol_adapters.md) - Emulated protocol adapters

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Parser Agent | Process that translates emulated protocols to IPC |
| ClientConnection | State for a single client connection |
| IPC Channel | Communication channel to engine |
| Portal | Named cursor for prepared statement execution |
| SBLR | ScratchBird Language Representation (bytecode) |
| Wire Protocol | Native database protocol (PostgreSQL/MySQL/Firebird) |

### References

- `/home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp` - Base parser agent implementation
- `/home/dcalford/CliWork/ScratchBird/src/ipc/external_agents/` - Protocol-specific agents
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/parser_agent.h` - Parser agent interface

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.1.0 | 2026-03-08 | Added capability negotiation, extended query protocol | ScratchBird Engineering |
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Engineering |
