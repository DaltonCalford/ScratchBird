# Phase 8: System Catalog

## Objective
Implement system catalog for metadata storage.

## Prerequisites
- Phase 7 complete (MVCC)

## Tasks

### 8.1 Catalog Tables
Create system tables:
```sql
SDB$RELATIONS (relation_id, name, type, root_page)
SDB$ATTRIBUTES (relation_id, attr_id, name, type, nullable)
SDB$INDICES (index_id, relation_id, name, type)
SDB$USERS (user_id, name, password_hash)
```

### 8.2 Bootstrap Process
- Create catalog tables on database creation
- Use fixed page numbers for catalog
- Bootstrap without SQL parser

### 8.3 Catalog API
```cpp
class CatalogManager {
    RelationId create_table(string name, vector<Attribute> attrs);
    void drop_table(RelationId id);
    RelationInfo get_relation(string name);
    vector<AttributeInfo> get_attributes(RelationId id);
};
```

### 8.4 Object IDs
- Generate unique IDs for all objects
- Use monotonic counter
- Reserve ranges for system objects

## Files to Create/Modify
- `include/scratchbird/engine/catalog.h`
- `src/engine/catalog_manager.cpp`
- `src/engine/bootstrap.cpp`

## Validation Tests
```cpp
// Create table via catalog
auto rel_id = catalog.create_table("users", {
    {"id", TypeInt, false},
    {"name", TypeText, false},
    {"email", TypeText, true}
});

// Retrieve metadata
auto info = catalog.get_relation("users");
assert(info.name == "users");
assert(info.attributes.size() == 3);

// Drop table
catalog.drop_table(rel_id);
assert(catalog.get_relation("users") == nullptr);
```

## Exit Criteria
- Catalog tables created and accessible
- Metadata persisted across restarts
- Object creation/deletion tracked