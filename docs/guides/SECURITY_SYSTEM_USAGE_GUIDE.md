# ScratchBird Security System Usage Guide
## Complete Guide to Users, Roles, Groups, and Permissions

### Table of Contents
1. [Introduction](#introduction)
2. [User Management](#user-management)
3. [Role Management](#role-management)
4. [Group Management](#group-management)
5. [Privilege Management](#privilege-management)
6. [Session Management](#session-management)
7. [Common Workflows](#common-workflows)
8. [Best Practices](#best-practices)
9. [Troubleshooting](#troubleshooting)

---

## Introduction

The ScratchBird security system implements SQL-standard user authentication and authorization with support for:
- **Users**: Individual database accounts with credentials
- **Roles**: Named collections of privileges that can be granted to users
- **Groups**: Collections of users for organizational purposes (supports LOCAL, AD, LDAP)
- **Privileges**: Fine-grained permissions on database objects (tables, schemas, etc.)
- **Session Management**: Dynamic privilege escalation with SET ROLE

### Security Model

ScratchBird uses a **discretionary access control (DAC)** model:
- Every database object has an owner
- Owners have full control over their objects
- Superusers bypass all permission checks
- Privileges can be granted to users, roles, groups, or PUBLIC
- Privileges can be granted with GRANT OPTION (allowing re-granting)

---

## User Management

### Creating Users

**Basic User:**
```sql
CREATE USER alice WITH PASSWORD 'secure_password_123';
```

**Superuser (full privileges):**
```sql
CREATE USER admin WITH PASSWORD 'admin_password' SUPERUSER;
```

**User without password (not recommended):**
```sql
CREATE USER service_account;
```

**Syntax:**
```sql
CREATE USER username
    [WITH PASSWORD 'password']
    [SUPERUSER | NOSUPERUSER];
```

**Notes:**
- Usernames are case-sensitive
- Passwords are hashed before storage (bcrypt/argon2)
- Only superusers can create users
- Default is NOSUPERUSER

### Modifying Users

**Change Password:**
```sql
ALTER USER alice WITH PASSWORD 'new_secure_password';
```

**Grant Superuser Status:**
```sql
ALTER USER alice WITH SUPERUSER;
```

**Revoke Superuser Status:**
```sql
ALTER USER alice WITH NOSUPERUSER;
```

**Syntax:**
```sql
ALTER USER username
    [WITH PASSWORD 'new_password']
    [SUPERUSER | NOSUPERUSER];
```

**Notes:**
- Only superusers can alter users
- Password changes take effect immediately
- Changing superuser status requires reconnection

### Deleting Users

**Drop User:**
```sql
DROP USER alice;
```

**Drop User if Exists (no error if missing):**
```sql
DROP USER IF EXISTS alice;
```

**Drop User and Owned Objects:**
```sql
DROP USER alice CASCADE;
```

**Syntax:**
```sql
DROP USER [IF EXISTS] username [CASCADE | RESTRICT];
```

**Notes:**
- CASCADE: Drops all objects owned by the user and revokes all granted privileges
- RESTRICT: Fails if user owns objects or has granted privileges (default)
- Only superusers can drop users

---

## Role Management

### What Are Roles?

Roles are named collections of privileges that can be granted to users. They simplify permission management by allowing you to:
- Define privilege sets once
- Grant the entire set to multiple users
- Change privileges for all users by updating the role

### Creating Roles

**Basic Role:**
```sql
CREATE ROLE read_only;
```

**Grant Privileges to Role:**
```sql
GRANT SELECT ON TABLE users TO read_only;
GRANT SELECT ON TABLE products TO read_only;
GRANT SELECT ON TABLE orders TO read_only;
```

**Syntax:**
```sql
CREATE ROLE rolename;
```

**Notes:**
- Roles don't have passwords (they're not login accounts)
- Roles can be granted to users
- Roles can have privileges granted to them
- Only superusers can create roles

### Granting Roles to Users

**Grant Role:**
```sql
GRANT read_only TO alice;
```

**Grant Multiple Roles:**
```sql
GRANT read_only TO alice;
GRANT reporting_user TO alice;
```

**Syntax:**
```sql
GRANT rolename TO username;
```

**Notes:**
- User inherits all privileges from the role
- Multiple roles can be granted to one user
- Role grants are cumulative

### Revoking Roles from Users

**Revoke Role:**
```sql
REVOKE read_only FROM alice;
```

**Revoke with CASCADE:**
```sql
REVOKE read_only FROM alice CASCADE;
```

**Syntax:**
```sql
REVOKE rolename FROM username [CASCADE | RESTRICT];
```

**Notes:**
- CASCADE: Also revokes any privileges the user granted while having this role
- RESTRICT: Fails if the user granted any privileges (default)

### Deleting Roles

**Drop Role:**
```sql
DROP ROLE read_only;
```

**Drop with CASCADE:**
```sql
DROP ROLE read_only CASCADE;
```

**Syntax:**
```sql
DROP ROLE [IF EXISTS] rolename [CASCADE | RESTRICT];
```

---

## Group Management

### What Are Groups?

Groups are collections of users for organizational purposes. ScratchBird supports three types:
- **LOCAL**: Database-internal groups
- **AD**: Active Directory groups (external authentication)
- **LDAP**: LDAP groups (external authentication)

### Creating Groups

**Create Local Group:**
```sql
CREATE GROUP developers;
```

**Syntax:**
```sql
CREATE GROUP groupname;
```

**Notes:**
- Currently only LOCAL groups are fully supported
- AD and LDAP groups are reserved for future external authentication
- Groups can be granted privileges like users and roles

### Deleting Groups

**Drop Group:**
```sql
DROP GROUP developers;
```

**Drop with Restrictions:**
```sql
DROP GROUP developers RESTRICT;  -- Fail if group has members
DROP GROUP developers CASCADE;   -- Remove all members
```

**Syntax:**
```sql
DROP GROUP [IF EXISTS] groupname [CASCADE | RESTRICT];
```

---

## Privilege Management

### Privilege Types

**Object Privileges:**
- `SELECT`: Read data from table/view
- `INSERT`: Add new rows to table
- `UPDATE`: Modify existing rows
- `DELETE`: Remove rows from table
- `TRUNCATE`: Remove all rows efficiently
- `REFERENCES`: Create foreign keys referencing table
- `TRIGGER`: Create triggers on table

**Schema Privileges:**
- `CREATE`: Create objects in schema
- `USAGE`: Use schema (required to access objects)

**Sequence Privileges:**
- `SEQUENCE_USAGE`: Use NEXTVAL/CURRVAL
- `SEQUENCE_UPDATE`: Use SETVAL/ALTER SEQUENCE

**Special Privileges:**
- `ALL`: All applicable privileges
- `EXECUTE`: Execute functions/procedures
- `CONNECT`: Connect to database
- `TEMPORARY`: Create temporary tables

### Granting Privileges

**Grant Single Privilege:**
```sql
GRANT SELECT ON TABLE users TO alice;
```

**Grant Multiple Privileges:**
```sql
GRANT SELECT, INSERT, UPDATE ON TABLE users TO alice;
```

**Grant All Privileges:**
```sql
GRANT ALL ON TABLE users TO alice;
```

**Grant to Role:**
```sql
GRANT SELECT ON TABLE users TO read_only;
```

**Grant to Group:**
```sql
GRANT SELECT ON TABLE users TO developers;
```

**Grant to PUBLIC (all users):**
```sql
GRANT SELECT ON TABLE public_data TO PUBLIC;
```

**Grant with GRANT OPTION:**
```sql
GRANT SELECT ON TABLE users TO alice WITH GRANT OPTION;
```

**Syntax:**
```sql
GRANT privilege[, ...]
    ON object_type object_name
    TO grantee[, ...]
    [WITH GRANT OPTION];
```

Where:
- `privilege`: SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER, ALL
- `object_type`: TABLE, VIEW, SEQUENCE, SCHEMA, DATABASE
- `object_name`: Name of the object
- `grantee`: username, rolename, groupname, or PUBLIC

**Notes:**
- WITH GRANT OPTION allows the grantee to grant the privilege to others
- Granting to PUBLIC makes the privilege available to all users
- Only object owners and superusers can grant privileges

### Revoking Privileges

**Revoke Single Privilege:**
```sql
REVOKE SELECT ON TABLE users FROM alice;
```

**Revoke Multiple Privileges:**
```sql
REVOKE INSERT, UPDATE, DELETE ON TABLE users FROM alice;
```

**Revoke All Privileges:**
```sql
REVOKE ALL ON TABLE users FROM alice;
```

**Revoke with CASCADE:**
```sql
REVOKE SELECT ON TABLE users FROM alice CASCADE;
```

**Syntax:**
```sql
REVOKE privilege[, ...]
    ON object_type object_name
    FROM grantee[, ...]
    [CASCADE | RESTRICT];
```

**Notes:**
- CASCADE: Also revokes privileges that were granted by the grantee
- RESTRICT: Fails if the grantee granted the privilege to others (default)

---

## Session Management

### SET ROLE

**Purpose:** Temporarily assume the privileges of a role during your session.

**Set Active Role:**
```sql
SET ROLE admin;
```

**Reset to Default Role:**
```sql
RESET ROLE;
```

**Use Cases:**
- Temporarily elevate privileges for specific operations
- Test permission configurations
- Principle of least privilege (use minimal role most of the time)

**Notes:**
- You must have been granted the role
- All subsequent operations use the role's privileges
- RESET ROLE returns to your normal user privileges

### SET SESSION AUTHORIZATION

**Purpose:** Change the effective user for the entire session.

**Set Session User:**
```sql
SET SESSION AUTHORIZATION alice;
```

**Reset to Connection User:**
```sql
RESET SESSION AUTHORIZATION;
```

**Use Cases:**
- Superusers testing permissions as another user
- Application connection pooling
- Debugging permission issues

**Notes:**
- Only superusers can use SET SESSION AUTHORIZATION
- Changes the effective user for all operations
- More powerful than SET ROLE

---

## Common Workflows

### Application User Setup

```sql
-- 1. Create application user
CREATE USER app_user WITH PASSWORD 'secure_app_password';

-- 2. Create role for application privileges
CREATE ROLE app_role;

-- 3. Grant privileges to role
GRANT SELECT, INSERT ON TABLE users TO app_role;
GRANT SELECT, INSERT, UPDATE ON TABLE sessions TO app_role;
GRANT SELECT ON TABLE products TO app_role;
GRANT SELECT, INSERT ON TABLE orders TO app_role;

-- 4. Grant role to user
GRANT app_role TO app_user;
```

### Read-Only User

```sql
-- 1. Create read-only role
CREATE ROLE readonly;

-- 2. Grant SELECT on all tables
GRANT SELECT ON TABLE users TO readonly;
GRANT SELECT ON TABLE products TO readonly;
GRANT SELECT ON TABLE orders TO readonly;
-- ... (repeat for all tables)

-- 3. Create user and grant role
CREATE USER analyst WITH PASSWORD 'analyst_password';
GRANT readonly TO analyst;
```

### Administrative Hierarchy

```sql
-- 1. Create roles
CREATE ROLE db_admin;
CREATE ROLE db_developer;
CREATE ROLE db_viewer;

-- 2. Set up privilege hierarchy
-- Viewer: Read-only access
GRANT SELECT ON ALL TABLES TO db_viewer;

-- Developer: Read/write access
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES TO db_developer;
GRANT CREATE ON SCHEMA public TO db_developer;

-- Admin: Full control
GRANT ALL ON ALL TABLES TO db_admin;
GRANT ALL ON SCHEMA public TO db_admin;

-- 3. Create users and assign roles
CREATE USER alice WITH PASSWORD 'password';
GRANT db_admin TO alice;

CREATE USER bob WITH PASSWORD 'password';
GRANT db_developer TO bob;

CREATE USER charlie WITH PASSWORD 'password';
GRANT db_viewer TO charlie;
```

### Temporary Privilege Escalation

```sql
-- Normal user with basic privileges
CREATE USER worker WITH PASSWORD 'worker_pass';
GRANT SELECT ON TABLE public_data TO worker;

-- Administrative role
CREATE ROLE maintenance_mode;
GRANT ALL ON ALL TABLES TO maintenance_mode;
GRANT maintenance_mode TO worker;

-- During maintenance window, worker can:
SET ROLE maintenance_mode;
-- Perform administrative tasks
UPDATE system_config SET maintenance = true;
-- ...
RESET ROLE;  -- Return to normal privileges
```

### Multi-Tenant Application

```sql
-- 1. Create tenant-specific roles
CREATE ROLE tenant_a_role;
CREATE ROLE tenant_b_role;

-- 2. Grant privileges per tenant
GRANT SELECT, INSERT, UPDATE, DELETE ON TABLE tenant_a_data TO tenant_a_role;
GRANT SELECT, INSERT, UPDATE, DELETE ON TABLE tenant_b_data TO tenant_b_role;

-- 3. Create users per tenant
CREATE USER tenant_a_user WITH PASSWORD 'password_a';
GRANT tenant_a_role TO tenant_a_user;

CREATE USER tenant_b_user WITH PASSWORD 'password_b';
GRANT tenant_b_role TO tenant_b_user;
```

---

## Best Practices

### 1. Principle of Least Privilege
- Grant only the minimum privileges required
- Use roles to group privileges logically
- Regularly review and revoke unused privileges

### 2. Password Management
- Use strong passwords (minimum 12 characters, mixed case, numbers, symbols)
- Change passwords regularly
- Never share passwords between users
- Store passwords securely (never in source code)

### 3. Superuser Usage
- Create minimal superuser accounts
- Use regular users for daily operations
- Use SET ROLE for temporary privilege escalation
- Audit all superuser operations

### 4. Role Organization
- Create roles based on job functions (analyst, developer, admin)
- Use descriptive role names (readonly_users, data_writers, etc.)
- Document role purposes and privileges
- Keep role hierarchy simple (avoid deep nesting)

### 5. PUBLIC Grants
- Be very careful with GRANT TO PUBLIC
- Only grant PUBLIC access to truly public data
- Regularly audit PUBLIC privileges
- Consider revoking PUBLIC access by default

### 6. Object Ownership
- Use dedicated owner accounts for database objects
- Don't use application accounts as owners
- Transfer ownership when staff changes
- Document object ownership

### 7. Audit and Monitoring
- Log all CREATE USER, GRANT, REVOKE operations
- Monitor failed authentication attempts
- Review privilege grants regularly
- Alert on privilege escalation

### 8. Testing
- Test permissions in non-production environment first
- Use SET SESSION AUTHORIZATION to test as other users
- Verify permission denials work correctly
- Test CASCADE behavior before using in production

---

## Troubleshooting

### Permission Denied Errors

**Symptom:** `Permission denied: SELECT on table users`

**Diagnosis:**
```sql
-- Check current user
SELECT CURRENT_USER;

-- Check granted privileges (as superuser)
SELECT * FROM pg_catalog.pg_roles WHERE rolname = 'username';
```

**Solutions:**
```sql
-- Grant required privilege
GRANT SELECT ON TABLE users TO username;

-- Or grant via role
GRANT readonly TO username;
```

### User Cannot Login

**Symptom:** `User 'alice' does not exist` or `Authentication failed`

**Solutions:**
```sql
-- Verify user exists
-- (Query catalog tables as superuser)

-- If user doesn't exist, create
CREATE USER alice WITH PASSWORD 'password';

-- If password is wrong, reset
ALTER USER alice WITH PASSWORD 'new_password';
```

### Role Privileges Not Working

**Symptom:** User has role but can't access objects

**Diagnosis:**
```sql
-- Verify role grant
-- (Check role membership in catalog)

-- Verify role has privileges
-- (Check privilege grants to role)
```

**Solutions:**
```sql
-- Grant role to user if missing
GRANT rolename TO username;

-- Grant privileges to role if missing
GRANT SELECT ON TABLE tablename TO rolename;
```

### CASCADE Failures

**Symptom:** `DROP USER failed: user has granted privileges`

**Solutions:**
```sql
-- Use CASCADE to force deletion
DROP USER username CASCADE;

-- Or manually revoke privileges first
REVOKE ALL PRIVILEGES FROM username;
DROP USER username;
```

### Superuser Locked Out

**Symptom:** No superuser accounts accessible

**Recovery:**
```bash
# Start database in single-user mode
# Reset superuser password
# Or create new superuser
```

**Prevention:**
- Always maintain at least 2 superuser accounts
- Document superuser credentials securely
- Have recovery procedures documented

---

## SQL Reference Quick Guide

### User Commands
```sql
CREATE USER username WITH PASSWORD 'password' [SUPERUSER];
ALTER USER username WITH PASSWORD 'new_password' [NOSUPERUSER];
DROP USER [IF EXISTS] username [CASCADE];
```

### Role Commands
```sql
CREATE ROLE rolename;
GRANT rolename TO username;
REVOKE rolename FROM username [CASCADE];
DROP ROLE [IF EXISTS] rolename [CASCADE];
```

### Group Commands
```sql
CREATE GROUP groupname;
DROP GROUP [IF EXISTS] groupname [CASCADE];
```

### Privilege Commands
```sql
GRANT privilege ON object_type object_name TO grantee [WITH GRANT OPTION];
REVOKE privilege ON object_type object_name FROM grantee [CASCADE];
```

### Session Commands
```sql
SET ROLE rolename;
RESET ROLE;
SET SESSION AUTHORIZATION username;
RESET SESSION AUTHORIZATION;
```

---

## Appendix: Privilege Matrix

| Privilege | Table | View | Sequence | Schema | Database |
|-----------|-------|------|----------|--------|----------|
| SELECT    | ✓     | ✓    | -        | -      | -        |
| INSERT    | ✓     | -    | -        | -      | -        |
| UPDATE    | ✓     | ✓    | -        | -      | -        |
| DELETE    | ✓     | -    | -        | -      | -        |
| TRUNCATE  | ✓     | -    | -        | -      | -        |
| REFERENCES| ✓     | -    | -        | -      | -        |
| TRIGGER   | ✓     | -    | -        | -      | -        |
| CREATE    | -     | -    | -        | ✓      | ✓        |
| USAGE     | -     | -    | ✓        | ✓      | -        |
| EXECUTE   | -     | -    | -        | -      | -        |
| CONNECT   | -     | -    | -        | -      | ✓        |
| TEMPORARY | -     | -    | -        | -      | ✓        |

---

**Document Version:** 1.0
**Last Updated:** November 10, 2025
**ScratchBird Version:** Phase 2 (Security System)
