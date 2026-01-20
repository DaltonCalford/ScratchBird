# Listener/Parser Pool Design Principles (Legacy Y-Valve Terminology)

**Scope Note:** The historical "Y-Valve" router is superseded by the listener +
parser pool control plane. Any "Y-Valve" references in this document should be
read as **listener/pool control plane** responsibilities.

## Core Design Philosophy

The listener/pool control plane follows a **lean, fast, and focused** design philosophy:

1. **Minimal Core**: Listener/pool only handles routing and basic protocol detection
2. **Parser Responsibility**: Protocol-specific logic lives in parser implementations
3. **No Feature Creep**: Resist adding features that belong in parsers
4. **Performance First**: Every decision prioritizes latency and throughput

## Separation of Concerns

### Listener/Pool Core (Minimal)

```c
class ListenerControlPlane {
    // ONLY these responsibilities:
    // 1. Detect protocol from initial bytes
    // 2. Create appropriate parser instance
    // 3. Route packets to parser
    // 4. Manage parser lifecycle
    // 5. Track basic metrics
    
    void handleConnection(int socket_fd, const uint8_t* initial_data, size_t len) {
        // Detect protocol (if using a shared port)
        ProtocolType protocol = detectProtocol(initial_data, len);
        
        // Create parser or select worker from pool
        auto parser = factory.createParser(protocol);
        
        // Hand off completely to parser
        parser->takeOwnership(socket_fd, initial_data, len);
        
        // Track for cleanup
        active_parsers[socket_fd] = std::move(parser);
    }
    
    // That's it! No protocol-specific logic here
};
```

### Parser Implementation (Feature-Rich)

```c
class PostgreSQLParser : public IParser {
    // ALL PostgreSQL-specific logic:
    // - System catalogs (pg_catalog, information_schema)
    // - COPY BINARY format
    // - LISTEN/NOTIFY
    // - Large object support
    // - Extension commands
    // - Version negotiation
    // - Replication protocol (future)
    
private:
    // Parser maintains its own:
    PGSystemCatalog system_catalog;      // pg_catalog emulation
    PGInformationSchema info_schema;     // information_schema emulation
    PGProtocolStateMachine state;        // Protocol state
    PGTypeMapper type_mapper;            // Type conversions
    PGFeatureEmulator emulator;          // SHOW, \d commands, etc.
    PGVersionAdapter version_adapter;    // Handle protocol versions
    
public:
    void takeOwnership(int socket_fd, const uint8_t* initial_data, size_t len) {
        // Parser handles EVERYTHING from here
        this->socket = socket_fd;
        
        // Version negotiation
        auto version = negotiateVersion(initial_data);
        
        // Initialize system catalogs for this connection
        system_catalog.initialize(version);
        info_schema.initialize(version);
        
        // Start protocol state machine
        state.start(version);
    }
    
    // Handle system catalog queries internally
    BLRResult handleSystemQuery(const std::string& sql) {
        if (isSystemCatalogQuery(sql)) {
            // Don't go to engine, handle locally
            return system_catalog.executeQuery(sql);
        }
        return parseToBLR(sql);
    }
};
```

## Implementation Priorities

### Phase 1: Current Version Support (RC Target)
Focus on the latest stable versions only:

```yaml
protocol_support:
  postgresql:
    target_version: "15.x"  # Latest stable
    protocol_version: 3.0
    features:
      - Extended query protocol
      - COPY support
      - Basic system catalogs
    deferred:
      - Logical replication
      - Older protocol versions
      
  mysql:
    target_version: "8.0.x"  # Latest stable
    protocol_version: 4.1
    features:
      - Prepared statements
      - Basic information_schema
      - COM_QUERY/COM_STMT_PREPARE
    deferred:
      - Replication protocol
      - MySQL 5.7 compatibility
      
  firebird:
    target_version: "4.0.x"  # Latest stable
    protocol_version: 13
    features:
      - Core protocol
      - BLOB support
      - Events
    deferred:
      - Older protocol versions
      
  tds:  # post-gold, reserved
    target_version: "7.4"  # SQL Server 2012+
    features:
      - Basic TDS
      - System stored procedures
    deferred:
      - Older TDS versions
      - Bulk copy
```

### Phase 2: Post-RC Additions
After Release Candidate, add:

1. **Version Compatibility**
   - PostgreSQL 12, 13, 14 differences
   - MySQL 5.7 compatibility mode
   - Firebird 3.0 support
   - TDS 7.0-7.3 support (post-gold)

2. **Replication Protocols** (Future)
   - PostgreSQL logical replication
   - MySQL binlog protocol
   - But NOT in initial release

## Parser Implementation Template

Each parser should follow this structure:

```c
class ProtocolParser : public IParser {
public:
    // Constructor does minimal work
    ProtocolParser() = default;
    
    // Take full ownership of connection
    void takeOwnership(int socket_fd, const uint8_t* initial_data, size_t len) override {
        socket = socket_fd;
        
        // Initialize protocol-specific components
        initializeSystemCatalogs();
        initializeStateMachine();
        initializeTypeMapper();
        
        // Start handling
        handleInitialPacket(initial_data, len);
    }
    
    // Core interface (minimal)
    BLRResult parseToBLR(const std::string& sql) override {
        // Check if it's a system query we handle internally
        if (shouldHandleInternally(sql)) {
            return handleInternally(sql);
        }
        
        // Otherwise translate to BLR
        return translateToBLR(sql);
    }
    
private:
    // Protocol-specific components (rich)
    void initializeSystemCatalogs() {
        // Create virtual system tables
        // This is where pg_catalog, information_schema, etc. live
    }
    
    bool shouldHandleInternally(const std::string& sql) {
        // Queries against system catalogs
        // SHOW commands
        // Protocol-specific commands
        return isSystemCatalogQuery(sql) || 
               isShowCommand(sql) || 
               isProtocolSpecific(sql);
    }
    
    BLRResult handleInternally(const std::string& sql) {
        // Generate results without going to engine
        // This keeps the listener/pool control plane and engine lean
    }
};
```

## System Catalog Emulation

Each parser maintains its own system catalog emulation:

### PostgreSQL Parser System Catalogs

```c
class PGSystemCatalog {
    // Emulate these without engine involvement:
    // - pg_catalog.pg_class
    // - pg_catalog.pg_attribute  
    // - pg_catalog.pg_namespace
    // - pg_catalog.pg_type
    // - information_schema.tables
    // - information_schema.columns
    
    ResultSet executeQuery(const std::string& sql) {
        // Parse query
        auto query = parseSystemQuery(sql);
        
        // Map to ScratchBird system tables
        if (query.table == "pg_class") {
            return mapFromScratchBirdTables();
        }
        
        // Generate expected result format
        return formatForPostgreSQL(result);
    }
    
private:
    // Cache of system metadata
    std::map<std::string, TableInfo> table_cache;
    std::map<std::string, ColumnInfo> column_cache;
    
    // Lazy load from engine when needed
    void refreshCache() {
        // Query ScratchBird system tables once
        // Cache for this connection
    }
};
```

### MySQL Parser System Catalogs

```c
class MySQLInformationSchema {
    // Emulate without engine:
    // - information_schema.tables
    // - information_schema.columns
    // - information_schema.schemata
    // - mysql.user (limited view)
    
    ResultSet handleShowCommand(const std::string& cmd) {
        if (cmd == "SHOW TABLES") {
            return generateTableList();
        }
        if (cmd.starts_with("SHOW CREATE TABLE")) {
            return generateCreateStatement();
        }
        // etc...
    }
};
```

## Performance Optimizations

### 1. Fast Path for System Queries

```c
class FastSystemQueryHandler {
    // Cache common system queries
    LRUCache<std::string, ResultSet> query_cache;
    
    bool canUseFastPath(const std::string& sql) {
        return query_cache.contains(sql) || 
               isSimpleSystemQuery(sql);
    }
    
    ResultSet handleFast(const std::string& sql) {
        if (auto cached = query_cache.get(sql)) {
            return *cached;
        }
        
        auto result = generateSystemResult(sql);
        query_cache.put(sql, result);
        return result;
    }
};
```

### 2. Minimal Listener/Pool Overhead

```c
class ListenerPoolMetrics {
    // Only track essential metrics
    struct CoreMetrics {
        std::atomic<uint64_t> connections_routed;
        std::atomic<uint64_t> detection_failures;
        std::atomic<uint64_t> parser_creation_failures;
        // That's it! Parser tracks everything else
    };
    
    // No protocol-specific metrics here
    // Each parser tracks its own
};
```

## Testing Strategy

### 1. Listener/Pool Core Tests (Minimal)

```c
TEST(ListenerControlPlane, DetectsProtocols) {
    // Test protocol detection only
    EXPECT_EQ(detectProtocol(pg_bytes), PROTOCOL_POSTGRESQL);
    EXPECT_EQ(detectProtocol(mysql_bytes), PROTOCOL_MYSQL);
}

TEST(ListenerControlPlane, CreatesCorrectParser) {
    // Test parser factory only
    auto parser = factory.create(PROTOCOL_POSTGRESQL);
    EXPECT_NE(parser, nullptr);
}
```

### 2. Parser Tests (Comprehensive)

```c
TEST(PostgreSQLParser, SystemCatalogs) {
    // Test full system catalog emulation
    auto result = parser.execute("SELECT * FROM pg_catalog.pg_class");
    EXPECT_TRUE(result.isValid());
}

TEST(PostgreSQLParser, CopyBinary) {
    // Test COPY BINARY format
    auto result = parser.handleCopyBinary(data);
    EXPECT_TRUE(result.isValid());
}

TEST(PostgreSQLParser, VersionNegotiation) {
    // Test protocol version handling
    auto v14_parser = createParser("14.0");
    auto v15_parser = createParser("15.0");
    // Test version-specific features
}
```

## Migration Path

### From Existing Databases

Since parsers handle system catalogs internally:

1. **Transparent Migration**: Applications see expected system tables
2. **No Application Changes**: Queries work unchanged
3. **Gradual Migration**: Can run ScratchBird alongside existing DB

### Example: PostgreSQL Application

```sql
-- Application query (unchanged)
SELECT 
    c.relname,
    a.attname,
    t.typname
FROM pg_catalog.pg_class c
JOIN pg_catalog.pg_attribute a ON c.oid = a.attrelid
JOIN pg_catalog.pg_type t ON a.atttypid = t.oid
WHERE c.relname = 'users';

-- Parser handles this entirely
-- Never reaches the engine
-- Returns PostgreSQL-formatted results
```

## Documentation Requirements

### For Listener/Pool Control Plane
- Protocol detection algorithm
- Parser lifecycle management
- Basic metrics
- **That's all!**

### For Each Parser
- Supported protocol version(s)
- System catalog emulation details
- Protocol-specific features
- Version differences
- Migration guides
- Compatibility notes

## Success Metrics

### Listener/Pool Control Plane
- **Detection Speed**: <1ms protocol detection
- **Routing Overhead**: <0.1ms to parser handoff
- **Memory Usage**: <1KB per connection in listener control plane

### Parser Performance
- **System Query Speed**: <5ms for system catalog queries
- **Translation Speed**: <10ms SQL to BLR
- **Compatibility**: 95%+ application compatibility

## Summary

This lean listener/pool design:
1. **Keeps the core minimal and fast**
2. **Pushes complexity to parsers where it belongs**
3. **Enables independent parser development**
4. **Supports current versions first, older versions later**
5. **Defers replication until post-RC**
6. **Maintains full client compatibility through parser emulation**
