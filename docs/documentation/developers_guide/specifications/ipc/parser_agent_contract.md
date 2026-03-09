# Specification: Parser Agent Contract

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | ipc/parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
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

This specification defines the contract between parser agents and the ScratchBird engine. Parser agents act as protocol translators, converting emulated database wire protocols (PostgreSQL, MySQL, Firebird) into native IPC messages for the engine. The contract specifies message formats, lifecycle management, and error handling for parser-agent communication.

## Scope

### In Scope

- Parser agent base class interface
- IPC message translation protocol
- Connection lifecycle management
- Query execution flow
- Error propagation from engine to client
- Flow control for COPY operations

### Out of Scope

- SQL parsing and SBLR generation (see SBLR specifications)
- Specific wire protocol implementations (see protocol_adapters.md)
- Authentication mechanisms (see security specifications)
- Network transport layer details

## Background

Parser agents enable ScratchBird to emulate multiple database protocols by:

1. **Protocol Translation**: Converting PostgreSQL, MySQL, Firebird wire formats to IPC messages
2. **SQL Normalization**: Translating dialect-specific SQL to SBLR bytecode
3. **Result Transformation**: Mapping engine results back to emulated protocol formats
4. **Session Management**: Maintaining connection state between client and engine

Each parser agent runs as an independent process that accepts client connections on a protocol-specific port and communicates with the engine via Unix domain sockets (or TCP fallback).

## Specification

### Data Structures

#### ParserAgentConfig Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/parser_agent.h

struct ParserAgentConfig {
    std::string protocol;           // "native", "postgresql", "mysql", "firebird"
    std::string listen_endpoint;    // Host:port or socket path for client connections
    std::string ipc_endpoint;       // Engine IPC socket path
    uint32_t max_connections = 100; // Maximum concurrent client connections
    uint32_t io_threads = 4;        // Number of I/O worker threads
    uint32_t idle_timeout_ms = 300000; // 5 minute idle timeout
};
```

#### ClientConnection Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/parser_agent.h

struct ClientConnection {
    uint32_t client_id;             // Unique client identifier
    int socket_fd;                  // Client socket file descriptor
    uint64_t connect_time_ms;       // Connection timestamp
    uint64_t last_activity_ms;      // Last activity timestamp
    std::string user;               // Authenticated username
    std::string database;           // Connected database
    uint32_t session_id;            // Engine session ID
    bool authenticated;             // Authentication status
    std::unique_ptr<IPCChannel> ipc_channel; // Engine IPC channel
};
```

#### IPCMessage Container

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:327

class IPCMessage {
public:
    IPCHeader header;               // 40-byte message header
    std::vector<uint8_t> payload;   // Variable-length payload
    
    IPCMessage();
    IPCMessage(IPCMessageType type, uint32_t session_id = 0);
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    bool deserialize(const uint8_t* data, size_t len);
    
    // Payload helpers
    template<typename T>
    T* getPayload() {
        payload.resize(sizeof(T));
        return reinterpret_cast<T*>(payload.data());
    }
    
    // Type checking
    IPCMessageType getType() const;
    void setType(IPCMessageType type);
    
    bool isValid() const;
    size_t getTotalSize() const;
};
```

#### IPCSimpleQueryPayload Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:213

struct IPCSimpleQueryPayload {
    uint32_t flags;           // Query flags
    uint32_t query_length;    // Length of SQL text
    // SQL text follows (variable length)
};
```

#### IPCParsePayload Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:220

struct IPCParsePayload {
    char stmt_name[64];           // Statement name
    char sql[IPC_MAX_SQL_LENGTH]; // SQL text (512KB max)
    uint16_t param_types[IPC_MAX_PARAMS]; // Parameter type OIDs
};
```

#### IPCBindPayload Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:227

struct IPCBindPayload {
    char portal_name[64];     // Portal name
    char stmt_name[64];       // Statement name
    uint16_t num_params;      // Number of parameters
    // IPCParamValue + data follows
};
```

#### IPCExecutePayload Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:235

struct IPCExecutePayload {
    char portal_name[64];     // Portal name
    uint32_t max_rows;        // Maximum rows (0=unlimited)
};
```

#### IPCFieldDesc Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:163

struct IPCFieldDesc {
    char name[64];            // Field name
    uint32_t table_oid;       // Table OID
    uint16_t column_num;      // Column number
    uint16_t type_oid;        // Type OID
    int16_t type_size;        // Type size (-1 for variable)
    int32_t type_modifier;    // Type modifier
    uint16_t format;          // 0=text, 1=binary
};
// Total size: 84 bytes (with alignment)
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

**Postconditions:**
- Socket listener is bound and listening
- Accept thread is running
- I/O threads are started
- running_ flag is true

**Error Handling:**
- Returns IO_ERROR if socket creation fails
- Returns NOT_IMPLEMENTED on unsupported platforms

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

#### Function: `NativeSBParserAgent::handleStartup()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp:527

core::Status NativeSBParserAgent::handleStartup(ClientConnection& client, core::ErrorContext* ctx);
```

**Preconditions:**
- Client socket is connected
- No active session for this client

**Postconditions:**
- Protocol version is validated
- Connection parameters are extracted
- READY message is sent
- Client is marked authenticated

**Protocol:**
```
Client -> Agent: VERSION(2 bytes) + SSL_MODE(1 byte) + PARAMS(null-terminated pairs)
Agent -> Client: READY(type:1, length:4, session_id:4, features:4, version:4)
```

#### Function: `ParserAgent::sendToEngine()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp:294

core::Status ParserAgent::sendToEngine(uint32_t client_id, const IPCMessage& msg,
                                      core::ErrorContext* ctx);
```

**Preconditions:**
- Client exists in connections_ map
- Client has an active IPC channel

**Postconditions:**
- Message is serialized and sent to engine
- Return status indicates send success/failure

**Thread Safety:**
- Thread-safe via connections_mutex_ shared_lock

#### Function: `ParserAgent::receiveFromEngine()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/parser_agent.cpp:320

core::Status ParserAgent::receiveFromEngine(uint32_t client_id, IPCMessage& msg,
                                           core::ErrorContext* ctx,
                                           uint32_t timeout_ms = 0);
```

**Preconditions:**
- Client exists with active IPC channel
- timeout_ms = 0 means blocking receive

**Postconditions:**
- msg is populated with received message
- Returns OK if message received within timeout

**Error Handling:**
- Returns NOT_FOUND if client doesn't exist
- Returns TIMEOUT if timeout expires

### Algorithms

#### Algorithm: Query Execution Flow

```
Input:  Client query via wire protocol
Output: Query results to client

1. Parse wire protocol message (PostgreSQL/MySQL/Firebird)
2. Extract SQL text and parameters
3. Create IPCMessage with type SIMPLE_QUERY or PARSE/BIND/EXECUTE
4. Copy SQL text into payload
5. Send message to engine via IPC channel
6. While not complete:
   a. Receive response from engine
   b. Translate response to wire protocol format
   c. Send to client
   d. If COMMAND_COMPLETE or ERROR_RESPONSE, break
7. Return success
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
   e. Send ParseComplete to client

2. BIND phase:
   a. Receive BIND message with portal, statement, parameters
   b. Create IPCMessage(IPCMessageType::BIND)
   c. Send to engine
   d. Wait for BIND_COMPLETE or ERROR_RESPONSE
   e. Send BindComplete to client

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

### State Machines

#### Parser Agent Connection State Machine

```
                         ┌─────────────┐
                    ┌────┤ DISCONNECTED│◄─────────────────────────┐
                    │    └──────┬──────┘                          │
                    │           │ accept()                         │
                    │           ▼                                  │
                    │    ┌─────────────┐                          │
                    │    │  CONNECTED  │                          │
                    │    └──────┬──────┘                          │
                    │           │ Startup received                 │
                    │           ▼                                  │
                    │    ┌─────────────┐     Auth failed          │
                    │    │  STARTING   │─────────────────────────┘
                    │    └──────┬──────┘
                    │           │ Auth success
                    │           ▼
                    │    ┌─────────────┐
                    └────┤    ACTIVE   │◄──────────────┐
                         └──────┬──────┘               │
                                │ Query received       │
                                ▼                      │
                         ┌─────────────┐    Complete   │
                         │  EXECUTING  │───────────────┘
                         └──────┬──────┘
                                │ COPY operation
                                ▼
                    ┌───────────────────────┐
                    │      COPY_IN          │
                    │      COPY_OUT         │
                    └──────┬────────────────┘
                           │ COPY complete/fail
                           ▼
                    ┌─────────────┐
                    │   CLOSING   │
                    └──────┬──────┘
                           │ disconnect()
                           ▼
                    ┌─────────────┐
                    │ DISCONNECTED│
                    └─────────────┘
```

**State Transition Table:**

| Current State | Event | Action | Next State |
|---------------|-------|--------|------------|
| DISCONNECTED | accept | Create ClientConnection | CONNECTED |
| CONNECTED | Startup | Validate version, extract params | STARTING |
| STARTING | Auth OK | Send READY, set session_id | ACTIVE |
| STARTING | Auth Fail | Send error, close socket | DISCONNECTED |
| ACTIVE | SIMPLE_QUERY | Forward to engine | EXECUTING |
| ACTIVE | PARSE | Forward to engine | EXECUTING |
| ACTIVE | BIND | Forward to engine | EXECUTING |
| ACTIVE | EXECUTE | Forward to engine | EXECUTING |
| EXECUTING | Results | Translate and forward | ACTIVE |
| EXECUTING | COPY_IN | Setup flow control | COPY_IN |
| EXECUTING | COPY_OUT | Setup streaming | COPY_OUT |
| COPY_IN | COPY_DONE | Send COPY_COMPLETE | ACTIVE |
| COPY_OUT | COPY_COMPLETE | Resume normal flow | ACTIVE |
| * | Terminate | Cleanup resources | CLOSING |
| CLOSING | Cleanup done | Remove from registry | DISCONNECTED |

### Decision Trees

#### Message Routing Decision Tree

```
Message received from client
│
├── Parse wire protocol header
│   ├── Parse error → Send error response, close
│   └── Parse OK → Determine message type
│
       Message type?
       ├── STARTUP → Handle startup handshake
       │
       ├── SIMPLE_QUERY
       │   ├── Create IPCMessage(SIMPLE_QUERY)
       │   ├── Copy SQL text to payload
       │   ├── sendToEngine()
       │   └── forwardResponsesToClient()
       │
       ├── PARSE
       │   ├── Create IPCMessage(PARSE)
       │   ├── Copy statement name and SQL
       │   ├── sendToEngine()
       │   ├── receiveFromEngine() → PARSE_COMPLETE
       │   └── Send ParseComplete to client
       │
       ├── BIND
       │   ├── Create IPCMessage(BIND)
       │   ├── Copy portal name, statement name, parameters
       │   ├── sendToEngine()
       │   ├── receiveFromEngine() → BIND_COMPLETE
       │   └── Send BindComplete to client
       │
       ├── EXECUTE
       │   ├── Create IPCMessage(EXECUTE)
       │   ├── Copy portal name, max rows
       │   ├── sendToEngine()
       │   └── forwardResponsesToClient()
       │
       ├── COPY_DATA
       │   ├── Check flow control credits
       │   ├── Create IPCMessage(COPY_DATA)
       │   ├── sendToEngine()
       │   └── Update credits
       │
       └── TERMINATE
           ├── sendToEngine(DETACH)
           ├── Close IPC channel
           └── Close client socket
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

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `NOT_FOUND` | Client ID not in connections map | Log error, ignore message |
| `CONNECTION_FAILURE` | IPC channel disconnected | Attempt reconnect, else close client |
| `INVALID_ARGUMENT` | Malformed IPC message payload | Send ERROR_RESPONSE to client |
| `TIMEOUT` | Engine response timeout | Send timeout error to client |
| `IO_ERROR` | Socket read/write failure | Close connection, cleanup |
| `PROTOCOL_VIOLATION` | Unexpected message type | Log error, close connection |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_ipc_server.cpp` | IPC server lifecycle |
| `tests/unit/test_ipc_contract.cpp` | Message serialization |
| `tests/unit/test_ipc_policy.cpp` | Policy enforcement |

## Migration Notes

- Parser agents are a new architecture in 1.0.0-alpha1
- Previous versions used in-process protocol adapters
- Migration requires configuring separate parser agent processes

## Related Specifications

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
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Engineering |
