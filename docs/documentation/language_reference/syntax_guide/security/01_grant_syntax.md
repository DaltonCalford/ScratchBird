<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# GRANT

[Prev](./README.md) | [Next](./02_revoke_syntax.md) | [Topic README](./README.md) | [Syntax Guide README](../README.md) | [Language Reference README](../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

Grants privileges on database objects to users or roles.

## Syntax

```sql
-- Table/View privileges
GRANT { { SELECT | INSERT | UPDATE | DELETE | TRUNCATE | REFERENCES | TRIGGER }
    [, ...] | ALL [ PRIVILEGES ] }
    ON { [ TABLE ] table_name [, ...]
       | ALL TABLES IN SCHEMA schema_name [, ...] }
    TO { [ GROUP ] role_name | PUBLIC } [, ...] 
    [ WITH GRANT OPTION ]

-- Column privileges
GRANT { { SELECT | INSERT | UPDATE | REFERENCES } ( column_name [, ...] ) [, ...]
    | ALL [ PRIVILEGES ] ( column_name [, ...] ) }
    ON [ TABLE ] table_name [, ...]
    TO { [ GROUP ] role_name | PUBLIC } [, ...]
    [ WITH GRANT OPTION ]

-- Sequence privileges
GRANT { { USAGE | SELECT | UPDATE }
    [, ...] | ALL [ PRIVILEGES ] }
    ON { SEQUENCE sequence_name [, ...]
       | ALL SEQUENCES IN SCHEMA schema_name [, ...] }
    TO { [ GROUP ] role_name | PUBLIC } [, ...]
    [ WITH GRANT OPTION ]

-- Database privileges
GRANT { { CREATE | CONNECT | TEMPORARY | TEMP } 
    [, ...] | ALL [ PRIVILEGES ] }
    ON DATABASE database_name [, ...]
    TO { [ GROUP ] role_name | PUBLIC } [, ...]
    [ WITH GRANT OPTION ]

-- Schema privileges
GRANT { { CREATE | USAGE } [, ...] | ALL [ PRIVILEGES ] }
    ON SCHEMA schema_name [, ...]
    TO { [ GROUP ] role_name | PUBLIC } [, ...]
    [ WITH GRANT OPTION ]

-- Function/Procedure privileges
GRANT { EXECUTE | ALL [ PRIVILEGES ] }
    ON { { FUNCTION | PROCEDURE | ROUTINE } routine_name [ ( [ [ argmode ] [ arg_name ] arg_type [, ...] ] ) ] [, ...]
       | ALL { FUNCTIONS | PROCEDURES | ROUTINES } IN SCHEMA schema_name [, ...] }
    TO { [ GROUP ] role_name | PUBLIC } [, ...]
    [ WITH GRANT OPTION ]

-- Language privileges
GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON LANGUAGE lang_name [, ...]
    TO { [ GROUP ] role_name | PUBLIC } [, ...]
    [ WITH GRANT OPTION ]

-- Tablespace privileges
GRANT { CREATE | USAGE } [, ...] | ALL [ PRIVILEGES ] }
    ON TABLESPACE tablespace_name [, ...]
    TO { [ GROUP ] role_name | PUBLIC } [, ...]
    [ WITH GRANT OPTION ]

-- Type privileges
GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON TYPE type_name [, ...]
    TO { [ GROUP ] role_name | PUBLIC } [, ...]
    [ WITH GRANT OPTION ]

-- Domain privileges
GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON DOMAIN domain_name [, ...]
    TO { [ GROUP ] role_name | PUBLIC } [, ...]
    [ WITH GRANT OPTION ]

-- Role membership
GRANT role_name [, ...] TO role_name [, ...]
    [ WITH { ADMIN | INHERIT | SET } OPTION ] [ GRANTED BY role_name ]
```

## Privilege Types

### Table Privileges

| Privilege | Description |
|-----------|-------------|
| `SELECT` | Read rows |
| `INSERT` | Add rows |
| `UPDATE` | Modify rows |
| `DELETE` | Remove rows |
| `TRUNCATE` | Empty table |
| `REFERENCES` | Create foreign key referencing this table |
| `TRIGGER` | Create triggers on table |
| `ALL` | All table privileges |

### Column Privileges

| Privilege | Description |
|-----------|-------------|
| `SELECT (col)` | Read specific column |
| `INSERT (col)` | Insert into specific column |
| `UPDATE (col)` | Update specific column |
| `REFERENCES (col)` | Reference column in FK |

### Schema Privileges

| Privilege | Description |
|-----------|-------------|
| `CREATE` | Create objects in schema |
| `USAGE` | Access objects in schema |

### Database Privileges

| Privilege | Description |
|-----------|-------------|
| `CREATE` | Create schemas/tables |
| `CONNECT` | Connect to database |
| `TEMPORARY` | Create temp tables |

## Examples

### Table Grants

```sql
-- Read-only access
GRANT SELECT ON users TO analyst_role;

-- Full table access
GRANT SELECT, INSERT, UPDATE, DELETE ON orders TO app_role;

-- All privileges
GRANT ALL ON products TO admin_role;

-- With grant option
GRANT SELECT ON users TO manager_role WITH GRANT OPTION;

-- Multiple tables
GRANT SELECT ON users, orders, products TO readonly_role;

-- All tables in schema
GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly_role;

-- Public access
GRANT SELECT ON categories TO PUBLIC;
```

### Column Grants

```sql
-- Grant access to specific columns
GRANT SELECT (id, name, email) ON users TO limited_user;

-- Update only specific columns
GRANT UPDATE (status, notes) ON orders TO support_role;

-- Multiple column privileges
GRANT SELECT (id, name), UPDATE (name) ON users TO partial_role;
```

### Schema Grants

```sql
-- Allow using schema objects
GRANT USAGE ON SCHEMA public TO app_user;

-- Allow creating in schema
GRANT CREATE, USAGE ON SCHEMA app_schema TO developer_role;
```

### Database Grants

```sql
-- Allow connection
GRANT CONNECT ON DATABASE myapp TO app_user;

-- Full database access
GRANT ALL ON DATABASE myapp TO admin_user;
```

### Role Membership

```sql
-- Grant role to user
GRANT readonly_role TO app_user;

-- Grant multiple roles
GRANT readonly_role, reporting_role TO analyst_user;

-- Grant with admin option
GRANT manager_role TO senior_user WITH ADMIN OPTION;

-- Role inheritance
GRANT admin_role TO superuser WITH INHERIT OPTION;
```

## WITH GRANT OPTION

Allows the recipient to grant the privilege to others.

```sql
-- Manager can grant SELECT to others
GRANT SELECT ON employees TO manager WITH GRANT OPTION;

-- Now manager can:
GRANT SELECT ON employees TO new_analyst;
```

## PUBLIC Pseudo-Role

`PUBLIC` represents all users, present and future.

```sql
-- Everyone can read categories
GRANT SELECT ON categories TO PUBLIC;

-- Revoke from PUBLIC
REVOKE SELECT ON categories FROM PUBLIC;
```

## Privilege Hierarchy

```
SUPERUSER
    └── Bypasses all privilege checks

Role with ADMIN OPTION
    └── Can grant role to others
    └── Can revoke role from others

Object owner
    └── All privileges on owned object
    └── Can grant privileges to others

Grantee WITH GRANT OPTION
    └── Can grant received privilege to others

Regular grantee
    └── Can use received privilege
```

## Default Privileges

Objects created by a user are owned by that user with full privileges.

```sql
-- Change default privileges for future tables
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT ON TABLES TO readonly_role;

-- For functions
ALTER DEFAULT PRIVILEGES 
    GRANT EXECUTE ON FUNCTIONS TO app_role;
```

## See Also

- [REVOKE](02_revoke_syntax.md)
- [CREATE ROLE](../ddl/security_objects/04_create_role.md)
- [ALTER DEFAULT PRIVILEGES](02_revoke_syntax.md)
