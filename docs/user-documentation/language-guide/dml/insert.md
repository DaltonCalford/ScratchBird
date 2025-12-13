# INSERT

Add rows to a table.

[Back to DML Index](index.md) | [Back to Language Guide](../index.md)

---

## Basic Syntax

```sql
INSERT INTO table_name [ ( column_list ) ]
VALUES ( value_list )
[ RETURNING columns ];
```

---

## Single Row

```sql
-- All columns
INSERT INTO users VALUES (1, 'Alice', 'alice@example.com');

-- Specified columns
INSERT INTO users (name, email)
VALUES ('Bob', 'bob@example.com');
```

---

## Multiple Rows

```sql
INSERT INTO users (name, email) VALUES
    ('Alice', 'alice@example.com'),
    ('Bob', 'bob@example.com'),
    ('Carol', 'carol@example.com');
```

---

## With RETURNING

Get inserted values back:

```sql
-- Return generated ID
INSERT INTO users (name, email)
VALUES ('Alice', 'alice@example.com')
RETURNING id;

-- Return all columns
INSERT INTO users (name, email)
VALUES ('Alice', 'alice@example.com')
RETURNING *;

-- Return multiple columns
INSERT INTO users (name, email)
VALUES ('Alice', 'alice@example.com')
RETURNING id, created_at;
```

---

## Default Values

```sql
-- Use column defaults
INSERT INTO users (name)
VALUES ('Alice');
-- email will be NULL, created_at will use default

-- Explicit default
INSERT INTO users (name, created_at)
VALUES ('Alice', DEFAULT);
```

---

## Insert from SELECT

```sql
-- Copy data
INSERT INTO archive_users (id, name, email)
SELECT id, name, email
FROM users
WHERE created_at < '2020-01-01';

-- With transformation
INSERT INTO user_summary (user_id, order_count, total_spent)
SELECT user_id, COUNT(*), SUM(total)
FROM orders
GROUP BY user_id;
```

---

## ON CONFLICT (Upsert)

Handle duplicate key violations:

### DO NOTHING

```sql
INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com')
ON CONFLICT (id) DO NOTHING;
```

### DO UPDATE

```sql
INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com')
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    email = EXCLUDED.email,
    updated_at = CURRENT_TIMESTAMP;
```

### With WHERE

```sql
INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com')
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name
WHERE users.version < EXCLUDED.version;
```

### On Multiple Columns

```sql
INSERT INTO subscriptions (user_id, service_id, status)
VALUES (1, 2, 'active')
ON CONFLICT (user_id, service_id) DO UPDATE SET
    status = EXCLUDED.status;
```

---

## Parameter Markers

### PostgreSQL Style

```sql
INSERT INTO users (name, email) VALUES ($1, $2);
```

### MySQL/Firebird Style

```sql
INSERT INTO users (name, email) VALUES (?, ?);
```

---

## Bulk Loading

### COPY (Fastest)

```sql
COPY users (name, email) FROM '/path/to/data.csv' WITH CSV HEADER;

-- From stdin
COPY users (name, email) FROM STDIN WITH CSV;
Alice,alice@example.com
Bob,bob@example.com
\.
```

### Multi-Value INSERT

Faster than individual inserts:

```sql
INSERT INTO users (name, email) VALUES
    ('User1', 'user1@example.com'),
    ('User2', 'user2@example.com'),
    -- ... up to 1000 rows per statement
    ('User1000', 'user1000@example.com');
```

---

## Insert with CTE

```sql
WITH new_users AS (
    SELECT 'Alice' AS name, 'alice@example.com' AS email
    UNION ALL
    SELECT 'Bob', 'bob@example.com'
)
INSERT INTO users (name, email)
SELECT name, email FROM new_users
RETURNING *;
```

---

## Transactions

```sql
BEGIN;

INSERT INTO orders (user_id, total) VALUES (1, 99.99) RETURNING id;
-- Assume returned id = 123

INSERT INTO order_items (order_id, product_id, quantity)
VALUES (123, 1, 2);

COMMIT;
```

---

## Common Patterns

### Insert or Ignore

```sql
INSERT INTO tags (name)
VALUES ('important')
ON CONFLICT (name) DO NOTHING;
```

### Insert with Timestamp

```sql
INSERT INTO logs (message, created_at)
VALUES ('User login', CURRENT_TIMESTAMP);
```

### Insert with UUID

```sql
INSERT INTO sessions (id, user_id, token)
VALUES (gen_random_uuid(), 1, 'abc123');
```

### Insert NULL

```sql
INSERT INTO users (name, email, phone)
VALUES ('Alice', 'alice@example.com', NULL);
```

---

## Notes

- Column order must match value order
- Omitted columns use defaults or NULL
- SERIAL/IDENTITY columns auto-increment
- RETURNING requires PostgreSQL protocol

---

## See Also

- [UPDATE](update.md)
- [MERGE](merge.md)
- [Data Types](../data-types/index.md)
