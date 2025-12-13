# CREATE VIEW

Create a virtual table based on a query.

[Back to DDL Index](index.md) | [Back to Language Guide](../index.md)

---

## Syntax

```sql
CREATE [ OR REPLACE ] [ TEMPORARY ] VIEW view_name [ ( column_name, ... ) ]
    AS query
    [ WITH [ CASCADED | LOCAL ] CHECK OPTION ];
```

---

## Examples

### Basic View

```sql
CREATE VIEW active_users AS
SELECT id, name, email
FROM users
WHERE active = TRUE;
```

### With Column Names

```sql
CREATE VIEW user_summary (user_id, full_name, email_address) AS
SELECT id, name, email
FROM users;
```

### Join View

```sql
CREATE VIEW order_details AS
SELECT
    o.id AS order_id,
    o.created_at,
    u.name AS customer_name,
    p.name AS product_name,
    oi.quantity,
    oi.unit_price
FROM orders o
JOIN users u ON o.user_id = u.id
JOIN order_items oi ON o.id = oi.order_id
JOIN products p ON oi.product_id = p.id;
```

### Aggregate View

```sql
CREATE VIEW sales_summary AS
SELECT
    DATE_TRUNC('month', created_at) AS month,
    COUNT(*) AS order_count,
    SUM(total) AS revenue
FROM orders
GROUP BY DATE_TRUNC('month', created_at);
```

### Replace Existing

```sql
CREATE OR REPLACE VIEW active_users AS
SELECT id, name, email, created_at
FROM users
WHERE active = TRUE AND verified = TRUE;
```

---

## Using Views

Views are queried like tables:

```sql
SELECT * FROM active_users;

SELECT * FROM active_users
WHERE created_at > '2024-01-01';

SELECT u.*, o.total
FROM active_users u
JOIN orders o ON u.id = o.user_id;
```

---

## Updatable Views

Simple views can be updated:

```sql
-- Simple updatable view
CREATE VIEW recent_users AS
SELECT * FROM users
WHERE created_at > CURRENT_DATE - INTERVAL '30 days';

-- These work:
INSERT INTO recent_users (name, email, created_at)
VALUES ('New User', 'new@example.com', CURRENT_DATE);

UPDATE recent_users SET name = 'Updated' WHERE id = 1;

DELETE FROM recent_users WHERE id = 1;
```

### WITH CHECK OPTION

Prevent updates that would remove rows from view:

```sql
CREATE VIEW active_users AS
SELECT * FROM users WHERE active = TRUE
WITH CHECK OPTION;

-- This fails (would make row invisible to view):
UPDATE active_users SET active = FALSE WHERE id = 1;
```

---

## Materialized Views

Pre-computed views for performance:

```sql
CREATE MATERIALIZED VIEW monthly_sales AS
SELECT
    DATE_TRUNC('month', created_at) AS month,
    COUNT(*) AS orders,
    SUM(total) AS revenue
FROM orders
GROUP BY DATE_TRUNC('month', created_at);

-- Refresh data
REFRESH MATERIALIZED VIEW monthly_sales;

-- Refresh concurrently (no lock)
REFRESH MATERIALIZED VIEW CONCURRENTLY monthly_sales;
```

### Index Materialized Views

```sql
CREATE INDEX idx_monthly_sales_month ON monthly_sales(month);
```

---

## Recursive Views

Views that reference themselves:

```sql
-- Hierarchical data (org chart)
CREATE RECURSIVE VIEW employee_tree (id, name, manager_id, level) AS
    -- Base case
    SELECT id, name, manager_id, 0
    FROM employees
    WHERE manager_id IS NULL
UNION ALL
    -- Recursive case
    SELECT e.id, e.name, e.manager_id, et.level + 1
    FROM employees e
    JOIN employee_tree et ON e.manager_id = et.id;
```

---

## View Management

### List Views

```sql
-- sb_isql / psql
\dv

-- SQL
SELECT viewname, definition
FROM pg_views
WHERE schemaname = 'public';
```

### View Definition

```sql
-- sb_isql / psql
\d+ view_name

-- SQL
SELECT definition
FROM pg_views
WHERE viewname = 'active_users';
```

### Alter View

```sql
-- Rename
ALTER VIEW active_users RENAME TO verified_users;

-- Change owner
ALTER VIEW active_users OWNER TO newowner;

-- Set options
ALTER VIEW active_users SET (security_barrier = true);
```

### Drop View

```sql
DROP VIEW active_users;

DROP VIEW IF EXISTS active_users;

-- Drop with dependent objects
DROP VIEW active_users CASCADE;
```

---

## Security Views

### Security Barrier

Prevent information leaks through optimizer:

```sql
CREATE VIEW user_public_info WITH (security_barrier = true) AS
SELECT id, name, public_email
FROM users;
```

### Row-Level Security

Combine with RLS for row filtering:

```sql
CREATE VIEW my_orders AS
SELECT * FROM orders
WHERE user_id = current_user_id();
```

---

## View Dependencies

```sql
-- Objects depending on view
SELECT
    dependent.relname AS dependent_object
FROM pg_depend d
JOIN pg_class dependent ON d.objid = dependent.oid
JOIN pg_class source ON d.refobjid = source.oid
WHERE source.relname = 'users';
```

---

## Best Practices

1. **Name clearly** - `active_users` not `v_users_1`
2. **Keep simple** - Complex logic in stored procedures
3. **Consider materialized** - For expensive aggregations
4. **Document purpose** - Use comments
5. **Watch dependencies** - CASCADE drops can be dangerous

---

## Notes

- Views don't store data (except materialized)
- Views can reference other views
- Views can't have indexes (except materialized)
- DROP CASCADE affects dependent objects

---

## See Also

- [CREATE TABLE](create-table.md)
- [SELECT](../dml/select.md)
