# ScratchBird Detailed Phase Breakdown
## Complete Step-by-Step Implementation Guide

## Overview
This document provides detailed specifications for each phase. One AI implements the code, another AI creates the tests. Each phase must support page sizes: 8K, 16K, 32K, 64K, 128K.

---

## ALPHA 1.01 - Database Creation Foundation

### Alpha 1.01.1 - Basic Database Creation
**Implementation Requirements:**
```cpp
class DatabaseCreator {
    Status createDatabase(const string& path, size_t page_size);
    Status validatePageSize(size_t page_size);  // Must be 8192, 16384, 32768, 65536, 131072
    Status writeHeader(const DatabaseHeader& header);
    Status initializeSchemaTree();
    Status createSystemTables();
};
```

**Test Requirements:**
```cpp
// Test for EACH page size
TEST_P(DatabaseCreationTest, CreateWithPageSize) {
    size_t page_size = GetParam();  // 8K, 16K, 32K, 64K, 128K
    
    // Test database creation
    ASSERT_OK(createDatabase("test.sdb", page_size));
    
    // Verify file structure
    ASSERT_TRUE(verifyHeader(page_size));
    ASSERT_TRUE(verifySchemaTree());
    ASSERT_TRUE(verifySystemTables());
    ASSERT_TRUE(verifyUUIDs());
}
```

### Alpha 1.01.2 - Encrypted Database Creation
**Implementation Requirements:**
```cpp
class EncryptedDatabaseCreator {
    Status createEncryptedDatabase(const string& path, size_t page_size, const EncryptionKey& key);
    Status encryptPage(const Page& page, const EncryptionKey& key);
    Status writeEncryptedHeader(const DatabaseHeader& header, const EncryptionKey& key);
};
```

**Test Requirements:**
```cpp
TEST_P(EncryptedDatabaseTest, CreateEncrypted) {
    size_t page_size = GetParam();
    EncryptionKey key("test-key-123");
    
    ASSERT_OK(createEncryptedDatabase("encrypted.sdb", page_size, key));
    ASSERT_FALSE(canReadWithoutKey("encrypted.sdb"));
    ASSERT_TRUE(canReadWithKey("encrypted.sdb", key));
}
```

### Alpha 1.01.3 - Database Open/Close
**Implementation Requirements:**
```cpp
class DatabaseManager {
    Status openDatabase(const string& path);
    Status closeDatabase();
    Status verifyChecksum();
    DatabaseInfo getDatabaseInfo();
};
```

**Test Requirements:**
```cpp
TEST_P(DatabaseLifecycleTest, OpenClose) {
    size_t page_size = GetParam();
    
    createDatabase("test.sdb", page_size);
    ASSERT_OK(openDatabase("test.sdb"));
    
    auto info = getDatabaseInfo();
    ASSERT_EQ(info.page_size, page_size);
    
    ASSERT_OK(closeDatabase());
    ASSERT_OK(openDatabase("test.sdb"));  // Reopen
}
```

---

## ALPHA 1.02 - DDL API Layer

### Alpha 1.02.1 - DDL API Interface Definition
**Implementation Requirements:**
```cpp
class DDL_API {
    // Schema operations
    virtual Status createSchema(const string& path) = 0;
    virtual Status dropSchema(const string& path) = 0;
    virtual Status alterSchema(const string& path, const SchemaOptions& opts) = 0;
    virtual bool schemaExists(const string& path) = 0;
    
    // Return values for testing (echo mode)
    virtual SchemaInfo getLastCreatedSchema() = 0;
};
```

**Test Requirements:**
```cpp
TEST(DDL_API_Test, InterfaceEcho) {
    DDL_API* api = createDDL_API();
    
    // Test echo mode - API returns what was given
    auto result = api->createSchema("[root].[app].[test]");
    ASSERT_EQ(result.status, Status::OK);
    
    auto info = api->getLastCreatedSchema();
    ASSERT_EQ(info.path, "[root].[app].[test]");
}
```

### Alpha 1.02.2 - Table DDL Interface
**Implementation Requirements:**
```cpp
class DDL_API {
    virtual Status createTable(const TableDefinition& def) = 0;
    virtual Status dropTable(const string& path) = 0;
    virtual Status alterTable(const string& path, const AlterSpec& spec) = 0;
    virtual bool tableExists(const string& path) = 0;
};
```

### Alpha 1.02.3 - Column DDL Interface
**Implementation Requirements:**
```cpp
class DDL_API {
    virtual Status addColumn(const string& table, const ColumnDef& col) = 0;
    virtual Status dropColumn(const string& table, const string& column) = 0;
    virtual Status alterColumn(const string& table, const string& column, const ColumnDef& newDef) = 0;
};
```

### Alpha 1.02.4 - Index DDL Interface
**Implementation Requirements:**
```cpp
class DDL_API {
    virtual Status createIndex(const IndexDefinition& def) = 0;
    virtual Status dropIndex(const string& name) = 0;
    virtual Status rebuildIndex(const string& name) = 0;
};
```

### Alpha 1.02.5 - Constraint DDL Interface
**Implementation Requirements:**
```cpp
class DDL_API {
    virtual Status addConstraint(const string& table, const Constraint& c) = 0;
    virtual Status dropConstraint(const string& table, const string& name) = 0;
    virtual Status enableConstraint(const string& table, const string& name) = 0;
    virtual Status disableConstraint(const string& table, const string& name) = 0;
};
```

---

## ALPHA 1.03 - DML API Layer

### Alpha 1.03.1 - DML API Interface Definition
**Implementation Requirements:**
```cpp
class DML_API {
    virtual Status insert(const string& table, const Row& row) = 0;
    virtual Status update(const string& table, const Row& row, const Predicate& where) = 0;
    virtual Status deleteRows(const string& table, const Predicate& where) = 0;
    virtual ResultSet select(const string& table, const vector<string>& columns, const Predicate& where) = 0;
};
```

### Alpha 1.03.2 - Transaction Interface
**Implementation Requirements:**
```cpp
class DML_API {
    virtual TransactionId beginTransaction(const TxnOptions& opts) = 0;
    virtual Status commit(TransactionId txn) = 0;
    virtual Status rollback(TransactionId txn) = 0;
    virtual Status savepoint(const string& name) = 0;
    virtual Status rollbackToSavepoint(const string& name) = 0;
};
```

### Alpha 1.03.3 - Bulk Operations Interface
**Implementation Requirements:**
```cpp
class DML_API {
    virtual Status bulkInsert(const string& table, const vector<Row>& rows) = 0;
    virtual Status bulkUpdate(const string& table, const vector<Row>& rows, const Predicate& where) = 0;
    virtual Status bulkDelete(const string& table, const vector<Predicate>& conditions) = 0;
};
```

---

## ALPHA 1.04 - Sequential API Implementation

### Alpha 1.04.1 - createSchema Implementation
**Implementation Requirements:**
```cpp
Status DDL_API_Impl::createSchema(const string& path) {
    // Parse path into components
    auto components = parsePath(path);
    
    // Generate UUID
    UUID schema_uuid = generateUUID();
    
    // Update schema tree in database file
    updateSchemaTree(components, schema_uuid);
    
    // Update system tables
    insertIntoSysSchemas(path, schema_uuid);
    
    return Status::OK;
}
```

**Test Requirements:**
```cpp
TEST_P(CreateSchemaTest, Implementation) {
    size_t page_size = GetParam();
    
    createDatabase("test.sdb", page_size);
    DDL_API* api = openDatabase("test.sdb");
    
    ASSERT_OK(api->createSchema("[root].[app].[myapp]"));
    
    // Verify in file
    DatabaseFile file("test.sdb");
    ASSERT_TRUE(file.hasSchema("[root].[app].[myapp]"));
    
    // Verify UUID assigned
    auto uuid = file.getSchemaUUID("[root].[app].[myapp]");
    ASSERT_FALSE(uuid.isNil());
    
    // Verify in system tables
    auto result = api->select("[root].[sys].schemas", {"*"}, "path = '[root].[app].[myapp]'");
    ASSERT_EQ(result.rowCount(), 1);
}
```

### Alpha 1.04.2 - dropSchema Implementation
**Implementation Requirements:**
```cpp
Status DDL_API_Impl::dropSchema(const string& path) {
    // Check if empty
    if (!isSchemaEmpty(path)) {
        return Status::SchemaNotEmpty;
    }
    
    // Remove from schema tree
    removeFromSchemaTree(path);
    
    // Remove from system tables
    deleteFromSysSchemas(path);
    
    return Status::OK;
}
```

### Alpha 1.04.3 - createTable Implementation
**Implementation Requirements:**
```cpp
Status DDL_API_Impl::createTable(const TableDefinition& def) {
    // Generate table UUID
    UUID table_uuid = generateUUID();
    
    // Allocate heap pages
    PageNumber heap_start = allocatePages(INITIAL_HEAP_PAGES);
    
    // Create table entry
    createTableEntry(def, table_uuid, heap_start);
    
    // Create column entries
    for (const auto& col : def.columns) {
        createColumnEntry(table_uuid, col);
    }
    
    return Status::OK;
}
```

### Alpha 1.04.4 - insert Implementation
**Implementation Requirements:**
```cpp
Status DML_API_Impl::insert(const string& table, const Row& row) {
    // Get table info
    auto table_info = getTableInfo(table);
    
    // Validate row against schema
    validateRow(row, table_info);
    
    // Generate tuple UUID
    UUID tuple_uuid = generateUUID();
    
    // Find heap page with space
    PageNumber page = findHeapPageWithSpace(table_info);
    
    // Insert tuple
    insertTupleIntoPage(page, tuple_uuid, row);
    
    // Update indexes
    updateIndexes(table_info, tuple_uuid, row);
    
    return Status::OK;
}
```

### Alpha 1.04.5 - select Implementation
**Implementation Requirements:**
```cpp
ResultSet DML_API_Impl::select(const string& table, const vector<string>& columns, const Predicate& where) {
    // Get table info
    auto table_info = getTableInfo(table);
    
    // Plan scan strategy
    ScanPlan plan = createScanPlan(table_info, where);
    
    // Execute scan
    ResultSet results;
    for (PageNumber page : plan.pages) {
        scanPage(page, where, columns, results);
    }
    
    return results;
}
```

### Alpha 1.04.6 - beginTransaction Implementation
**Implementation Requirements:**
```cpp
TransactionId DML_API_Impl::beginTransaction(const TxnOptions& opts) {
    // Generate transaction ID
    TransactionId txn_id = getNextTransactionId();
    
    // Create transaction context
    TransactionContext ctx{
        .id = txn_id,
        .start_time = getCurrentTime(),
        .isolation_level = opts.isolation_level
    };
    
    // Register in TIP (Transaction Inventory Page)
    registerTransaction(txn_id);
    
    return txn_id;
}
```

### Alpha 1.04.7 - createIndex Implementation
**Implementation Requirements:**
```cpp
Status DDL_API_Impl::createIndex(const IndexDefinition& def) {
    // Generate index UUID
    UUID index_uuid = generateUUID();
    
    // Allocate B-tree root page
    PageNumber root_page = allocatePage();
    
    // Initialize B-tree
    initializeBTree(root_page, def);
    
    // Populate index from existing data
    populateIndex(index_uuid, def);
    
    // Register in system tables
    insertIntoSysIndexes(def, index_uuid, root_page);
    
    return Status::OK;
}
```

[Continue for all 1.04.x implementations...]

---

## Test Progress Tracking Structure

```
ProjectPlan/
├── progress/
│   ├── implementation/
│   │   ├── alpha_1_01_1.impl.log.md
│   │   ├── alpha_1_01_2.impl.log.md
│   │   └── ...
│   └── testing/
│       ├── alpha_1_01_1.test.log.md
│       ├── alpha_1_01_2.test.log.md
│       └── ...
```

---

## Parallel Development Guidelines

### For Implementation AI:
1. Read phase specification
2. Implement required classes/methods
3. Ensure all 5 page sizes supported
4. Update implementation log
5. Mark ready for testing

### For Testing AI:
1. Read phase specification
2. Create comprehensive tests
3. Test all 5 page sizes
4. Test error conditions
5. Update test log
6. Report pass/fail to implementation

### Synchronization Points:
- Implementation complete → Testing begins
- Tests fail → Implementation fixes
- Tests pass → Move to next phase

This structure ensures clear separation of concerns and parallel development!