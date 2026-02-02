[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL - Security (DCL)

**Status:** Alpha documentation
**Last Updated:** 2026-01-30

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

---

## Overview

This document covers Data Control Language (DCL) statements in PostgreSQL emulation mode. DCL statements manage database security through users, roles, and privileges. ScratchBird maps PostgreSQL security concepts to its native security model.

**Spec refs:**
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`

---

## Roles and Users

In PostgreSQL, users and roles are essentially the same thing. A "user" is a role with the LOGIN privilege. ScratchBird follows this model.

### CREATE ROLE

Create a new role (without login capability by default).

**Syntax:**
```sql
CREATE ROLE role_name [WITH option [...]];
```

**Options:**
| Option | Description |
|--------|-------------|
| `SUPERUSER` / `NOSUPERUSER` | Superuser status (default: NOSUPERUSER) |
| `CREATEDB` / `NOCREATEDB` | Can create databases (default: NOCREATEDB) |
| `CREATEROLE` / `NOCREATEROLE` | Can create roles (default: NOCREATEROLE) |
| `LOGIN` / `NOLOGIN` | Can log in (default: NOLOGIN) |
| `REPLICATION` / `NOREPLICATION` | Replication role (default: NOREPLICATION) |
| `INHERIT` / `NOINHERIT` | Inherit privileges (default: INHERIT) |
| `PASSWORD 'password'` | Set password |
| `VALID UNTIL 'timestamp'` | Password expiration |
| `IN ROLE role_name [, ...]` | Add to existing roles |
| `ROLE role_name [, ...]` | Members of this role |
| `ADMIN role_name [, ...]` | Members with admin option |
| `CONNECTION LIMIT n` | Max connections (-1 = unlimited) |

**Examples:**
```sql
-- Basic role (no login)
CREATE ROLE readonly;

-- Role with options
CREATE ROLE app_admin WITH
    LOGIN
    CREATEDB
    PASSWORD 'secure_password'
    CONNECTION LIMIT 10;

-- Role that inherits from another
CREATE ROLE senior_dev WITH
    LOGIN
    PASSWORD 'password'
    IN ROLE developers;

-- Role with expiring password
CREATE ROLE temp_user WITH
    LOGIN
    PASSWORD 'temporary'
    VALID UNTIL '2026-12-31';
```

### CREATE USER

Create a role with LOGIN privilege (convenience syntax).

**Syntax:**
```sql
CREATE USER user_name [WITH option [...]];
```

**Examples:**
```sql
-- Basic user (equivalent to CREATE ROLE ... WITH LOGIN)
CREATE USER john WITH PASSWORD 'password123';

-- User with options
CREATE USER app_service WITH
    PASSWORD 'service_pass'
    NOCREATEDB
    NOCREATEROLE
    CONNECTION LIMIT 50;
```

### ALTER ROLE / ALTER USER

Modify role attributes.

**Syntax:**
```sql
ALTER ROLE role_name [WITH option [...]];
ALTER ROLE role_name RENAME TO new_name;
ALTER ROLE role_name SET parameter TO value;
ALTER ROLE role_name RESET parameter;
```

**Examples:**
```sql
-- Change password
ALTER ROLE john WITH PASSWORD 'new_password';

-- Grant superuser
ALTER ROLE admin WITH SUPERUSER;

-- Remove login capability
ALTER ROLE old_user WITH NOLOGIN;

-- Rename role
ALTER ROLE john RENAME TO john_doe;

-- Set session defaults for role
ALTER ROLE analyst SET search_path TO analytics, public;
ALTER ROLE developer SET statement_timeout TO '30s';

-- Reset to default
ALTER ROLE analyst RESET search_path;

-- Set password expiration
ALTER ROLE temp_user VALID UNTIL '2026-06-30';

-- Never expire
ALTER ROLE permanent_user VALID UNTIL 'infinity';
```

### DROP ROLE / DROP USER

Remove a role.

**Syntax:**
```sql
DROP ROLE [IF EXISTS] role_name [, ...];
DROP USER [IF EXISTS] user_name [, ...];
```

**Examples:**
```sql
-- Drop single role
DROP ROLE readonly;

-- Drop if exists (no error if missing)
DROP ROLE IF EXISTS old_role;

-- Drop multiple roles
DROP ROLE role1, role2, role3;

-- Drop user
DROP USER john;
```

**Important:** You cannot drop a role that owns objects or has privileges. First reassign or drop owned objects:

```sql
-- Reassign owned objects to another role
REASSIGN OWNED BY old_user TO new_user;

-- Or drop all owned objects
DROP OWNED BY old_user;

-- Then drop the role
DROP ROLE old_user;
```

---

## Role Membership

Roles can be members of other roles, enabling hierarchical permission management.

### GRANT Role Membership

**Syntax:**
```sql
GRANT role_name [, ...] TO role_name [, ...]
[WITH ADMIN OPTION];
```

**Examples:**
```sql
-- Add user to role
GRANT developers TO john;

-- Add to multiple roles
GRANT readers, writers TO app_user;

-- Grant with admin option (can grant to others)
GRANT team_lead TO senior_dev WITH ADMIN OPTION;
```

### REVOKE Role Membership

**Syntax:**
```sql
REVOKE [ADMIN OPTION FOR] role_name [, ...] FROM role_name [, ...]
[CASCADE | RESTRICT];
```

**Examples:**
```sql
-- Remove from role
REVOKE developers FROM john;

-- Remove admin option only (keep membership)
REVOKE ADMIN OPTION FOR team_lead FROM senior_dev;

-- Cascade to dependent grants
REVOKE developers FROM john CASCADE;
```

### SET ROLE

Change the current role within a session.

**Syntax:**
```sql
SET ROLE role_name;
SET ROLE NONE;
RESET ROLE;
```

**Examples:**
```sql
-- Switch to another role (must be a member)
SET ROLE admin;

-- Execute with elevated privileges
SET ROLE superuser;
CREATE DATABASE new_db;
RESET ROLE;

-- Reset to login role
SET ROLE NONE;
-- or
RESET ROLE;
```

---

## Privileges

### GRANT Privileges

Grant permissions on database objects.

**Syntax:**
```sql
GRANT privilege [, ...] ON object_type object_name [, ...]
TO role_name [, ...] [WITH GRANT OPTION];

-- Grant all privileges
GRANT ALL [PRIVILEGES] ON object_type object_name
TO role_name;
```

**Object Types and Privileges:**

| Object Type | Available Privileges |
|-------------|---------------------|
| TABLE | SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER |
| SEQUENCE | USAGE, SELECT, UPDATE |
| DATABASE | CREATE, CONNECT, TEMPORARY |
| SCHEMA | CREATE, USAGE |
| FUNCTION | EXECUTE |
| TYPE | USAGE |

### Table Privileges

```sql
-- Grant SELECT on specific table
GRANT SELECT ON users TO readonly;

-- Grant multiple privileges
GRANT SELECT, INSERT, UPDATE ON orders TO app_user;

-- Grant all privileges
GRANT ALL PRIVILEGES ON products TO admin;

-- Grant on multiple tables
GRANT SELECT ON users, orders, products TO analyst;

-- Grant on all tables in schema
GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly;

-- Column-level privileges
GRANT SELECT (id, name, email) ON users TO limited_user;
GRANT UPDATE (status) ON orders TO support_user;

-- With grant option (can grant to others)
GRANT SELECT ON reports TO manager WITH GRANT OPTION;
```

### Sequence Privileges

```sql
-- Grant usage (NEXTVAL, CURRVAL)
GRANT USAGE ON SEQUENCE user_id_seq TO app_user;

-- Grant all sequence privileges
GRANT ALL ON SEQUENCE order_id_seq TO admin;

-- Grant on all sequences in schema
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO app_user;
```

### Schema Privileges

```sql
-- Grant usage (access objects in schema)
GRANT USAGE ON SCHEMA analytics TO analyst;

-- Grant create (can create objects in schema)
GRANT CREATE ON SCHEMA development TO developer;

-- Grant both
GRANT USAGE, CREATE ON SCHEMA app TO app_admin;
```

### Database Privileges

```sql
-- Grant connect
GRANT CONNECT ON DATABASE myapp TO app_user;

-- Grant create schemas
GRANT CREATE ON DATABASE myapp TO developer;

-- Grant temporary table creation
GRANT TEMPORARY ON DATABASE myapp TO analyst;
```

### Function Privileges

```sql
-- Grant execute
GRANT EXECUTE ON FUNCTION calculate_tax(numeric) TO accountant;

-- Grant on all functions in schema
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA utils TO developer;
```

### REVOKE Privileges

Remove previously granted privileges.

**Syntax:**
```sql
REVOKE [GRANT OPTION FOR] privilege [, ...] ON object_type object_name
FROM role_name [, ...] [CASCADE | RESTRICT];
```

**Examples:**
```sql
-- Revoke specific privilege
REVOKE INSERT ON users FROM app_user;

-- Revoke multiple privileges
REVOKE INSERT, UPDATE, DELETE ON orders FROM readonly;

-- Revoke all privileges
REVOKE ALL PRIVILEGES ON products FROM old_user;

-- Revoke grant option only (keep privilege)
REVOKE GRANT OPTION FOR SELECT ON reports FROM manager;

-- Cascade revocation
REVOKE SELECT ON users FROM parent_role CASCADE;

-- Revoke from PUBLIC
REVOKE ALL ON FUNCTION sensitive_func() FROM PUBLIC;
```

---

## Default Privileges

Set default privileges for objects created in the future.

### ALTER DEFAULT PRIVILEGES

**Syntax:**
```sql
ALTER DEFAULT PRIVILEGES
[FOR ROLE role_name [, ...]]
[IN SCHEMA schema_name [, ...]]
GRANT privilege ON object_type TO role_name;

ALTER DEFAULT PRIVILEGES
[FOR ROLE role_name [, ...]]
[IN SCHEMA schema_name [, ...]]
REVOKE privilege ON object_type FROM role_name;
```

**Examples:**
```sql
-- All future tables in schema readable by role
ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT SELECT ON TABLES TO readonly;

-- All future sequences usable by app
ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT USAGE ON SEQUENCES TO app_user;

-- All future functions executable
ALTER DEFAULT PRIVILEGES IN SCHEMA utils
GRANT EXECUTE ON FUNCTIONS TO developer;

-- For objects created by specific role
ALTER DEFAULT PRIVILEGES FOR ROLE admin IN SCHEMA public
GRANT SELECT ON TABLES TO analyst;

-- Revoke default
ALTER DEFAULT PRIVILEGES IN SCHEMA public
REVOKE SELECT ON TABLES FROM old_role;
```

---

## Row-Level Security (RLS)

Control access to individual rows in a table.

### Enable RLS

```sql
-- Enable RLS on table
ALTER TABLE sensitive_data ENABLE ROW LEVEL SECURITY;

-- Force RLS for table owner too
ALTER TABLE sensitive_data FORCE ROW LEVEL SECURITY;

-- Disable RLS
ALTER TABLE sensitive_data DISABLE ROW LEVEL SECURITY;
```

### CREATE POLICY

**Syntax:**
```sql
CREATE POLICY policy_name ON table_name
[AS { PERMISSIVE | RESTRICTIVE }]
[FOR { ALL | SELECT | INSERT | UPDATE | DELETE }]
[TO role_name [, ...]]
[USING (using_expression)]
[WITH CHECK (check_expression)];
```

**Examples:**
```sql
-- Users can only see their own data
CREATE POLICY user_isolation ON user_data
FOR ALL
TO app_user
USING (user_id = current_user_id());

-- Different policies for different operations
CREATE POLICY select_own ON documents
FOR SELECT
USING (owner_id = current_user_id() OR is_public = true);

CREATE POLICY insert_own ON documents
FOR INSERT
WITH CHECK (owner_id = current_user_id());

CREATE POLICY update_own ON documents
FOR UPDATE
USING (owner_id = current_user_id())
WITH CHECK (owner_id = current_user_id());

-- Department-based access
CREATE POLICY dept_access ON employees
FOR SELECT
TO hr_staff
USING (department_id IN (
    SELECT department_id FROM user_departments
    WHERE user_id = current_user_id()
));

-- Restrictive policy (must pass ALL restrictive policies)
CREATE POLICY audit_active ON records
AS RESTRICTIVE
FOR ALL
USING (deleted_at IS NULL);

-- Policy for specific role
CREATE POLICY admin_all ON sensitive_data
FOR ALL
TO admin
USING (true);  -- Admin sees everything
```

### ALTER POLICY

```sql
-- Rename policy
ALTER POLICY user_isolation ON user_data RENAME TO user_row_policy;

-- Change policy expression
ALTER POLICY user_isolation ON user_data
USING (user_id = current_user_id() OR role = 'admin');
```

### DROP POLICY

```sql
DROP POLICY policy_name ON table_name;
DROP POLICY IF EXISTS old_policy ON table_name;
```

---

## Security Labels

Assign security labels to objects (for mandatory access control).

```sql
-- Set security label
SECURITY LABEL FOR provider ON TABLE classified_docs
IS 'top_secret';

-- Remove security label
SECURITY LABEL FOR provider ON TABLE docs IS NULL;
```

---

## Password Management

### Setting Passwords

```sql
-- Set password on creation
CREATE USER john WITH PASSWORD 'secure_password';

-- Change password
ALTER USER john WITH PASSWORD 'new_password';

-- Encrypted password (scram-sha-256)
ALTER USER john WITH PASSWORD 'SCRAM-SHA-256$iterations:salt$...';

-- Password expiration
ALTER USER john VALID UNTIL '2026-12-31';

-- Remove password (disable password auth)
ALTER USER john WITH PASSWORD NULL;
```

### Password Policies

ScratchBird supports password policies through configuration:

```sql
-- Check password strength (via extension or function)
SELECT password_check('proposed_password');

-- Require password change
ALTER ROLE john WITH PASSWORD NULL VALID UNTIL 'now';
```

---

## Session Security

### Set Session Authorization

```sql
-- Change session user (superuser only)
SET SESSION AUTHORIZATION john;

-- Reset to original
RESET SESSION AUTHORIZATION;
```

### Current User Functions

```sql
-- Current session user
SELECT SESSION_USER;

-- Current role
SELECT CURRENT_USER;

-- Check role membership
SELECT pg_has_role('john', 'admin', 'MEMBER');

-- Check privilege
SELECT has_table_privilege('john', 'users', 'SELECT');
SELECT has_schema_privilege('john', 'public', 'USAGE');
```

---

## Common Security Patterns

### Read-Only User

```sql
-- Create readonly role
CREATE ROLE readonly;
GRANT CONNECT ON DATABASE myapp TO readonly;
GRANT USAGE ON SCHEMA public TO readonly;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly;
ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT SELECT ON TABLES TO readonly;

-- Create user with readonly role
CREATE USER analyst WITH PASSWORD 'password' IN ROLE readonly;
```

### Application User

```sql
-- Role for application
CREATE ROLE app_role;
GRANT CONNECT ON DATABASE myapp TO app_role;
GRANT USAGE ON SCHEMA public TO app_role;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO app_role;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO app_role;
ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO app_role;
ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT USAGE ON SEQUENCES TO app_role;

-- Create service account
CREATE USER app_service WITH PASSWORD 'app_password' IN ROLE app_role;
```

### Schema Isolation

```sql
-- Create schema per tenant
CREATE SCHEMA tenant_a;
CREATE SCHEMA tenant_b;

-- Create roles per tenant
CREATE ROLE tenant_a_role;
GRANT USAGE, CREATE ON SCHEMA tenant_a TO tenant_a_role;
GRANT ALL ON ALL TABLES IN SCHEMA tenant_a TO tenant_a_role;

CREATE ROLE tenant_b_role;
GRANT USAGE, CREATE ON SCHEMA tenant_b TO tenant_b_role;
GRANT ALL ON ALL TABLES IN SCHEMA tenant_b TO tenant_b_role;

-- Users can only access their schema
CREATE USER tenant_a_user WITH PASSWORD 'pass' IN ROLE tenant_a_role;
ALTER USER tenant_a_user SET search_path TO tenant_a;
```

### Audit Role

```sql
-- Role that can only read audit tables
CREATE ROLE auditor;
GRANT CONNECT ON DATABASE myapp TO auditor;
GRANT USAGE ON SCHEMA audit TO auditor;
GRANT SELECT ON ALL TABLES IN SCHEMA audit TO auditor;
-- No access to main schema
```

---

## Known Limitations

### Current Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| CREATE ROLE | Implemented | Emits EXT_CREATE_ROLE opcode with options |
| CREATE USER | Implemented | Emits EXT_CREATE_USER opcode |
| ALTER ROLE | Partial | Basic alterations work |
| DROP ROLE | Implemented | Works correctly |
| GRANT (role membership) | Implemented | Emits EXT_GRANT_PRIVILEGE |
| GRANT (privileges) | Implemented | Emits EXT_GRANT_PRIVILEGE with privilege type/object |
| REVOKE | Implemented | Emits EXT_REVOKE_PRIVILEGE |
| ALTER DEFAULT PRIVILEGES | Not implemented | Parser does not accept |
| Row-Level Security | Parsed only | Parser accepts, not enforced at runtime |
| SET ROLE | Implemented | Via SET statement handling |

### Specific Limitations

**Limited Scope:**
- GRANT/REVOKE support a single object per statement (ON ALL not fully supported)
- GRANT/REVOKE support a single grantee per statement
- Column-level privileges not supported
- GRANT WITH GRANT OPTION parsed but flag may not be enforced

**Not Implemented:**
- ALTER DEFAULT PRIVILEGES
- Row-Level Security enforcement
- Security labels
- REASSIGN OWNED / DROP OWNED

---

## See Also

- [Admin Security Guide](../../admin/security.md) - Server security configuration
- [User Management](../../admin/user-management.md) - Administrative user management
- [Databases and Schemas](01_databases_and_schemas.md) - Schema management
- [Session Configuration](10_session_show_set.md) - Session parameters

