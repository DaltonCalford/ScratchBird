<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# REVOKE

[Prev](./01_grant_syntax.md) | [Next](./03_role_user_group_membership_syntax.md) | [Topic README](./README.md) | [Syntax Guide README](../README.md) | [Language Reference README](../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

Removes privileges from users or roles.

## Syntax

```sql
-- Revoke table privileges
REVOKE [ GRANT OPTION FOR ]
    { { SELECT | INSERT | UPDATE | DELETE | TRUNCATE | REFERENCES | TRIGGER }
    [, ...] | ALL [ PRIVILEGES ] }
    ON { [ TABLE ] table_name [, ...] | ALL TABLES IN SCHEMA schema_name [, ...] }
    FROM { [ GROUP ] role_name | PUBLIC } [, ...]
    [ CASCADE | RESTRICT ]

-- Revoke column privileges
REVOKE [ GRANT OPTION FOR ]
    { { SELECT | INSERT | UPDATE | REFERENCES } ( column_name [, ...] ) [, ...]
    | ALL [ PRIVILEGES ] ( column_name [, ...] ) }
    ON [ TABLE ] table_name [, ...]
    FROM { [ GROUP ] role_name | PUBLIC } [, ...]
    [ CASCADE | RESTRICT ]

-- Revoke other object types (same pattern as GRANT)
REVOKE [ GRANT OPTION FOR ] privileges ON object_type FROM role [ CASCADE | RESTRICT ];

-- Revoke role membership
REVOKE [ ADMIN OPTION FOR ] role_name [, ...] FROM role_name [, ...]
    [ GRANTED BY role_name ] [ CASCADE | RESTRICT ]
```

## Parameters

| Parameter | Description |
|-----------|-------------|
| `GRANT OPTION FOR` | Remove grant option only, keep privilege |
| `CASCADE` | Drop dependent objects that require the privilege |
| `RESTRICT` | Refuse if dependent objects exist (default) |

## Examples

### Revoke Table Privileges

```sql
-- Remove SELECT
REVOKE SELECT ON users FROM former_employee;

-- Remove all privileges
REVOKE ALL ON orders FROM old_app_role;

-- Remove grant option only
REVOKE GRANT OPTION FOR SELECT ON users FROM manager;

-- Revoke from multiple
REVOKE SELECT ON customers FROM user1, user2;
```

### Revoke Column Privileges

```sql
REVOKE SELECT (salary) ON employees FROM intern;
```

### Revoke Role Membership

```sql
-- Remove user from role
REVOKE admin_role FROM former_admin;

-- Remove admin option only
REVOKE ADMIN OPTION FOR manager_role FROM supervisor;
```

### Revoke PUBLIC

```sql
-- Restrict public access
REVOKE SELECT ON sensitive_data FROM PUBLIC;
```

## CASCADE vs RESTRICT

| Mode | Dependent Objects | Result |
|------|------------------|--------|
| `RESTRICT` (default) | None | Privilege revoked |
| `RESTRICT` (default) | Exist | Error: dependent objects |
| `CASCADE` | Exist | Privilege revoked, dependent objects dropped |

```sql
-- View depends on SELECT privilege
-- This fails:
REVOKE SELECT ON users FROM app_role;  -- Error: view uses this

-- This drops the view:
REVOKE SELECT ON users FROM app_role CASCADE;
```

## See Also

- [GRANT](01_grant_syntax.md)
