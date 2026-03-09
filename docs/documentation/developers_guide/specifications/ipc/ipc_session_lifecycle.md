# Specification: IPC Session Lifecycle

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | ipc |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 1.0.0-alpha1 |
| **Authors** | ScratchBird Engineering |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/ipc/ipc_server.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/ipc/ipc_contract_v1_1.cpp`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_server.h`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_server.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_contract.cpp:1`

## Synopsis

This specification defines the session lifecycle for IPC communication between ScratchBird parser agents and the engine. It covers session establishment, handshake protocol, feature negotiation, session states, and teardown procedures. The IPC protocol uses Unix domain sockets on Linux/macOS, named pipes on Windows, and TCP localhost as a fallback.

## Scope

### In Scope

- Session state machine and transitions
- IPC handshake protocol (STARTUP/READY)
- Feature negotiation
- Session attachment to databases
- Graceful and abnormal session termination
- Session statistics and monitoring

### Out of Scope

- Authentication mechanisms (see security specifications)
- SQL parsing and execution (see SBLR specifications)
- Copy flow control (see copy_flow_control.cpp)
- Network transport implementation details

## Background

The IPC subsystem provides reliable communication between parser agents and the ScratchBird engine. Each client connection results in a session that maintains state for:

1. **Connection Context**: Protocol version, feature flags, session ID
2. **Transaction State**: Current transaction status and savepoints
3. **Query Context**: Active query execution state
4. **Flow Control**: COPY operation credits and buffering

Sessions are managed by `IPCServer` and `IPCSession` classes with thread-safe message handling.

## Specification

### Data Structures

#### IPC Header Structure (40 bytes)

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:136

struct alignas(8) IPCHeader {
    uint32_t magic;           // Offset 0: 'SBIP' (0x53424950)
    uint16_t version;         // Offset 4: Protocol version (0x0101 = v1.1)
    uint16_t type;            // Offset 6: IPCMessageType
    uint32_t length;          // Offset 8: Payload length (excluding header)
    uint32_t request_id;      // Offset 12: Request sequence number
    uint32_t session_id;      // Offset 16: Session identifier
    uint64_t timestamp;       // Offset 20: Message timestamp (ns since epoch)
    uint32_t flags;           // Offset 28: Message flags
    uint32_t reserved;        // Offset 32: Reserved for future use
    // Padding to 40 bytes
};

static_assert(sizeof(IPCHeader) == 40, "IPCHeader must be 40 bytes");
```

**Binary Layout:**

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     'S'       |     'B'       |     'I'       |     'P'       | 0-3: Magic
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Version    |     Type      |            Length             | 4-9: Version/Type/Length
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Request ID            |          Session ID           | 10-17: IDs
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               | 18-25: Timestamp
|                        Timestamp (uint64)                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Flags     |    Reserved   |            Padding            | 26-31: Flags/Reserved
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

#### IPC Message Types

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:61

enum class IPCMessageType : uint16_t {
    // Connection Management (0x01-0x0F)
    STARTUP = 0x01,           // Client -> Server: Initial connection
    READY = 0x02,             // Server -> Client: Connection ready
    FEATURE_NEGOTIATE = 0x03, // Bidirectional: Feature negotiation
    TERMINATE = 0x04,         // Bidirectional: Clean disconnect
    PING = 0x05,              // Client -> Server: Keepalive
    PONG = 0x06,              // Server -> Client: Keepalive response
    
    // Session Management (0x10-0x1F)
    ATTACH = 0x10,            // Client -> Server: Attach to database
    DETACH = 0x11,            // Client -> Server: Detach from database
    ATTACHED = 0x12,          // Server -> Client: Attach successful
    DETACHED = 0x13,          // Server -> Client: Detach successful
    
    // Query Execution (0x20-0x2F)
    SIMPLE_QUERY = 0x20,      // Client -> Server: Execute SQL string
    PARSE = 0x21,             // Client -> Server: Parse SQL
    BIND = 0x22,              // Client -> Server: Bind parameters
    DESCRIBE = 0x23,          // Client -> Server: Describe statement/portal
    EXECUTE = 0x24,           // Client -> Server: Execute statement
    CLOSE = 0x25,             // Client -> Server: Close statement/portal
    SYNC = 0x26,              // Client -> Server: End request batch
    
    // Results (0x30-0x3F)
    ROW_DESCRIPTION = 0x30,   // Server -> Client: Result schema
    DATA_ROW = 0x31,          // Server -> Client: Single row
    DATA_BATCH = 0x32,        // Server -> Client: Multiple rows
    COMMAND_COMPLETE = 0x33,  // Server -> Client: Query done
    EMPTY_RESPONSE = 0x34,    // Server -> Client: No results
    PARSE_COMPLETE = 0x35,    // Server -> Client: Parse done
    BIND_COMPLETE = 0x36,     // Server -> Client: Bind done
    CLOSE_COMPLETE = 0x37,    // Server -> Client: Close done
    READY_FOR_QUERY = 0x38,   // Server -> Client: Ready for next query
    
    // COPY Operations (0x40-0x4F)
    COPY_IN_REQUEST = 0x40,   // Server -> Client: Expect COPY data
    COPY_OUT_RESPONSE = 0x41, // Server -> Client: COPY data incoming
    COPY_DATA = 0x42,         // Bidirectional: COPY data chunk
    COPY_DONE = 0x43,         // Client -> Server: COPY complete
    COPY_FAIL = 0x44,         // Client -> Server: COPY error
    COPY_COMPLETE = 0x45,     // Server -> Client: COPY done
    STREAM_CONTROL = 0x46,    // Bidirectional: Flow control
    
    // Transactions (0x50-0x5F)
    TXN_BEGIN = 0x50,         // Client -> Server: Begin transaction
    TXN_COMMIT = 0x51,        // Client -> Server: Commit transaction
    TXN_ROLLBACK = 0x52,      // Client -> Server: Rollback transaction
    SAVEPOINT = 0x53,         // Client -> Server: Create savepoint
    RELEASE = 0x54,           // Client -> Server: Release savepoint
    ROLLBACK_TO = 0x55,       // Client -> Server: Rollback to savepoint
    TXN_COMPLETE = 0x56,      // Server -> Client: Transaction command done
    
    // Asynchronous (0x60-0x6F)
    NOTIFY_SUBSCRIBE = 0x60,  // Client -> Server: Subscribe to channel
    NOTIFY_UNSUBSCRIBE = 0x61,// Client -> Server: Unsubscribe
    NOTIFY_DELIVER = 0x62,    // Server -> Client: Notification
    CANCEL_REQUEST = 0x63,    // Client -> Server: Cancel query
    CANCEL_ACK = 0x64,        // Server -> Client: Cancel accepted
    
    // Errors (0x70-0x7F)
    ERROR_RESPONSE = 0x70,    // Server -> Client: Error occurred
    NOTICE = 0x71,            // Server -> Client: Warning/info
    
    // Internal (0x80-0xFF)
    HEARTBEAT = 0x80,         // Internal: Health check
    SHUTDOWN = 0x81,          // Internal: Server shutdown notice
};
```

#### Feature Flags

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:47

constexpr uint32_t IPC_FEATURE_PREPARED_STATEMENTS = 0x00000001;
constexpr uint32_t IPC_FEATURE_COPY_STREAMING      = 0x00000002;
constexpr uint32_t IPC_FEATURE_NOTIFICATIONS       = 0x00000004;
constexpr uint32_t IPC_FEATURE_CANCEL              = 0x00000008;
constexpr uint32_t IPC_FEATURE_BINARY_RESULTS      = 0x00000010;
constexpr uint32_t IPC_FEATURE_COMPRESSION         = 0x00000020;
constexpr uint32_t IPC_FEATURE_ENCRYPTION          = 0x00000040;
constexpr uint32_t IPC_FEATURE_BATCH_EXECUTION     = 0x00000080;
```

#### Session State Enumeration

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_server.h

enum class SessionState {
    INITIALIZING,    // Session created, waiting for STARTUP
    NEGOTIATING,     // Feature negotiation in progress
    ACTIVE,          // Ready for queries
    EXECUTING,       // Query in progress
    COPY_IN,         // COPY FROM operation active
    COPY_OUT,        // COPY TO operation active
    CLOSING,         // Graceful shutdown in progress
    CLOSED           // Session terminated
};
```

#### STARTUP Payload Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:189

struct IPCStartupPayload {
    uint32_t process_id;      // Client process ID
    uint32_t secret_key;      // Secret key for cancel operations
    uint32_t feature_flags;   // Requested features bitmask
    char database[64];        // Target database name
    char user[64];            // Username for authentication
    char application[64];     // Application name
};
// Total: 200 bytes (without padding)
```

#### READY Payload Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:199

struct IPCReadyPayload {
    uint32_t session_id;      // Assigned session ID
    uint32_t server_features; // Supported server features
    char server_version[32];  // Server version string
};
// Total: 40 bytes
```

### Interface Contracts

#### Function: `IPCSession::start()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/ipc_server.cpp:57

core::Status IPCSession::start(core::ErrorContext* ctx);
```

**Preconditions:**
- Channel must be connected
- Handler must be set

**Postconditions:**
- Session state transitions to INITIALIZING
- Start time is recorded

**Thread Safety:**
- Thread-safe for concurrent read/write operations via io_mutex_

#### Function: `IPCSession::handleMessage()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/ipc_server.cpp:82

core::Status IPCSession::handleMessage(const IPCMessage& msg, core::ErrorContext* ctx);
```

**Preconditions:**
- Message header must be valid (magic, version)
- Session must not be in CLOSED state

**Postconditions:**
- Activity timestamp updated
- Message statistics incremented
- Appropriate handler invoked based on message type

**Error Handling:**
- Returns NOT_SUPPORTED for unknown message types
- Returns INVALID_ARGUMENT for malformed payloads

#### Function: `IPCSession::handleStartup()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/ipc_server.cpp:175

core::Status IPCSession::handleStartup(const IPCMessage& msg, core::ErrorContext* ctx);
```

**Preconditions:**
- Session state must be INITIALIZING
- Payload must contain valid IPCStartupPayload

**Postconditions:**
- State transitions to NEGOTIATING during processing
- On success, state transitions to ACTIVE
- READY message sent to client with assigned features

**Process Flow:**
1. Validate startup payload
2. Call handler_->onAttach() for authentication
3. Send READY response with negotiated features
4. Transition to ACTIVE state

### Algorithms

#### Algorithm: Session Handshake

```
Input:  Client connection on IPC channel
Output: Established session or error

1. Create IPCSession with unique session ID
2. Transition state to INITIALIZING
3. Wait for STARTUP message from client
4. Validate STARTUP payload:
   a. Check magic is 0x53424950 ('SBIP')
   b. Check version is 0x0101 (v1.1)
   c. Extract feature_flags, database, user
5. Transition state to NEGOTIATING
6. Authenticate user via handler_->onAttach()
7. If authentication fails:
   a. Send ERROR_RESPONSE
   b. Close connection
   c. Return error
8. Calculate agreed_features = client_features & server_features
9. Send READY message with:
   - session_id (assigned unique ID)
   - server_features (capabilities)
   - server_version (e.g., "1.0.0-alpha1")
10. Transition state to ACTIVE
11. Return success
```

**Complexity:**
- Time: O(1) for fixed-size operations
- Space: O(1) for session state

#### Algorithm: Session Teardown

```
Input:  Active IPCSession
Output: Clean session termination

1. Transition state to CLOSING
2. If query is executing:
   a. Signal cancellation via handler_->onCancel()
   b. Wait for query termination (max 5s timeout)
3. Release all resources:
   a. Close prepared statements
   b. Close portals
   c. Rollback any open transactions
4. Disconnect channel
5. Transition state to CLOSED
6. Remove session from server registry
```

### State Machines

#### Session Lifecycle State Machine

```
                         ┌─────────────┐
                    ┌────┤   CLOSED    │◄─────────────────────────┐
                    │    └──────┬──────┘                          │
                    │           │ createSession()                  │
                    │           ▼                                  │
                    │    ┌─────────────┐                          │
                    │    │ INITIALIZING│                          │
                    │    └──────┬──────┘                          │
                    │           │ STARTUP received                 │
                    │           ▼                                  │
                    │    ┌─────────────┐     Auth failed          │
                    │    │ NEGOTIATING │─────────────────────────┘
                    │    └──────┬──────┘
                    │           │ Auth success, READY sent
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
                           │ COPY_DONE/FAIL
                           ▼
                    ┌─────────────┐
                    │   CLOSING   │
                    └──────┬──────┘
                           │ Cleanup complete
                           ▼
                    ┌─────────────┐
                    │   CLOSED    │
                    └─────────────┘
```

**State Transition Table:**

| Current State | Event | Condition | Action | Next State |
|---------------|-------|-----------|--------|------------|
| - | createSession | - | Allocate session ID | INITIALIZING |
| INITIALIZING | STARTUP | Valid header | Validate credentials | NEGOTIATING |
| NEGOTIATING | Auth OK | Credentials valid | Send READY | ACTIVE |
| NEGOTIATING | Auth Fail | Credentials invalid | Send ERROR | CLOSING |
| ACTIVE | SIMPLE_QUERY | - | Begin execution | EXECUTING |
| ACTIVE | PARSE/BIND/EXECUTE | - | Extended query | EXECUTING |
| ACTIVE | COPY_IN_REQUEST | - | Setup COPY | COPY_IN |
| ACTIVE | COPY_OUT_RESPONSE | - | Setup COPY | COPY_OUT |
| EXECUTING | Complete | Success | Send results | ACTIVE |
| EXECUTING | Error | Failure | Send ERROR | ACTIVE |
| COPY_IN | COPY_DONE | Success | Send COMPLETE | ACTIVE |
| COPY_IN | COPY_FAIL | Error | Send ERROR | ACTIVE |
| COPY_OUT | COPY_COMPLETE | Success | - | ACTIVE |
| * | TERMINATE | - | Cleanup resources | CLOSING |
| * | Timeout | Idle > threshold | Send warning | CLOSING |
| CLOSING | Cleanup done | - | Release resources | CLOSED |

### Decision Trees

#### Handshake Decision Tree

```
Client connects
│
├── STARTUP message received?
│   ├── No → Wait (with timeout)
│   └── Yes → Validate header
│
       Header valid?
       ├── No → Send ERROR_RESPONSE, close connection
       └── Yes → Check version
│
              Version supported?
              ├── No → Send version negotiation error
              └── Yes → Authenticate user
│
                     Authentication successful?
                     ├── No → Send AUTH_FAILED, close connection
                     └── Yes → Calculate agreed features
│
                            Send READY
                            Transition to ACTIVE
```

## Invariants

1. **Header Magic Invariant**: All IPC messages MUST have magic = 0x53424950 ('SBIP')
   - Verification: `header.magic == IPCHeader::MAGIC`
   - Source: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_contract.cpp:35`

2. **Header Size Invariant**: IPCHeader MUST be exactly 40 bytes
   - Verification: `sizeof(IPCHeader) == 40`
   - Source: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_contract.cpp:39`

3. **Session State Progression**: Session states MUST follow valid transitions only
   - No direct transition from INITIALIZING to EXECUTING
   - CLOSED is terminal state

4. **Message Size Limit**: Payload MUST NOT exceed 1MB
   - Verification: `length <= IPC_MAX_MESSAGE_SIZE` (1MB)
   - Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:39`

5. **Timestamp Monotonicity**: Message timestamps SHOULD increase monotonically
   - Used for detecting delayed/reordered messages

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `INVALID_ARGUMENT` | Malformed STARTUP payload | Send ERROR_RESPONSE, close |
| `CONNECTION_FAILURE` | Channel disconnected | Transition to CLOSING |
| `NOT_SUPPORTED` | Unknown message type | Send ERROR_RESPONSE |
| `AUTH_FAILURE` | Authentication failed | Send ERROR_RESPONSE, close |
| `TIMEOUT` | Handshake timeout | Close connection |
| `PROTOCOL_VIOLATION` | Invalid state transition | Close connection |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_ipc_server.cpp` | Server lifecycle, session management |
| `tests/unit/test_ipc_contract.cpp` | Message serialization, header validation |
| `tests/unit/test_ipc_policy.cpp` | Feature negotiation, policy enforcement |

## Related Specifications

- [wire_protocol.md](./wire_protocol.md) - SBWP frame format
- [parser_agent_contract.md](./parser_agent_contract.md) - Parser agent protocol
- [protocol_adapters.md](./protocol_adapters.md) - Emulated protocol adapters

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| IPC | Inter-Process Communication |
| Session | Logical connection between client and server |
| Attachment | Database connection context within a session |
| Portal | Cursor for executing prepared statements |
| Feature Flags | Bitmask indicating protocol capabilities |

### References

- `/home/dcalford/CliWork/ScratchBird/src/ipc/ipc_server.cpp` - IPC server implementation
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h` - IPC contract definitions
- `/home/dcalford/CliWork/local_work/docs/specifications/27_Native_Handshake/` - External handshake reference

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Engineering |
