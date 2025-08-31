# ScratchBird Architecture Goals

## Core Design Principles

### 1. Embedded Engine as Foundation
```
┌─────────────────────────────────────┐
│         Applications                 │
├─────────────────────────────────────┤
│    Embedded ScratchBird Engine      │ ← Core (Phases 1-16)
│         (MGA + WAL)                 │
└─────────────────────────────────────┘
```

The embedded engine is the heart - fully functional database without any network layer.

### 2. Server as Engine Consumer
```
┌─────────────────────────────────────┐
│      Network Server Layer           │ ← Server (Phase 17+)
├─────────────────────────────────────┤
│    Embedded ScratchBird Engine      │ ← Reused, not reimplemented
└─────────────────────────────────────┘
```

Server doesn't reimplement database logic - it wraps the embedded engine.

### 3. Modular Server Architecture
```
┌─────────────────────────────────────┐
│     Cluster Coordination Layer      │ ← Optional
├─────────────────────────────────────┤
│      Replication Layer              │ ← Optional
├─────────────────────────────────────┤
│      Cache Coherency Layer          │ ← Optional
├─────────────────────────────────────┤
│      Basic Network Server           │ ← Minimal
├─────────────────────────────────────┤
│    Embedded ScratchBird Engine      │ ← Always present
└─────────────────────────────────────┘
```

Each layer adds capabilities without modifying lower layers.

### 4. Multi-Protocol Listeners
```
     MySQL Client    PostgreSQL Client    MSSQL Client
           ↓               ↓                  ↓
    [MySQL Listener] [PG Listener]    [TDS Listener]
           ↓               ↓                  ↓
    ┌──────────────────────────────────────────┐
    │            Y-Valve Router                │
    └──────────────────────────────────────────┘
                       ↓
              Embedded Engine
```

Each listener speaks native wire protocol of its database type.

### 5. Y-Valve Translation Layer
```cpp
class YValve {
    // Detect client type from initial handshake
    ClientType detect_client(socket);
    
    // Create appropriate translator
    unique_ptr<Translator> create_translator(ClientType type) {
        switch(type) {
            case MySQL: return make_unique<MySQLTranslator>();
            case PostgreSQL: return make_unique<PGTranslator>();
            case MSSQL: return make_unique<TDSTranslator>();
            case Firebird: return make_unique<NativeTranslator>();
        }
    }
    
    // Route to engine with translation
    Result execute(ClientRequest req) {
        auto translator = translators[connection_id];
        auto native_query = translator->to_native(req);
        auto result = engine->execute(native_query);
        return translator->from_native(result);
    }
};
```

### 6. Universal Type System
```cpp
// Core types support all database variants
enum UniversalType {
    // Numeric
    TINYINT,     // MySQL/MSSQL
    SMALLINT,    // All
    INTEGER,     // All
    BIGINT,      // All
    DECIMAL,     // All (with precision/scale)
    REAL,        // All
    DOUBLE,      // All
    
    // String
    CHAR,        // All (with length)
    VARCHAR,     // All (with length)
    TEXT,        // PostgreSQL/MySQL
    NVARCHAR,    // MSSQL
    BLOB,        // All
    
    // Temporal
    DATE,        // All
    TIME,        // All
    TIMESTAMP,   // All
    DATETIME,    // MySQL/MSSQL
    DATETIME2,   // MSSQL
    
    // Special
    UUID,        // PostgreSQL/MSSQL
    JSON,        // PostgreSQL/MySQL
    JSONB,       // PostgreSQL
    XML,         // MSSQL/PostgreSQL
    ARRAY,       // PostgreSQL
    
    // ... complete mapping
};
```

### 7. UUID-Based Schema System
```cpp
struct SchemaObject {
    UUID object_id;           // Immutable identifier
    string current_name;      // Can change
    UUID parent_namespace;    // Hierarchical
    ObjectType type;         // Table, View, Procedure, etc.
};

// BLR references UUIDs, not names
struct BLRInstruction {
    OpCode op;
    UUID target_object;  // Not affected by renames
};

// Mount remote schemas at any point
class SchemaManager {
    void mount_remote(UUID mount_point, RemoteSchema schema) {
        // Remote MySQL database appears as schema branch
        namespaces[mount_point].add_child(schema);
    }
    
    // Client sees only their namespace
    Namespace get_client_view(ClientType type, UUID root) {
        // MySQL client sees MySQL-compatible schema
        // PostgreSQL client sees PG-compatible schema
        return filter_namespace(root, type);
    }
};
```

### 8. Plugin Architecture for Remote Engines
```cpp
class RemoteEnginePlugin {
    virtual Connection connect(ConnectionString) = 0;
    virtual Result execute(Query) = 0;
    virtual Schema discover_schema() = 0;
};

class MySQLPlugin : public RemoteEnginePlugin {
    // Connect to real MySQL and federate queries
};

class PostgreSQLPlugin : public RemoteEnginePlugin {
    // Connect to real PostgreSQL
};

// Federation through plugins
class FederationEngine {
    Result execute_federated(Query q) {
        if (q.touches_remote_table()) {
            auto remote = plugins[q.remote_engine];
            auto remote_result = remote->execute(q.remote_part);
            return merge_results(local_result, remote_result);
        }
    }
};
```

## Implementation Phases Restructured

### Foundation Phases (1-10): Embedded Engine Core
- Pure embedded database
- No network code
- MGA architecture
- Basic SQL

### Extension Phases (11-16): Advanced Embedded Features
- Indexing, constraints, optimization
- Still pure embedded
- WAL for durability
- Complete embedded engine

### Server Foundation (17-19): Basic Network Server
- Y-Valve framework
- Native Firebird protocol
- Single listener
- Wraps embedded engine

### Multi-Protocol Support (20-23): Protocol Adapters
- MySQL wire protocol listener
- PostgreSQL wire protocol listener
- TDS (MSSQL) protocol listener
- Protocol translation layers

### Universal Features (24-27): Cross-Database Support
- Universal type system
- Dialect-specific parsers
- Function/operator translation
- Compatibility modes

### UUID Schema System (28-30): Advanced Schema Management
- UUID-based object identification
- Hierarchical namespaces
- Schema mounting
- Rename without invalidation

### Federation (31-33): Remote Engine Plugins
- Plugin framework
- MySQL federation plugin
- PostgreSQL federation plugin
- Distributed query execution

### Clustering (34-36): Scale-Out
- Cluster coordination
- Distributed transactions
- Cache coherency
- Load balancing

## Key Architectural Decisions

### 1. Embedded First
- Server is optional
- All features work embedded
- Simplifies testing
- Enables edge deployments

### 2. Protocol Compatibility, Not Emulation
- Speak native protocols
- Translate to MGA model
- Maintain Firebird advantages
- No read locks even for MySQL clients

### 3. UUID Everywhere
- Objects identified by UUID
- Names are just labels
- Enables seamless federation
- Supports multiple name views

### 4. Plugin-Based Federation
- Don't reimplement other engines
- Connect to real instances
- Federate at query level
- Maintain transactional semantics

## Benefits of This Architecture

1. **Universal Drop-In Replacement**: Applications think they're talking to their native database
2. **Best of All Worlds**: MGA's lock-free reads with compatibility for lock-based clients
3. **Seamless Migration**: Can federate with existing databases during migration
4. **Multi-Tenant Friendly**: Different tenants can use different SQL dialects
5. **Future-Proof**: New database protocols can be added as plugins

## Challenges to Address

1. **Semantic Differences**: MySQL's AUTO_INCREMENT vs PostgreSQL's SERIAL vs MSSQL's IDENTITY
2. **Transaction Models**: How to present MGA to clients expecting locks
3. **Feature Gaps**: Database-specific features (e.g., PostgreSQL's LISTEN/NOTIFY)
4. **Performance**: Translation overhead must be minimal
5. **Testing**: Each protocol needs comprehensive compatibility testing

## Success Criteria

- MySQL application connects and runs without modification
- PostgreSQL application connects and runs without modification  
- MSSQL application connects and runs without modification
- Performance within 10% of native databases
- Can federate queries across different database types
- Maintains MGA advantages (no read locks) regardless of client type