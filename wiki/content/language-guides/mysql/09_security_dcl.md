[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL - Security (DCL)

**Status:** Alpha documentation
**Last Updated:** 2026-01-30

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

---

## Overview

This document covers Data Control Language (DCL) statements in MySQL emulation mode. DCL statements manage database security through users, roles, and privileges. ScratchBird maps MySQL security concepts to its native security model.

**Important:** MySQL security DCL is currently in a stubbed state. The parser accepts MySQL security syntax but most statements are not executed by the backend. Use native ScratchBird security commands or PostgreSQL emulation mode for production security management.

---

## User Management

### CREATE USER

Create a new database user.

**Syntax:**
```sql
CREATE USER [IF NOT EXISTS]
    user_specification [, user_specification] ...
    [DEFAULT ROLE role [, role] ...]
    [REQUIRE {NONE | tls_option [[AND] tls_option] ...}]
    [WITH resource_option [resource_option] ...]
    [password_option | lock_option] ...
```

**User specification:**
```sql
user_name [@host_name] [IDENTIFIED BY 'password']
user_name [@host_name] [IDENTIFIED WITH auth_plugin [BY 'password']]
```

**Examples:**
```sql
-- Basic user creation
CREATE USER 'john'@'localhost' IDENTIFIED BY 'password123';

-- User with any host
CREATE USER 'app_user'@'%' IDENTIFIED BY 'app_password';

-- User from specific network
CREATE USER 'admin'@'192.168.1.%' IDENTIFIED BY 'admin_pass';

-- User if not exists
CREATE USER IF NOT EXISTS 'service'@'localhost' IDENTIFIED BY 'service_pass';

-- User with password expiration
CREATE USER 'temp'@'%'
    IDENTIFIED BY 'temp_pass'
    PASSWORD EXPIRE INTERVAL 90 DAY;

-- User with resource limits
CREATE USER 'limited'@'%'
    IDENTIFIED BY 'password'
    WITH MAX_QUERIES_PER_HOUR 100
         MAX_UPDATES_PER_HOUR 50
         MAX_CONNECTIONS_PER_HOUR 10
         MAX_USER_CONNECTIONS 2;

-- User requiring SSL
CREATE USER 'secure'@'%'
    IDENTIFIED BY 'password'
    REQUIRE SSL;

-- User requiring specific certificate
CREATE USER 'cert_user'@'%'
    IDENTIFIED BY 'password'
    REQUIRE X509;
```

### ALTER USER

Modify user attributes.

**Syntax:**
```sql
ALTER USER [IF EXISTS]
    user_specification [, user_specification] ...
    [REQUIRE {NONE | tls_option [[AND] tls_option] ...}]
    [WITH resource_option [resource_option] ...]
    [password_option | lock_option] ...
```

**Examples:**
```sql
-- Change password
ALTER USER 'john'@'localhost' IDENTIFIED BY 'new_password';

-- Change own password
ALTER USER CURRENT_USER() IDENTIFIED BY 'my_new_password';

-- Lock account
ALTER USER 'john'@'localhost' ACCOUNT LOCK;

-- Unlock account
ALTER USER 'john'@'localhost' ACCOUNT UNLOCK;

-- Expire password
ALTER USER 'john'@'localhost' PASSWORD EXPIRE;

-- Set password expiration interval
ALTER USER 'john'@'localhost' PASSWORD EXPIRE INTERVAL 90 DAY;

-- Never expire password
ALTER USER 'john'@'localhost' PASSWORD EXPIRE NEVER;

-- Set default password policy
ALTER USER 'john'@'localhost' PASSWORD EXPIRE DEFAULT;

-- Change resource limits
ALTER USER 'app'@'%'
    WITH MAX_QUERIES_PER_HOUR 1000
         MAX_USER_CONNECTIONS 20;

-- Update TLS requirements
ALTER USER 'secure'@'%' REQUIRE SSL;

-- Remove TLS requirements
ALTER USER 'secure'@'%' REQUIRE NONE;
```

### DROP USER

Remove a user account.

**Syntax:**
```sql
DROP USER [IF EXISTS] user_name [@host_name] [, user_name [@host_name]] ...
```

**Examples:**
```sql
-- Drop single user
DROP USER 'john'@'localhost';

-- Drop if exists
DROP USER IF EXISTS 'old_user'@'%';

-- Drop multiple users
DROP USER 'user1'@'localhost', 'user2'@'localhost';
```

### RENAME USER

Change a user's name.

**Syntax:**
```sql
RENAME USER old_user TO new_user [, old_user TO new_user] ...
```

**Examples:**
```sql
-- Rename user
RENAME USER 'john'@'localhost' TO 'john_doe'@'localhost';

-- Change host
RENAME USER 'app'@'localhost' TO 'app'@'%';

-- Multiple renames
RENAME USER
    'user1'@'localhost' TO 'user1_new'@'localhost',
    'user2'@'localhost' TO 'user2_new'@'localhost';
```

### SET PASSWORD

Change user password (legacy syntax).

**Syntax:**
```sql
SET PASSWORD [FOR user] = 'password'
SET PASSWORD [FOR user] = PASSWORD('password')  -- Deprecated
```

**Examples:**
```sql
-- Set own password
SET PASSWORD = 'new_password';

-- Set another user's password
SET PASSWORD FOR 'john'@'localhost' = 'new_password';
```

---

## Role Management

MySQL 8.0+ supports roles for grouping privileges.

### CREATE ROLE

Create a new role.

**Syntax:**
```sql
CREATE ROLE [IF NOT EXISTS] role_name [, role_name] ...
```

**Examples:**
```sql
-- Create single role
CREATE ROLE 'app_read';

-- Create multiple roles
CREATE ROLE 'app_read', 'app_write', 'app_admin';

-- Create if not exists
CREATE ROLE IF NOT EXISTS 'developers';
```

### DROP ROLE

Remove a role.

**Syntax:**
```sql
DROP ROLE [IF EXISTS] role_name [, role_name] ...
```

**Examples:**
```sql
DROP ROLE 'old_role';
DROP ROLE IF EXISTS 'deprecated_role';
```

### SET ROLE

Activate roles for current session.

**Syntax:**
```sql
SET ROLE {
    DEFAULT
  | NONE
  | ALL
  | ALL EXCEPT role_name [, role_name] ...
  | role_name [, role_name] ...
}
```

**Examples:**
```sql
-- Activate specific role
SET ROLE 'app_admin';

-- Activate multiple roles
SET ROLE 'app_read', 'app_write';

-- Activate all granted roles
SET ROLE ALL;

-- Deactivate all roles
SET ROLE NONE;

-- Activate all except specific
SET ROLE ALL EXCEPT 'dangerous_role';

-- Reset to default roles
SET ROLE DEFAULT;
```

### SET DEFAULT ROLE

Set which roles activate on login.

**Syntax:**
```sql
SET DEFAULT ROLE
    {NONE | ALL | role_name [, role_name] ...}
    TO user_name [, user_name] ...
```

**Examples:**
```sql
-- Set default role for user
SET DEFAULT ROLE 'app_read' TO 'john'@'localhost';

-- Multiple default roles
SET DEFAULT ROLE 'app_read', 'app_write' TO 'developer'@'%';

-- All roles as default
SET DEFAULT ROLE ALL TO 'admin'@'localhost';

-- No default roles
SET DEFAULT ROLE NONE TO 'temp_user'@'%';
```

---

## Privileges

### GRANT Privileges

Grant permissions to users or roles.

**Syntax:**
```sql
GRANT privilege_type [(column_list)] [, privilege_type [(column_list)]] ...
ON [object_type] priv_level
TO user_or_role [, user_or_role] ...
[WITH GRANT OPTION]
```

**Privilege levels:**
```sql
*               -- All databases
*.*             -- All databases and tables
db_name.*       -- All tables in database
db_name.tbl_name -- Specific table
tbl_name        -- Table in current database
```

**Common privileges:**
| Privilege | Description |
|-----------|-------------|
| `ALL [PRIVILEGES]` | All privileges (except GRANT OPTION) |
| `SELECT` | Read data |
| `INSERT` | Insert data |
| `UPDATE` | Modify data |
| `DELETE` | Delete data |
| `CREATE` | Create databases/tables |
| `DROP` | Drop databases/tables |
| `INDEX` | Create/drop indexes |
| `ALTER` | Alter tables |
| `REFERENCES` | Create foreign keys |
| `EXECUTE` | Execute stored routines |
| `TRIGGER` | Create triggers |
| `CREATE VIEW` | Create views |
| `SHOW VIEW` | Show view definitions |
| `CREATE ROUTINE` | Create stored procedures |
| `ALTER ROUTINE` | Alter/drop stored procedures |

**Examples:**
```sql
-- Grant on all tables in database
GRANT SELECT, INSERT, UPDATE ON mydb.* TO 'app_user'@'%';

-- Grant on specific table
GRANT SELECT ON mydb.users TO 'readonly'@'%';

-- Grant all privileges
GRANT ALL PRIVILEGES ON mydb.* TO 'admin'@'localhost';

-- Grant on all databases
GRANT SELECT ON *.* TO 'readonly'@'%';

-- Grant with grant option
GRANT SELECT ON mydb.reports TO 'manager'@'%' WITH GRANT OPTION;

-- Grant column-level privileges
GRANT SELECT (id, name, email) ON mydb.users TO 'limited'@'%';
GRANT UPDATE (status) ON mydb.orders TO 'support'@'%';

-- Grant to role
GRANT SELECT, INSERT ON mydb.* TO 'app_read';

-- Grant role to user
GRANT 'app_read' TO 'john'@'localhost';

-- Grant execute on procedure
GRANT EXECUTE ON PROCEDURE mydb.process_order TO 'app_user'@'%';

-- Grant execute on all procedures
GRANT EXECUTE ON mydb.* TO 'app_user'@'%';
```

### REVOKE Privileges

Remove privileges from users or roles.

**Syntax:**
```sql
REVOKE privilege_type [(column_list)] [, privilege_type [(column_list)]] ...
ON [object_type] priv_level
FROM user_or_role [, user_or_role] ...

REVOKE ALL [PRIVILEGES], GRANT OPTION
FROM user_or_role [, user_or_role] ...
```

**Examples:**
```sql
-- Revoke specific privilege
REVOKE INSERT ON mydb.* FROM 'app_user'@'%';

-- Revoke multiple privileges
REVOKE INSERT, UPDATE, DELETE ON mydb.orders FROM 'readonly'@'%';

-- Revoke all privileges on database
REVOKE ALL PRIVILEGES ON mydb.* FROM 'old_user'@'%';

-- Revoke grant option
REVOKE GRANT OPTION ON mydb.reports FROM 'manager'@'%';

-- Revoke all privileges and grant option
REVOKE ALL PRIVILEGES, GRANT OPTION FROM 'user'@'%';

-- Revoke role from user
REVOKE 'app_write' FROM 'john'@'localhost';
```

### SHOW GRANTS

Display granted privileges.

```sql
-- Show own grants
SHOW GRANTS;

-- Show grants for specific user
SHOW GRANTS FOR 'john'@'localhost';

-- Show grants for role
SHOW GRANTS FOR 'app_read';

-- Show grants using current roles
SHOW GRANTS FOR CURRENT_USER USING 'app_read', 'app_write';
```

---

## Privilege System Tables

MySQL stores privilege information in the `mysql` database.

### mysql.user

User accounts and global privileges.

```sql
SELECT
    User,
    Host,
    Select_priv,
    Insert_priv,
    Update_priv,
    Delete_priv,
    Create_priv,
    Drop_priv
FROM mysql.user
WHERE User NOT LIKE 'mysql.%';
```

### mysql.db

Database-level privileges.

```sql
SELECT
    User,
    Host,
    Db,
    Select_priv,
    Insert_priv,
    Update_priv
FROM mysql.db;
```

### mysql.tables_priv

Table-level privileges.

```sql
SELECT
    User,
    Host,
    Db,
    Table_name,
    Table_priv
FROM mysql.tables_priv;
```

### mysql.columns_priv

Column-level privileges.

```sql
SELECT
    User,
    Host,
    Db,
    Table_name,
    Column_name,
    Column_priv
FROM mysql.columns_priv;
```

### mysql.role_edges

Role membership.

```sql
SELECT
    FROM_USER AS role,
    TO_USER AS member,
    WITH_ADMIN_OPTION
FROM mysql.role_edges;
```

---

## Authentication

### Authentication Plugins

MySQL supports multiple authentication methods.

```sql
-- Create user with specific auth plugin
CREATE USER 'app'@'%'
    IDENTIFIED WITH mysql_native_password BY 'password';

-- Use caching_sha2_password (MySQL 8.0 default)
CREATE USER 'secure_app'@'%'
    IDENTIFIED WITH caching_sha2_password BY 'password';

-- Change auth plugin
ALTER USER 'app'@'%'
    IDENTIFIED WITH caching_sha2_password BY 'new_password';
```

### Password Validation

Check password strength requirements.

```sql
-- Check if password validation is enabled
SHOW VARIABLES LIKE 'validate_password%';

-- Test password strength
SELECT VALIDATE_PASSWORD_STRENGTH('MyP@ssw0rd!');
```

---

## Common Security Patterns

### Read-Only User

```sql
-- Create readonly role
CREATE ROLE 'readonly';
GRANT SELECT ON mydb.* TO 'readonly';

-- Create user with readonly role
CREATE USER 'analyst'@'%' IDENTIFIED BY 'password';
GRANT 'readonly' TO 'analyst'@'%';
SET DEFAULT ROLE 'readonly' TO 'analyst'@'%';
```

### Application User

```sql
-- Create app role
CREATE ROLE 'app_role';
GRANT SELECT, INSERT, UPDATE, DELETE ON mydb.* TO 'app_role';
GRANT EXECUTE ON mydb.* TO 'app_role';

-- Create service account
CREATE USER 'app_service'@'%'
    IDENTIFIED BY 'app_password'
    WITH MAX_USER_CONNECTIONS 50;
GRANT 'app_role' TO 'app_service'@'%';
SET DEFAULT ROLE 'app_role' TO 'app_service'@'%';
```

### Administrator User

```sql
-- Create admin with full access to specific database
CREATE USER 'db_admin'@'localhost' IDENTIFIED BY 'admin_pass';
GRANT ALL PRIVILEGES ON mydb.* TO 'db_admin'@'localhost' WITH GRANT OPTION;
```

### Developer User

```sql
-- Create developer role
CREATE ROLE 'developer';
GRANT SELECT, INSERT, UPDATE, DELETE ON dev_db.* TO 'developer';
GRANT CREATE, DROP, ALTER, INDEX ON dev_db.* TO 'developer';
GRANT CREATE ROUTINE, ALTER ROUTINE ON dev_db.* TO 'developer';
GRANT TRIGGER ON dev_db.* TO 'developer';

-- Create developer user
CREATE USER 'dev1'@'localhost' IDENTIFIED BY 'dev_pass';
GRANT 'developer' TO 'dev1'@'localhost';
SET DEFAULT ROLE 'developer' TO 'dev1'@'localhost';
```

---

## Security Best Practices

### 1. Use Strong Passwords

```sql
-- Set password validation
SET GLOBAL validate_password.policy = STRONG;
SET GLOBAL validate_password.length = 12;
```

### 2. Limit Host Access

```sql
-- Specific host instead of %
CREATE USER 'app'@'192.168.1.100' IDENTIFIED BY 'password';

-- Network range
CREATE USER 'app'@'192.168.1.%' IDENTIFIED BY 'password';
```

### 3. Use Roles

```sql
-- Prefer roles over direct grants
CREATE ROLE 'data_reader', 'data_writer';
GRANT SELECT ON mydb.* TO 'data_reader';
GRANT INSERT, UPDATE ON mydb.* TO 'data_writer';

-- Users get roles
GRANT 'data_reader' TO 'analyst'@'%';
GRANT 'data_reader', 'data_writer' TO 'app'@'%';
```

### 4. Principle of Least Privilege

```sql
-- Grant only needed privileges
GRANT SELECT (id, name, email) ON users TO 'support'@'%';
GRANT UPDATE (status) ON orders TO 'support'@'%';
```

### 5. Require SSL for Remote Connections

```sql
CREATE USER 'remote_app'@'%'
    IDENTIFIED BY 'password'
    REQUIRE SSL;
```

---

## Known Limitations

### Current Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| CREATE USER | Missing | Not implemented in MySQL parser |
| ALTER USER | Missing | Not implemented |
| DROP USER | Missing | Not implemented |
| CREATE ROLE | Missing | Not implemented |
| GRANT (privileges) | Missing | Not implemented |
| GRANT (roles) | Missing | Not implemented |
| REVOKE | Missing | Not implemented |
| SET ROLE | Missing | Not implemented |
| SHOW GRANTS | Missing | Not implemented |
| mysql.user table | Missing | Not emulated |

### Workarounds

**For User Management:** Use one of these alternatives:

1. **PostgreSQL Emulation Mode:**
   Connect via port 5432 and use PostgreSQL DCL syntax:
   ```sql
   CREATE ROLE app_user WITH LOGIN PASSWORD 'password';
   GRANT SELECT ON users TO app_user;
   ```

2. **Native ScratchBird Commands:**
   Connect via native port (3092) and use native security commands.

3. **Configuration-Based Access:**
   Configure access control in `sb_hba.conf`:
   ```
   # TYPE  DATABASE  USER       ADDRESS        METHOD
   host    mydb      app_user   192.168.1.0/24 scram-sha-256
   ```

**For Application Integration:** Most MySQL applications can be configured to use the PostgreSQL driver instead, which provides full security command support.

---

## See Also

- [Admin Security Guide](../../admin/security.md) - Server security configuration
- [User Management](../../admin/user-management.md) - Administrative user management
- [PostgreSQL Security DCL](../postgresql/09_security_dcl.md) - PostgreSQL security (more complete)
- [System Catalog](13_system_catalog.md) - MySQL system tables

