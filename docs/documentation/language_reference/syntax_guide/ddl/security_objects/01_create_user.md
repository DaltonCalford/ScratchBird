<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# CREATE USER

[Prev](./README.md) | [Next](./02_alter_user.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

Creates a new database user with authentication credentials and privileges.

## Syntax

```sql
CREATE USER [ environment_path ] user_name
    [ [ WITH ] option [ ... ] ]

where option can be:
    SUPERUSER | NOSUPERUSER
    | CREATEDB | NOCREATEDB
    | CREATEROLE | NOCREATEROLE
    | INHERIT | NOINHERIT
    | LOGIN | NOLOGIN
    | REPLICATION | NOREPLICATION
    | BYPASSRLS | NOBYPASSRLS
    | CONNECTION LIMIT connlimit
    | [ ENCRYPTED ] PASSWORD 'password' | PASSWORD NULL
    | VALID UNTIL 'timestamp'
    | IN ROLE role_name [, ...]
    | IN GROUP group_name [, ...]
    | ROLE role_name [, ...]
    | ADMIN role_name [, ...]
    | USER role_name [, ...]
    | SYSID uid
    | PROFILE profile_name
    | AUTH plugin_name [ USING 'auth_options' ]
```

## Parameters

| Parameter | Description |
|-----------|-------------|
| `environment_path` | Optional path: `!:env.name` or `.:name` |
| `user_name` | Unique identifier for the user |
| `SUPERUSER` | Bypass all access restrictions |
| `CREATEDB` | Can create databases |
| `CREATEROLE` | Can create roles/users |
| `INHERIT` | Inherit privileges from granted roles |
| `LOGIN` | Can connect to database |
| `REPLICATION` | Can initiate replication |
| `BYPASSRLS` | Bypass row-level security |
| `CONNECTION LIMIT` | Max concurrent connections |
| `PASSWORD` | Authentication password |
| `VALID UNTIL` | Password expiration timestamp |
| `IN ROLE` | Member of specified roles (with admin option from role) |
| `IN GROUP` | Member of specified groups (legacy) |
| `ROLE` | Roles granted to this user |
| `ADMIN` | Roles this user is admin of |
| `PROFILE` | Quota and resource limits |
| `AUTH` | Authentication plugin to use |

## Description

Users are environment-scoped security principals with:
- A Master UUID (universal identity)
- Authentication credentials
- Privilege attributes
- Role memberships

### Environment-Scoped Users

Users exist within an environment path:

```sql
-- Create in specific environment
CREATE USER !:prod.emulated_pg.john WITH PASSWORD 'secret';

-- Creates: !:prod.emulated_pg.john
-- Master UUID: maps to single SB identity
```

All emulated database users for the same person map to the same Master UUID.

## Examples

### Basic User

```sql
-- Simple user with password
CREATE USER app_user WITH PASSWORD 'app_password';

-- User that can login
CREATE USER web_app WITH LOGIN PASSWORD 'web_secret';
```

### Privileged User

```sql
-- Database administrator
CREATE USER admin WITH 
    SUPERUSER 
    CREATEDB 
    CREATEROLE 
    LOGIN 
    PASSWORD 'admin_pass';
```

### Application User

```sql
-- Application service account
CREATE USER reporting_service WITH
    LOGIN
    NOCREATEDB
    NOCREATEROLE
    CONNECTION LIMIT 50
    PASSWORD 'service_secret'
    VALID UNTIL '2025-12-31';
```

### Emulated Database User

```sql
-- PostgreSQL emulation user
CREATE USER !:prod.emulated_pg.app_user
    WITH LOGIN PASSWORD 'pg_compat_pass';

-- MySQL emulation user
CREATE USER !:prod.emulated_mysql.app_user
    WITH LOGIN PASSWORD 'mysql_compat_pass';

-- Both map to same Master UUID internally
```

### With Role Membership

```sql
-- User with read-only role
CREATE USER analyst WITH
    LOGIN
    PASSWORD 'analyst_pass'
    IN ROLE readonly_role;

-- Admin of a role
CREATE USER security_admin WITH
    LOGIN
    PASSWORD 'admin_pass'
    ADMIN user_manager_role;
```

### With Authentication Plugin

```sql
-- LDAP authentication
CREATE USER corp_user WITH
    LOGIN
    AUTH ldap USING 'cn=corp_user,dc=company,dc=com';

-- Certificate authentication
CREATE USER api_client WITH
    LOGIN
    AUTH certificate USING 'CN=api.client.com';

-- Kerberos
CREATE USER domain_user WITH
    LOGIN
    AUTH kerberos USING 'user@REALM.COM';
```

### Without Password (External Auth)

```sql
-- Peer authentication (OS user match)
CREATE USER os_user WITH LOGIN PASSWORD NULL AUTH peer;

-- Trust authentication (no password, local only)
CREATE USER local_admin WITH LOGIN PASSWORD NULL AUTH trust;
```

## Parser Acceptance Cases

```sql
CREATE USER u1 WITH PASSWORD 'pass';
CREATE USER u1 WITH LOGIN PASSWORD 'pass';
CREATE USER u1 WITH SUPERUSER PASSWORD 'pass';
CREATE USER !:prod.u1 WITH PASSWORD 'pass';
CREATE USER u1 WITH PASSWORD 'pass' VALID UNTIL '2025-01-01';
```

## Parser Rejection Cases

```sql
-- Reserved name
CREATE USER PUBLIC WITH PASSWORD 'pass';  -- Error: reserved name

-- Empty password without external auth
CREATE USER u1 WITH PASSWORD '';  -- Error: empty password
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `duplicate_user` | User exists |
| `reserved_name` | Using reserved identifier |
| `invalid_password` | Password policy violation |
| `invalid_auth_plugin` | Unknown authentication method |

## See Also

- [ALTER USER](02_alter_user.md)
- [DROP USER](03_drop_user.md)
- [CREATE ROLE](04_create_role.md)
- [GRANT](../../security/README.md)
- [Authentication Plugins](../../../../how-to-guide/security_and_access/authentication_plugins/README.md)
