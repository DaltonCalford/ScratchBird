<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# CREATE VIEW

[Prev](./06_drop_index.md) | [Next](./08_alter_view.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

## Synopsis

Creates a view - a virtual table defined by a query. Views provide abstraction, security, and simplify complex queries.

## Syntax

```sql
CREATE [ OR REPLACE ] [ TEMP | TEMPORARY ] VIEW [ IF NOT EXISTS ] view_name [ ( column_name [, ...] ) ]
    [ WITH ( view_option [, ...] ) ]
    AS query
    [ WITH [ CASCADED | LOCAL ] CHECK OPTION ]

where view_option can be:
    security_barrier = boolean
    security_invoker = boolean
```

## Parameters

| Parameter | Description |
|-----------|-------------|
| `OR REPLACE` | Replace existing view with same name |
| `TEMP`, `TEMPORARY` | View is session-local |
| `IF NOT EXISTS` | Skip if view exists (no error, no replacement) |
| `view_name` | Name of view. Supports qualified paths. |
| `column_name` | Override column names from query |
| `security_barrier` | Prevent optimizations that bypass row security |
| `security_invoker` | Execute with invoker's privileges (default: definer) |
| `query` | SELECT statement defining view |
| `CHECK OPTION` | Prevent modifications through view that would exclude rows |

## Description

A view is a stored query that behaves like a table. Each time a view is queried, the underlying query executes.

### View Characteristics

| Property | Behavior |
|----------|----------|
| Data storage | None (virtual) - data comes from base tables |
| Updatable | Some views support INSERT/UPDATE/DELETE |
| Permissions | Can have separate access control |
| Dependencies | Dropped if base tables dropped (CASCADE) |

### Updatable Views

A view is updatable if:
- Single base table in FROM
- No DISTINCT, GROUP BY, HAVING, aggregates, window functions
- No set operations (UNION, INTERSECT, EXCEPT)

## Examples

### Basic View

```sql
-- Simple column subset
CREATE VIEW active_users AS
SELECT id, email, created_at
FROM users
WHERE status = 'active';

-- Query the view
SELECT * FROM active_users WHERE created_at > '2024-01-01';
```

### Complex View

```sql
-- Aggregated view
CREATE VIEW order_summary AS
SELECT 
    user_id,
    COUNT(*) AS order_count,
    SUM(amount) AS total_amount,
    MAX(created_at) AS last_order
FROM orders
GROUP BY user_id;
```

### Join View

```sql
-- Denormalized view
CREATE VIEW user_orders AS
SELECT 
    u.id AS user_id,
    u.email,
    o.id AS order_id,
    o.amount,
    o.status
FROM users u
LEFT JOIN orders o ON u.id = o.user_id;
```

### With Column Aliases

```sql
-- Override column names
CREATE VIEW user_report (user_id, email_address, registration_date) AS
SELECT id, email, created_at
FROM users;
```

### Security Barrier

```sql
-- Prevent row bypass in view with RLS
CREATE VIEW user_private_data AS
SELECT id, email, ssn
FROM users
WITH (security_barrier = true);
```

### Security Invoker

```sql
-- View runs with caller's permissions
CREATE VIEW public.my_data AS
SELECT * FROM private.data
WITH (security_invoker = true);
```

### Updatable View with Check Option

```sql
-- Only allow active users through view
CREATE VIEW active_users AS
SELECT * FROM users WHERE status = 'active'
WITH CHECK OPTION;

-- This will fail:
INSERT INTO active_users (email, status) 
VALUES ('test@example.com', 'inactive');  -- Error: violates check option
```

### OR REPLACE

```sql
-- Modify view without dropping
CREATE OR REPLACE VIEW order_summary AS
SELECT 
    user_id,
    COUNT(*) AS order_count,
    SUM(amount) AS total_amount,
    AVG(amount) AS avg_amount,  -- Added column
    MAX(created_at) AS last_order
FROM orders
GROUP BY user_id;
```

## Updatable View Rules

### Automatically Updatable

```sql
-- This view is updatable
CREATE VIEW simple_view AS
SELECT id, email, status
FROM users;

-- These work:
INSERT INTO simple_view (email, status) VALUES ('new@example.com', 'active');
UPDATE simple_view SET status = 'inactive' WHERE id = 1;
DELETE FROM simple_view WHERE id = 1;
```

### Not Updatable

```sql
-- Not updatable (aggregation)
CREATE VIEW summary_view AS
SELECT user_id, COUNT(*) FROM orders GROUP BY user_id;

-- This fails:
INSERT INTO summary_view VALUES (1, 5);  -- Error: not updatable
```

## Parser Acceptance Cases

```sql
CREATE VIEW v1 AS SELECT * FROM t1;
CREATE OR REPLACE VIEW v1 AS SELECT * FROM t1;
CREATE VIEW IF NOT EXISTS v1 AS SELECT * FROM t1;
CREATE VIEW v1 (a, b) AS SELECT col1, col2 FROM t1;
CREATE VIEW v1 AS SELECT * FROM t1 WITH CHECK OPTION;
```

## Parser Rejection Cases

```sql
-- Column count mismatch
CREATE VIEW v1 (a, b) AS SELECT col1 FROM t1;  -- Error: 2 columns expected, 1 provided

-- OR REPLACE and IF NOT EXISTS conflict
CREATE OR REPLACE VIEW IF NOT EXISTS v1 AS SELECT 1;  -- Error: cannot use both
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `duplicate_view` | View exists (no OR REPLACE/IF NOT EXISTS) |
| `undefined_table` | Base table doesn't exist |
| `column_count_mismatch` | Column list doesn't match query |
| `feature_not_supported` | Non-updatable view modification attempted |

## See Also

- [ALTER VIEW](08_alter_view.md)
- [DROP VIEW](09_drop_view.md)
- [CREATE RULE](../routines_and_code/README.md) - For INSTEAD OF triggers
- [Row-Level Security](../../../security_hardening_and_compliance/authorization_rls_cls_domain_masking/README.md)
