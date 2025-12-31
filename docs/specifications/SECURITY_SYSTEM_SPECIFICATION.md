# ScratchBird Security System Specification

**Version**: 1.0
**Date**: November 9, 2025
**Status**: Design Complete - Ready for Implementation
**Author**: dcalford with Claude Code

---

## Table of Contents

1. [Overview](#overview)
2. [Core Philosophy](#core-philosophy)
3. [Architecture](#architecture)
4. [Authentication](#authentication)
5. [Authorization Model](#authorization-model)
6. [Roles vs Groups](#roles-vs-groups)
7. [Privilege System](#privilege-system)
8. [GRANT/REVOKE Commands](#grantrevoke-commands)
9. [SHOW Commands](#show-commands)
10. [External Authentication](#external-authentication)
11. [Catalog Integration](#catalog-integration)
12. [Implementation Details](#implementation-details)
13. [Security Enforcement](#security-enforcement)
14. [Configuration](#configuration)
15. [Examples](#examples)

---

## 1. Overview

ScratchBird implements a **three-layer security model** combining:
- **Role-Based Access Control (RBAC)** - Context-switching privileges
- **Group-Based Access Control (GBAC)** - Cumulative privileges
- **Access Control Lists (ACLs)** - Object-level permissions

This design is based on **Firebird's security model** with **PostgreSQL-compatible SQL syntax** and **Active Directory/LDAP integration**.

### Key Design Principles

1. **No Direct Catalog Access** - System tables in `sys.*` schemas are protected
2. **SHOW Commands** - Controlled metadata discovery with permission filtering
3. **Embedded-First** - Designed for embedded database (no network layer)
4. **MGA-Compatible** - Security checks happen before transaction operations, not during visibility
5. **External Auth Ready** - LDAP, Kerberos, Active Directory integration built-in

---

## 2. Core Philosophy

### Firebird-Compatible with PostgreSQL Extensions

**Baseline**: Firebird security model (matches MGA architecture)
**Extensions**: PostgreSQL-compatible syntax where no conflict exists
**Integration**: AD/LDAP group mapping for enterprise environments

### Security by Design

- Catalog tables are **not directly queryable** by non-superusers
- All metadata discovery goes through **permission-filtered SHOW commands**
- Optional **information_schema views** for tool compatibility
- All operations require **explicit privilege checks**

---

## 3. Architecture

### Three-Layer Permission Model

```
┌─────────────────────────────────────────────────────┐
│ Layer 1: User Direct Privileges                     │
│  - Explicitly granted to user                        │
│  - SELECT, INSERT, UPDATE, DELETE, etc.              │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│ Layer 2: Role Privileges (Exclusive)                │
│  - User must SET ROLE to activate                    │
│  - Only ONE role active at a time                    │
│  - Context-switching (like sudo)                     │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│ Layer 3: Group Privileges (Cumulative)              │
│  - Always active (all groups simultaneously)         │
│  - Automatic privilege union                         │
│  - Can be nested (groups in groups)                  │
│  - Maps to external auth (LDAP/AD)                   │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│ Layer 4: PUBLIC Role (Implicit)                     │
│  - All users are implicit members                    │
│  - Default privileges for all users                  │
└─────────────────────────────────────────────────────┘
```

### Privilege Resolution Algorithm

```cpp
granted_privileges =
    user_direct_privileges |
    active_role_privileges |        // Only ONE role
    union(all_group_privileges) |   // ALL groups
    public_role_privileges;

if (user_is_superuser) {
    granted_privileges = ALL;
}

if (user_is_owner) {
    granted_privileges = ALL;
}
```

---

## 4. Authentication

### Embedded Database Context

ScratchBird is an **embedded engine** (like SQLite, not PostgreSQL server):
- No network connections
- No `pg_hba.conf`
- Authentication at **database open time**
- Session persists for entire database connection

### Authentication Methods

#### Internal Authentication

```cpp
Status Database::open(
    const string& db_path,
    const string& username,
    const string& password,
    ErrorContext* ctx
) {
    // 1. Load UserRecord from sys.sec.users
    UserRecord user;
    Status s = catalog_->getUserByName(username, &user);
    if (s != Status::OK) return Status::AUTH_FAILED;

    // 2. Verify password hash (bcrypt/argon2)
    string password_hash = toast_->read(user.password_hash_oid);
    if (!verifyPassword(password, password_hash)) {
        return Status::AUTH_FAILED;
    }

    // 3. Create session
    Session* session = new Session();
    session->user_id = user.user_id;
    session->is_superuser = user.is_superuser;
    session->default_schema_id = user.default_schema_id;
    session->current_schema_id = user.default_schema_id;

    // 4. Load role memberships
    catalog_->getRoleMemberships(user.user_id, session->available_roles);

    // 5. Load group memberships (transitive)
    session->group_ids = catalog_->resolveGroupMemberships(user.user_id);

    // 6. Active role is NONE initially (user must SET ROLE)
    session->active_role_id = INVALID_ID;

    return Status::OK;
}
```

#### External Authentication (LDAP/Kerberos/AD)

```cpp
Status Database::authenticateExternal(
    const string& username,
    const string& password,
    const string& auth_method,  // "ldap", "kerberos", "ad"
    ErrorContext* ctx
) {
    // 1. Authenticate with external system
    ExternalAuthResult result;
    Status s = external_auth_->authenticate(username, password, auth_method, &result);
    if (s != Status::OK) return Status::AUTH_FAILED;

    // 2. Get external groups from authenticator
    vector<string> external_groups = result.group_memberships;

    // 3. Map external groups to internal groups
    vector<ID> internal_group_ids;
    for (const string& ext_group : external_groups) {
        ID internal_group_id;
        if (mapExternalGroup(ext_group, auth_method, &internal_group_id) == Status::OK) {
            internal_group_ids.push_back(internal_group_id);
        }
    }

    // 4. Resolve transitive group memberships
    vector<ID> all_groups = resolveGroupMemberships(internal_group_ids);

    // 5. Lookup or auto-create user
    ID user_id = lookupOrCreateExternalUser(username, auth_method);

    // 6. Create session with group memberships
    Session* session = new Session();
    session->user_id = user_id;
    session->group_ids = all_groups;
    session->external_auth_time = getCurrentTime();
    session->auth_method = auth_method;

    return Status::OK;
}
```

### Password Policy

```cpp
struct PasswordPolicy {
    uint32_t min_length = 8;
    bool require_uppercase = true;
    bool require_lowercase = true;
    bool require_digit = true;
    bool require_special = true;
    uint32_t max_age_days = 90;         // Password expiry
    uint32_t history_count = 5;         // Can't reuse last N passwords
    uint32_t max_failed_attempts = 5;   // Lockout threshold
    uint32_t lockout_duration_minutes = 30;
};
```

---

## 5. Authorization Model

### Session Structure

```cpp
struct Session {
    // Identity
    ID user_id;                          // Current user UUID
    string username;                     // For logging

    // Privileges
    ID active_role_id;                   // Currently active role (or INVALID_ID)
    vector<ID> available_roles;          // Roles user CAN activate
    vector<ID> group_ids;                // All groups (always active, transitive)

    // Context
    ID current_schema_id;                // Current working schema
    ID default_schema_id;                // From UserRecord

    // Flags
    bool is_superuser;                   // Cached from UserRecord

    // External auth
    string auth_method;                  // "internal", "ldap", "kerberos", "ad"
    uint64_t external_auth_time;         // For revalidation

    // Performance
    unordered_map<ID, uint32_t> privilege_cache;  // object_id -> privileges
    mutex cache_mutex;

    // Session management
    uint64_t session_start_time;
    uint64_t last_activity_time;
    uint32_t transaction_count;
};
```

### Privilege Check Entry Point

```cpp
Status CatalogManager::checkPrivilege(
    const Session* session,
    const ID& object_id,
    ObjectType object_type,
    uint32_t required_privileges,
    ErrorContext* ctx
) {
    // 1. SUPERUSER bypasses all checks
    if (session->is_superuser) {
        return Status::OK;
    }

    // 2. OBJECT OWNER has implicit ALL privileges
    ObjectInfo obj_info;
    if (getObjectInfo(object_id, object_type, &obj_info) == Status::OK) {
        if (obj_info.owner_id == session->user_id) {
            return Status::OK;
        }
    }

    // 3. Check privilege cache first
    {
        std::lock_guard<std::mutex> lock(session->cache_mutex);
        auto it = session->privilege_cache.find(object_id);
        if (it != session->privilege_cache.end()) {
            uint32_t cached_privileges = it->second;
            if ((cached_privileges & required_privileges) == required_privileges) {
                return Status::OK;
            }
            // Cache hit but insufficient privileges - still check grants
        }
    }

    // 4. Calculate granted privileges
    uint32_t granted_privileges = 0;

    // Layer 1: Direct USER grants
    granted_privileges |= getPrivilegesForUser(session->user_id, object_id);

    // Layer 2: ACTIVE ROLE grants (only ONE role, not all memberships)
    if (session->active_role_id != INVALID_ID) {
        granted_privileges |= getPrivilegesForRole(session->active_role_id, object_id);
    }

    // Layer 3: ALL GROUP grants (cumulative - union of all groups)
    for (const ID& group_id : session->group_ids) {
        granted_privileges |= getPrivilegesForGroup(group_id, object_id);
    }

    // Layer 4: PUBLIC role (implicit for everyone)
    granted_privileges |= getPrivilegesForRole(PUBLIC_ROLE_ID, object_id);

    // 5. Update cache
    {
        std::lock_guard<std::mutex> lock(session->cache_mutex);
        session->privilege_cache[object_id] = granted_privileges;
    }

    // 6. Check if we have required privileges
    if ((granted_privileges & required_privileges) == required_privileges) {
        return Status::OK;
    }

    // 7. Permission denied
    SET_ERROR_CONTEXT(ctx, Status::PERMISSION_DENIED,
        "User %s lacks %s privilege on %s %s",
        session->username.c_str(),
        privilegeToString(required_privileges).c_str(),
        objectTypeToString(object_type).c_str(),
        objectIdToString(object_id).c_str());
    return Status::PERMISSION_DENIED;
}
```

---

## 6. Roles vs Groups

### Roles: Exclusive Context-Switching

**Concept**: Roles are "hats you wear" - you choose which one to wear at a time.

**Characteristics**:
- User is **member** of multiple roles
- User **activates** ONE role at a time (or none)
- Permissions are **exclusive** to active role
- Similar to Unix `su` or `sudo` (explicit privilege escalation)

**SQL Syntax**:
```sql
-- Create role
CREATE ROLE role_accountant;
CREATE ROLE role_manager;
CREATE ROLE role_auditor;

-- Grant role membership to user
GRANT ROLE role_accountant TO alice;
GRANT ROLE role_manager TO alice;

-- User activates role
SET ROLE role_accountant;   -- Now acting as accountant

-- User switches role
SET ROLE role_manager;      -- Now acting as manager (lost accountant privileges)

-- User returns to default privileges
RESET ROLE;                 -- No role active
```

**Implementation**:
```cpp
Status Executor::executeSetRole(SetRoleStmt* stmt, Session* session) {
    // 1. Validate role exists
    ID role_id;
    Status s = catalog_->getRoleByName(stmt->role_name, &role_id);
    if (s != Status::OK) return Status::INVALID_ROLE;

    // 2. Check user is member of this role
    bool is_member = false;
    for (const ID& available_role : session->available_roles) {
        if (available_role == role_id) {
            is_member = true;
            break;
        }
    }

    if (!is_member && !session->is_superuser) {
        return Status::PERMISSION_DENIED;
    }

    // 3. Activate role (clear privilege cache)
    {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->active_role_id = role_id;
        session->privilege_cache.clear();  // Privileges changed
    }

    LOG_INFO(SECURITY, "User %s activated role %s",
        session->username.c_str(), stmt->role_name.c_str());

    return Status::OK;
}
```

**Use Cases**:
- Separation of duties (accounting vs management)
- Temporary privilege elevation
- Job function isolation
- Audit trails (know which "hat" user was wearing)

### Groups: Cumulative Always-On

**Concept**: Groups are "badges you carry" - all active simultaneously.

**Characteristics**:
- User is **member** of multiple groups
- All groups **always active** (no activation needed)
- Permissions are **cumulative** (union of all groups)
- Can be **nested** (groups can be members of groups)
- Maps to **external authentication** (LDAP/AD groups)

**SQL Syntax**:
```sql
-- Create groups
CREATE GROUP accounting;
CREATE GROUP managers;
CREATE GROUP all_employees;

-- Add user to groups (cumulative)
ALTER USER alice ADD TO GROUP accounting;
ALTER USER alice ADD TO GROUP managers;
ALTER USER alice ADD TO GROUP all_employees;

-- Grant privileges to groups
GRANT SELECT ON TABLE invoices TO GROUP accounting;
GRANT UPDATE ON TABLE budgets TO GROUP managers;
GRANT SELECT ON TABLE directory TO GROUP all_employees;

-- Alice automatically has privileges from ALL groups
-- No SET GROUP command - always active
```

**Group Nesting**:
```sql
-- Groups can contain groups
CREATE GROUP all_employees;
CREATE GROUP engineering;
CREATE GROUP senior_engineers;

ALTER GROUP engineering ADD TO GROUP all_employees;
ALTER GROUP senior_engineers ADD TO GROUP engineering;

-- User transitively inherits privileges
ALTER USER alice ADD TO GROUP senior_engineers;
-- Alice gets privileges from: senior_engineers + engineering + all_employees
```

**Implementation**:
```cpp
// Recursive group membership resolution (with cycle detection)
vector<ID> CatalogManager::resolveGroupMemberships(const ID& user_id) {
    vector<ID> all_groups;
    unordered_set<ID> visited;
    queue<ID> to_process;

    // Start with user's direct group memberships
    vector<ID> direct_groups = getDirectGroupsForUser(user_id);
    for (const ID& group_id : direct_groups) {
        to_process.push(group_id);
    }

    // BFS to find all transitive group memberships
    while (!to_process.empty()) {
        ID group_id = to_process.front();
        to_process.pop();

        // Cycle detection
        if (visited.count(group_id)) continue;
        visited.insert(group_id);

        all_groups.push_back(group_id);

        // Find parent groups (groups this group belongs to)
        vector<ID> parent_groups = getParentGroups(group_id);
        for (const ID& parent_group_id : parent_groups) {
            to_process.push(parent_group_id);
        }
    }

    return all_groups;
}
```

**Use Cases**:
- Department/team membership
- External authentication mapping (AD/LDAP)
- Physical access control
- Cross-functional teams

### Comparison Table

| Aspect | Roles | Groups |
|--------|-------|--------|
| **Activation** | Explicit (SET ROLE) | Automatic (always on) |
| **Multiplicity** | ONE at a time | ALL simultaneously |
| **Privilege Logic** | Exclusive | Cumulative (union) |
| **Analogy** | Wearing a hat | Carrying badges |
| **Nesting** | No | Yes (transitive) |
| **External Auth** | No mapping | Maps to LDAP/AD |
| **Use Case** | Job function isolation | Team membership |
| **SQL Command** | SET ROLE / RESET ROLE | ALTER USER ADD/REMOVE TO GROUP |

---

## 7. Privilege System

### Privilege Bitmask

```cpp
enum Privilege : uint32_t {
    NONE        = 0,
    SELECT      = 1 << 0,   // Read data
    INSERT      = 1 << 1,   // Insert rows
    UPDATE      = 1 << 2,   // Modify rows
    DELETE      = 1 << 3,   // Delete rows
    TRUNCATE    = 1 << 4,   // TRUNCATE TABLE
    REFERENCES  = 1 << 5,   // Create foreign keys
    TRIGGER     = 1 << 6,   // Create triggers
    EXECUTE     = 1 << 7,   // Execute functions/procedures
    USAGE       = 1 << 8,   // Use sequences, domains, types
    CREATE      = 1 << 9,   // Create objects in schema
    CONNECT     = 1 << 10,  // Open database
    TEMPORARY   = 1 << 11,  // Create temp tables

    // Meta-privileges
    ALL         = 0xFFFFFFFF,
    DDL         = CREATE | REFERENCES | TRIGGER,
    DML         = SELECT | INSERT | UPDATE | DELETE
};
```

### Object-Specific Privileges

| Object Type | Applicable Privileges |
|-------------|-----------------------|
| **TABLE** | SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER |
| **VIEW** | SELECT, INSERT, UPDATE, DELETE (if updatable) |
| **SEQUENCE** | USAGE, SELECT, UPDATE |
| **FUNCTION/PROCEDURE** | EXECUTE |
| **SCHEMA** | CREATE, USAGE |
| **DATABASE** | CONNECT, CREATE, TEMPORARY |
| **DOMAIN/TYPE** | USAGE |

### Enforcement Points

```cpp
// SELECT
Status Executor::executeSelect(SelectStmt* stmt, Session* session) {
    for (const Table& table : stmt->from_tables) {
        Status s = catalog_->checkPrivilege(
            session, table.table_id, OBJECT_TABLE, Privilege::SELECT, ctx_
        );
        if (s != Status::OK) return s;
    }
    // ... execute query
}

// INSERT
Status Executor::executeInsert(InsertStmt* stmt, Session* session) {
    Status s = catalog_->checkPrivilege(
        session, stmt->table_id, OBJECT_TABLE, Privilege::INSERT, ctx_
    );
    if (s != Status::OK) return s;
    // ... execute insert
}

// UPDATE
Status Executor::executeUpdate(UpdateStmt* stmt, Session* session) {
    // Need both UPDATE (for SET) and SELECT (for WHERE)
    uint32_t required = Privilege::UPDATE | Privilege::SELECT;
    Status s = catalog_->checkPrivilege(
        session, stmt->table_id, OBJECT_TABLE, required, ctx_
    );
    if (s != Status::OK) return s;
    // ... execute update
}

// DELETE
Status Executor::executeDelete(DeleteStmt* stmt, Session* session) {
    // Need both DELETE and SELECT (for WHERE)
    uint32_t required = Privilege::DELETE | Privilege::SELECT;
    Status s = catalog_->checkPrivilege(
        session, stmt->table_id, OBJECT_TABLE, required, ctx_
    );
    if (s != Status::OK) return s;
    // ... execute delete
}

// DDL (CREATE TABLE)
Status Executor::executeCreateTable(CreateTableStmt* stmt, Session* session) {
    // Need CREATE privilege on schema
    Status s = catalog_->checkPrivilege(
        session, stmt->schema_id, OBJECT_SCHEMA, Privilege::CREATE, ctx_
    );
    if (s != Status::OK) return s;
    // ... execute create
}

// DDL (DROP TABLE)
Status Executor::executeDropTable(DropTableStmt* stmt, Session* session) {
    // Must be owner or superuser
    TableInfo table;
    catalog_->getTable(stmt->table_id, table);

    if (table.owner_id != session->user_id && !session->is_superuser) {
        return Status::PERMISSION_DENIED;
    }

    // If CASCADE, check privileges on all dependents
    if (stmt->cascade) {
        vector<DependencyInfo> dependents;
        catalog_->getDependents(stmt->table_id, dependents);

        for (const DependencyInfo& dep : dependents) {
            // Must be owner of all dependent objects
            ObjectInfo obj;
            catalog_->getObjectInfo(dep.dependent_object_id, dep.dependent_type, obj);
            if (obj.owner_id != session->user_id && !session->is_superuser) {
                return Status::PERMISSION_DENIED;
            }
        }
    }

    // ... execute drop
}
```

---

## 8. GRANT/REVOKE Commands

### Syntax

```sql
-- Grant privileges on objects
GRANT privilege_list ON object_type object_name TO grantee [WITH GRANT OPTION];
GRANT ALL PRIVILEGES ON object_type object_name TO grantee;
GRANT privilege_list ON ALL TABLES IN SCHEMA schema_name TO grantee;

-- Grant role membership
GRANT ROLE role_name TO user_name [WITH ADMIN OPTION];

-- Revoke privileges
REVOKE privilege_list ON object_type object_name FROM grantee [CASCADE];
REVOKE ALL PRIVILEGES ON object_type object_name FROM grantee;

-- Revoke role membership
REVOKE ROLE role_name FROM user_name [CASCADE];
```

### Examples

```sql
-- Grant table privileges
GRANT SELECT, INSERT ON TABLE employees TO alice;
GRANT ALL PRIVILEGES ON TABLE salaries TO GROUP hr;
GRANT UPDATE (salary, bonus) ON TABLE employees TO ROLE payroll_admin;

-- Grant function privileges
GRANT EXECUTE ON FUNCTION calculate_bonus(INT, DECIMAL) TO PUBLIC;

-- Grant schema privileges
GRANT CREATE ON SCHEMA hr TO ROLE hr_admin;
GRANT USAGE ON SCHEMA public TO PUBLIC;

-- Grant with GRANT OPTION (can re-grant to others)
GRANT SELECT ON TABLE employees TO alice WITH GRANT OPTION;

-- Grant role membership
GRANT ROLE db_owner TO alice;
GRANT ROLE read_only TO bob WITH ADMIN OPTION;

-- Revoke privileges
REVOKE INSERT ON TABLE employees FROM alice;
REVOKE ALL PRIVILEGES ON TABLE salaries FROM GROUP hr CASCADE;

-- Revoke role membership
REVOKE ROLE db_owner FROM alice;
```

### AST Nodes

```cpp
struct GrantStmt : public Statement {
    enum GrantType {
        PRIVILEGE,      // GRANT SELECT, INSERT, ...
        ROLE            // GRANT ROLE role_name
    };

    GrantType grant_type;
    vector<Privilege> privileges;   // If grant_type == PRIVILEGE
    ID role_id;                     // If grant_type == ROLE
    ObjectType object_type;
    ID object_id;

    enum GranteeType {
        USER,
        ROLE,
        GROUP,
        PUBLIC
    };
    GranteeType grantee_type;
    ID grantee_id;

    bool with_grant_option;
    bool with_admin_option;         // For role grants

    GrantStmt() : Statement(StatementType::GRANT) {}
};

struct RevokeStmt : public Statement {
    enum RevokeType {
        PRIVILEGE,
        ROLE
    };

    RevokeType revoke_type;
    vector<Privilege> privileges;
    ID role_id;
    ObjectType object_type;
    ID object_id;

    enum GranteeType {
        USER,
        ROLE,
        GROUP,
        PUBLIC
    };
    GranteeType grantee_type;
    ID grantee_id;

    bool cascade;

    RevokeStmt() : Statement(StatementType::REVOKE) {}
};
```

### Implementation

```cpp
Status CatalogManager::grantPrivilege(
    const Session* session,
    uint32_t privileges,
    const ID& object_id,
    ObjectType object_type,
    const ID& grantee_id,
    GranteeType grantee_type,
    bool with_grant_option,
    ErrorContext* ctx
) {
    // 1. Check if grantor has GRANT OPTION
    if (!session->is_superuser) {
        ObjectInfo obj;
        getObjectInfo(object_id, object_type, &obj);

        // Must be owner OR have grant option
        if (obj.owner_id != session->user_id) {
            uint32_t grantor_privileges = getEffectivePrivileges(session, object_id, object_type);
            uint32_t grant_option_privileges = getGrantOptionPrivileges(session, object_id);

            // Check if grantor has grant option for all privileges being granted
            if ((grant_option_privileges & privileges) != privileges) {
                SET_ERROR_CONTEXT(ctx, Status::PERMISSION_DENIED,
                    "Grantor lacks GRANT OPTION");
                return Status::PERMISSION_DENIED;
            }
        }
    }

    // 2. Create PermissionRecord
    PermissionRecord perm;
    memset(&perm, 0, sizeof(PermissionRecord));
    perm.permission_id = generateUuidV7();
    perm.object_id = object_id;
    perm.object_type = static_cast<uint8_t>(object_type);
    perm.grantee_type = static_cast<uint8_t>(grantee_type);
    perm.grantee_id = grantee_id;
    perm.privileges = privileges;
    perm.grant_option = with_grant_option ? 1 : 0;
    perm.grantor_id = session->user_id;
    perm.created_time = getCurrentTime();
    perm.is_valid = 1;

    // 3. Write to disk
    Status s = writeRecordToHeapPage(permissions_table_page_, perm, ctx);
    if (s != Status::OK) return s;

    // 4. Update cache
    {
        std::lock_guard<std::mutex> lock(permissions_cache_mutex_);
        permission_cache_[perm.permission_id] = convertToPermissionInfo(perm);
    }

    // 5. Invalidate privilege caches for affected users
    invalidatePrivilegeCachesForGrantee(grantee_id, grantee_type);

    LOG_INFO(SECURITY, "Granted %s on %s to %s by %s",
        privilegesToString(privileges).c_str(),
        objectIdToString(object_id).c_str(),
        granteeToString(grantee_type, grantee_id).c_str(),
        session->username.c_str());

    return Status::OK;
}
```

---

## 9. SHOW Commands

### Design Philosophy

**No Direct Catalog Access**: Users cannot query `sys.*` schemas directly (except superusers).

**SHOW Commands**: Controlled, permission-filtered metadata discovery.

**Optional Compatibility Views**: Read-only `information_schema` views for tool compatibility.

### Complete SHOW Syntax

```sql
-- Tables
SHOW TABLES [IN schema_name] [LIKE pattern];
SHOW TABLE table_name [IN schema_name];

-- Columns
SHOW COLUMNS IN table_name [IN schema_name] [LIKE pattern];
SHOW COLUMN column_name IN table_name [IN schema_name];

-- Indexes
SHOW INDEXES [IN schema_name] [LIKE pattern];
SHOW INDEXES IN table_name [IN schema_name];
SHOW INDEX index_name [IN schema_name];

-- Views
SHOW VIEWS [IN schema_name] [LIKE pattern];
SHOW VIEW view_name [IN schema_name];

-- Sequences
SHOW SEQUENCES [IN schema_name] [LIKE pattern];
SHOW SEQUENCE sequence_name [IN schema_name];

-- Functions/Procedures
SHOW FUNCTIONS [IN schema_name] [LIKE pattern];
SHOW FUNCTION function_name [IN schema_name];
SHOW PROCEDURES [IN schema_name] [LIKE pattern];
SHOW PROCEDURE procedure_name [IN schema_name];

-- Schemas
SHOW SCHEMAS [LIKE pattern];
SHOW SCHEMA schema_name;

-- Triggers
SHOW TRIGGERS [IN schema_name] [LIKE pattern];
SHOW TRIGGERS IN table_name [IN schema_name];
SHOW TRIGGER trigger_name [IN schema_name];

-- Constraints
SHOW CONSTRAINTS IN table_name [IN schema_name];
SHOW CONSTRAINT constraint_name [IN table_name] [IN schema_name];

-- Types/Domains
SHOW DOMAINS [IN schema_name] [LIKE pattern];
SHOW DOMAIN domain_name [IN schema_name];
SHOW TYPES [IN schema_name] [LIKE pattern];
SHOW TYPE type_name [IN schema_name];

-- Users/Roles/Groups
SHOW USERS [LIKE pattern];              -- Superuser only
SHOW ROLES [LIKE pattern];              -- Own roles + PUBLIC
SHOW GROUPS [LIKE pattern];             -- Own groups

-- Metadata
SHOW COMMENT ON object_type object_name [IN schema_name];
SHOW METADATA FOR object_name [IN schema_name];

-- Dependencies
SHOW DEPENDENCIES OF object_name [IN schema_name];
SHOW DEPENDENTS OF object_name [IN schema_name];

-- Privileges
SHOW PRIVILEGES ON object_name [IN schema_name];
SHOW GRANTS ON object_name [IN schema_name];

-- Database/Session
SHOW DATABASE;
SHOW TABLESPACES;
SHOW CURRENT_USER;
SHOW CURRENT_ROLE;
SHOW CURRENT_SCHEMA;
SHOW SESSION;
SHOW ALL;
```

### Permission Filtering

```cpp
Status CatalogManager::listTablesFiltered(
    const Session* session,
    const ID& schema_id,
    const string& pattern,
    vector<TableInfo>& tables_out,
    ErrorContext* ctx
) {
    // 1. Get all tables in schema
    vector<TableInfo> all_tables;
    Status s = listTables(schema_id, all_tables, ctx);
    if (s != Status::OK) return s;

    // 2. Filter by privileges
    for (const TableInfo& table : all_tables) {
        // User sees table if they have ANY privilege on it
        if (hasAnyPrivilege(session, table.table_id, OBJECT_TABLE)) {
            // Apply LIKE pattern if provided
            if (pattern.empty() || matchesPattern(table.table_name, pattern)) {
                tables_out.push_back(table);
            }
        }
    }

    return Status::OK;
}

bool CatalogManager::hasAnyPrivilege(
    const Session* session,
    const ID& object_id,
    ObjectType object_type
) {
    // Superuser sees everything
    if (session->is_superuser) return true;

    // Owner sees their objects
    ObjectInfo obj_info;
    if (getObjectInfo(object_id, object_type, &obj_info) == Status::OK) {
        if (obj_info.owner_id == session->user_id) return true;
    }

    // Check if user has any privilege at all
    uint32_t granted = getEffectivePrivileges(session, object_id, object_type);
    return granted != 0;
}
```

### SHOW METADATA Implementation

```sql
-- Example: Get all metadata about a table
SHOW METADATA FOR employees IN hr;

-- Returns result set:
-- property           | value
-- -------------------|---------------------------
-- object_type        | TABLE
-- schema             | hr
-- owner              | alice
-- created_time       | 2025-11-09 10:30:00
-- last_modified_time | 2025-11-09 14:22:00
-- row_count          | 15234
-- column_count       | 12
-- has_primary_key    | true
-- has_foreign_keys   | true
-- index_count        | 4
-- trigger_count      | 0
-- tablespace         | main
-- dependencies       | 3
-- dependents         | 7
-- comment            | Employee master table
-- your_privileges    | SELECT, INSERT, UPDATE
-- is_owner           | false
```

```cpp
Status Executor::executeShowMetadata(ShowStmt* stmt, Session* session) {
    // 1. Resolve object
    ID object_id;
    ObjectType object_type;
    Status s = catalog_->resolveObject(
        stmt->object_name,
        stmt->schema_name,
        &object_id,
        &object_type
    );
    if (s != Status::OK) return s;

    // 2. Check privileges
    if (!catalog_->hasAnyPrivilege(session, object_id, object_type)) {
        return Status::PERMISSION_DENIED;
    }

    // 3. Build result set (name-value pairs)
    ResultSet result;
    result.addColumn("property", DataType::VARCHAR);
    result.addColumn("value", DataType::VARCHAR);

    // Object type
    result.addRow({"object_type", objectTypeToString(object_type)});

    // Type-specific metadata
    if (object_type == OBJECT_TABLE) {
        TableInfo table;
        catalog_->getTable(object_id, table);

        result.addRow({"schema", getSchemaName(table.schema_id)});
        result.addRow({"owner", getOwnerName(table.owner_id)});
        result.addRow({"created_time", timestampToString(table.created_time)});
        result.addRow({"row_count", std::to_string(table.row_count)});
        result.addRow({"column_count", std::to_string(table.column_count)});

        // Counts
        int index_count = catalog_->countIndexesForTable(object_id);
        int trigger_count = catalog_->countTriggersForTable(object_id);
        result.addRow({"index_count", std::to_string(index_count)});
        result.addRow({"trigger_count", std::to_string(trigger_count)});

        // Dependencies
        vector<DependencyInfo> deps, dependents;
        catalog_->getDependenciesFor(object_id, deps);
        catalog_->getDependents(object_id, dependents);
        result.addRow({"dependencies", std::to_string(deps.size())});
        result.addRow({"dependents", std::to_string(dependents.size())});

        // User's privileges
        uint32_t privs = catalog_->getEffectivePrivileges(session, object_id, object_type);
        result.addRow({"your_privileges", privilegesToString(privs)});
        result.addRow({"is_owner", (table.owner_id == session->user_id) ? "true" : "false"});

        // Comment
        string comment;
        if (catalog_->getComment(object_id, comment) == Status::OK) {
            result.addRow({"comment", comment});
        }
    }

    session->setResult(result);
    return Status::OK;
}
```

### Blocking Direct Catalog Access

```cpp
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
        Status s = catalog_->checkPrivilege(
            session, table.table_id, OBJECT_TABLE, Privilege::SELECT, ctx_
        );
        if (s != Status::OK) return s;
    }

    // ... execute query
}
```

---

## 10. External Authentication

### Group Mapping

**Concept**: Map external groups (LDAP/AD/Kerberos) to internal database groups.

```sql
-- Create internal groups
CREATE GROUP engineering;
CREATE GROUP senior_engineers;
CREATE GROUP all_employees;

-- Map external groups to internal groups
CREATE GROUP MAPPING
    EXTERNAL 'cn=engineers,ou=groups,dc=company,dc=com'
    FROM LDAP
    TO INTERNAL GROUP engineering;

CREATE GROUP MAPPING
    EXTERNAL 'DOMAIN\Domain Admins'
    FROM KERBEROS
    TO INTERNAL GROUP system_admins;

CREATE GROUP MAPPING
    EXTERNAL 'S-1-5-21-1234567890-1234567890-1234567890-1001'
    FROM AD
    TO INTERNAL GROUP hr;

-- View mappings
SELECT * FROM information_schema.group_mappings;  -- If exposed

-- Remove mapping
DROP GROUP MAPPING EXTERNAL 'DOMAIN\Old Group' FROM KERBEROS;
```

### Catalog Structure

```cpp
struct GroupMappingRecord {
    ID mapping_id;                  // UUID v7
    char external_group_name[512];  // LDAP DN, Kerberos principal, AD SID
    uint8_t auth_method;            // LDAP=1, KERBEROS=2, AD=3
    ID internal_group_id;           // Maps to GroupRecord
    uint8_t auto_create_users;      // Auto-create users on first login?
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Authentication Flow

```
User Logs In
     ↓
[1] External Authenticator (LDAP/Kerberos/AD)
     ↓ (username, password)
     ↓
[2] Get External Groups
     ↓ (list of external group names/DNs)
     ↓
[3] Map External Groups → Internal Groups
     ↓ (using GroupMappingRecord)
     ↓
[4] Resolve Transitive Memberships
     ↓ (groups in groups)
     ↓
[5] Create Session
     ↓ (user_id, group_ids, auth_time)
     ↓
Session Ready (with all group privileges)
```

### Periodic Revalidation

**Problem**: User removed from AD group, but session still has privileges.

**Solution**: Periodically re-query external authenticator.

```cpp
struct SecurityConfig {
    uint32_t group_revalidation_interval_seconds = 3600;  // 1 hour default
    bool terminate_session_on_revalidation_failure = false;
};

Status Session::validateGroupMemberships() {
    // Check if revalidation is needed
    uint64_t now = getCurrentTime();
    uint64_t elapsed = now - this->external_auth_time;

    if (elapsed < config_.group_revalidation_interval_seconds) {
        return Status::OK;  // Still valid
    }

    // Re-query external authenticator
    vector<string> current_external_groups;
    Status s = external_auth_->getGroupMemberships(
        this->username,
        this->auth_method,
        &current_external_groups
    );

    if (s != Status::OK) {
        LOG_WARNING(SECURITY, "Group revalidation failed for user %s", username.c_str());

        if (config_.terminate_session_on_revalidation_failure) {
            this->is_valid = false;
            return Status::SESSION_INVALID;
        } else {
            // Allow session to continue with stale groups
            return Status::OK;
        }
    }

    // Map external groups to internal groups
    vector<ID> new_group_ids = catalog_->mapAndResolveGroups(
        current_external_groups,
        this->auth_method
    );

    // Atomic update
    {
        std::lock_guard<std::mutex> lock(this->mutex);
        this->group_ids = new_group_ids;
        this->privilege_cache.clear();  // Invalidate cache
        this->external_auth_time = now;
    }

    LOG_INFO(SECURITY, "Revalidated group memberships for user %s", username.c_str());
    return Status::OK;
}
```

### External Auth Interface

```cpp
class ExternalAuthenticator {
public:
    virtual ~ExternalAuthenticator() = default;

    // Authenticate user
    virtual Status authenticate(
        const string& username,
        const string& password,
        string& external_user_id,
        vector<string>& group_memberships,
        ErrorContext* ctx
    ) = 0;

    // Get current group memberships (for revalidation)
    virtual Status getGroupMemberships(
        const string& username,
        vector<string>& group_memberships,
        ErrorContext* ctx
    ) = 0;
};

class LDAPAuthenticator : public ExternalAuthenticator {
    string ldap_server_;
    uint16_t ldap_port_;
    string base_dn_;
    string bind_dn_;
    string bind_password_;

public:
    Status authenticate(...) override {
        // 1. Bind to LDAP server
        // 2. Search for user DN
        // 3. Verify password
        // 4. Get memberOf attributes (groups)
        // 5. Return user DN + group DNs
    }

    Status getGroupMemberships(...) override {
        // 1. Bind to LDAP server
        // 2. Search for user DN
        // 3. Get memberOf attributes
        // 4. Return group DNs
    }
};

class KerberosAuthenticator : public ExternalAuthenticator {
    string realm_;
    string kdc_server_;

public:
    Status authenticate(...) override {
        // 1. Obtain Kerberos TGT
        // 2. Verify ticket
        // 3. Get PAC (Privilege Attribute Certificate) for groups
        // 4. Return principal + group principals
    }
};

class ActiveDirectoryAuthenticator : public ExternalAuthenticator {
    string domain_controller_;

public:
    Status authenticate(...) override {
        // 1. Authenticate with AD
        // 2. Get user SID
        // 3. Get group SIDs (primary + supplemental)
        // 4. Return SID + group SIDs
    }
};
```

---

## 11. Catalog Integration

### Existing Catalog Tables (Already Defined)

**From**: `/docs/IMPLEMENTATION_AUDIT.md`

```cpp
// 13. Users (sys.sec.users)
struct UserRecord {
    ID user_id;                     // UUID v7
    char username[512];
    uint32_t password_hash_oid;     // TOAST reference (bcrypt/argon2)
    uint32_t user_metadata_oid;     // TOAST reference (JSON)
    ID default_schema_id;
    uint8_t is_active;
    uint8_t is_superuser;
    uint8_t reserved[6];
    uint64_t created_time;
    uint64_t last_login_time;
    uint32_t is_valid;
    uint32_t padding;
};

// 14. Roles (sys.sec.roles)
struct RoleRecord {
    ID role_id;                     // UUID v7
    char role_name[512];
    ID owner_id;                    // Owner UUID reference
    uint32_t role_metadata_oid;     // TOAST reference (JSON)
    uint8_t is_active;
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};

// 15. Groups (sys.sec.groups)
struct GroupRecord {
    ID group_id;                    // UUID v7
    char group_name[512];
    char external_id[512];          // LDAP DN, Kerberos principal, AD SID
    uint8_t group_type;             // LOCAL=0, LDAP=1, KERBEROS=2, AD=3
    uint8_t reserved[7];
    uint32_t group_metadata_oid;    // TOAST reference (JSON)
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};

// 16. RoleMemberships (sys.sec.roles)
struct RoleMembershipRecord {
    ID membership_id;               // UUID v7
    ID user_id;
    ID role_id;
    ID granted_by;
    uint8_t with_admin_option;
    uint8_t reserved[7];
    uint64_t granted_time;
    uint32_t is_valid;
    uint32_t padding;
};

// 28. Permissions (infrastructure)
struct PermissionRecord {
    ID permission_id;               // UUID v7
    ID object_id;
    char grantee[128];              // DEPRECATED - use grantee_id
    uint8_t object_type;
    uint8_t reserved[3];
    uint32_t privileges;
    uint8_t grant_option;
    uint8_t reserved2[3];
    char grantor[128];              // DEPRECATED - use grantor_id
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Required Changes to PermissionRecord

```cpp
// Updated PermissionRecord structure
struct PermissionRecord {
    ID permission_id;               // UUID v7
    ID object_id;                   // Object being protected
    uint8_t object_type;            // ObjectType enum

    // Grantee (who receives privileges)
    ID grantee_id;                  // User, Role, or Group UUID
    uint8_t grantee_type;           // USER=0, ROLE=1, GROUP=2, PUBLIC=3

    // Privileges
    uint32_t privileges;            // Bitmask of Privilege enum
    uint8_t grant_option;           // Can re-grant to others?

    // Grantor (who granted privileges)
    ID grantor_id;                  // User UUID of grantor

    uint8_t reserved[6];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### New Catalog Tables Needed

#### GroupMemberships (sys.sec.groups)

```cpp
struct GroupMembershipRecord {
    ID membership_id;               // UUID v7
    ID user_id;                     // User or Group (for nesting)
    uint8_t member_type;            // USER=0, GROUP=1
    ID group_id;                    // Parent group
    ID granted_by;                  // Who added member
    uint64_t granted_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

#### GroupMappings (sys.sec.groups)

```cpp
struct GroupMappingRecord {
    ID mapping_id;                  // UUID v7
    char external_group_name[512];  // LDAP DN, Kerberos principal, AD SID
    uint8_t auth_method;            // LDAP=1, KERBEROS=2, AD=3
    ID internal_group_id;           // Maps to GroupRecord
    uint8_t auto_create_users;      // Auto-create users on first login?
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### CRUD Operations Required

```cpp
// Users
Status createUser(const string& username, const string& password, bool is_superuser, ID& user_id, ErrorContext* ctx);
Status dropUser(const ID& user_id, bool cascade, ErrorContext* ctx);
Status alterUser(const ID& user_id, const string& new_password, const ID& new_default_schema, ErrorContext* ctx);
Status getUserByName(const string& username, UserRecord& user, ErrorContext* ctx);
Status listUsers(vector<UserInfo>& users, ErrorContext* ctx);

// Roles
Status createRole(const string& role_name, ID& role_id, ErrorContext* ctx);
Status dropRole(const ID& role_id, bool cascade, ErrorContext* ctx);
Status getRoleByName(const string& role_name, ID& role_id, ErrorContext* ctx);
Status listRoles(vector<RoleInfo>& roles, ErrorContext* ctx);

// Role Memberships
Status grantRoleToUser(const ID& role_id, const ID& user_id, bool with_admin_option, const ID& grantor_id, ErrorContext* ctx);
Status revokeRoleFromUser(const ID& role_id, const ID& user_id, bool cascade, ErrorContext* ctx);
Status getRoleMemberships(const ID& user_id, vector<ID>& role_ids, ErrorContext* ctx);
Status getUsersInRole(const ID& role_id, vector<ID>& user_ids, ErrorContext* ctx);

// Groups
Status createGroup(const string& group_name, GroupType type, const string& external_id, ID& group_id, ErrorContext* ctx);
Status dropGroup(const ID& group_id, bool cascade, ErrorContext* ctx);
Status getGroupByName(const string& group_name, ID& group_id, ErrorContext* ctx);
Status listGroups(vector<GroupInfo>& groups, ErrorContext* ctx);

// Group Memberships
Status addUserToGroup(const ID& user_id, const ID& group_id, const ID& grantor_id, ErrorContext* ctx);
Status removeUserFromGroup(const ID& user_id, const ID& group_id, ErrorContext* ctx);
Status addGroupToGroup(const ID& child_group_id, const ID& parent_group_id, const ID& grantor_id, ErrorContext* ctx);  // Nesting
Status removeGroupFromGroup(const ID& child_group_id, const ID& parent_group_id, ErrorContext* ctx);
Status getDirectGroupsForUser(const ID& user_id, vector<ID>& group_ids, ErrorContext* ctx);
Status resolveGroupMemberships(const ID& user_id, vector<ID>& all_group_ids, ErrorContext* ctx);  // Transitive
Status getUsersInGroup(const ID& group_id, bool recursive, vector<ID>& user_ids, ErrorContext* ctx);

// Group Mappings (External Auth)
Status createGroupMapping(const string& external_group, AuthMethod auth_method, const ID& internal_group_id, ErrorContext* ctx);
Status dropGroupMapping(const ID& mapping_id, ErrorContext* ctx);
Status mapExternalGroup(const string& external_group, AuthMethod auth_method, ID& internal_group_id, ErrorContext* ctx);
Status listGroupMappings(vector<GroupMappingInfo>& mappings, ErrorContext* ctx);

// Permissions
Status grantPrivilege(const Session* session, uint32_t privileges, const ID& object_id, ObjectType object_type, const ID& grantee_id, GranteeType grantee_type, bool with_grant_option, ErrorContext* ctx);
Status revokePrivilege(const Session* session, uint32_t privileges, const ID& object_id, ObjectType object_type, const ID& grantee_id, GranteeType grantee_type, bool cascade, ErrorContext* ctx);
Status getPrivilegesForUser(const ID& user_id, const ID& object_id, uint32_t& privileges, ErrorContext* ctx);
Status getPrivilegesForRole(const ID& role_id, const ID& object_id, uint32_t& privileges, ErrorContext* ctx);
Status getPrivilegesForGroup(const ID& group_id, const ID& object_id, uint32_t& privileges, ErrorContext* ctx);
Status getEffectivePrivileges(const Session* session, const ID& object_id, ObjectType object_type, uint32_t& privileges, ErrorContext* ctx);
Status listPermissionsForObject(const ID& object_id, vector<PermissionInfo>& permissions, ErrorContext* ctx);
```

---

## 12. Implementation Details

### Bootstrap Security

```cpp
// During database initialization
Status CatalogManager::initializeSecurity(ErrorContext* ctx) {
    // 1. Create SYSTEM user (owner of all system objects)
    ID system_user_id = generateUuidV7();  // Per-database UUIDv7
    UserRecord system_user;
    memset(&system_user, 0, sizeof(UserRecord));
    system_user.user_id = system_user_id;
    strcpy(system_user.username, "SYSTEM");
    system_user.is_superuser = 1;
    system_user.is_active = 1;
    system_user.created_time = getCurrentTime();
    system_user.is_valid = 1;
    writeRecordToHeapPage(users_table_page_, system_user, ctx);

    // 2. Create PUBLIC role (all users implicit members)
    ID public_role_id = generateUuidV7();
    RoleRecord public_role;
    memset(&public_role, 0, sizeof(RoleRecord));
    public_role.role_id = public_role_id;
    strcpy(public_role.role_name, "PUBLIC");
    public_role.owner_id = system_user_id;
    public_role.is_active = 1;
    public_role.created_time = getCurrentTime();
    public_role.is_valid = 1;
    writeRecordToHeapPage(roles_table_page_, public_role, ctx);

    // 3. Create DB_OWNER role
    ID db_owner_role_id = generateUuidV7();
    RoleRecord db_owner_role;
    memset(&db_owner_role, 0, sizeof(RoleRecord));
    db_owner_role.role_id = db_owner_role_id;
    strcpy(db_owner_role.role_name, "DB_OWNER");
    db_owner_role.owner_id = system_user_id;
    db_owner_role.is_active = 1;
    db_owner_role.created_time = getCurrentTime();
    db_owner_role.is_valid = 1;
    writeRecordToHeapPage(roles_table_page_, db_owner_role, ctx);

    // 4. Grant CONNECT to PUBLIC
    grantPrivilege(nullptr, Privilege::CONNECT, database_id_, OBJECT_DATABASE,
                   public_role_id, GRANTEE_ROLE, false, ctx);

    // 5. Grant USAGE on PUBLIC schema to PUBLIC
    ID public_schema_id;
    getSchemaByName("public", public_schema_id);
    grantPrivilege(nullptr, Privilege::USAGE, public_schema_id, OBJECT_SCHEMA,
                   public_role_id, GRANTEE_ROLE, false, ctx);

    return Status::OK;
}
```

### Session Creation

```cpp
Status Database::createSession(
    const ID& user_id,
    const string& username,
    const string& auth_method,
    Session** session_out,
    ErrorContext* ctx
) {
    UserRecord user;
    Status s = catalog_->getUser(user_id, &user);
    if (s != Status::OK) return s;

    if (!user.is_active) {
        SET_ERROR_CONTEXT(ctx, Status::USER_INACTIVE, "User %s is inactive", username.c_str());
        return Status::USER_INACTIVE;
    }

    Session* session = new Session();
    session->user_id = user_id;
    session->username = username;
    session->is_superuser = user.is_superuser;
    session->default_schema_id = user.default_schema_id;
    session->current_schema_id = user.default_schema_id;
    session->auth_method = auth_method;
    session->session_start_time = getCurrentTime();
    session->last_activity_time = getCurrentTime();

    // Load role memberships
    catalog_->getRoleMemberships(user_id, session->available_roles);
    session->active_role_id = INVALID_ID;  // No role active initially

    // Load group memberships (transitive)
    session->group_ids = catalog_->resolveGroupMemberships(user_id);

    // Update last login time
    catalog_->updateUserLastLogin(user_id, getCurrentTime());

    *session_out = session;
    return Status::OK;
}
```

### Privilege Cache Invalidation

```cpp
void CatalogManager::invalidatePrivilegeCachesForGrantee(
    const ID& grantee_id,
    GranteeType grantee_type
) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    for (Session* session : active_sessions_) {
        bool should_invalidate = false;

        if (grantee_type == GRANTEE_USER && session->user_id == grantee_id) {
            should_invalidate = true;
        }
        else if (grantee_type == GRANTEE_ROLE) {
            if (session->active_role_id == grantee_id) {
                should_invalidate = true;
            }
        }
        else if (grantee_type == GRANTEE_GROUP) {
            for (const ID& group_id : session->group_ids) {
                if (group_id == grantee_id) {
                    should_invalidate = true;
                    break;
                }
            }
        }
        else if (grantee_type == GRANTEE_PUBLIC) {
            should_invalidate = true;  // Everyone affected
        }

        if (should_invalidate) {
            std::lock_guard<std::mutex> cache_lock(session->cache_mutex);
            session->privilege_cache.clear();
        }
    }
}
```

---

## 13. Security Enforcement

### MGA Integration

**Key Principle**: Security checks happen **before** MGA visibility checks, not during.

```cpp
// WRONG: Checking privileges during tuple visibility
bool HeapPage::isVisible(TID tid, Session* session) {
    // DO NOT check privileges here
    return isVersionVisible(xmin, session->xid);
}

// CORRECT: Check privileges before starting scan
Status Executor::executeSelect(SelectStmt* stmt, Session* session) {
    // 1. Check privileges ONCE at statement start
    for (const Table& table : stmt->from_tables) {
        Status s = catalog_->checkPrivilege(
            session, table.table_id, OBJECT_TABLE, Privilege::SELECT, ctx_
        );
        if (s != Status::OK) return s;
    }

    // 2. Execute scan with MGA visibility (no security checks)
    for (TID tid : heap_scan) {
        if (isVersionVisible(tid.xmin, session->xid)) {
            // Process tuple
        }
    }

    return Status::OK;
}
```

**Rationale**: MGA visibility is about **transaction consistency**, security is about **authorization**. Keep them separate.

### Preventing Privilege Escalation

```cpp
// Example: User tries to ALTER TABLE to change owner
Status Executor::executeAlterTable(AlterTableStmt* stmt, Session* session) {
    TableInfo table;
    catalog_->getTable(stmt->table_id, table);

    // If changing owner, check if user can grant ownership
    if (stmt->new_owner_id != INVALID_ID) {
        // Must be current owner OR superuser
        if (table.owner_id != session->user_id && !session->is_superuser) {
            return Status::PERMISSION_DENIED;
        }

        // Cannot grant ownership to yourself (already owner)
        if (stmt->new_owner_id == session->user_id) {
            return Status::INVALID_OPERATION;
        }
    }

    // ... proceed with alter
}
```

### Audit Logging

```cpp
struct AuditLogEntry {
    uint64_t timestamp;
    ID user_id;
    string username;
    ID role_id;                     // Active role at time of action
    string operation;               // SELECT, INSERT, GRANT, etc.
    ObjectType object_type;
    ID object_id;
    string object_name;
    uint32_t privileges_used;
    bool success;
    string error_message;           // If failed
    string client_info;             // IP, application, etc. (for future)
};

void CatalogManager::auditLog(
    const Session* session,
    const string& operation,
    ObjectType object_type,
    const ID& object_id,
    bool success,
    const string& error_message
) {
    AuditLogEntry entry;
    entry.timestamp = getCurrentTime();
    entry.user_id = session->user_id;
    entry.username = session->username;
    entry.role_id = session->active_role_id;
    entry.operation = operation;
    entry.object_type = object_type;
    entry.object_id = object_id;
    entry.success = success;
    entry.error_message = error_message;

    // Write to audit log (could be separate file, table, or syslog)
    writeAuditLog(entry);
}
```

---

## 14. Configuration

### SecurityConfig Structure

```cpp
struct SecurityConfig {
    // Password policy
    uint32_t min_password_length = 8;
    bool require_uppercase = true;
    bool require_lowercase = true;
    bool require_digit = true;
    bool require_special_char = true;
    uint32_t password_expiry_days = 90;
    uint32_t password_history_count = 5;
    uint32_t max_failed_login_attempts = 5;
    uint32_t account_lockout_duration_minutes = 30;

    // External authentication
    bool allow_ldap_auth = true;
    bool allow_kerberos_auth = true;
    bool allow_ad_auth = true;
    bool auto_create_external_users = true;
    bool auto_map_external_groups = true;
    uint32_t group_revalidation_interval_seconds = 3600;  // 1 hour
    bool terminate_session_on_revalidation_failure = false;

    // Session management
    uint32_t max_concurrent_sessions_per_user = 10;
    uint32_t session_idle_timeout_seconds = 3600;  // 1 hour
    uint32_t session_absolute_timeout_seconds = 28800;  // 8 hours

    // Privilege caching
    bool enable_privilege_cache = true;
    uint32_t privilege_cache_ttl_seconds = 300;  // 5 minutes

    // Audit logging
    bool enable_audit_logging = true;
    bool audit_select_statements = false;  // Can be very verbose
    bool audit_failed_attempts = true;
    string audit_log_path = "audit.log";

    // Security features
    bool allow_direct_catalog_access = false;  // Only for superuser
    bool enable_row_level_security = false;    // Phase 3 feature
    bool enable_column_level_privileges = false;  // Phase 2 feature
};
```

---

## 15. Examples

### Example 1: Role-Based Separation of Duties

```sql
-- Setup
CREATE ROLE role_accountant;
CREATE ROLE role_manager;
CREATE ROLE role_auditor;

-- Grant privileges to roles
GRANT SELECT, INSERT ON TABLE invoices TO ROLE role_accountant;
GRANT UPDATE ON TABLE invoices TO ROLE role_manager;
GRANT SELECT ON ALL TABLES IN SCHEMA accounting TO ROLE role_auditor;

-- Grant role memberships to user
GRANT ROLE role_accountant TO alice;
GRANT ROLE role_manager TO alice;
GRANT ROLE role_auditor TO alice;

-- Alice logs in (no role active)
-- She has NO privileges yet

-- Alice activates accountant role
SET ROLE role_accountant;
-- Now she can: SELECT, INSERT on invoices
-- But NOT: UPDATE on invoices (that's manager role)

SELECT * FROM invoices WHERE amount > 1000;  -- ✅ Works
INSERT INTO invoices VALUES (...);            -- ✅ Works
UPDATE invoices SET approved = true;          -- ❌ Permission denied

-- Alice switches to manager role
SET ROLE role_manager;
-- Now she can: UPDATE on invoices
-- But NOT: INSERT (lost accountant privileges)

UPDATE invoices SET approved = true WHERE id = 123;  -- ✅ Works
INSERT INTO invoices VALUES (...);                   -- ❌ Permission denied

-- Alice switches to auditor role
SET ROLE role_auditor;
-- Now she can: SELECT on all accounting tables
-- But NOT: any modifications

SELECT * FROM invoices;     -- ✅ Works
SELECT * FROM expenses;     -- ✅ Works
UPDATE invoices SET ...;    -- ❌ Permission denied
```

### Example 2: Group-Based Team Access

```sql
-- Setup
CREATE GROUP accounting;
CREATE GROUP engineering;
CREATE GROUP all_employees;

-- Add users to groups (cumulative)
ALTER USER alice ADD TO GROUP accounting;
ALTER USER alice ADD TO GROUP all_employees;
ALTER USER bob ADD TO GROUP engineering;
ALTER USER bob ADD TO GROUP all_employees;

-- Grant privileges to groups
GRANT SELECT, INSERT, UPDATE ON TABLE invoices TO GROUP accounting;
GRANT SELECT, INSERT ON TABLE timesheets TO GROUP engineering;
GRANT SELECT ON TABLE company_directory TO GROUP all_employees;

-- Alice logs in (no role active)
-- She automatically has privileges from BOTH groups:
SELECT * FROM invoices;          -- ✅ Works (from accounting group)
INSERT INTO invoices VALUES ...; -- ✅ Works (from accounting group)
SELECT * FROM company_directory; -- ✅ Works (from all_employees group)
INSERT INTO timesheets VALUES ...;  -- ❌ Permission denied (not in engineering)

-- Bob logs in
SELECT * FROM timesheets;        -- ✅ Works (from engineering group)
SELECT * FROM company_directory; -- ✅ Works (from all_employees group)
SELECT * FROM invoices;          -- ❌ Permission denied (not in accounting)
```

### Example 3: External Authentication with Group Mapping

```sql
-- Create internal groups
CREATE GROUP db_admins;
CREATE GROUP developers;
CREATE GROUP analysts;

-- Map LDAP groups to internal groups
CREATE GROUP MAPPING
    EXTERNAL 'cn=database_admins,ou=groups,dc=company,dc=com'
    FROM LDAP
    TO INTERNAL GROUP db_admins;

CREATE GROUP MAPPING
    EXTERNAL 'cn=developers,ou=groups,dc=company,dc=com'
    FROM LDAP
    TO INTERNAL GROUP developers;

-- Grant privileges to internal groups
GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA hr TO GROUP db_admins;
GRANT SELECT ON TABLE employees TO GROUP developers;
GRANT SELECT ON TABLE salaries TO GROUP analysts;

-- User "alice" logs in via LDAP
-- LDAP reports she's in: database_admins, developers
-- System maps: database_admins → db_admins, developers → developers
-- Alice gets privileges from BOTH groups automatically

SELECT * FROM hr.employees;  -- ✅ Works (from both groups)
DELETE FROM hr.employees;    -- ✅ Works (from db_admins group)

-- Later, Alice is removed from database_admins in LDAP
-- After revalidation interval (1 hour), her session is updated
-- She loses db_admins privileges but keeps developers privileges
```

### Example 4: SHOW Commands with Permission Filtering

```sql
-- Alice has SELECT on employees, invoices
-- Alice has NO privileges on salaries

SHOW TABLES IN hr;
-- Returns:
--   table_name | owner | row_count
--   employees  | bob   | 1523
--   invoices   | bob   | 9841
--   (salaries not shown - no privileges)

SHOW COLUMNS IN salaries IN hr;
-- Error: No privileges on table salaries

SHOW METADATA FOR employees IN hr;
-- Returns:
--   property         | value
--   object_type      | TABLE
--   schema           | hr
--   owner            | bob
--   row_count        | 1523
--   your_privileges  | SELECT
--   is_owner         | false

-- Bob is superuser
SHOW TABLES IN hr;
-- Returns ALL tables (including salaries)

-- System catalog is protected
SELECT * FROM sys.sec.users;
-- Error: Direct access to system catalog forbidden. Use SHOW commands.

SHOW USERS;  -- Only superuser can run this
```

---

## 16. Implementation Phases

### Phase 1: Core Infrastructure (200-300 hours) - ALPHA

1. **Catalog CRUD** (80-120 hours)
   - Implement Users CRUD operations
   - Implement Roles CRUD operations
   - Implement Groups CRUD operations (without nesting)
   - Implement RoleMemberships CRUD
   - Implement GroupMemberships CRUD
   - Update PermissionRecord structure
   - Implement Permissions CRUD

2. **Session Management** (40-60 hours)
   - Implement Session structure
   - Implement authentication (internal only)
   - Implement `checkPrivilege()` method
   - Implement privilege caching
   - Add Session parameter to all Executor methods

3. **GRANT/REVOKE** (40-60 hours)
   - Parse GRANT/REVOKE statements
   - Generate bytecode opcodes
   - Implement executor operations
   - Implement privilege resolution

4. **Basic Enforcement** (40-60 hours)
   - Add privilege checks to SELECT/INSERT/UPDATE/DELETE
   - Add privilege checks to CREATE/DROP TABLE
   - Block direct sys.* access
   - Implement bootstrap security

### Phase 2: SHOW Commands & Enforcement (150-250 hours) - ALPHA

1. **SHOW Commands** (80-120 hours)
   - Implement SHOW TABLES/COLUMNS/INDEXES
   - Implement SHOW CURRENT_USER/ROLE/SCHEMA
   - Implement permission filtering
   - Implement LIKE pattern matching
   - Implement SHOW METADATA

2. **Extended Enforcement** (70-130 hours)
   - Add privilege checks to all DDL operations
   - Implement CASCADE logic for REVOKE
   - Add EXECUTE privilege for functions
   - Implement USAGE privilege for sequences

### Phase 3: Advanced Features (150-250 hours) - BETA

1. **External Authentication** (100-150 hours)
   - LDAP authenticator
   - Kerberos authenticator (optional)
   - Active Directory authenticator (optional)
   - Group mapping
   - Periodic revalidation

2. **Group Nesting** (30-50 hours)
   - Implement transitive group resolution
   - Cycle detection
   - Update privilege calculation

3. **Additional Features** (20-50 hours)
   - Column-level privileges
   - DEFAULT PRIVILEGES
   - Audit logging

### Phase 4: Optional Features (100-150 hours) - Future

1. **Row-Level Security** (60-90 hours)
   - RLS policies
   - Policy enforcement in WHERE clauses
   - Performance optimization

2. **Compatibility Views** (40-60 hours)
   - information_schema.tables
   - information_schema.columns
   - information_schema.table_privileges
   - PostgreSQL pg_catalog views

**Total Estimate**: 600-900 hours

---

## 17. Testing Strategy

### Unit Tests

- Test privilege resolution algorithm
- Test group nesting resolution
- Test pattern matching (LIKE)
- Test privilege cache invalidation

### Integration Tests

- Test GRANT/REVOKE with various combinations
- Test SET ROLE / RESET ROLE
- Test external authentication flow
- Test group revalidation
- Test CASCADE operations

### Security Tests

- Test privilege escalation attempts
- Test SQL injection in SHOW commands
- Test concurrent privilege changes
- Test session hijacking prevention

### Performance Tests

- Benchmark privilege checks (with/without cache)
- Benchmark SHOW commands with large catalogs
- Benchmark external auth revalidation
- Stress test with many concurrent sessions

---

## Appendices

### A. Privilege Matrix

| Operation | Required Privileges |
|-----------|---------------------|
| SELECT | SELECT |
| INSERT | INSERT |
| UPDATE | UPDATE + SELECT (for WHERE) |
| DELETE | DELETE + SELECT (for WHERE) |
| TRUNCATE | TRUNCATE (or owner) |
| CREATE TABLE | CREATE on schema |
| DROP TABLE | Owner or superuser |
| CREATE INDEX | Owner or superuser |
| DROP INDEX | Owner or superuser |
| ALTER TABLE | Owner or superuser |
| CREATE VIEW | CREATE on schema + SELECT on base tables |
| CREATE FUNCTION | CREATE on schema |
| EXECUTE function | EXECUTE |
| CREATE SEQUENCE | CREATE on schema |
| sequence.NEXTVAL | USAGE |

### B. Bootstrap Objects

| Object | UUID | Description |
|--------|------|-------------|
| SYSTEM user | Generated UUIDv7 | Owner of all system objects |
| PUBLIC role | Generated | All users implicit members |
| DB_OWNER role | Generated | First user becomes DB_OWNER |

### C. Default Grants

| Grantee | Privilege | Object |
|---------|-----------|--------|
| PUBLIC | CONNECT | DATABASE |
| PUBLIC | USAGE | PUBLIC schema |

---

**End of Specification**
