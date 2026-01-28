# User Management

**Last Updated:** 2026-01-28

---

## Overview

User management in ScratchBird follows standard SQL patterns for creating, modifying, and managing database users and roles. This guide covers user creation, role management, permissions, and best practices.

**Topics covered:**
- Creating and managing users
- Role-based access control
- Permission management
- Password policies
- User administration queries

---

## Part 1: Users vs Roles

### Understanding the Difference

In ScratchBird, users and roles are essentially the same object with different default attributes:

| Attribute | User (CREATE USER) | Role (CREATE ROLE) |
|-----------|-------------------|-------------------|
| LOGIN | Yes (default) | No (default) |
| INHERIT | Yes (default) | Yes (default) |

**Practical usage:**
- **Users**: Accounts that connect to the database
- **Roles**: Permission groups assigned to users

```sql
-- These are equivalent:
CREATE USER app_user WITH PASSWORD 'secret';
CREATE ROLE app_user WITH LOGIN PASSWORD 'secret';

-- This creates a role that cannot login (permission group):
CREATE ROLE readonly;
```

---

## Part 2: Creating Users

### Basic User Creation

```sql
-- Create user with password
CREATE USER app_user WITH PASSWORD 'SecureP@ssw0rd!';

-- Create user with specific attributes
CREATE USER report_user WITH
    PASSWORD 'SecureP@ssw0rd!'
    VALID UNTIL '2027-01-01'
    CONNECTION LIMIT 5;

-- Create superuser (use sparingly)
CREATE USER admin_user WITH
    PASSWORD 'VerySecureP@ssw0rd!'
    SUPERUSER
    CREATEDB
    CREATEROLE;
```

### User Attributes

| Attribute | Description |
|-----------|-------------|
| `PASSWORD 'pass'` | Set password |
| `SUPERUSER` / `NOSUPERUSER` | Superuser privileges |
| `CREATEDB` / `NOCREATEDB` | Can create databases |
| `CREATEROLE` / `NOCREATEROLE` | Can create roles |
| `LOGIN` / `NOLOGIN` | Can login |
| `REPLICATION` / `NOREPLICATION` | Can initiate replication |
| `CONNECTION LIMIT n` | Max connections (-1 = unlimited) |
| `VALID UNTIL 'timestamp'` | Password expiry |
| `IN ROLE role_name` | Inherit role on creation |

### Modifying Users

```sql
-- Change password
ALTER USER app_user WITH PASSWORD 'NewSecureP@ssw0rd!';

-- Set password expiry
ALTER USER app_user VALID UNTIL '2027-06-01';

-- Change connection limit
ALTER USER app_user CONNECTION LIMIT 10;

-- Grant/revoke attributes
ALTER USER app_user CREATEDB;
ALTER USER app_user NOCREATEDB;

-- Rename user
ALTER USER old_name RENAME TO new_name;

-- Disable login (lock account)
ALTER USER app_user NOLOGIN;

-- Re-enable login
ALTER USER app_user LOGIN;
```

### Deleting Users

```sql
-- Drop user (must have no owned objects)
DROP USER app_user;

-- Drop user and reassign owned objects
REASSIGN OWNED BY app_user TO admin;
DROP OWNED BY app_user;
DROP USER app_user;

-- Check what a user owns before dropping
SELECT
    n.nspname AS schema,
    c.relname AS object_name,
    c.relkind AS type
FROM pg_class c
JOIN pg_namespace n ON c.relnamespace = n.oid
JOIN pg_roles r ON c.relowner = r.oid
WHERE r.rolname = 'app_user';
```

---

## Part 3: Creating Roles

### Permission Roles

```sql
-- Read-only role
CREATE ROLE readonly;
GRANT CONNECT ON DATABASE mydb TO readonly;
GRANT USAGE ON SCHEMA public TO readonly;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly;
ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT SELECT ON TABLES TO readonly;

-- Read-write role
CREATE ROLE readwrite;
GRANT CONNECT ON DATABASE mydb TO readwrite;
GRANT USAGE ON SCHEMA public TO readwrite;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO readwrite;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO readwrite;
ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO readwrite;
ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT USAGE, SELECT ON SEQUENCES TO readwrite;

-- Admin role (no superuser)
CREATE ROLE db_admin;
GRANT CONNECT ON DATABASE mydb TO db_admin;
GRANT ALL ON SCHEMA public TO db_admin;
GRANT ALL ON ALL TABLES IN SCHEMA public TO db_admin;
GRANT ALL ON ALL SEQUENCES IN SCHEMA public TO db_admin;
GRANT ALL ON ALL FUNCTIONS IN SCHEMA public TO db_admin;
```

### Assigning Roles to Users

```sql
-- Grant role to user
GRANT readonly TO report_user;
GRANT readwrite TO app_user;
GRANT db_admin TO dba_user;

-- Grant multiple roles
GRANT readonly, readwrite TO power_user;

-- Grant with admin option (can grant to others)
GRANT readwrite TO team_lead WITH ADMIN OPTION;

-- Revoke role
REVOKE readwrite FROM app_user;
```

### Role Inheritance

```sql
-- Create hierarchical roles
CREATE ROLE base_access;
CREATE ROLE extended_access;
CREATE ROLE full_access;

GRANT base_access TO extended_access;
GRANT extended_access TO full_access;

-- User with full_access inherits all permissions
GRANT full_access TO admin_user;

-- Disable inheritance for a user
ALTER USER special_user NOINHERIT;
-- Must explicitly SET ROLE to use granted roles
```

---

## Part 4: Permission Management

### Database Permissions

```sql
-- Grant connect to database
GRANT CONNECT ON DATABASE mydb TO app_user;

-- Grant create schema permission
GRANT CREATE ON DATABASE mydb TO db_admin;

-- Grant all database permissions
GRANT ALL ON DATABASE mydb TO owner_user;

-- Revoke all from public
REVOKE ALL ON DATABASE mydb FROM PUBLIC;
```

### Schema Permissions

```sql
-- Grant usage (access objects in schema)
GRANT USAGE ON SCHEMA public TO app_user;

-- Grant create (create objects in schema)
GRANT CREATE ON SCHEMA app TO app_user;

-- Grant all schema permissions
GRANT ALL ON SCHEMA app TO db_admin;
```

### Table Permissions

```sql
-- Individual permissions
GRANT SELECT ON users TO report_user;
GRANT INSERT ON orders TO app_user;
GRANT UPDATE ON products TO app_user;
GRANT DELETE ON temp_data TO app_user;
GRANT TRUNCATE ON log_table TO db_admin;
GRANT REFERENCES ON users TO app_user;  -- For foreign keys
GRANT TRIGGER ON orders TO db_admin;

-- Multiple permissions
GRANT SELECT, INSERT, UPDATE ON orders TO app_user;

-- All permissions
GRANT ALL ON orders TO db_admin;

-- All tables in schema
GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly;
GRANT ALL ON ALL TABLES IN SCHEMA app TO app_user;

-- Column-level permissions
GRANT SELECT (id, name, email) ON users TO limited_user;
GRANT UPDATE (status) ON orders TO status_updater;
```

### Sequence Permissions

```sql
-- Grant usage (nextval, currval)
GRANT USAGE ON SEQUENCE users_id_seq TO app_user;

-- Grant all sequence permissions
GRANT ALL ON SEQUENCE users_id_seq TO db_admin;

-- All sequences in schema
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO app_user;
```

### Function Permissions

```sql
-- Grant execute
GRANT EXECUTE ON FUNCTION calculate_total(integer) TO app_user;

-- All functions in schema
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public TO app_user;
```

### Default Privileges

Set permissions for future objects:

```sql
-- Future tables created by any user
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT ON TABLES TO readonly;

-- Future tables created by specific user
ALTER DEFAULT PRIVILEGES FOR ROLE db_admin IN SCHEMA public
    GRANT ALL ON TABLES TO app_user;

-- Future sequences
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT USAGE ON SEQUENCES TO app_user;

-- Future functions
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT EXECUTE ON FUNCTIONS TO app_user;
```

---

## Part 5: Schema-Based Multi-Tenancy

### Create Tenant Schemas

```sql
-- Create schema for each tenant
CREATE SCHEMA tenant_a;
CREATE SCHEMA tenant_b;
CREATE SCHEMA tenant_c;

-- Create tenant users
CREATE USER tenant_a_user WITH PASSWORD 'secret_a';
CREATE USER tenant_b_user WITH PASSWORD 'secret_b';
CREATE USER tenant_c_user WITH PASSWORD 'secret_c';

-- Grant permissions
GRANT USAGE, CREATE ON SCHEMA tenant_a TO tenant_a_user;
GRANT ALL ON ALL TABLES IN SCHEMA tenant_a TO tenant_a_user;
ALTER DEFAULT PRIVILEGES IN SCHEMA tenant_a GRANT ALL ON TABLES TO tenant_a_user;

-- Set default schema (search path)
ALTER USER tenant_a_user SET search_path TO tenant_a, public;
ALTER USER tenant_b_user SET search_path TO tenant_b, public;
ALTER USER tenant_c_user SET search_path TO tenant_c, public;
```

### Tenant Isolation Script

```sql
-- Function to create a new tenant
CREATE OR REPLACE FUNCTION create_tenant(
    tenant_name TEXT,
    user_password TEXT
) RETURNS VOID AS $$
DECLARE
    schema_name TEXT := 'tenant_' || tenant_name;
    user_name TEXT := tenant_name || '_user';
BEGIN
    -- Create schema
    EXECUTE format('CREATE SCHEMA %I', schema_name);

    -- Create user
    EXECUTE format('CREATE USER %I WITH PASSWORD %L', user_name, user_password);

    -- Grant permissions
    EXECUTE format('GRANT USAGE, CREATE ON SCHEMA %I TO %I', schema_name, user_name);
    EXECUTE format('GRANT ALL ON ALL TABLES IN SCHEMA %I TO %I', schema_name, user_name);
    EXECUTE format('ALTER DEFAULT PRIVILEGES IN SCHEMA %I GRANT ALL ON TABLES TO %I', schema_name, user_name);
    EXECUTE format('ALTER DEFAULT PRIVILEGES IN SCHEMA %I GRANT ALL ON SEQUENCES TO %I', schema_name, user_name);

    -- Set search path
    EXECUTE format('ALTER USER %I SET search_path TO %I, public', user_name, schema_name);

    RAISE NOTICE 'Created tenant: %, user: %', schema_name, user_name;
END;
$$ LANGUAGE plpgsql;

-- Usage
SELECT create_tenant('acme', 'AcmeP@ssw0rd!');
SELECT create_tenant('globex', 'GlobexP@ssw0rd!');
```

---

## Part 6: Application Users

### Best Practices for Application Users

```sql
-- Create application-specific role
CREATE ROLE myapp_role;

-- Grant minimum necessary permissions
GRANT CONNECT ON DATABASE production TO myapp_role;
GRANT USAGE ON SCHEMA public TO myapp_role;
GRANT SELECT, INSERT, UPDATE, DELETE ON
    users, orders, products, order_items
TO myapp_role;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO myapp_role;

-- Create application user
CREATE USER myapp_user WITH PASSWORD 'AppP@ssw0rd!';
GRANT myapp_role TO myapp_user;

-- Set connection limit
ALTER USER myapp_user CONNECTION LIMIT 50;

-- Set statement timeout for runaway queries
ALTER USER myapp_user SET statement_timeout = '30s';
```

### Service Account Pattern

```sql
-- Read-only service account
CREATE USER readonly_svc WITH PASSWORD 'ReadOnlyP@ss!';
GRANT readonly TO readonly_svc;
ALTER USER readonly_svc CONNECTION LIMIT 20;
ALTER USER readonly_svc SET statement_timeout = '60s';

-- ETL service account
CREATE USER etl_svc WITH PASSWORD 'EtlP@ss!';
GRANT readwrite TO etl_svc;
ALTER USER etl_svc CONNECTION LIMIT 5;
ALTER USER etl_svc SET work_mem = '256MB';  -- For large sorts

-- Backup service account
CREATE USER backup_svc WITH PASSWORD 'BackupP@ss!';
GRANT SELECT ON ALL TABLES IN SCHEMA public TO backup_svc;
ALTER USER backup_svc SET statement_timeout = '0';  -- No timeout for backups
```

---

## Part 7: Password Management

### Password Policies

Configure in `sb_server.conf`:
```ini
[authentication]
password_encryption = scram-sha-256
password_min_length = 12
password_require_uppercase = true
password_require_lowercase = true
password_require_digit = true
password_require_special = true
password_max_age_days = 90
password_history_count = 5
```

### Password Expiry

```sql
-- Set password expiry
ALTER USER app_user VALID UNTIL '2026-06-01';

-- Check password expiry
SELECT
    usename,
    valuntil AS password_expires
FROM pg_user
WHERE valuntil IS NOT NULL
ORDER BY valuntil;

-- Find expiring passwords (within 30 days)
SELECT
    usename,
    valuntil AS expires,
    valuntil - CURRENT_DATE AS days_until_expiry
FROM pg_user
WHERE valuntil IS NOT NULL
AND valuntil < CURRENT_DATE + INTERVAL '30 days'
ORDER BY valuntil;
```

### Force Password Change

```sql
-- Lock account until password is changed
ALTER USER app_user NOLOGIN;

-- After user provides new password
ALTER USER app_user WITH PASSWORD 'NewSecureP@ss!';
ALTER USER app_user LOGIN;
ALTER USER app_user VALID UNTIL '2026-12-31';
```

### Password Reset Script

```bash
#!/bin/bash
# Reset user password

USER=$1
NEW_PASSWORD=$(openssl rand -base64 16)

sb_isql -U admin -d postgres -c "ALTER USER $USER WITH PASSWORD '$NEW_PASSWORD'"

echo "Password for $USER has been reset."
echo "New password: $NEW_PASSWORD"
echo "Please provide this securely to the user."
```

---

## Part 8: User Administration Queries

### List Users

```sql
-- All users
SELECT
    usename AS username,
    usesysid AS user_id,
    usecreatedb AS can_create_db,
    usesuper AS is_superuser,
    valuntil AS password_expires
FROM pg_user
ORDER BY usename;

-- Users with their roles
SELECT
    r.rolname AS username,
    ARRAY_AGG(m.rolname) AS member_of
FROM pg_roles r
LEFT JOIN pg_auth_members am ON r.oid = am.member
LEFT JOIN pg_roles m ON am.roleid = m.oid
WHERE r.rolcanlogin = true
GROUP BY r.rolname
ORDER BY r.rolname;
```

### List Roles

```sql
-- All roles (including users)
SELECT
    rolname,
    rolsuper,
    rolcreaterole,
    rolcreatedb,
    rolcanlogin,
    rolconnlimit,
    rolvaliduntil
FROM pg_roles
ORDER BY rolcanlogin DESC, rolname;

-- Role hierarchy
WITH RECURSIVE role_tree AS (
    SELECT
        r.rolname,
        r.rolcanlogin,
        0 AS level,
        r.rolname::text AS path
    FROM pg_roles r
    WHERE NOT EXISTS (
        SELECT 1 FROM pg_auth_members am
        WHERE am.member = r.oid
    )

    UNION ALL

    SELECT
        r.rolname,
        r.rolcanlogin,
        rt.level + 1,
        rt.path || ' -> ' || r.rolname
    FROM pg_roles r
    JOIN pg_auth_members am ON r.oid = am.member
    JOIN role_tree rt ON am.roleid = (SELECT oid FROM pg_roles WHERE rolname = rt.rolname)
)
SELECT * FROM role_tree ORDER BY path;
```

### Check Permissions

```sql
-- Table permissions for a user
SELECT
    table_schema,
    table_name,
    privilege_type
FROM information_schema.table_privileges
WHERE grantee = 'app_user'
ORDER BY table_schema, table_name;

-- Check if user has specific permission
SELECT has_table_privilege('app_user', 'public.users', 'SELECT');
SELECT has_table_privilege('app_user', 'public.users', 'INSERT');
SELECT has_schema_privilege('app_user', 'public', 'USAGE');
SELECT has_database_privilege('app_user', 'mydb', 'CONNECT');

-- All permissions on a specific table
SELECT
    grantee,
    privilege_type,
    is_grantable
FROM information_schema.table_privileges
WHERE table_schema = 'public'
AND table_name = 'users'
ORDER BY grantee, privilege_type;
```

### Active Sessions

```sql
-- Current user sessions
SELECT
    usename AS username,
    datname AS database,
    client_addr,
    state,
    query_start,
    NOW() - query_start AS query_duration,
    LEFT(query, 50) AS current_query
FROM pg_stat_activity
WHERE backend_type = 'client backend'
ORDER BY query_start;

-- Connection count by user
SELECT
    usename AS username,
    COUNT(*) AS connections
FROM pg_stat_activity
WHERE backend_type = 'client backend'
GROUP BY usename
ORDER BY connections DESC;

-- Terminate user's sessions
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE usename = 'app_user'
AND pid != pg_backend_pid();
```

### User Activity

```sql
-- Recent activity by user (requires pg_stat_statements)
SELECT
    usename,
    COUNT(*) AS query_count,
    SUM(calls) AS total_calls,
    ROUND(SUM(total_exec_time)::numeric, 2) AS total_time_ms
FROM pg_stat_statements ss
JOIN pg_user u ON ss.userid = u.usesysid
GROUP BY usename
ORDER BY total_time_ms DESC;
```

---

## Part 9: Automation Scripts

### User Provisioning Script

```bash
#!/bin/bash
# provision_user.sh - Create new database user

set -euo pipefail

usage() {
    echo "Usage: $0 -u USERNAME -r ROLE [-d DATABASE] [-c CONN_LIMIT]"
    exit 1
}

# Defaults
DATABASE="production"
CONN_LIMIT=10

while getopts "u:r:d:c:" opt; do
    case $opt in
        u) USERNAME=$OPTARG ;;
        r) ROLE=$OPTARG ;;
        d) DATABASE=$OPTARG ;;
        c) CONN_LIMIT=$OPTARG ;;
        *) usage ;;
    esac
done

if [ -z "${USERNAME:-}" ] || [ -z "${ROLE:-}" ]; then
    usage
fi

# Generate password
PASSWORD=$(openssl rand -base64 16)

# Create user
sb_isql -U admin -d postgres << EOF
CREATE USER $USERNAME WITH PASSWORD '$PASSWORD';
GRANT $ROLE TO $USERNAME;
ALTER USER $USERNAME CONNECTION LIMIT $CONN_LIMIT;
GRANT CONNECT ON DATABASE $DATABASE TO $USERNAME;
EOF

echo "User created successfully!"
echo "Username: $USERNAME"
echo "Password: $PASSWORD"
echo "Role: $ROLE"
echo "Database: $DATABASE"
echo "Connection Limit: $CONN_LIMIT"
```

### Bulk User Import

```bash
#!/bin/bash
# import_users.sh - Import users from CSV

# CSV format: username,role,connection_limit
CSV_FILE=$1

while IFS=',' read -r username role conn_limit; do
    PASSWORD=$(openssl rand -base64 16)

    sb_isql -U admin -d postgres << EOF
CREATE USER $username WITH PASSWORD '$PASSWORD';
GRANT $role TO $username;
ALTER USER $username CONNECTION LIMIT $conn_limit;
EOF

    echo "$username,$PASSWORD" >> passwords.csv
done < "$CSV_FILE"

echo "Users imported. Passwords saved to passwords.csv"
```

### Permission Audit Script

```bash
#!/bin/bash
# audit_permissions.sh - Generate permission report

sb_isql -U admin -d mydb << 'EOF'
\echo '=== SUPERUSERS ==='
SELECT usename FROM pg_user WHERE usesuper = true;

\echo ''
\echo '=== USERS WITH CREATEROLE ==='
SELECT rolname FROM pg_roles WHERE rolcreaterole = true;

\echo ''
\echo '=== USER ROLE MEMBERSHIPS ==='
SELECT
    r.rolname AS user_name,
    ARRAY_AGG(m.rolname) AS roles
FROM pg_roles r
JOIN pg_auth_members am ON r.oid = am.member
JOIN pg_roles m ON am.roleid = m.oid
WHERE r.rolcanlogin = true
GROUP BY r.rolname
ORDER BY r.rolname;

\echo ''
\echo '=== TABLE PERMISSIONS ==='
SELECT
    grantee,
    table_schema,
    table_name,
    STRING_AGG(privilege_type, ', ') AS privileges
FROM information_schema.table_privileges
WHERE table_schema NOT IN ('pg_catalog', 'information_schema')
GROUP BY grantee, table_schema, table_name
ORDER BY grantee, table_schema, table_name;
EOF
```

---

## Quick Reference

### Common Commands

```sql
-- Create user
CREATE USER app_user WITH PASSWORD 'SecureP@ss!';

-- Create role
CREATE ROLE readonly;

-- Grant role to user
GRANT readonly TO app_user;

-- Grant table permissions
GRANT SELECT, INSERT ON users TO app_user;

-- Grant all on schema
GRANT ALL ON ALL TABLES IN SCHEMA public TO db_admin;

-- Change password
ALTER USER app_user WITH PASSWORD 'NewP@ss!';

-- Lock account
ALTER USER app_user NOLOGIN;

-- Delete user
REASSIGN OWNED BY app_user TO admin;
DROP OWNED BY app_user;
DROP USER app_user;
```

### Permission Levels

| Level | Grant Command |
|-------|---------------|
| Database | `GRANT CONNECT ON DATABASE db TO user` |
| Schema | `GRANT USAGE ON SCHEMA schema TO user` |
| Table | `GRANT SELECT ON table TO user` |
| Column | `GRANT SELECT (col) ON table TO user` |
| Sequence | `GRANT USAGE ON SEQUENCE seq TO user` |
| Function | `GRANT EXECUTE ON FUNCTION func TO user` |

### Table Privileges

| Privilege | Description |
|-----------|-------------|
| SELECT | Read rows |
| INSERT | Add rows |
| UPDATE | Modify rows |
| DELETE | Remove rows |
| TRUNCATE | Empty table |
| REFERENCES | Create foreign key |
| TRIGGER | Create trigger |
| ALL | All privileges |

---

## See Also

- [Security Administration](security.md)
- [Monitoring](monitoring.md)
- [Troubleshooting](troubleshooting.md)
- [Basic SQL Guide](../getting-started/basic-sql.md)

