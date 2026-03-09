# Specification: DDL Operations

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Synopsis

This specification defines the internal operations for CREATE, ALTER, and DROP statements, including transaction handling, dependency validation, and catalog updates.

## Scope

### In Scope

- CREATE operations (TABLE, INDEX, VIEW, etc.)
- ALTER operations
- DROP operations with CASCADE/RESTRICT
- Transaction handling for DDL
- DDL locking

### Out of Scope

- DML operations (INSERT, UPDATE, DELETE)
- Query optimization

## Specification

### DDL Transaction Semantics

```cpp
// DDL operations are atomic
// They either complete entirely or roll back entirely

Status executeDDL(DDLOperation op) {
    BEGIN TRANSACTION;
    
    try {
        // Acquire necessary locks
        acquireDDLLocks(op);
        
        // Validate operation
        validateDDL(op);
        
        // Execute operation
        executeDDLInternal(op);
        
        // Update dependencies
        updateDependencies(op);
        
        // Invalidate caches
        invalidateCaches(op);
        
        COMMIT TRANSACTION;
        return Status::OK;
        
    } catch (const Exception& e) {
        ROLLBACK TRANSACTION;
        return Status::ERROR;
    }
}
```

### DDL Lock Hierarchy

```
Lock Order (must acquire in this order):
1. Database lock (if database-level operation)
2. Schema lock
3. Table lock(s)
4. Index lock(s) (if index operation)
```

### CREATE TABLE

```cpp
Status CatalogManager::createTable(
    const ID& schema_id,
    const std::string& table_name,
    const std::vector<ColumnInfo>& columns,
    const TableCreateOptions& options,
    ErrorContext* ctx
) {
    // 1. Validate schema exists
    SchemaInfo schema;
    RETURN_IF_ERROR(getSchema(schema_id, schema, ctx));
    
    // 2. Validate table name unique in schema
    if (tableNameExists(schema_id, table_name)) {
        SET_ERROR_CONTEXT(ctx, Status::ALREADY_EXISTS, 
                         "Table already exists");
        return Status::ALREADY_EXISTS;
    }
    
    // 3. Generate table ID
    ID table_id = generateUUID();
    
    // 4. Allocate root page
    GPID root_gpid;
    RETURN_IF_ERROR(allocateTableRootPage(&root_gpid, ctx));
    
    // 5. Validate columns
    for (const auto& col : columns) {
        RETURN_IF_ERROR(validateColumn(col, ctx));
    }
    
    // 6. Create TOAST table if needed
    ID toast_table_id{};
    bool has_toast = needsToastTable(columns);
    if (has_toast) {
        RETURN_IF_ERROR(createToastTable(table_id, &toast_table_id, ctx));
    }
    
    // 7. Create table record
    TableInfo table_info;
    table_info.table_id = table_id;
    table_info.schema_id = schema_id;
    table_info.table_name = table_name;
    table_info.root_gpid = root_gpid;
    table_info.column_count = columns.size();
    table_info.has_toast = has_toast;
    table_info.toast_table_id = toast_table_id;
    table_info.table_type = options.table_type;
    // ... set other fields
    
    RETURN_IF_ERROR(insertTableRecord(table_info, ctx));
    
    // 8. Create column records
    uint16_t ordinal = 1;
    for (const auto& col : columns) {
        ColumnInfo col_info = col;
        col_info.table_id = table_id;
        col_info.column_id = generateUUID();
        col_info.ordinal = ordinal++;
        
        RETURN_IF_ERROR(insertColumnRecord(col_info, ctx));
    }
    
    // 9. Create implicit indexes (PRIMARY KEY, UNIQUE)
    for (const auto& col : columns) {
        if (col.is_primary_key) {
            RETURN_IF_ERROR(createImplicitPrimaryKeyIndex(table_id, col, ctx));
        }
        if (col.is_unique && !col.is_primary_key) {
            RETURN_IF_ERROR(createImplicitUniqueIndex(table_id, col, ctx));
        }
    }
    
    return Status::OK;
}
```

### ALTER TABLE

```cpp
enum class AlterTableOperation {
    ADD_COLUMN,
    DROP_COLUMN,
    ALTER_COLUMN_TYPE,
    ALTER_COLUMN_DEFAULT,
    ALTER_COLUMN_NULLABLE,
    RENAME_COLUMN,
    RENAME_TABLE,
    ADD_CONSTRAINT,
    DROP_CONSTRAINT
};

Status CatalogManager::alterTable(
    const ID& table_id,
    const AlterTableOperation& operation,
    const AlterTableParams& params,
    ErrorContext* ctx
) {
    // 1. Validate table exists
    TableInfo table;
    RETURN_IF_ERROR(getTable(table_id, table, ctx));
    
    // 2. Acquire exclusive table lock
    LockGuard lock = acquireExclusiveTableLock(table_id);
    
    switch (operation) {
        case AlterTableOperation::ADD_COLUMN:
            RETURN_IF_ERROR(alterTableAddColumn(table, params, ctx));
            break;
            
        case AlterTableOperation::DROP_COLUMN:
            RETURN_IF_ERROR(alterTableDropColumn(table, params, ctx));
            break;
            
        case AlterTableOperation::ALTER_COLUMN_TYPE:
            RETURN_IF_ERROR(alterTableColumnType(table, params, ctx));
            break;
            
        // ... other operations
    }
    
    // 3. Update last_modified_time
    RETURN_IF_ERROR(updateTableModifiedTime(table_id, ctx));
    
    // 4. Invalidate plan cache
    invalidatePlanCache(table_id);
    
    return Status::OK;
}
```

### DROP TABLE

```cpp
Status CatalogManager::dropTable(
    const ID& table_id,
    bool cascade,
    ErrorContext* ctx
) {
    // 1. Validate table exists
    TableInfo table;
    RETURN_IF_ERROR(getTable(table_id, table, ctx));
    
    // 2. Check dependencies
    std::vector<DependencyInfo> dependencies;
    RETURN_IF_ERROR(getDependencies(table_id, dependencies, ctx));
    
    if (!dependencies.empty() && !cascade) {
        SET_ERROR_CONTEXT(ctx, Status::DEPENDENCY_EXISTS,
                         "Table has dependent objects");
        return Status::DEPENDENCY_EXISTS;
    }
    
    // 3. If cascade, drop dependents first
    if (cascade) {
        for (const auto& dep : dependencies) {
            RETURN_IF_ERROR(dropObject(dep.dependent_object_id, 
                                       cascade, ctx));
        }
    }
    
    // 4. Acquire exclusive lock
    LockGuard lock = acquireExclusiveTableLock(table_id);
    
    // 5. Mark table as invalid (soft delete)
    RETURN_IF_ERROR(markTableInvalid(table_id, ctx));
    
    // 6. Drop indexes
    std::vector<IndexInfo> indexes;
    RETURN_IF_ERROR(getTableIndexes(table_id, indexes, ctx));
    for (const auto& idx : indexes) {
        RETURN_IF_ERROR(dropIndex(idx.index_id, ctx));
    }
    
    // 7. Drop constraints
    std::vector<ConstraintInfo> constraints;
    RETURN_IF_ERROR(getTableConstraints(table_id, constraints, ctx));
    for (const auto& con : constraints) {
        RETURN_IF_ERROR(dropConstraint(con.constraint_id, ctx));
    }
    
    // 8. Drop triggers
    std::vector<TriggerInfo> triggers;
    RETURN_IF_ERROR(getTableTriggers(table_id, triggers, ctx));
    for (const auto& trg : triggers) {
        RETURN_IF_ERROR(dropTrigger(trg.trigger_id, ctx));
    }
    
    // 9. Drop columns
    std::vector<ColumnInfo> columns;
    RETURN_IF_ERROR(getTableColumns(table_id, columns, ctx));
    for (const auto& col : columns) {
        RETURN_IF_ERROR(dropColumn(col.column_id, ctx));
    }
    
    // 10. If TOAST table exists, drop it
    if (table.has_toast) {
        RETURN_IF_ERROR(dropToastTable(table.toast_table_id, ctx));
    }
    
    // 11. Schedule data pages for deletion
    schedulePageDeallocation(table.root_gpid);
    
    // 12. Remove dependencies
    RETURN_IF_ERROR(removeDependencies(table_id, ctx));
    
    // 13. Invalidate caches
    invalidatePlanCache(table_id);
    
    return Status::OK;
}
```

### CREATE INDEX

```cpp
Status CatalogManager::createIndex(
    const ID& table_id,
    const std::string& index_name,
    const std::vector<ID>& column_ids,
    const IndexCreateOptions& options,
    ErrorContext* ctx
) {
    // 1. Validate table exists
    TableInfo table;
    RETURN_IF_ERROR(getTable(table_id, table, ctx));
    
    // 2. Validate index name unique
    if (indexNameExists(table_id, index_name)) {
        return Status::ALREADY_EXISTS;
    }
    
    // 3. Validate columns exist
    for (ID col_id : column_ids) {
        ColumnInfo col;
        RETURN_IF_ERROR(getColumn(col_id, col, ctx));
        if (col.table_id != table_id) {
            return Status::INVALID_ARGUMENT;
        }
    }
    
    // 4. Generate index ID
    ID index_id = generateUUID();
    
    // 5. Allocate root page
    GPID root_gpid;
    RETURN_IF_ERROR(allocateIndexRootPage(&root_gpid, ctx));
    
    // 6. Create index record with state = BUILDING
    IndexInfo index_info;
    index_info.index_id = index_id;
    index_info.table_id = table_id;
    index_info.index_name = index_name;
    index_info.root_gpid = root_gpid;
    index_info.column_ids = column_ids;
    index_info.index_type = options.index_type;
    index_info.is_unique = options.is_unique;
    index_info.state = static_cast<uint8_t>(IndexState::BUILDING);
    // ... set other fields
    
    RETURN_IF_ERROR(insertIndexRecord(index_info, ctx));
    
    // 7. Create dependency on table
    RETURN_IF_ERROR(createDependency(index_id, ObjectType::INDEX,
                                     table_id, ObjectType::TABLE,
                                     DependencyType::AUTO, ctx));
    
    // 8. Build index (may be async for large tables)
    RETURN_IF_ERROR(buildIndex(index_id, ctx));
    
    // 9. Set state to ACTIVE
    RETURN_IF_ERROR(setIndexState(index_id, IndexState::ACTIVE, ctx));
    
    return Status::OK;
}
```

## Algorithms

### Algorithm: Validate DDL Operation

```
Input:  DDL operation
Output: Valid/Invalid with error details

1. Check privileges:
   a. Does user have required permission?
   b. Is object in system schema (protected)?

2. Check name validity:
   a. Identifier length <= 128 chars
   b. Valid UTF-8
   c. Not reserved word (unless quoted)

3. Check namespace conflicts:
   a. Name unique in schema
   b. No case-insensitive conflicts (Firebird rules)

4. Check type compatibility:
   a. Data types exist
   b. Collations compatible with charset
   c. Defaults compatible with column type

5. Check constraint validity:
   a. CHECK expressions valid
   b. FK references exist and are compatible
   c. UNIQUE columns exist

6. Return result
```

### Algorithm: Handle DDL Failure

```
Input:  DDL operation, error
Output: Cleanup and return error

1. Log error with context
2. If transaction active:
   a. Rollback transaction
3. Release all locks
4. Clean up partial state:
   a. Free allocated pages
   b. Remove partial catalog entries
5. Return error to caller
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `DDL_INV_001` | DDL is atomic | Transaction wrapper |
| `DDL_INV_002` | Locks acquired before changes | Lock order |
| `DDL_INV_003` | Dependencies updated with changes | Foreign keys |
| `DDL_INV_004` | Cache invalidated after changes | Epoch check |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `ALREADY_EXISTS` | Name conflict | Choose different name |
| `DEPENDENCY_EXISTS` | Object has dependents | Use CASCADE |
| `INSUFFICIENT_PRIVILEGE` | No permission | Grant privileges |
| `INVALID_DEFINITION` | Syntax/semantic error | Fix definition |
| `LOCK_TIMEOUT` | Cannot acquire lock | Retry or abort |

## Related Specifications

- [dependency_tracking.md](./dependency_tracking.md) - Dependencies
- [invalidation.md](./invalidation.md) - Cache invalidation
- [tables.md](./tables.md) - Table operations

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
