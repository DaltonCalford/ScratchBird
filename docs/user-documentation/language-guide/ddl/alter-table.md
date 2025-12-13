# ALTER TABLE

Modify existing tables.

[Back to DDL Index](index.md) | [Back to Language Guide](../index.md)

---

## Syntax

```sql
ALTER TABLE table_name action [, action ...];
```

---

## Add Column

```sql
ALTER TABLE users ADD COLUMN phone VARCHAR(20);

ALTER TABLE users ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP;

-- Multiple columns
ALTER TABLE users
    ADD COLUMN phone VARCHAR(20),
    ADD COLUMN address TEXT;
```

---

## Drop Column

```sql
ALTER TABLE users DROP COLUMN phone;

-- If column has dependencies
ALTER TABLE users DROP COLUMN phone CASCADE;
```

---

## Rename

### Rename Column

```sql
ALTER TABLE users RENAME COLUMN name TO full_name;
```

### Rename Table

```sql
ALTER TABLE users RENAME TO customers;
```

---

## Change Column Type

```sql
ALTER TABLE users ALTER COLUMN name TYPE VARCHAR(200);

-- With conversion
ALTER TABLE products
    ALTER COLUMN price TYPE DECIMAL(12,2)
    USING price::DECIMAL(12,2);
```

---

## Set/Drop Default

```sql
-- Set default
ALTER TABLE users ALTER COLUMN active SET DEFAULT TRUE;

-- Drop default
ALTER TABLE users ALTER COLUMN active DROP DEFAULT;
```

---

## Set/Drop NOT NULL

```sql
-- Add NOT NULL
ALTER TABLE users ALTER COLUMN email SET NOT NULL;

-- Remove NOT NULL
ALTER TABLE users ALTER COLUMN email DROP NOT NULL;
```

---

## Add Constraints

### Primary Key

```sql
ALTER TABLE users ADD PRIMARY KEY (id);
```

### Foreign Key

```sql
ALTER TABLE orders
    ADD CONSTRAINT fk_orders_user
    FOREIGN KEY (user_id) REFERENCES users(id);

-- With options
ALTER TABLE orders
    ADD CONSTRAINT fk_orders_user
    FOREIGN KEY (user_id) REFERENCES users(id)
    ON DELETE CASCADE
    ON UPDATE CASCADE;
```

### Unique Constraint

```sql
ALTER TABLE users ADD CONSTRAINT uk_users_email UNIQUE (email);

-- Composite
ALTER TABLE subscriptions
    ADD CONSTRAINT uk_user_service UNIQUE (user_id, service_id);
```

### Check Constraint

```sql
ALTER TABLE accounts
    ADD CONSTRAINT ck_balance_positive CHECK (balance >= 0);
```

---

## Drop Constraints

```sql
ALTER TABLE users DROP CONSTRAINT uk_users_email;

-- If constraint name unknown
ALTER TABLE users DROP CONSTRAINT IF EXISTS uk_users_email;
```

---

## Indexes

Add and drop indexes (preferred method):

```sql
-- Add
CREATE INDEX idx_users_email ON users(email);

-- Drop
DROP INDEX idx_users_email;
```

Via ALTER TABLE:

```sql
ALTER TABLE users ADD INDEX idx_users_email (email);
```

---

## Partitioning

### Attach Partition

```sql
ALTER TABLE sales
    ATTACH PARTITION sales_2024_q1
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');
```

### Detach Partition

```sql
ALTER TABLE sales DETACH PARTITION sales_2024_q1;
```

---

## Owner and Schema

```sql
-- Change owner
ALTER TABLE users OWNER TO newowner;

-- Move to different schema
ALTER TABLE users SET SCHEMA archive;
```

---

## Triggers

```sql
-- Enable/disable trigger
ALTER TABLE users DISABLE TRIGGER audit_trigger;
ALTER TABLE users ENABLE TRIGGER audit_trigger;

-- All triggers
ALTER TABLE users DISABLE TRIGGER ALL;
ALTER TABLE users ENABLE TRIGGER ALL;
```

---

## Row-Level Security

```sql
-- Enable RLS
ALTER TABLE orders ENABLE ROW LEVEL SECURITY;

-- Force RLS for owner
ALTER TABLE orders FORCE ROW LEVEL SECURITY;

-- Disable
ALTER TABLE orders DISABLE ROW LEVEL SECURITY;
```

---

## Storage Parameters

```sql
-- Set fill factor
ALTER TABLE users SET (fillfactor = 80);

-- Reset to default
ALTER TABLE users RESET (fillfactor);
```

---

## Multiple Actions

```sql
ALTER TABLE users
    ADD COLUMN phone VARCHAR(20),
    ADD COLUMN updated_at TIMESTAMP,
    ALTER COLUMN name TYPE VARCHAR(200),
    DROP COLUMN legacy_field;
```

---

## Examples

### Add Audit Columns

```sql
ALTER TABLE orders
    ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    ADD COLUMN created_by VARCHAR(50),
    ADD COLUMN updated_at TIMESTAMP,
    ADD COLUMN updated_by VARCHAR(50);
```

### Add Soft Delete

```sql
ALTER TABLE users
    ADD COLUMN deleted_at TIMESTAMP,
    ADD COLUMN deleted_by VARCHAR(50);

-- Then use view for active records
CREATE VIEW active_users AS
SELECT * FROM users WHERE deleted_at IS NULL;
```

### Schema Migration

```sql
-- Add new column
ALTER TABLE products ADD COLUMN category_id INTEGER;

-- Populate data
UPDATE products SET category_id = (
    SELECT id FROM categories WHERE name = products.category_name
);

-- Add constraint
ALTER TABLE products
    ADD CONSTRAINT fk_products_category
    FOREIGN KEY (category_id) REFERENCES categories(id);

-- Drop old column
ALTER TABLE products DROP COLUMN category_name;
```

---

## Notes

- Some operations lock the table
- Use CONCURRENTLY for index operations when possible
- Test on non-production first
- Backup before major changes
- NOT NULL requires existing data to comply

---

## See Also

- [CREATE TABLE](create-table.md)
- [CREATE INDEX](create-index.md)
- [DROP](drop-statements.md)
