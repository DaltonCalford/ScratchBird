# Specification: IPC Session Lifecycle

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | ipc/session |
| **Spec Version** | 1.1.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 1.0.0-alpha1 |
| **Authors** | ScratchBird Engineering |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/ipc/ipc_server.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/ipc/ipc_contract_v1_1.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_server.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_server.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_contract.cpp:1`

## Synopsis

This specification defines the complete session lifecycle for IPC communication between ScratchBird parser agents and the engine. It covers session establishment, handshake protocol, feature negotiation, session states, session maintenance, and teardown procedures. The IPC protocol uses Unix domain sockets on Linux/macOS, named pipes on Windows, and TCP localhost as a fallback.

## Scope

### In Scope

- Session state machine and transitions
- IPC handshake protocol (STARTUP/READY)
- Feature negotiation and capability discovery
- Session attachment to databases
- Session maintenance (keepalive, idle detection)
- Graceful and abnormal session termination
- Session statistics and monitoring
- Session resource limits

### Out of Scope

- Authentication mechanisms (see security specifications)
- SQL parsing and execution (see SBLR specifications)
- Copy flow control (see copy_protocol.md)
- Network transport implementation details (see ipc_channels.md)

## Background

The IPC subsystem provides reliable communication between parser agents and the ScratchBird engine. Each client connection results in a session that maintains state for:

1. **Connection Context**: Protocol version, feature flags, session ID, security context
2. **Transaction State**: Current transaction status, savepoints, isolation level
3. **Query Context**: Active query execution state, prepared statements, portals
4. **Flow Control**: COPY operation credits, buffering, backpressure
5. **Statistics**: Message counts, timing, resource usage

Sessions are managed by `IPCServer` and `IPCSession` classes with thread-safe message handling.

## Specification

### Data Structures

#### IPC Header Structure (40 bytes)

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:136-158

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
    
    static constexpr uint32_t MAGIC = 0x53424950;
    static constexpr uint32_t FLAG_COMPRESSED = 0x00000001;
    static constexpr uint32_t FLAG_ENCRYPTED = 0x00000002;
    static constexpr uint32_t FLAG_URGENT = 0x00000004;
    
    bool isValid() const {
        return magic == MAGIC && version == IPC_CURRENT_VERSION;
    }
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

#### Feature Flags

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:47-56

constexpr uint32_t IPC_FEATURE_PREPARED_STATEMENTS = 0x00000001;
constexpr uint32_t IPC_FEATURE_COPY_STREAMING      = 0x00000002;
constexpr uint32_t IPC_FEATURE_NOTIFICATIONS       = 0x00000004;
constexpr uint32_t IPC_FEATURE_CANCEL              = 0x00000008;
constexpr uint32_t IPC_FEATURE_BINARY_RESULTS      = 0x00000010;
constexpr uint32_t IPC_FEATURE_COMPRESSION         = 0x00000020;
constexpr uint32_t IPC_FEATURE_ENCRYPTION          = 0x00000040;
constexpr uint32_t IPC_FEATURE_BATCH_EXECUTION     = 0x00000080;
```

#### STARTUP Payload Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:189-197

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
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:199-204

struct IPCReadyPayload {
    uint32_t session_id;      // Assigned session ID
    uint32_t server_features; // Supported server features
    char server_version[32];  // Server version string
};
// Total: 40 bytes
```

#### Session Statistics

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_server.h

struct SessionStats {
    uint64_t start_time_ms;          // Session start timestamp
    uint64_t last_activity_ms;       // Last message timestamp
    uint32_t messages_sent;          // Messages sent count
    uint32_t messages_received;      // Messages received count
    uint32_t queries_executed;       // Queries executed count
    uint32_t transactions_started;   // Transactions started
    uint32_t errors;                 // Error count
    uint64_t bytes_sent;             // Bytes sent
    uint64_t bytes_received;         // Bytes received
};
```

### Interface Contracts

#### Function: `IPCServer::start()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/ipc_server.cpp:567

core::Status IPCServer::start(core::ErrorContext* ctx);
```

**Preconditions:**
- Server is not already running
- Handler is configured
- Endpoint is available (not in use)

**Postconditions:**
- Listener is bound and listening
- Worker threads are started
- Accept thread is running
- `running_` flag is true

**Error Handling:**
- Returns IO_ERROR if socket creation fails
- Returns ALREADY_EXISTS if endpoint is in use

#### Function: `IPCServer::createSession()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/ipc_server.cpp:805

uint32_t IPCServer::createSession(std::unique_ptr<IPCChannel> channel);
```

**Preconditions:**
- Channel is connected and valid
- Server is running

**Postconditions:**
- Session is added to sessions_ map
- Session ID is assigned (monotonically increasing)
- Session state is INITIALIZING
- Session read loop thread is spawned

**Thread Safety:**
- Thread-safe via sessions_mutex_

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
3. Calculate agreed_features = client_features & server_features
4. Send READY response with:
   - session_id (assigned unique ID)
   - server_features (capabilities)
   - server_version (e.g., "1.0.0-alpha1")
5. Transition to ACTIVE state

#### Function: `IPCServer::destroySession()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/ipc/ipc_server.cpp:818

void IPCServer::destroySession(uint32_t session_id);
```

**Preconditions:**
- Session exists in sessions_ map

**Postconditions:**
- Session state set to CLOSING
- Session shutdown initiated
- Session removed from sessions_ map
- Statistics updated

**Thread Safety:**
- Thread-safe via sessions_mutex_

### Algorithms

#### Algorithm: Session Handshake

```
Input:  Client connection on IPC channel
Output: Established session or error

1. Create IPCSession with unique session ID
2. Transition state to INITIALIZING
3. Start session reader thread
4. Wait for STARTUP message from client
5. Validate STARTUP payload:
   a. Check magic is 0x53424950 ('SBIP')
   b. Check version is 0x0101 (v1.1)
   c. Extract feature_flags, database, user
6. Transition state to NEGOTIATING
7. Authenticate user via handler_->onAttach()
8. If authentication fails:
   a. Send ERROR_RESPONSE
   b. Close connection
   c. Return error
9. Calculate agreed_features = client_features & server_features
10. Send READY message with:
    - session_id (assigned unique ID)
    - server_features (capabilities)
    - server_version (e.g., "1.0.0-alpha1")
11. Transition state to ACTIVE
12. Return success
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

#### Algorithm: Session Maintenance

```
Input:  IPCServer with active sessions
Output: Healthy session pool

Every 60 seconds:
1. For each session in sessions_:
   a. Check last_activity_ms
   b. If idle > idle_timeout_ms:
      - Send NOTICE (idle timeout warning)
      - Schedule session for cleanup
   c. If idle > max_idle_timeout_ms:
      - Initiate graceful shutdown
2. For sessions marked for cleanup:
   a. Send TERMINATE message
   b. Call destroySession()
3. Update server statistics
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
│   │        └── Timeout exceeded?
│   │            ├── Yes → Send ERROR, close connection
│   │            └── No → Continue waiting
│   └── Yes → Validate header
│
       Header valid?
       ├── No → Send ERROR_RESPONSE, close connection
       └── Yes → Check version
│
              Version supported?
              ├── No → Send version negotiation error
              │        └── Close connection
              └── Yes → Authenticate user
│
                     Authentication successful?
                     ├── No → Send AUTH_FAILED, close connection
                     └── Yes → Calculate agreed features
│
                            Send READY
                            Transition to ACTIVE
```

#### Session Cleanup Decision Tree

```
Session cleanup triggered
│
├── State is EXECUTING?
│   ├── Yes → Signal cancellation
│   │        └── Wait for completion (5s timeout)
│   │            ├── Timeout → Force close
│   │            └── Complete → Continue
│   └── No → Continue
│
├── Active transaction?
│   ├── Yes → Rollback transaction
│   └── No → Continue
│
├── Prepared statements exist?
│   ├── Yes → Close all prepared statements
│   └── No → Continue
│
├── Active portals?
│   ├── Yes → Close all portals
│   └── No → Continue
│
├── COPY in progress?
│   ├── Yes → Terminate COPY
│   └── No → Continue
│
Disconnect channel
Transition to CLOSED
Remove from registry
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
   - Verification: State machine assertions

4. **Message Size Limit**: Payload MUST NOT exceed 1MB
   - Verification: `length <= IPC_MAX_MESSAGE_SIZE` (1MB)
   - Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h:39`

5. **Timestamp Monotonicity**: Message timestamps SHOULD increase monotonically
   - Used for detecting delayed/reordered messages
   - Warning if timestamp < last_timestamp

6. **Session ID Uniqueness**: Each session MUST have a unique session_id
   - Session IDs are monotonically increasing
   - Never reused within server lifetime

7. **Feature Negotiation**: Agreed features MUST be subset of both client and server features
   - Verification: `agreed = client & server`

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `INVALID_ARGUMENT` | Malformed STARTUP payload | Send ERROR_RESPONSE, close |
| `CONNECTION_FAILURE` | Channel disconnected | Transition to CLOSING |
| `NOT_SUPPORTED` | Unknown message type | Send ERROR_RESPONSE |
| `AUTH_FAILURE` | Authentication failed | Send ERROR_RESPONSE, close |
| `TIMEOUT` | Handshake timeout | Close connection |
| `PROTOCOL_VIOLATION` | Invalid state transition | Close connection |
| `RESOURCE_BUSY` | Session limit reached | Send ERROR_RESPONSE, close |

## Resource Limits

| Limit | Default | Description |
|-------|---------|-------------|
| max_sessions | 1000 | Maximum concurrent sessions |
| idle_timeout | 300s | Idle session timeout |
| max_idle_timeout | 3600s | Maximum idle before forced close |
| startup_timeout | 30s | Handshake completion timeout |
| message_timeout | 60s | Message response timeout |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_ipc_server.cpp` | Server lifecycle, session management |
| `tests/unit/test_ipc_contract.cpp` | Message serialization, header validation |
| `tests/unit/test_ipc_policy.cpp` | Feature negotiation, policy enforcement |
| `tests/unit/test_session_limits.cpp` | Resource limit enforcement |

## Related Specifications

- [session_states.md](./session_states.md) - Detailed state machine documentation
- [connection_handshake.md](./connection_handshake.md) - Handshake protocol details
- [session_termination.md](./session_termination.md) - Termination procedures
- [wire_protocol.md](./wire_protocol.md) - SBWP frame format
- [parser_agent_contract.md](./parser_agent_contract.md) - Parser agent protocol

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| IPC | Inter-Process Communication |
| Session | Logical connection between client and server |
| Attachment | Database connection context within a session |
| Portal | Cursor for executing prepared statements |
| Feature Flags | Bitmask indicating protocol capabilities |
| Agreed Features | Intersection of client and server capabilities |

### References

- `/home/dcalford/CliWork/ScratchBird/src/ipc/ipc_server.cpp` - IPC server implementation
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/ipc/ipc_contract_v1_1.h` - IPC contract definitions

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.1.0 | 2026-03-08 | Added session maintenance, resource limits | ScratchBird Engineering |
| 1.0.0 | 2026-03-01 | Initial specification | ScratchBird Engineering |
