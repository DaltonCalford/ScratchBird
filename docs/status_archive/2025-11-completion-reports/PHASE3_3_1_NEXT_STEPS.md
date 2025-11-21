# Phase 3.3.1 Implementation Notes

**Date**: November 11, 2025
**Status**: Ready for Implementation (requires catalog bootstrap modification)

---

## Summary

Phase 3.2.3 (Permission Cache) is complete. Phase 3.3 (Column-Level Permissions) is next, starting with Phase 3.3.1 (Catalog Schema).

---

## What Phase 3.3.1 Requires

### 1. Add pg_column_permissions Table (Table #39)

The catalog system currently has 38 tables. We need to add table #39: `pg_column_permissions`.

**Challenge**: The catalog bootstrap code is not easily located in the current codebase. The system indicates "100% structures" which suggests all 38 tables are already defined in some initialization code.

**Options**:
1. **Manual SQL Migration** (Recommended for now):
   - Create the table manually using SQL after database initialization
   - This allows testing column permissions without modifying bootstrap
   - Can be formalized into bootstrap later

2. **Modify Bootstrap Code** (Future):
   - Locate the catalog initialization/bootstrap method
   - Add pg_column_permissions table definition
   - Requires fresh database recreation to test

---

## Manual Migration Approach (Recommended)

### Step 1: Create Table via SQL

```sql
-- Run this on an existing database to add column permissions support
CREATE TABLE sys.pg_column_permissions (
    permission_id      UUID PRIMARY KEY,
    table_id           UUID NOT NULL,
    column_name        VARCHAR(128) NOT NULL,
    grantee_id         UUID NOT NULL,
    grantee_type       UINT8 NOT NULL,
    privileges         UINT32 NOT NULL,
    grantor_id         UUID NOT NULL,
    grant_option       BOOLEAN NOT NULL,
    created_at         TIMESTAMP NOT NULL,
    deleted_at         TIMESTAMP,
    deleted_by         UUID
);

-- Create indexes
CREATE INDEX idx_column_perms_table ON sys.pg_column_permissions(table_id);
CREATE INDEX idx_column_perms_grantee ON sys.pg_column_permissions(grantee_id);
CREATE INDEX idx_column_perms_lookup ON sys.pg_column_permissions(table_id, column_name, grantee_id);
```

### Step 2: Implement CRUD Operations

Proceed directly to Phase 3.3.2 and implement the catalog manager methods:
- `grantColumnPermission()`
- `revokeColumnPermission()`
- `hasColumnPermission()`
- `getAccessibleColumns()`

These methods will work with the manually created table.

### Step 3: Later: Formalize in Bootstrap

Once column permissions are working and tested, we can:
1. Locate the catalog bootstrap code
2. Add pg_column_permissions table creation
3. Test with fresh database initialization

---

## Alternative: Use Existing Infrastructure

**Observation**: The current `pg_permissions` table (if it exists) might already support table-level permissions. We could potentially:

1. **Check if pg_permissions exists** and what its schema is
2. **Extend pg_permissions** to include a `column_name` field (nullable)
   - NULL column_name = table-level permission
   - Non-NULL column_name = column-level permission
3. This avoids creating a new catalog table entirely

**Benefits**:
- No bootstrap changes needed
- Single unified permissions table
- Simpler query logic (one table to check)

**Drawbacks**:
- Mixes table and column permissions (could be confusing)
- Indexes less efficient (nullable column in composite index)

---

## Recommended Path Forward

**Immediate Next Session**:

1. **Investigate Existing Permissions Table**:
   ```bash
   # Search for existing permissions infrastructure
   grep -r "pg_permissions\|permissions.*table" src/core/catalog_manager.cpp
   ```

2. **Choose Approach**:
   - **Option A**: Manual SQL migration + new table (clean separation)
   - **Option B**: Extend existing permissions table (simpler bootstrap)
   - **Option C**: Locate and modify bootstrap code (proper solution)

3. **Implement Phase 3.3.2** (CRUD operations) using chosen approach

4. **Test** with integration tests before moving to Phase 3.3.3 (Parser)

---

## Key Files to Investigate

When resuming implementation:

1. **Find Catalog Bootstrap**:
   ```bash
   # Possible locations:
   src/core/catalog_manager.cpp  # bootstrap() method?
   src/core/database.cpp          # initializeCatalog()?
   src/core/schema_manager.cpp    # createSystemTables()?
   ```

2. **Find Existing Permissions**:
   ```bash
   # Look for how grantPermission() stores data:
   grep -A 50 "grantPermission.*Status" src/core/catalog_manager.cpp
   ```

3. **Check Table Definitions**:
   ```bash
   # Find where the 38 catalog tables are defined:
   grep -r "pg_tables\|pg_columns\|pg_indexes" include/scratchbird/core/
   ```

---

## Summary

**Current Status**: Planning complete, ready for implementation
**Blocker**: Need to understand catalog table management
**Workaround**: Manual SQL migration allows immediate progress
**Timeline**: 2-3 hours for Phase 3.3.1 once approach is decided

**Next Action**: Investigate catalog infrastructure and choose implementation approach

---

**Date**: November 11, 2025
**Status**: Phase 3.2.3 Complete ✅ | Phase 3.3.1 Planning Complete 📋
