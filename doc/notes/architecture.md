# ScratchBird Architecture Overview

## System Architecture

ScratchBird v0.5.0 is built on Firebird's proven multi-generational architecture (MVCC) with significant enhancements for modern database requirements.

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                    ScratchBird Database Engine               │
├─────────────────────────────────────────────────────────────┤
│  Client Applications & Tools                                │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │   sb_isql   │ │   sb_gbak   │ │  sb_gstat   │    ...    │
│  └─────────────┘ └─────────────┘ └─────────────┘           │
├─────────────────────────────────────────────────────────────┤
│  Client Library (libsbclient)                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  API Layer  │  Network  │  Transaction  │  Memory   │   │
│  │  Bindings   │  Protocol │  Management   │  Management│   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Network Layer                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  TCP/IP     │  Named    │  Shared     │  Embedded   │   │
│  │  (Port 4050)│  Pipes    │  Memory     │  Mode       │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Database Engine Core                                       │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  SQL        │  Query    │  Execution  │  Transaction│   │
│  │  Parser     │  Optimizer│  Engine     │  Manager    │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Storage Engine                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Page       │  Index    │  Blob       │  Buffer     │   │
│  │  Manager    │  Manager  │  Manager    │  Manager    │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Operating System Interface                                 │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  File I/O   │  Memory   │  Threading  │  Networking │   │
│  │  Operations │  Management│  Support   │  Interface  │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Key Architectural Enhancements

**1. Hierarchical Schema Engine**
- 8-level deep schema nesting support
- Schema-aware query optimization
- Context-sensitive name resolution
- Caching system for performance

**2. Advanced Data Type System**
- PostgreSQL-compatible type engine
- Custom type handlers for network, UUID, range types
- Efficient binary storage formats
- Type-specific indexing strategies

**3. Modern C++17 Implementation**
- GPRE-free utilities (96.3% code reduction)
- Standards-compliant codebase
- Cross-platform compatibility
- Memory safety improvements

## Database Engine Components

### SQL Parser & Compiler

**Enhanced Grammar Support**
- SQL Dialect 4 with modern syntax
- FROM-less SELECT statements
- Multi-row INSERT operations
- Hierarchical schema qualified names

**Parser Architecture**
```cpp
ScratchBird::Parser {
  ├── Lexical Analysis (tokenization)
  ├── Syntax Analysis (AST generation)
  ├── Semantic Analysis (type checking)
  └── Code Generation (execution plan)
}
```

### Query Optimizer

**Advanced Optimization Features**
- Cost-based optimization with statistics
- Schema-aware join optimization
- Index selection for new data types
- Query rewriting for hierarchical schemas

**Optimization Phases**
1. **Logical Optimization**: Query tree transformation
2. **Physical Optimization**: Access path selection
3. **Execution Planning**: Operator ordering
4. **Runtime Optimization**: Adaptive query execution

### Storage Engine

**Multi-Generational Architecture (MVCC)**
- Snapshot isolation for concurrent access
- No read locks for better performance
- Automatic garbage collection
- Version-based conflict resolution

**Page Structure**
```
┌─────────────────────────────────────────────────────────┐
│                    Database Page (8KB)                   │
├─────────────────────────────────────────────────────────┤
│  Page Header  │  Schema Info  │  Data Records  │  Free   │
│  (64 bytes)   │  (variable)   │  (variable)    │  Space  │
└─────────────────────────────────────────────────────────┘
```

## Schema Management Architecture

### Hierarchical Schema System

**Schema Metadata Storage**
```sql
-- Enhanced RDB$SCHEMAS table
RDB$SCHEMAS {
  RDB$SCHEMA_NAME         VARCHAR(63)    -- Schema name
  RDB$PARENT_SCHEMA_NAME  VARCHAR(63)    -- Parent schema reference
  RDB$SCHEMA_PATH         VARCHAR(511)   -- Full hierarchical path
  RDB$SCHEMA_LEVEL        INTEGER        -- Nesting depth (0-8)
  RDB$SCHEMA_OWNER        VARCHAR(63)    -- Schema owner
  RDB$DESCRIPTION         BLOB           -- Schema description
  RDB$CREATED_DATE        TIMESTAMP      -- Creation timestamp
  RDB$MODIFIED_DATE       TIMESTAMP      -- Last modification
}
```

**Schema Resolution Engine**
```cpp
class SchemaResolver {
  // High-performance schema path parsing
  SchemaPathCache pathCache;
  
  // Hierarchical name resolution
  bool resolveSchemaPath(const string& path, SchemaInfo& info);
  
  // Context-aware schema lookup
  bool resolveInContext(const string& name, const SchemaContext& ctx);
  
  // Schema validation and integrity
  bool validateHierarchy(const string& parentPath, const string& childName);
};
```

### Database Links Architecture

**Schema-Aware Remote Access**
```cpp
class DatabaseLink {
  // Connection management
  RemoteConnection connection;
  
  // Schema resolution modes
  enum SchemaMode {
    SCHEMA_MODE_NONE = 0,           // No schema awareness
    SCHEMA_MODE_FIXED = 1,          // Fixed remote schema
    SCHEMA_MODE_CONTEXT_AWARE = 2,  // Context-aware resolution
    SCHEMA_MODE_HIERARCHICAL = 3,   // Hierarchical mapping
    SCHEMA_MODE_MIRROR = 4          // Mirror mode
  };
  
  // Remote schema resolution
  string resolveRemoteSchema(const string& localSchema);
  
  // Query execution with schema context
  ResultSet executeQuery(const string& query, const SchemaContext& ctx);
};
```

## Data Type System Architecture

### Type Engine Design

**Type Hierarchy**
```cpp
namespace ScratchBird::Types {
  
  class DataType {
    virtual bool validate(const void* data) = 0;
    virtual size_t getStorageSize() = 0;
    virtual int compare(const void* left, const void* right) = 0;
    virtual string toString(const void* data) = 0;
  };
  
  // Network types
  class InetType : public DataType { /* IPv4/IPv6 addresses */ };
  class CidrType : public DataType { /* Network blocks */ };
  class MacAddrType : public DataType { /* MAC addresses */ };
  
  // Range types
  class RangeType : public DataType { /* Generic range support */ };
  class Int4Range : public RangeType { /* Integer ranges */ };
  class TsRange : public RangeType { /* Timestamp ranges */ };
  
  // UUID support
  class UuidType : public DataType { /* UUID with versioning */ };
  
  // Unsigned integers
  class UInt64Type : public DataType { /* UBIGINT */ };
  class UInt128Type : public DataType { /* UINT128 */ };
}
```

### Storage Format Optimization

**Network Types Storage**
```cpp
// INET type: 17 bytes
struct InetStorage {
  uint8_t family;        // 1 byte: 4 for IPv4, 6 for IPv6
  uint8_t address[16];   // 16 bytes: IPv4 uses first 4 bytes
};

// CIDR type: 18 bytes
struct CidrStorage {
  uint8_t family;        // 1 byte: address family
  uint8_t address[16];   // 16 bytes: network address
  uint8_t prefix_len;    // 1 byte: prefix length
};

// MACADDR type: 6 bytes
struct MacAddrStorage {
  uint8_t octets[6];     // 6 bytes: MAC address octets
};
```

## Performance Architecture

### Caching System

**Multi-Level Cache Hierarchy**
```
┌─────────────────────────────────────────────────────────┐
│                    Cache Architecture                    │
├─────────────────────────────────────────────────────────┤
│  L1: Metadata Cache (Schema, Type Info)                │
│  ├── Schema Path Cache (hierarchical paths)            │
│  ├── Type Descriptor Cache (data type metadata)        │
│  └── Function Cache (built-in functions)               │
├─────────────────────────────────────────────────────────┤
│  L2: Query Cache (Compiled Plans)                      │
│  ├── SQL Statement Cache (parsed queries)              │
│  ├── Execution Plan Cache (optimized plans)            │
│  └── Prepared Statement Cache (client queries)         │
├─────────────────────────────────────────────────────────┤
│  L3: Data Cache (Page Buffer)                          │
│  ├── Page Cache (database pages)                       │
│  ├── Index Cache (B-tree nodes)                        │
│  └── Blob Cache (large objects)                        │
└─────────────────────────────────────────────────────────┘
```

### Concurrency Control

**Lock-Free Data Structures**
```cpp
// Lock-free schema cache
template<typename T>
class LockFreeCache {
  std::atomic<CacheNode<T>*> head;
  
  bool tryInsert(const string& key, const T& value);
  bool tryLookup(const string& key, T& value);
  void evictOldEntries();
};

// Read-Write locks for schema operations
class SchemaLock {
  std::shared_mutex schemaMutex;
  
  void readLock() { schemaMutex.lock_shared(); }
  void writeLock() { schemaMutex.lock(); }
  void unlock() { schemaMutex.unlock(); }
};
```

## Security Architecture

### Authentication System

**Multi-Plugin Authentication**
```cpp
class AuthenticationManager {
  // Plugin registry
  std::vector<AuthPlugin*> plugins;
  
  // Authentication methods
  bool authenticateUser(const string& username, const string& password);
  bool authenticateKerberos(const string& token);
  bool authenticateCertificate(const X509Certificate& cert);
  
  // Session management
  SessionToken createSession(const UserInfo& user);
  bool validateSession(const SessionToken& token);
};
```

### Schema-Level Security

**Hierarchical Permissions**
```sql
-- Schema permissions inherit down the hierarchy
GRANT SELECT ON SCHEMA finance TO user1;
-- Grants access to finance.* (all sub-schemas)

GRANT INSERT ON SCHEMA finance.accounting TO user2;
-- Grants INSERT only on finance.accounting.* sub-schemas

-- Object-level permissions
GRANT UPDATE ON finance.accounting.reports.monthly_data TO user3;
-- Specific table permissions
```

## Network Architecture

### Protocol Design

**ScratchBird Wire Protocol**
```
┌─────────────────────────────────────────────────────────┐
│                    Network Protocol Stack                │
├─────────────────────────────────────────────────────────┤
│  Application Layer                                      │
│  ┌─────────────────────────────────────────────────┐   │
│  │  SQL Commands  │  Result Sets  │  Transactions  │   │
│  └─────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────┤
│  Session Layer                                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Authentication │  Encryption  │  Compression   │   │
│  └─────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────┤
│  Transport Layer                                        │
│  ┌─────────────────────────────────────────────────┐   │
│  │  TCP/IP (4050)  │  Named Pipes │  Shared Memory │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### Connection Management

**Connection Pooling**
```cpp
class ConnectionPool {
  // Pool configuration
  size_t maxConnections;
  size_t minConnections;
  std::chrono::seconds connectionTimeout;
  
  // Connection lifecycle
  Connection* acquireConnection();
  void releaseConnection(Connection* conn);
  void closeIdleConnections();
  
  // Schema-aware routing
  Connection* getConnectionForSchema(const string& schemaPath);
};
```

## Build System Architecture

### Modern C++17 Build Pipeline

**Cross-Platform Build Strategy**
```bash
# Build components
├── Core Engine (libsbengine)
│   ├── SQL Parser & Optimizer
│   ├── Storage Engine
│   ├── Type System
│   └── Schema Manager
├── Client Library (libsbclient)
│   ├── API Bindings
│   ├── Network Protocol
│   └── Connection Management
├── Utilities (sb_*)
│   ├── sb_isql (Interactive SQL)
│   ├── sb_gbak (Backup/Restore)
│   ├── sb_gstat (Statistics)
│   ├── sb_gfix (Maintenance)
│   └── sb_gsec (Security)
└── Server (sb_server)
    ├── Network Service
    ├── Process Management
    └── Resource Monitoring
```

### Deployment Architecture

**Production Deployment**
```
┌─────────────────────────────────────────────────────────┐
│                   Production Environment                 │
├─────────────────────────────────────────────────────────┤
│  Load Balancer (HAProxy/Nginx)                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Port 4050 → ScratchBird Instances             │   │
│  │  Health Checks & Failover                       │   │
│  └─────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────┤
│  ScratchBird Instances                                  │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐     │
│  │  sb_server  │ │  sb_server  │ │  sb_server  │     │
│  │  (Primary)  │ │  (Replica)  │ │  (Replica)  │     │
│  └─────────────┘ └─────────────┘ └─────────────┘     │
├─────────────────────────────────────────────────────────┤
│  Storage Layer                                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Database Files  │  Backup Storage  │  Logs     │   │
│  │  (SSD/NVMe)      │  (Network/Cloud) │  (Local)  │   │
│  └─────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────┤
│  Monitoring & Management                                │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Prometheus  │  Grafana  │  Log Aggregation     │   │
│  │  (Metrics)   │  (Dashboards) │  (ELK Stack)     │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

## Future Architecture Considerations

### Scalability Enhancements
- Horizontal scaling with sharding
- Read replicas with schema awareness
- Distributed query execution
- Cloud-native deployment options

### Performance Optimizations
- Columnar storage for analytics
- Parallel query execution
- Advanced caching strategies
- GPU-accelerated operations

### Integration Capabilities
- REST API server
- GraphQL endpoint
- Message queue integration
- Real-time streaming support

This architecture provides a solid foundation for current v0.5.0 features while maintaining flexibility for future enhancements and scaling requirements.