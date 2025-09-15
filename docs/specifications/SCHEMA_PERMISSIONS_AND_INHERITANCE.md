# Schema Permissions and Hierarchical Rights Management

## Overview

ScratchBird implements granular, hierarchical schema permissions that allow fine-grained control over what users can do within schemas and their child schemas. This goes beyond traditional database permissions by providing schema-specific rights that cascade through the schema hierarchy.

## Schema Permission Model

### Permission Categories

```sql
-- Schema-level permissions are divided into categories:

1. DDL Rights (Data Definition Language)
   - CREATE_TABLE
   - ALTER_TABLE  
   - DROP_TABLE
   - CREATE_INDEX
   - DROP_INDEX
   - CREATE_VIEW
   - DROP_VIEW
   - CREATE_PROCEDURE
   - DROP_PROCEDURE
   - CREATE_TRIGGER
   - DROP_TRIGGER
   - CREATE_SEQUENCE
   - DROP_SEQUENCE
   - CREATE_TYPE
   - DROP_TYPE

2. DML Rights (Data Manipulation Language)
   - SELECT
   - INSERT
   - UPDATE
   - DELETE
   - TRUNCATE
   - EXECUTE

3. Schema Management Rights
   - CREATE_SCHEMA    -- Create child schemas
   - ALTER_SCHEMA     -- Modify schema properties
   - DROP_SCHEMA      -- Drop child schemas
   - GRANT_RIGHTS     -- Grant permissions to others
   - REVOKE_RIGHTS    -- Revoke permissions

4. Utility Rights
   - SHOW_OBJECTS     -- View schema contents
   - ANALYZE          -- Run ANALYZE on tables
   - VACUUM           -- Run VACUUM on tables
   - BACKUP           -- Backup schema
   - RESTORE          -- Restore schema
   - MONITOR          -- View statistics
```

### Grant Syntax

```sql
-- Grant specific rights on schema
GRANT permission_list 
  ON SCHEMA schema_name 
  TO user_or_role
  [WITH INHERITANCE {CASCADE | LOCAL}]
  [WITH GRANT OPTION];

-- Examples:

-- John can read/alter data but not perform DDL
GRANT SELECT, INSERT, UPDATE, DELETE 
  ON SCHEMA sales 
  TO john 
  WITH INHERITANCE CASCADE;

-- Jake can create tables but not perform CRUD
GRANT CREATE_TABLE, ALTER_TABLE, DROP_TABLE, 
      CREATE_INDEX, DROP_INDEX 
  ON SCHEMA development 
  TO jake 
  WITH INHERITANCE LOCAL;

-- Mixed permissions
GRANT SELECT, CREATE_VIEW 
  ON SCHEMA reporting 
  TO analyst_role 
  WITH INHERITANCE CASCADE;
```

## Hierarchical Schema Structure

### Schema Hierarchy Example

```
[root]
├── [sys]           -- System schemas
├── [app]           -- Application schemas
│   ├── sales       -- Sales application
│   │   ├── orders  -- Orders subschema
│   │   └── reports -- Reports subschema
│   └── inventory   -- Inventory application
│       ├── products
│       └── warehouses
└── [users]         -- User schemas
    ├── john
    └── jake
```

### Inheritance Rules

```sql
-- CASCADE inheritance: Rights apply to all descendant schemas
GRANT SELECT ON SCHEMA app TO reader_role 
  WITH INHERITANCE CASCADE;
-- reader_role can SELECT from app, app.sales, app.sales.orders, etc.

-- LOCAL inheritance: Rights apply only to immediate children
GRANT CREATE_TABLE ON SCHEMA app TO developer_role 
  WITH INHERITANCE LOCAL;
-- developer_role can create tables in app.sales and app.inventory
-- but NOT in app.sales.orders

-- NO inheritance: Rights apply only to specified schema
GRANT DELETE ON SCHEMA app.sales.orders TO admin_role 
  WITH INHERITANCE NONE;
-- admin_role can only DELETE from app.sales.orders specifically
```

## Permission Resolution

### Resolution Algorithm

```sql
-- Permission check follows this precedence:
1. Explicit DENY (if implemented)
2. Explicit GRANT on exact schema
3. Inherited GRANT from parent schema (CASCADE)
4. Inherited GRANT from immediate parent (LOCAL)
5. Role-based permissions
6. Default permissions
7. DENY (if no grants found)

-- Example resolution:
-- User 'john' wants to SELECT from app.sales.orders.line_items

CHECK: Explicit grant on app.sales.orders.line_items? No
CHECK: Inherited from app.sales.orders (CASCADE)? No  
CHECK: Inherited from app.sales (CASCADE)? Yes - GRANTED
```

### Effective Permissions View

```sql
-- View effective permissions for a user on a schema
CREATE VIEW effective_schema_permissions AS
SELECT 
    s.schema_path,
    u.username,
    p.permission_type,
    p.granted_directly,
    p.inherited_from,
    p.inheritance_type,
    p.grant_option
FROM 
    sys.schemas s
    CROSS JOIN sys.users u
    LEFT JOIN sys.schema_permissions p 
        ON s.schema_id = p.schema_id 
        AND u.user_id = p.grantee_id;

-- Query effective permissions
SELECT * FROM effective_schema_permissions
WHERE username = 'john' 
  AND schema_path LIKE 'app.sales%';
```

## Advanced Permission Patterns

### Role-Based Schema Access

```sql
-- Create roles for different access patterns
CREATE ROLE schema_reader;
CREATE ROLE schema_writer;
CREATE ROLE schema_admin;
CREATE ROLE schema_developer;

-- Grant schema permissions to roles
GRANT SELECT, SHOW_OBJECTS 
  ON SCHEMA app 
  TO schema_reader 
  WITH INHERITANCE CASCADE;

GRANT SELECT, INSERT, UPDATE, DELETE 
  ON SCHEMA app 
  TO schema_writer 
  WITH INHERITANCE CASCADE;

GRANT ALL DDL RIGHTS 
  ON SCHEMA app 
  TO schema_developer 
  WITH INHERITANCE LOCAL;

GRANT ALL RIGHTS 
  ON SCHEMA app 
  TO schema_admin 
  WITH INHERITANCE CASCADE 
  WITH GRANT OPTION;

-- Assign roles to users
GRANT schema_reader TO john;
GRANT schema_developer TO jake;
GRANT schema_admin TO admin_user;
```

### Conditional Permissions

```sql
-- Time-based permissions
GRANT SELECT ON SCHEMA financial 
  TO auditor 
  WITH INHERITANCE CASCADE
  VALID FROM '2024-01-01' TO '2024-12-31';

-- Connection-based permissions
GRANT ALL DML RIGHTS ON SCHEMA production 
  TO app_user
  WITH INHERITANCE CASCADE
  WHEN CONNECTION FROM '10.0.0.0/24';  -- Only from internal network

-- Row-level security integration
GRANT SELECT ON SCHEMA customers 
  TO sales_rep
  WITH INHERITANCE CASCADE
  WITH ROW FILTER (region = CURRENT_USER_REGION());
```

### Schema Permission Templates

```sql
-- Define permission templates
CREATE PERMISSION TEMPLATE readonly_template AS
  SELECT, SHOW_OBJECTS;

CREATE PERMISSION TEMPLATE dataentry_template AS
  SELECT, INSERT, UPDATE, SHOW_OBJECTS;

CREATE PERMISSION TEMPLATE developer_template AS
  ALL DDL RIGHTS, SELECT, SHOW_OBJECTS;

-- Apply templates
GRANT TEMPLATE readonly_template 
  ON SCHEMA reporting 
  TO analysts 
  WITH INHERITANCE CASCADE;

GRANT TEMPLATE developer_template 
  ON SCHEMA development 
  TO developers 
  WITH INHERITANCE LOCAL;
```

## Cross-Schema Permissions

### Schema Traversal Rights

```sql
-- USAGE permission allows traversing through a schema
GRANT USAGE ON SCHEMA app TO public;
-- Users can "pass through" app to reach app.sales if they have rights there

-- Without USAGE, even with rights on child, access is blocked
REVOKE USAGE ON SCHEMA secure FROM public;
GRANT SELECT ON SCHEMA secure.public_data TO public;
-- public cannot access secure.public_data without USAGE on secure
```

### Cross-Schema References

```sql
-- Permission to reference objects across schemas
GRANT REFERENCES ON SCHEMA master_data 
  TO app_schemas 
  WITH INHERITANCE CASCADE;

-- Now schemas under app_schemas can create foreign keys to master_data tables
CREATE TABLE app.sales.orders (
    customer_id INT REFERENCES master_data.customers(id)  -- Allowed
);
```

## Default Permissions

### Schema Creation Defaults

```sql
-- Set default permissions for new schemas
ALTER DEFAULT PRIVILEGES 
  FOR SCHEMAS IN SCHEMA app
  GRANT SELECT TO readonly_role;

-- When any user creates a schema under app, readonly_role gets SELECT

-- Set defaults for specific users
ALTER DEFAULT PRIVILEGES 
  FOR USER developer
  IN SCHEMA app
  GRANT ALL DDL RIGHTS TO developer_team;
```

### Permission Inheritance Policies

```sql
-- Define inheritance policies
CREATE POLICY schema_inheritance_policy AS
  DEFAULT INHERITANCE CASCADE
  FOR DDL RIGHTS USE LOCAL
  FOR GRANT_RIGHTS USE NONE;

-- Apply policy to schema tree
ALTER SCHEMA app 
  SET INHERITANCE POLICY schema_inheritance_policy;
```

## System Tables for Schema Permissions

```sql
-- Core permission storage
CREATE TABLE sys.schema_permissions (
    permission_id UUID PRIMARY KEY,
    schema_id UUID NOT NULL REFERENCES sys.schemas(schema_id),
    grantee_id UUID NOT NULL,  -- User or role ID
    grantee_type CHAR(1),      -- 'U' for user, 'R' for role
    permission_type VARCHAR(50),
    inheritance_type VARCHAR(10), -- CASCADE, LOCAL, NONE
    grant_option BOOLEAN,
    granted_by UUID,
    granted_at TIMESTAMP,
    valid_from TIMESTAMP,
    valid_until TIMESTAMP,
    conditions JSONB,          -- Additional conditions
    UNIQUE(schema_id, grantee_id, permission_type)
);

-- Permission inheritance cache
CREATE TABLE sys.schema_permission_cache (
    cache_id UUID PRIMARY KEY,
    schema_id UUID NOT NULL,
    user_id UUID NOT NULL,
    permission_type VARCHAR(50),
    is_granted BOOLEAN,
    inherited_from UUID,       -- Schema where permission originated
    cache_timestamp TIMESTAMP,
    UNIQUE(schema_id, user_id, permission_type)
);

-- Audit trail
CREATE TABLE sys.schema_permission_audit (
    audit_id UUID PRIMARY KEY,
    action VARCHAR(10),        -- GRANT, REVOKE
    schema_id UUID,
    grantee_id UUID,
    permission_type VARCHAR(50),
    performed_by UUID,
    performed_at TIMESTAMP,
    details JSONB
);
```

## Permission Management Functions

```sql
-- Check if user has permission
CREATE FUNCTION has_schema_permission(
    p_user_id ID,
    p_schema_path TEXT,
    p_permission TEXT
) RETURNS BOOLEAN AS $
BEGIN
    -- Check cache first
    -- Then check explicit grants
    -- Then check inherited permissions
    -- Then check role permissions
    RETURN check_permission_cascade(p_user_id, p_schema_path, p_permission);
END;
$ LANGUAGE plpgsql;

-- Get all permissions for user on schema
CREATE FUNCTION get_schema_permissions(
    p_user_id ID,
    p_schema_path TEXT
) RETURNS TABLE(permission_type TEXT, source TEXT) AS $
BEGIN
    RETURN QUERY
    SELECT 
        permission_type,
        CASE 
            WHEN inherited_from IS NULL THEN 'Direct'
            ELSE 'Inherited from ' || inherited_from
        END as source
    FROM sys.schema_permission_cache
    WHERE user_id = p_user_id
      AND schema_id = get_schema_id(p_schema_path)
      AND is_granted = true;
END;
$ LANGUAGE plpgsql;

-- Grant with cascade
CREATE PROCEDURE grant_schema_permission_cascade(
    p_permission TEXT,
    p_schema_path TEXT,
    p_grantee TEXT,
    p_inheritance TEXT DEFAULT 'CASCADE'
) AS $$
BEGIN
    -- Grant to specified schema
    -- If CASCADE, grant to all descendants
    -- If LOCAL, grant to immediate children
    -- Update permission cache
END;
$$ LANGUAGE plpgsql;
```

## Usage Examples

### Example 1: Department-Based Access

```sql
-- Sales department structure
CREATE SCHEMA sales;
CREATE SCHEMA sales.orders;
CREATE SCHEMA sales.reports;
CREATE SCHEMA sales.archive;

-- Sales staff can do everything except DDL in active schemas
GRANT ALL DML RIGHTS ON SCHEMA sales 
  TO sales_staff 
  WITH INHERITANCE CASCADE;

-- But only SELECT on archive
REVOKE ALL ON SCHEMA sales.archive FROM sales_staff;
GRANT SELECT ON SCHEMA sales.archive TO sales_staff;

-- Sales managers can also create views and procedures
GRANT CREATE_VIEW, CREATE_PROCEDURE 
  ON SCHEMA sales.reports 
  TO sales_managers;

-- IT can do DDL but not see data
GRANT ALL DDL RIGHTS ON SCHEMA sales 
  TO it_staff 
  WITH INHERITANCE CASCADE;
REVOKE ALL DML RIGHTS ON SCHEMA sales 
  FROM it_staff 
  CASCADE;
```

### Example 2: Multi-Tenant Application

```sql
-- Tenant isolation with schema permissions
CREATE SCHEMA tenants;
CREATE SCHEMA tenants.acme_corp;
CREATE SCHEMA tenants.globex_inc;

-- Each tenant user can only access their schema
GRANT ALL RIGHTS ON SCHEMA tenants.acme_corp 
  TO acme_users 
  WITH INHERITANCE CASCADE;

GRANT ALL RIGHTS ON SCHEMA tenants.globex_inc 
  TO globex_users 
  WITH INHERITANCE CASCADE;

-- Shared read-only data
CREATE SCHEMA shared_data;
GRANT SELECT ON SCHEMA shared_data 
  TO PUBLIC 
  WITH INHERITANCE CASCADE;
```

### Example 3: Development Lifecycle

```sql
-- Environment-based permissions
CREATE SCHEMA dev;
CREATE SCHEMA staging;
CREATE SCHEMA production;

-- Developers have full access to dev
GRANT ALL RIGHTS ON SCHEMA dev 
  TO developers 
  WITH INHERITANCE CASCADE;

-- Limited access to staging
GRANT SELECT, EXECUTE ON SCHEMA staging 
  TO developers 
  WITH INHERITANCE CASCADE;

-- No access to production
-- Production access only through roles
GRANT SELECT ON SCHEMA production 
  TO prod_readonly_role 
  WITH INHERITANCE CASCADE;

GRANT ALL DML RIGHTS ON SCHEMA production 
  TO prod_app_role 
  WITH INHERITANCE CASCADE;
```

## Security Considerations

### Permission Denial

```sql
-- Explicit DENY (overrides all GRANTs)
DENY DELETE ON SCHEMA financial 
  TO ALL EXCEPT financial_admin 
  CASCADE;

-- Even if user has DELETE through a role, DENY takes precedence
```

### Permission Limits

```sql
-- Maximum permissions per schema
ALTER SYSTEM SET max_schema_permissions = 1000;

-- Permission cache size
ALTER SYSTEM SET schema_permission_cache_size = '256MB';

-- Permission check timeout
ALTER SYSTEM SET permission_check_timeout = '100ms';
```

### Audit Requirements

```sql
-- Enable permission auditing
ALTER SCHEMA sensitive_data 
  SET AUDIT_PERMISSIONS = TRUE;

-- All permission changes logged to sys.schema_permission_audit
```

## Migration and Compatibility

### From Traditional Model

```sql
-- Migration helper function
CREATE FUNCTION migrate_table_permissions_to_schema() RETURNS VOID AS $$
DECLARE
    r RECORD;
BEGIN
    -- Convert table-level grants to schema-level
    FOR r IN 
        SELECT DISTINCT 
            schema_name, 
            grantee, 
            privilege_type
        FROM information_schema.table_privileges
    LOOP
        EXECUTE format(
            'GRANT %s ON SCHEMA %s TO %s WITH INHERITANCE CASCADE',
            r.privilege_type,
            r.schema_name,
            r.grantee
        );
    END LOOP;
END;
$$ LANGUAGE plpgsql;
```

## Performance Optimizations

### Permission Caching

```sql
-- Aggressive caching for permission checks
CREATE INDEX idx_perm_cache_lookup 
  ON sys.schema_permission_cache(user_id, schema_id, permission_type);

-- Refresh cache periodically
CREATE PROCEDURE refresh_permission_cache() AS $$
BEGIN
    TRUNCATE sys.schema_permission_cache;
    INSERT INTO sys.schema_permission_cache
    SELECT * FROM calculate_all_effective_permissions();
END;
$$ LANGUAGE plpgsql;

-- Schedule refresh
CREATE EVENT refresh_permissions
  ON SCHEDULE EVERY 5 MINUTES
  DO CALL refresh_permission_cache();
```

This comprehensive schema permission system provides the granular control needed for complex applications while maintaining performance through intelligent caching and inheritance.