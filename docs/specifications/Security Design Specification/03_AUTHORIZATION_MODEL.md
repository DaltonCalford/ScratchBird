# ScratchBird Authorization Model Specification

**Document ID**: SBSEC-03  
**Version**: 1.0  
**Status**: Draft for Review  
**Date**: January 2026  
**Scope**: All deployment modes, Security Levels 1-6  

---

## 1. Introduction

### 1.1 Purpose

This document defines the authorization model for the ScratchBird database engine. It specifies how permissions are granted, evaluated, and enforced across all database objects and operations.

### 1.2 Scope

This specification covers:
- Role-Based Access Control (RBAC)
- Group-Based Access Control (GBAC)
- Object-level permissions (ACLs)
- Privilege taxonomy
- GRANT/REVOKE semantics
- Row-Level Security (RLS)
- Column-Level Security (CLS)
- Namespace and catalog security

### 1.3 Related Documents

| Document ID | Title |
|-------------|-------|
| SBSEC-01 | Security Architecture |
| SBSEC-02 | Identity and Authentication |
| SBSEC-04 | Encryption and Key Management |

### 1.4 Security Level Applicability

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Basic RBAC | | ● | ● | ● | ● | ● | ● |
| Groups (GBAC) | | ○ | ○ | ● | ● | ● | ● |
| Object ACLs | | ● | ● | ● | ● | ● | ● |
| Row-Level Security | | | | ● | ● | ● | ● |
| Column-Level Security | | | | ● | ● | ● | ● |
| Catalog protection | | ● | ● | ● | ● | ● | ● |
| Quorum for grants | | | | | | | ● |

● = Required | ○ = Optional | (blank) = Not applicable

---

## 2. Authorization Model Overview

### 2.1 Three-Layer Permission Model

ScratchBird implements a three-layer permission model combining RBAC, GBAC, and object-level ACLs:

```
┌─────────────────────────────────────────────────────────────────┐
│                 THREE-LAYER PERMISSION MODEL                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ Layer 1: User Direct Privileges                           │  │
│  │                                                            │  │
│  │  • Privileges granted directly to user UUID               │  │
│  │  • Always active                                          │  │
│  │  • Highest priority                                       │  │
│  └───────────────────────────────────────────────────────────┘  │
│                              │                                   │
│                              ▼                                   │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ Layer 2: Role Privileges (Exclusive)                      │  │
│  │                                                            │  │
│  │  • User must SET ROLE to activate                         │  │
│  │  • Only ONE role active at a time                         │  │
│  │  • Context-switching (like sudo)                          │  │
│  │  • Role change requires transaction boundary              │  │
│  └───────────────────────────────────────────────────────────┘  │
│                              │                                   │
│                              ▼                                   │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ Layer 3: Group Privileges (Cumulative)                    │  │
│  │                                                            │  │
│  │  • Always active (all groups simultaneously)              │  │
│  │  • Automatic privilege union                              │  │
│  │  • Can be nested (groups in groups)                       │  │
│  │  • Maps to external auth (LDAP/AD)                        │  │
│  └───────────────────────────────────────────────────────────┘  │
│                              │                                   │
│                              ▼                                   │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ Layer 4: PUBLIC Privileges (Implicit)                     │  │
│  │                                                            │  │
│  │  • All users are implicit members                         │  │
│  │  • Default privileges for all authenticated users         │  │
│  │  • Lowest priority                                        │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Privilege Resolution Algorithm

```cpp
PrivilegeSet resolve_privileges(Session* session, ObjectUUID object) {
    PrivilegeSet effective;
    
    // 1. Check superuser
    if (session->is_superuser) {
        return ALL_PRIVILEGES;
    }
    
    // 2. Check object ownership
    if (is_owner(session->user_uuid, object)) {
        return ALL_PRIVILEGES;
    }
    
    // 3. User direct privileges
    effective |= get_direct_privileges(session->user_uuid, object);
    
    // 4. Active role privileges (only one role)
    if (session->active_role_uuid != INVALID_UUID) {
        effective |= get_role_privileges(session->active_role_uuid, object);
    }
    
    // 5. All group privileges (cumulative)
    for (auto& group_uuid : session->group_uuids) {
        effective |= get_group_privileges(group_uuid, object);
    }
    
    // 6. PUBLIC privileges
    effective |= get_public_privileges(object);
    
    return effective;
}
```

### 2.3 Security Context

The Security Context captures all authorization-relevant information for a transaction:

```sql
CREATE TYPE sys.security_context AS (
    -- Identity
    session_uuid        UUID,
    user_uuid           UUID,
    authkey_uuid        UUID,
    
    -- Effective privileges
    active_role_uuid    UUID,           -- Currently active role (or NULL)
    group_uuids         UUID[],         -- All group memberships
    
    -- Context
    current_schema_uuid UUID,
    dialect             VARCHAR(32),
    namespace_root      VARCHAR(64),
    
    -- Policy state
    policy_epoch        BIGINT,         -- For cache invalidation
    
    -- Computed hashes (for caching)
    context_hash        BYTEA           -- SHA-256 of context
);
```

The Security Context is:
- Computed at transaction start
- Immutable for transaction lifetime
- Used for all authorization decisions
- Included in audit records

---

## 3. Principals

### 3.1 Users

Users are persistent identities that can authenticate and hold privileges.

```sql
CREATE TABLE sys.sec_users (
    user_uuid           UUID PRIMARY KEY,
    username            VARCHAR(128) NOT NULL UNIQUE,
    
    -- Flags
    is_superuser        BOOLEAN DEFAULT FALSE,
    is_system           BOOLEAN DEFAULT FALSE,
    can_login           BOOLEAN DEFAULT TRUE,
    
    -- Ownership
    default_schema_uuid UUID,
    
    -- Metadata
    created_at          TIMESTAMP NOT NULL,
    created_by          UUID
);
```

#### 3.1.1 Superuser

A superuser bypasses all permission checks:
- All privileges are implicitly granted
- Can access all objects in all schemas
- Can perform all administrative operations
- Cannot be denied access to anything

Superuser status is:
- Granted via `ALTER USER username SUPERUSER`
- Requires existing superuser or bootstrap
- Logged as critical security event

#### 3.1.2 System Users

System users are internal accounts that cannot login:
- Used for object ownership (e.g., catalog objects)
- Cannot authenticate
- Exist for privilege tracking only

### 3.2 Roles

Roles are named collections of privileges that can be activated by users.

```sql
CREATE TABLE sys.sec_roles (
    role_uuid           UUID PRIMARY KEY,
    role_name           VARCHAR(128) NOT NULL UNIQUE,
    
    -- Flags
    is_system           BOOLEAN DEFAULT FALSE,
    can_login           BOOLEAN DEFAULT FALSE,  -- Roles cannot login

    -- Defaults
    default_schema_uuid UUID,                  -- Home schema for role
    
    -- Metadata
    created_at          TIMESTAMP NOT NULL,
    created_by          UUID,
    description         TEXT
);
```

#### 3.2.1 Role Characteristics

- **Exclusive activation**: Only one role active at a time
- **Explicit activation**: User must `SET ROLE role_name`
- **Transaction-bound**: Role change requires commit/rollback
- **Grant-based membership**: Users are granted membership in roles
- **Home schema**: Optional default schema used when the role is active

#### 3.2.2 Role Membership

```sql
CREATE TABLE sys.sec_role_memberships (
    membership_uuid     UUID PRIMARY KEY,
    user_uuid           UUID NOT NULL,
    role_uuid           UUID NOT NULL,
    
    -- Grant options
    with_admin_option   BOOLEAN DEFAULT FALSE,  -- Can grant role to others
    
    -- Validity
    valid_from          TIMESTAMP,
    valid_until         TIMESTAMP,
    
    -- Metadata
    granted_at          TIMESTAMP NOT NULL,
    granted_by          UUID NOT NULL,
    
    UNIQUE (user_uuid, role_uuid)
);
```

#### 3.2.3 Built-in Roles

| Role | Description | Privileges |
|------|-------------|------------|
| PUBLIC | Implicit membership for all users | CONNECT, USAGE on public schema |
| DB_OWNER | Database owner role | All privileges on database |
| SYSTEM_ADMINISTRATOR | Full administrative access | Superuser equivalent |

### 3.3 Groups

Groups are collections of users with cumulative privileges.

```sql
CREATE TABLE sys.sec_groups (
    group_uuid          UUID PRIMARY KEY,
    group_name          VARCHAR(128) NOT NULL UNIQUE,
    
    -- Nesting
    parent_group_uuid   UUID,           -- For nested groups
    
    -- External mapping
    external_source     VARCHAR(64),    -- 'ldap', 'ad', etc.
    external_id         VARCHAR(256),   -- DN or group name

    -- Defaults
    default_schema_uuid UUID,           -- Home schema for group
    
    -- Metadata
    created_at          TIMESTAMP NOT NULL,
    created_by          UUID,
    description         TEXT,
    
    FOREIGN KEY (parent_group_uuid) REFERENCES sys.sec_groups(group_uuid)
);
```

#### 3.3.1 Group Characteristics

- **Cumulative**: All group privileges apply simultaneously
- **Automatic**: No activation required
- **Nestable**: Groups can contain other groups
- **External mapping**: Can map to LDAP/AD groups
- **Home schema**: Optional default schema used when a user has no role/user default

#### 3.3.2 Group Membership

```sql
CREATE TABLE sys.sec_group_memberships (
    membership_uuid     UUID PRIMARY KEY,
    
    -- Member (user or nested group)
    member_type         ENUM('USER', 'GROUP'),
    member_uuid         UUID NOT NULL,
    
    -- Group
    group_uuid          UUID NOT NULL,
    
    -- Metadata
    added_at            TIMESTAMP NOT NULL,
    added_by            UUID,
    
    UNIQUE (member_uuid, group_uuid)
);
```

#### 3.3.3 Transitive Group Resolution

Groups are resolved transitively at session creation:

```python
def resolve_groups(user_uuid):
    """Resolve all groups for a user, including nested groups."""
    direct_groups = get_direct_group_memberships(user_uuid)
    all_groups = set(direct_groups)
    
    # Resolve nested groups (with cycle detection)
    visited = set()
    to_process = list(direct_groups)
    
    while to_process:
        group = to_process.pop()
        if group in visited:
            continue  # Cycle detected, skip
        visited.add(group)
        
        parent = get_parent_group(group)
        if parent and parent not in all_groups:
            all_groups.add(parent)
            to_process.append(parent)
    
    return list(all_groups)
```

### 3.4 Roles vs Groups Comparison

| Aspect | Roles | Groups |
|--------|-------|--------|
| Activation | Explicit (SET ROLE) | Automatic |
| Concurrent | One at a time | All simultaneously |
| Use case | Elevated privileges | Organizational membership |
| External mapping | Limited | Primary use case |
| Nesting | No | Yes |
| Typical usage | DBA tasks, application contexts | Department, team, project |

---

## 4. Privileges

### 4.1 Privilege Taxonomy

#### 4.1.1 Object Privileges

| Privilege | Applies To | Description |
|-----------|------------|-------------|
| SELECT | Table, View, Column | Read data |
| INSERT | Table, Column | Insert new rows |
| UPDATE | Table, Column | Modify existing rows |
| DELETE | Table | Delete rows |
| TRUNCATE | Table | Remove all rows |
| REFERENCES | Table, Column | Create foreign key |
| TRIGGER | Table | Create trigger on table |
| EXECUTE | Function, Procedure | Execute routine |
| USAGE | Schema, Sequence, Domain, Type | Use object |
| CREATE | Schema, Database | Create objects within |

#### 4.1.2 Administrative Privileges

| Privilege | Description |
|-----------|-------------|
| SUPERUSER | Bypass all permission checks |
| CREATEDB | Create new databases |
| CREATEROLE | Create and manage roles |
| CREATEUSER | Create and manage users |
| REPLICATION | Perform replication operations |
| BACKUP | Perform backup operations |
| COPY | Server-side COPY file access (grantable on DATABASE) |

#### 4.1.3 Special Privileges

| Privilege | Description |
|-----------|-------------|
| ALL | All applicable privileges |
| WITH GRANT OPTION | Can grant this privilege to others |
| WITH ADMIN OPTION | Can grant role membership to others |

### 4.2 Privilege Representation

```sql
CREATE TABLE sys.sec_privileges (
    privilege_uuid      UUID PRIMARY KEY,
    
    -- Grantee
    grantee_type        ENUM('USER', 'ROLE', 'GROUP', 'PUBLIC'),
    grantee_uuid        UUID,           -- NULL for PUBLIC
    
    -- Object
    object_type         VARCHAR(32) NOT NULL,
    object_uuid         UUID NOT NULL,
    column_uuid         UUID,           -- For column-level privileges
    
    -- Privilege
    privilege_type      VARCHAR(32) NOT NULL,
    with_grant_option   BOOLEAN DEFAULT FALSE,
    
    -- Metadata
    granted_at          TIMESTAMP NOT NULL,
    granted_by          UUID NOT NULL,
    
    UNIQUE (grantee_type, grantee_uuid, object_uuid, column_uuid, privilege_type)
);
```

### 4.3 Privilege Checking

```cpp
bool check_privilege(
    SecurityContext* ctx,
    ObjectUUID object,
    PrivilegeType required
) {
    // 1. Superuser check
    if (ctx->is_superuser) {
        return true;
    }
    
    // 2. Owner check
    if (get_object_owner(object) == ctx->user_uuid) {
        return true;
    }
    
    // 3. Resolve effective privileges
    PrivilegeSet effective = resolve_privileges(ctx, object);
    
    // 4. Check required privilege
    if (effective.contains(required) || effective.contains(ALL)) {
        return true;
    }
    
    // 5. Denied - log attempt
    audit_log(PRIVILEGE_DENIED, ctx, object, required);
    return false;
}
```

---

## 5. GRANT and REVOKE

### 5.1 GRANT Syntax

```sql
-- Grant object privileges
GRANT privilege_list
ON object_type object_name
TO grantee_list
[WITH GRANT OPTION];

-- Grant role membership
GRANT role_name [, ...]
TO user_or_role [, ...]
[WITH ADMIN OPTION];

-- Grant group membership
ALTER USER username ADD TO GROUP group_name;
-- or
ALTER GROUP group_name ADD MEMBER username;

-- Examples
GRANT SELECT, INSERT ON TABLE employees TO alice;
GRANT ALL PRIVILEGES ON SCHEMA hr TO ROLE hr_admin;
GRANT SELECT ON TABLE salaries TO GROUP finance WITH GRANT OPTION;
GRANT dba_role TO bob WITH ADMIN OPTION;
```

### 5.2 REVOKE Syntax

```sql
-- Revoke object privileges
REVOKE [GRANT OPTION FOR] privilege_list
ON object_type object_name
FROM grantee_list
[CASCADE | RESTRICT];

-- Revoke role membership
REVOKE [ADMIN OPTION FOR] role_name [, ...]
FROM user_or_role [, ...]
[CASCADE | RESTRICT];

-- Revoke group membership
ALTER USER username DROP FROM GROUP group_name;

-- Examples
REVOKE INSERT ON TABLE employees FROM alice;
REVOKE ALL PRIVILEGES ON SCHEMA hr FROM ROLE hr_admin CASCADE;
REVOKE GRANT OPTION FOR SELECT ON TABLE salaries FROM GROUP finance;
```

### 5.3 CASCADE vs RESTRICT

When revoking privileges granted WITH GRANT OPTION:

**RESTRICT** (default):
- Fails if grantee has granted the privilege to others
- Requires manual revocation of dependent grants

**CASCADE**:
- Automatically revokes dependent grants
- Recursively processes entire grant chain
- Logs all affected grants

```
┌─────────────────────────────────────────────────────────────────┐
│                    CASCADE REVOCATION                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  admin GRANTS SELECT TO alice WITH GRANT OPTION                  │
│       │                                                          │
│       └──► alice GRANTS SELECT TO bob WITH GRANT OPTION          │
│                   │                                              │
│                   └──► bob GRANTS SELECT TO charlie              │
│                                                                  │
│  REVOKE SELECT FROM alice CASCADE:                               │
│       • Revokes alice's SELECT                                   │
│       • Revokes bob's SELECT (granted by alice)                  │
│       • Revokes charlie's SELECT (granted by bob)                │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.4 Default Privileges

Set default privileges for objects created in a schema:

```sql
-- Set default privileges
ALTER DEFAULT PRIVILEGES IN SCHEMA schema_name
GRANT privilege_list ON object_type TO grantee;

-- Examples
ALTER DEFAULT PRIVILEGES IN SCHEMA hr
GRANT SELECT ON TABLES TO GROUP hr_readers;

ALTER DEFAULT PRIVILEGES IN SCHEMA hr
GRANT EXECUTE ON FUNCTIONS TO GROUP hr_users;

-- Remove default privileges
ALTER DEFAULT PRIVILEGES IN SCHEMA hr
REVOKE SELECT ON TABLES FROM GROUP hr_readers;
```

### 5.5 Grant Processing

```
┌─────────────────────────────────────────────────────────────────┐
│                    GRANT PROCESSING FLOW                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Parse GRANT statement                                        │
│                                                                  │
│  2. Resolve object UUID(s)                                       │
│     • Table, schema, function, etc.                              │
│     • May expand wildcards (ALL TABLES IN SCHEMA)                │
│                                                                  │
│  3. Resolve grantee UUID(s)                                      │
│     • User, role, group, or PUBLIC                               │
│                                                                  │
│  4. Validate grantor has privilege                               │
│     • Must have privilege WITH GRANT OPTION                      │
│     • Or be owner/superuser                                      │
│                                                                  │
│  5. For each (object, grantee, privilege):                       │
│     a. Check existing grant                                      │
│     b. If exists: update grant option if needed                  │
│     c. If not exists: insert new privilege record                │
│     d. Log grant event                                           │
│                                                                  │
│  6. Invalidate permission cache                                  │
│     • Increment policy epoch                                     │
│     • Affected sessions re-resolve on next transaction           │
│                                                                  │
│  7. Commit                                                       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. Row-Level Security (RLS)

### 6.1 Overview

Row-Level Security restricts which rows a user can see or modify based on policies.

### 6.2 RLS Policies

```sql
CREATE TABLE sys.sec_rls_policies (
    policy_uuid         UUID PRIMARY KEY,
    policy_name         VARCHAR(128) NOT NULL,
    
    -- Target
    table_uuid          UUID NOT NULL,
    
    -- Policy type
    policy_type         ENUM('PERMISSIVE', 'RESTRICTIVE'),
    command_type        ENUM('ALL', 'SELECT', 'INSERT', 'UPDATE', 'DELETE'),
    
    -- Roles this policy applies to
    applies_to          UUID[],         -- Empty = all roles
    
    -- Expressions (stored as SBLR or expression tree)
    using_expr          BYTEA,          -- For SELECT, UPDATE, DELETE
    check_expr          BYTEA,          -- For INSERT, UPDATE
    
    -- Metadata
    is_enabled          BOOLEAN DEFAULT TRUE,
    created_at          TIMESTAMP NOT NULL,
    created_by          UUID
);
```

### 6.3 Policy Types

**PERMISSIVE** (default):
- Multiple permissive policies are OR'd together
- Row visible if ANY permissive policy allows

**RESTRICTIVE**:
- Multiple restrictive policies are AND'd together
- Row visible only if ALL restrictive policies allow

```
Visibility = (P1 OR P2 OR ... OR Pn) AND (R1 AND R2 AND ... AND Rm)
where P = permissive policies, R = restrictive policies
```

### 6.4 RLS SQL Syntax

```sql
-- Enable RLS on table
ALTER TABLE table_name ENABLE ROW LEVEL SECURITY;

-- Force RLS even for table owner
ALTER TABLE table_name FORCE ROW LEVEL SECURITY;

-- Create policy
CREATE POLICY policy_name ON table_name
[AS {PERMISSIVE | RESTRICTIVE}]
[FOR {ALL | SELECT | INSERT | UPDATE | DELETE}]
[TO {role_name | PUBLIC | CURRENT_USER}]
[USING (using_expression)]
[WITH CHECK (check_expression)];

-- Examples

-- Tenant isolation
CREATE POLICY tenant_isolation ON orders
USING (tenant_id = current_setting('app.tenant_id')::uuid);

-- Users see only their own records
CREATE POLICY own_records ON user_data
USING (user_id = CURRENT_USER_UUID());

-- Managers see subordinates
CREATE POLICY manager_view ON employees
USING (
    department_id IN (
        SELECT department_id FROM managers 
        WHERE manager_id = CURRENT_USER_UUID()
    )
);

-- Restrictive: only active records
CREATE POLICY active_only ON products
AS RESTRICTIVE
USING (status = 'ACTIVE');
```

### 6.5 RLS Enforcement

```
┌─────────────────────────────────────────────────────────────────┐
│                    RLS ENFORCEMENT                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Query: SELECT * FROM orders WHERE amount > 1000                 │
│                                                                  │
│  1. Check if RLS enabled on 'orders'                             │
│                                                                  │
│  2. Find applicable policies:                                    │
│     • Match command type (SELECT)                                │
│     • Match role (CURRENT_USER or applicable roles)              │
│                                                                  │
│  3. Build combined predicate:                                    │
│     • Permissive: P1 OR P2 OR ...                               │
│     • Restrictive: R1 AND R2 AND ...                            │
│     • Combined: (permissive_expr) AND (restrictive_expr)         │
│                                                                  │
│  4. Rewrite query:                                               │
│     SELECT * FROM orders                                         │
│     WHERE amount > 1000                                          │
│       AND (tenant_id = 'abc123')      -- RLS predicate           │
│       AND (status = 'ACTIVE')         -- Restrictive             │
│                                                                  │
│  5. Execute rewritten query                                      │
│                                                                  │
│  Note: RLS/CLS policy gates are determined and applied by the Engine during plan construction/approval (engine-side rewrite). The parser may carry query representation, but it is not trusted to enforce policies.     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 6.6 RLS Bypass

Certain users may bypass RLS:

| Bypass Condition | Description |
|------------------|-------------|
| Superuser | Always bypasses unless FORCE RLS |
| Table owner | Bypasses unless FORCE RLS |
| BYPASSRLS privilege | Explicit bypass privilege |
| RLS disabled | Table has RLS disabled |

```sql
-- Grant bypass privilege (use sparingly)
GRANT BYPASSRLS TO maintenance_user;

-- Force RLS even for owner
ALTER TABLE sensitive_data FORCE ROW LEVEL SECURITY;
```

### 6.7 RLS Security Context

RLS policies can reference security context variables:

| Function | Returns |
|----------|---------|
| `CURRENT_USER_UUID()` | Session's user UUID |
| `CURRENT_ROLE_UUID()` | Active role UUID (or NULL) |
| `CURRENT_GROUPS()` | Array of group UUIDs |
| `current_setting('name')` | Session variable value |

---

## 7. Column-Level Security (CLS)

### 7.1 Overview

Column-Level Security restricts which columns a user can access within rows they can see.

### 7.2 Column Privileges

```sql
-- Grant column-level SELECT
GRANT SELECT (first_name, last_name, email) ON employees TO hr_staff;

-- Grant column-level UPDATE
GRANT UPDATE (email, phone) ON employees TO hr_staff;

-- Revoke column access
REVOKE SELECT (salary, ssn) ON employees FROM hr_staff;
```

### 7.3 Column-Level SELECT

When a user has column-level (not table-level) SELECT:

```sql
-- User has SELECT on (first_name, last_name, email) only

SELECT * FROM employees;
-- Error: permission denied for column "salary"

SELECT first_name, last_name, email FROM employees;
-- Success: only permitted columns

SELECT first_name, salary FROM employees;
-- Error: permission denied for column "salary"
```

### 7.4 Column Masking

For sensitive columns, masking can be applied instead of denial:

```sql
-- Create masking policy
CREATE MASK salary_mask ON employees.salary
FOR ROLE hr_basic
RETURNS '***MASKED***';

CREATE MASK ssn_mask ON employees.ssn
FOR GROUP all_employees
RETURNS CONCAT('***-**-', RIGHT(ssn, 4));

-- Result: users see masked values instead of errors
SELECT name, ssn FROM employees;
-- Returns: "John Doe", "***-**-1234"
```

### 7.5 CLS Enforcement

```
┌─────────────────────────────────────────────────────────────────┐
│                    CLS ENFORCEMENT                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Query: SELECT * FROM employees                                  │
│                                                                  │
│  User has: SELECT(first_name, last_name, email)                  │
│  User lacks: SELECT(salary, ssn, hire_date)                      │
│                                                                  │
│  Options (configurable):                                         │
│                                                                  │
│  1. STRICT mode (default):                                       │
│     • SELECT * fails with permission error                       │
│     • Must explicitly list permitted columns                     │
│                                                                  │
│  2. FILTER mode:                                                 │
│     • SELECT * returns only permitted columns                    │
│     • Silent omission of restricted columns                      │
│                                                                  │
│  3. MASK mode:                                                   │
│     • SELECT * returns all columns                               │
│     • Restricted columns show masked values                      │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 8. Namespace and Catalog Security

### 8.1 Schema Permissions

Schemas require USAGE privilege before accessing objects within:

```sql
-- Grant schema usage
GRANT USAGE ON SCHEMA hr TO alice;

-- Grant create in schema
GRANT CREATE ON SCHEMA hr TO alice;

-- Without USAGE, objects in schema are invisible
-- even if user has privileges on the objects
```

### 8.2 System Catalog Protection

### 8.2 System Catalog Protection

The `sys.*` schemas contain security-critical metadata (users, roles, grants, keys, audits, cluster state, etc.).
In **strict mode**, ScratchBird treats the system catalog as *engine-owned* and *non-queryable* by ordinary SQL
to avoid privilege escalation, metadata exfiltration, and side-channel enumeration.

**Decision (Option A):** `sys.*` is **blocked** for direct `SELECT/UPDATE/DELETE/INSERT` access, including for superusers
when strict mode is enabled. Introspection is provided through **engine-owned virtual views** and **SHOW/DESCRIBE**
commands which:

- perform explicit authorization checks in Engine Core,
- apply row/field redaction (least privilege),
- normalize output to avoid information leaks (e.g., hide objects without USAGE/SELECT),
- provide stable operational tooling without exposing raw catalog tables.

#### 8.2.1 Engine-owned Virtual Views

Virtual views are not ordinary SQL views; they are engine endpoints that materialize curated result sets at execution time.
Examples (illustrative):

- `SHOW DATABASES`, `SHOW SCHEMAS`, `SHOW TABLES`, `SHOW COLUMNS`
- `SHOW GRANTS [FOR <principal>]`
- `SHOW ROLES`, `SHOW GROUPS`, `SHOW MEMBERSHIP`
- `SHOW SESSIONS`, `SHOW LOCKS`, `SHOW TRANSACTIONS`
- `SHOW AUDIT [SINCE ...]` (subject to audit access rules)
- `SHOW KEY HEALTH` / `SHOW ENCRYPTION STATUS` (redacted)

Each virtual view/SHOW command MUST define:
- required privilege(s),
- redaction rules,
- whether it is available at each security level.

#### 8.2.2 Compatibility Modes

Non-strict deployments (security levels 0–2) MAY offer limited, read-only catalog access for development convenience,
but this MUST be disabled by default in strict mode and MUST be explicitly configured.


### 8.3 SHOW Commands

Metadata discovery is via SHOW commands that filter by permissions:

```sql
-- Show tables user can access
SHOW TABLES [IN schema_name] [LIKE 'pattern'];

-- Show columns (requires table privilege)
SHOW COLUMNS IN table_name;

-- Show user's privileges on an object
SHOW GRANTS ON object_name;

-- Show user's effective privileges
SHOW PRIVILEGES;

-- Show current session info
SHOW CURRENT_USER;
SHOW CURRENT_ROLE;
SHOW CURRENT_SCHEMA;
```

### 8.4 Dialect Namespace Isolation

Each dialect operates in its own namespace:

```
┌─────────────────────────────────────────────────────────────────┐
│                 NAMESPACE ISOLATION                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  MySQL session:                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ Root namespace: mysql_compat                             │    │
│  │                                                          │    │
│  │ Visible schemas:                                         │    │
│  │   information_schema  →  mysql_compat.information_schema │    │
│  │   mysql              →  mysql_compat.mysql               │    │
│  │   user_db            →  user_schemas.user_db             │    │
│  │                                                          │    │
│  │ NOT visible:                                             │    │
│  │   sys.*, pg_compat.*, fb_compat.*                       │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  PostgreSQL session:                                             │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ Root namespace: pg_compat                                │    │
│  │                                                          │    │
│  │ Visible schemas:                                         │    │
│  │   pg_catalog         →  pg_compat.pg_catalog             │    │
│  │   information_schema →  pg_compat.information_schema     │    │
│  │   public             →  user_schemas.public              │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  V2 (Native) session:                                            │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ Root namespace: sys                                      │    │
│  │                                                          │    │
│  │ Full access to all schemas (subject to permissions)      │    │
│  │ Can see sys.*, all compat namespaces, all user schemas   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 8.5 Cross-Namespace Access

Users may need to access objects across dialects:

```sql
-- V2 session accessing MySQL-created table
SELECT * FROM mysql_compat.app_db.orders;

-- Requires:
-- 1. USAGE on mysql_compat schema
-- 2. USAGE on app_db schema
-- 3. SELECT on orders table

-- MySQL session cannot access pg_compat:
SELECT * FROM pg_compat.public.users;
-- Error: Schema 'pg_compat' not found
-- (namespace isolation prevents resolution)
```

---

## 9. Authorization Caching

### 9.1 Permission Cache

Privilege lookups are cached for performance:

```sql
CREATE TABLE sys.local_permission_cache (
    cache_key           BYTEA PRIMARY KEY,  -- Hash of (user, object, privilege)
    
    -- Result
    is_granted          BOOLEAN NOT NULL,
    grant_source        VARCHAR(32),        -- 'DIRECT', 'ROLE', 'GROUP', 'PUBLIC'
    
    -- Validity
    cached_at           TIMESTAMP NOT NULL,
    policy_epoch        BIGINT NOT NULL,
    expires_at          TIMESTAMP NOT NULL
);
```

### 9.2 Cache Invalidation

The cache is invalidated when:

| Event | Invalidation Scope |
|-------|-------------------|
| GRANT/REVOKE | Affected grantee + object |
| Role membership change | User's role-derived permissions |
| Group membership change | User's group-derived permissions |
| Object dropped | All permissions on object |
| Policy epoch change | All cached permissions |
| User blocked/deleted | All user's permissions |

### 9.3 Policy Epoch

The policy epoch is a monotonic counter that increments on any authorization change:

```python
def increment_policy_epoch():
    """Called on any authorization change."""
    current = get_policy_epoch()
    new_epoch = current + 1
    set_policy_epoch(new_epoch)
    
    # Broadcast to cluster (if applicable)
    if is_cluster_mode():
        broadcast_policy_epoch(new_epoch)
```

Sessions compare their Security Context's policy epoch to current:
- If match: cached permissions valid
- If mismatch: re-resolve permissions at next transaction

---



### 9.4 Revocation-Sensitive Objects and Mid-Statement Hooks (BeforeSelect)

ScratchBird’s default model binds authorization to the transaction Security Context, meaning privilege/policy changes
take effect at **statement/transaction boundaries**. This is deliberate for consistency and to avoid mixing security checks
into row-visibility (MGA).

Some SQLObjects require **immediate** response to permission changes (e.g., regulated datasets, emergency revocation,
highly sensitive views). For these cases the engine supports **revocation-sensitive enforcement** using engine-executed hooks.

#### 9.4.1 Object Tag / Policy Flag

An object (table, view, function, etc.) MAY be marked `REVOCATION_SENSITIVE`, or a policy MAY require immediate enforcement.
This is metadata stored in the engine catalog and included in SBLR validation.

#### 9.4.2 BeforeSelect Hook Semantics

When a statement reads a `REVOCATION_SENSITIVE` object, Engine Core MUST run a `BeforeSelect` hook at least once:

- **At statement start** before any scan begins, and
- **Optionally per batch/chunk** before producing the next batch to the client (recommended at higher security levels).

The hook is executed by Engine Core (not the parser) and MAY consult:
- current grant/role/group epochs,
- current policy epochs,
- external authorization epochs (if configured).

If the hook detects revocation or policy changes that invalidate the previously approved plan, Engine Core MUST enforce
one of the configured actions:

- `DENY_NEW_BATCHES`: stop producing rows and error the statement,
- `CANCEL_STATEMENT`: cancel immediately,
- `TERMINATE_SESSION`: terminate the session/transaction (highest assurance).

#### 9.4.3 Interaction With Plan Approval

Plan approval (including RLS/CLS gates) still occurs before execution begins. `BeforeSelect` does not “repair” a plan;
it gates continued execution when immediate revocation semantics are required.

#### 9.4.4 Security Levels

- Levels 0–4: `BeforeSelect` is optional and typically disabled.
- Level 5: `BeforeSelect` MAY be required for objects flagged `REVOCATION_SENSITIVE`.
- Level 6: `BeforeSelect` SHOULD run per batch for revocation-sensitive objects, and MAY be combined with NPB/MEK policies.

## 10. Authorization in Transactions

### 10.1 Privilege Staleness Window

Because Security Context is transaction-bound:

1. Privileges are resolved at transaction start
2. Changes during transaction are not visible
3. New transaction sees current privileges

This creates a staleness window where:
- Revoked privileges are still effective
- Granted privileges are not yet available

### 10.2 Handling Privilege Changes

```
┌─────────────────────────────────────────────────────────────────┐
│             PRIVILEGE CHANGE HANDLING                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Scenario: Alice's SELECT on sensitive_table is revoked         │
│                                                                  │
│  Option 1: TRANSACTION_BOUNDARY (default)                        │
│  • Alice's current transaction continues with old privileges     │
│  • Next transaction sees revocation                              │
│  • Audit: "Privilege exercised within staleness window"          │
│                                                                  │
│  Option 2: IMMEDIATE_NOTIFY                                      │
│  • Alice's session receives notification                         │
│  • Transaction continues (no interruption)                       │
│  • Application can choose to commit/rollback                     │
│                                                                  │
│  Option 3: IMMEDIATE_RESTRICT (high security)                    │
│  • Current transaction enters read-only mode                     │
│  • New privileged operations fail                                │
│  • Must commit/rollback to continue                              │
│                                                                  │
│  Option 4: IMMEDIATE_TERMINATE (maximum security)                │
│  • Current transaction is rolled back                            │
│  • Session may be terminated                                     │
│  • Used for emergency revocations                                │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 10.3 Long-Running Transaction Limits

To limit staleness windows:

```sql
-- Set maximum transaction duration
SET POLICY transaction.max_duration = '1 hour';

-- Set maximum idle time in transaction
SET POLICY transaction.idle_timeout = '10 minutes';

-- Force privilege refresh interval
SET POLICY authorization.refresh_interval = '5 minutes';
```

---

## 11. Audit Events

### 11.1 Authorization Events

| Event | Logged Data |
|-------|-------------|
| PRIVILEGE_GRANTED | grantor, grantee, object, privilege, with_grant |
| PRIVILEGE_REVOKED | revoker, grantee, object, privilege, cascade_count |
| PRIVILEGE_DENIED | session, object, required_privilege, reason |
| ROLE_ACTIVATED | session, role_uuid |
| ROLE_DEACTIVATED | session, role_uuid |
| RLS_POLICY_CREATED | policy_name, table, creator |
| RLS_POLICY_APPLIED | session, table, rows_filtered |

### 11.2 Denied Access Logging

All denied access attempts are logged:

```json
{
    "event_type": "PRIVILEGE_DENIED",
    "timestamp": "2026-01-15T10:30:45.123Z",
    "session_uuid": "0198f0b2-1111-...",
    "user_uuid": "0198f0b2-2222-...",
    "object_uuid": "0198f0b2-3333-...",
    "object_type": "TABLE",
    "object_name": "employees",
    "required_privilege": "SELECT",
    "user_has_privileges": ["USAGE"],
    "denial_reason": "INSUFFICIENT_PRIVILEGE",
    "query_hash": "sha256:abc123..."
}
```

---

## 12. Security Considerations

### 12.1 Privilege Escalation Prevention

| Attack Vector | Mitigation |
|---------------|------------|
| Grant chain manipulation | CASCADE revocation, grant tracking |
| Role assumption | Explicit SET ROLE, no auto-activation |
| Group injection | External group mapping validation |
| Object ownership transfer | Requires owner or superuser |
| SECURITY DEFINER abuse | search_path pinning, limited use |

### 12.2 Information Disclosure

| Risk | Mitigation |
|------|------------|
| Catalog enumeration | SHOW commands filter by privilege |
| Error message leakage | Generic errors for unauthorized objects |
| Timing attacks | Constant-time privilege checks |
| RLS bypass via functions | SECURITY INVOKER default |

### 12.3 Best Practices

1. **Principle of Least Privilege**: Grant minimum necessary permissions
2. **Use Groups**: Organize by function, not individual grants
3. **Role Separation**: Separate admin, operational, and read-only roles
4. **Avoid PUBLIC Grants**: Explicit grants preferred
5. **Regular Audits**: Review and prune unused privileges
6. **RLS for Multi-Tenancy**: Enforce tenant isolation at database level

---

## Appendix A: Privilege Matrix

### A.1 Object Type Privileges

| Object Type | SELECT | INSERT | UPDATE | DELETE | TRUNCATE | REFERENCES | TRIGGER | EXECUTE | USAGE | CREATE |
|-------------|:------:|:------:|:------:|:------:|:--------:|:----------:|:-------:|:-------:|:-----:|:------:|
| Table | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | | | |
| View | ✓ | ✓* | ✓* | ✓* | | | ✓ | | | |
| Column | ✓ | ✓ | ✓ | | | ✓ | | | | |
| Sequence | ✓ | | ✓ | | | | | | ✓ | |
| Function | | | | | | | | ✓ | | |
| Procedure | | | | | | | | ✓ | | |
| Schema | | | | | | | | | ✓ | ✓ |
| Database | | | | | | | | | | ✓ |
| Domain | | | | | | | | | ✓ | |
| Type | | | | | | | | | ✓ | |

*If view is updatable

### A.2 Required Privileges by Operation

| Operation | Required Privileges |
|-----------|---------------------|
| SELECT FROM table | SELECT on table (or columns) |
| INSERT INTO table | INSERT on table (or columns) |
| UPDATE table | UPDATE + SELECT (for WHERE) |
| DELETE FROM table | DELETE + SELECT (for WHERE) |
| TRUNCATE table | TRUNCATE or owner |
| COPY TO/FROM file | COPY on database + SELECT/INSERT on table |
| CREATE TABLE | CREATE on schema |
| DROP TABLE | Owner or superuser |
| ALTER TABLE | Owner or superuser |
| CREATE INDEX | Owner of table or superuser |
| CREATE VIEW | CREATE on schema + SELECT on base tables |
| EXECUTE function | EXECUTE on function |
| CALL procedure | EXECUTE on procedure |

---

## Appendix B: Configuration Reference

### B.1 Authorization Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `auth.default_role` | uuid | NULL | Default role for new sessions |
| `auth.allow_role_switching` | boolean | true | Allow SET ROLE |
| `rls.default_mode` | enum | PERMISSIVE | Default RLS policy type |
| `cls.default_mode` | enum | STRICT | STRICT, FILTER, or MASK |
| `catalog.direct_access` | boolean | false | Allow direct sys.* access |
| `privilege.cache_ttl` | interval | 1m | Permission cache TTL |
| `privilege.revoke_mode` | enum | TRANSACTION_BOUNDARY | Revocation handling |

---

*End of Document*
