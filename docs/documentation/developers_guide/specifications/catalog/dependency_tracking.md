# Specification: Dependency Tracking

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:824`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:833`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp`

## Synopsis

This specification defines object dependency tracking, including dependency types, dependency graph management, and CASCADE behavior for DROP operations.

## Scope

### In Scope

- Dependency types (NORMAL, AUTO, INTERNAL, PIN)
- DependencyInfo structure
- Dependency graph construction
- CASCADE vs RESTRICT drop behavior
- Dependency validation

### Out of Scope

- Plan cache invalidation (see `invalidation.md`)
- Trigger execution order

## Specification

### Dependency Types

**Source:** `include/scratchbird/core/catalog_manager.h:824`

```cpp
enum class DependencyType : uint8_t {
    NORMAL = 0,     // User-created dependency (views, procedures, FKs)
    AUTO = 1,       // System-created (auto-generated indexes, sequences)
    INTERNAL = 2,   // System-critical (cannot be dropped)
    PIN = 3         // User-defined INTERNAL (only admin can unpin)
};
```

**Dependency Type Characteristics:**

| Type | Created By | DROP CASCADE | DROP RESTRICT | Example |
|------|------------|--------------|---------------|---------|
| NORMAL | User | Drops dependent | Rejects if dependent | View on table |
| AUTO | System | Drops dependent | Rejects if dependent | Index on FK |
| INTERNAL | System | Rejects | Rejects | System table |
| PIN | Admin | Rejects | Rejects | Pinned extension |

### DependencyInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:833`

```cpp
struct DependencyInfo {
    ID dependency_id;               // UUID of dependency record
    ID dependent_object_id;         // Object that depends ON something
    ObjectType dependent_type;      // Type of dependent object
    ID referenced_object_id;        // Object being depended upon
    ObjectType referenced_type;     // Type of referenced object
    DependencyType dependency_type; // NORMAL, AUTO, INTERNAL, PIN
    uint64_t created_time = 0;
};
```

### Dependency Graph

```
                    ┌─────────────────┐
                    │   employees     │
                    │    (table)      │
                    └────────┬────────┘
                             │
           ┌─────────────────┼─────────────────┐
           │                 │                 │
           ▼                 ▼                 ▼
    ┌─────────────┐   ┌─────────────┐   ┌─────────────┐
    │   emp_view  │   │emp_trigger  │   │ dept_fk     │
    │   (view)    │   │  (trigger)  │   │ (FK const)  │
    └─────────────┘   └─────────────┘   └──────┬──────┘
                                               │
                                               ▼
                                        ┌─────────────┐
                                        │ departments │
                                        │   (table)   │
                                        └─────────────┘

Dependency Records:
1. emp_view → employees (NORMAL)
2. emp_trigger → employees (NORMAL)
3. dept_fk → employees (AUTO - child side)
4. dept_fk → departments (NORMAL - parent side)
```

### sb_dependencies Catalog Table

```cpp
struct DependencyRecord {
    // Primary key
    ID dependency_id;
    
    // Dependent (the object that has the dependency)
    ID dependent_object_id;
    uint8_t dependent_type;         // ObjectType
    uint8_t reserved[7];
    
    // Referenced (the object being depended on)
    ID referenced_object_id;
    uint8_t referenced_type;        // ObjectType
    uint8_t reserved2[7];
    
    // Dependency characteristics
    uint8_t dependency_type;        // DependencyType
    uint8_t reserved3[7];
    
    // Metadata
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Dependency Creation

Dependencies are automatically created when objects reference other objects:

```cpp
// Creating a view automatically creates dependency
Status createView(const ViewInfo& info) {
    // ... create view ...
    
    // Identify base tables from view definition
    for (ID base_table_id : base_table_ids) {
        DependencyInfo dep;
        dep.dependency_id = generateUUID();
        dep.dependent_object_id = view_id;
        dep.dependent_type = ObjectType::VIEW;
        dep.referenced_object_id = base_table_id;
        dep.referenced_type = ObjectType::TABLE;
        dep.dependency_type = DependencyType::NORMAL;
        
        createDependency(dep);
    }
}

// Creating FK creates two dependencies
Status createForeignKey(const ForeignKeyInfo& info) {
    // ... create FK ...
    
    // Dependency on child table (AUTO - system managed)
    DependencyInfo child_dep;
    child_dep.dependent_object_id = fk_id;
    child_dep.dependent_type = ObjectType::CONSTRAINT;
    child_dep.referenced_object_id = child_table_id;
    child_dep.referenced_type = ObjectType::TABLE;
    child_dep.dependency_type = DependencyType::AUTO;
    createDependency(child_dep);
    
    // Dependency on parent table (NORMAL)
    DependencyInfo parent_dep;
    parent_dep.dependent_object_id = fk_id;
    parent_dep.dependent_type = ObjectType::CONSTRAINT;
    parent_dep.referenced_object_id = parent_table_id;
    parent_dep.referenced_type = ObjectType::TABLE;
    parent_dep.dependency_type = DependencyType::NORMAL;
    createDependency(parent_dep);
}
```

### DROP Behavior

**RESTRICT (default):**
```
DROP TABLE employees RESTRICT;

1. Check for dependencies where referenced_object_id = employees
2. If any found:
   - Return ERROR: "cannot drop table employees because other objects depend on it"
   - List dependent objects
3. If none: proceed with drop
```

**CASCADE:**
```
DROP TABLE employees CASCADE;

1. Find all dependencies where referenced_object_id = employees
2. Build dependency tree (transitive closure)
3. Sort by dependency depth (deepest first)
4. For each dependent object:
   a. DROP dependent_object CASCADE
5. Drop the original object
```

## Algorithms

### Algorithm: Check Dependencies

```
Input:  Object ID, drop_type (CASCADE/RESTRICT)
Output: Can drop / List of dependents

1. Query sb_dependencies:
   SELECT * FROM sb_dependencies
   WHERE referenced_object_id = ? AND is_valid = 1

2. If RESTRICT and any dependencies found:
   a. Build list of dependent objects
   b. Return ERROR with list

3. If CASCADE:
   a. Build dependency graph
   b. Perform topological sort (leaf nodes first)
   c. Return ordered list to drop

4. If no dependencies: return SUCCESS
```

### Algorithm: Build Dependency Tree

```
Input:  Root object ID
Output: Dependency tree (object IDs to drop in order)

1. Initialize queue with root object
2. Initialize empty set for all_deps
3. While queue not empty:
   a. obj = queue.pop()
   b. If obj in all_deps: continue
   c. Add obj to all_deps
   d. Find objects that depend on obj:
      SELECT dependent_object_id FROM sb_dependencies
      WHERE referenced_object_id = ?
   e. For each dependent: queue.push(dependent)

4. Return all_deps sorted by dependency depth
```

### Algorithm: Drop With Cascade

```
Input:  Object ID
Output: Success/Failure

1. Build dependency tree (algorithm above)
2. Sort objects: deepest dependencies first
3. For each object in sorted list:
   a. If INTERNAL or PIN dependency:
      - ERROR: "cannot drop system object"
   b. Drop object:
      - Set is_valid = 0
      - Free resources
   c. Remove dependency records
4. Return SUCCESS
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `DEP_INV_001` | No circular dependencies | Cycle detection |
| `DEP_INV_002` | Referenced objects exist | Foreign key check |
| `DEP_INV_003` | INTERNAL deps cannot be removed | Permission check |
| `DEP_INV_004` | Dependent objects valid or being dropped | State check |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `DEPENDENCY_EXISTS` | Object has dependents (RESTRICT) | Use CASCADE |
| `CIRCULAR_DEPENDENCY` | Cycle in dependencies | Fix schema |
| `CANNOT_DROP_INTERNAL` | Attempt to drop INTERNAL | Don't drop |

## Related Specifications

- [invalidation.md](./invalidation.md) - Cache invalidation
- [ddl_operations.md](./ddl_operations.md) - DROP operations

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
