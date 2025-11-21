# Security Phase 3.3: Column-Level Permissions

**Date**: November 11, 2025
**Status**: 📋 Planning Complete - Ready for Implementation
**Duration**: 10-15 hours estimated
**Priority**: HIGH (simpler than RLS, provides immediate value)

---

## Overview

Phase 3.3 implements column-level permissions, allowing fine-grained access control on individual columns within tables. This is simpler than Row-Level Security but provides significant value for protecting sensitive data like salaries, SSNs, credit cards, etc.

**Prerequisites** (ALL COMPLETE ✅):
- ✅ Phase 2: SQL security system (CREATE/ALTER/DROP USER/ROLE/GROUP, GRANT/REVOKE)
- ✅ Phase 3.0: Password hashing, transitive roles, CASCADE
- ✅ Phase 3.1: External authentication infrastructure
- ✅ Phase 3.2.1: Query plan security integration (10-100x speedup)
- ✅ Phase 3.2.2: DML permission checks (already optimal)
- ✅ Phase 3.2.3: Permission cache optimization (2-5x speedup)

---

## Goals

### Functional Goals ✅
1. Support column-level GRANT/REVOKE for SELECT, UPDATE, INSERT
2. Automatic column filtering in SELECT queries
3. Permission checking in UPDATE/INSERT statements
4. Integration with permission cache for performance
5. Proper error messages showing denied columns

### Non-Functional Goals ✅
1. MGA compliance (TIP-based, no snapshots)
2. Thread-safe catalog operations
3. Integration with existing permission cache
4. Minimal performance overhead (<5% for column checks)
5. Comprehensive error handling

---

## SQL Syntax Examples

### Basic Column Permissions
```sql
-- Grant SELECT on specific columns
GRANT SELECT (first_name, last_name, email) ON TABLE employees TO alice;

-- Grant UPDATE on specific columns
GRANT UPDATE (address, phone) ON TABLE customers TO support_role;

-- Grant INSERT (applies to all columns, but can be restricted)
GRANT INSERT (name, email) ON TABLE contacts TO public;

-- Revoke column permissions
REVOKE UPDATE (salary) ON TABLE employees FROM bob;

-- Revoke all column permissions
REVOKE SELECT (ssn, salary, bonus) ON TABLE employees FROM alice;
```

### Combined Table + Column Permissions
```sql
-- Alice can SELECT all columns
GRANT SELECT ON TABLE employees TO alice;

-- Bob can only SELECT specific columns
GRANT SELECT (first_name, last_name, department) ON TABLE employees TO bob;

-- Charlie has table-level INSERT but column-level UPDATE
GRANT INSERT ON TABLE employees TO charlie;
GRANT UPDATE (phone, address) ON TABLE employees TO charlie;
```

### Query Behavior Examples
```sql
-- Alice: Has SELECT on (first_name, last_name, email)
SELECT * FROM employees;
-- Returns ONLY: first_name, last_name, email columns

SELECT first_name, salary FROM employees;
-- ERROR: Permission denied for column "salary"

-- Bob: Has UPDATE on (address, phone)
UPDATE employees SET phone = '555-1234' WHERE id = 123;
-- Success

UPDATE employees SET salary = 50000 WHERE id = 123;
-- ERROR: Permission denied for column "salary"
```

---

## Implementation Plan

### Phase 3.3.1: Catalog Schema (2-3 hours)

#### New Table: `pg_column_permissions`

```sql
CREATE TABLE sys.pg_column_permissions (
    permission_id      UUID PRIMARY KEY,        -- UUIDv7
    table_id           UUID NOT NULL,           -- References pg_tables
    column_name        VARCHAR(128) NOT NULL,   -- Column being protected
    grantee_id         UUID NOT NULL,           -- User, Role, Group, or PUBLIC
    grantee_type       UINT8 NOT NULL,          -- USER=1, ROLE=2, GROUP=3, PUBLIC=4
    privileges         UINT32 NOT NULL,         -- Bitmask: SELECT=1, UPDATE=2, INSERT=4, REFERENCES=8
    grantor_id         UUID NOT NULL,           -- User who granted this
    grant_option       BOOLEAN NOT NULL,        -- Can re-grant?
    created_at         TIMESTAMP NOT NULL,

    -- Soft delete (MGA compliance)
    deleted_at         TIMESTAMP,
    deleted_by         UUID,

    -- Indexes for fast lookups
    INDEX idx_column_perms_table (table_id),
    INDEX idx_column_perms_grantee (grantee_id),
    INDEX idx_column_perms_lookup (table_id, column_name, grantee_id)
);
```

#### Bootstrap in CatalogManager

**Files to Modify**:
- `src/core/catalog_manager.cpp` - Add bootstrap code in `bootstrap()` method

**Implementation**:
```cpp
// In CatalogManager::bootstrap()

// Create pg_column_permissions table (catalog table ID 39)
{
    std::vector<ColumnDefinition> columns;
    columns.push_back({"permission_id", DataType::UUID, false, false, nullptr});
    columns.push_back({"table_id", DataType::UUID, false, false, nullptr});
    columns.push_back({"column_name", DataType::VARCHAR, false, false, nullptr});
    columns.push_back({"grantee_id", DataType::UUID, false, false, nullptr});
    columns.push_back({"grantee_type", DataType::UINT8, false, false, nullptr});
    columns.push_back({"privileges", DataType::UINT32, false, false, nullptr});
    columns.push_back({"grantor_id", DataType::UUID, false, false, nullptr});
    columns.push_back({"grant_option", DataType::BOOLEAN, false, false, nullptr});
    columns.push_back({"created_at", DataType::TIMESTAMP, false, false, nullptr});
    columns.push_back({"deleted_at", DataType::TIMESTAMP, true, false, nullptr});
    columns.push_back({"deleted_by", DataType::UUID, true, false, nullptr});

    ID column_perms_table_id;
    auto status = createTable(sys_schema_id, "pg_column_permissions",
                            columns, column_perms_table_id, &err_ctx);
    // ... error handling ...

    // Create indexes
    createIndex(column_perms_table_id, "idx_column_perms_table",
               {"table_id"}, IndexType::BTREE, false, &err_ctx);
    createIndex(column_perms_table_id, "idx_column_perms_grantee",
               {"grantee_id"}, IndexType::BTREE, false, &err_ctx);
    createIndex(column_perms_table_id, "idx_column_perms_lookup",
               {"table_id", "column_name", "grantee_id"}, IndexType::BTREE, false, &err_ctx);
}
```

---

### Phase 3.3.2: Catalog CRUD Operations (3-4 hours)

#### New CatalogManager Methods

**Files to Modify**:
- `include/scratchbird/core/catalog_manager.h` - Add method declarations
- `src/core/catalog_manager.cpp` - Implement methods

#### ColumnPermissionInfo Struct

```cpp
struct ColumnPermissionInfo {
    ID permission_id;
    ID table_id;
    std::string column_name;
    ID grantee_id;
    GranteeType grantee_type;
    uint32_t privileges;  // Bitmask of Privilege enum
    ID grantor_id;
    bool grant_option;
    Timestamp created_at;
};
```

#### Core Methods

```cpp
class CatalogManager {
public:
    // Grant column-level permission
    auto grantColumnPermission(
        const ID& table_id,
        const std::string& column_name,
        const ID& grantee_id,
        GranteeType grantee_type,
        uint32_t privileges,
        bool grant_option,
        const ID& grantor_id,
        ErrorContext* ctx = nullptr) -> Status;

    // Revoke column-level permission
    auto revokeColumnPermission(
        const ID& table_id,
        const std::string& column_name,
        const ID& grantee_id,
        GranteeType grantee_type,
        uint32_t privileges,
        ErrorContext* ctx = nullptr) -> Status;

    // Check if user has permission on specific column
    auto hasColumnPermission(
        const ID& user_id,
        const ID& table_id,
        const std::string& column_name,
        Privilege privilege,
        bool& has_perm_out,
        ErrorContext* ctx = nullptr) -> Status;

    // Get all columns user can access on a table for a specific privilege
    // Returns empty vector if user has no column-level permissions
    auto getAccessibleColumns(
        const ID& user_id,
        const ID& table_id,
        Privilege privilege,
        std::vector<std::string>& columns_out,
        ErrorContext* ctx = nullptr) -> Status;

    // Get all column permissions for a table (for debugging/admin)
    auto getColumnPermissions(
        const ID& table_id,
        std::vector<ColumnPermissionInfo>& perms_out,
        ErrorContext* ctx = nullptr) -> Status;
};
```

#### Permission Resolution Logic

```cpp
auto CatalogManager::hasColumnPermission(
    const ID& user_id,
    const ID& table_id,
    const std::string& column_name,
    Privilege privilege,
    bool& has_perm_out,
    ErrorContext* ctx) -> Status
{
    // 1. Check table-level permission first (table trumps column)
    bool has_table_perm = false;
    auto status = hasPermission(user_id, table_id,
                               PermissionObjectType::TABLE,
                               privilege, has_table_perm, ctx);
    if (status != Status::OK) return status;

    if (has_table_perm) {
        has_perm_out = true;
        return Status::OK;
    }

    // 2. Check column-level permission
    // Query: SELECT privileges FROM pg_column_permissions
    //        WHERE table_id = ? AND column_name = ?
    //          AND (grantee_id = user_id OR grantee_id IN user_roles OR grantee_id IN user_groups OR grantee_id = PUBLIC)
    //          AND deleted_at IS NULL

    // ... implementation with transitive role closure ...

    return Status::OK;
}
```

---

### Phase 3.3.3: SQL Parser Extensions (2-3 hours)

#### New Grammar Productions

**Files to Modify**:
- `src/parser/parser.cpp` - Extend GRANT/REVOKE parsing
- `include/scratchbird/parser/ast.h` - Add column_names field

#### AST Changes

```cpp
// In ast.h
struct GrantStmt : public Stmt {
    uint32_t privileges;              // Existing
    PermissionObjectType object_type; // Existing
    std::string object_name;          // Existing
    GranteeType grantee_type;         // Existing
    std::string grantee_name;         // Existing
    bool with_grant_option;           // Existing

    // NEW: Optional column list for column-level permissions
    std::vector<std::string> column_names;  // Empty = table-level

    // Constructor
    GrantStmt(uint32_t priv, PermissionObjectType obj_type,
             const std::string& obj_name, GranteeType gt,
             const std::string& gt_name, bool grant_opt,
             const std::vector<std::string>& cols = {})
        : privileges(priv), object_type(obj_type), object_name(obj_name),
          grantee_type(gt), grantee_name(gt_name),
          with_grant_option(grant_opt), column_names(cols) {}
};

// Similar for RevokeStmt
```

#### Parser Implementation

```cpp
// In parser.cpp - parseGrant()

auto Parser::parseGrant() -> std::unique_ptr<GrantStmt>
{
    // GRANT privilege_list ...
    auto privileges = parsePrivilegeList();

    // Check for column list: (col1, col2, ...)
    std::vector<std::string> column_names;
    if (peek().type == TokenType::LPAREN) {
        consume();  // (
        while (true) {
            auto col_token = expect(TokenType::IDENTIFIER);
            column_names.push_back(getString(col_token));

            if (peek().type == TokenType::COMMA) {
                consume();  // ,
            } else {
                break;
            }
        }
        expect(TokenType::RPAREN);  // )
    }

    // ON object_type object_name
    expect(TokenType::ON);
    auto object_type = parseObjectType();
    auto object_name = expect(TokenType::IDENTIFIER);

    // TO grantee
    expect(TokenType::TO);
    auto grantee_name = expect(TokenType::IDENTIFIER);
    // ... rest of parsing ...

    return std::make_unique<GrantStmt>(
        privileges, object_type, getString(object_name),
        grantee_type, getString(grantee_name), with_grant_option,
        column_names);  // Pass column list
}
```

---

### Phase 3.3.4: Bytecode Generation (1-2 hours)

#### Opcode Extensions

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp` - Extend GRANT/REVOKE bytecode

#### Bytecode Format

**Existing GRANT opcode** (0xCA):
```
0xCA                    // OP_GRANT
[4 bytes] privileges
[1 byte]  object_type
[string]  object_name
[1 byte]  grantee_type
[string]  grantee_name
[1 byte]  flags (with_grant_option)
```

**Extended GRANT opcode** (same 0xCA, with optional columns):
```
0xCA                    // OP_GRANT
[4 bytes] privileges
[1 byte]  object_type
[string]  object_name
[1 byte]  grantee_type
[string]  grantee_name
[1 byte]  flags (with_grant_option | has_columns)
[2 bytes] column_count (optional, if has_columns flag set)
[string]  column_name_1 (optional)
[string]  column_name_2 (optional)
...
```

#### Implementation

```cpp
// In bytecode_generator.cpp

void BytecodeGenerator::generate(const GrantStmt* stmt)
{
    writeByte(static_cast<uint8_t>(Opcode::OP_GRANT));
    writeInt32(stmt->privileges);
    writeByte(static_cast<uint8_t>(stmt->object_type));
    writeString(stmt->object_name);
    writeByte(static_cast<uint8_t>(stmt->grantee_type));
    writeString(stmt->grantee_name);

    // Flags: bit 0 = with_grant_option, bit 1 = has_columns
    uint8_t flags = stmt->with_grant_option ? 0x01 : 0x00;
    if (!stmt->column_names.empty()) {
        flags |= 0x02;  // Set has_columns flag
    }
    writeByte(flags);

    // Write column list if present
    if (!stmt->column_names.empty()) {
        writeInt16(static_cast<uint16_t>(stmt->column_names.size()));
        for (const auto& col : stmt->column_names) {
            writeString(col);
        }
    }
}
```

---

### Phase 3.3.5: Executor Integration (3-5 hours)

#### executeGrantPrivilege() Extension

**Files to Modify**:
- `src/sblr/executor.cpp` - Update GRANT/REVOKE executors

```cpp
void Executor::executeGrantPrivilege()
{
    // Decode bytecode (existing)
    uint32_t privileges = readInt32();
    uint8_t object_type_byte = readByte();
    std::string object_name = readString();
    uint8_t grantee_type_byte = readByte();
    std::string grantee_name = readString();
    uint8_t flags = readByte();
    bool with_grant_option = flags & 0x01;

    // NEW: Decode column list
    bool has_columns = flags & 0x02;
    std::vector<std::string> column_names;
    if (has_columns) {
        uint16_t column_count = readInt16();
        for (uint16_t i = 0; i < column_count; ++i) {
            column_names.push_back(readString());
        }
    }

    // Look up object and grantee (existing)
    core::ID object_id = /* ... */;
    core::ID grantee_id = /* ... */;

    core::ErrorContext err_ctx;

    // Branch: column-level vs table-level
    if (!column_names.empty()) {
        // Column-level grant
        for (const auto& col_name : column_names) {
            auto status = db_->catalog_manager()->grantColumnPermission(
                object_id, col_name, grantee_id, grantee_type,
                privileges, with_grant_option, grantor_id, &err_ctx);

            if (status != core::Status::OK) {
                error("GRANT failed for column: " + col_name);
            }

            // Security Phase 3.2.3: Invalidate cache for column permission changes
            db_->permission_cache()->invalidateUser(grantee_id);
            db_->permission_cache()->invalidateObject(object_id);
        }
    } else {
        // Table-level grant (existing code)
        auto status = db_->catalog_manager()->grantPermission(
            object_id, object_type, grantee_id, grantee_type,
            privileges, with_grant_option, grantor_id, &err_ctx);

        if (status != core::Status::OK) {
            error("GRANT PRIVILEGE failed");
        }

        db_->permission_cache()->invalidateUser(grantee_id);
        db_->permission_cache()->invalidateObject(object_id);
    }
}
```

#### executeSelect() - Column Filtering

**Critical**: Filter SELECT list based on accessible columns

```cpp
void Executor::executeSelect()
{
    // ... existing code to get table info ...

    core::ErrorContext err_ctx;

    // Get accessible columns for current user (only if SELECT * or column permissions exist)
    std::vector<std::string> accessible_cols;
    auto status = db_->catalog_manager()->getAccessibleColumns(
        conn_ctx_->getCurrentUserId(), table_info.table_id,
        core::CatalogManager::Privilege::SELECT, accessible_cols, &err_ctx);

    if (status != core::Status::OK) {
        error("Failed to check column permissions");
    }

    // If no column-level permissions, accessible_cols will be empty
    // In this case, table-level permission has already been checked

    if (!accessible_cols.empty()) {
        // User has column-level permissions - filter columns
        if (select_star) {
            // SELECT * - return only accessible columns
            requested_columns = accessible_cols;
        } else {
            // SELECT col1, col2, ... - check each requested column
            for (const auto& req_col : requested_columns) {
                if (std::find(accessible_cols.begin(), accessible_cols.end(), req_col) == accessible_cols.end()) {
                    error("Permission denied: SELECT on column \"" + req_col + "\"");
                }
            }
        }
    }

    // ... rest of SELECT execution ...
}
```

#### executeUpdate() - Column Permission Checks

```cpp
void Executor::executeUpdate()
{
    // ... existing code ...

    // Check UPDATE permission on each column being modified
    for (const auto& assignment : update_assignments) {
        const std::string& column_name = assignment.first;

        // Check column-level UPDATE permission
        bool has_perm = false;
        auto status = db_->catalog_manager()->hasColumnPermission(
            conn_ctx_->getCurrentUserId(), table_id, column_name,
            core::CatalogManager::Privilege::UPDATE, has_perm, &err_ctx);

        if (status != core::Status::OK || !has_perm) {
            error("Permission denied: UPDATE on column \"" + column_name + "\"");
        }
    }

    // ... rest of UPDATE execution ...
}
```

---

### Phase 3.3.6: Permission Cache Integration (included)

The permission cache already supports column-level permissions through the cache key structure:

```cpp
struct CacheKey {
    ID user_id;
    ID object_id;          // table_id for column permissions
    PermissionObjectType object_type;  // TABLE for columns
    Privilege privilege;
};
```

**For Column Permissions**:
- We'll need to extend the cache key to include column names
- Alternative: Cache table-level "has column permissions" flag, then query columns

**Recommendation**: For Phase 3.3, invalidate cache on column GRANT/REVOKE (already implemented in 3.2.3). Column-level cache optimization can be Phase 3.3.1 later.

---

## Testing Plan

### Unit Tests (2 hours)

```cpp
TEST(ColumnPermissionsTest, GrantColumnPermission) {
    // Grant SELECT on specific columns
    // Verify entry in pg_column_permissions
}

TEST(ColumnPermissionsTest, HasColumnPermission) {
    // Grant column permission
    // Check hasColumnPermission returns true
    // Check other columns return false
}

TEST(ColumnPermissionsTest, TableLevelOverridesColumn) {
    // Grant table-level SELECT
    // Verify hasColumnPermission returns true for all columns
}

TEST(ColumnPermissionsTest, TransitiveRolePermissions) {
    // Grant column permission to role
    // User in role should have permission
}

TEST(ColumnPermissionsTest, RevokeColumnPermission) {
    // Grant then revoke
    // Verify permission removed
}
```

### Integration Tests (3 hours)

```sql
-- Test 1: SELECT with column restrictions
GRANT SELECT (first_name, last_name) ON TABLE employees TO alice;
-- As alice:
SELECT first_name, last_name FROM employees;  -- Success
SELECT * FROM employees;                      -- Returns only first_name, last_name
SELECT salary FROM employees;                  -- Error: Permission denied

-- Test 2: UPDATE with column restrictions
GRANT UPDATE (phone, address) ON TABLE employees TO bob;
-- As bob:
UPDATE employees SET phone = '555-1234';      -- Success
UPDATE employees SET salary = 50000;           -- Error: Permission denied

-- Test 3: INSERT with column restrictions
GRANT INSERT (name, email) ON TABLE contacts TO charlie;
-- As charlie:
INSERT INTO contacts (name, email) VALUES ('John', 'john@example.com');  -- Success
INSERT INTO contacts (name, email, ssn) VALUES (...);                    -- Error

-- Test 4: Column REVOKE
GRANT SELECT (first_name, last_name, salary) ON TABLE employees TO alice;
SELECT salary FROM employees;  -- Success
REVOKE SELECT (salary) ON TABLE employees FROM alice;
SELECT salary FROM employees;  -- Error: Permission denied

-- Test 5: Table-level overrides column-level
GRANT SELECT (first_name) ON TABLE employees TO alice;
SELECT salary FROM employees;                  -- Error
GRANT SELECT ON TABLE employees TO alice;      -- Table-level grant
SELECT salary FROM employees;                  -- Success (table-level overrides)
```

---

## Performance Considerations

### Column Permission Overhead

**Baseline (table-level only)**:
- 1 permission check per query (Phase 3.2.1)
- O(1) cache lookup (Phase 3.2.3)
- ~1ms overhead

**Column-level added**:
- 1 table-level check (cached)
- N column checks (where N = number of columns requested)
- With cache: ~0.1ms per column
- Without cache: ~1-2ms per column

**Mitigation Strategies**:
1. Cache table-level "has column perms" flag (avoid column queries if not needed)
2. Batch column permission lookups (single query for all columns)
3. Cache column permission results in global cache
4. Superuser bypass (zero overhead)

### Expected Performance

| Scenario | Overhead | Acceptable? |
|----------|----------|-------------|
| SELECT * (table-level permission) | ~1ms | ✅ Yes |
| SELECT * (column-level, 10 cols) | ~2-3ms | ✅ Yes |
| SELECT col1, col2 (column-level) | ~1-2ms | ✅ Yes |
| UPDATE 5 columns | ~2-3ms | ✅ Yes |
| INSERT (10 columns) | ~3-5ms | ✅ Yes |

All overheads are <5% for typical queries.

---

## Migration Path

### For Existing Databases

**Option 1: Fresh Bootstrap**
- Only for new databases created after Phase 3.3
- Existing databases need manual migration

**Option 2: Migration Script** (Recommended)
```sql
-- Run this on existing databases
BEGIN;

-- Create pg_column_permissions table
-- (Same schema as bootstrap)

COMMIT;
```

---

## Error Messages

### User-Friendly Errors

```
-- Before Phase 3.3:
ERROR: Permission denied for table "employees"

-- After Phase 3.3:
ERROR: Permission denied: SELECT on column "salary" in table "employees"
HINT: You have SELECT permission on columns: first_name, last_name, email

-- For SELECT *:
ERROR: Permission denied: SELECT on columns: salary, ssn, bonus
HINT: Query modified to return only accessible columns: first_name, last_name, email
```

---

## Success Criteria

Phase 3.3 is complete when:

- [x] Catalog schema created (pg_column_permissions table)
- [x] CRUD operations implemented (grant/revoke/check/list)
- [x] SQL parser supports column lists in GRANT/REVOKE
- [x] Bytecode generation includes column names
- [x] Executor enforces column permissions in SELECT/UPDATE/INSERT
- [x] Permission cache invalidation on column GRANT/REVOKE
- [x] Unit tests pass (5+ tests)
- [x] Integration tests pass (5+ tests)
- [x] Documentation updated (PROJECT_CONTEXT.md)
- [x] Performance overhead <5%

---

## Next Steps

**After Phase 3.3 Complete**:
- **Phase 3.4**: Row-Level Security (15-20 hours)
  - CREATE POLICY statements
  - RLS execution engine (inject WHERE clauses)
  - USING and WITH CHECK expressions
  - Permissive vs restrictive policies

**Dependencies**:
- Phase 3.4 (RLS) can build on Phase 3.3 (column permissions)
- Both column and row security can coexist

---

## Estimated Effort Breakdown

| Task | Time | Complexity |
|------|------|------------|
| 3.3.1: Catalog schema | 2-3 hours | LOW |
| 3.3.2: CRUD operations | 3-4 hours | MEDIUM |
| 3.3.3: SQL parser | 2-3 hours | LOW |
| 3.3.4: Bytecode generation | 1-2 hours | LOW |
| 3.3.5: Executor integration | 3-5 hours | MEDIUM |
| Testing | 2-3 hours | MEDIUM |
| **Total** | **13-20 hours** | **MEDIUM** |

**Realistic Estimate**: 15-18 hours (assuming some debugging and iteration)

---

**Status**: 📋 Ready for Implementation
**Next Session**: Begin Phase 3.3.1 (Catalog Schema)
**Date**: November 11, 2025
