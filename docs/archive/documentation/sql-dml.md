### DML: INSERT, UPDATE, DELETE, MERGE, UPSERT

**What it is**

Data Manipulation Language (DML) statements modify data within tables. ScratchBird implements comprehensive DML support including standard INSERT, UPDATE, DELETE operations plus advanced features like MERGE for conditional updates/inserts, UPSERT for conflict resolution, and RETURNING clauses for immediate result retrieval.

**Why it matters**

- **Data Management**: DML is essential for maintaining and updating database content
- **Atomicity**: Each DML operation is atomic, ensuring data consistency
- **Performance**: Bulk operations and RETURNING clauses reduce round-trips
- **Triggers**: DML operations fire triggers and maintain constraints
- **Concurrency**: Proper locking and conflict resolution enable multi-user access

**How to use it**

Choose the appropriate DML statement for your use case. Use RETURNING clauses to get immediate feedback, leverage MERGE for complex conditional logic, and consider UPSERT for conflict-free inserts.

## DML Parser Architecture

The DML parser (`src/engine/parser_dml.cpp`) handles:
- Statement tokenization and normalization
- Expression parsing for conditions and values
- RETURNING clause extraction
- Complex MERGE action parsing
- WHERE clause normalization via expression parser

## INSERT Statement

### Basic INSERT Syntax

```sql
INSERT INTO table_name [(column_list)]
{VALUES (value_list) [, ...] | DEFAULT VALUES | select_statement}
[RETURNING column_list]
```

### Single Row Insert

```sql
-- Explicit columns
INSERT INTO users (username, email, created_at)
VALUES ('john_doe', 'john@example.com', CURRENT_TIMESTAMP);

-- All columns (order matters)
INSERT INTO products
VALUES (1, 'Widget', 19.99, 100, TRUE);

-- DEFAULT VALUES
INSERT INTO audit_log DEFAULT VALUES;
```

### Multi-Row Insert

```sql
-- Multiple value sets
INSERT INTO order_items (order_id, product_id, quantity, price)
VALUES 
    (1001, 5, 2, 29.99),
    (1001, 8, 1, 49.99),
    (1001, 12, 3, 9.99);

-- With expressions
INSERT INTO temperature_readings (sensor_id, reading, timestamp)
VALUES 
    (1, 23.5, NOW()),
    (2, 24.1, NOW()),
    (3, 22.8, NOW());
```

### INSERT with SELECT

```sql
-- Copy from another table
INSERT INTO archived_orders
SELECT * FROM orders
WHERE order_date < DATE '2023-01-01';

-- Transform during insert
INSERT INTO user_stats (user_id, total_orders, total_spent)
SELECT 
    customer_id,
    COUNT(*),
    SUM(total_amount)
FROM orders
GROUP BY customer_id;

-- With CTEs
WITH new_customers AS (
    SELECT DISTINCT email, name
    FROM import_data
    WHERE email NOT IN (SELECT email FROM users)
)
INSERT INTO users (email, username, created_at)
SELECT email, LOWER(REPLACE(name, ' ', '_')), CURRENT_TIMESTAMP
FROM new_customers;
```

### RETURNING Clause

Get immediate feedback from INSERT operations:

```sql
-- Return generated ID
INSERT INTO posts (title, content, author_id)
VALUES ('My First Post', 'Hello, world!', 42)
RETURNING id;

-- Return multiple columns
INSERT INTO orders (customer_id, total, status)
VALUES (123, 99.99, 'pending')
RETURNING id, created_at, order_number;

-- Return all columns
INSERT INTO audit_entries (action, user_id, details)
VALUES ('LOGIN', 456, '{"ip": "192.168.1.1"}')
RETURNING *;

-- Return expressions
INSERT INTO products (name, cost, markup)
VALUES ('Gadget', 10.00, 2.5)
RETURNING id, name, cost * markup AS sale_price;
```

## UPDATE Statement

### Basic UPDATE Syntax

```sql
UPDATE table_name
SET column = value [, ...]
[FROM from_clause]
[WHERE condition]
[RETURNING column_list]
```

### Simple Updates

```sql
-- Update single column
UPDATE products
SET price = price * 1.10
WHERE category = 'electronics';

-- Update multiple columns
UPDATE users
SET 
    last_login = CURRENT_TIMESTAMP,
    login_count = login_count + 1,
    last_ip = '192.168.1.100'
WHERE id = 789;

-- Update with expressions
UPDATE inventory
SET 
    quantity = quantity - 5,
    last_updated = NOW(),
    status = CASE 
        WHEN quantity - 5 <= reorder_point THEN 'low'
        WHEN quantity - 5 <= 0 THEN 'out_of_stock'
        ELSE 'available'
    END
WHERE product_id = 123;
```

### UPDATE with FROM/JOIN

```sql
-- Update with JOIN
UPDATE order_items oi
SET discount = p.category_discount
FROM products p
WHERE oi.product_id = p.id
  AND p.category = 'clearance';

-- Complex JOIN update
UPDATE employees e
SET 
    salary = e.salary * (1 + d.raise_percentage / 100),
    last_raise_date = CURRENT_DATE
FROM departments d
JOIN performance_reviews pr ON e.id = pr.employee_id
WHERE e.department_id = d.id
  AND pr.rating >= 4
  AND pr.review_year = 2024;

-- Update with subquery
UPDATE products p
SET average_rating = (
    SELECT AVG(rating)
    FROM reviews r
    WHERE r.product_id = p.id
)
WHERE EXISTS (
    SELECT 1 FROM reviews r
    WHERE r.product_id = p.id
);
```

### UPDATE with RETURNING

```sql
-- Return updated values
UPDATE accounts
SET balance = balance - 100
WHERE id = 456
RETURNING id, balance AS new_balance;

-- Return old and new values using expressions
UPDATE products
SET price = price * 0.8
WHERE category = 'sale'
RETURNING 
    id,
    name,
    price AS new_price,
    price / 0.8 AS old_price;

-- Bulk update with RETURNING
UPDATE inventory
SET quantity = 0, status = 'discontinued'
WHERE last_sold < DATE '2023-01-01'
RETURNING *;
```

## DELETE Statement

### Basic DELETE Syntax

```sql
DELETE FROM table_name
[USING using_clause]
[WHERE condition]
[RETURNING column_list]
```

### Simple Deletes

```sql
-- Delete with condition
DELETE FROM sessions
WHERE last_activity < NOW() - INTERVAL '30 days';

-- Delete all rows (use with caution!)
DELETE FROM temp_data;

-- Delete with subquery
DELETE FROM orders
WHERE customer_id IN (
    SELECT id FROM customers
    WHERE status = 'inactive'
    AND last_order_date < DATE '2022-01-01'
);
```

### DELETE with USING

```sql
-- Delete with JOIN
DELETE FROM order_items
USING orders
WHERE order_items.order_id = orders.id
  AND orders.status = 'cancelled'
  AND orders.created_at < DATE '2023-01-01';

-- Complex USING clause
DELETE FROM duplicate_records d1
USING duplicate_records d2
WHERE d1.email = d2.email
  AND d1.id > d2.id;  -- Keep the record with smaller ID

-- Delete with multiple tables
DELETE FROM user_sessions
USING users, login_attempts
WHERE user_sessions.user_id = users.id
  AND users.id = login_attempts.user_id
  AND login_attempts.failed_count > 5
  AND login_attempts.last_attempt > NOW() - INTERVAL '1 hour';
```

### DELETE with RETURNING

```sql
-- Archive before deletion
WITH deleted AS (
    DELETE FROM orders
    WHERE status = 'completed'
      AND order_date < DATE '2023-01-01'
    RETURNING *
)
INSERT INTO archived_orders
SELECT * FROM deleted;

-- Return deleted count and details
DELETE FROM expired_tokens
WHERE expiry < CURRENT_TIMESTAMP
RETURNING id, user_id, created_at;

-- Cascade delete with tracking
DELETE FROM departments
WHERE id = 10
RETURNING 
    id,
    name,
    (SELECT COUNT(*) FROM employees WHERE department_id = 10) AS affected_employees;
```

## MERGE Statement

MERGE combines INSERT, UPDATE, and DELETE operations based on conditions:

### Basic MERGE Syntax

```sql
MERGE INTO target_table [alias]
USING source_table [alias]
ON merge_condition
WHEN MATCHED [AND condition] THEN
    {UPDATE SET assignments | DELETE}
WHEN NOT MATCHED [AND condition] THEN
    INSERT [(columns)] VALUES (values)
```

### MERGE Examples

```sql
-- Basic upsert pattern
MERGE INTO inventory t
USING incoming_shipment s
ON t.product_id = s.product_id
WHEN MATCHED THEN
    UPDATE SET 
        quantity = t.quantity + s.quantity,
        last_updated = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (product_id, quantity, last_updated)
    VALUES (s.product_id, s.quantity, CURRENT_TIMESTAMP);

-- Conditional actions
MERGE INTO customer_status target
USING (
    SELECT 
        customer_id,
        SUM(amount) as total_spent,
        COUNT(*) as order_count
    FROM orders
    WHERE order_date >= DATE '2024-01-01'
    GROUP BY customer_id
) source
ON target.customer_id = source.customer_id
WHEN MATCHED AND source.total_spent > 1000 THEN
    UPDATE SET 
        status = 'premium',
        total_spent = source.total_spent,
        last_updated = NOW()
WHEN MATCHED AND source.total_spent <= 1000 THEN
    UPDATE SET 
        status = 'regular',
        total_spent = source.total_spent,
        last_updated = NOW()
WHEN NOT MATCHED AND source.total_spent > 500 THEN
    INSERT (customer_id, status, total_spent, created_at)
    VALUES (source.customer_id, 'regular', source.total_spent, NOW());

-- MERGE with DELETE
MERGE INTO products p
USING product_updates u
ON p.id = u.product_id
WHEN MATCHED AND u.action = 'DELETE' THEN
    DELETE
WHEN MATCHED AND u.action = 'UPDATE' THEN
    UPDATE SET 
        name = u.name,
        price = u.price,
        updated_at = NOW()
WHEN NOT MATCHED AND u.action = 'INSERT' THEN
    INSERT (id, name, price, created_at)
    VALUES (u.product_id, u.name, u.price, NOW());
```

### DO NOTHING Action

```sql
-- Skip certain conditions
MERGE INTO users t
USING import_users s
ON t.email = s.email
WHEN MATCHED AND t.verified = TRUE THEN
    DO NOTHING  -- Don't update verified users
WHEN MATCHED THEN
    UPDATE SET 
        name = s.name,
        phone = s.phone
WHEN NOT MATCHED THEN
    INSERT (email, name, phone, created_at)
    VALUES (s.email, s.name, s.phone, NOW());
```

## UPSERT (UPDATE OR INSERT)

ScratchBird supports Firebird-style UPSERT:

### UPSERT Syntax

```sql
UPDATE OR INSERT INTO table [(columns)]
VALUES (values)
[MATCHING (key_columns)]
[RETURNING columns]
```

### UPSERT Examples

```sql
-- Basic UPSERT with explicit matching
UPDATE OR INSERT INTO user_preferences (user_id, theme, language)
VALUES (123, 'dark', 'en')
MATCHING (user_id);

-- UPSERT with RETURNING
UPDATE OR INSERT INTO product_views (product_id, view_count, last_viewed)
VALUES (456, 1, CURRENT_TIMESTAMP)
MATCHING (product_id)
RETURNING view_count;

-- Multiple column matching
UPDATE OR INSERT INTO price_history (product_id, date, price)
VALUES (789, CURRENT_DATE, 29.99)
MATCHING (product_id, date);
```

### UPSERT vs MERGE

| Feature | UPSERT | MERGE |
|---------|--------|-------|
| Syntax complexity | Simple | Complex |
| Conditional logic | Limited | Full |
| Multiple actions | No | Yes |
| Source flexibility | VALUES only | Any source |
| Performance | Optimized for single row | Optimized for bulk |

## Transaction Considerations

### Atomicity

```sql
BEGIN;

-- Multiple related updates
UPDATE accounts SET balance = balance - 100 WHERE id = 1;
UPDATE accounts SET balance = balance + 100 WHERE id = 2;
INSERT INTO transfers (from_id, to_id, amount) VALUES (1, 2, 100);

COMMIT;
```

### Isolation Levels

```sql
-- Set isolation level
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;

-- Perform consistent updates
UPDATE inventory
SET reserved = reserved + 10
WHERE product_id = 123
  AND available >= 10;
```

### Deadlock Prevention

```sql
-- Order updates consistently
BEGIN;

-- Always lock in ID order to prevent deadlocks
UPDATE accounts SET balance = balance - 100 
WHERE id = LEAST(1, 2);

UPDATE accounts SET balance = balance + 100
WHERE id = GREATEST(1, 2);

COMMIT;
```

## Performance Optimization

### Bulk Operations

```sql
-- Efficient bulk insert
INSERT INTO log_entries (timestamp, level, message)
SELECT 
    generate_series(
        '2024-01-01'::timestamp,
        '2024-01-31'::timestamp,
        '1 hour'::interval
    ),
    'INFO',
    'Hourly checkpoint'
;

-- Batch update
UPDATE products
SET discount = 
    CASE category
        WHEN 'electronics' THEN 0.15
        WHEN 'clothing' THEN 0.20
        WHEN 'books' THEN 0.10
        ELSE 0.05
    END
WHERE sale_ends > CURRENT_DATE;
```

### Index Considerations

```sql
-- Create indexes for UPDATE/DELETE conditions
CREATE INDEX idx_orders_status_date ON orders(status, order_date);

-- Efficient deletion with index
DELETE FROM orders
WHERE status = 'cancelled'
  AND order_date < DATE '2023-01-01';
```

## Implementation Details

**Parser Components** (`src/engine/parser_dml.cpp`):
- `parse_insert_minimal`: INSERT statement parser
- `parse_update_minimal`: UPDATE statement parser
- `parse_delete_minimal`: DELETE statement parser
- `parse_merge_minimal`: MERGE statement parser
- `parse_upsert_minimal`: UPSERT statement parser

**Data Structures** (`include/scratchbird/engine/parser_dml.h`):
- `InsertStmt`: Captures INSERT components
- `UpdateStmt`: UPDATE with SET assignments
- `DeleteStmt`: DELETE with USING support
- `MergeStmt`: Complex MERGE actions
- `UpsertStmt`: UPDATE OR INSERT structure

**Expression Normalization**:
- WHERE clauses parsed via `normalize_where_expr`
- SET assignments handled as key-value pairs
- RETURNING lists captured as string vectors

**Code Anchors**:
- DML parser: `src/engine/parser_dml.cpp`
- Statement structures: `include/scratchbird/engine/parser_dml.h`
- Expression parsing: Integration with `parser_expr.cpp`

## See also

- [Operators](./sql-operators.md) - Expression operators in conditions
- [SELECT Queries](./sql-select.md) - Subqueries in DML
- [Tables](./ddl-tables.md) - Table constraints affecting DML
- [Triggers](./psql-routines-and-triggers.md) - DML-triggered procedures
- [Transactions](./session-and-transaction.md) - Transaction control