# ScratchBird Local Server Architecture Implementation Plan

**Date:** November 23, 2025
**Priority:** PRIORITY 2.5 (After improvements, before CLI tools)
**Estimated Effort:** 120-160 hours (3-4 weeks)
**Prerequisite For:** Command-line tools (sb_isql, sb_verify, sb_backup, sb_security)

---

## Executive Summary

This document defines the implementation plan for transitioning ScratchBird from an embedded database to a local client-server architecture. This is **mandatory** before CLI tools can be implemented, as the tools need to connect to a running server process rather than directly embedding the database engine.

**Key Change**: Database files will be opened exclusively by a server process (`sb_server`), and all clients (including CLI tools) will connect via Inter-Process Communication (IPC).

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [IPC Method Selection](#ipc-method-selection)
3. [Wire Protocol Design](#wire-protocol-design)
4. [Server Implementation](#server-implementation)
5. [Client Library](#client-library)
6. [Auto-Start Mechanism](#auto-start-mechanism)
7. [Security & Authentication](#security--authentication)
8. [Implementation Phases](#implementation-phases)
9. [Testing Strategy](#testing-strategy)
10. [Migration Path to Alpha 3](#migration-path-to-alpha-3)

---

## Architecture Overview

### Current State (Embedded)

```
┌─────────────────┐
│  Application    │
│  (main.cpp)     │
│                 │
│  ┌───────────┐  │
│  │ ScratchBird │ │  Direct function calls
│  │   Engine    │ │  (embedded library)
│  └───────────┘  │
│                 │
│  Database files │
└─────────────────┘
```

### Target State (Local Server)

```
┌──────────────┐      IPC       ┌──────────────┐
│ CLI Tool     │ ←─────────────→ │  sb_server   │
│ (sb_isql)    │   Unix Socket   │              │
└──────────────┘    Named Pipe   │ ┌──────────┐ │
                    TCP localhost │ │ScratchBird│ │
┌──────────────┐                  │ │  Engine  │ │
│ CLI Tool     │ ←─────────────→ │ └──────────┘ │
│ (sb_verify)  │                  │              │
└──────────────┘                  │ Database files│
                                  │ (exclusive)  │
┌──────────────┐                  └──────────────┘
│ CLI Tool     │ ←─────────────→
│ (sb_backup)  │
└──────────────┘
```

### Key Benefits

1. **Exclusive File Access**: Only server touches database files (no corruption from concurrent access)
2. **Centralized Engine**: Single process manages all resources (buffer pool, transactions, locks)
3. **Multi-Client**: Multiple tools can connect simultaneously
4. **Auto-Start**: Clients automatically start server if needed
5. **Natural Evolution**: Clean migration path to network protocols (Alpha 3)

---

## IPC Method Selection

### Platform-Specific Primary Methods

#### Linux / macOS: Unix Domain Sockets

**Socket Path:** `/tmp/scratchbird-{database-name}.sock`

**Advantages:**
- 25-50% faster than TCP for local connections
- No kernel network stack overhead
- Filesystem-based permissions (0600 for user-only access)
- Peer credential passing (client authentication)
- Battle-tested (PostgreSQL's default local method)

**Implementation:**
```cpp
// Server side
int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
struct sockaddr_un addr;
addr.sun_family = AF_UNIX;
strncpy(addr.sun_path, "/tmp/scratchbird-mydb.sock", sizeof(addr.sun_path)-1);
bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
listen(server_fd, SOMAXCONN);
chmod(addr.sun_path, 0600);  // User-only access

// Client side
int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
connect(client_fd, (struct sockaddr*)&addr, sizeof(addr));
```

**Cleanup**: Server must `unlink()` socket file on shutdown

---

#### Windows: Named Pipes

**Pipe Name:** `\\.\pipe\scratchbird-{database-name}`

**Advantages:**
- Native Windows IPC (no third-party dependencies)
- Good performance
- Built-in security (ACLs)
- SQL Server's default local method

**Implementation:**
```cpp
// Server side
HANDLE hPipe = CreateNamedPipe(
    "\\\\.\\pipe\\scratchbird-mydb",
    PIPE_ACCESS_DUPLEX,
    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
    PIPE_UNLIMITED_INSTANCES,
    8192, 8192, 0, NULL
);

// Client side
HANDLE hPipe = CreateFile(
    "\\\\.\\pipe\\scratchbird-mydb",
    GENERIC_READ | GENERIC_WRITE,
    0, NULL, OPEN_EXISTING, 0, NULL
);
```

---

#### Fallback: TCP/IP Localhost

**Address:** `127.0.0.1:5433` (default port, configurable)

**Advantages:**
- Cross-platform identical code
- Standard debugging tools (tcpdump, wireshark, netstat)
- Natural upgrade to network access (Alpha 3)
- Well-understood semantics

**Disadvantages:**
- Higher overhead (~2x Unix sockets)
- Kernel network stack involvement
- Port conflicts possible

**When to Use:**
- Debugging IPC issues
- Development/testing on mixed platforms
- Explicit user preference (config file)

**Implementation:**
```cpp
// Server side
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
struct sockaddr_in addr;
addr.sin_family = AF_INET;
addr.sin_addr.s_addr = inet_addr("127.0.0.1");
addr.sin_port = htons(5433);
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
listen(server_fd, SOMAXCONN);

// Client side
int client_fd = socket(AF_INET, SOCK_STREAM, 0);
connect(client_fd, (struct sockaddr*)&addr, sizeof(addr));
```

---

### Selection Logic

```cpp
ConnectionMethod selectIPCMethod() {
#ifdef __linux__
    return ConnectionMethod::UNIX_SOCKET;
#elif defined(__APPLE__)
    return ConnectionMethod::UNIX_SOCKET;
#elif defined(_WIN32)
    return ConnectionMethod::NAMED_PIPE;
#else
    return ConnectionMethod::TCP_LOCALHOST;  // Fallback
#endif
}
```

**Override via Environment Variable:**
```bash
export SCRATCHBIRD_IPC_METHOD=tcp  # Force TCP localhost
```

---

## Wire Protocol Design

### Protocol Version

**Version:** 1.0 (Alpha 1 Local Protocol)

**Future**: Wire protocol compatibility matrix for Alpha 3 (PostgreSQL, MySQL, TDS)

---

### Message Format

All messages use a simple binary format:

```
┌────────────────────────────────────────────────────┐
│  Magic (4 bytes)  │  'S' 'B' 'D' 'B'               │
├────────────────────────────────────────────────────┤
│  Protocol Version │  uint16 (1.0 = 0x0100)         │
├────────────────────────────────────────────────────┤
│  Message Type     │  uint8 (see below)             │
├────────────────────────────────────────────────────┤
│  Flags            │  uint8 (reserved)              │
├────────────────────────────────────────────────────┤
│  Payload Length   │  uint32 (N bytes)              │
├────────────────────────────────────────────────────┤
│  Payload          │  N bytes (message-dependent)   │
└────────────────────────────────────────────────────┘
```

**Total Header Size:** 12 bytes

---

### Message Types

```cpp
enum class MessageType : uint8_t {
    // Connection lifecycle
    CONNECT_REQUEST = 0x01,
    CONNECT_RESPONSE = 0x02,
    DISCONNECT = 0x03,

    // Authentication
    AUTH_REQUEST = 0x10,
    AUTH_RESPONSE = 0x11,

    // Query execution
    QUERY = 0x20,
    QUERY_RESULT = 0x21,
    QUERY_ERROR = 0x22,

    // Prepared statements
    PREPARE = 0x30,
    EXECUTE = 0x31,
    CLOSE_STATEMENT = 0x32,

    // Transactions
    BEGIN_TRANSACTION = 0x40,
    COMMIT = 0x41,
    ROLLBACK = 0x42,
    SAVEPOINT = 0x43,

    // Result set streaming
    ROW_DATA = 0x50,
    END_OF_RESULTS = 0x51,

    // Administrative
    SHUTDOWN = 0x60,
    PING = 0x61,
    PONG = 0x62
};
```

---

### Message Payloads

#### CONNECT_REQUEST

```cpp
struct ConnectRequest {
    uint32_t protocol_version;  // 0x00010000 for 1.0
    char database_name[256];    // Null-terminated
    char client_name[64];       // "sb_isql", "sb_verify", etc.
    uint32_t client_pid;        // For logging
};
```

**Response:** `CONNECT_RESPONSE` with `session_id` (UUID)

---

#### AUTH_REQUEST

```cpp
struct AuthRequest {
    uint8_t session_id[16];     // UUID from CONNECT_RESPONSE
    char username[64];          // Null-terminated
    uint8_t password_hash[60];  // BCrypt hash (client-side pre-hashed)
};
```

**Response:** `AUTH_RESPONSE` with success/failure status

---

#### QUERY

```cpp
struct QueryMessage {
    uint8_t session_id[16];     // UUID
    uint32_t query_length;      // N bytes
    char query_text[N];         // SQL statement (UTF-8)
};
```

**Response:** `QUERY_RESULT` or `QUERY_ERROR`

---

#### QUERY_RESULT

```cpp
struct QueryResult {
    uint32_t num_columns;
    // For each column:
    //   uint16_t name_length
    //   char name[name_length]
    //   uint8_t type_oid
    uint32_t num_rows;          // Total (or -1 for streaming)

    // Followed by ROW_DATA messages
};
```

---

#### ROW_DATA

```cpp
struct RowData {
    uint16_t column_count;
    // For each column:
    //   int32_t value_length (-1 = NULL)
    //   uint8_t value[value_length]
};
```

**Final Message:** `END_OF_RESULTS`

---

#### QUERY_ERROR

```cpp
struct QueryError {
    uint32_t error_code;        // SQLSTATE-compatible
    uint16_t message_length;
    char error_message[message_length];
    uint16_t detail_length;
    char error_detail[detail_length];
};
```

---

### Data Type Encoding

All values are sent in binary format (little-endian):

| ScratchBird Type | Wire Format |
|------------------|-------------|
| INT32 | 4 bytes (int32_t) |
| INT64 | 8 bytes (int64_t) |
| FLOAT | 4 bytes (IEEE 754) |
| DOUBLE | 8 bytes (IEEE 754) |
| DECIMAL | String representation |
| VARCHAR | Length-prefixed UTF-8 |
| TIMESTAMP | 8 bytes (microseconds since epoch) |
| BOOLEAN | 1 byte (0 or 1) |
| NULL | Length = -1 |

**Large Objects (TOAST):**
- Sent in chunks (8KB per ROW_DATA message)
- Client reassembles

---

## Server Implementation

### Server Process: `sb_server`

**Responsibilities:**
1. Open database files exclusively
2. Initialize ScratchBird engine
3. Listen for client connections
4. Authenticate clients
5. Execute queries
6. Manage transactions
7. Handle graceful shutdown

---

### Main Server Loop

```cpp
int main(int argc, char* argv[]) {
    // Parse arguments
    Config config = parseArgs(argc, argv);

    // Initialize database engine
    Database db;
    Status s = db.open(config.database_path, OpenMode::EXCLUSIVE);
    if (!s.ok()) {
        fprintf(stderr, "Failed to open database: %s\n", s.message());
        return 1;
    }

    // Create IPC listener
    IPCServer ipc_server(config.ipc_method, config.database_name);
    s = ipc_server.listen();
    if (!s.ok()) {
        fprintf(stderr, "Failed to start IPC server: %s\n", s.message());
        return 1;
    }

    // Write PID file for auto-start detection
    writePIDFile(config.database_name, getpid());

    // Main event loop
    while (!shutdown_requested) {
        ClientConnection* conn = ipc_server.acceptConnection();
        if (conn) {
            // Spawn worker thread
            std::thread worker(&handleClient, conn, &db);
            worker.detach();
        }
    }

    // Graceful shutdown
    ipc_server.close();
    db.close();
    removePIDFile(config.database_name);

    return 0;
}
```

---

### Client Handler

```cpp
void handleClient(ClientConnection* conn, Database* db) {
    SessionContext session;
    bool authenticated = false;

    while (conn->isOpen()) {
        Message msg = conn->receiveMessage();

        switch (msg.type) {
            case MessageType::CONNECT_REQUEST:
                session.id = generateUUID();
                conn->sendConnectResponse(session.id);
                break;

            case MessageType::AUTH_REQUEST:
                authenticated = authenticateUser(msg.username, msg.password_hash, db);
                conn->sendAuthResponse(authenticated);
                if (authenticated) {
                    session.user_id = getUserID(msg.username, db);
                }
                break;

            case MessageType::QUERY:
                if (!authenticated) {
                    conn->sendError("Not authenticated");
                    break;
                }
                executeQuery(msg.query_text, &session, db, conn);
                break;

            case MessageType::BEGIN_TRANSACTION:
                if (!authenticated) {
                    conn->sendError("Not authenticated");
                    break;
                }
                session.xid = db->beginTransaction(session.user_id);
                conn->sendSuccess();
                break;

            case MessageType::COMMIT:
                db->commitTransaction(session.xid);
                conn->sendSuccess();
                break;

            case MessageType::DISCONNECT:
                conn->close();
                break;
        }
    }

    // Cleanup
    if (session.xid != INVALID_XID) {
        db->rollbackTransaction(session.xid);
    }
    delete conn;
}
```

---

### Query Execution

```cpp
void executeQuery(const std::string& sql,
                 SessionContext* session,
                 Database* db,
                 ClientConnection* conn) {
    ErrorContext ctx;

    // Parse SQL
    Parser parser;
    ASTNode* ast = parser.parse(sql, &ctx);
    if (!ast) {
        conn->sendQueryError(ctx);
        return;
    }

    // Generate bytecode
    BytecodeGenerator gen;
    BytecodeProgram program = gen.generate(ast, &ctx);
    if (!program.isValid()) {
        conn->sendQueryError(ctx);
        return;
    }

    // Execute
    Executor executor(db, session->user_id, session->xid);
    ResultSet results;
    Status s = executor.execute(program, &results, &ctx);

    if (!s.ok()) {
        conn->sendQueryError(ctx);
        return;
    }

    // Send results
    conn->sendQueryResult(results);

    // Stream rows
    while (results.hasMore()) {
        Row row = results.next();
        conn->sendRowData(row);
    }

    conn->sendEndOfResults();
}
```

---

### Configuration

**Config File:** `~/.scratchbird/server.conf` (optional)

```ini
[server]
# IPC method: unix, pipe, tcp, auto (default: auto)
ipc_method = auto

# TCP port (if using TCP)
tcp_port = 5433

# Maximum connections
max_connections = 100

# Connection timeout (seconds)
connection_timeout = 60

# Log level: debug, info, warning, error
log_level = info

# Log file path
log_file = ~/.scratchbird/server.log

[security]
# Require authentication (default: true)
require_auth = true

# Allow empty passwords (default: false)
allow_empty_passwords = false

# Session timeout (seconds)
session_timeout = 3600
```

---

## Client Library

### libscratchbird_client

Shared library for all CLI tools to use.

**Files:**
- `include/scratchbird/client/connection.h`
- `src/client/connection.cpp`

---

### C++ API

```cpp
namespace scratchbird {
namespace client {

class Connection {
public:
    Connection();
    ~Connection();

    // Connect to local server (auto-start if needed)
    Status connect(const std::string& database_name,
                  const std::string& username,
                  const std::string& password);

    // Execute query
    Status executeQuery(const std::string& sql, ResultSet* results);

    // Prepared statements
    Status prepare(const std::string& sql, PreparedStatement* stmt);
    Status execute(PreparedStatement* stmt, ResultSet* results);

    // Transactions
    Status beginTransaction();
    Status commit();
    Status rollback();
    Status savepoint(const std::string& name);

    // Disconnect
    void disconnect();

    // Server control
    static Status startServer(const std::string& database_name);
    static Status stopServer(const std::string& database_name);
    static bool isServerRunning(const std::string& database_name);

private:
    void* impl_;  // Pimpl idiom for ABI stability
};

class ResultSet {
public:
    size_t getColumnCount() const;
    std::string getColumnName(size_t index) const;
    TypeOID getColumnType(size_t index) const;

    bool next();

    bool isNull(size_t column) const;
    int32_t getInt32(size_t column) const;
    int64_t getInt64(size_t column) const;
    double getDouble(size_t column) const;
    std::string getString(size_t column) const;
    // ... other getters
};

} // namespace client
} // namespace scratchbird
```

---

### Usage Example

```cpp
#include <scratchbird/client/connection.h>

using namespace scratchbird::client;

int main() {
    Connection conn;

    // Auto-starts server if not running
    Status s = conn.connect("mydb", "admin", "password123");
    if (!s.ok()) {
        fprintf(stderr, "Connection failed: %s\n", s.message());
        return 1;
    }

    // Execute query
    ResultSet results;
    s = conn.executeQuery("SELECT * FROM users WHERE active = true", &results);
    if (!s.ok()) {
        fprintf(stderr, "Query failed: %s\n", s.message());
        return 1;
    }

    // Process results
    while (results.next()) {
        int64_t id = results.getInt64(0);
        std::string name = results.getString(1);
        printf("%lld: %s\n", id, name.c_str());
    }

    conn.disconnect();
    return 0;
}
```

---

## Auto-Start Mechanism

### PID File

**Location:** `/tmp/scratchbird-{database-name}.pid`

**Contents:**
```
12345
```
(Server process PID)

---

### Auto-Start Logic

```cpp
bool Connection::isServerRunning(const std::string& database_name) {
    std::string pid_file = "/tmp/scratchbird-" + database_name + ".pid";

    // Read PID file
    std::ifstream f(pid_file);
    if (!f.is_open()) {
        return false;  // No PID file
    }

    pid_t pid;
    f >> pid;
    f.close();

    // Check if process exists
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) {
        return false;  // Process doesn't exist
    }
    DWORD exitCode;
    GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);
    return (exitCode == STILL_ACTIVE);
#else
    // Unix: send signal 0 (doesn't actually send, just checks existence)
    return (kill(pid, 0) == 0);
#endif
}

Status Connection::startServer(const std::string& database_name) {
    if (isServerRunning(database_name)) {
        return Status::ok();  // Already running
    }

    // Find sb_server executable
    std::string server_path = findExecutable("sb_server");
    if (server_path.empty()) {
        return Status::error("sb_server not found in PATH");
    }

    // Start server as background process
#ifdef _WIN32
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    std::string cmdline = server_path + " --database=" + database_name + " --daemon";

    if (!CreateProcess(NULL, (LPSTR)cmdline.c_str(),
                      NULL, NULL, FALSE, CREATE_NO_WINDOW,
                      NULL, NULL, &si, &pi)) {
        return Status::error("Failed to start server");
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execl(server_path.c_str(), "sb_server",
              "--database", database_name.c_str(),
              "--daemon", NULL);
        exit(1);  // execl failed
    } else if (pid < 0) {
        return Status::error("fork() failed");
    }
#endif

    // Wait for server to start (poll for PID file)
    for (int i = 0; i < 50; i++) {  // 5 seconds max
        if (isServerRunning(database_name)) {
            return Status::ok();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return Status::error("Server failed to start within timeout");
}
```

---

### Connection Sequence

```
Client                          Server
  |                               |
  |  1. Check if server running   |
  |     (read PID file)           |
  |                               |
  |  2. If not, fork/exec         |
  |     sb_server --daemon        |
  |                               |
  |  3. Wait for PID file         |
  |                               |
  |  4. Connect to IPC socket     |
  |  ─────────────────────────>   |
  |                               |
  |  5. Send CONNECT_REQUEST      |
  |  ─────────────────────────>   |
  |                               |
  |  6. Receive session_id        |
  |  <─────────────────────────   |
  |                               |
  |  7. Send AUTH_REQUEST         |
  |  ─────────────────────────>   |
  |                               |
  |  8. Receive auth success      |
  |  <─────────────────────────   |
  |                               |
  |  9. Send QUERY                |
  |  ─────────────────────────>   |
  |                               |
  |  10. Receive results          |
  |  <─────────────────────────   |
```

---

## Security & Authentication

### Client Authentication Flow

1. **Connection**: Client connects via IPC
2. **Hello**: Client sends CONNECT_REQUEST
3. **Session**: Server generates UUID session_id
4. **Credentials**: Client sends AUTH_REQUEST with username + BCrypt hash
5. **Verification**: Server checks against `pg_users` catalog table
6. **Authorization**: Server loads user permissions, RLS policies
7. **Session Context**: All queries run with user's permissions

---

### Security Considerations

**Unix Domain Sockets:**
- Socket file permissions: `0600` (user-only)
- Peer credential passing: `SO_PEERCRED` (Linux) / `LOCAL_PEERCRED` (BSD)
- Verify client UID/GID matches server

**Named Pipes (Windows):**
- ACLs restrict to current user
- Token impersonation for client identity

**TCP Localhost:**
- Bind to `127.0.0.1` only (not `0.0.0.0`)
- Still require username/password authentication
- Future: Add TLS support (Alpha 3)

**Password Handling:**
- Never send plaintext passwords
- Client pre-hashes with BCrypt
- Server verifies hash against stored hash
- Session timeout enforced

---

## Implementation Phases

### Phase 1: IPC Infrastructure (40-50 hours) ✅ COMPLETE

**Completed:** November 27, 2025

**Deliverables:**
- [x] `include/scratchbird/server/ipc_server.h` - IPC abstraction layer
- [x] `src/server/ipc_unix.cpp` - Unix domain socket implementation
- [x] `src/server/ipc_windows.cpp` - Named pipe implementation (Windows)
- [x] `src/server/ipc_tcp.cpp` - TCP localhost fallback
- [x] `src/server/ipc_common.cpp` - Platform detection and factory methods
- [x] `tests/unit/test_ipc_server.cpp` - Unit tests (22 tests, all passing)
- [x] CMake integration (`scratchbird_server` library)

**Features Implemented:**
- Platform-automatic IPC method selection (Unix sockets on Linux/macOS, Named pipes on Windows, TCP fallback)
- IPCServer/IPCClient abstract interfaces with factory methods
- IPCConnection for read/write/close with timeout support
- Peer credential retrieval (Unix SO_PEERCRED, Windows GetNamedPipeClientProcessId)
- Connection statistics tracking
- PID file management for server detection
- Server running detection (isServerRunning())

**Tests Passing:**
- Platform detection tests
- Path generation tests (socket/pipe paths)
- TCP server/client integration tests
- Unix socket tests (Linux/macOS)
- Data transfer tests (including 64KB large data)
- Connection stats tests

---

### Phase 2: Wire Protocol (30-40 hours) ✅ COMPLETE

**Completed:** November 27, 2025

**Deliverables:**
- [x] `include/scratchbird/protocol/wire_protocol.h` - Wire protocol definitions and codec
- [x] `src/protocol/wire_protocol.cpp` - Full implementation (~1000 lines)
- [x] `tests/unit/test_wire_protocol.cpp` - 37 unit tests (all passing)
- [x] Message serialization/deserialization (little-endian binary)
- [x] Message type handlers (22 message types)
- [x] Result set streaming (ROW_DESCRIPTION, ROW_DATA, END_OF_RESULTS)
- [x] Error handling and propagation (QUERY_ERROR, PROTOCOL_ERROR)
- [x] Protocol version negotiation (CONNECT_REQUEST/RESPONSE)

**Protocol Features:**
- 12-byte message header (magic, version, type, flags, length)
- UUID v4 session IDs
- Typed column descriptions with wire types
- Transaction message support (BEGIN, COMMIT, ROLLBACK)
- Ping/Pong keepalive
- Administrative commands (shutdown, status)

---

### Phase 3: Server Implementation (40-50 hours) ✅ COMPLETE

**Completed:** November 27, 2025

**Deliverables:**
- [x] `include/scratchbird/server/server_session.h` - Session management header
- [x] `include/scratchbird/server/scratchbird_server.h` - Main server class
- [x] `src/server/server_session.cpp` - Session and SessionManager implementation
- [x] `src/server/scratchbird_server.cpp` - Server lifecycle, accept loop, client handling
- [x] `src/server/sb_server_main.cpp` - Server executable with CLI argument parsing

**Features Implemented:**
- [x] `sb_server` main executable with `--create`, `--port`, `--verbose` flags
- [x] Connection listener (Unix sockets, TCP fallback)
- [x] Client handler (multi-threaded, thread-per-connection)
- [x] Session management (ServerSession, SessionManager classes)
- [x] Authentication integration (CatalogManager user lookup, password verification)
- [x] Query execution pipeline (Parser → BytecodeGenerator → Executor)
- [x] PID file management for server detection
- [x] Graceful shutdown with signal handling (SIGTERM, SIGINT, SIGHUP)
- [x] Transaction support (BEGIN, COMMIT, ROLLBACK via wire protocol)

**Architecture:**
- Thread-per-connection model (future: thread pool for high concurrency)
- Session UUID assignment on connection
- Query results streamed via ROW_DATA messages
- Error handling with QUERY_ERROR responses

---

### Phase 4: Client Library (20-30 hours) ✅ COMPLETE

**Completed:**
- [x] `libscratchbird_client` static library
- [x] Connection class (full implementation)
- [x] ResultSet class
- [x] PreparedStatement class
- [x] ConnectionPool class
- [x] Auto-start mechanism
- [x] Error handling
- [x] Connection pooling

**Deliverables:**
- `include/scratchbird/client/connection.h` ✅
- `src/client/connection.cpp` ✅
- Static library (`libscratchbird_client.a`) ✅

**Tests (95+ tests total):**
- [x] Connection open/close (7 tests)
- [x] Query execution (5 tests)
- [x] Result set iteration (3 tests)
- [x] Transaction management (6 tests)
- [x] Prepared statements (4 tests)
- [x] Connection pooling (4 tests)
- [x] Error handling (4 tests)
- [x] Integration tests (7 tests)

---

### Phase 5: Integration & Testing (10-20 hours)

**Week 4:**
- [ ] End-to-end integration tests
- [ ] Performance benchmarks
- [ ] Security audit (authentication, permissions)
- [ ] Cross-platform testing (Linux, macOS, Windows)
- [ ] Documentation

**Deliverables:**
- Integration test suite
- Performance benchmarks
- Security audit report
- User documentation

---

## Testing Strategy

### Unit Tests

- IPC method tests (connect, send, receive, close)
- Message codec tests (encode, decode, edge cases)
- Session management tests
- Authentication tests

### Integration Tests

- Client-server round-trip
- Multi-client concurrent access
- Large result sets
- Transaction isolation
- Error recovery

### Performance Tests

- Connection establishment latency
- Query throughput (queries/second)
- Result set streaming bandwidth
- Concurrent client scaling

### Security Tests

- Authentication bypass attempts
- Permission escalation attempts
- SQL injection (handled by existing parser)
- Session hijacking

### Platform Tests

- Linux (Ubuntu 20.04+, Fedora 35+)
- macOS (11.0+)
- Windows (WSL2, native Windows 10+)

---

## Migration Path to Alpha 3

### Alpha 1: Local Server (This Plan)

- Unix domain sockets / Named pipes / TCP localhost
- Simple binary wire protocol
- Authentication only
- Local connections only

### Alpha 3: Network Layer (Future)

**Upgrade Path:**
1. **Keep** IPC infrastructure (still used for local connections)
2. **Add** network listener (TCP with TLS)
3. **Implement** wire protocol adapters:
   - PostgreSQL wire protocol (libpq compatible)
   - MySQL wire protocol (MySQL client compatible)
   - TDS wire protocol (SQL Server client compatible)
   - ScratchBird native wire protocol
4. **Add** TLS/SSL encryption
5. **Add** network authentication (SCRAM-SHA-256, etc.)

**Code Reuse:**
- Message handling infrastructure
- Session management
- Query execution pipeline
- Authentication framework

**New Components:**
- Protocol adapters (PostgreSQL, MySQL, TDS)
- TLS/SSL layer
- Network address binding (not just localhost)
- Connection pooling
- Query cancellation over network

---

## File Structure

```
include/scratchbird/
├── server/
│   ├── ipc_server.h          # IPC abstraction
│   ├── session_manager.h     # Session tracking
│   └── server_config.h       # Configuration
├── client/
│   ├── connection.h          # Client API
│   └── result_set.h          # Result iteration
└── protocol/
    ├── wire_protocol.h       # Message definitions
    └── message_codec.h       # Serialization

src/
├── server/
│   ├── main.cpp              # sb_server entry point
│   ├── ipc_unix.cpp          # Unix domain sockets
│   ├── ipc_windows.cpp       # Named pipes
│   ├── ipc_tcp.cpp           # TCP localhost
│   ├── client_handler.cpp    # Per-client worker
│   ├── session_manager.cpp   # Session state
│   └── server_config.cpp     # Config parsing
├── client/
│   ├── connection.cpp        # Client implementation
│   └── result_set.cpp        # Result handling
└── protocol/
    ├── message_codec.cpp     # Encode/decode
    └── wire_protocol.cpp     # Message handling

tests/
├── integration/
│   ├── test_client_server.cpp
│   ├── test_multi_client.cpp
│   └── test_auto_start.cpp
└── unit/
    ├── test_ipc_unix.cpp
    ├── test_ipc_windows.cpp
    ├── test_message_codec.cpp
    └── test_session_manager.cpp
```

---

## Estimated Effort Summary

| Phase | Description | Effort | Status |
|-------|-------------|--------|--------|
| Phase 1 | IPC Infrastructure | 40-50 hours | ✅ COMPLETE |
| Phase 2 | Wire Protocol | 30-40 hours | ✅ COMPLETE |
| Phase 3 | Server Implementation | 40-50 hours | ✅ COMPLETE |
| Phase 4 | Client Library | 20-30 hours | ✅ COMPLETE (36 unit + 7 integration tests) |
| Phase 5 | Integration & Testing | 10-20 hours | ❌ Pending |
| **Total** | | **140-190 hours** | **~90% Complete** |

**Original estimate:** 140-190 hours (3.5-4.5 weeks)
**Remaining work:** 10-20 hours - Phase 5 (integration testing, docs)

**CLI Tools:** ✅ **100% COMPLETE** (November 28, 2025)
- All 4 CLI tools (sb_isql, sb_verify, sb_backup, sb_security) built and tested
- Located in `src/cli/` directory

---

## Dependencies

**Before This Work:**
- PRIORITY 1: Missing Functions (207-312 hours)
- PRIORITY 2: Improvement Opportunities (430-540 hours)
- Specifically: P0-3 (Security Audit Logging) should be done first

**After This Work:** ✅ **COMPLETE**
- ✅ Command-Line Tools (90-110 hours estimated, completed November 28, 2025)
  - ✅ sb_isql - Interactive SQL shell (~750 lines)
  - ✅ sb_verify - Database verification (~510 lines)
  - ✅ sb_backup - Backup/restore (~550 lines)
  - ✅ sb_security - Security administration (~800 lines)
  - All tools built and tested!

---

## Success Criteria

**MVP (Minimum Viable Product):**
1. ✅ `sb_server` starts and opens database exclusively
2. ✅ Client can connect via platform-appropriate IPC
3. ✅ Client can authenticate with username/password
4. ✅ Client can execute SELECT query
5. ✅ Client receives result set correctly
6. ✅ Multiple clients can connect simultaneously
7. ✅ Server shuts down gracefully
8. ✅ Auto-start works (client starts server if not running)

**Full Success:**
1. All above + transaction support (BEGIN, COMMIT, ROLLBACK)
2. Prepared statements
3. Large result set streaming (GB size)
4. Performance: <1ms connection latency, >10K qps
5. Security: Authentication required, session timeout enforced
6. Cross-platform: Works on Linux, macOS, Windows

---

**Document Version:** 1.2
**Last Updated:** November 28, 2025
**Next Review:** After Phase 5 (Integration Testing) completion
