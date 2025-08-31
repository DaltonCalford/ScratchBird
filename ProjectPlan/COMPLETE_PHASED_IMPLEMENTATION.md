# ScratchBird Complete Phased Implementation Plan
## Incremental API-First Development Strategy

## Overview

Build ScratchBird incrementally through API layers, testing each component thoroughly before moving to the next. Each API call is implemented, tested against actual database files, and verified for all supported page sizes.

---

## ALPHA PHASE 1: Core Engine API (1.01 - 1.05)
### Foundation: DDL, DML, Security, Schema Navigation

### Alpha 1.01 - Database Creation & Encryption
#### A 1.01.1 - Basic Database Creation
```cpp
// API Definition
class EngineAPI {
    // Database lifecycle
    virtual Status createDatabase(const string& path, const DatabaseOptions& options) = 0;
    virtual Status openDatabase(const string& path) = 0;
    virtual Status closeDatabase() = 0;
    virtual string getVersion() = 0;
};

// Test verification
TEST(Alpha_1_01_1, CreateDatabase) {
    for (auto page_size : {8192, 16384, 32768}) {
        DatabaseOptions opts{.page_size = page_size};
        ASSERT_OK(engine->createDatabase("test.sdb", opts));
        
        // Verify file structure
        DatabaseFile file("test.sdb");
        ASSERT_EQ(file.readHeader().page_size, page_size);
        ASSERT_TRUE(file.verifySchemaTree());
        ASSERT_TRUE(file.verifySystemTables());
    }
}
```

#### A 1.01.2 - Database Encryption
```cpp
// Extended API
class EngineAPI {
    virtual Status createEncryptedDatabase(
        const string& path, 
        const DatabaseOptions& options,
        const EncryptionKey& key
    ) = 0;
};

// Test verification
TEST(Alpha_1_01_2, CreateEncryptedDatabase) {
    EncryptionKey key("my-secret-key");
    ASSERT_OK(engine->createEncryptedDatabase("encrypted.sdb", opts, key));
    
    // Verify encrypted
    ASSERT_FALSE(canReadWithoutKey("encrypted.sdb"));
    ASSERT_TRUE(canReadWithKey("encrypted.sdb", key));
}
```

### Alpha 1.02 - DDL API Interface
```cpp
class DDL_API {
    // Schema operations
    virtual Status createSchema(const string& schemaPath) = 0;
    virtual Status dropSchema(const string& schemaPath) = 0;
    virtual Status alterSchema(const string& schemaPath, const SchemaOptions& opts) = 0;
    
    // Table operations
    virtual Status createTable(const TableDefinition& def) = 0;
    virtual Status dropTable(const string& tablePath) = 0;
    virtual Status alterTable(const string& tablePath, const AlterTableSpec& spec) = 0;
    
    // Column operations
    virtual Status addColumn(const string& tablePath, const ColumnDefinition& col) = 0;
    virtual Status dropColumn(const string& tablePath, const string& columnName) = 0;
    virtual Status alterColumn(const string& tablePath, const string& columnName, const ColumnDefinition& newDef) = 0;
    
    // Index operations
    virtual Status createIndex(const IndexDefinition& def) = 0;
    virtual Status dropIndex(const string& indexPath) = 0;
    
    // Constraint operations
    virtual Status addConstraint(const string& tablePath, const Constraint& constraint) = 0;
    virtual Status dropConstraint(const string& tablePath, const string& constraintName) = 0;
};

// Test - API returns what was given
TEST(Alpha_1_02, DDL_API_Echo) {
    TableDefinition def{
        .name = "test_table",
        .schema = "[root].[app]",
        .columns = {
            {"id", DataType::INTEGER, {.primary_key = true}},
            {"name", DataType::VARCHAR, {.length = 100}}
        }
    };
    
    // At this stage, just verify API accepts and returns
    auto result = ddl_api->createTable(def);
    ASSERT_EQ(result.table_name, def.name);
    ASSERT_EQ(result.column_count, 2);
}
```

### Alpha 1.03 - DML API Interface
```cpp
class DML_API {
    // Data operations
    virtual Status insert(const string& tablePath, const Row& row) = 0;
    virtual Status update(const string& tablePath, const Row& row, const Predicate& where) = 0;
    virtual Status delete(const string& tablePath, const Predicate& where) = 0;
    virtual ResultSet select(const string& tablePath, const vector<string>& columns, const Predicate& where) = 0;
    
    // Transaction operations
    virtual TransactionId beginTransaction(const TransactionOptions& opts) = 0;
    virtual Status commit(TransactionId txn) = 0;
    virtual Status rollback(TransactionId txn) = 0;
    
    // Bulk operations
    virtual Status bulkInsert(const string& tablePath, const vector<Row>& rows) = 0;
    virtual Status bulkUpdate(const string& tablePath, const vector<Row>& rows, const Predicate& where) = 0;
};

// Test - API echo
TEST(Alpha_1_03, DML_API_Echo) {
    Row row{
        {"id", Value(1)},
        {"name", Value("test")}
    };
    
    auto result = dml_api->insert("[root].[app].test_table", row);
    ASSERT_EQ(result.rows_affected, 1);
}
```

### Alpha 1.04.x - Sequential API Implementation
Each sub-version implements ONE API method completely:

#### A 1.04.1 - createSchema Implementation
```cpp
TEST(Alpha_1_04_1, CreateSchema_Implementation) {
    // Create schema via API
    ASSERT_OK(ddl_api->createSchema("[root].[app].[myapp]"));
    
    // Read database file directly and verify
    DatabaseFile file("test.sdb");
    SchemaTree tree = file.readSchemaTree();
    
    ASSERT_TRUE(tree.hasSchema("[root].[app].[myapp]"));
    ASSERT_FALSE(tree.getSchemaUUID("[root].[app].[myapp]").isNil());
    
    // Verify in system tables
    auto schemas = file.readSystemTable("sys.schemas");
    ASSERT_TRUE(schemas.hasRow("path", "[root].[app].[myapp]"));
}
```

#### A 1.04.2 - createTable Implementation
```cpp
TEST(Alpha_1_04_2, CreateTable_Implementation) {
    TableDefinition def{...};
    ASSERT_OK(ddl_api->createTable(def));
    
    // Verify in file
    DatabaseFile file("test.sdb");
    ASSERT_TRUE(file.hasTable("[root].[app].customers"));
    
    // Verify structure
    auto table = file.readTableStructure("[root].[app].customers");
    ASSERT_EQ(table.column_count, 2);
    ASSERT_EQ(table.columns[0].name, "id");
    ASSERT_EQ(table.columns[0].type, DataType::INTEGER);
}
```

#### A 1.04.3 - insert Implementation
```cpp
TEST(Alpha_1_04_3, Insert_Implementation) {
    Row row{{"id", 1}, {"name", "John"}};
    ASSERT_OK(dml_api->insert("[root].[app].customers", row));
    
    // Read file directly
    DatabaseFile file("test.sdb");
    auto heap = file.readHeapPages("[root].[app].customers");
    
    ASSERT_EQ(heap.tuple_count, 1);
    auto tuple = heap.readTuple(0);
    ASSERT_EQ(tuple.getValue("id").asInt(), 1);
    ASSERT_EQ(tuple.getValue("name").asString(), "John");
}
```

#### A 1.04.4 - select Implementation
```cpp
TEST(Alpha_1_04_4, Select_Implementation) {
    // Insert test data
    dml_api->insert("[root].[app].customers", {{"id", 1}, {"name", "John"}});
    dml_api->insert("[root].[app].customers", {{"id", 2}, {"name", "Jane"}});
    
    // Select via API
    auto result = dml_api->select(
        "[root].[app].customers",
        {"id", "name"},
        Predicate("id > 0")
    );
    
    ASSERT_EQ(result.row_count, 2);
    ASSERT_EQ(result.rows[0]["name"].asString(), "John");
}
```

Continue for EVERY API method...

### Alpha 1.05 - Complete Core API
All fundamental operations implemented and tested:
- ✅ All DDL operations
- ✅ All DML operations  
- ✅ Transaction support
- ✅ Basic indexing
- ✅ Constraints
- ✅ Schema navigation
- ✅ Security checks

---

## ALPHA PHASE 2: Schema & Security (1.1 - 1.3)

### Alpha 1.1.x - Schema Navigation API
```cpp
class SchemaAPI {
    virtual vector<string> listSchemas(const string& parent = "[root]") = 0;
    virtual vector<string> listTables(const string& schema) = 0;
    virtual vector<string> listColumns(const string& table) = 0;
    virtual SchemaInfo getSchemaInfo(const string& path) = 0;
    virtual TableInfo getTableInfo(const string& path) = 0;
    virtual Status setCurrentSchema(const string& path) = 0;
    virtual string getCurrentSchema() = 0;
    virtual Status setSearchPath(const vector<string>& path) = 0;
};
```

### Alpha 1.2.x - Triggers & Events
```cpp
class TriggerAPI {
    virtual Status createTrigger(const TriggerDefinition& def) = 0;
    virtual Status dropTrigger(const string& triggerPath) = 0;
    virtual Status enableTrigger(const string& triggerPath) = 0;
    virtual Status disableTrigger(const string& triggerPath) = 0;
    virtual Status setTriggerPosition(const string& triggerPath, int position) = 0;
    
    // Events
    virtual Status postEvent(const string& eventName, const Value& payload) = 0;
    virtual EventHandle subscribeToEvent(const string& eventName, EventCallback callback) = 0;
    virtual Status unsubscribe(EventHandle handle) = 0;
};
```

### Alpha 1.3.x - Security API
```cpp
class SecurityAPI {
    virtual Status createUser(const UserDefinition& user) = 0;
    virtual Status createRole(const RoleDefinition& role) = 0;
    virtual Status grant(const Permission& perm) = 0;
    virtual Status revoke(const Permission& perm) = 0;
    virtual bool checkPermission(const string& user, const string& object, const string& action) = 0;
    virtual Status setConnectionPolicy(const ConnectionPolicy& policy) = 0;
};

// Core security enforcement
TEST(Alpha_1_3, Security_Enforcement) {
    // Create user with limited permissions
    security_api->createUser({"john", "password"});
    security_api->grant({"john", "[root].[app]", "SELECT"});
    
    // Switch to user context
    engine->setCurrentUser("john");
    
    // Should succeed
    ASSERT_OK(dml_api->select("[root].[app].customers", {}, {}));
    
    // Should fail - no INSERT permission
    ASSERT_ERROR(dml_api->insert("[root].[app].customers", row));
}
```

---

## ALPHA PHASE 3: Parser & BLR (2.0 - 4.0)

### Alpha 2.x - SQL Parser Development
```cpp
class SQLParser {
    virtual AST parse(const string& sql) = 0;
    virtual bool validate(const string& sql) = 0;
    virtual vector<Token> tokenize(const string& sql) = 0;
};

// Progressive parser development
// A 2.1 - Basic SELECT
// A 2.2 - INSERT/UPDATE/DELETE
// A 2.3 - CREATE/ALTER/DROP
// A 2.4 - JOINs
// A 2.5 - Subqueries
// A 2.6 - CTEs
// A 2.7 - Window functions
```

### Alpha 3.x - BLR Generator
```cpp
class BLRGenerator {
    virtual BLRProgram generate(const AST& ast) = 0;
    virtual BLRProgram optimize(const BLRProgram& program) = 0;
    virtual string decompile(const BLRProgram& program) = 0;
};
```

### Alpha 4.x - BLR Executor
```cpp
class BLRExecutor {
    virtual ExecutionResult execute(const BLRProgram& program) = 0;
    virtual ExecutionPlan explain(const BLRProgram& program) = 0;
    virtual Status prepare(const BLRProgram& program) = 0;
};
```

---

## ALPHA PHASE 5: Integration (5.0.x)

### Alpha 5.0.x - Complete Embedded Engine
```cpp
TEST(Alpha_5_0, Complete_Embedded_Engine) {
    // Create database
    EmbeddedEngine engine;
    engine.createDatabase("test.sdb");
    
    // Parse SQL to BLR
    auto ast = parser.parse("CREATE TABLE test (id INTEGER PRIMARY KEY)");
    auto blr = generator.generate(ast);
    
    // Execute BLR
    engine.execute(blr);
    
    // Verify with SQL query
    auto result = engine.query("SELECT * FROM test");
    ASSERT_TRUE(result.success);
    
    // Test triggers
    engine.execute("CREATE TRIGGER test_trigger AFTER INSERT ON test BEGIN POST_EVENT 'inserted'; END");
    
    // Test events
    bool event_received = false;
    engine.subscribe("inserted", [&](Event e) { event_received = true; });
    engine.execute("INSERT INTO test VALUES (1)");
    ASSERT_TRUE(event_received);
}
```

### Alpha 5.1.x - ScratchBird to ScratchBird
```cpp
class RemoteConnection {
    virtual Status connect(const string& host, int port) = 0;
    virtual Status authenticate(const string& user, const string& password) = 0;
    virtual ExecutionResult executeRemote(const BLRProgram& program) = 0;
};
```

### Alpha 5.2.x - Background Agents
```cpp
class AgentManager {
    virtual AgentHandle createAgent(const AgentDefinition& def) = 0;
    virtual Status startAgent(AgentHandle handle) = 0;
    virtual Status stopAgent(AgentHandle handle) = 0;
    virtual AgentStatus getStatus(AgentHandle handle) = 0;
};
```

---

## BETA PHASE: Hardening & Optimization

### Beta 1.x - Bug Fixes & Stability
- Fix all issues found in Alpha
- Memory leak detection
- Thread safety verification
- Crash recovery testing

### Beta 2.x - Performance Optimization
- Query optimization improvements
- Index selection algorithms
- Buffer pool tuning
- Parallel execution

### Beta 3.x - Additional Index Types
```cpp
class IndexAPI {
    virtual Status createBitmapIndex(...) = 0;
    virtual Status createGINIndex(...) = 0;
    virtual Status createGiSTIndex(...) = 0;
    virtual Status createHashIndex(...) = 0;
};
```

### Beta 4.x - Monitoring & Metrics
```cpp
class MonitoringAPI {
    virtual QueryStats getQueryStats() = 0;
    virtual BufferPoolStats getBufferStats() = 0;
    virtual TransactionStats getTransactionStats() = 0;
    virtual Status enableProfiling() = 0;
    virtual Profile getProfile(QueryId id) = 0;
};
```

### Beta 5.0 - 5.7 - Protocol Implementation
#### Beta 5.1 - Firebird Wire Protocol
#### Beta 5.2 - PostgreSQL Wire Protocol  
#### Beta 5.3 - MySQL Wire Protocol
#### Beta 5.4 - MariaDB Compatibility
#### Beta 5.5 - Y-Valve Complete
#### Beta 5.6 - Protocol Auto-Detection
#### Beta 5.7 - Full Compatibility Testing

### Beta 5.8+ - Advanced Features
#### Beta 5.8 - Clustering Foundation
```cpp
class ClusterAPI {
    virtual Status joinCluster(const ClusterConfig& config) = 0;
    virtual Status addNode(const NodeDefinition& node) = 0;
    virtual Status removeNode(const string& nodeId) = 0;
    virtual ClusterStatus getClusterStatus() = 0;
};
```

#### Beta 5.9 - Sharding Support
```cpp
class ShardingAPI {
    virtual Status createShardKey(const ShardKeyDefinition& def) = 0;
    virtual Status distributeTable(const string& table, const ShardingStrategy& strategy) = 0;
    virtual ShardMap getShardMap(const string& table) = 0;
};
```

---

## RELEASE CANDIDATE: Production Ready

### RC 1.0 - Feature Complete
- All planned features implemented
- All APIs stable
- Documentation complete

### RC 2.0 - Performance Validated
- TPC-C benchmark passing
- TPC-H benchmark passing
- Real-world workload testing

### RC 3.0 - Security Certified
- Security audit passed
- Penetration testing complete
- Compliance verified (GDPR, HIPAA, etc.)

### RC 4.0 - Production Hardened
- 30-day stability test
- Chaos engineering validated
- Disaster recovery tested

### RC 5.0 - Release Ready
- All tests passing 100%
- Performance goals met
- Documentation finalized
- Migration tools ready

---

## Testing Strategy

### Every API Method Gets:
1. **Unit Test** - Method works in isolation
2. **Integration Test** - Method works with others
3. **File Verification Test** - Database file correctly modified
4. **Multi-Page-Size Test** - Works with 8K, 16K, 32K pages
5. **Encryption Test** - Works with encrypted databases
6. **Security Test** - Permissions enforced
7. **Concurrent Test** - Thread-safe operation
8. **Performance Test** - Meets performance targets

### Test Example Structure:
```cpp
TEST_P(APITest, MethodName) {
    auto page_size = GetParam();  // 8192, 16384, 32768
    
    // Create database with page size
    createTestDatabase(page_size);
    
    // Execute API method
    auto result = api->method(...);
    
    // Verify result
    ASSERT_OK(result);
    
    // Verify database file
    DatabaseFile file("test.sdb");
    ASSERT_TRUE(file.verifyStructure());
    
    // Verify with different API
    auto verification = other_api->verify(...);
    ASSERT_TRUE(verification);
}

INSTANTIATE_TEST_SUITE_P(
    PageSizes,
    APITest,
    ::testing::Values(8192, 16384, 32768)
);
```

---

## Success Metrics

### Alpha Complete When:
- All core APIs implemented
- Embedded engine fully functional
- Parser generates correct BLR
- BLR executor works
- Basic clustering works

### Beta Complete When:
- All protocols supported
- Performance targets met
- No critical bugs
- Monitoring complete
- Clustering stable

### RC Complete When:
- 100% test pass rate
- 30-day stability proven
- Security certified
- Performance validated
- Production ready

---

## Development Principles

1. **API First** - Define interface, then implement
2. **Test Everything** - Every method, every scenario
3. **Incremental Progress** - One feature at a time
4. **File Verification** - Always verify database file changes
5. **Multi-Page Support** - Test all page sizes always
6. **Security by Default** - Permissions checked at core
7. **Performance Awareness** - Measure everything
8. **Documentation Parallel** - Document as we build

## Progress Tracking

### READ-ONLY Plan vs Progress Logs

This implementation plan is **READ-ONLY** - it defines what needs to be built but is never modified based on progress.

Actual implementation progress is tracked in:
- `progress/` directory - Contains all work logs
- `progress/alpha_X_XX_X.log.md` - One log file per version
- `progress/PROGRESS_LOG_TEMPLATE.md` - Template for new logs

### How to Track Progress

1. **Before starting work**: Read the relevant section of this plan
2. **Create progress log**: Copy template to new version log
3. **During work**: Append progress to the log file
4. **After each session**: Commit the updated log
5. **Never modify**: This plan or previous log entries

### Progress Structure

```
ProjectPlan/
├── COMPLETE_PHASED_IMPLEMENTATION.md  # This file (READ-ONLY)
├── Alpha_101_Specification.md         # Detailed specs (READ-ONLY)
└── progress/                          # All progress tracking
    ├── README.md                      # How to use progress logs
    ├── PROGRESS_LOG_TEMPLATE.md       # Template for new logs
    ├── alpha_1_01_1.log.md           # Actual work log
    └── ...                           # One log per version
```

### Key Rules

- **Plan = WHERE we're going** (this document)
- **Logs = WHERE we are** (progress directory)
- **Plan is immutable** - Never changes based on reality
- **Logs are append-only** - Never modify past entries
- **File verification mandatory** - Every log must verify database changes

This incremental approach ensures solid foundation at each step!