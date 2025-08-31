# Phase 25: Y-Valve Framework

## Objective
Implement Y-Valve router that detects client types and routes to appropriate translators.

## Prerequisites
- Phase 24 complete (embedded engine fully functional)
- Native Firebird protocol working

## Tasks

### 25.1 Client Detection
```cpp
enum ClientType {
    Firebird,
    MySQL,
    PostgreSQL,
    MSSQL,
    MariaDB
};

class ClientDetector {
    ClientType detect_from_handshake(socket s) {
        // Read initial bytes
        // MySQL: capability flags
        // PostgreSQL: startup message
        // TDS: prelogin packet
        // Firebird: op_connect
    }
};
```

### 25.2 Y-Valve Router
```cpp
class YValve {
    map<ConnectionId, unique_ptr<Translator>> translators;
    EmbeddedEngine* engine;
    
    void handle_connection(socket client) {
        auto type = detect_client(client);
        auto translator = create_translator(type);
        translators[conn_id] = move(translator);
        
        // Route all requests through translator
        while (connected) {
            auto request = translator->read_request(client);
            auto native = translator->to_native(request);
            auto result = engine->execute(native);
            auto response = translator->from_native(result);
            translator->write_response(client, response);
        }
    }
};
```

### 25.3 Translator Interface
```cpp
class Translator {
    virtual Request read_request(socket) = 0;
    virtual NativeQuery to_native(Request) = 0;
    virtual Response from_native(Result) = 0;
    virtual void write_response(socket, Response) = 0;
    
    // State management
    virtual void set_session_var(string name, Value val) = 0;
    virtual void begin_transaction(IsolationLevel) = 0;
};
```

### 25.4 Connection Context
```cpp
struct ConnectionContext {
    ClientType type;
    UUID namespace_root;     // Client's view of schema
    map<string, Value> variables;  // Client-specific vars
    IsolationLevel isolation;
    CharacterSet charset;
    Collation collation;
};
```

## Files to Create
- `include/scratchbird/yvalve/yvalve.h`
- `src/yvalve/client_detector.cpp`
- `src/yvalve/router.cpp`
- `src/yvalve/translator_base.cpp`

## Validation Tests
```cpp
// Connect with MySQL client
auto mysql_conn = mysql_connect("localhost", 3306);
mysql_query(mysql_conn, "SELECT 1");

// Connect with psql
auto pg_conn = PQconnectdb("host=localhost port=5432");
PQexec(pg_conn, "SELECT 1");

// Both work through same Y-Valve
```

## Exit Criteria
- Y-Valve correctly identifies client types
- Routes requests to appropriate translators
- Maintains connection context
- Embedded engine processes all requests