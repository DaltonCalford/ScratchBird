# Phase 19: Network Protocol and Server

## Objective
Implement network server for remote connections.

## Prerequisites
- Phase 18 complete (permissions)

## Tasks

### 19.1 Server Architecture
```cpp
class Server {
    void start(uint16_t port);
    void stop();
    void handle_connection(int client_fd);
    void process_message(ClientMessage msg);
};
```

### 19.2 Wire Protocol
Message types:
- Startup (version, database, user)
- Query (SQL string)
- Parse (prepare statement)
- Bind (bind parameters)
- Execute (run prepared statement)
- Sync (end of message batch)

### 19.3 Result Streaming
- Send row description
- Stream data rows
- Handle large result sets
- Support cursors

### 19.4 Connection Pooling
- Reuse connections
- Limit concurrent connections
- Timeout idle connections

### 19.5 TLS Support
- Negotiate TLS upgrade
- Certificate verification
- Encrypted communication

## Files to Create/Modify
- `include/scratchbird/server.h`
- `src/server/server.cpp`
- `src/server/protocol.cpp`
- `src/server/connection.cpp`

## Validation Tests
```cpp
// Start server
Server server;
server.start(5432);

// Connect from client
auto conn = connect("localhost", 5432, "user", "pass");
assert(conn.is_connected());

// Execute query
auto result = conn.execute("SELECT 1");
assert(result.rows[0][0] == "1");

// Prepared statements
auto stmt = conn.prepare("SELECT * FROM users WHERE id = ?");
result = conn.execute(stmt, {42});

// Large result set
conn.execute("CREATE TABLE large (id INTEGER, data TEXT)");
// Insert 100000 rows
result = conn.execute("SELECT * FROM large");
assert(result.rows.size() == 100000);

// TLS connection
auto secure_conn = connect("localhost", 5432, "user", "pass", 
                          ConnectOptions{.require_ssl = true});
assert(secure_conn.is_encrypted());
```

## Exit Criteria
- Server accepts remote connections
- Protocol handles all message types
- Large results stream efficiently
- TLS encryption works