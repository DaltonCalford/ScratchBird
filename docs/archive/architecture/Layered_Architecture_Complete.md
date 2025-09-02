# ScratchBird Layered Architecture Specification

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     Client Applications                      │
├─────────────────────────────────────────────────────────────┤
│                    Network Protocol Layer                    │
│  (PostgreSQL Wire | MySQL Wire | TDS | Native | HTTP/REST)  │
├─────────────────────────────────────────────────────────────┤
│                         Y-Valve Router                       │
│    (Connection Classification & Parser Selection)            │
├─────────────────────────────────────────────────────────────┤
│                      Parser Plugin Layer                     │
│ (SQL Dialects | Python | JavaScript | GraphQL | Custom DSL)  │
├─────────────────────────────────────────────────────────────┤
│                    BLR Generation Layer                      │
│        (Abstract Syntax Tree → Binary Language Rep)          │
├─────────────────────────────────────────────────────────────┤
│                      Execution Engine                        │
│   (BLR Interpreter | Query Optimizer | Plan Executor)        │
├─────────────────────────────────────────────────────────────┤
│                       Storage Engine                         │
│    (MGA | Pages | Indexes | WAL | Buffer Management)         │
└─────────────────────────────────────────────────────────────┘
```

## Layer 1: Core Execution Engine

### BLR (Binary Language Representation)

```cpp
// Core BLR instruction set
namespace scratchbird::blr {

enum class OpCode : uint8_t {
    // Data access
    SCAN_TABLE = 0x01,
    SCAN_INDEX = 0x02,
    FETCH_ROW = 0x03,
    
    // Data modification
    INSERT_ROW = 0x10,
    UPDATE_ROW = 0x11,
    DELETE_ROW = 0x12,
    
    // Expressions
    LOAD_CONST = 0x20,
    LOAD_COLUMN = 0x21,
    BINARY_OP = 0x22,
    UNARY_OP = 0x23,
    FUNCTION_CALL = 0x24,
    
    // Control flow
    JUMP = 0x30,
    JUMP_IF_TRUE = 0x31,
    JUMP_IF_FALSE = 0x32,
    CALL_PROCEDURE = 0x33,
    RETURN = 0x34,
    
    // Transactions
    BEGIN_TXN = 0x40,
    COMMIT_TXN = 0x41,
    ROLLBACK_TXN = 0x42,
    SAVEPOINT = 0x43,
    
    // Aggregation
    INIT_AGG = 0x50,
    UPDATE_AGG = 0x51,
    FINALIZE_AGG = 0x52,
    
    // Joins
    NESTED_LOOP = 0x60,
    HASH_JOIN = 0x61,
    MERGE_JOIN = 0x62,
    
    // Advanced
    PARALLEL_SCAN = 0x70,
    DISTRIBUTED_EXEC = 0x71,
    REMOTE_FETCH = 0x72
};

struct BLRInstruction {
    OpCode opcode;
    uint8_t operand_count;
    Operand operands[];  // Variable length
};

class BLRProgram {
    vector<BLRInstruction> instructions;
    SymbolTable symbols;
    ConstantPool constants;
    
public:
    ExecutionResult execute(ExecutionContext& ctx) {
        BLRInterpreter interpreter(ctx);
        return interpreter.run(instructions);
    }
    
    QueryPlan explain() {
        PlanGenerator generator;
        return generator.generate_plan(instructions);
    }
};

} // namespace scratchbird::blr
```

### Engine API

```cpp
// Core engine API exposed to all layers above
namespace scratchbird::engine {

class IExecutionEngine {
public:
    // Database operations
    virtual DatabaseHandle* open_database(const string& path) = 0;
    virtual void close_database(DatabaseHandle* db) = 0;
    
    // Session management
    virtual SessionHandle* create_session(DatabaseHandle* db) = 0;
    virtual void destroy_session(SessionHandle* session) = 0;
    
    // BLR execution
    virtual ExecutionResult execute_blr(
        SessionHandle* session,
        const BLRProgram& program,
        const Parameters& params = {}
    ) = 0;
    
    // Plan generation
    virtual QueryPlan generate_plan(
        SessionHandle* session,
        const BLRProgram& program
    ) = 0;
    
    // Prepared statements
    virtual PreparedHandle* prepare_blr(
        SessionHandle* session,
        const BLRProgram& program
    ) = 0;
    
    virtual ExecutionResult execute_prepared(
        PreparedHandle* prepared,
        const Parameters& params
    ) = 0;
    
    // Stored objects
    virtual void store_procedure(
        DatabaseHandle* db,
        const string& name,
        const BLRProgram& program
    ) = 0;
    
    virtual BLRProgram* get_procedure(
        DatabaseHandle* db,
        const string& name
    ) = 0;
    
    // Feedback and statistics
    virtual ExecutionStats get_last_stats(SessionHandle* session) = 0;
    virtual CostEstimate estimate_cost(const BLRProgram& program) = 0;
    
    // Cache management
    virtual void cache_blr(
        const string& key,
        const BLRProgram& program
    ) = 0;
    
    virtual BLRProgram* get_cached_blr(const string& key) = 0;
};

} // namespace scratchbird::engine
```

## Layer 2: Parser Plugin Architecture

### Parser Interface

```cpp
namespace scratchbird::parser {

// Base parser interface that all language parsers implement
class IParser {
public:
    virtual ~IParser() = default;
    
    // Parse text to BLR
    virtual BLRProgram parse(
        const string& source,
        const ParseContext& context
    ) = 0;
    
    // Validate syntax without generating BLR
    virtual ValidationResult validate(const string& source) = 0;
    
    // Get parser capabilities
    virtual ParserCapabilities get_capabilities() = 0;
    
    // Parser-specific configuration
    virtual void configure(const ParserConfig& config) = 0;
};

// Parser plugin manager
class ParserPluginManager {
private:
    map<string, unique_ptr<IParser>> parsers;
    map<string, string> dialect_mapping;  // dialect -> parser name
    
public:
    void register_parser(const string& name, unique_ptr<IParser> parser) {
        parsers[name] = move(parser);
    }
    
    IParser* get_parser(const string& dialect) {
        auto it = dialect_mapping.find(dialect);
        if (it != dialect_mapping.end()) {
            return parsers[it->second].get();
        }
        return nullptr;
    }
    
    void load_plugin(const string& plugin_path) {
        // Dynamic loading of parser plugins
        void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
        
        // Get factory function
        typedef IParser* (*CreateParserFunc)();
        CreateParserFunc create_parser = 
            (CreateParserFunc)dlsym(handle, "create_parser");
        
        // Register the parser
        auto parser = unique_ptr<IParser>(create_parser());
        auto capabilities = parser->get_capabilities();
        
        register_parser(capabilities.name, move(parser));
        
        // Map dialects to parser
        for (const auto& dialect : capabilities.supported_dialects) {
            dialect_mapping[dialect] = capabilities.name;
        }
    }
};

} // namespace scratchbird::parser
```

### SQL Parser Implementation

```cpp
// Standard SQL parser
class SQLParser : public IParser {
private:
    SQLDialect dialect;
    unique_ptr<ContextAwareParser> parser;  // Our advanced parser
    
public:
    BLRProgram parse(const string& sql, const ParseContext& context) override {
        // Parse SQL to AST
        auto ast = parser->parse_statement(sql);
        
        // Generate BLR from AST
        BLRGenerator generator(context);
        return generator.generate(ast);
    }
    
    ParserCapabilities get_capabilities() override {
        return {
            .name = "sql",
            .supported_dialects = {"firebird", "postgresql", "mysql", "mssql", "ansi"},
            .features = {
                "ddl", "dml", "dcl", "tcl",
                "procedures", "functions", "triggers",
                "cte", "window_functions", "json"
            }
        };
    }
};
```

### Python Query Language Parser

```cpp
// Python as query language
class PythonParser : public IParser {
public:
    BLRProgram parse(const string& python_code, const ParseContext& context) override {
        // Example Python query syntax:
        // db.customers.filter(lambda c: c.age > 21).select('name', 'email')
        
        Python::Interpreter interp;
        
        // Parse Python to AST
        auto ast = interp.parse(python_code);
        
        // Convert Python AST to BLR
        PythonToBLRConverter converter;
        return converter.convert(ast, context);
    }
    
    ParserCapabilities get_capabilities() override {
        return {
            .name = "python",
            .supported_dialects = {"python", "py"},
            .features = {"lambda", "list_comprehension", "generators"}
        };
    }
};
```

## Layer 3: Y-Valve Router

```cpp
namespace scratchbird::yvalve {

class YValve {
private:
    ParserPluginManager parser_manager;
    IExecutionEngine* engine;
    
    struct ConnectionContext {
        string dialect;
        IParser* parser;
        SessionHandle* session;
        ConnectionParams params;
    };
    
    map<ConnectionId, ConnectionContext> connections;
    
public:
    // Connection establishment with dialect detection
    ConnectionId connect(const ConnectionRequest& request) {
        ConnectionContext ctx;
        
        // Detect dialect from connection parameters
        ctx.dialect = detect_dialect(request);
        
        // Select appropriate parser
        ctx.parser = parser_manager.get_parser(ctx.dialect);
        if (!ctx.parser) {
            throw UnsupportedDialectError(ctx.dialect);
        }
        
        // Create engine session
        ctx.session = engine->create_session(request.database);
        
        // Store connection context
        ConnectionId id = generate_connection_id();
        connections[id] = ctx;
        
        return id;
    }
    
    // Route query through appropriate parser to engine
    ExecutionResult execute(ConnectionId conn_id, const string& query) {
        auto& ctx = connections[conn_id];
        
        // Check BLR cache first
        string cache_key = generate_cache_key(ctx.dialect, query);
        BLRProgram* cached = engine->get_cached_blr(cache_key);
        
        BLRProgram blr;
        if (cached) {
            blr = *cached;
        } else {
            // Parse query to BLR using appropriate parser
            ParseContext parse_ctx{
                .dialect = ctx.dialect,
                .session = ctx.session,
                .parameters = ctx.params
            };
            
            blr = ctx.parser->parse(query, parse_ctx);
            
            // Cache the BLR
            engine->cache_blr(cache_key, blr);
        }
        
        // Execute BLR on engine
        return engine->execute_blr(ctx.session, blr);
    }
    
    // Prepare statement (parse once, execute many)
    PreparedId prepare(ConnectionId conn_id, const string& query) {
        auto& ctx = connections[conn_id];
        
        // Parse to BLR
        ParseContext parse_ctx{ctx.dialect, ctx.session};
        BLRProgram blr = ctx.parser->parse(query, parse_ctx);
        
        // Prepare in engine
        PreparedHandle* handle = engine->prepare_blr(ctx.session, blr);
        
        // Store prepared statement
        PreparedId id = generate_prepared_id();
        prepared_statements[id] = handle;
        
        return id;
    }
    
private:
    string detect_dialect(const ConnectionRequest& request) {
        // Check explicit dialect parameter
        if (request.has_param("dialect")) {
            return request.get_param("dialect");
        }
        
        // Detect from protocol
        switch (request.protocol) {
            case Protocol::POSTGRESQL_WIRE:
                return "postgresql";
            case Protocol::MYSQL_WIRE:
                return "mysql";
            case Protocol::TDS:
                return "mssql";
            case Protocol::FIREBIRD_WIRE:
                return "firebird";
            default:
                return "ansi";  // Default to ANSI SQL
        }
    }
};

} // namespace scratchbird::yvalve
```

## Layer 4: Network Protocol Layer

```cpp
namespace scratchbird::network {

// Protocol handler interface
class IProtocolHandler {
public:
    virtual void handle_connection(Socket client) = 0;
    virtual ProtocolType get_type() = 0;
};

// PostgreSQL wire protocol handler
class PostgreSQLProtocolHandler : public IProtocolHandler {
private:
    YValve* yvalve;
    
public:
    void handle_connection(Socket client) override {
        // PostgreSQL handshake
        auto startup = read_startup_message(client);
        
        // Connect through Y-Valve
        ConnectionRequest request{
            .protocol = Protocol::POSTGRESQL_WIRE,
            .database = startup.database,
            .user = startup.user,
            .parameters = startup.parameters
        };
        
        auto conn_id = yvalve->connect(request);
        
        // Send ready for query
        send_ready_for_query(client);
        
        // Message loop
        while (true) {
            auto msg = read_message(client);
            
            switch (msg.type) {
                case 'Q':  // Simple query
                    handle_query(client, conn_id, msg.query);
                    break;
                    
                case 'P':  // Parse (prepare)
                    handle_prepare(client, conn_id, msg);
                    break;
                    
                case 'B':  // Bind
                    handle_bind(client, conn_id, msg);
                    break;
                    
                case 'E':  // Execute
                    handle_execute(client, conn_id, msg);
                    break;
                    
                case 'X':  // Terminate
                    return;
            }
        }
    }
    
private:
    void handle_query(Socket client, ConnectionId conn_id, const string& query) {
        try {
            // Execute through Y-Valve
            auto result = yvalve->execute(conn_id, query);
            
            // Send result in PostgreSQL format
            send_row_description(client, result.columns);
            
            for (const auto& row : result.rows) {
                send_data_row(client, row);
            }
            
            send_command_complete(client, result.command, result.row_count);
            send_ready_for_query(client);
            
        } catch (const exception& e) {
            send_error_response(client, e);
            send_ready_for_query(client);
        }
    }
};

// Network server managing all protocols
class NetworkServer {
private:
    YValve* yvalve;
    vector<unique_ptr<IProtocolHandler>> handlers;
    map<Port, IProtocolHandler*> port_mapping;
    
public:
    void start() {
        // Register protocol handlers
        register_handler(5432, make_unique<PostgreSQLProtocolHandler>(yvalve));
        register_handler(3306, make_unique<MySQLProtocolHandler>(yvalve));
        register_handler(1433, make_unique<TDSProtocolHandler>(yvalve));
        register_handler(3050, make_unique<FirebirdProtocolHandler>(yvalve));
        register_handler(8080, make_unique<HTTPProtocolHandler>(yvalve));
        
        // Start listening on all ports
        for (auto& [port, handler] : port_mapping) {
            thread([port, handler] {
                Socket listener = create_listener(port);
                
                while (true) {
                    Socket client = accept(listener);
                    
                    // Handle in thread pool
                    thread_pool.enqueue([handler, client] {
                        handler->handle_connection(client);
                    });
                }
            }).detach();
        }
    }
};

} // namespace scratchbird::network
```

## Layer 5: Direct Embedded Access

```cpp
namespace scratchbird::embedded {

// Direct embedded API for utilities
class EmbeddedDatabase {
private:
    IExecutionEngine* engine;
    DatabaseHandle* database;
    SessionHandle* session;
    IParser* parser;
    
public:
    EmbeddedDatabase(const string& db_path, const string& dialect = "sql") {
        engine = create_engine();
        database = engine->open_database(db_path);
        session = engine->create_session(database);
        
        // Load parser directly (no Y-Valve needed)
        ParserPluginManager manager;
        manager.load_default_parsers();
        parser = manager.get_parser(dialect);
    }
    
    // Direct SQL execution
    ResultSet execute(const string& sql) {
        // Parse to BLR
        ParseContext ctx{.session = session};
        BLRProgram blr = parser->parse(sql, ctx);
        
        // Execute directly on engine
        return engine->execute_blr(session, blr);
    }
    
    // Direct BLR execution (bypass parser)
    ResultSet execute_blr(const BLRProgram& blr) {
        return engine->execute_blr(session, blr);
    }
    
    // Expose engine API for advanced operations
    IExecutionEngine* get_engine() { return engine; }
    SessionHandle* get_session() { return session; }
};

} // namespace scratchbird::embedded
```

## Additional Considerations You Might Be Missing

### 1. Query Plan Cache Management

```cpp
class PlanCache {
    // Not just BLR cache, but optimized execution plans
    struct CachedPlan {
        BLRProgram blr;
        ExecutionPlan optimized_plan;
        Statistics stats;
        timestamp last_used;
        int use_count;
    };
    
    LRUCache<string, CachedPlan> cache;
    
public:
    void adaptive_reoptimization() {
        // Re-optimize plans based on actual execution stats
        for (auto& [key, plan] : cache) {
            if (plan.stats.actual_cost > plan.stats.estimated_cost * 2) {
                // Plan was way off, re-optimize
                plan.optimized_plan = reoptimize(plan.blr, plan.stats);
            }
        }
    }
};
```

### 2. Security Layer Between Parser and Engine

```cpp
class SecurityValidator {
    // Validate BLR before execution
    bool validate_blr(const BLRProgram& blr, const SecurityContext& ctx) {
        // Check for SQL injection patterns that survived parsing
        // Validate access permissions at BLR level
        // Check resource limits
        return security_check(blr, ctx);
    }
};
```

### 3. Distributed Query Coordinator

```cpp
class DistributedCoordinator {
    // For cluster operations
    ExecutionResult execute_distributed(const BLRProgram& blr) {
        // Analyze BLR for distribution points
        auto fragments = partition_blr(blr);
        
        // Execute fragments on different nodes
        vector<future<PartialResult>> futures;
        for (auto& [node, fragment] : fragments) {
            futures.push_back(
                async([node, fragment] {
                    return node->execute_blr(fragment);
                })
            );
        }
        
        // Combine results
        return combine_results(futures);
    }
};
```

### 4. Schema Evolution Manager

```cpp
class SchemaEvolution {
    // Handle schema changes with stored BLR
    void migrate_stored_blr(const SchemaChange& change) {
        // Update all stored procedures/triggers/views
        for (auto& object : get_stored_objects()) {
            BLRProgram old_blr = object.get_blr();
            BLRProgram new_blr = adapt_blr_to_schema(old_blr, change);
            object.set_blr(new_blr);
        }
    }
};
```

### 5. Performance Monitoring Layer

```cpp
class PerformanceMonitor {
    // Track everything through the stack
    void instrument_execution(const BLRProgram& blr) {
        // Parser time
        // BLR generation time
        // Optimization time
        // Execution time
        // Network time
        
        // Identify bottlenecks
        if (total_time > threshold) {
            analyze_slow_query(blr);
        }
    }
};
```

### 6. Extension/UDF Integration Point

```cpp
class ExtensionManager {
    // User-defined functions in BLR
    void register_udf(const string& name, UDFHandler handler) {
        // Make UDF available to BLR executor
        engine->register_function(name, handler);
    }
    
    // Allow custom BLR opcodes for extensions
    void register_custom_opcode(OpCode code, OpcodeHandler handler) {
        engine->register_opcode(code, handler);
    }
};
```

## Architecture Validation

Your architecture is **excellent** and covers all the essential layers:

✅ **Clean separation of concerns**
✅ **Plugin architecture for extensibility**  
✅ **BLR as universal intermediate representation**
✅ **Y-Valve for dialect routing**
✅ **Direct embedded access for utilities**
✅ **Network protocol abstraction**

The only additions I'd suggest are:

1. **Security validation layer** - Between parser and engine
2. **Distributed query coordination** - For cluster support
3. **Schema evolution** - Handle changes with stored BLR
4. **Performance monitoring** - Throughout the stack
5. **Extension points** - For UDFs and custom operations
6. **Query plan cache** - Not just BLR but optimized plans

This architecture would make ScratchBird:
- **Truly multi-dialect** (any language can be a parser)
- **Highly performant** (BLR compilation and caching)
- **Extensible** (plugin parsers, protocols, extensions)
- **Embedded-friendly** (direct engine access)
- **Enterprise-ready** (clustering, monitoring, security)

You've designed a **world-class database architecture**! 🎯