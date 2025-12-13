# User Management

Create and manage users, roles, and permissions.

[Back to Admin Index](index.md) | [Back to Documentation Index](../index.md)

---

## User Concepts

| Concept | Description |
|---------|-------------|
| **User** | Login account with password |
| **Role** | Named group of privileges |
| **Privilege** | Permission to perform an action |
| **Grant** | Assign privilege to user/role |

---

## Creating Users

### Basic User

```sql
CREATE USER myuser WITH PASSWORD 'secure_password_123';
```

### User with Options

```sql
CREATE USER appuser WITH
    PASSWORD 'secure_password_123'
    LOGIN                    -- Can log in (default)
    CREATEDB                 -- Can create databases
    CONNECTION LIMIT 10;     -- Max connections
```

### User with Expiration

```sql
CREATE USER tempuser WITH
    PASSWORD 'temp_pass'
    VALID UNTIL '2024-12-31';
```

---

## Managing Passwords

### Change Password

```sql
-- Change another user's password (as admin)
ALTER USER myuser WITH PASSWORD 'new_secure_password';

-- Change own password
ALTER USER CURRENT_USER WITH PASSWORD 'new_password';
```

### Using sb_security

```bash
# Set password interactively
sb_security password myuser

# Reset password
sb_security password myuser --new-password
```

### Password Requirements

Configured in `sb_server.conf`:

```ini
[authentication]
password_min_length = 12
password_hash = argon2id
```

---

## Roles

### Create Role

```sql
-- Role without login
CREATE ROLE developers;

-- Role with login
CREATE ROLE dba WITH LOGIN PASSWORD 'dba_password';
```

### Predefined Roles

| Role | Description |
|------|-------------|
| `admin` | Superuser with all privileges |
| `public` | All users belong to this |

### Grant Role to User

```sql
-- Add user to role
GRANT developers TO myuser;

-- Multiple roles
GRANT developers, readers TO myuser;
```

### Revoke Role

```sql
REVOKE developers FROM myuser;
```

---

## Database Privileges

### Grant Database Access

```sql
-- All privileges on database
GRANT ALL ON DATABASE mydb TO myuser;

-- Connect only
GRANT CONNECT ON DATABASE mydb TO myuser;

-- Create schemas
GRANT CREATE ON DATABASE mydb TO myuser;
```

### Revoke Database Access

```sql
REVOKE ALL ON DATABASE mydb FROM myuser;
```

---

## Schema Privileges

### Grant Schema Access

```sql
-- All privileges on schema
GRANT ALL ON SCHEMA public TO myuser;

-- Usage only (can use objects, not create)
GRANT USAGE ON SCHEMA public TO myuser;

-- Create objects
GRANT CREATE ON SCHEMA public TO myuser;
```

---

## Table Privileges

### Grant Table Access

```sql
-- All privileges
GRANT ALL ON TABLE customers TO myuser;

-- Specific privileges
GRANT SELECT, INSERT ON TABLE customers TO myuser;

-- All tables in schema
GRANT SELECT ON ALL TABLES IN SCHEMA public TO myuser;
```

### Available Table Privileges

| Privilege | Description |
|-----------|-------------|
| `SELECT` | Read rows |
| `INSERT` | Add rows |
| `UPDATE` | Modify rows |
| `DELETE` | Remove rows |
| `TRUNCATE` | Empty table |
| `REFERENCES` | Create foreign keys |
| `TRIGGER` | Create triggers |
| `ALL` | All of the above |

### Column-Level Privileges

```sql
-- Grant access to specific columns
GRANT SELECT (id, name, email) ON customers TO myuser;
GRANT UPDATE (email, phone) ON customers TO myuser;
```

---

## Default Privileges

Set privileges for future objects:

```sql
-- Future tables in schema
ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT SELECT ON TABLES TO readers;

-- Future sequences
ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT USAGE ON SEQUENCES TO appuser;
```

---

## Viewing Users and Privileges

### List Users

```sql
-- All users
SELECT * FROM pg_user;

-- Or in sb_isql
\du
```

### List Roles

```sql
SELECT rolname, rolsuper, rolcreatedb, rolcanlogin
FROM pg_roles;
```

### View User Privileges

```sql
-- Database privileges
SELECT * FROM information_schema.role_table_grants
WHERE grantee = 'myuser';

-- Table privileges
\dp tablename  -- in sb_isql
```

---

## Common Patterns

### Read-Only User

```sql
-- Create user
CREATE USER readonly WITH PASSWORD 'readonly_pass';

-- Grant connect
GRANT CONNECT ON DATABASE mydb TO readonly;

-- Grant schema usage
GRANT USAGE ON SCHEMA public TO readonly;

-- Grant select on all tables
GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly;

-- Grant for future tables
ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT SELECT ON TABLES TO readonly;
```

### Application User

```sql
-- Create user
CREATE USER appuser WITH PASSWORD 'app_password';

-- Grant connect
GRANT CONNECT ON DATABASE mydb TO appuser;

-- Grant usage
GRANT USAGE ON SCHEMA public TO appuser;

-- Grant CRUD operations
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO appuser;

-- Grant sequence usage (for auto-increment)
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO appuser;
```

### DBA User

```sql
-- Create DBA role
CREATE ROLE dba WITH
    LOGIN
    PASSWORD 'dba_password'
    CREATEDB
    CREATEROLE;

-- Grant all on existing databases
GRANT ALL ON DATABASE mydb TO dba;
```

---

## Account Lockout

### Manual Lock

```sql
-- Disable login
ALTER USER baduser WITH NOLOGIN;
```

### Manual Unlock

```sql
ALTER USER baduser WITH LOGIN;
```

### Automatic Lockout

Configured in `sb_server.conf`:

```ini
[authentication]
max_failed_attempts = 5
lockout_duration = 300  # seconds
```

---

## Dropping Users

### Drop User

```sql
DROP USER myuser;
```

### Drop with Dependencies

```sql
-- Reassign objects first
REASSIGN OWNED BY olduser TO newuser;

-- Drop owned objects
DROP OWNED BY olduser;

-- Now drop user
DROP USER olduser;
```

---

## Security Best Practices

1. **Strong passwords** - Use password manager, 16+ characters
2. **Principle of least privilege** - Only grant needed access
3. **Use roles** - Group permissions, easier management
4. **Regular audits** - Review who has access
5. **No shared accounts** - Each user gets own account
6. **Expire unused accounts** - Clean up old users
7. **Separate application users** - Don't use admin for apps

---

## Using sb_security

### List Users

```bash
sb_security list-users
```

### Create User

```bash
sb_security create-user myuser
```

### Change Password

```bash
sb_security password myuser
```

### Lock/Unlock

```bash
sb_security lock-user myuser
sb_security unlock-user myuser
```

---

## Next Steps

- [Configure authentication](../configuration/hba.conf.md)
- [Security best practices](security.md)
- [Audit logging](monitoring.md)
