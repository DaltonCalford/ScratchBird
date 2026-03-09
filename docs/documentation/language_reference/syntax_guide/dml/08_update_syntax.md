<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# UPDATE

[Prev](./07_insert_syntax.md) | [Next](./09_delete_syntax.md) | [Topic README](./README.md) | [DML README](./README.md) | [Syntax Guide README](../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

## Synopsis

UPDATE modifies existing rows in a table.

## Syntax

```sql
UPDATE [ ONLY ] table_name [ * ] [ [ AS ] alias ]
    SET { column_name = { expression | DEFAULT } |
          ( column_name [, ...] ) = [ ROW ] ( { expression | DEFAULT } [, ...] ) |
          ( column_name [, ...] ) = ( sub-SELECT )
        } [, ...]
    [ FROM from_list ]
    [ WHERE condition ]
    [ RETURNING { * | output_expression [ [ AS ] output_name ] } [, ...] ]
```

## Basic UPDATE

### Single Column

```sql
-- Update single column
UPDATE users SET status = 'inactive' WHERE id = 1;

-- Update with expression
UPDATE products SET price = price * 1.1 WHERE category = 'electronics';

-- Update with DEFAULT
UPDATE users SET created_at = DEFAULT WHERE id = 1;
```

### Multiple Columns

```sql
-- Update multiple columns
UPDATE users SET 
    name = 'John Smith',
    email = 'john.smith@example.com',
    updated_at = NOW()
WHERE id = 1;

-- Row syntax
UPDATE users SET (name, email) = ('John', 'john@example.com') WHERE id = 1;
```

### Subquery UPDATE

```sql
-- Update from another table
UPDATE users u
SET status = 'premium'
FROM subscriptions s
WHERE u.id = s.user_id AND s.plan = 'premium';

-- Update with correlated subquery
UPDATE employees e
SET salary = (
    SELECT AVG(salary) * 1.1 
    FROM employees 
    WHERE department = e.department
)
WHERE performance_rating = 'excellent';
```

## FROM Clause

```sql
-- Update with join
UPDATE orders o
SET total = sub.total
FROM (
    SELECT order_id, SUM(quantity * price) AS total
    FROM order_items
    GROUP BY order_id
) sub
WHERE o.id = sub.order_id;

-- Update multiple tables concept
UPDATE users u
SET last_login = NOW()
FROM login_events le
WHERE u.id = le.user_id AND le.event_time > u.last_login;
```

## RETURNING Clause

```sql
-- Return updated rows
UPDATE users SET status = 'verified' WHERE id = 1 RETURNING *;

-- Return specific columns
UPDATE products SET price = price * 0.9 
WHERE category = 'clearance' 
RETURNING id, name, old_price, price;

-- Return count
UPDATE users SET status = 'inactive' 
WHERE last_login < '2023-01-01' 
RETURNING count(*);
```

## Complete Examples

### Increment Counter

```sql
-- Atomic increment
UPDATE counters 
SET value = value + 1 
WHERE name = 'page_views'
RETURNING value;
```

### Status Update

```sql
-- Update order status with tracking
UPDATE orders 
SET 
    status = 'shipped',
    shipped_at = NOW(),
    tracking_number = 'UPS123456'
WHERE id = 100
RETURNING id, status, shipped_at;
```

### Batch Update

```sql
-- Update multiple rows efficiently
UPDATE users
SET status = CASE 
    WHEN last_login > NOW() - INTERVAL '30 days' THEN 'active'
    WHEN last_login > NOW() - INTERVAL '90 days' THEN 'idle'
    ELSE 'inactive'
END
WHERE status != 'suspended';
```

### Update with CTE

```sql
WITH recent_orders AS (
    SELECT user_id, MAX(created_at) AS last_order
    FROM orders
    WHERE created_at > NOW() - INTERVAL '30 days'
    GROUP BY user_id
)
UPDATE users u
SET last_order_date = ro.last_order
FROM recent_orders ro
WHERE u.id = ro.user_id;
```

## Parser Acceptance Cases

```sql
UPDATE t1 SET a = 1;
UPDATE t1 SET a = 1, b = 2 WHERE c = 3;
UPDATE t1 SET (a, b) = (1, 2);
UPDATE t1 SET a = t2.b FROM t2 WHERE t1.id = t2.id;
UPDATE t1 SET a = 1 RETURNING *;
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `undefined_column` | Column doesn't exist |
| `check_violation` | CHECK constraint failed |
| `foreign_key_violation` | Update would violate FK |

## See Also

- [INSERT](07_insert_syntax.md)
- [DELETE](09_delete_syntax.md)
- [SELECT](01_select_core_syntax.md)
