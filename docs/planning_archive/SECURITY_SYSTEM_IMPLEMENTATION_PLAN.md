# Security System Implementation Plan

**Version**: 1.0
**Date**: November 9, 2025
**Status**: Ready for Implementation
**Specification**: `/docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md`
**Estimated Total Effort**: 600-900 hours (15-23 weeks)

---

## Table of Contents

1. [Overview](#overview)
2. [Phase 1: Core Infrastructure (ALPHA)](#phase-1-core-infrastructure-alpha)
3. [Phase 2: SHOW Commands & Enforcement (ALPHA)](#phase-2-show-commands--enforcement-alpha)
4. [Phase 3: Advanced Features (BETA)](#phase-3-advanced-features-beta)
5. [Phase 4: Optional Features (Future)](#phase-4-optional-features-future)
6. [Testing Plan](#testing-plan)
7. [Migration Strategy](#migration-strategy)
8. [Dependencies](#dependencies)
9. [Risk Assessment](#risk-assessment)

---

## Overview

### Scope

Implement a three-layer security model for ScratchBird:
1. **Role-Based Access Control (RBAC)** - Exclusive, context-switching privileges
2. **Group-Based Access Control (GBAC)** - Cumulative, always-on privileges
3. **Access Control Lists (ACLs)** - Object-level permissions

### Goals

- **ALPHA Release**: Core security with table-level privileges, GRANT/REVOKE, SHOW commands
- **BETA Release**: External authentication (LDAP/AD), group nesting, column-level privileges
- **Future**: Row-level security, compatibility views

### Success Criteria

- All DML/DDL operations enforce privilege checks
- Users cannot query system catalog directly (except superusers)
- SHOW commands work with permission filtering
- GRANT/REVOKE SQL syntax matches PostgreSQL
- External authentication integrates with LDAP/AD
- Performance impact < 5% with privilege caching

---

## Phase 1: Core Infrastructure (ALPHA)

**Estimated Effort**: 200-300 hours (5-8 weeks)
**Priority**: HIGH
**Dependencies**: Catalog system complete (✅ Done)

### 1.1: Catalog Structure Updates (20-30 hours)

#### Tasks

1. **Update PermissionRecord** (4-6 hours)
   ```cpp
   // File: src/core/catalog_manager.cpp
   // Update PermissionRecord to use grantee_id instead of grantee name
   struct PermissionRecord {
       ID permission_id;
       ID object_id;
       uint8_t object_type;
       ID grantee_id;              // NEW
       uint8_t grantee_type;       // NEW: USER/ROLE/GROUP/PUBLIC
       uint32_t privileges;
       uint8_t grant_option;
       ID grantor_id;              // NEW
       uint64_t created_time;
       uint32_t is_valid;
   };
   ```

2. **Create GroupMembershipRecord** (4-6 hours)
   ```cpp
   // File: src/core/catalog_manager.cpp (line ~602)
   struct GroupMembershipRecord {
       ID membership_id;
       ID user_id;                 // Or group_id for nesting
       uint8_t member_type;        // USER=0, GROUP=1
       ID group_id;
       ID granted_by;
       uint64_t granted_time;
       uint32_t is_valid;
   };
   ```

3. **Create GroupMappingRecord** (4-6 hours)
   ```cpp
   // File: src/core/catalog_manager.cpp (line ~615)
   struct GroupMappingRecord {
       ID mapping_id;
       char external_group_name[512];
       uint8_t auth_method;        // LDAP/KERBEROS/AD
       ID internal_group_id;
       uint8_t auto_create_users;
       uint64_t created_time;
       uint32_t is_valid;
   };
   ```

4. **Update CatalogRootPage** (4-6 hours)
   - Add table page IDs for: group_memberships, group_mappings
   - Update `writeCatalogRoot()` and `readCatalogRoot()`

5. **Update initialize()** (4-6 hours)
   - Allocate pages for new tables
   - Bootstrap security (SYSTEM user, PUBLIC role, DB_OWNER role)

**Files Modified**:
- `src/core/catalog_manager.cpp` (structures, bootstrap)
- `include/scratchbird/core/catalog_manager.h` (member variables)

**Testing**:
- Fresh database creation includes all security tables
- Bootstrap creates SYSTEM user, PUBLIC role, DB_OWNER role

---

### 1.2: Users CRUD (15-25 hours)

#### Functions to Implement

```cpp
// File: src/core/catalog_manager.cpp

Status createUser(
    const string& username,
    const string& password,
    bool is_superuser,
    const ID& default_schema_id,
    ID& user_id,
    ErrorContext* ctx
);

Status dropUser(
    const ID& user_id,
    bool cascade,
    ErrorContext* ctx
);

Status alterUser(
    const ID& user_id,
    const optional<string>& new_password,
    const optional<ID>& new_default_schema,
    ErrorContext* ctx
);

Status getUserByName(
    const string& username,
    UserInfo& user,
    ErrorContext* ctx
);

Status getUser(
    const ID& user_id,
    UserInfo& user,
    ErrorContext* ctx
);

Status listUsers(
    vector<UserInfo>& users,
    ErrorContext* ctx
);

Status updateUserLastLogin(
    const ID& user_id,
    uint64_t login_time,
    ErrorContext* ctx
);
```

#### Implementation Details

1. **Password Hashing** (use existing libraries)
   - bcrypt or argon2
   - Store hash in TOAST (password_hash_oid)

2. **Validation**
   - Username uniqueness
   - Password policy (min length, complexity)
   - Cascade: check role memberships, object ownership

**Files Modified**:
- `src/core/catalog_manager.cpp` (~200 lines)
- `include/scratchbird/core/catalog_manager.h` (declarations)

**Testing**:
- Create user with valid/invalid passwords
- Drop user with CASCADE (removes role memberships)
- Duplicate username rejection
- Update last login time

---

### 1.3: Roles CRUD (15-25 hours)

#### Functions to Implement

```cpp
Status createRole(
    const string& role_name,
    ID& role_id,
    ErrorContext* ctx
);

Status dropRole(
    const ID& role_id,
    bool cascade,
    ErrorContext* ctx
);

Status getRoleByName(
    const string& role_name,
    ID& role_id,
    ErrorContext* ctx
);

Status getRole(
    const ID& role_id,
    RoleInfo& role,
    ErrorContext* ctx
);

Status listRoles(
    vector<RoleInfo>& roles,
    ErrorContext* ctx
);
```

**Files Modified**:
- `src/core/catalog_manager.cpp` (~150 lines)
- `include/scratchbird/core/catalog_manager.h` (declarations)

**Testing**:
- Create/drop roles
- Duplicate role name rejection
- CASCADE drops role memberships and permissions

---

### 1.4: Groups CRUD (15-25 hours)

#### Functions to Implement

```cpp
Status createGroup(
    const string& group_name,
    GroupType type,
    const string& external_id,
    ID& group_id,
    ErrorContext* ctx
);

Status dropGroup(
    const ID& group_id,
    bool cascade,
    ErrorContext* ctx
);

Status getGroupByName(
    const string& group_name,
    ID& group_id,
    ErrorContext* ctx
);

Status getGroup(
    const ID& group_id,
    GroupInfo& group,
    ErrorContext* ctx
);

Status listGroups(
    vector<GroupInfo>& groups,
    ErrorContext* ctx
);
```

**Files Modified**:
- `src/core/catalog_manager.cpp` (~150 lines)
- `include/scratchbird/core/catalog_manager.h` (declarations)

**Testing**:
- Create local and external groups
- Drop groups with CASCADE
- External ID mapping

---

### 1.5: Role & Group Memberships (20-30 hours)

#### Functions to Implement

```cpp
// Role Memberships
Status grantRoleToUser(
    const ID& role_id,
    const ID& user_id,
    bool with_admin_option,
    const ID& grantor_id,
    ErrorContext* ctx
);

Status revokeRoleFromUser(
    const ID& role_id,
    const ID& user_id,
    bool cascade,
    ErrorContext* ctx
);

Status getRoleMemberships(
    const ID& user_id,
    vector<ID>& role_ids,
    ErrorContext* ctx
);

// Group Memberships
Status addUserToGroup(
    const ID& user_id,
    const ID& group_id,
    const ID& grantor_id,
    ErrorContext* ctx
);

Status removeUserFromGroup(
    const ID& user_id,
    const ID& group_id,
    ErrorContext* ctx
);

Status getDirectGroupsForUser(
    const ID& user_id,
    vector<ID>& group_ids,
    ErrorContext* ctx
);

Status resolveGroupMemberships(
    const ID& user_id,
    vector<ID>& all_group_ids,
    ErrorContext* ctx
);

Status getUsersInGroup(
    const ID& group_id,
    bool recursive,
    vector<ID>& user_ids,
    ErrorContext* ctx
);
```

#### Implementation Notes

- `resolveGroupMemberships()` does BFS for transitive groups
- Cycle detection for group nesting
- Cache invalidation when memberships change

**Files Modified**:
- `src/core/catalog_manager.cpp` (~250 lines)
- `include/scratchbird/core/catalog_manager.h` (declarations)

**Testing**:
- Add/remove users from roles
- Add/remove users from groups
- Transitive group membership resolution
- WITH ADMIN OPTION enforcement

---

### 1.6: Permissions CRUD (25-35 hours)

#### Functions to Implement

```cpp
Status grantPrivilege(
    const Session* session,
    uint32_t privileges,
    const ID& object_id,
    ObjectType object_type,
    const ID& grantee_id,
    GranteeType grantee_type,
    bool with_grant_option,
    ErrorContext* ctx
);

Status revokePrivilege(
    const Session* session,
    uint32_t privileges,
    const ID& object_id,
    ObjectType object_type,
    const ID& grantee_id,
    GranteeType grantee_type,
    bool cascade,
    ErrorContext* ctx
);

Status getPrivilegesForUser(
    const ID& user_id,
    const ID& object_id,
    uint32_t& privileges,
    ErrorContext* ctx
);

Status getPrivilegesForRole(
    const ID& role_id,
    const ID& object_id,
    uint32_t& privileges,
    ErrorContext* ctx
);

Status getPrivilegesForGroup(
    const ID& group_id,
    const ID& object_id,
    uint32_t& privileges,
    ErrorContext* ctx
);

Status getEffectivePrivileges(
    const Session* session,
    const ID& object_id,
    ObjectType object_type,
    uint32_t& privileges,
    ErrorContext* ctx
);

Status listPermissionsForObject(
    const ID& object_id,
    vector<PermissionInfo>& permissions,
    ErrorContext* ctx
);
```

**Files Modified**:
- `src/core/catalog_manager.cpp` (~300 lines)
- `include/scratchbird/core/catalog_manager.h` (declarations)

**Testing**:
- Grant/revoke to user, role, group, PUBLIC
- WITH GRANT OPTION
- CASCADE revoke
- Effective privilege calculation

---

### 1.7: Session Management (30-40 hours)

#### Session Structure

```cpp
// File: include/scratchbird/core/session.h (NEW FILE)

struct Session {
    // Identity
    ID user_id;
    string username;

    // Privileges
    ID active_role_id;
    vector<ID> available_roles;
    vector<ID> group_ids;

    // Context
    ID current_schema_id;
    ID default_schema_id;

    // Flags
    bool is_superuser;

    // External auth
    string auth_method;
    uint64_t external_auth_time;

    // Caching
    unordered_map<ID, uint32_t> privilege_cache;
    mutex cache_mutex;

    // Session tracking
    uint64_t session_start_time;
    uint64_t last_activity_time;
    uint32_t transaction_count;
};
```

#### Functions to Implement

```cpp
// File: src/core/database.cpp

Status Database::authenticate(
    const string& username,
    const string& password,
    Session** session_out,
    ErrorContext* ctx
);

Status Database::createSession(
    const ID& user_id,
    const string& username,
    const string& auth_method,
    Session** session_out,
    ErrorContext* ctx
);

Status Database::closeSession(
    Session* session,
    ErrorContext* ctx
);
```

#### Privilege Check Entry Point

```cpp
// File: src/core/catalog_manager.cpp

Status CatalogManager::checkPrivilege(
    const Session* session,
    const ID& object_id,
    ObjectType object_type,
    uint32_t required_privileges,
    ErrorContext* ctx
);

bool CatalogManager::hasAnyPrivilege(
    const Session* session,
    const ID& object_id,
    ObjectType object_type
);

void CatalogManager::invalidatePrivilegeCachesForGrantee(
    const ID& grantee_id,
    GranteeType grantee_type
);
```

**Files Created**:
- `include/scratchbird/core/session.h`

**Files Modified**:
- `src/core/database.cpp` (~200 lines)
- `src/core/catalog_manager.cpp` (~150 lines)
- `include/scratchbird/core/catalog_manager.h` (declarations)

**Testing**:
- Successful authentication
- Failed authentication (wrong password, inactive user)
- Session creation with role/group memberships
- Privilege caching and invalidation

---

### 1.8: GRANT/REVOKE Parsing (20-30 hours)

#### AST Nodes

```cpp
// File: include/scratchbird/parser/ast.h

struct GrantStmt : public Statement {
    enum GrantType { PRIVILEGE, ROLE };
    GrantType grant_type;

    vector<Privilege> privileges;
    ID role_id;
    ObjectType object_type;
    ID object_id;

    enum GranteeType { USER, ROLE, GROUP, PUBLIC };
    GranteeType grantee_type;
    ID grantee_id;

    bool with_grant_option;
    bool with_admin_option;

    GrantStmt() : Statement(StatementType::GRANT) {}
};

struct RevokeStmt : public Statement {
    enum RevokeType { PRIVILEGE, ROLE };
    RevokeType revoke_type;

    vector<Privilege> privileges;
    ID role_id;
    ObjectType object_type;
    ID object_id;

    enum GranteeType { USER, ROLE, GROUP, PUBLIC };
    GranteeType grantee_type;
    ID grantee_id;

    bool cascade;

    RevokeStmt() : Statement(StatementType::REVOKE) {}
};

struct SetRoleStmt : public Statement {
    string role_name;
    bool reset;  // RESET ROLE

    SetRoleStmt() : Statement(StatementType::SET_ROLE) {}
};
```

#### Parser Implementation

```cpp
// File: src/parser/parser.cpp

unique_ptr<Statement> Parser::parseGrant();
unique_ptr<Statement> Parser::parseRevoke();
unique_ptr<Statement> Parser::parseSetRole();
```

**Files Modified**:
- `include/scratchbird/parser/ast.h` (~100 lines)
- `src/parser/parser.cpp` (~200 lines)
- `include/scratchbird/parser/token.h` (new keywords: GRANT, REVOKE, PRIVILEGES, WITH, OPTION, ROLE, GROUP)

**Testing**:
- Parse GRANT SELECT, INSERT ON TABLE t TO user
- Parse GRANT ROLE r TO user
- Parse REVOKE ... CASCADE
- Parse SET ROLE / RESET ROLE

---

### 1.9: GRANT/REVOKE Bytecode (15-25 hours)

#### Opcodes

```cpp
// File: include/scratchbird/sblr/opcodes.h

enum Opcode : uint8_t {
    // ... existing opcodes
    OP_GRANT_PRIVILEGE,
    OP_REVOKE_PRIVILEGE,
    OP_GRANT_ROLE,
    OP_REVOKE_ROLE,
    OP_SET_ROLE,
    OP_RESET_ROLE,
};
```

#### Bytecode Generator

```cpp
// File: src/sblr/bytecode_generator.cpp

Status BytecodeGenerator::generateGrant(GrantStmt* stmt);
Status BytecodeGenerator::generateRevoke(RevokeStmt* stmt);
Status BytecodeGenerator::generateSetRole(SetRoleStmt* stmt);
```

**Files Modified**:
- `include/scratchbird/sblr/opcodes.h`
- `src/sblr/bytecode_generator.cpp` (~150 lines)

**Testing**:
- Generate bytecode for all GRANT/REVOKE variants
- Verify opcode parameters

---

### 1.10: GRANT/REVOKE Executor (20-30 hours)

#### Executor Implementation

```cpp
// File: src/sblr/executor.cpp

Status Executor::executeGrantPrivilege(GrantStmt* stmt, Session* session);
Status Executor::executeRevokePrivilege(RevokeStmt* stmt, Session* session);
Status Executor::executeGrantRole(GrantStmt* stmt, Session* session);
Status Executor::executeRevokeRole(RevokeStmt* stmt, Session* session);
Status Executor::executeSetRole(SetRoleStmt* stmt, Session* session);
Status Executor::executeResetRole(Session* session);
```

**Files Modified**:
- `src/sblr/executor.cpp` (~250 lines)
- `include/scratchbird/sblr/executor.h` (declarations)

**Testing**:
- Execute GRANT/REVOKE with various combinations
- Verify privilege cache invalidation
- Test CASCADE behavior
- Test SET ROLE / RESET ROLE

---

### 1.11: DML Enforcement (25-35 hours)

#### Privilege Checks in Executor

```cpp
// File: src/sblr/executor.cpp

Status Executor::executeSelect(SelectStmt* stmt, Session* session) {
    // Check SELECT privilege on all tables
    for (const Table& table : stmt->from_tables) {
        Status s = catalog_->checkPrivilege(
            session, table.table_id, OBJECT_TABLE, Privilege::SELECT, ctx_
        );
        if (s != Status::OK) return s;
    }
    // ... existing logic
}

Status Executor::executeInsert(InsertStmt* stmt, Session* session) {
    // Check INSERT privilege
    Status s = catalog_->checkPrivilege(
        session, stmt->table_id, OBJECT_TABLE, Privilege::INSERT, ctx_
    );
    if (s != Status::OK) return s;
    // ... existing logic
}

Status Executor::executeUpdate(UpdateStmt* stmt, Session* session) {
    // Check UPDATE and SELECT (for WHERE clause)
    uint32_t required = Privilege::UPDATE | Privilege::SELECT;
    Status s = catalog_->checkPrivilege(
        session, stmt->table_id, OBJECT_TABLE, required, ctx_
    );
    if (s != Status::OK) return s;
    // ... existing logic
}

Status Executor::executeDelete(DeleteStmt* stmt, Session* session) {
    // Check DELETE and SELECT (for WHERE clause)
    uint32_t required = Privilege::DELETE | Privilege::SELECT;
    Status s = catalog_->checkPrivilege(
        session, stmt->table_id, OBJECT_TABLE, required, ctx_
    );
    if (s != Status::OK) return s;
    // ... existing logic
}
```

**Files Modified**:
- `src/sblr/executor.cpp` (~200 lines - modifications to existing functions)

**Testing**:
- DML operations with insufficient privileges fail
- DML operations with sufficient privileges succeed
- Test role activation/deactivation

---

### 1.12: DDL Enforcement (25-35 hours)

#### Privilege Checks in DDL

```cpp
Status Executor::executeCreateTable(CreateTableStmt* stmt, Session* session) {
    // Check CREATE privilege on schema
    Status s = catalog_->checkPrivilege(
        session, stmt->schema_id, OBJECT_SCHEMA, Privilege::CREATE, ctx_
    );
    if (s != Status::OK) return s;
    // ... existing logic
}

Status Executor::executeDropTable(DropTableStmt* stmt, Session* session) {
    // Must be owner or superuser
    TableInfo table;
    catalog_->getTable(stmt->table_id, table);

    if (table.owner_id != session->user_id && !session->is_superuser) {
        return Status::PERMISSION_DENIED;
    }
    // ... existing logic
}

Status Executor::executeAlterTable(AlterTableStmt* stmt, Session* session) {
    // Must be owner or superuser
    TableInfo table;
    catalog_->getTable(stmt->table_id, table);

    if (table.owner_id != session->user_id && !session->is_superuser) {
        return Status::PERMISSION_DENIED;
    }
    // ... existing logic
}
```

**Files Modified**:
- `src/sblr/executor.cpp` (~200 lines - modifications to existing functions)

**Testing**:
- CREATE TABLE without CREATE privilege fails
- DROP TABLE by non-owner fails
- ALTER TABLE by non-owner fails
- Superuser can do all operations

---

### 1.13: Block Direct Catalog Access (10-15 hours)

#### Implementation

```cpp
// File: src/sblr/executor.cpp

Status Executor::executeSelect(SelectStmt* stmt, Session* session) {
    for (const Table& table : stmt->from_tables) {
        // Get schema for this table
        TableInfo table_info;
        catalog_->getTable(table.table_id, table_info);

        SchemaInfo schema;
        catalog_->getSchema(table_info.schema_id, schema);

        // Block access to sys.* schemas (except superuser)
        if (schema.schema_name == "sys" || schema.schema_name.starts_with("sys.")) {
            if (!session->is_superuser) {
                SET_ERROR_CONTEXT(ctx_, Status::PERMISSION_DENIED,
                    "Direct access to system catalog forbidden. Use SHOW commands.");
                return Status::PERMISSION_DENIED;
            }
        }

        // Normal privilege check
        // ...
    }
    // ... existing logic
}
```

**Files Modified**:
- `src/sblr/executor.cpp` (~50 lines)

**Testing**:
- Non-superuser SELECT from sys.* fails
- Superuser SELECT from sys.* succeeds
- SHOW commands work for all users

---

### Phase 1 Summary

**Total Estimated Effort**: 200-300 hours

**Deliverables**:
- ✅ Users, Roles, Groups, RoleMemberships, GroupMemberships CRUD
- ✅ Permissions CRUD (grant/revoke)
- ✅ Session management with privilege caching
- ✅ GRANT/REVOKE SQL syntax (parsing, bytecode, execution)
- ✅ SET ROLE / RESET ROLE
- ✅ DML privilege enforcement (SELECT/INSERT/UPDATE/DELETE)
- ✅ DDL privilege enforcement (CREATE/DROP/ALTER TABLE)
- ✅ Block direct sys.* access

**Testing Checklist**:
- [ ] Create/drop users, roles, groups
- [ ] Grant/revoke privileges to user/role/group/PUBLIC
- [ ] Set/reset role
- [ ] DML operations enforce privileges
- [ ] DDL operations enforce ownership
- [ ] Non-superuser cannot query sys.* schemas

---

## Phase 2: SHOW Commands & Enforcement (ALPHA)

**Estimated Effort**: 150-250 hours (4-6 weeks)
**Priority**: HIGH
**Dependencies**: Phase 1 complete

### 2.1: SHOW Command Parsing (20-30 hours)

#### AST Node

```cpp
// File: include/scratchbird/parser/ast.h

enum class ShowTarget : uint8_t {
    TABLES, TABLE, COLUMNS, COLUMN, INDEXES, INDEX,
    VIEWS, VIEW, SEQUENCES, SEQUENCE,
    FUNCTIONS, FUNCTION, PROCEDURES, PROCEDURE,
    SCHEMAS, SCHEMA, TRIGGERS, TRIGGER,
    CONSTRAINTS, CONSTRAINT, DOMAINS, DOMAIN, TYPES, TYPE,
    USERS, ROLES, GROUPS,
    COMMENT, METADATA, DEPENDENCIES, DEPENDENTS,
    PRIVILEGES, GRANTS,
    DATABASE, TABLESPACES,
    CURRENT_USER, CURRENT_ROLE, CURRENT_SCHEMA,
    SESSION, ALL_SETTINGS
};

struct ShowStmt : public Statement {
    ShowTarget target;
    string object_name;
    string parent_name;         // e.g., table name for SHOW COLUMNS
    string schema_name;
    string pattern;             // LIKE pattern

    ShowStmt() : Statement(StatementType::SHOW) {}
};
```

#### Parser Implementation

```cpp
// File: src/parser/parser.cpp

unique_ptr<Statement> Parser::parseShow();
```

**Files Modified**:
- `include/scratchbird/parser/ast.h` (~50 lines)
- `src/parser/parser.cpp` (~200 lines)
- `include/scratchbird/parser/token.h` (new keywords: SHOW, METADATA, DEPENDENCIES, etc.)

**Testing**:
- Parse all SHOW variants
- Parse LIKE patterns
- Parse IN schema_name clauses

---

### 2.2: SHOW Command Bytecode (10-15 hours)

#### Opcodes

```cpp
// File: include/scratchbird/sblr/opcodes.h

enum Opcode : uint8_t {
    // ... existing opcodes
    OP_SHOW_TABLES,
    OP_SHOW_TABLE,
    OP_SHOW_COLUMNS,
    OP_SHOW_INDEXES,
    OP_SHOW_METADATA,
    OP_SHOW_DEPENDENCIES,
    OP_SHOW_PRIVILEGES,
    // ... one per ShowTarget
};
```

#### Bytecode Generator

```cpp
// File: src/sblr/bytecode_generator.cpp

Status BytecodeGenerator::generateShow(ShowStmt* stmt);
```

**Files Modified**:
- `include/scratchbird/sblr/opcodes.h`
- `src/sblr/bytecode_generator.cpp` (~100 lines)

---

### 2.3: Permission-Filtered List Functions (30-40 hours)

#### Catalog Methods

```cpp
// File: src/core/catalog_manager.cpp

Status listTablesFiltered(
    const Session* session,
    const ID& schema_id,
    const string& pattern,
    vector<TableInfo>& tables_out,
    ErrorContext* ctx
);

Status listColumnsFiltered(
    const Session* session,
    const ID& table_id,
    const string& pattern,
    vector<ColumnInfo>& columns_out,
    ErrorContext* ctx
);

Status listIndexesFiltered(
    const Session* session,
    const ID& schema_id,
    const string& pattern,
    vector<IndexInfo>& indexes_out,
    ErrorContext* ctx
);

Status listSchemasFiltered(
    const Session* session,
    const string& pattern,
    vector<SchemaInfo>& schemas_out,
    ErrorContext* ctx
);

// ... similar for views, sequences, functions, procedures
```

#### Pattern Matching

```cpp
bool matchesPattern(const string& str, const string& pattern);
```

**Files Modified**:
- `src/core/catalog_manager.cpp` (~300 lines)
- `include/scratchbird/core/catalog_manager.h` (declarations)

**Testing**:
- List returns only objects user has privileges on
- LIKE patterns work correctly
- Empty pattern returns all (filtered)

---

### 2.4: SHOW Command Executor (40-60 hours)

#### Executor Implementation

```cpp
// File: src/sblr/executor.cpp

Status Executor::executeShowTables(ShowStmt* stmt, Session* session);
Status Executor::executeShowColumns(ShowStmt* stmt, Session* session);
Status Executor::executeShowIndexes(ShowStmt* stmt, Session* session);
Status Executor::executeShowMetadata(ShowStmt* stmt, Session* session);
Status Executor::executeShowDependencies(ShowStmt* stmt, Session* session);
Status Executor::executeShowPrivileges(ShowStmt* stmt, Session* session);
// ... one per ShowTarget
```

#### SHOW METADATA Implementation

```cpp
Status Executor::executeShowMetadata(ShowStmt* stmt, Session* session) {
    // 1. Resolve object
    ID object_id;
    ObjectType object_type;
    catalog_->resolveObject(stmt->object_name, stmt->schema_name,
                            &object_id, &object_type);

    // 2. Check privileges
    if (!catalog_->hasAnyPrivilege(session, object_id, object_type)) {
        return Status::PERMISSION_DENIED;
    }

    // 3. Build result set (name-value pairs)
    ResultSet result;
    result.addColumn("property", DataType::VARCHAR);
    result.addColumn("value", DataType::VARCHAR);

    // 4. Gather metadata (type-specific)
    // ... add rows for each property

    session->setResult(result);
    return Status::OK;
}
```

**Files Modified**:
- `src/sblr/executor.cpp` (~600 lines)
- `include/scratchbird/sblr/executor.h` (declarations)

**Testing**:
- SHOW TABLES returns only visible tables
- SHOW COLUMNS requires privilege on table
- SHOW METADATA returns comprehensive info
- SHOW DEPENDENCIES/DEPENDENTS work correctly
- SHOW PRIVILEGES shows user's privileges

---

### 2.5: Extended DDL Enforcement (30-40 hours)

#### Additional Privilege Checks

```cpp
// CREATE INDEX
Status Executor::executeCreateIndex(CreateIndexStmt* stmt, Session* session) {
    // Must be owner of table or superuser
    TableInfo table;
    catalog_->getTable(stmt->table_id, table);

    if (table.owner_id != session->user_id && !session->is_superuser) {
        return Status::PERMISSION_DENIED;
    }
    // ... existing logic
}

// CREATE VIEW
Status Executor::executeCreateView(CreateViewStmt* stmt, Session* session) {
    // Need CREATE on schema
    Status s = catalog_->checkPrivilege(
        session, stmt->schema_id, OBJECT_SCHEMA, Privilege::CREATE, ctx_
    );
    if (s != Status::OK) return s;

    // Need SELECT on all base tables
    for (const ID& table_id : stmt->base_table_ids) {
        Status s2 = catalog_->checkPrivilege(
            session, table_id, OBJECT_TABLE, Privilege::SELECT, ctx_
        );
        if (s2 != Status::OK) return s2;
    }
    // ... existing logic
}

// CREATE FUNCTION
Status Executor::executeCreateFunction(CreateFunctionStmt* stmt, Session* session) {
    // Need CREATE on schema
    Status s = catalog_->checkPrivilege(
        session, stmt->schema_id, OBJECT_SCHEMA, Privilege::CREATE, ctx_
    );
    if (s != Status::OK) return s;
    // ... existing logic
}

// EXECUTE function
Status Executor::executeFunctionCall(FunctionCallExpr* expr, Session* session) {
    // Need EXECUTE privilege
    Status s = catalog_->checkPrivilege(
        session, expr->function_id, OBJECT_FUNCTION, Privilege::EXECUTE, ctx_
    );
    if (s != Status::OK) return s;
    // ... existing logic
}
```

**Files Modified**:
- `src/sblr/executor.cpp` (~200 lines - modifications)

**Testing**:
- CREATE VIEW without SELECT on base tables fails
- CREATE INDEX by non-owner fails
- EXECUTE function without privilege fails

---

### 2.6: CASCADE Logic for REVOKE (20-30 hours)

#### Implementation

```cpp
Status CatalogManager::revokePrivilege(
    const Session* session,
    uint32_t privileges,
    const ID& object_id,
    ObjectType object_type,
    const ID& grantee_id,
    GranteeType grantee_type,
    bool cascade,
    ErrorContext* ctx
) {
    // 1. Find all permissions granted by grantee (if they had GRANT OPTION)
    vector<PermissionInfo> dependent_grants;
    if (cascade) {
        findDependentGrants(grantee_id, object_id, dependent_grants);
    } else {
        // Check if there are dependent grants
        if (hasDependentGrants(grantee_id, object_id)) {
            SET_ERROR_CONTEXT(ctx, Status::DEPENDENT_OBJECTS_EXIST,
                "Cannot revoke: dependent grants exist. Use CASCADE.");
            return Status::DEPENDENT_OBJECTS_EXIST;
        }
    }

    // 2. Revoke all dependent grants (if CASCADE)
    for (const PermissionInfo& grant : dependent_grants) {
        deletePermissionRecord(grant.permission_id, ctx);
    }

    // 3. Revoke the target privilege
    deletePermissionForGrantee(object_id, grantee_id, grantee_type, privileges, ctx);

    // 4. Invalidate caches
    invalidatePrivilegeCachesForGrantee(grantee_id, grantee_type);

    return Status::OK;
}
```

**Files Modified**:
- `src/core/catalog_manager.cpp` (~150 lines)

**Testing**:
- REVOKE without CASCADE fails if dependent grants exist
- REVOKE with CASCADE removes all dependent grants
- Privilege chains are handled correctly

---

### Phase 2 Summary

**Total Estimated Effort**: 150-250 hours

**Deliverables**:
- ✅ All SHOW commands implemented
- ✅ Permission-filtered results
- ✅ Pattern matching (LIKE)
- ✅ SHOW METADATA comprehensive info
- ✅ Extended DDL privilege enforcement
- ✅ CASCADE logic for REVOKE

**Testing Checklist**:
- [ ] SHOW TABLES returns only visible tables
- [ ] SHOW COLUMNS requires table privilege
- [ ] SHOW METADATA returns all properties
- [ ] SHOW PRIVILEGES shows effective privileges
- [ ] CREATE VIEW checks base table privileges
- [ ] REVOKE CASCADE removes dependent grants

---

## Phase 3: Advanced Features (BETA)

**Estimated Effort**: 162-268 hours (4-7 weeks)
**Priority**: MEDIUM
**Dependencies**: Phase 1-2 complete

### 3.0: Phase 2 Completion Tasks (12-18 hours)

**Priority**: HIGH
**Status**: Required before Phase 3 features

These tasks complete the Phase 2 implementation and must be done first:

#### 3.0.1: Password Hashing Implementation (2-3 hours)

**Current State**: Placeholder `"hashed_" + password` in executors
**Required**: bcrypt or argon2 integration
**Priority**: HIGH (security vulnerability)

**Files to Modify**:
- `src/sblr/executor.cpp` (lines 12456-12467, 12519-12520)

**Implementation**:
```cpp
// Add dependency: libbcrypt or libargon2
#include <bcrypt/BCrypt.hpp>

// In executeCreateUser() and executeAlterUser()
std::string hashPassword(const std::string& plaintext)
{
    // Using bcrypt with 12 rounds (~100ms per hash)
    return BCrypt::generateHash(plaintext, 12);
}

bool verifyPassword(const std::string& plaintext, const std::string& hash)
{
    return BCrypt::validatePassword(plaintext, hash);
}

// Replace placeholder in executors:
if (has_password)
{
    password_hash = hashPassword(password);  // Instead of "hashed_" + password
}
```

**CMake Changes**:
```cmake
# Add to CMakeLists.txt
find_package(BCrypt REQUIRED)
target_link_libraries(scratchbird_sblr BCrypt::BCrypt)
```

**Testing**:
- Test password hashing during CREATE USER
- Test password verification (when authentication is added)
- Test ALTER USER password change
- Benchmark hashing performance (~50-100ms acceptable)

---

#### 3.0.2: Integration Test API Update (10 minutes)

**Current State**: Test uses old Parser API (single parameter constructor)
**Required**: Update to Lexer + StringPool pattern

**File to Modify**:
- `tests/integration/test_security_phase2.cpp`

**Changes Required**:
```cpp
// OLD (incorrect):
Parser parser(sql);
auto parse_result = parser.parse();

// NEW (correct):
StringPool pool;
Lexer lexer(sql, pool);
Parser parser(lexer, pool);
auto stmt = parser.parse();

// Also update BytecodeGenerator constructor calls:
BytecodeGenerator generator(pool, db.get());
```

**Pattern to Follow**: See `docs/archive/phase1_standalone_tests/test_aggregation_execution.cpp`

**Testing**:
- Build test: `make test_security_phase2`
- Run test: `./test_security_phase2`
- Verify all 15 test cases pass

---

#### 3.0.3: Superuser Flag in ALTER USER (1 hour)

**Current State**: `updateUser()` doesn't support changing superuser flag
**Required**: Extend catalog API

**Files to Modify**:
- `include/scratchbird/core/catalog_manager.h` (line ~800)
- `src/core/catalog_manager.cpp` (UserRecord update function)

**Implementation**:
```cpp
// Option 1: Extend existing updateUser()
Status updateUser(
    const ID& user_id,
    const std::string& password_hash,
    const ID& default_schema_id,
    bool is_active,
    bool is_superuser,  // NEW PARAMETER
    ErrorContext* ctx
);

// Option 2: Add separate function
Status setUserSuperuser(
    const ID& user_id,
    bool is_superuser,
    ErrorContext* ctx
);
```

**Executor Changes**:
- `src/sblr/executor.cpp` (line 12523-12525)
- Use extended API to change superuser flag

**Testing**:
- ALTER USER user SUPERUSER
- ALTER USER user NOSUPERUSER
- Verify catalog persists changes
- Verify non-superusers cannot promote themselves

---

#### 3.0.4: Role Transitive Closure in checkPermission() (3-4 hours)

**Current State**: Only checks direct user permissions
**Required**: Check active_role, groups, PUBLIC permissions

**File to Modify**:
- `src/sblr/executor.cpp` (lines 13179-13185)

**Implementation**:
```cpp
bool Executor::checkPermission(const ID& object_id,
                              PermissionObjectType object_type,
                              uint32_t required_privilege)
{
    if (!conn_ctx_)
        return false;

    // Superusers bypass all checks
    if (conn_ctx_->isSuperuser())
        return true;

    const ID& current_user_id = conn_ctx_->getCurrentUserId();
    const ID& active_role_id = conn_ctx_->getActiveRoleId();

    ErrorContext err_ctx;
    bool has_permission = false;

    // 1. Check user's direct permissions
    auto status = db_->catalog_manager()->hasPermission(
        current_user_id, object_id, object_type,
        static_cast<Privilege>(required_privilege),
        has_permission, &err_ctx);

    if (status == Status::OK && has_permission)
        return true;

    // 2. Check active role permissions (if role is active)
    static const ID zero_id = {};
    if (active_role_id != zero_id)
    {
        status = db_->catalog_manager()->hasPermission(
            active_role_id, object_id, object_type,
            static_cast<Privilege>(required_privilege),
            has_permission, &err_ctx);

        if (status == Status::OK && has_permission)
            return true;
    }

    // 3. Check PUBLIC permissions
    // Get PUBLIC special role ID
    CatalogManager::RoleInfo public_role;
    status = db_->catalog_manager()->getRoleByName("PUBLIC", public_role, &err_ctx);
    if (status == Status::OK)
    {
        status = db_->catalog_manager()->hasPermission(
            public_role.role_id, object_id, object_type,
            static_cast<Privilege>(required_privilege),
            has_permission, &err_ctx);

        if (status == Status::OK && has_permission)
            return true;
    }

    // 4. Check group permissions
    std::vector<CatalogManager::GroupMembershipInfo> user_groups;
    status = db_->catalog_manager()->getUserGroups(current_user_id, user_groups, &err_ctx);
    if (status == Status::OK)
    {
        for (const auto& group_membership : user_groups)
        {
            status = db_->catalog_manager()->hasPermission(
                group_membership.group_id, object_id, object_type,
                static_cast<Privilege>(required_privilege),
                has_permission, &err_ctx);

            if (status == Status::OK && has_permission)
                return true;
        }
    }

    return false;
}
```

**Testing**:
- Grant privilege to role, verify user with role can access
- Grant privilege to group, verify group members can access
- Grant privilege to PUBLIC, verify all users can access
- Test permission hierarchy (user > role > group > PUBLIC)

---

#### 3.0.5: CASCADE Implementation (5-8 hours)

**Current State**: CASCADE specified but not enforced in DROP operations
**Required**: Recursive dependency deletion

**Files to Modify**:
- `src/core/catalog_manager.cpp` (deleteUser, deleteRole, deleteGroup functions)
- `src/sblr/executor.cpp` (executeDropUser, executeDropRole, executeDropGroup)

**Implementation**:
```cpp
Status CatalogManager::deleteUser(const ID& user_id, bool cascade, ErrorContext* ctx)
{
    if (cascade)
    {
        // 1. Remove all role memberships
        std::vector<RoleMembershipInfo> roles;
        getUserRoles(user_id, roles, ctx);
        for (const auto& role : roles)
        {
            revokeRole(user_id, role.role_id, ctx);
        }

        // 2. Remove all group memberships
        std::vector<GroupMembershipInfo> groups;
        getUserGroups(user_id, groups, ctx);
        for (const auto& group : groups)
        {
            removeGroupMember(user_id, group.group_id, ctx);
        }

        // 3. Remove all granted permissions
        // Query sb_permissions where grantee_id = user_id
        // Delete matching records

        // 4. Revoke permissions granted BY this user
        // Query sb_permissions where grantor_id = user_id
        // Either delete or transfer to system user
    }
    else  // RESTRICT (default)
    {
        // Check for dependencies
        std::vector<RoleMembershipInfo> roles;
        getUserRoles(user_id, roles, ctx);
        if (!roles.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::ERROR_DEPENDENCY,
                "Cannot drop user: has role memberships. Use CASCADE.");
            return Status::ERROR_DEPENDENCY;
        }

        // Check for group memberships, granted permissions, etc.
    }

    // Finally, mark user as deleted (soft delete)
    return Status::OK;
}
```

**Testing**:
- DROP USER CASCADE removes all dependencies
- DROP USER RESTRICT fails if dependencies exist
- Verify cascading deletes for roles and groups
- Test grantor_id handling when user is dropped

---

#### 3.0.6: Documentation Updates (1 hour)

**Files to Update**:
- `docs/guides/SECURITY_SYSTEM_USAGE_GUIDE.md` - Add password hashing notes
- `PROJECT_CONTEXT.md` - Update Phase 2 status to 100% complete
- `docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md` - Mark Phase 2 complete

**Content**:
- Document password hashing algorithm (bcrypt with 12 rounds)
- Document CASCADE behavior for DROP operations
- Update completion percentages
- Add Phase 3 planning section

---

### 3.0 Summary

**Total Effort**: 12-18 hours
**Priority**: HIGH (must complete before Phase 3 features)
**Risk**: LOW (straightforward implementations)

**Completion Criteria**:
- ✅ Password hashing implemented with bcrypt
- ✅ Integration test runs and passes
- ✅ ALTER USER can change superuser flag
- ✅ checkPermission() checks all permission sources
- ✅ CASCADE properly deletes dependencies
- ✅ Documentation updated

**Once 3.0 is complete, Phase 2 will be 100% production-ready.**

---

### 3.1: External Authentication Interface (30-40 hours)

#### Interface Definition

```cpp
// File: include/scratchbird/auth/external_auth.h (NEW FILE)

class ExternalAuthenticator {
public:
    virtual ~ExternalAuthenticator() = default;

    virtual Status authenticate(
        const string& username,
        const string& password,
        string& external_user_id,
        vector<string>& group_memberships,
        ErrorContext* ctx
    ) = 0;

    virtual Status getGroupMemberships(
        const string& username,
        vector<string>& group_memberships,
        ErrorContext* ctx
    ) = 0;
};
```

**Files Created**:
- `include/scratchbird/auth/external_auth.h`

---

### 3.2: LDAP Authenticator (40-60 hours)

#### Implementation

```cpp
// File: src/auth/ldap_auth.cpp (NEW FILE)

class LDAPAuthenticator : public ExternalAuthenticator {
    string ldap_server_;
    uint16_t ldap_port_;
    string base_dn_;
    string bind_dn_;
    string bind_password_;

public:
    Status authenticate(...) override;
    Status getGroupMemberships(...) override;
};
```

**Dependencies**: OpenLDAP library

**Files Created**:
- `src/auth/ldap_auth.cpp`
- `include/scratchbird/auth/ldap_auth.h`

**Testing**:
- Authenticate against test LDAP server
- Retrieve group memberships
- Handle LDAP errors gracefully

---

### 3.3: Group Mapping (20-30 hours)

#### CRUD Operations

```cpp
Status createGroupMapping(
    const string& external_group,
    AuthMethod auth_method,
    const ID& internal_group_id,
    ErrorContext* ctx
);

Status dropGroupMapping(
    const ID& mapping_id,
    ErrorContext* ctx
);

Status mapExternalGroup(
    const string& external_group,
    AuthMethod auth_method,
    ID& internal_group_id,
    ErrorContext* ctx
);
```

#### SQL Interface

```sql
CREATE GROUP MAPPING
    EXTERNAL 'cn=engineers,ou=groups,dc=company,dc=com'
    FROM LDAP
    TO INTERNAL GROUP engineering;

DROP GROUP MAPPING EXTERNAL '...' FROM LDAP;
```

**Files Modified**:
- `src/core/catalog_manager.cpp` (~150 lines)
- `include/scratchbird/parser/ast.h` (GroupMappingStmt)
- `src/parser/parser.cpp` (parse CREATE/DROP GROUP MAPPING)

**Testing**:
- Create/drop group mappings
- Map external groups during authentication
- Handle unmapped groups

---

### 3.4: Periodic Revalidation (20-30 hours)

#### Implementation

```cpp
// File: src/core/session.cpp (NEW FILE)

Status Session::validateGroupMemberships() {
    // Check if revalidation needed
    uint64_t elapsed = getCurrentTime() - external_auth_time;
    if (elapsed < config_.group_revalidation_interval_seconds) {
        return Status::OK;
    }

    // Re-query external authenticator
    vector<string> current_external_groups;
    Status s = external_auth_->getGroupMemberships(username, current_external_groups);

    if (s != Status::OK) {
        if (config_.terminate_session_on_revalidation_failure) {
            is_valid = false;
            return Status::SESSION_INVALID;
        }
        return Status::OK;  // Continue with stale groups
    }

    // Update group memberships
    vector<ID> new_group_ids = catalog_->mapAndResolveGroups(
        current_external_groups, auth_method
    );

    {
        std::lock_guard<std::mutex> lock(mutex);
        group_ids = new_group_ids;
        privilege_cache.clear();
        external_auth_time = getCurrentTime();
    }

    return Status::OK;
}
```

#### Background Thread

```cpp
// File: src/core/database.cpp

void Database::revalidationThread() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(60));

        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (Session* session : active_sessions_) {
            if (!session->auth_method.empty() && session->auth_method != "internal") {
                session->validateGroupMemberships();
            }
        }
    }
}
```

**Files Created**:
- `src/core/session.cpp`

**Files Modified**:
- `src/core/database.cpp` (~100 lines)

**Testing**:
- Revalidation updates group memberships
- Failed revalidation handled correctly
- Configuration options work

---

### 3.5: Group Nesting (20-30 hours)

#### Implementation

```cpp
Status addGroupToGroup(
    const ID& child_group_id,
    const ID& parent_group_id,
    const ID& grantor_id,
    ErrorContext* ctx
);

Status removeGroupFromGroup(
    const ID& child_group_id,
    const ID& parent_group_id,
    ErrorContext* ctx
);

// Transitive resolution with cycle detection
vector<ID> CatalogManager::resolveGroupMemberships(const ID& user_id) {
    vector<ID> all_groups;
    unordered_set<ID> visited;
    queue<ID> to_process;

    // BFS
    vector<ID> direct = getDirectGroupsForUser(user_id);
    for (const ID& group_id : direct) {
        to_process.push(group_id);
    }

    while (!to_process.empty()) {
        ID group_id = to_process.front();
        to_process.pop();

        if (visited.count(group_id)) continue;  // Cycle
        visited.insert(group_id);
        all_groups.push_back(group_id);

        vector<ID> parents = getParentGroups(group_id);
        for (const ID& parent : parents) {
            to_process.push(parent);
        }
    }

    return all_groups;
}
```

**Files Modified**:
- `src/core/catalog_manager.cpp` (~150 lines)

**Testing**:
- Add groups to groups
- Transitive membership resolution
- Cycle detection

---

### 3.6: Column-Level Privileges (Optional, 20-30 hours)

#### Extended PermissionRecord

```cpp
struct PermissionRecord {
    // ... existing fields
    uint16_t column_count;
    ID column_ids[16];  // Specific columns (if column-level)
};
```

#### SQL Syntax

```sql
GRANT UPDATE (salary, bonus) ON TABLE employees TO alice;
REVOKE UPDATE (salary) ON TABLE employees FROM alice;
```

**Files Modified**:
- `src/core/catalog_manager.cpp` (update structures, CRUD)
- `include/scratchbird/parser/ast.h` (GrantStmt with column list)
- `src/sblr/executor.cpp` (check column-level privileges in UPDATE)

**Testing**:
- Grant/revoke column-level privileges
- UPDATE with column privileges

---

### Phase 3 Summary

**Total Estimated Effort**: 150-250 hours

**Deliverables**:
- ✅ External authentication interface
- ✅ LDAP authenticator
- ✅ Group mapping (external → internal)
- ✅ Periodic revalidation
- ✅ Group nesting with cycle detection
- ⚠️ Column-level privileges (optional)

**Testing Checklist**:
- [ ] Authenticate via LDAP
- [ ] Map LDAP groups to internal groups
- [ ] Revalidation updates group memberships
- [ ] Group nesting works correctly
- [ ] Column-level privileges (if implemented)

---

## Phase 4: Optional Features (Future)

**Estimated Effort**: 100-150 hours
**Priority**: LOW
**Dependencies**: Phase 1-3 complete

### 4.1: Row-Level Security (60-90 hours)

#### Policy Definition

```sql
CREATE POLICY employees_rls ON employees
    FOR SELECT
    USING (department_id = current_user_department());

CREATE POLICY employees_update_rls ON employees
    FOR UPDATE
    USING (department_id = current_user_department())
    WITH CHECK (salary <= 100000);
```

#### Implementation

- Parse CREATE POLICY
- Store policy in catalog
- Inject policy predicates into WHERE clauses
- Optimize with indexes

---

### 4.2: Compatibility Views (40-60 hours)

#### information_schema Views

```sql
CREATE VIEW information_schema.tables AS
SELECT ...
WHERE <user has privileges>;

CREATE VIEW information_schema.columns AS ...;
CREATE VIEW information_schema.table_privileges AS ...;
```

#### pg_catalog Views

```sql
CREATE VIEW pg_catalog.pg_tables AS ...;
CREATE VIEW pg_catalog.pg_class AS ...;
```

---

## Testing Plan

### Unit Tests

**Catalog CRUD** (20 tests):
- Create/drop users, roles, groups
- Grant/revoke memberships
- Grant/revoke privileges
- Permission calculations

**Privilege Resolution** (15 tests):
- User direct privileges
- Role privileges (active/inactive)
- Group privileges (cumulative)
- PUBLIC privileges
- Effective privilege calculation
- Cache invalidation

**Pattern Matching** (5 tests):
- LIKE patterns with %, _
- Case sensitivity
- Empty patterns

### Integration Tests

**Authentication** (10 tests):
- Successful login
- Failed login (wrong password, inactive user)
- Session creation with memberships
- External authentication

**GRANT/REVOKE** (20 tests):
- Grant to user/role/group/PUBLIC
- WITH GRANT OPTION
- CASCADE revoke
- Privilege inheritance

**SHOW Commands** (15 tests):
- SHOW TABLES filters correctly
- SHOW METADATA returns all fields
- SHOW PRIVILEGES accurate
- Pattern matching

**DML/DDL Enforcement** (25 tests):
- SELECT/INSERT/UPDATE/DELETE with privileges
- CREATE/DROP TABLE ownership
- CREATE VIEW base table privileges
- sys.* access blocked

### Security Tests

**Privilege Escalation** (10 tests):
- Non-owner cannot ALTER TABLE
- Non-owner cannot DROP TABLE
- GRANT OPTION enforcement
- WITH ADMIN OPTION enforcement

**SQL Injection** (5 tests):
- SHOW commands with malicious patterns
- GRANT/REVOKE with malicious names

**Concurrent Access** (10 tests):
- Concurrent GRANT/REVOKE
- Privilege cache consistency
- Session isolation

### Performance Tests

**Benchmark** (5 tests):
- Privilege check latency (with/without cache)
- SHOW TABLES with 10,000 tables
- Effective privilege calculation with 100 groups
- Group nesting (10 levels deep)

---

## Migration Strategy

### Fresh Database

**ALPHA Release**: Fresh database creation only (no migration).

**Bootstrap**:
1. Allocate all security tables
2. Create SYSTEM user
3. Create PUBLIC role
4. Create DB_OWNER role
5. Grant default privileges

### Existing Database Upgrade (BETA)

**Migration Steps**:
1. Add new catalog tables (group_memberships, group_mappings)
2. Update PermissionRecord structure
3. Migrate existing permissions (if any)
4. Create SYSTEM user, PUBLIC role
5. Assign all existing users to PUBLIC role
6. Grant all privileges to first user (DB_OWNER)

**Migration Script**: `scripts/migrate_security_v1.sql`

---

## Dependencies

### External Libraries

**Required**:
- bcrypt or argon2 (password hashing)

**Optional**:
- OpenLDAP (for LDAP auth)
- libkrb5 (for Kerberos auth)

### Internal Dependencies

**Phase 1**:
- Catalog system (✅ complete)
- TOAST system (✅ complete)
- UUID system (✅ complete)

**Phase 2**:
- Phase 1 complete

**Phase 3**:
- Phase 1-2 complete
- External auth library

---

## Risk Assessment

### High Risk

**Privilege Escalation Bugs**: Incorrect privilege checks could allow unauthorized access.
- **Mitigation**: Comprehensive testing, security audit, code review

**Performance Impact**: Privilege checks on every operation.
- **Mitigation**: Aggressive caching, optimize hot paths

**External Auth Downtime**: LDAP server unavailable.
- **Mitigation**: Cached group memberships, fallback to stale data

### Medium Risk

**Compatibility**: Tools expecting information_schema views.
- **Mitigation**: Implement views in Phase 4

**Migration**: Upgrading existing databases.
- **Mitigation**: Test migration thoroughly, provide rollback

### Low Risk

**Group Nesting Cycles**: Infinite loops in group resolution.
- **Mitigation**: Cycle detection in BFS

**Privilege Cache Stale**: Cache not invalidated properly.
- **Mitigation**: TTL on cache entries, invalidation on GRANT/REVOKE

---

## Timeline Summary

| Phase | Effort (hours) | Duration (weeks) | Priority | Status |
|-------|----------------|------------------|----------|--------|
| Phase 1: Core Infrastructure | 200-300 | 5-8 | HIGH | ⏳ Pending |
| Phase 2: SHOW Commands | 150-250 | 4-6 | HIGH | ⏳ Pending |
| Phase 3: Advanced Features | 150-250 | 4-6 | MEDIUM | ⏳ Pending |
| Phase 4: Optional Features | 100-150 | 3-4 | LOW | ⏳ Pending |
| **Total** | **600-900** | **15-23** | | |

**Assumptions**: 1 developer, 40 hours/week

**Parallel Work Opportunities**:
- Phase 1.1-1.6 (catalog) can be done in parallel with Phase 1.7-1.13 (session/enforcement)
- Phase 2.1-2.2 (parsing/bytecode) can be done in parallel with Phase 2.3-2.4 (executor)

---

## Appendix: File Manifest

### New Files Created

```
include/scratchbird/core/session.h
include/scratchbird/auth/external_auth.h
include/scratchbird/auth/ldap_auth.h
src/core/session.cpp
src/auth/ldap_auth.cpp
```

### Modified Files

```
src/core/catalog_manager.cpp          (~1,500 lines added)
include/scratchbird/core/catalog_manager.h  (~400 lines added)
src/core/database.cpp                 (~300 lines added)
include/scratchbird/parser/ast.h      (~200 lines added)
src/parser/parser.cpp                 (~500 lines added)
include/scratchbird/parser/token.h    (~30 lines added)
include/scratchbird/sblr/opcodes.h    (~15 lines added)
src/sblr/bytecode_generator.cpp       (~300 lines added)
src/sblr/executor.cpp                 (~1,200 lines added)
include/scratchbird/sblr/executor.h   (~50 lines added)
```

**Total New Code**: ~4,500 lines

---

**End of Implementation Plan**
