# UPDATE

Modify existing rows.

[Back to DML Index](index.md) | [Back to Language Guide](../index.md)

---

## Basic Syntax

```sql
UPDATE table_name
SET column = value, ...
[ FROM other_tables ]
[ WHERE condition ]
[ RETURNING columns ];
```

---

## Simple Update

```sql
-- Single column
UPDATE users SET active = FALSE WHERE id = 1;

-- Multiple columns
UPDATE users SET
    name = 'Alice Smith',
    email = 'alice.smith@example.com',
    updated_at = CURRENT_TIMESTAMP
WHERE id = 1;
```

---

## Update All Rows

```sql
-- WARNING: Updates every row
UPDATE products SET price = price * 1.10;
```

---

## With Expressions

```sql
-- Arithmetic
UPDATE products SET price = price * 1.10 WHERE category = 'electronics';

-- String functions
UPDATE users SET email = LOWER(email);

-- Concatenation
UPDATE users SET name = first_name || ' ' || last_name;

-- Conditional
UPDATE orders SET
    status = CASE
        WHEN total > 1000 THEN 'priority'
        ELSE 'standard'
    END;
```

---

## With RETURNING

```sql
UPDATE users
SET active = FALSE
WHERE last_login < CURRENT_DATE - INTERVAL '1 year'
RETURNING id, name, email;
```

---

## Update from Another Table

### PostgreSQL Style (FROM)

```sql
UPDATE orders
SET discount = c.discount_rate
FROM customers c
WHERE orders.customer_id = c.id
  AND c.vip = TRUE;
```

### With Join

```sql
UPDATE products p
SET stock = p.stock - oi.quantity
FROM order_items oi
WHERE p.id = oi.product_id
  AND oi.order_id = 123;
```

---

## Update with Subquery

```sql
UPDATE users
SET order_count = (
    SELECT COUNT(*)
    FROM orders
    WHERE orders.user_id = users.id
);

-- In WHERE
UPDATE products
SET featured = TRUE
WHERE id IN (
    SELECT product_id
    FROM order_items
    GROUP BY product_id
    ORDER BY SUM(quantity) DESC
    LIMIT 10
);
```

---

## Update with CTE

```sql
WITH high_spenders AS (
    SELECT user_id
    FROM orders
    GROUP BY user_id
    HAVING SUM(total) > 10000
)
UPDATE users
SET tier = 'gold'
WHERE id IN (SELECT user_id FROM high_spenders);
```

---

## Conditional Update

```sql
UPDATE products SET
    price = CASE
        WHEN category = 'clearance' THEN price * 0.5
        WHEN category = 'sale' THEN price * 0.8
        ELSE price
    END,
    updated_at = CURRENT_TIMESTAMP
WHERE category IN ('clearance', 'sale');
```

---

## Increment/Decrement

```sql
-- Increment
UPDATE counters SET value = value + 1 WHERE name = 'page_views';

-- Decrement with floor
UPDATE inventory
SET quantity = GREATEST(quantity - 1, 0)
WHERE product_id = 123;
```

---

## NULL Handling

```sql
-- Set to NULL
UPDATE users SET phone = NULL WHERE id = 1;

-- Update only non-NULL
UPDATE users SET
    phone = COALESCE(new_phone, phone)
WHERE id = 1;
```

---

## Batch Updates

```sql
-- Update multiple specific rows
UPDATE users SET status = 'verified'
WHERE id IN (1, 2, 3, 4, 5);

-- Update first N rows
UPDATE users SET processed = TRUE
WHERE id IN (
    SELECT id FROM users
    WHERE processed = FALSE
    ORDER BY created_at
    LIMIT 1000
);
```

---

## Transactions

```sql
BEGIN;

-- Lock rows for update
SELECT * FROM accounts WHERE id IN (1, 2) FOR UPDATE;

-- Transfer
UPDATE accounts SET balance = balance - 100 WHERE id = 1;
UPDATE accounts SET balance = balance + 100 WHERE id = 2;

COMMIT;
```

---

## Common Patterns

### Soft Delete

```sql
UPDATE users SET
    deleted_at = CURRENT_TIMESTAMP,
    deleted_by = current_user
WHERE id = 1;
```

### Toggle Boolean

```sql
UPDATE settings SET enabled = NOT enabled WHERE name = 'feature_x';
```

### Timestamp on Update

```sql
UPDATE documents SET
    content = 'New content',
    updated_at = CURRENT_TIMESTAMP
WHERE id = 1;
```

### Reset to Default

```sql
UPDATE users SET preferences = DEFAULT WHERE id = 1;
```

---

## Safety Tips

1. **Always use WHERE** unless updating all rows intentionally
2. **Test with SELECT first**:
   ```sql
   SELECT * FROM users WHERE condition;  -- Verify rows
   UPDATE users SET ... WHERE condition; -- Then update
   ```
3. **Use transactions** for related updates
4. **Backup before bulk updates**

---

## Notes

- Without WHERE, all rows are updated
- RETURNING requires PostgreSQL protocol
- Updates can trigger ON UPDATE triggers
- Updates modify `updated_at` if defined as a trigger

---

## See Also

- [DELETE](delete.md)
- [INSERT](insert.md)
- [Transactions](../index.md#transactions)
