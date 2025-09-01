# Phase 26: UUID-Based Schema System

## Objective
Implement UUID-based object identification with hierarchical namespaces for multi-database support.

## Prerequisites
- Phase 25 complete (Y-Valve framework)

## Tasks

### 26.1 UUID Object System
```cpp
struct DatabaseObject {
    UUID id;                  // Immutable identifier
    string name;              // Current name (mutable)
    ObjectType type;          // Table, View, Index, etc.
    UUID parent_namespace;    // Hierarchical location
    uint64_t version;         // Schema version
    
    // Rename doesn't change UUID
    void rename(string new_name) {
        name = new_name;
        version++;
        // No need to update dependent objects
    }
};
```

### 26.2 Hierarchical Namespaces
```cpp
class Namespace {
    UUID id;
    string name;
    UUID parent;
    map<string, UUID> children;  // Name to UUID mapping
    
    // Mount external schema
    void mount(string mount_point, UUID external_root) {
        children[mount_point] = external_root;
    }
    
    // Resolve path to UUID
    UUID resolve(vector<string> path) {
        // catalog.schema.table -> UUID
    }
};
```

### 26.3 BLR with UUIDs
```cpp
// Binary Language Representation uses UUIDs
struct BLRInstruction {
    OpCode op;
    UUID object_id;  // Not name
    
    // Rename-proof execution
    void execute() {
        auto object = catalog.get_by_uuid(object_id);
        // Works regardless of current name
    }
};

// Stored procedures/views reference UUIDs
struct StoredProcedure {
    UUID id;
    vector<BLRInstruction> body;
    // No recompilation on rename
};
```

### 26.4 Multi-Database Views
```cpp
class SchemaView {
    // MySQL client sees MySQL-style information_schema
    Namespace create_mysql_view(UUID root) {
        auto ns = Namespace("information_schema");
        ns.add_table("tables", mysql_tables_view);
        ns.add_table("columns", mysql_columns_view);
        return ns;
    }
    
    // PostgreSQL client sees pg_catalog
    Namespace create_pg_view(UUID root) {
        auto ns = Namespace("pg_catalog");
        ns.add_table("pg_class", pg_class_view);
        ns.add_table("pg_attribute", pg_attribute_view);
        return ns;
    }
};
```

### 26.5 Federation Mounting
```cpp
class FederationManager {
    // Mount remote MySQL database
    void mount_mysql(string local_path, MySQLConnection remote) {
        auto schema = discover_remote_schema(remote);
        auto mount_uuid = generate_uuid();
        
        // Create virtual namespace
        namespaces.mount(local_path, mount_uuid);
        
        // Map remote objects to UUIDs
        for (auto& table : schema.tables) {
            auto uuid = generate_uuid();
            remote_mappings[uuid] = {remote, table};
        }
    }
};
```

## Files to Create
- `include/scratchbird/schema/uuid_catalog.h`
- `src/schema/namespace_manager.cpp`
- `src/schema/uuid_resolver.cpp`
- `src/schema/schema_views.cpp`

## Validation Tests
```cpp
// Create table and get UUID
auto uuid = execute("CREATE TABLE test (id INT)").object_uuid;

// Rename doesn't change UUID
execute("ALTER TABLE test RENAME TO test_new");
auto obj = catalog.get_by_uuid(uuid);
assert(obj.name == "test_new");
assert(obj.id == uuid);  // Same UUID

// Stored procedure still works after rename
execute("CREATE PROCEDURE p() SELECT * FROM test");
execute("ALTER TABLE test RENAME TO renamed");
execute("CALL p()");  // Still works - uses UUID

// Mount remote database
mount_mysql("/remote/mysql_db", mysql_connection);
execute("SELECT * FROM `/remote/mysql_db`.users");  // Federated query

// Different clients see different schemas
set_client_type(MySQL);
auto tables = execute("SELECT * FROM information_schema.tables");

set_client_type(PostgreSQL);
tables = execute("SELECT * FROM pg_catalog.pg_class");
```

## Exit Criteria
- All objects have immutable UUIDs
- Renames don't break dependencies
- BLR uses UUIDs not names
- Multiple namespace views work
- Remote schemas can be mounted