# DROP Statements

Remove database objects.

[Back to DDL Index](index.md) | [Back to Language Guide](../index.md)

---

## DROP DATABASE

```sql
DROP DATABASE database_name;

DROP DATABASE IF EXISTS database_name;
```

**Warning:** This permanently deletes all data in the database.

---

## DROP TABLE

```sql
DROP TABLE table_name;

DROP TABLE IF EXISTS table_name;

-- Drop with dependent objects
DROP TABLE table_name CASCADE;

-- Multiple tables
DROP TABLE table1, table2, table3;
```

---

## DROP INDEX

```sql
DROP INDEX index_name;

DROP INDEX IF EXISTS index_name;

-- Concurrent drop (no lock)
DROP INDEX CONCURRENTLY index_name;
```

---

## DROP VIEW

```sql
DROP VIEW view_name;

DROP VIEW IF EXISTS view_name;

-- Drop with dependent views
DROP VIEW view_name CASCADE;

-- Materialized view
DROP MATERIALIZED VIEW mv_name;
```

---

## DROP SEQUENCE

```sql
DROP SEQUENCE sequence_name;

DROP SEQUENCE IF EXISTS sequence_name CASCADE;
```

---

## DROP FUNCTION

```sql
-- With signature
DROP FUNCTION function_name(argument_types);

-- Example
DROP FUNCTION calculate_tax(DECIMAL);

DROP FUNCTION IF EXISTS calculate_tax(DECIMAL);

DROP FUNCTION calculate_tax(DECIMAL) CASCADE;
```

---

## DROP PROCEDURE

```sql
DROP PROCEDURE procedure_name(argument_types);

DROP PROCEDURE IF EXISTS process_order(INTEGER);
```

---

## DROP TRIGGER

```sql
DROP TRIGGER trigger_name ON table_name;

DROP TRIGGER IF EXISTS audit_trigger ON users;
```

---

## DROP SCHEMA

```sql
DROP SCHEMA schema_name;

DROP SCHEMA IF EXISTS schema_name;

-- Drop with all contained objects
DROP SCHEMA schema_name CASCADE;
```

---

## DROP TYPE

```sql
DROP TYPE type_name;

DROP TYPE IF EXISTS status_enum CASCADE;
```

---

## DROP USER / ROLE

```sql
DROP USER username;

DROP USER IF EXISTS username;

DROP ROLE role_name;
```

**Note:** User must not own any objects. See [User Management](../../admin/user-management.md).

---

## DROP CONSTRAINT

```sql
ALTER TABLE table_name DROP CONSTRAINT constraint_name;

ALTER TABLE users DROP CONSTRAINT uk_users_email;

ALTER TABLE orders DROP CONSTRAINT fk_orders_user;
```

---

## CASCADE vs RESTRICT

| Option | Behavior |
|--------|----------|
| `CASCADE` | Drop dependent objects too |
| `RESTRICT` | Fail if dependencies exist (default) |

```sql
-- Will fail if views depend on it
DROP TABLE users;

-- Drops table and dependent views
DROP TABLE users CASCADE;
```

---

## IF EXISTS

Prevents errors when object doesn't exist:

```sql
-- Error if not exists
DROP TABLE missing_table;
-- ERROR: table "missing_table" does not exist

-- No error
DROP TABLE IF EXISTS missing_table;
-- NOTICE: table "missing_table" does not exist, skipping
```

---

## Transactions

DROP statements can be rolled back within a transaction:

```sql
BEGIN;
DROP TABLE users;
-- Oops, wrong table!
ROLLBACK;
-- Table still exists
```

Exception: `DROP DATABASE` cannot be in a transaction.

---

## TRUNCATE

Remove all rows without dropping the table:

```sql
TRUNCATE TABLE table_name;

-- Reset sequences
TRUNCATE TABLE table_name RESTART IDENTITY;

-- Multiple tables
TRUNCATE TABLE table1, table2;

-- With CASCADE for FK dependencies
TRUNCATE TABLE orders CASCADE;
```

TRUNCATE is faster than DELETE for large tables.

---

## Best Practices

1. **Always backup** before dropping production objects
2. **Use IF EXISTS** in scripts
3. **Be careful with CASCADE** - check dependencies first
4. **Test in development** before production
5. **Document drops** in migration scripts

---

## Checking Dependencies

Before dropping, check what depends on an object:

```sql
-- View dependencies
SELECT
    dependent.relname AS dependent_object,
    dependent.relkind AS type
FROM pg_depend d
JOIN pg_class dependent ON d.objid = dependent.oid
JOIN pg_class source ON d.refobjid = source.oid
WHERE source.relname = 'users';
```

---

## See Also

- [CREATE TABLE](create-table.md)
- [ALTER TABLE](alter-table.md)
- [Backup and Restore](../../admin/backup-restore.md)
