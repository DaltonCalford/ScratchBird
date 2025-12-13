# CREATE TABLE

Create a new table.

[Back to DDL Index](index.md) | [Back to Language Guide](../index.md)

---

## Syntax

```sql
CREATE TABLE [ IF NOT EXISTS ] table_name (
    column_name data_type [ constraints ],
    ...
    [ table_constraints ]
);
```

---

## Column Definition

```sql
column_name data_type
    [ NOT NULL | NULL ]
    [ DEFAULT expression ]
    [ PRIMARY KEY ]
    [ UNIQUE ]
    [ REFERENCES other_table(column) ]
    [ CHECK (expression) ]
```

---

## Examples

### Basic Table

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255)
);
```

### Auto-Increment

```sql
-- SERIAL (PostgreSQL style)
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,
    customer_id INTEGER NOT NULL,
    total DECIMAL(10,2)
);

-- GENERATED (SQL standard)
CREATE TABLE products (
    id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(100) NOT NULL
);
```

### With Defaults

```sql
CREATE TABLE posts (
    id SERIAL PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    content TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_published BOOLEAN DEFAULT FALSE,
    view_count INTEGER DEFAULT 0
);
```

### Foreign Keys

```sql
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    product_id INTEGER REFERENCES products(id),
    quantity INTEGER NOT NULL
);

-- With options
CREATE TABLE order_items (
    id SERIAL PRIMARY KEY,
    order_id INTEGER REFERENCES orders(id)
        ON DELETE CASCADE
        ON UPDATE CASCADE,
    product_id INTEGER REFERENCES products(id)
        ON DELETE RESTRICT
);
```

### Check Constraints

```sql
CREATE TABLE accounts (
    id SERIAL PRIMARY KEY,
    email VARCHAR(255) NOT NULL,
    balance DECIMAL(12,2) CHECK (balance >= 0),
    status VARCHAR(20) CHECK (status IN ('active', 'suspended', 'closed'))
);
```

### Composite Primary Key

```sql
CREATE TABLE order_items (
    order_id INTEGER REFERENCES orders(id),
    product_id INTEGER REFERENCES products(id),
    quantity INTEGER NOT NULL,
    PRIMARY KEY (order_id, product_id)
);
```

### Unique Constraints

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    username VARCHAR(50),
    UNIQUE (username)
);

-- Composite unique
CREATE TABLE subscriptions (
    user_id INTEGER,
    service_id INTEGER,
    UNIQUE (user_id, service_id)
);
```

---

## Data Types

### Numeric

| Type | Description |
|------|-------------|
| `SMALLINT` | -32768 to 32767 |
| `INTEGER` | -2B to 2B |
| `BIGINT` | Large integers |
| `DECIMAL(p,s)` | Exact numeric |
| `REAL` | 32-bit float |
| `DOUBLE PRECISION` | 64-bit float |
| `SERIAL` | Auto-increment integer |
| `BIGSERIAL` | Auto-increment bigint |

### Text

| Type | Description |
|------|-------------|
| `CHAR(n)` | Fixed-length |
| `VARCHAR(n)` | Variable-length |
| `TEXT` | Unlimited length |

### Date/Time

| Type | Description |
|------|-------------|
| `DATE` | Date only |
| `TIME` | Time only |
| `TIMESTAMP` | Date and time |
| `INTERVAL` | Duration |

### Other

| Type | Description |
|------|-------------|
| `BOOLEAN` | TRUE/FALSE |
| `UUID` | Unique identifier |
| `JSON` | JSON data |
| `JSONB` | Binary JSON |
| `BYTEA` | Binary data |

---

## Table Options

### IF NOT EXISTS

```sql
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100)
);
```

### TEMPORARY

```sql
CREATE TEMPORARY TABLE temp_results (
    id INTEGER,
    value TEXT
);
-- Dropped at end of session
```

### UNLOGGED

```sql
CREATE UNLOGGED TABLE cache (
    key VARCHAR(255) PRIMARY KEY,
    value TEXT
);
-- Faster but not crash-safe
```

---

## Partitioning

```sql
-- Range partitioning
CREATE TABLE sales (
    id SERIAL,
    sale_date DATE,
    amount DECIMAL(10,2)
) PARTITION BY RANGE (sale_date);

-- Create partitions
CREATE TABLE sales_2024_q1 PARTITION OF sales
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');

CREATE TABLE sales_2024_q2 PARTITION OF sales
    FOR VALUES FROM ('2024-04-01') TO ('2024-07-01');
```

---

## Generated Columns

```sql
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    price DECIMAL(10,2),
    tax_rate DECIMAL(4,2) DEFAULT 0.10,
    total DECIMAL(10,2) GENERATED ALWAYS AS (price * (1 + tax_rate)) STORED
);
```

---

## CREATE TABLE AS

Create from query:

```sql
CREATE TABLE active_users AS
SELECT id, name, email
FROM users
WHERE active = TRUE;
```

---

## Describing Tables

```sql
-- sb_isql / psql
\d users
\d+ users  -- with more detail

-- SQL
SELECT column_name, data_type, is_nullable
FROM information_schema.columns
WHERE table_name = 'users';
```

---

## Notes

- Table names are case-insensitive unless quoted
- Maximum columns per table: 1600
- Column names cannot be SQL keywords unless quoted

---

## See Also

- [ALTER TABLE](alter-table.md)
- [CREATE INDEX](create-index.md)
- [Data Types](../data-types/index.md)
