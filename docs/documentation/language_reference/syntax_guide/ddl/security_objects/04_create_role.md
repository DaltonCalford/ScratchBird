<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# CREATE ROLE

[Prev](./03_drop_user.md) | [Next](./05_alter_role.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

Creates a role for grouping privileges and users. Roles are environment-scoped authorization containers.

## Syntax

```sql
CREATE ROLE [ environment_path ] role_name
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
    | ROLE role_name [, ...]
    | ADMIN role_name [, ...]
    | USER role_name [, ...]
    | SYSID uid
```

## User vs Role

| Aspect | User | Role |
|--------|------|------|
| Primary use | Individual identity | Privilege grouping |
| LOGIN default | YES (typically) | NO |
| Password | Usually | Rarely |
| Membership | Can be member of roles | Can be member of roles |
| Environment scope | Environment-scoped | Environment-scoped |

In practice, `CREATE USER` is equivalent to `CREATE ROLE WITH LOGIN`.

## Examples

### Basic Role

```sql
-- Read-only role
CREATE ROLE readonly;

-- Grant privileges to role
GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly;
```

### Login Role (User Equivalent)

```sql
-- Role that can login (same as CREATE USER)
CREATE ROLE app_role WITH LOGIN PASSWORD 'secret';
```

### Privilege Roles

```sql
-- Read-write role
CREATE ROLE readwrite;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES TO readwrite;

-- DDL role
CREATE ROLE ddl_creator WITH CREATEDB CREATEROLE;
```

### Nested Roles

```sql
-- Admin role includes readwrite
CREATE ROLE admin;
GRANT readwrite TO admin;
GRANT readonly TO admin;
```

### Environment-Scoped Role

```sql
-- Production role
CREATE ROLE !:prod.readonly;

-- Development role
CREATE ROLE !:dev.readwrite WITH CREATEDB;
```

## Parser Acceptance Cases

```sql
CREATE ROLE r1;
CREATE ROLE r1 WITH LOGIN;
CREATE ROLE !:prod.r1;
```

## See Also

- [CREATE USER](01_create_user.md)
- [DROP ROLE](06_drop_role.md)
- [GRANT](../../security/README.md)
