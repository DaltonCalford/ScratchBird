[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL - DML Modification

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

---

## Overview

This document covers data modification statements in PostgreSQL emulation mode: INSERT, UPDATE, DELETE, and MERGE. These statements change data in tables and support PostgreSQL-specific features like RETURNING clauses and ON CONFLICT handling.

**Spec refs:**
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `ScratchBird/docs/audit/17_postgresql_parser_statement_reference_actual.md`

---

## INSERT Statement

### Basic Syntax

```sql
INSERT INTO table_name [(column_list)]
{ VALUES (value_list) [, ...] | query | DEFAULT VALUES }
[ON CONFLICT [conflict_target] conflict_action]
[RETURNING output_expression [AS alias] [, ...]]
```

### Basic INSERT

**Single row with all columns:**
```sql
INSERT INTO users VALUES (1, 'john@example.com', 'John Doe', NOW());
```

**Single row with column list:**
```sql
INSERT INTO users (email, name)
VALUES ('john@example.com', 'John Doe');
```

**Multiple rows:**
```sql
INSERT INTO users (email, name)
VALUES
    ('john@example.com', 'John Doe'),
    ('jane@example.com', 'Jane Smith'),
    ('bob@example.com', 'Bob Wilson');
```

**With expressions:**
```sql
INSERT INTO orders (customer_id, order_date, total, status)
VALUES (
    123,
    CURRENT_DATE,
    199.99 * 1.08,  -- With tax
    'pending'
);
```

### DEFAULT VALUES

Insert a row using all default values:

```sql
-- Table with defaults
CREATE TABLE audit_log (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMP DEFAULT NOW(),
    action VARCHAR(50) DEFAULT 'unknown'
);

-- Insert using all defaults
INSERT INTO audit_log DEFAULT VALUES;

-- Mix explicit and default values
INSERT INTO audit_log (action)
VALUES (DEFAULT);  -- Uses default for action column
```

### INSERT ... SELECT

Insert rows from a query result:

```sql
-- Copy all rows
INSERT INTO archived_orders
SELECT * FROM orders WHERE order_date < '2025-01-01';

-- Copy specific columns
INSERT INTO user_emails (user_id, email)
SELECT id, email FROM users WHERE active = true;

-- With transformations
INSERT INTO summary_table (category, total_sales, avg_price)
SELECT
    category,
    SUM(price * quantity),
    AVG(price)
FROM order_items
GROUP BY category;

-- From multiple tables
INSERT INTO customer_orders (customer_name, order_total)
SELECT
    c.name,
    o.total
FROM customers c
JOIN orders o ON c.id = o.customer_id
WHERE o.status = 'completed';
```

### ON CONFLICT (UPSERT)

Handle unique constraint violations gracefully.

**DO NOTHING:**
```sql
-- Skip if conflict
INSERT INTO users (id, email, name)
VALUES (1, 'john@example.com', 'John Doe')
ON CONFLICT DO NOTHING;

-- Specify conflict column
INSERT INTO users (id, email, name)
VALUES (1, 'john@example.com', 'John Doe')
ON CONFLICT (email) DO NOTHING;

-- Specify constraint name
INSERT INTO users (id, email, name)
VALUES (1, 'john@example.com', 'John Doe')
ON CONFLICT ON CONSTRAINT users_email_key DO NOTHING;
```

**DO UPDATE (upsert):**
```sql
-- Update on conflict
INSERT INTO products (sku, name, price, stock)
VALUES ('WIDGET-001', 'Widget', 9.99, 100)
ON CONFLICT (sku) DO UPDATE SET
    price = EXCLUDED.price,
    stock = products.stock + EXCLUDED.stock;

-- Conditional update
INSERT INTO user_stats (user_id, login_count, last_login)
VALUES (123, 1, NOW())
ON CONFLICT (user_id) DO UPDATE SET
    login_count = user_stats.login_count + 1,
    last_login = EXCLUDED.last_login
WHERE user_stats.last_login < EXCLUDED.last_login;

-- Update all columns
INSERT INTO settings (key, value, updated_at)
VALUES ('theme', 'dark', NOW())
ON CONFLICT (key) DO UPDATE SET
    value = EXCLUDED.value,
    updated_at = EXCLUDED.updated_at;
```

**Conflict on composite key:**
```sql
INSERT INTO inventory (warehouse_id, product_id, quantity)
VALUES (1, 100, 50)
ON CONFLICT (warehouse_id, product_id) DO UPDATE SET
    quantity = inventory.quantity + EXCLUDED.quantity;
```

### RETURNING Clause

Return data from inserted rows:

```sql
-- Return all columns
INSERT INTO users (email, name)
VALUES ('john@example.com', 'John Doe')
RETURNING *;

-- Return specific columns
INSERT INTO users (email, name)
VALUES ('john@example.com', 'John Doe')
RETURNING id, email;

-- Return with alias
INSERT INTO users (email, name)
VALUES ('john@example.com', 'John Doe')
RETURNING id AS user_id, created_at AS signup_date;

-- Return computed values
INSERT INTO orders (customer_id, subtotal, tax_rate)
VALUES (123, 100.00, 0.08)
RETURNING id, subtotal * (1 + tax_rate) AS total;

-- Use in CTE
WITH new_user AS (
    INSERT INTO users (email, name)
    VALUES ('john@example.com', 'John Doe')
    RETURNING id
)
INSERT INTO user_profiles (user_id, bio)
SELECT id, 'New user' FROM new_user;
```

---

## UPDATE Statement

### Basic Syntax

```sql
UPDATE table_name [[AS] alias]
SET column = {expression | DEFAULT} [, ...]
[FROM from_list]
[WHERE condition]
[RETURNING output_expression [AS alias] [, ...]]
```

### Basic UPDATE

**Update single column:**
```sql
UPDATE users SET status = 'active' WHERE id = 123;
```

**Update multiple columns:**
```sql
UPDATE users
SET
    status = 'active',
    updated_at = NOW(),
    login_count = login_count + 1
WHERE id = 123;
```

**Update all rows:**
```sql
UPDATE products SET price = price * 1.10;  -- 10% price increase
```

**Update with expression:**
```sql
UPDATE orders
SET
    total = subtotal * (1 + tax_rate),
    status = CASE
        WHEN payment_received THEN 'paid'
        ELSE 'pending'
    END
WHERE order_date = CURRENT_DATE;
```

### UPDATE with DEFAULT

Reset column to its default value:

```sql
UPDATE users SET theme = DEFAULT WHERE id = 123;
```

### UPDATE ... FROM

Update using data from other tables (PostgreSQL-specific):

```sql
-- Join update
UPDATE orders o
SET status = 'shipped'
FROM shipments s
WHERE o.id = s.order_id
AND s.shipped_date IS NOT NULL;

-- Update with subquery in FROM
UPDATE products p
SET discount = c.default_discount
FROM categories c
WHERE p.category_id = c.id
AND c.name = 'Clearance';

-- Multi-table join
UPDATE order_items oi
SET price = p.current_price
FROM orders o, products p
WHERE oi.order_id = o.id
AND oi.product_id = p.id
AND o.status = 'pending';
```

### UPDATE with Subquery

```sql
-- Subquery in SET
UPDATE products
SET category_id = (
    SELECT id FROM categories WHERE name = 'Uncategorized'
)
WHERE category_id IS NULL;

-- Subquery in WHERE
UPDATE users
SET status = 'premium'
WHERE id IN (
    SELECT customer_id
    FROM orders
    GROUP BY customer_id
    HAVING SUM(total) > 10000
);

-- Correlated subquery
UPDATE products p
SET avg_rating = (
    SELECT AVG(rating)
    FROM reviews r
    WHERE r.product_id = p.id
);
```

### UPDATE with RETURNING

```sql
-- Return updated rows
UPDATE users
SET status = 'inactive'
WHERE last_login < CURRENT_DATE - INTERVAL '1 year'
RETURNING id, email, status;

-- Return old and new values (using FROM)
UPDATE accounts a
SET balance = a.balance - 100
FROM (SELECT id, balance FROM accounts WHERE id = 123) old
WHERE a.id = old.id
RETURNING a.id, old.balance AS old_balance, a.balance AS new_balance;

-- Use in CTE
WITH updated AS (
    UPDATE inventory
    SET quantity = quantity - 10
    WHERE product_id = 100 AND quantity >= 10
    RETURNING product_id, quantity
)
INSERT INTO inventory_log (product_id, new_quantity, changed_at)
SELECT product_id, quantity, NOW() FROM updated;
```

### Row-Level UPDATE

Update using row constructor:

```sql
UPDATE coordinates
SET (x, y, z) = (10, 20, 30)
WHERE id = 1;

-- From subquery
UPDATE points
SET (x, y) = (SELECT new_x, new_y FROM transforms WHERE point_id = points.id)
WHERE needs_transform = true;
```

---

## DELETE Statement

### Basic Syntax

```sql
DELETE FROM table_name [[AS] alias]
[USING using_list]
[WHERE condition]
[RETURNING output_expression [AS alias] [, ...]]
```

### Basic DELETE

**Delete specific rows:**
```sql
DELETE FROM users WHERE id = 123;
```

**Delete with multiple conditions:**
```sql
DELETE FROM sessions
WHERE user_id = 123
AND created_at < NOW() - INTERVAL '24 hours';
```

**Delete all rows:**
```sql
DELETE FROM temp_data;  -- Deletes all rows (use TRUNCATE for better performance)
```

### DELETE ... USING

Delete based on data in other tables (PostgreSQL-specific):

```sql
-- Delete with join
DELETE FROM order_items oi
USING orders o
WHERE oi.order_id = o.id
AND o.status = 'cancelled';

-- Multi-table condition
DELETE FROM notifications n
USING users u
WHERE n.user_id = u.id
AND u.status = 'deleted';

-- Complex join
DELETE FROM inventory i
USING products p, categories c
WHERE i.product_id = p.id
AND p.category_id = c.id
AND c.name = 'Discontinued';
```

### DELETE with Subquery

```sql
-- Subquery in WHERE
DELETE FROM users
WHERE id NOT IN (
    SELECT DISTINCT customer_id FROM orders
);

-- EXISTS subquery
DELETE FROM products p
WHERE NOT EXISTS (
    SELECT 1 FROM order_items oi WHERE oi.product_id = p.id
);

-- Correlated subquery
DELETE FROM duplicate_emails d1
WHERE EXISTS (
    SELECT 1 FROM duplicate_emails d2
    WHERE d1.email = d2.email
    AND d1.id > d2.id
);
```

### DELETE with RETURNING

```sql
-- Return deleted rows
DELETE FROM expired_sessions
WHERE expires_at < NOW()
RETURNING id, user_id, expires_at;

-- Archive before delete
WITH deleted AS (
    DELETE FROM orders
    WHERE status = 'cancelled'
    AND order_date < CURRENT_DATE - INTERVAL '1 year'
    RETURNING *
)
INSERT INTO archived_orders SELECT * FROM deleted;

-- Count deleted rows
WITH deleted AS (
    DELETE FROM temp_data
    WHERE created_at < NOW() - INTERVAL '1 hour'
    RETURNING id
)
SELECT COUNT(*) AS deleted_count FROM deleted;
```

### DELETE with LIMIT (via CTE)

PostgreSQL doesn't support DELETE ... LIMIT directly, but you can use a CTE:

```sql
-- Delete first 100 rows matching condition
WITH to_delete AS (
    SELECT id FROM large_table
    WHERE status = 'expired'
    ORDER BY created_at
    LIMIT 100
)
DELETE FROM large_table
WHERE id IN (SELECT id FROM to_delete);

-- Batch delete pattern
WITH batch AS (
    SELECT id FROM events
    WHERE processed = true
    ORDER BY id
    LIMIT 1000
    FOR UPDATE SKIP LOCKED
)
DELETE FROM events WHERE id IN (SELECT id FROM batch);
```

---

## MERGE Statement

PostgreSQL 15+ supports the SQL standard MERGE statement.

### Basic Syntax

```sql
MERGE INTO target_table [[AS] alias]
USING source_table_or_query [[AS] alias]
ON merge_condition
WHEN MATCHED [AND condition] THEN
    { UPDATE SET column = expression [, ...] | DELETE }
WHEN NOT MATCHED [AND condition] THEN
    INSERT [(column_list)] VALUES (value_list)
```

### Basic MERGE

```sql
MERGE INTO inventory i
USING incoming_stock s
ON i.product_id = s.product_id AND i.warehouse_id = s.warehouse_id
WHEN MATCHED THEN
    UPDATE SET quantity = i.quantity + s.quantity
WHEN NOT MATCHED THEN
    INSERT (product_id, warehouse_id, quantity)
    VALUES (s.product_id, s.warehouse_id, s.quantity);
```

### MERGE with Multiple Actions

```sql
MERGE INTO customers c
USING new_customer_data n
ON c.email = n.email
WHEN MATCHED AND n.action = 'update' THEN
    UPDATE SET
        name = n.name,
        phone = n.phone,
        updated_at = NOW()
WHEN MATCHED AND n.action = 'delete' THEN
    DELETE
WHEN NOT MATCHED THEN
    INSERT (email, name, phone, created_at)
    VALUES (n.email, n.name, n.phone, NOW());
```

### MERGE from VALUES

```sql
MERGE INTO settings s
USING (VALUES
    ('theme', 'dark'),
    ('language', 'en'),
    ('timezone', 'UTC')
) AS v(key, value)
ON s.key = v.key
WHEN MATCHED THEN
    UPDATE SET value = v.value
WHEN NOT MATCHED THEN
    INSERT (key, value) VALUES (v.key, v.value);
```

### MERGE from Subquery

```sql
MERGE INTO product_stats ps
USING (
    SELECT
        product_id,
        COUNT(*) AS order_count,
        SUM(quantity) AS total_sold
    FROM order_items
    WHERE order_date > CURRENT_DATE - INTERVAL '30 days'
    GROUP BY product_id
) AS recent
ON ps.product_id = recent.product_id
WHEN MATCHED THEN
    UPDATE SET
        monthly_orders = recent.order_count,
        monthly_sold = recent.total_sold
WHEN NOT MATCHED THEN
    INSERT (product_id, monthly_orders, monthly_sold)
    VALUES (recent.product_id, recent.order_count, recent.total_sold);
```

---

## TRUNCATE Statement

Fast table clearing without logging individual row deletions.

### Basic Syntax

```sql
TRUNCATE [TABLE] table_name [, ...]
[RESTART IDENTITY | CONTINUE IDENTITY]
[CASCADE | RESTRICT]
```

### Examples

```sql
-- Basic truncate
TRUNCATE TABLE temp_data;

-- Multiple tables
TRUNCATE TABLE logs, events, notifications;

-- Reset sequences
TRUNCATE TABLE orders RESTART IDENTITY;

-- Keep sequence values
TRUNCATE TABLE orders CONTINUE IDENTITY;

-- Cascade to dependent tables
TRUNCATE TABLE customers CASCADE;

-- Restrict if dependencies exist (default)
TRUNCATE TABLE customers RESTRICT;
```

### TRUNCATE vs DELETE

| Feature | TRUNCATE | DELETE |
|---------|----------|--------|
| Speed | Fast (no row-by-row) | Slower |
| WHERE clause | No | Yes |
| Triggers | No row triggers | Yes |
| Transaction | Can rollback | Can rollback |
| RETURNING | No | Yes |
| Resets sequences | Optional | No |
| Vacuum needed | No | Yes |

---

## COPY Statement

Bulk data import/export.

### Basic Syntax

```sql
-- Export
COPY table_name [(column_list)] TO 'filename' [WITH options]

-- Import
COPY table_name [(column_list)] FROM 'filename' [WITH options]

-- Using STDIN/STDOUT
COPY table_name FROM STDIN;
COPY table_name TO STDOUT;
```

### Export Examples

```sql
-- Export to CSV
COPY users TO '/tmp/users.csv' WITH (FORMAT csv, HEADER true);

-- Export specific columns
COPY (SELECT id, email FROM users WHERE active = true)
TO '/tmp/active_users.csv' WITH (FORMAT csv, HEADER true);

-- Export with options
COPY products TO '/tmp/products.csv' WITH (
    FORMAT csv,
    HEADER true,
    DELIMITER ',',
    QUOTE '"',
    ESCAPE '"',
    NULL ''
);

-- Export to STDOUT (for piping)
COPY users TO STDOUT WITH (FORMAT csv);
```

### Import Examples

```sql
-- Import from CSV
COPY users FROM '/tmp/users.csv' WITH (FORMAT csv, HEADER true);

-- Import specific columns
COPY users (email, name) FROM '/tmp/partial.csv'
WITH (FORMAT csv, HEADER true);

-- Import with options
COPY products FROM '/tmp/products.csv' WITH (
    FORMAT csv,
    HEADER true,
    DELIMITER ',',
    QUOTE '"',
    NULL 'NULL'
);

-- Import from STDIN
COPY users FROM STDIN WITH (FORMAT csv);
-- Then paste data, end with \.
```

### COPY Formats

| Format | Description |
|--------|-------------|
| `text` | Tab-separated (default) |
| `csv` | Comma-separated values |
| `binary` | PostgreSQL binary format |

---

## Batch Operations

### Batch INSERT

```sql
-- Multi-row VALUES
INSERT INTO logs (message, level, created_at)
VALUES
    ('Starting process', 'INFO', NOW()),
    ('Processing item 1', 'DEBUG', NOW()),
    ('Processing item 2', 'DEBUG', NOW()),
    ('Process complete', 'INFO', NOW());

-- INSERT ... SELECT for bulk copy
INSERT INTO archive_logs
SELECT * FROM logs WHERE created_at < '2026-01-01';
```

### Batch UPDATE

```sql
-- Update from VALUES list
UPDATE products p
SET price = v.new_price
FROM (VALUES
    (1, 9.99),
    (2, 19.99),
    (3, 29.99)
) AS v(id, new_price)
WHERE p.id = v.id;

-- Update from temp table
CREATE TEMP TABLE price_updates (id INT, new_price DECIMAL);
-- ... populate temp table ...
UPDATE products p SET price = u.new_price
FROM price_updates u WHERE p.id = u.id;
```

### Batch DELETE

```sql
-- Delete with IN list
DELETE FROM notifications
WHERE id IN (1, 2, 3, 4, 5);

-- Delete from subquery
DELETE FROM sessions
WHERE id IN (
    SELECT id FROM sessions
    WHERE expires_at < NOW()
    ORDER BY expires_at
    LIMIT 10000
);
```

---

## Known Limitations

### Current Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| Basic INSERT | Partial | Simple inserts work |
| INSERT ... VALUES | Partial | Single and multi-row |
| INSERT ... SELECT | Stubbed | Bytecode mismatch |
| INSERT ... RETURNING | Stubbed | Executor doesn't process |
| ON CONFLICT | Stubbed | Parser accepts, executor fails |
| Basic UPDATE | Partial | Simple updates work |
| UPDATE ... FROM | Stubbed | Bytecode mismatch |
| UPDATE ... RETURNING | Stubbed | Executor doesn't process |
| Basic DELETE | Partial | Simple deletes work |
| DELETE ... USING | Stubbed | Bytecode mismatch |
| DELETE ... RETURNING | Stubbed | Executor doesn't process |
| MERGE | Stubbed | Parser accepts, no executor |
| TRUNCATE | Implemented | Works correctly |
| COPY | Partial | Basic import/export works |

### Specific Issues

**Bytecode Format Mismatches:**
- RETURNING clause generates EXT_RETURNING_CLAUSE that executor doesn't handle
- ON CONFLICT generates unsupported opcode payload
- UPDATE ... FROM join encoding differs from executor expectations
- DELETE ... USING similarly mismatched

**Unsupported Features:**
- RETURNING clause on INSERT/UPDATE/DELETE
- ON CONFLICT DO UPDATE
- UPDATE ... FROM (multi-table update)
- DELETE ... USING (multi-table delete)
- MERGE statement
- CTEs with data modification

### Workarounds

**For RETURNING:** Perform a separate SELECT after modification:
```sql
-- Instead of:
-- INSERT INTO users (email) VALUES ('test@test.com') RETURNING id;

-- Use:
INSERT INTO users (email) VALUES ('test@test.com');
SELECT id FROM users WHERE email = 'test@test.com';
```

**For ON CONFLICT:** Use explicit check:
```sql
-- Instead of:
-- INSERT INTO users (email) VALUES ('test@test.com')
-- ON CONFLICT (email) DO UPDATE SET updated_at = NOW();

-- Use:
DO $$
BEGIN
    UPDATE users SET updated_at = NOW() WHERE email = 'test@test.com';
    IF NOT FOUND THEN
        INSERT INTO users (email) VALUES ('test@test.com');
    END IF;
END $$;
```

**For UPDATE ... FROM:** Use correlated subquery:
```sql
-- Instead of:
-- UPDATE orders o SET status = 'shipped' FROM shipments s
-- WHERE o.id = s.order_id;

-- Use:
UPDATE orders
SET status = 'shipped'
WHERE id IN (SELECT order_id FROM shipments);
```

---

## See Also

- [DML SELECT](06_dml_select.md) - SELECT queries
- [Transaction Control](08_transactions.md) - BEGIN, COMMIT, ROLLBACK
- [Tables and Constraints](02_tables_and_constraints.md) - CREATE TABLE
- [Indexes](03_indexes_views_sequences.md) - Index management

