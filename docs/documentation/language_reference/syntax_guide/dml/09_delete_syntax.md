<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# DELETE

[Prev](./08_update_syntax.md) | [Next](./10_merge_syntax.md) | [Topic README](./README.md) | [DML README](./README.md) | [Syntax Guide README](../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

## Synopsis

DELETE removes rows from a table.

## Syntax

```sql
DELETE FROM [ ONLY ] table_name [ * ] [ [ AS ] alias ]
    [ USING using_list ]
    [ WHERE condition ]
    [ RETURNING { * | output_expression [ [ AS ] output_name ] } [, ...] ]
```

## Basic DELETE

### Delete All Rows

```sql
-- Delete all rows (use with caution!)
DELETE FROM temp_logs;

-- Equivalent to TRUNCATE for unlogged tables
DELETE FROM cache_entries;
```

### Delete with WHERE

```sql
-- Delete specific row
DELETE FROM users WHERE id = 1;

-- Delete multiple rows
DELETE FROM sessions WHERE expires_at < NOW();

-- Delete with subquery
DELETE FROM users WHERE id NOT IN (SELECT user_id FROM orders);
```

## USING Clause

```sql
-- Delete with join
DELETE FROM users u
USING deleted_accounts d
WHERE u.id = d.user_id;

-- Alternative syntax
DELETE FROM users
WHERE id IN (SELECT user_id FROM deleted_accounts);
```

## RETURNING Clause

```sql
-- Return deleted rows
DELETE FROM users WHERE id = 1 RETURNING *;

-- Return specific columns
DELETE FROM temp_data WHERE created_at < NOW() - INTERVAL '1 day'
RETURNING id, file_path;

-- Archive before delete
WITH deleted AS (
    DELETE FROM users WHERE status = 'deleted'
    RETURNING *
)
INSERT INTO user_archive
SELECT * FROM deleted;
```

## Complete Examples

### Soft Delete Pattern

```sql
-- Instead of DELETE, UPDATE status
UPDATE users SET status = 'deleted', deleted_at = NOW() WHERE id = 1;

-- Or use DELETE with trigger to archive
DELETE FROM users WHERE id = 1;
-- Trigger moves to archive table
```

### Cascade Delete

```sql
-- Delete user and all related data (with FK CASCADE)
DELETE FROM users WHERE id = 1;
-- Automatically deletes from user_profiles, user_settings, etc.
```

### Batch Delete

```sql
-- Delete in batches to avoid long transactions
DELETE FROM logs WHERE created_at < '2023-01-01' LIMIT 10000;

-- Or use loop
DO $$
DECLARE
    deleted_count INTEGER;
BEGIN
    LOOP
        DELETE FROM old_events 
        WHERE created_at < NOW() - INTERVAL '1 year'
        LIMIT 1000;
        
        GET DIAGNOSTICS deleted_count = ROW_COUNT;
        EXIT WHEN deleted_count = 0;
        
        COMMIT;
    END LOOP;
END $$;
```

### Delete with CTE

```sql
-- Delete and return count
WITH deleted AS (
    DELETE FROM inactive_users
    WHERE last_login < '2023-01-01'
    RETURNING id
)
SELECT COUNT(*) AS deleted_count FROM deleted;
```

## TRUNCATE Alternative

```sql
-- For complete table clearing, TRUNCATE is faster
TRUNCATE TABLE temp_data;

-- TRUNCATE with foreign keys
TRUNCATE TABLE orders, order_items RESTART IDENTITY CASCADE;
```

## Parser Acceptance Cases

```sql
DELETE FROM t1;
DELETE FROM t1 WHERE a = 1;
DELETE FROM t1 USING t2 WHERE t1.id = t2.id;
DELETE FROM t1 WHERE a = 1 RETURNING *;
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `foreign_key_violation` | Row referenced by foreign key (no CASCADE) |
| `undefined_table` | Table doesn't exist |

## See Also

- [TRUNCATE TABLE](../ddl/table_and_constraints/03_drop_table.md)
- [INSERT](07_insert_syntax.md)
- [UPDATE](08_update_syntax.md)
- [Soft Delete with RLS](../../security_hardening_and_compliance/authorization_rls_cls_domain_masking/02_row_level_security_design_and_validation.md)
