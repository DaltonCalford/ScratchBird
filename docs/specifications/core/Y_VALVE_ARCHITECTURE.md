# Listener/Parser Pool Architecture (Legacy Y-Valve Spec)

---

## IMPLEMENTATION STATUS: 🟢 IMPLEMENTED (Listener/Pool) / 🔴 LEGACY (Y-Valve)

**Current Alpha Implementation (Authoritative):**
- Per-protocol listeners + parser pools
- Protocol adapters for native/PG/MySQL/Firebird
- Parser processes per connection (sb_parser_*)
- Listener control plane handles routing and pool lifecycle

**Legacy Note:** The historical "Y-Valve" router is superseded by the listener/pool
architecture. The remainder of this document preserves the original Y-Valve
design for reference only.

---

## Related Specs

- [Component Model and Responsibilities](../catalog/COMPONENT_MODEL_AND_RESPONSIBILITIES.md)

Reuse artifacts: [Component Model Diagrams](../../diagrams/component_model_diagrams.md),
[Component Responsibility Matrix](../../diagrams/component_responsibility_matrix.md).

---

## Current Architecture (Alpha)

ScratchBird now uses **listener + parser pool** processes instead of a central
Y-Valve router. The listener acts as the control plane for routing and lifecycle
management; parser processes own the data plane after socket handoff.

```
┌─────────────────────────────────────────────────────────────────┐
│            Network Layer (Per-Protocol Listeners)               │
│  Native │ PostgreSQL │ MySQL │ Firebird                          │
└─────────────┬───────────────────────────────────────────────────┘
              │ Socket Handoff
              ▼
┌─────────────────────────────────────────────────────────────────┐
│        Listener Control Plane + Parser Pool                     │
│  - protocol routing                                              │
│  - spawn/recycle workers                                         │
│  - health checks, metrics                                        │
└─────────────┬───────────────────────────────────────────────────┘
              │ Parser Interface
              ▼
┌─────────────────────────────────────────────────────────────────┐
│            Parser Process (Per Connection)                       │
│  Protocol wire handling + SQL -> SBLR                            │
└─────────────┬───────────────────────────────────────────────────┘
              │ SBLR
              ▼
┌─────────────────────────────────────────────────────────────────┐
│              ScratchBird Engine                                  │
└─────────────────────────────────────────────────────────────────┘
```

Terminology mapping:
- **Y-Valve** (legacy) => **Listener control plane + parser pool**
- **Y-Valve router** => **Listener routing + parser selection**

Authoritative spec for this model:
- `ScratchBird/docs/specifications/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`

## Legacy Y-Valve Design (Reference)

The Y-Valve (legacy design) is a universal connection router and protocol
abstraction layer. Unlike Firebird's Y-Valve (which simply routes between
embedded/server modes), the historical ScratchBird Y-Valve design described:

1. Accepts connections from any supported database client
2. Detects and validates the protocol
3. Instantiates the appropriate parser/translator
4. Maintains protocol state and context
5. Translates between external protocols and internal BLR
6. Manages connection lifecycle and resources

## Architectural Position (Legacy)

```
┌─────────────────────────────────────────────────┐
│            Network Layer (Listeners)             │
│  PostgreSQL │ MySQL │ Firebird │ TDS │ Native   │
└─────────────┬───────────────────────────────────┘
              │ Connection Handoff
              ▼
┌─────────────────────────────────────────────────┐
│              Y-Valve Router                      │
│  ┌──────────────────────────────────────────┐   │
│  │ Protocol Detection & Classification      │   │
│  ├──────────────────────────────────────────┤   │
│  │ Parser Factory & Management              │   │
│  ├──────────────────────────────────────────┤   │
│  │ Connection Context Management            │   │
│  ├──────────────────────────────────────────┤   │
│  │ Protocol State Machines                  │   │
│  └──────────────────────────────────────────┘   │
└─────────────┬───────────────────────────────────┘
              │ Parser Interface
              ▼
┌─────────────────────────────────────────────────┐
│            Parser Implementations                │
│  PostgreSQL │ MySQL │ Firebird │ TDS │ Native   │
└─────────────┬───────────────────────────────────┘
              │ BLR
              ▼
┌─────────────────────────────────────────────────┐
│              ScratchBird Engine                  │
└─────────────────────────────────────────────────┘
```

## Process/Connection Model (Current)

- Listener: accepts TCP connections on protocol ports; performs minimal detection
  only when using a shared port; selects a parser worker from the pool.
- Parser process (per connection):
  - Frontend: speaks the client's native wire protocol.
  - Translation: parses SQL into ScratchBird SBLR bytecode.
  - Backend: issues API calls to the ScratchBird engine; formats responses on the wire.
- Engine: executes SBLR/queries; provides a stable C/C++ API boundary; no direct
  wire protocol handling.

Data plane: After handoff, all client I/O flows directly between client
<-> parser process. The listener is not on the hot path.

Control plane: The listener manages lifecycle (spawn, monitor, restart),
observability, and admission control for its parser pool.

## Phasing

1) Phase A: ScratchBird Native Parser & Protocol
- Implement a single process‑per‑connection parser for the native protocol.
- Showcase context-aware parsing and minimal reserved words.

2) Phase B: PostgreSQL/MySQL/Firebird Parsers
- Add per‑protocol parser processes with full wire compliance tiers.
- Maintain identical engine API usage; only translators differ.

## Process Spawning & Lifecycle (Beta)

Parser process strategies:
- Pre-fork pool (Unix): listener pre-forks a pool of idle parser workers per protocol; Y‑Valve assigns a worker and hands off the socket.
- On-demand fork/exec (Unix): Y‑Valve spawns the parser on connection if pool is empty.
- Windows: CreateProcess with inherited/duplicated handle; maintain a warm pool when practical.

Control messages:
- Y‑Valve passes connection metadata (protocol type, negotiated params, auth hints) over a control channel (pipe or domain socket) at handoff time.
- Parser acknowledges handoff, assumes ownership of the socket, and reports lifecycle events (ready, auth-complete, error, exit code) for observability.

Failure policy:
- If parser spawn fails, Y‑Valve returns a protocol‑appropriate error to the client and logs telemetry.
- Crash handling: Y‑Valve reclaims the connection if spawn or init fails; otherwise the parser process owns the connection until close.

## Cross‑Platform Socket Handoff

Unix:
- Use `sendmsg()` with `SCM_RIGHTS` to pass the connected socket fd to the parser worker.
- Control channel: UNIX domain socket or pipe for metadata; validate pid/uid/gid as needed.

Windows:
- Use `WSADuplicateSocket()` to create a `WSAPROTOCOL_INFO` and transmit it to the child process via an inherited/IPC channel.
- The parser reconstructs a `SOCKET` via `WSASocket()` using the duplicated protocol info.

Security considerations:
- Validate parser binary path/signature; restrict spawn directories.
- Drop privileges (Unix) after bind/accept; run parsers under least-privileged accounts.
- Audit log: connection id, parser pid, protocol, and handoff timestamp.

## Connection Handoff Structure

```c
// Connection handoff from Network Layer to Y-Valve
struct YValveConnectionHandoff {
    // Connection Identity
    uint64_t    connection_id;      // Unique connection identifier
    int         socket_fd;          // Socket file descriptor
    
    // Protocol Detection
    enum ProtocolType {
        PROTOCOL_UNKNOWN = 0,
        PROTOCOL_POSTGRESQL = 1,
        PROTOCOL_MYSQL = 2,
        PROTOCOL_FIREBIRD = 3,
        PROTOCOL_TDS = 4,
        PROTOCOL_NATIVE = 5,
        PROTOCOL_HTTP = 6,         // REST API
        PROTOCOL_GRPC = 7          // gRPC API
    } protocol_type;
    
    // Network Information
    struct {
        uint8_t  ip_address[16];    // IPv4 or IPv6
        uint16_t port;              // Client port
        uint8_t  ip_version;        // 4 or 6
        bool     ssl_requested;     // SSL/TLS requested
        uint8_t  ssl_version;       // TLS version if SSL
    } network_info;
    
    // Initial Protocol Data
    struct {
        uint8_t* data;              // Initial bytes received
        size_t   length;            // Length of initial data
        bool     complete_packet;   // Is this a complete packet?
    } initial_data;
    
    // Client Hints (if available from initial handshake)
    struct {
        char     client_name[64];   // Client application name
        char     client_version[32];// Client version
        char     username[128];     // Username if provided
        char     database[128];     // Target database if provided
        uint32_t client_encoding;   // Character encoding
        uint32_t client_collation;  // Collation
    } client_hints;
    
    // Connection Options
    struct {
        uint32_t timeout_ms;        // Connection timeout
        uint32_t max_packet_size;   // Maximum packet size
        bool     compression;       // Compression requested
        bool     readonly;          // Read-only connection
        uint8_t  isolation_level;   // Requested isolation level
    } options;
};
```

## Y-Valve Core Components

### 1. Protocol Detector

```c
class ProtocolDetector {
public:
    struct DetectionResult {
        ProtocolType protocol;
        float confidence;           // 0.0 to 1.0
        uint32_t protocol_version;  // Protocol-specific version
        bool needs_more_data;       // Need more bytes to be certain
        size_t bytes_consumed;      // Bytes used for detection
    };
    
    // Detect protocol from initial bytes
    DetectionResult detect(const uint8_t* data, size_t length) {
        // PostgreSQL: StartupMessage or SSLRequest
        if (length >= 8) {
            uint32_t len = ntohl(*(uint32_t*)data);
            uint32_t code = ntohl(*(uint32_t*)(data + 4));
            
            if (code == 196608) {  // Protocol 3.0
                return {PROTOCOL_POSTGRESQL, 1.0, 3, false, 8};
            }
            if (code == 80877103) { // SSL Request
                return {PROTOCOL_POSTGRESQL, 0.9, 3, false, 8};
            }
        }
        
        // MySQL: HandshakeV10 starts with protocol version
        if (length >= 5 && data[4] == 10) {
            return {PROTOCOL_MYSQL, 0.95, 10, false, 5};
        }
        
        // TDS: Pre-login packet
        if (length >= 8 && data[0] == 0x12) {
            return {PROTOCOL_TDS, 0.9, 0x74, false, 8};
        }
        
        // Firebird: op_connect
        if (length >= 8) {
            uint32_t op = ntohl(*(uint32_t*)data);
            if (op == 1) { // op_connect
                return {PROTOCOL_FIREBIRD, 0.95, 13, false, 8};
            }
        }
        
        // Need more data
        if (length < 8) {
            return {PROTOCOL_UNKNOWN, 0.0, 0, true, 0};
        }
        
        return {PROTOCOL_UNKNOWN, 0.0, 0, false, length};
    }
    
private:
    // Pattern matching for each protocol
    bool matchesPostgreSQL(const uint8_t* data, size_t len);
    bool matchesMySQL(const uint8_t* data, size_t len);
    bool matchesTDS(const uint8_t* data, size_t len);
    bool matchesFirebird(const uint8_t* data, size_t len);
};
```

### 2. Parser Factory

```c
class ParserFactory {
public:
    // Parser interface that all implementations must follow
    class IParser {
    public:
        virtual ~IParser() = default;
        
        // Initialize with connection context
        virtual bool initialize(const ConnectionContext& ctx) = 0;
        
        // Parse SQL to BLR
        virtual BLRResult parseToBLR(const std::string& sql) = 0;
        
        // Handle protocol-specific commands
        virtual ResponsePacket handleCommand(const CommandPacket& cmd) = 0;
        
        // Get protocol-specific metadata
        virtual MetadataPacket getMetadata(const BLRResult& result) = 0;
        
        // Format results for client
        virtual DataPacket formatResults(const ResultSet& results) = 0;
        
        // Handle authentication
        virtual AuthResult authenticate(const AuthPacket& auth) = 0;
        
        // Protocol-specific features
        virtual bool supportsFeature(const std::string& feature) = 0;
    };
    
    // Create parser for detected protocol
    std::unique_ptr<IParser> createParser(ProtocolType protocol) {
        switch (protocol) {
            case PROTOCOL_POSTGRESQL:
                return std::make_unique<PostgreSQLParser>();
            case PROTOCOL_MYSQL:
                return std::make_unique<MySQLParser>();
            case PROTOCOL_FIREBIRD:
                return std::make_unique<FirebirdParser>();
            case PROTOCOL_TDS:
                return std::make_unique<TDSParser>();
            case PROTOCOL_NATIVE:
                return std::make_unique<NativeParser>();
            default:
                return nullptr;
        }
    }
    
    // Register custom parser
    void registerParser(ProtocolType protocol, 
                        std::function<std::unique_ptr<IParser>()> factory) {
        custom_parsers[protocol] = factory;
    }
    
private:
    std::map<ProtocolType, std::function<std::unique_ptr<IParser>()>> custom_parsers;
};
```

### 3. Connection Context Manager

```c
class ConnectionContext {
public:
    // Connection state
    enum State {
        STATE_INITIAL,
        STATE_AUTHENTICATING,
        STATE_AUTHENTICATED,
        STATE_IN_TRANSACTION,
        STATE_EXECUTING,
        STATE_FETCHING,
        STATE_ERROR,
        STATE_CLOSING
    };
    
    struct Context {
        // Identity
        uint64_t connection_id;
        uint64_t session_id;
        
        // Protocol
        ProtocolType protocol;
        uint32_t protocol_version;
        std::unique_ptr<IParser> parser;
        
        // Authentication
        std::string username;
        std::string database;
        uint8_t auth_method;
        bool authenticated;
        
        // Current state
        State state;
        uint64_t transaction_id;
        uint8_t isolation_level;
        
        // Schema context
        UUID current_schema;
        std::vector<UUID> search_path;
        
        // Client capabilities
        struct {
            bool supports_prepared_statements;
            bool supports_cursors;
            bool supports_portals;
            bool supports_extended_query;
            bool supports_copy;
            bool supports_compression;
            uint32_t max_identifier_length;
            uint32_t max_columns;
            uint32_t max_parameters;
        } capabilities;
        
        // Performance tracking
        struct {
            uint64_t queries_executed;
            uint64_t bytes_sent;
            uint64_t bytes_received;
            uint64_t errors;
            std::chrono::steady_clock::time_point connected_at;
            std::chrono::steady_clock::time_point last_activity;
        } stats;
        
        // Protocol-specific state
        std::any protocol_state;  // PostgreSQL: ProtocolState, MySQL: SessionState, etc.
    };
    
    // Manage contexts
    Context* createContext(uint64_t conn_id, ProtocolType protocol);
    Context* getContext(uint64_t conn_id);
    void removeContext(uint64_t conn_id);
    
    // State transitions
    bool transitionState(uint64_t conn_id, State new_state);
    
private:
    std::unordered_map<uint64_t, std::unique_ptr<Context>> contexts;
    std::shared_mutex context_mutex;
};
```

### 4. Protocol State Machines

```c
// Base state machine for all protocols
class ProtocolStateMachine {
public:
    virtual ~ProtocolStateMachine() = default;
    
    // Process incoming packet based on current state
    virtual ProcessResult processPacket(const Packet& packet) = 0;
    
    // Get next expected packet type
    virtual PacketType getExpectedPacket() = 0;
    
    // Handle state-specific timeouts
    virtual void handleTimeout() = 0;
};

// PostgreSQL Extended Query Protocol State Machine
class PostgreSQLStateMachine : public ProtocolStateMachine {
    enum PGState {
        PG_STARTUP,
        PG_AUTH_REQUEST_SENT,
        PG_AUTH_RESPONSE_WAIT,
        PG_READY_FOR_QUERY,
        PG_PARSE_WAIT,
        PG_BIND_WAIT,
        PG_DESCRIBE_WAIT,
        PG_EXECUTE_WAIT,
        PG_COPY_IN,
        PG_COPY_OUT,
        PG_FUNCTION_CALL
    };
    
    ProcessResult processPacket(const Packet& packet) override {
        switch (current_state) {
            case PG_STARTUP:
                return handleStartup(packet);
            case PG_READY_FOR_QUERY:
                return handleQuery(packet);
            // ... etc
        }
    }
};
```

## Protocol Translation Layer

### SQL to BLR Translation

```c
class ProtocolTranslator {
public:
    // Translate protocol-specific SQL to ScratchBird SQL
    struct TranslationResult {
        std::string scratchbird_sql;
        std::vector<Parameter> parameters;
        std::vector<Hint> hints;
        bool requires_emulation;
        std::vector<EmulationStep> emulation_steps;
    };
    
    // PostgreSQL-specific translations
    TranslationResult translatePostgreSQL(const std::string& sql) {
        TranslationResult result;
        
        // Handle PostgreSQL-specific syntax
        // - Dollar quoting: $$...$$
        // - :: casting
        // - RETURNING clause
        // - Arrays
        // - LATERAL joins
        
        // Example: ARRAY[1,2,3] -> ScratchBird array literal
        // Example: table::text -> CAST(table AS TEXT)
        
        return result;
    }
    
    // MySQL-specific translations
    TranslationResult translateMySQL(const std::string& sql) {
        TranslationResult result;
        
        // Handle MySQL-specific syntax
        // - Backticks for identifiers
        // - LIMIT without OFFSET
        // - SHOW commands
        // - HANDLER statements
        // - REPLACE INTO
        
        return result;
    }
    
    // Type mapping for results
    DataType mapToProtocolType(SBType sb_type, ProtocolType protocol) {
        // Use the universal type mapping
        return type_mapper.mapType(sb_type, protocol);
    }
};
```

## Connection Lifecycle

### 1. Connection Establishment

```c
class YValve {
public:
    void handleNewConnection(const YValveConnectionHandoff& handoff) {
        // Step 1: Validate handoff
        if (!validateHandoff(handoff)) {
            rejectConnection(handoff.connection_id, "Invalid handoff");
            return;
        }
        
        // Step 2: Detect protocol if unknown
        ProtocolType protocol = handoff.protocol_type;
        if (protocol == PROTOCOL_UNKNOWN) {
            auto detection = detector.detect(
                handoff.initial_data.data,
                handoff.initial_data.length
            );
            
            if (detection.needs_more_data) {
                // Read more data from socket
                readMoreData(handoff.socket_fd);
            }
            
            protocol = detection.protocol;
        }
        
        // Step 3: Create parser
        auto parser = factory.createParser(protocol);
        if (!parser) {
            rejectConnection(handoff.connection_id, "Unsupported protocol");
            return;
        }
        
        // Step 4: Create connection context
        auto* context = context_manager.createContext(
            handoff.connection_id, 
            protocol
        );
        context->parser = std::move(parser);
        
        // Step 5: Initialize parser with context
        if (!context->parser->initialize(*context)) {
            rejectConnection(handoff.connection_id, "Parser initialization failed");
            return;
        }
        
        // Step 6: Start authentication
        startAuthentication(context);
    }
    
private:
    ProtocolDetector detector;
    ParserFactory factory;
    ConnectionContextManager context_manager;
    ProtocolTranslator translator;
};
```

### 2. Query Processing Flow

```c
void YValve::processQuery(uint64_t conn_id, const QueryPacket& query) {
    auto* context = context_manager.getContext(conn_id);
    
    // Step 1: Protocol-specific SQL to ScratchBird SQL
    auto translation = translator.translate(
        query.sql, 
        context->protocol
    );
    
    // Step 2: Parse to BLR
    auto blr_result = context->parser->parseToBLR(
        translation.scratchbird_sql
    );
    
    // Step 3: Execute via engine
    auto engine_result = engine->execute(
        blr_result.blr,
        context->transaction_id
    );
    
    // Step 4: Format results for client protocol
    auto response = context->parser->formatResults(
        engine_result
    );
    
    // Step 5: Send response
    network_layer->sendResponse(conn_id, response);
}
```

## Advanced Features

### 1. Protocol Emulation

```c
class ProtocolEmulator {
public:
    // Emulate protocol-specific features not in ScratchBird
    void emulateShowTables(Context* ctx) {
        // MySQL SHOW TABLES -> 
        // SELECT table_name FROM information_schema.tables
        std::string sql = R"(
            SELECT table_name 
            FROM [root].[sys].tables 
            WHERE schema_uuid = ?
        )";
        
        auto blr = generateBLR(sql, {ctx->current_schema});
        executeAndFormat(ctx, blr);
    }
    
    void emulateDescribeTable(Context* ctx, const std::string& table) {
        // MySQL DESCRIBE -> column information query
        // PostgreSQL \d -> similar transformation
    }
};
```

### 2. Connection Pooling Integration

```c
class YValvePoolManager {
    // Pool connections by protocol and database
    struct PoolKey {
        ProtocolType protocol;
        std::string database;
        std::string username;
    };
    
    struct PooledConnection {
        std::unique_ptr<Context> context;
        std::chrono::steady_clock::time_point last_used;
        bool in_transaction;
    };
    
    // Get or create pooled connection
    Context* getPooledConnection(const PoolKey& key) {
        // Check for available connection
        // Reset state if reusing
        // Create new if needed
    }
};
```

### 3. Load Balancing

```c
class LoadBalancer {
    // Route read queries to replicas
    EngineConnection* routeQuery(const BLRResult& blr, bool is_write) {
        if (!is_write && has_replicas) {
            return selectReplica();  // Round-robin, least-connections, etc.
        }
        return primary_engine;
    }
};
```

## Performance Optimizations

### 1. Protocol-Specific Caching

```c
class ProtocolCache {
    // Cache prepared statements per protocol
    struct CacheKey {
        ProtocolType protocol;
        std::string sql_hash;
    };
    
    struct CachedStatement {
        BLRData blr;
        QueryPlan plan;
        std::chrono::steady_clock::time_point created;
        uint64_t hit_count;
    };
    
    LRUCache<CacheKey, CachedStatement> statement_cache;
};
```

### 2. Fast Path Execution

```c
class FastPath {
    // Bypass full parsing for simple queries
    bool canUseFastPath(const std::string& sql) {
        // Simple SELECT/INSERT/UPDATE/DELETE
        // No CTEs, subqueries, joins
        // Direct table access
    }
    
    BLRResult generateFastBLR(const std::string& sql) {
        // Direct BLR generation for simple cases
    }
};
```

## Security Considerations

### 1. Protocol-Specific Authentication

```c
class AuthenticationManager {
    // Map protocol auth to ScratchBird auth
    AuthResult authenticatePostgreSQL(const PGAuthPacket& auth) {
        switch (auth.method) {
            case PG_AUTH_MD5:
                return authenticateMD5(auth.username, auth.password_hash);
            case PG_AUTH_SCRAM:
                return authenticateSCRAM(auth.scram_data);
            case PG_AUTH_CERT:
                return authenticateCertificate(auth.certificate);
        }
    }
};
```

### 2. SQL Injection Prevention

```c
class SQLSanitizer {
    // Validate and sanitize protocol-specific SQL
    bool validateSQL(const std::string& sql, ProtocolType protocol) {
        // Check for injection patterns
        // Validate parameter placeholders
        // Ensure proper escaping
    }
};
```

## Monitoring and Diagnostics

```c
struct YValveMetrics {
    // Per-protocol metrics
    struct ProtocolMetrics {
        uint64_t connections_total;
        uint64_t connections_active;
        uint64_t queries_total;
        uint64_t errors_total;
        uint64_t bytes_in;
        uint64_t bytes_out;
        std::chrono::nanoseconds total_latency;
    };
    
    std::map<ProtocolType, ProtocolMetrics> by_protocol;
    
    // Parser performance
    struct ParserMetrics {
        uint64_t parse_count;
        uint64_t parse_errors;
        std::chrono::nanoseconds parse_time;
        uint64_t cache_hits;
        uint64_t cache_misses;
    };
    
    ParserMetrics parser_metrics;
};
```

## Configuration

```yaml
# y_valve.yaml
y_valve:
  # Protocol detection
  detection:
    timeout_ms: 1000
    max_peek_bytes: 1024
    
  # Parser configuration
  parsers:
    postgresql:
      enabled: true
      version: "14.0"
      extensions: ["arrays", "json", "full_text"]
    mysql:
      enabled: true
      version: "8.0"
      sql_mode: "TRADITIONAL"
    firebird:
      enabled: true
      version: "4.0"
    tds:
      enabled: true
      version: "7.4"
      
  # Connection management
  connections:
    max_per_protocol: 1000
    idle_timeout_seconds: 300
    authentication_timeout_ms: 5000
    
  # Pooling
  pooling:
    enabled: true
    min_size: 10
    max_size: 100
    
  # Caching
  cache:
    statement_cache_size: 10000
    plan_cache_size: 5000
    ttl_seconds: 3600
```

## Testing Strategy

### 1. Protocol Compliance Tests

```c
class ProtocolComplianceTest {
    // Test against official test suites
    void testPostgreSQLCompliance() {
        // Run PostgreSQL regression tests
        // Verify wire protocol compatibility
        // Test extended query protocol
    }
    
    void testMySQLCompliance() {
        // Run MySQL test suite
        // Verify packet formats
        // Test prepared statements
    }
};
```

### 2. Multi-Protocol Tests

```c
class MultiProtocolTest {
    // Same query through different protocols
    void testCrossProtocolQuery() {
        std::string sql = "SELECT * FROM users WHERE id = ?";
        
        auto pg_result = executeViaPostgreSQL(sql);
        auto mysql_result = executeViaMySQL(sql);
        auto fb_result = executeViaFirebird(sql);
        
        ASSERT_EQ(pg_result, mysql_result);
        ASSERT_EQ(mysql_result, fb_result);
    }
};
```

## Future Enhancements

1. **HTTP/REST API Protocol**
   - JSON request/response
   - GraphQL support
   - WebSocket for streaming

2. **gRPC Protocol**
   - Protocol Buffers
   - Streaming queries
   - Bidirectional streaming

3. **Redis Protocol**
   - Key-value operations
   - Pub/sub support

4. **MongoDB Wire Protocol**
   - Document operations
   - Aggregation pipeline

5. **JDBC/ODBC Bridges**
   - Direct JDBC support
   - ODBC driver interface
