### DDL: Roles, Users, and Grants

**What it is**

Roles, users, and grants form the security foundation of the database system. Roles are collections of privileges that can be granted to users or other roles. Users are database accounts that can connect and perform operations. Grants assign specific privileges on database objects to roles and users, implementing fine-grained access control.

**Why it matters**

- **Security**: Enforce principle of least privilege
- **Compliance**: Meet regulatory requirements for data access
- **Multi-tenancy**: Isolate data between different applications or customers
- **Audit Trail**: Track who can access what data
- **Delegation**: Allow controlled administrative capabilities

**How to use it**

Create roles to group related privileges, create users for authentication, then grant appropriate permissions. Use role hierarchies for complex permission structures. Regularly audit and revoke unnecessary privileges.

## Roles

### CREATE ROLE

```sql
-- Basic role creation
CREATE ROLE role_name;

-- Role with login capability
CREATE ROLE app_user LOGIN;

-- Role with specific attributes
CREATE ROLE admin_role
    LOGIN
    SUPERUSER
    CREATEDB
    CREATEROLE
    INHERIT
    REPLICATION;

-- Application roles
CREATE ROLE read_only;
CREATE ROLE read_write;
CREATE ROLE data_analyst;
CREATE ROLE app_service;
CREATE ROLE backup_operator;

-- Role with connection limit
CREATE ROLE limited_user
    LOGIN
    CONNECTION LIMIT 5;

-- Role with password
CREATE ROLE secure_user
    LOGIN
    PASSWORD 'StrongPassword123!';

-- Role valid for limited time
CREATE ROLE temp_contractor
    LOGIN
    VALID UNTIL '2024-12-31';
```

### ALTER ROLE

```sql
-- Grant login capability
ALTER ROLE read_only LOGIN;

-- Change password
ALTER ROLE secure_user PASSWORD 'NewPassword456!';

-- Set connection limit
ALTER ROLE app_service CONNECTION LIMIT 100;

-- Grant administrative privileges
ALTER ROLE admin_role SUPERUSER CREATEDB;

-- Set role validity
ALTER ROLE temp_contractor VALID UNTIL '2025-06-30';

-- Remove privileges
ALTER ROLE former_admin NOSUPERUSER NOCREATEDB;

-- Rename role
ALTER ROLE old_name RENAME TO new_name;
```

### DROP ROLE

```sql
-- Drop role
DROP ROLE obsolete_role;

-- Drop if exists
DROP ROLE IF EXISTS temporary_role;

-- Drop with cascade (revokes all grants first)
DROP ROLE app_role CASCADE;
```

### Role Membership

```sql
-- Grant role to another role
GRANT read_only TO data_analyst;
GRANT read_write TO app_service;

-- Create role hierarchy
CREATE ROLE base_user;
CREATE ROLE power_user;
CREATE ROLE admin_user;

GRANT base_user TO power_user;
GRANT power_user TO admin_user;

-- Grant multiple roles
GRANT read_only, data_analyst TO john_doe;

-- With admin option (can grant to others)
GRANT manager_role TO team_lead WITH ADMIN OPTION;

-- Revoke role membership
REVOKE read_write FROM former_employee;
```

## Users

### CREATE USER

```sql
-- Basic user (shorthand for CREATE ROLE with LOGIN)
CREATE USER username PASSWORD 'password';

-- User with specific attributes
CREATE USER john_doe
    PASSWORD 'SecurePass123!'
    VALID UNTIL '2025-01-01'
    CONNECTION LIMIT 3;

-- Service account
CREATE USER app_service_account
    PASSWORD 'ServicePassword789!'
    CONNECTION LIMIT 50
    NOSUPERUSER
    NOCREATEDB;

-- Read-only user
CREATE USER report_viewer
    PASSWORD 'ViewOnly456!'
    IN ROLE read_only;

-- Administrative user
CREATE USER db_admin
    PASSWORD 'AdminPass321!'
    SUPERUSER
    CREATEDB
    CREATEROLE;
```

### ALTER USER

```sql
-- Change password
ALTER USER john_doe PASSWORD 'NewSecurePass789!';

-- Extend validity
ALTER USER temp_user VALID UNTIL '2025-12-31';

-- Change connection limit
ALTER USER app_service_account CONNECTION LIMIT 100;

-- Grant privileges
ALTER USER developer CREATEDB;

-- Rename user
ALTER USER old_username RENAME TO new_username;

-- Set default transaction isolation
ALTER USER sensitive_user SET DEFAULT TRANSACTION ISOLATION LEVEL SERIALIZABLE;
```

### DROP USER

```sql
-- Drop user
DROP USER former_employee;

-- Drop if exists
DROP USER IF EXISTS temp_user;

-- Drop with cascade
DROP USER contractor CASCADE;
```

## Grants

### Database Privileges

```sql
-- Grant all privileges on database
GRANT ALL PRIVILEGES ON DATABASE production TO admin_role;

-- Grant specific database privileges
GRANT CREATE, CONNECT ON DATABASE development TO developer;
GRANT CONNECT ON DATABASE reporting TO data_analyst;

-- Grant temporary access
GRANT TEMPORARY ON DATABASE staging TO tester_role;

-- Revoke database privileges
REVOKE CREATE ON DATABASE production FROM junior_dev;
REVOKE ALL PRIVILEGES ON DATABASE old_db FROM PUBLIC;
```

### Schema Privileges

```sql
-- Grant schema privileges
GRANT ALL ON SCHEMA public TO admin_role;
GRANT USAGE ON SCHEMA analytics TO data_analyst;
GRANT CREATE ON SCHEMA staging TO developer;

-- Grant on all schemas
GRANT USAGE ON ALL SCHEMAS IN DATABASE main TO read_only;

-- Future schemas (default privileges)
ALTER DEFAULT PRIVILEGES 
    GRANT USAGE ON SCHEMAS TO read_only;
```

### Table Privileges

```sql
-- Grant all table privileges
GRANT ALL ON TABLE customers TO admin_role;

-- Grant specific privileges
GRANT SELECT ON TABLE products TO read_only;
GRANT SELECT, INSERT, UPDATE ON TABLE orders TO app_service;
GRANT SELECT, DELETE ON TABLE temp_data TO cleanup_role;

-- Grant on all tables in schema
GRANT SELECT ON ALL TABLES IN SCHEMA public TO read_only;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA app TO app_service;

-- Column-level privileges
GRANT SELECT (id, name, email) ON TABLE users TO support_role;
GRANT UPDATE (status, last_login) ON TABLE users TO app_service;

-- With grant option
GRANT SELECT ON TABLE reference_data TO team_lead WITH GRANT OPTION;

-- Future tables (default privileges)
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT ON TABLES TO read_only;

ALTER DEFAULT PRIVILEGES FOR ROLE app_owner IN SCHEMA app
    GRANT ALL ON TABLES TO app_service;
```

### Sequence Privileges

```sql
-- Grant sequence privileges
GRANT USAGE ON SEQUENCE order_id_seq TO app_service;
GRANT ALL ON SEQUENCE customer_id_seq TO admin_role;

-- Grant on all sequences
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO app_service;

-- Future sequences
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT USAGE ON SEQUENCES TO app_service;
```

### Function and Procedure Privileges

```sql
-- Grant execute privilege
GRANT EXECUTE ON FUNCTION calculate_tax TO app_service;
GRANT EXECUTE ON PROCEDURE process_payment TO payment_service;

-- Grant on all functions
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA utils TO developer;
GRANT EXECUTE ON ALL PROCEDURES IN SCHEMA business TO app_service;

-- Future routines
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT EXECUTE ON FUNCTIONS TO app_service;
```

### View Privileges

```sql
-- Grant view privileges
GRANT SELECT ON VIEW customer_summary TO report_viewer;
GRANT SELECT ON VIEW active_orders TO support_role;

-- Grant on materialized view
GRANT SELECT ON MATERIALIZED VIEW sales_dashboard TO analyst_role;

-- All views in schema
GRANT SELECT ON ALL VIEWS IN SCHEMA reporting TO report_viewer;
```

## Advanced Security Patterns

### Role-Based Access Control (RBAC)

```sql
-- Create role hierarchy
CREATE ROLE base_permissions;
CREATE ROLE read_permissions;
CREATE ROLE write_permissions;
CREATE ROLE admin_permissions;

-- Define base permissions
GRANT USAGE ON SCHEMA public TO base_permissions;
GRANT CONNECT ON DATABASE main TO base_permissions;

-- Read permissions inherit from base
GRANT base_permissions TO read_permissions;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO read_permissions;

-- Write permissions inherit from read
GRANT read_permissions TO write_permissions;
GRANT INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO write_permissions;

-- Admin inherits all
GRANT write_permissions TO admin_permissions;
GRANT CREATE ON SCHEMA public TO admin_permissions;

-- Assign to users
CREATE USER viewer IN ROLE read_permissions;
CREATE USER editor IN ROLE write_permissions;
CREATE USER admin IN ROLE admin_permissions;
```

### Multi-Tenant Security

```sql
-- Create tenant-specific schemas
CREATE SCHEMA tenant_001;
CREATE SCHEMA tenant_002;

-- Create tenant roles
CREATE ROLE tenant_001_role;
CREATE ROLE tenant_002_role;

-- Grant schema access
GRANT ALL ON SCHEMA tenant_001 TO tenant_001_role;
GRANT ALL ON SCHEMA tenant_002 TO tenant_002_role;

-- Create tenant users
CREATE USER tenant_001_user PASSWORD 'Pass1!' IN ROLE tenant_001_role;
CREATE USER tenant_002_user PASSWORD 'Pass2!' IN ROLE tenant_002_role;

-- Ensure isolation
REVOKE ALL ON SCHEMA tenant_001 FROM tenant_002_role;
REVOKE ALL ON SCHEMA tenant_002 FROM tenant_001_role;
```

### Audit and Compliance

```sql
-- Create audit role with read-only access
CREATE ROLE auditor;
GRANT CONNECT ON DATABASE production TO auditor;
GRANT USAGE ON ALL SCHEMAS IN DATABASE production TO auditor;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO auditor;

-- Create compliance role with specific access
CREATE ROLE compliance_officer;
GRANT SELECT ON TABLE audit_log TO compliance_officer;
GRANT SELECT ON TABLE user_activity TO compliance_officer;
GRANT EXECUTE ON FUNCTION generate_compliance_report TO compliance_officer;

-- Sensitive data protection
CREATE ROLE pii_reader;
GRANT SELECT (id, created_at) ON TABLE users TO PUBLIC;  -- Non-sensitive
GRANT SELECT ON TABLE users TO pii_reader;  -- Full access for authorized role
```

## Security Best Practices

### Password Management

```sql
-- Strong password policy
CREATE USER secure_app
    PASSWORD 'C0mpl3x!P@ssw0rd#2024'
    VALID UNTIL '2024-12-31';

-- Regular password rotation
ALTER USER secure_app PASSWORD 'N3w!P@ssw0rd#2024';

-- Service account with certificate auth (if supported)
CREATE USER cert_user
    WITH CERTIFICATE 'CN=app.example.com';
```

### Principle of Least Privilege

```sql
-- Start with minimal permissions
CREATE ROLE minimal_role;
GRANT CONNECT ON DATABASE app TO minimal_role;
GRANT USAGE ON SCHEMA public TO minimal_role;

-- Add only required permissions
GRANT SELECT ON TABLE products TO minimal_role;
GRANT SELECT ON TABLE categories TO minimal_role;
-- Don't grant unnecessary access
```

### Regular Audits

```sql
-- Review role memberships
SELECT 
    r1.rolname AS role,
    r2.rolname AS member
FROM pg_auth_members m
JOIN pg_roles r1 ON m.roleid = r1.oid
JOIN pg_roles r2 ON m.member = r2.oid
ORDER BY r1.rolname, r2.rolname;

-- Review table privileges
SELECT 
    schemaname,
    tablename,
    grantee,
    privilege_type
FROM information_schema.table_privileges
WHERE schemaname = 'public'
ORDER BY tablename, grantee;

-- Find users with superuser privilege
SELECT rolname FROM pg_roles WHERE rolsuper = true;
```

## REVOKE Operations

```sql
-- Revoke specific privileges
REVOKE SELECT ON TABLE sensitive_data FROM PUBLIC;
REVOKE INSERT, UPDATE ON TABLE audit_log FROM app_service;

-- Revoke all privileges
REVOKE ALL PRIVILEGES ON TABLE temp_table FROM user_name;
REVOKE ALL ON DATABASE test_db FROM developer;

-- Revoke with cascade
REVOKE SELECT ON TABLE orders FROM manager_role CASCADE;

-- Revoke grant option
REVOKE GRANT OPTION FOR SELECT ON TABLE products FROM team_lead;

-- Revoke role membership
REVOKE admin_role FROM former_admin;
REVOKE read_write FROM terminated_user;
```

## Implementation Details

**Parser** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_role`: CREATE/ALTER/DROP ROLE
- `parse_ddl_user`: CREATE/ALTER/DROP USER  
- `parse_ddl_grant`: GRANT statements
- `parse_ddl_revoke`: REVOKE statements

**AST Structure** (`include/scratchbird/engine/ast.h`):
```cpp
struct DdlRoleAst {
    std::string name;
    std::vector<std::string> attributes;  // LOGIN, SUPERUSER, etc
    std::optional<std::string> password;
    std::optional<int> connection_limit;
};

struct DdlGrantAst {
    std::vector<std::string> privileges;
    std::string object_type;  // TABLE, SCHEMA, etc
    std::string object_name;
    std::vector<std::string> grantees;
    bool with_grant_option;
};
```

**Code Anchors**:
- Role parser: `src/engine/parser_ddl.cpp` (parse_ddl_role)
- User parser: `src/engine/parser_ddl.cpp` (parse_ddl_user)
- Grant parser: `src/engine/parser_ddl.cpp` (parse_ddl_grant)
- Revoke parser: `src/engine/parser_ddl.cpp` (parse_ddl_revoke)
- AST definitions: `include/scratchbird/engine/ast.h`

## See also

- [Schemas](./ddl-schemas.md) - Schema-level organization
- [Tables](./ddl-tables.md) - Table-level permissions
- [Views](./ddl-views.md) - View-based security
- [RLS Policies](./ddl-policies-rls.md) - Row-level security
- [Session & Transaction](./session-and-transaction.md) - SET ROLE operations