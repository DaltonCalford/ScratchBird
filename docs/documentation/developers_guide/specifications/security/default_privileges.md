# Specification: Default Privileges

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/authorization/default_privs |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase2.cpp`

## Synopsis

This specification defines the default privileges system in ScratchBird, which allows roles to set privileges that will be applied automatically to objects created in the future. This is implemented via `ALTER DEFAULT PRIVILEGES`.

## Scope

### In Scope

- Default privileges data model
- ALTER DEFAULT PRIVILEGES syntax and semantics
- Privilege application on object creation
- Default privilege inheritance
- Schema-level defaults

### Out of Scope

- Basic GRANT/REVOKE operations
- ACL storage format (see `acl_format.md`)
- Permission checking algorithms

## Background

Default privileges solve the problem of ensuring consistent access control across newly created objects. Without defaults, each object creator must remember to grant appropriate permissions after creation.

## Specification

### Data Structures

```cpp
// Default Privileges Record

struct DefaultPrivilegesRecord {
    ID default_priv_id;         // UUIDv7
    ID role_id;                 // Role whose defaults these are
    ID schema_id;               // Schema scope (0 = all schemas)
    ObjectType object_type;     // TABLES, SEQUENCES, FUNCTIONS, TYPES, SCHEMAS
    
    // The ACL that will be applied
    std::vector<AclItem> acl;
    
    // Metadata
    ID grantor_id;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};
```

### System Catalog Table

```
┌─────────────────────────────────────────────────────────────────┐
│ sb_default_privileges                                           │
├─────────────────┬─────────────────┬─────────────────────────────┤
│ default_priv_id │ UUID (PK)       │ Unique identifier           │
│ role_id         │ UUID (FK)       │ Role whose defaults         │
│ schema_id       │ UUID (FK/null)  │ Schema scope (NULL=all)     │
│ object_type     │ char(1)         │ r=TABLE, S=SEQUENCE,        │
│                 │                 │ f=FUNCTION, T=TYPE,         │
│                 │                 │ n=NAMESPACE(schema)         │
│ acl             │ aclitem[]       │ ACL to apply                │
│ grantor_id      │ UUID (FK)       │ Who set these defaults      │
│ created_at      │ timestamp       │ When created                │
│ updated_at      │ timestamp       │ When last modified          │
└─────────────────┴─────────────────┴─────────────────────────────┘

Indexes:
- (role_id, schema_id, object_type) - Lookup defaults
- (schema_id) - Schema-specific defaults
```

### SQL Syntax

```sql
ALTER DEFAULT PRIVILEGES
    [ FOR { ROLE | USER } target_role [, ...] ]
    [ IN SCHEMA schema_name [, ...] ]
    grant_or_revoke_clause

where grant_or_revoke_clause is:
    GRANT { { SELECT | INSERT | UPDATE | DELETE | TRUNCATE | REFERENCES | TRIGGER }
        [, ...] | ALL [ PRIVILEGES ] }
        ON TABLES
        TO { [ GROUP ] role_name | PUBLIC } [, ...] [ WITH GRANT OPTION ]

    GRANT { { USAGE | SELECT | UPDATE }
        [, ...] | ALL [ PRIVILEGES ] }
        ON SEQUENCES
        TO { [ GROUP ] role_name | PUBLIC } [, ...] [ WITH GRANT OPTION ]

    GRANT { EXECUTE | ALL [ PRIVILEGES ] }
        ON FUNCTIONS
        TO { [ GROUP ] role_name | PUBLIC } [, ...] [ WITH GRANT OPTION ]

    GRANT { USAGE | ALL [ PRIVILEGES ] }
        ON TYPES
        TO { [ GROUP ] role_name | PUBLIC } [, ...] [ WITH GRANT OPTION ]

    GRANT { USAGE | CREATE | ALL [ PRIVILEGES ] }
        ON SCHEMAS
        TO { [ GROUP ] role_name | PUBLIC } [, ...] [ WITH GRANT OPTION ]

    REVOKE [ GRANT OPTION FOR ]
        { { SELECT | INSERT | UPDATE | DELETE | TRUNCATE | REFERENCES | TRIGGER }
        [, ...] | ALL [ PRIVILEGES ] }
        ON TABLES
        FROM { [ GROUP ] role_name | PUBLIC } [, ...]
        [ CASCADE | RESTRICT ]

    -- Similar REVOKE clauses for SEQUENCES, FUNCTIONS, TYPES, SCHEMAS
```

### Default Privileges Application

When an object is created, default privileges are applied as follows:

```
Algorithm: Apply Default Privileges on Object Creation

Input: new_object, creator_role, schema
Output: Modified ACL for new object

1. INITIALIZE acl with owner entry only
   acl = [{owner: creator, privileges: ALL}]

2. COLLECT applicable default privilege records
   defaults = []
   
   // Global defaults (no schema specified)
   global_defaults = selectDefaults(role_id=creator, schema_id=NULL)
   defaults += global_defaults
   
   // Schema-specific defaults
   schema_defaults = selectDefaults(role_id=creator, schema_id=schema)
   defaults += schema_defaults

3. FOR EACH default_record IN defaults:
     FOR EACH acl_item IN default_record.acl:
       // Add grantee to object ACL
       IF grantee not in acl:
         acl.add({
           grantee: acl_item.grantee,
           privileges: acl_item.privileges,
           grantor: creator,
           grant_option: acl_item.grant_option
         })

4. RETURN acl
```

### Default Privileges Scope Resolution

```
Scope Hierarchy (most specific wins):

1. Schema-specific defaults for creating role
   ALTER DEFAULT PRIVILEGES IN SCHEMA myschema ...

2. Global defaults for creating role
   ALTER DEFAULT PRIVILEGES ...

3. Inherited defaults from member roles (INHERIT)
   If creating role inherits from parent roles

4. No defaults (owner only)
```

### Interface Contracts

#### Function: `setDefaultPrivileges()`

```cpp
// CatalogManager interface
Status setDefaultPrivileges(
    const ID& role_id,           // Target role
    const std::optional<ID>& schema_id,  // NULL for all schemas
    ObjectType object_type,      // Tables, sequences, etc.
    const std::vector<AclItem>& acl,     // ACL to set as default
    ErrorContext* ctx
);
```

**Preconditions:**
- Role exists
- Schema exists (if specified)
- Caller has permission to alter role's defaults

**Postconditions:**
- Default privilege record created or updated
- Future objects will receive these privileges

#### Function: `getDefaultPrivileges()`

```cpp
// Retrieve defaults for a role
std::vector<DefaultPrivilegesRecord> getDefaultPrivileges(
    const ID& role_id,
    const std::optional<ID>& schema_id = std::nullopt
);
```

**Returns:**
- All default privilege records for role
- Optionally filtered by schema

#### Function: `applyDefaultPrivileges()`

```cpp
// Called during object creation
Status applyDefaultPrivileges(
    const ID& object_id,
    ObjectType object_type,
    const ID& creator_id,
    const ID& schema_id,
    std::vector<AclItem>& out_acl,
    ErrorContext* ctx
);
```

**Called by:**
- CREATE TABLE
- CREATE SEQUENCE
- CREATE FUNCTION
- CREATE TYPE
- CREATE SCHEMA

### Examples

```sql
-- Set default privileges for tables created by app_admin
ALTER DEFAULT PRIVILEGES FOR ROLE app_admin
    GRANT SELECT ON TABLES TO app_reader;

ALTER DEFAULT PRIVILEGES FOR ROLE app_admin
    GRANT SELECT, INSERT, UPDATE ON TABLES TO app_writer;

-- Set schema-specific defaults
ALTER DEFAULT PRIVILEGES FOR ROLE data_engineer IN SCHEMA analytics
    GRANT SELECT ON TABLES TO analyst_group;

-- Set defaults for functions
ALTER DEFAULT PRIVILEGES FOR ROLE developer
    GRANT EXECUTE ON FUNCTIONS TO api_service;

-- View current defaults
SELECT * FROM pg_default_acl WHERE defaclrole = 'app_admin'::regrole;

-- Revoke default privileges
ALTER DEFAULT PRIVILEGES FOR ROLE app_admin
    REVOKE SELECT ON TABLES FROM app_reader;
```

### Default Privileges State Machine

```
┌─────────────────────────────────────────────────────────────────┐
│                    Default Privileges Lifecycle                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CREATE ROLE                                                    │
│       │                                                         │
│       ▼                                                         │
│  ┌─────────┐                                                    │
│  │  NO     │  ◄── Initial state (no defaults)                  │
│  │DEFAULTS │                                                    │
│  └────┬────┘                                                    │
│       │                                                         │
│       │ ALTER DEFAULT PRIVILEGES                                │
│       ▼                                                         │
│  ┌─────────┐     CREATE object     ┌───────────┐               │
│  │ DEFAULTS│ ───────────────────►  │ APPLIED   │               │
│  │ DEFINED │     privileges        │ TO OBJECT │               │
│  └────┬────┘                       └───────────┘               │
│       │                                                         │
│       │ ALTER DEFAULT PRIVILEGES (modify)                       │
│       ▼                                                         │
│  ┌─────────┐     Future objects    ┌───────────┐               │
│  │DEFAULTS │ ───────────────────►  │ MODIFIED  │               │
│  │ UPDATED │     get new defaults  │ APPLIED   │               │
│  └─────────┘                       └───────────┘               │
│       │                                                         │
│       │ DROP ROLE / ALTER DEFAULT PRIVILEGES (clear)            │
│       ▼                                                         │
│  ┌─────────┐                                                    │
│  │DEFAULTS │  ◄── Back to no defaults                          │
│  │ REMOVED │                                                    │
│  └─────────┘                                                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Interaction with Object Creation

```sql
-- Example: How defaults flow through object creation

-- 1. Admin sets defaults
ALTER DEFAULT PRIVILEGES FOR ROLE app_admin
    GRANT SELECT ON TABLES TO reporting_user;

-- 2. Admin creates table
CREATE TABLE app_admin.metrics (
    id UUID PRIMARY KEY,
    value NUMERIC
);
-- ACL automatically becomes: 
-- {app_admin=arwdDxt/app_admin, reporting_user=r/app_admin}

-- 3. Reporting user can query
SELECT * FROM app_admin.metrics;  -- Success!
```

### Inheritance of Defaults

```sql
-- When a role inherits from another
CREATE ROLE developer;
CREATE ROLE senior_developer INHERIT;
GRANT developer TO senior_developer;

-- Set defaults for base role
ALTER DEFAULT PRIVILEGES FOR ROLE developer
    GRANT SELECT ON TABLES TO qa_team;

-- Senior developer creates table
-- Both their own defaults AND inherited defaults are applied
```

### Viewing Default Privileges

```sql
-- System catalogs
SELECT 
    pg_get_userbyid(defaclrole) as role,
    n.nspname as schema,
    defaclobjtype as object_type,
    pg_catalog.array_to_string(defaclacl, E'\n') as acl
FROM pg_default_acl
LEFT JOIN pg_namespace n ON n.oid = defaclnamespace;

-- Helper function
SELECT * FROM pg_default_acl WHERE defaclrole = 'myrole'::regrole;
```

## Invariants

1. **Creator Application**: Only defaults for the creating role are applied
   - Verification: Filter by role_id=creator at object creation

2. **Schema Specificity**: Schema-specific defaults override global
   - Verification: Apply schema-specific after global

3. **Owner Exclusion**: Owner doesn't need explicit grants
   - Verification: Owner always has ALL

4. **Future Only**: Defaults don't affect existing objects
   - Verification: Only called during CREATE operations

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `UNDEFINED_ROLE` | Target role doesn't exist | Create role first |
| `UNDEFINED_SCHEMA` | Schema doesn't exist | Create schema first |
| `INVALID_OBJECT_TYPE` | Object type not supported | Use valid type |
| `INSUFFICIENT_PRIVILEGE` | Can't alter role's defaults | Request appropriate privilege |

## Related Specifications

- `authorization_model.md` - Authorization model
- `acl_format.md` - ACL storage format
- `privilege_types.md` - Privilege definitions

## Appendix

### Default Privileges Best Practices

1. **Application Roles**: Set defaults for application service accounts
   ```sql
   ALTER DEFAULT PRIVILEGES FOR ROLE app_svc
       GRANT SELECT, INSERT, UPDATE ON TABLES TO app_reader;
   ```

2. **Schema Isolation**: Use schema-specific defaults for multi-tenant apps
   ```sql
   ALTER DEFAULT PRIVILEGES FOR ROLE tenant_a_svc IN SCHEMA tenant_a
       GRANT ALL ON TABLES TO tenant_a_user;
   ```

3. **Function Visibility**: Default execute on functions for API users
   ```sql
   ALTER DEFAULT PRIVILEGES FOR ROLE api_svc
       GRANT EXECUTE ON FUNCTIONS TO api_consumer;
   ```

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
