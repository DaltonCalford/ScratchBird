# ADR-002: UUID-Based Object Identification

## Status
Accepted

## Context
Database objects (tables, columns, indexes, etc.) need unique identification. Options:
1. Name-based identification (traditional)
2. OID (Object ID) integers 
3. UUID-based identification
4. Hybrid (names + internal IDs)

## Decision
Use UUIDs as the primary identifier for all database objects, with names as mutable labels.

## Rationale

### Problems with Name-Based Systems

1. **Rename Breaks Dependencies**
   ```sql
   CREATE VIEW v AS SELECT * FROM users;
   ALTER TABLE users RENAME TO customers;
   -- View 'v' is now broken
   ```

2. **Federation Conflicts**
   - Two databases might have tables with same names
   - Mounting requires complex renaming

3. **Case Sensitivity Issues**
   - MySQL: case-insensitive
   - PostgreSQL: case-sensitive
   - Leads to compatibility problems

### UUID Solution

```cpp
struct DatabaseObject {
    UUID id;              // Immutable: e.g., "550e8400-e29b-41d4-a716-446655440000"
    string current_name;  // Mutable: e.g., "users" -> "customers"
    ObjectType type;      // Table, View, Index, etc.
    UUID namespace_id;    // Hierarchical organization
};
```

### Benefits

1. **Rename Without Breaking**
   ```sql
   CREATE VIEW v AS SELECT * FROM users;  -- Stores UUID of 'users'
   ALTER TABLE users RENAME TO customers; -- Changes name, not UUID
   SELECT * FROM v;  -- Still works, uses UUID
   ```

2. **Perfect Federation**
   ```sql
   -- Mount two databases with conflicting names
   MOUNT DATABASE mysql_db AT '/mysql/';
   MOUNT DATABASE pg_db AT '/pg/';
   
   -- Both can have 'users' table
   SELECT * FROM `/mysql/users`;  -- UUID: 123e4567-e89b-12d3-a456-426614174000
   SELECT * FROM `/pg/users`;     -- UUID: 987f6543-e21b-45d6-b789-123456789012
   ```

3. **Multi-Protocol Views**
   - MySQL client sees one namespace
   - PostgreSQL client sees another
   - Same underlying objects via UUIDs

### Implementation Design

```cpp
class CatalogManager {
    // Primary lookup by UUID (fast)
    map<UUID, DatabaseObject> objects_by_uuid;
    
    // Secondary lookup by name (for queries)
    map<string, UUID> current_names;
    
    // Namespace hierarchy
    map<UUID, Namespace> namespaces;
    
    UUID resolve_name(const vector<string>& path) {
        // schema.table -> UUID
        // Handles case-sensitivity per client type
    }
};
```

### Storage Format

```sql
-- System catalog stores UUIDs
CREATE TABLE SDB$OBJECTS (
    object_uuid UUID PRIMARY KEY,
    object_name VARCHAR(128),
    object_type INT,
    namespace_uuid UUID,
    created_at TIMESTAMP,
    version INT
);

-- BLR stores UUIDs, not names
CREATE TABLE SDB$PROCEDURES (
    proc_uuid UUID PRIMARY KEY,
    proc_blr BLOB  -- Contains UUIDs of referenced objects
);
```

## Consequences

### Positive
- Renames are trivial O(1) operations
- Federation works seamlessly
- Multi-tenant isolation perfect
- Dependencies never break
- Enables schema versioning

### Negative
- UUIDs less readable than names
- Slightly more storage (16 bytes vs variable)
- Query planning needs UUID resolution
- Debugging requires UUID->name mapping

### Migration Strategy

For existing databases:
1. Generate UUIDs for all objects
2. Create mapping table
3. Update internal references
4. Maintain compatibility layer

## Performance Considerations

```cpp
// UUID lookup: O(1) with hash map
auto object = catalog.get_by_uuid(uuid);  // Fast

// Name lookup: O(log n) with tree, then O(1)
auto uuid = catalog.resolve_name("schema.table");  // Two steps
auto object = catalog.get_by_uuid(uuid);

// Cache hot paths
LRUCache<string, UUID> name_cache(10000);
```

## Example Use Cases

### Schema Evolution
```sql
-- Version 1
CREATE TABLE orders (id INT, user_id INT);

-- Version 2: Rename without breaking views/procedures
ALTER TABLE orders RENAME TO purchase_orders;
-- UUID unchanged, all references still work

-- Version 3: Schema branching
CREATE SCHEMA v2;
CREATE TABLE v2.orders AS SELECT * FROM purchase_orders;
-- Different UUID, can coexist
```

### Multi-Database Mounting
```sql
-- Mount multiple MySQL databases
MOUNT DATABASE prod AT '/prod/';
MOUNT DATABASE staging AT '/staging/';

-- Query across them
SELECT p.*, s.*
FROM `/prod/users` p
JOIN `/staging/users` s ON p.email = s.email
WHERE p.created_at <> s.created_at;
```

## References
- RFC 4122: UUID Standard
- PostgreSQL OID System
- Oracle ROWID Design
- SQL Server Object_ID Implementation