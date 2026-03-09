# Specification: Privilege Types

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/authorization/privileges |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:5216-5294`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:5253-5264` (Column permissions)
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_security_acl_abac_graph_token_quota_settings_contract.cpp`

## Synopsis

This specification defines all privilege types in ScratchBird, including object-level privileges, column-level privileges, system privileges, and their usage patterns across different database object types.

## Scope

### In Scope

- Object-level privileges (SELECT, INSERT, UPDATE, DELETE, etc.)
- Column-level privileges (SELECT, UPDATE, REFERENCES)
- System privileges (BYPASSRLS, CREATEDB, CREATEROLE, etc.)
- Privilege bitmasks and storage format
- Privilege applicability by object type

### Out of Scope

- ACL storage format details (see `acl_format.md`)
- GRANT/REVOKE syntax (see parser specs)
- Permission caching (see `authorization_model.md`)

## Background

ScratchBird implements a comprehensive privilege system modeled after PostgreSQL, with support for fine-grained access control at multiple levels.

## Specification

### Object-Level Privileges

```cpp
// From catalog_manager.cpp:5294

enum ObjectPermissions : uint32_t {
    EXECUTE     = 1,        // Execute function/procedure
    SELECT      = 2,        // Read data
    INSERT      = 4,        // Add data
    UPDATE      = 8,        // Modify data
    DELETE      = 16,       // Remove data
    USAGE       = 32,       // Use schema/sequence/domain
    CREATE      = 64,       // Create objects
    DROP        = 128,      // Drop objects
    ALTER       = 256,      // Alter objects
    INDEX       = 512,      // Create indexes
    TRIGGER     = 1024,     // Create triggers
    REFERENCES  = 2048,     // Foreign key references
    TRUNCATE    = 4096,     // Truncate table
    CONNECT     = 8192,     // Connect to database
    TEMPORARY   = 16384,    // Create temp objects
    ALL_PRIVILEGES = 0xFFFFFFFF
};
```

### Column-Level Privileges

```cpp
// From catalog_manager.cpp:5253-5264 (ColumnPermissionRecord)

enum ColumnPermission : uint32_t {
    COLUMN_SELECT     = 1,  // Select column
    COLUMN_UPDATE     = 2,  // Update column
    COLUMN_REFERENCES = 4   // Reference column in FK
};

struct ColumnPermissionRecord {
    ID permission_id;
    ID grantee_id;
    ID grantor_id;
    ID object_id;
    uint32_t column_id;      // Column ordinal position
    uint32_t permissions;    // Bitmask of ColumnPermission
    uint8_t grant_option;
};
```

### System Privileges

System privileges are attributes on roles/users, not grantable permissions:

```cpp
enum SystemPrivilege : uint64_t {
    // Role attributes
    SUPERUSER       = 1ull << 0,   // Bypass all permission checks
    CREATEDB        = 1ull << 1,   // Create databases
    CREATEROLE      = 1ull << 2,   // Create roles
    REPLICATION     = 1ull << 3,   // Replication connections
    BYPASSRLS       = 1ull << 4,   // Bypass Row-Level Security
    
    // Additional capabilities
    LOGIN           = 1ull << 5,   // Can login
    INHERIT         = 1ull << 6,   // Inherit parent role privileges
    BYPASSCLS       = 1ull << 7,   // Bypass Column-Level Security (UNMASK)
    
    // Audit
    AUDIT_ADMIN     = 1ull << 8,   // Configure audit logging
    AUDIT_VIEWER    = 1ull << 9,   // View audit logs
};
```

## Privilege Reference by Object Type

### Tables

| Privilege | Description | Allows |
|-----------|-------------|--------|
| `SELECT` | Read rows | SELECT, COPY TO |
| `INSERT` | Add rows | INSERT, COPY FROM |
| `UPDATE` | Modify rows | UPDATE |
| `DELETE` | Remove rows | DELETE |
| `TRUNCATE` | Empty table | TRUNCATE |
| `REFERENCES` | Create foreign keys | FOREIGN KEY referencing this table |
| `TRIGGER` | Create triggers | CREATE TRIGGER |
| `ALL` | All of above | Full table access |

**Column-level for Tables:**
- `SELECT (col1, col2)` - Select specific columns
- `UPDATE (col1)` - Update specific columns
- `REFERENCES (col1)` - Reference columns in FK

### Columns

| Privilege | Description |
|-----------|-------------|
| `SELECT` | Read column value |
| `UPDATE` | Modify column value |
| `REFERENCES` | Reference in foreign key |

### Views

| Privilege | Description | Notes |
|-----------|-------------|-------|
| `SELECT` | Read from view | Also need underlying table permissions |
| `INSERT` | Insert through view | Requires view to be updatable |
| `UPDATE` | Update through view | Requires view to be updatable |
| `DELETE` | Delete through view | Requires view to be updatable |

### Sequences

| Privilege | Description |
|-----------|-------------|
| `USAGE` | Access sequence (currval, nextval) |
| `SELECT` | currval() |
| `UPDATE` | nextval(), setval() |
| `ALL` | All sequence operations |

### Schemas

| Privilege | Description |
|-----------|-------------|
| `USAGE` | Access objects in schema |
| `CREATE` | Create objects in schema |
| `ALL` | Full schema access |

### Databases

| Privilege | Description |
|-----------|-------------|
| `CREATE` | Create schemas/tables in database |
| `CONNECT` | Connect to database |
| `TEMPORARY` | Create temporary tables |
| `ALL` | Full database access |

### Functions/Procedures

| Privilege | Description |
|-----------|-------------|
| `EXECUTE` | Call function/procedure |
| `ALL` | Same as EXECUTE |

### Domains

| Privilege | Description |
|-----------|-------------|
| `USAGE` | Use domain in column definitions |
| `ALL` | Same as USAGE |

### Foreign Data Wrappers

| Privilege | Description |
|-----------|-------------|
| `USAGE` | Create foreign servers using FDW |
| `ALL` | Same as USAGE |

### Foreign Servers

| Privilege | Description |
|-----------|-------------|
| `USAGE` | Create foreign tables using server |
| `ALL` | Same as USAGE |

### Large Objects (BLOBs)

| Privilege | Description |
|-----------|-------------|
| `SELECT` | Read large object |
| `UPDATE` | Write large object |
| `ALL` | Full access |

## WITH GRANT OPTION

The `WITH GRANT OPTION` allows a grantee to grant the same privilege to others:

```sql
-- Alice grants SELECT to Bob with grant option
GRANT SELECT ON table TO bob WITH GRANT OPTION;

-- Bob can now grant SELECT to others
GRANT SELECT ON table TO charlie;  -- Valid
```

**Propagation Rules:**
- Grant option is separate from the privilege itself
- Revoking from grantor cascades to grantees
- `CASCADE` revokes dependent grants
- `RESTRICT` fails if dependent grants exist

## Privilege Checking Algorithm

```
Input: user, object, privilege
Output: Boolean - has permission

1. CHECK if user is owner
   if user == object.owner:
       return true  // Owners have all privileges

2. CHECK if user is superuser
   if user.hasAttribute(SUPERUSER):
       return true  // Superusers bypass all checks

3. CHECK direct grant
   grant = findGrant(user, object, privilege)
   if grant exists:
       return true

4. CHECK role inheritance
   for role in user.roles:
       if checkPermission(role, object, privilege):
           return true

5. CHECK PUBLIC grant
   public_grant = findGrant(PUBLIC, object, privilege)
   if public_grant exists:
       return true

6. RETURN false
```

## Column-Level Checking

```
Input: user, table, column, privilege
Output: Boolean - has permission

1. CHECK table-level privilege
   if hasPermission(user, table, privilege):
       return true  // Table privilege covers all columns

2. CHECK column-level grant
   col_grant = findColumnGrant(user, table, column, privilege)
   if col_grant exists:
       return true

3. CHECK role column grants
   for role in user.roles:
       if findColumnGrant(role, table, column, privilege):
           return true

4. CHECK PUBLIC column grant
   if findColumnGrant(PUBLIC, table, column, privilege):
       return true

5. RETURN false
```

## ALL PRIVILEGES Expansion

```sql
-- ALL PRIVILEGES expands differently per object type

-- Table: SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER
GRANT ALL ON table TO user;

-- Sequence: USAGE, SELECT, UPDATE
GRANT ALL ON sequence TO user;

-- Schema: USAGE, CREATE
GRANT ALL ON schema TO user;

-- Database: CREATE, CONNECT, TEMPORARY
GRANT ALL ON DATABASE db TO user;

-- Function: EXECUTE
GRANT ALL ON FUNCTION func() TO user;
```

## Privilege Matrix

| Object Type | SELECT | INSERT | UPDATE | DELETE | EXECUTE | USAGE | CREATE | CONNECT | TEMP |
|-------------|--------|--------|--------|--------|---------|-------|--------|---------|------|
| Table | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| Column | ✅ | N/A | ✅ | N/A | ❌ | ❌ | ❌ | ❌ | ❌ |
| View | ✅ | ✅* | ✅* | ✅* | ❌ | ❌ | ❌ | ❌ | ❌ |
| Sequence | ✅ | ❌ | ✅ | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ |
| Schema | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ❌ | ❌ |
| Database | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |
| Function | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ |
| Domain | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ |

*Only for updatable views

## SQL Examples

```sql
-- Basic grants
GRANT SELECT ON employees TO hr_user;
GRANT SELECT, INSERT, UPDATE ON employees TO manager;
GRANT ALL ON employees TO admin WITH GRANT OPTION;

-- Column-level grants
GRANT SELECT (name, email) ON employees TO public;
GRANT UPDATE (salary) ON employees TO payroll;

-- Schema grants
GRANT USAGE ON SCHEMA app_schema TO app_user;
GRANT CREATE ON SCHEMA app_schema TO app_admin;

-- Database grants
GRANT CONNECT ON DATABASE production TO app_user;
GRANT TEMPORARY ON DATABASE production TO analyst;

-- Function grants
GRANT EXECUTE ON FUNCTION calculate_tax(numeric) TO public;

-- Role grants
GRANT analyst TO john;
GRANT manager TO jane WITH ADMIN OPTION;

-- Revoke
REVOKE INSERT ON employees FROM manager;
REVOKE ALL ON employees FROM admin CASCADE;
```

## Invariants

1. **Owner Has All**: Object owners implicitly have all privileges
   - Verification: Owner check before grant lookup

2. **Superuser Bypass**: SUPERUSER bypasses all privilege checks
   - Verification: Superuser check in permission evaluation

3. **Column Subset**: Column privileges are subset of table privileges
   - Verification: Table-level grants override column-level

4. **Grant Option Required**: Must have GRANT OPTION to grant to others
   - Verification: Check grant_option flag

## Related Specifications

- `authorization_model.md` - Overall authorization model
- `acl_format.md` - ACL storage format
- `default_privileges.md` - Default privileges
- `cls_column_masking.md` - Column-level security

## Appendix

### PostgreSQL Compatibility

ScratchBird privilege types are compatible with PostgreSQL 14+:
- Same privilege names
- Same bitmask values for common privileges
- Compatible GRANT/REVOKE syntax

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
