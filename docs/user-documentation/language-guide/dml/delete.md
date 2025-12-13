# DELETE

Remove rows from a table.

[Back to DML Index](index.md) | [Back to Language Guide](../index.md)

---

## Basic Syntax

```sql
DELETE FROM table_name
[ USING other_tables ]
[ WHERE condition ]
[ RETURNING columns ];
```

---

## Simple Delete

```sql
DELETE FROM users WHERE id = 1;

DELETE FROM logs WHERE created_at < '2023-01-01';
```

---

## Delete All Rows

```sql
-- WARNING: Deletes all rows
DELETE FROM temp_data;

-- Faster alternative for all rows
TRUNCATE TABLE temp_data;
```

---

## With RETURNING

```sql
DELETE FROM users
WHERE last_login < CURRENT_DATE - INTERVAL '2 years'
RETURNING id, name, email;
```

---

## Delete with Join (USING)

```sql
DELETE FROM order_items
USING orders
WHERE order_items.order_id = orders.id
  AND orders.status = 'cancelled';
```

---

## Delete with Subquery

```sql
DELETE FROM users
WHERE id IN (
    SELECT user_id
    FROM audit_log
    WHERE action = 'fraud_detected'
);

DELETE FROM products
WHERE NOT EXISTS (
    SELECT 1 FROM order_items
    WHERE order_items.product_id = products.id
);
```

---

## Delete with CTE

```sql
WITH inactive_users AS (
    SELECT id FROM users
    WHERE last_login < CURRENT_DATE - INTERVAL '1 year'
)
DELETE FROM users
WHERE id IN (SELECT id FROM inactive_users);
```

---

## Limiting Deletes

```sql
-- Delete first N rows
DELETE FROM logs
WHERE id IN (
    SELECT id FROM logs
    ORDER BY created_at
    LIMIT 10000
);
```

---

## Cascade Delete

If foreign keys have `ON DELETE CASCADE`:

```sql
-- Automatically deletes related order_items
DELETE FROM orders WHERE id = 123;
```

---

## Common Patterns

### Soft Delete (Preferred)

```sql
-- Instead of deleting, mark as deleted
UPDATE users SET
    deleted_at = CURRENT_TIMESTAMP,
    deleted_by = current_user
WHERE id = 1;
```

### Archive Before Delete

```sql
BEGIN;

-- Copy to archive
INSERT INTO users_archive
SELECT *, CURRENT_TIMESTAMP AS archived_at
FROM users
WHERE created_at < '2020-01-01';

-- Delete originals
DELETE FROM users WHERE created_at < '2020-01-01';

COMMIT;
```

### Delete Duplicates

```sql
DELETE FROM users
WHERE id NOT IN (
    SELECT MIN(id)
    FROM users
    GROUP BY email
);
```

---

## Transactions

```sql
BEGIN;

-- Verify what will be deleted
SELECT COUNT(*) FROM old_records WHERE condition;

-- Delete
DELETE FROM old_records WHERE condition;

-- Commit only if correct
COMMIT;
-- Or ROLLBACK if wrong
```

---

## Safety Tips

1. **Always use WHERE** unless deleting all rows intentionally
2. **Test with SELECT first**:
   ```sql
   SELECT * FROM users WHERE condition;  -- Check rows
   DELETE FROM users WHERE condition;    -- Then delete
   ```
3. **Use transactions** for safety
4. **Backup before bulk deletes**
5. **Consider soft delete** instead

---

## TRUNCATE vs DELETE

| Feature | DELETE | TRUNCATE |
|---------|--------|----------|
| WHERE clause | Yes | No |
| RETURNING | Yes | No |
| Triggers | Fires | Doesn't fire |
| Speed | Slower | Faster |
| Rollback | Yes | Yes* |
| Reset sequence | No | Optional |

```sql
-- TRUNCATE with sequence reset
TRUNCATE TABLE logs RESTART IDENTITY;

-- TRUNCATE with cascade
TRUNCATE TABLE orders CASCADE;
```

---

## Notes

- Without WHERE, all rows are deleted
- DELETE can be rolled back in a transaction
- Foreign key constraints may prevent deletion
- Consider soft delete for audit trails

---

## See Also

- [UPDATE](update.md)
- [INSERT](insert.md)
- [Backup and Restore](../../admin/backup-restore.md)
