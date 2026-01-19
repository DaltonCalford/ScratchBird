# Migrating from MySQL

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

---

## Overview

MySQL is a popular migration source for ScratchBird. Thanks to ScratchBird's MySQL protocol emulation, most applications can connect using their existing MySQL drivers (mysql-connector, MySQLdb, mysql2, JDBC, etc.) with minimal changes.

**Topics covered:**
- Compatibility overview
- Data type mapping
- Schema migration
- Stored procedure conversion
- Data export and import
- Application connection changes

---

## Part 1: Compatibility Overview

### How MySQL Emulation Works

ScratchBird provides MySQL compatibility at two levels:

1. **Wire Protocol** - Port 3306 speaks MySQL protocol
2. **SQL Parser** - MySQL SQL syntax is parsed and converted to SBLR bytecode

```
┌─────────────────────────────────────────────────────────────┐
│                   MySQL Application                          │
│        (mysql-connector, MySQLdb, JDBC, etc.)               │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                ScratchBird MySQL Listener                    │
│                      (Port 3306)                            │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                  MySQL SQL Parser                            │
│                 (MySQL SQL → SBLR)                          │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              SBLR Executor + MGA Storage                    │
└─────────────────────────────────────────────────────────────┘
```

### What Works Automatically

**SQL syntax (DML):**
- SELECT with all clauses (WHERE, GROUP BY, HAVING, ORDER BY, LIMIT)
- JOINs (INNER, LEFT, RIGHT, CROSS, NATURAL, STRAIGHT_JOIN)
- Subqueries (scalar, derived tables, EXISTS, IN, ANY, ALL)
- Common Table Expressions (WITH, WITH RECURSIVE - MySQL 8.0+)
- Set operations (UNION, UNION ALL)
- INSERT, UPDATE, DELETE
- INSERT ... ON DUPLICATE KEY UPDATE
- REPLACE INTO

**SQL syntax (DDL):**
- CREATE/ALTER/DROP TABLE
- All constraint types (PRIMARY KEY, FOREIGN KEY, UNIQUE, CHECK)
- Indexes (including composite and partial)
- AUTO_INCREMENT columns
- Views
- Schemas/Databases

**Data types:**
- All numeric types (TINYINT to BIGINT, FLOAT, DOUBLE, DECIMAL)
- String types (CHAR, VARCHAR, TEXT, BLOB)
- Date/time types (DATE, TIME, DATETIME, TIMESTAMP)
- JSON type
- ENUM and SET (with some limitations)

**MySQL-specific features:**
- Backtick quoting for identifiers
- LIMIT with offset (LIMIT 10, 20)
- IF() function
- IFNULL(), COALESCE()
- String concatenation with CONCAT()

### What Needs Attention

| Feature | MySQL | ScratchBird | Migration Action |
|---------|-------|-------------|------------------|
| Storage engines | InnoDB, MyISAM, etc. | MGA (single engine) | Remove ENGINE clause |
| MySQL procedures | Native | Convert to SBLR | Rewrite procedures |
| FULLTEXT indexes | Native | Limited | Use alternative search |
| Spatial indexes | Native | Limited | Review usage |
| MySQL replication | Binary log | Native | Reconfigure |
| Events/Scheduler | EVENT | Not supported | Use external scheduler |
| Partitioning | Multiple types | Declarative | Review syntax |
| Generated columns | STORED/VIRTUAL | GENERATED | Adjust syntax |

---

## Part 2: Data Type Mapping

### Numeric Types

| MySQL Type | ScratchBird Type | Notes |
|------------|------------------|-------|
| TINYINT | SMALLINT | Promoted to 16-bit |
| TINYINT UNSIGNED | SMALLINT | 0-255 range preserved |
| SMALLINT | SMALLINT | 16-bit signed |
| MEDIUMINT | INTEGER | Promoted to 32-bit |
| INT / INTEGER | INTEGER | 32-bit signed |
| BIGINT | BIGINT | 64-bit signed |
| FLOAT | REAL | 4-byte float |
| DOUBLE | DOUBLE PRECISION | 8-byte float |
| DECIMAL(p,s) | DECIMAL(p,s) | Exact numeric |
| NUMERIC(p,s) | NUMERIC(p,s) | Exact numeric |

**Unsigned integers:**
```sql
-- MySQL unsigned types
CREATE TABLE counters (
    id INT UNSIGNED,
    value BIGINT UNSIGNED
);

-- ScratchBird: Use larger signed type or CHECK constraint
CREATE TABLE counters (
    id BIGINT CHECK (id >= 0),
    value NUMERIC(20,0) CHECK (value >= 0)
);

-- Or simply use signed types if range permits
CREATE TABLE counters (
    id INTEGER,
    value BIGINT
);
```

### String Types

| MySQL Type | ScratchBird Type | Notes |
|------------|------------------|-------|
| CHAR(n) | CHAR(n) | Fixed length |
| VARCHAR(n) | VARCHAR(n) | Variable length |
| TINYTEXT | TEXT | Unlimited |
| TEXT | TEXT | Unlimited |
| MEDIUMTEXT | TEXT | Unlimited |
| LONGTEXT | TEXT | Unlimited |
| TINYBLOB | BYTEA | Binary |
| BLOB | BYTEA | Binary |
| MEDIUMBLOB | BYTEA | Binary |
| LONGBLOB | BYTEA | Binary |
| BINARY(n) | BYTEA | Binary fixed |
| VARBINARY(n) | BYTEA | Binary variable |

### Date/Time Types

| MySQL Type | ScratchBird Type | Notes |
|------------|------------------|-------|
| DATE | DATE | Date only |
| TIME | TIME | Time only |
| DATETIME | TIMESTAMP | Date + time |
| TIMESTAMP | TIMESTAMPTZ | With time zone |
| YEAR | SMALLINT | Or use DATE |

**Timestamp handling:**
```sql
-- MySQL TIMESTAMP with auto-update
CREATE TABLE logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    message TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- ScratchBird equivalent
CREATE TABLE logs (
    id INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
    message TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Add trigger for ON UPDATE behavior
CREATE FUNCTION update_timestamp() RETURNS TRIGGER
LANGUAGE SBLR AS $$
BEGIN
    NEW.updated_at := CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$;

CREATE TRIGGER trg_logs_update
BEFORE UPDATE ON logs
FOR EACH ROW EXECUTE FUNCTION update_timestamp();
```

### ENUM and SET Types

```sql
-- MySQL ENUM
CREATE TABLE orders (
    id INT PRIMARY KEY,
    status ENUM('pending', 'processing', 'shipped', 'delivered')
);

-- ScratchBird: Use CHECK constraint
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    status VARCHAR(20) CHECK (status IN ('pending', 'processing', 'shipped', 'delivered'))
);

-- Or create a proper enum type
CREATE TYPE order_status AS ENUM ('pending', 'processing', 'shipped', 'delivered');
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    status order_status
);

-- MySQL SET
CREATE TABLE preferences (
    id INT PRIMARY KEY,
    options SET('email', 'sms', 'push')
);

-- ScratchBird: Use array or JSONB
CREATE TABLE preferences (
    id INTEGER PRIMARY KEY,
    options TEXT[]  -- Array of options
);

-- Or JSONB
CREATE TABLE preferences (
    id INTEGER PRIMARY KEY,
    options JSONB  -- ["email", "sms"]
);
```

### JSON Type

```sql
-- MySQL JSON works similarly
CREATE TABLE documents (
    id INT PRIMARY KEY,
    data JSON
);

-- JSON path operators differ slightly
-- MySQL
SELECT data->'$.name' FROM documents;
SELECT JSON_EXTRACT(data, '$.name') FROM documents;

-- ScratchBird (PostgreSQL-style)
SELECT data->>'name' FROM documents;
SELECT data->'nested'->>'field' FROM documents;
```

---

## Part 3: Schema Migration

### Step 1: Export Schema from MySQL

```bash
# Schema only (no data)
mysqldump -h source-host -u user -p --no-data mydb > schema.sql

# With specific tables
mysqldump -h source-host -u user -p --no-data mydb orders customers > tables.sql

# Without definer information (recommended)
mysqldump -h source-host -u user -p --no-data \
    --skip-definer \
    --skip-add-locks \
    mydb > schema.sql
```

### Step 2: Review and Transform Schema

**Remove storage engine references:**

```sql
-- Before (MySQL)
CREATE TABLE orders (
    id INT PRIMARY KEY,
    total DECIMAL(10,2)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- After (ScratchBird)
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    total DECIMAL(10,2)
);
```

**Convert AUTO_INCREMENT:**

```sql
-- MySQL AUTO_INCREMENT
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100)
);

-- ScratchBird options:

-- Option 1: SERIAL (MySQL-compatible syntax works)
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100)
);

-- Option 2: GENERATED AS IDENTITY
CREATE TABLE users (
    id INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
    name VARCHAR(100)
);

-- Option 3: Explicit sequence
CREATE SEQUENCE users_id_seq;
CREATE TABLE users (
    id INTEGER DEFAULT nextval('users_id_seq') PRIMARY KEY,
    name VARCHAR(100)
);
```

**Handle MySQL-specific column options:**

```sql
-- MySQL with various options
CREATE TABLE products (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
    price DECIMAL(10,2) UNSIGNED,
    description TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_name (name(50))
) ENGINE=InnoDB;

-- ScratchBird
CREATE TABLE products (
    id INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
    name VARCHAR(100),
    price DECIMAL(10,2) CHECK (price >= 0),
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_name ON products(name);

-- Add trigger for updated_at
CREATE TRIGGER trg_products_update
BEFORE UPDATE ON products
FOR EACH ROW EXECUTE FUNCTION update_timestamp();
```

### Step 3: Transform Indexes

```sql
-- MySQL index syntax
CREATE INDEX idx_orders_customer ON orders(customer_id);
CREATE UNIQUE INDEX idx_users_email ON users(email);
CREATE FULLTEXT INDEX idx_products_search ON products(name, description);

-- ScratchBird
CREATE INDEX idx_orders_customer ON orders(customer_id);
CREATE UNIQUE INDEX idx_users_email ON users(email);
-- FULLTEXT not directly supported - use text search alternatives
```

### Step 4: Import Schema to ScratchBird

```bash
# Via MySQL protocol
mysql -h scratchbird-host -P 3306 -u admin -p mydb < schema.sql

# Or using sb_isql
sb_isql -U admin -d mydb -f schema.sql
```

---

## Part 4: Stored Procedure Migration

### Basic Procedure Conversion

**MySQL procedure:**
```sql
DELIMITER //
CREATE PROCEDURE GetCustomerOrders(IN p_customer_id INT)
BEGIN
    SELECT id, order_date, total
    FROM orders
    WHERE customer_id = p_customer_id
    ORDER BY order_date DESC;
END //
DELIMITER ;
```

**ScratchBird SBLR function:**
```sql
CREATE FUNCTION get_customer_orders(p_customer_id INTEGER)
RETURNS TABLE (id INTEGER, order_date DATE, total DECIMAL(10,2))
LANGUAGE SBLR AS $$
BEGIN
    RETURN QUERY
    SELECT id, order_date, total
    FROM orders
    WHERE customer_id = p_customer_id
    ORDER BY order_date DESC;
END;
$$;
```

### Variables and Control Flow

**MySQL:**
```sql
DELIMITER //
CREATE PROCEDURE CalculateDiscount(
    IN p_amount DECIMAL(10,2),
    OUT p_discount DECIMAL(10,2)
)
BEGIN
    DECLARE v_rate DECIMAL(5,2);

    IF p_amount > 1000 THEN
        SET v_rate = 0.10;
    ELSEIF p_amount > 500 THEN
        SET v_rate = 0.05;
    ELSE
        SET v_rate = 0;
    END IF;

    SET p_discount = p_amount * v_rate;
END //
DELIMITER ;
```

**ScratchBird SBLR:**
```sql
CREATE FUNCTION calculate_discount(p_amount DECIMAL(10,2))
RETURNS DECIMAL(10,2)
LANGUAGE SBLR AS $$
DECLARE
    v_rate DECIMAL(5,2);
BEGIN
    IF p_amount > 1000 THEN
        v_rate := 0.10;
    ELSIF p_amount > 500 THEN
        v_rate := 0.05;
    ELSE
        v_rate := 0;
    END IF;

    RETURN p_amount * v_rate;
END;
$$;
```

### Cursors and Loops

**MySQL:**
```sql
DELIMITER //
CREATE PROCEDURE ProcessOrders()
BEGIN
    DECLARE v_done INT DEFAULT FALSE;
    DECLARE v_order_id INT;
    DECLARE v_total DECIMAL(10,2);

    DECLARE cur CURSOR FOR SELECT id, total FROM orders WHERE status = 'pending';
    DECLARE CONTINUE HANDLER FOR NOT FOUND SET v_done = TRUE;

    OPEN cur;
    read_loop: LOOP
        FETCH cur INTO v_order_id, v_total;
        IF v_done THEN
            LEAVE read_loop;
        END IF;

        -- Process order
        UPDATE orders SET status = 'processing' WHERE id = v_order_id;
    END LOOP;
    CLOSE cur;
END //
DELIMITER ;
```

**ScratchBird SBLR:**
```sql
CREATE PROCEDURE process_orders()
LANGUAGE SBLR AS $$
DECLARE
    v_order RECORD;
BEGIN
    FOR v_order IN SELECT id, total FROM orders WHERE status = 'pending'
    LOOP
        -- Process order
        UPDATE orders SET status = 'processing' WHERE id = v_order.id;
    END LOOP;
END;
$$;
```

### Error Handling

**MySQL:**
```sql
DELIMITER //
CREATE PROCEDURE SafeTransfer(
    IN p_from INT,
    IN p_to INT,
    IN p_amount DECIMAL(10,2)
)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Transfer failed';
    END;

    START TRANSACTION;
    UPDATE accounts SET balance = balance - p_amount WHERE id = p_from;
    UPDATE accounts SET balance = balance + p_amount WHERE id = p_to;
    COMMIT;
END //
DELIMITER ;
```

**ScratchBird SBLR:**
```sql
CREATE PROCEDURE safe_transfer(
    p_from INTEGER,
    p_to INTEGER,
    p_amount DECIMAL(10,2)
)
LANGUAGE SBLR AS $$
BEGIN
    UPDATE accounts SET balance = balance - p_amount WHERE id = p_from;
    UPDATE accounts SET balance = balance + p_amount WHERE id = p_to;
EXCEPTION
    WHEN OTHERS THEN
        RAISE EXCEPTION 'Transfer failed: %', SQLERRM;
END;
$$;
```

### Triggers

**MySQL:**
```sql
DELIMITER //
CREATE TRIGGER before_order_insert
BEFORE INSERT ON orders
FOR EACH ROW
BEGIN
    SET NEW.created_at = NOW();
    SET NEW.order_number = CONCAT('ORD-', LPAD(NEW.id, 8, '0'));
END //
DELIMITER ;
```

**ScratchBird:**
```sql
CREATE FUNCTION before_order_insert() RETURNS TRIGGER
LANGUAGE SBLR AS $$
BEGIN
    NEW.created_at := CURRENT_TIMESTAMP;
    NEW.order_number := 'ORD-' || LPAD(NEW.id::TEXT, 8, '0');
    RETURN NEW;
END;
$$;

CREATE TRIGGER trg_order_insert
BEFORE INSERT ON orders
FOR EACH ROW EXECUTE FUNCTION before_order_insert();
```

---

## Part 5: Data Migration

### Method 1: mysqldump with MySQL Protocol

**Export from MySQL:**
```bash
# Full dump with data
mysqldump -h source-host -u user -p mydb > full_dump.sql

# Data only
mysqldump -h source-host -u user -p --no-create-info mydb > data.sql

# Specific tables
mysqldump -h source-host -u user -p mydb orders customers > tables.sql

# With extended inserts (faster)
mysqldump -h source-host -u user -p --extended-insert mydb > data.sql

# Single transaction (consistent snapshot)
mysqldump -h source-host -u user -p --single-transaction mydb > data.sql
```

**Import to ScratchBird:**
```bash
# Via MySQL protocol
mysql -h scratchbird-host -P 3306 -u admin -p mydb < full_dump.sql

# With error handling
mysql -h scratchbird-host -P 3306 -u admin -p mydb \
    --force < full_dump.sql 2> errors.log
```

### Method 2: CSV Export/Import

**Export from MySQL:**
```sql
-- In MySQL client
SELECT * FROM customers
INTO OUTFILE '/tmp/customers.csv'
FIELDS TERMINATED BY ','
ENCLOSED BY '"'
LINES TERMINATED BY '\n';

-- Or using command line
mysql -h source-host -u user -p -e \
    "SELECT * FROM customers" mydb | \
    sed 's/\t/,/g' > customers.csv
```

**Import to ScratchBird:**
```sql
-- Via PostgreSQL protocol (COPY is faster)
COPY customers FROM '/path/to/customers.csv'
WITH (FORMAT CSV, HEADER true);

-- Via MySQL protocol
LOAD DATA INFILE '/path/to/customers.csv'
INTO TABLE customers
FIELDS TERMINATED BY ','
ENCLOSED BY '"'
LINES TERMINATED BY '\n';
```

### Method 3: Python Migration Script

```python
import mysql.connector
import psycopg2

# Connect to MySQL source
mysql_conn = mysql.connector.connect(
    host='mysql-server',
    user='user',
    password='password',
    database='mydb'
)

# Connect to ScratchBird via PostgreSQL protocol
sb_conn = psycopg2.connect(
    host='scratchbird-server',
    port=5432,
    database='mydb',
    user='admin',
    password='password'
)

mysql_cur = mysql_conn.cursor()
sb_cur = sb_conn.cursor()

def migrate_table(table_name):
    # Get data from MySQL
    mysql_cur.execute(f"SELECT * FROM {table_name}")
    rows = mysql_cur.fetchall()

    if not rows:
        print(f"{table_name}: no data")
        return

    # Get column count
    col_count = len(rows[0])
    placeholders = ','.join(['%s'] * col_count)

    # Insert into ScratchBird
    for i in range(0, len(rows), 1000):
        batch = rows[i:i+1000]
        sb_cur.executemany(
            f"INSERT INTO {table_name} VALUES ({placeholders})",
            batch
        )
        sb_conn.commit()
        print(f"{table_name}: {i + len(batch)}/{len(rows)}")

# Migrate tables
tables = ['customers', 'orders', 'products', 'order_items']
for table in tables:
    migrate_table(table)

mysql_conn.close()
sb_conn.close()
```

### Method 4: Using DBeaver or Similar Tools

1. Connect to both MySQL source and ScratchBird target
2. Right-click source table → Export Data
3. Choose "Database Table(s)" as target
4. Select ScratchBird connection
5. Map columns and execute

---

## Part 6: SQL Syntax Differences

### String Functions

| MySQL | ScratchBird | Notes |
|-------|-------------|-------|
| `CONCAT(a, b)` | `CONCAT(a, b)` or `a \|\| b` | Both work |
| `CONCAT_WS(',', a, b)` | `CONCAT_WS(',', a, b)` | Same |
| `SUBSTRING(s, 1, 5)` | `SUBSTRING(s FROM 1 FOR 5)` | Standard SQL |
| `LENGTH(s)` | `LENGTH(s)` | Byte length |
| `CHAR_LENGTH(s)` | `CHAR_LENGTH(s)` | Character length |
| `LOCATE('x', s)` | `POSITION('x' IN s)` | Standard SQL |
| `LPAD(s, 10, '0')` | `LPAD(s, 10, '0')` | Same |
| `TRIM(s)` | `TRIM(s)` | Same |

### Date Functions

| MySQL | ScratchBird | Notes |
|-------|-------------|-------|
| `NOW()` | `NOW()` or `CURRENT_TIMESTAMP` | Same |
| `CURDATE()` | `CURRENT_DATE` | Standard SQL |
| `CURTIME()` | `CURRENT_TIME` | Standard SQL |
| `DATE_ADD(d, INTERVAL 1 DAY)` | `d + INTERVAL '1 day'` | Standard SQL |
| `DATEDIFF(d1, d2)` | `d1 - d2` | Returns interval |
| `DATE_FORMAT(d, '%Y-%m-%d')` | `TO_CHAR(d, 'YYYY-MM-DD')` | Different format |
| `STR_TO_DATE(s, '%Y-%m-%d')` | `TO_DATE(s, 'YYYY-MM-DD')` | Different format |
| `UNIX_TIMESTAMP()` | `EXTRACT(EPOCH FROM NOW())` | Returns seconds |
| `FROM_UNIXTIME(n)` | `TO_TIMESTAMP(n)` | From seconds |

### Control Flow

| MySQL | ScratchBird | Notes |
|-------|-------------|-------|
| `IF(cond, a, b)` | `CASE WHEN cond THEN a ELSE b END` | Standard SQL |
| `IFNULL(a, b)` | `COALESCE(a, b)` | Standard SQL |
| `NULLIF(a, b)` | `NULLIF(a, b)` | Same |
| `COALESCE(a, b, c)` | `COALESCE(a, b, c)` | Same |

**Query transformations:**
```sql
-- MySQL
SELECT IF(status = 'active', 'Yes', 'No') AS is_active FROM users;

-- ScratchBird
SELECT CASE WHEN status = 'active' THEN 'Yes' ELSE 'No' END AS is_active FROM users;

-- MySQL
SELECT IFNULL(phone, 'N/A') FROM customers;

-- ScratchBird
SELECT COALESCE(phone, 'N/A') FROM customers;
```

### LIMIT Syntax

```sql
-- MySQL LIMIT with offset
SELECT * FROM orders LIMIT 10, 20;  -- Skip 10, return 20

-- ScratchBird (both work)
SELECT * FROM orders LIMIT 20 OFFSET 10;  -- Standard SQL
SELECT * FROM orders LIMIT 10, 20;        -- MySQL syntax supported
```

### INSERT Variations

```sql
-- MySQL INSERT ... ON DUPLICATE KEY UPDATE
INSERT INTO users (id, name, email)
VALUES (1, 'John', 'john@example.com')
ON DUPLICATE KEY UPDATE name = VALUES(name), email = VALUES(email);

-- ScratchBird (PostgreSQL-style)
INSERT INTO users (id, name, email)
VALUES (1, 'John', 'john@example.com')
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name, email = EXCLUDED.email;

-- MySQL REPLACE INTO
REPLACE INTO users (id, name, email)
VALUES (1, 'John', 'john@example.com');

-- ScratchBird equivalent
INSERT INTO users (id, name, email)
VALUES (1, 'John', 'john@example.com')
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name, email = EXCLUDED.email;
```

---

## Part 7: Application Changes

### Connection String Changes

**Python (mysql-connector):**
```python
# Before (MySQL)
conn = mysql.connector.connect(
    host='mysql-server',
    port=3306,
    database='mydb',
    user='appuser',
    password='secret'
)

# After (ScratchBird - just change host)
conn = mysql.connector.connect(
    host='scratchbird-server',
    port=3306,
    database='mydb',
    user='appuser',
    password='secret'
)
```

**Node.js (mysql2):**
```javascript
// Before
const pool = mysql.createPool({
  host: 'mysql-server',
  port: 3306,
  database: 'mydb',
  user: 'appuser',
  password: 'secret'
});

// After (just change host)
const pool = mysql.createPool({
  host: 'scratchbird-server',
  port: 3306,
  database: 'mydb',
  user: 'appuser',
  password: 'secret'
});
```

**PHP (PDO):**
```php
// Before
$dsn = 'mysql:host=mysql-server;port=3306;dbname=mydb';
$pdo = new PDO($dsn, 'appuser', 'secret');

// After (just change host)
$dsn = 'mysql:host=scratchbird-server;port=3306;dbname=mydb';
$pdo = new PDO($dsn, 'appuser', 'secret');
```

**JDBC (Java):**
```java
// Before
String url = "jdbc:mysql://mysql-server:3306/mydb";

// After (just change host)
String url = "jdbc:mysql://scratchbird-server:3306/mydb";
```

**Laravel:**
```php
// .env
DB_CONNECTION=mysql
DB_HOST=scratchbird-server  // Changed from mysql-server
DB_PORT=3306
DB_DATABASE=mydb
DB_USERNAME=appuser
DB_PASSWORD=secret
```

**Django:**
```python
# settings.py
DATABASES = {
    'default': {
        'ENGINE': 'django.db.backends.mysql',
        'HOST': 'scratchbird-server',  # Changed from mysql-server
        'PORT': '3306',
        'NAME': 'mydb',
        'USER': 'appuser',
        'PASSWORD': 'secret',
    }
}
```

### Code Changes

**1. Date format strings:**
```python
# Before (MySQL format)
cursor.execute("SELECT DATE_FORMAT(created_at, '%Y-%m-%d') FROM orders")

# After (ScratchBird - use TO_CHAR)
cursor.execute("SELECT TO_CHAR(created_at, 'YYYY-MM-DD') FROM orders")
```

**2. AUTO_INCREMENT value:**
```python
# Before (MySQL)
cursor.execute("SELECT LAST_INSERT_ID()")

# After (ScratchBird via MySQL protocol - works)
cursor.execute("SELECT LAST_INSERT_ID()")

# Or use RETURNING (via PostgreSQL protocol)
cursor.execute("INSERT INTO users (name) VALUES ('John') RETURNING id")
```

**3. SHOW commands:**
```sql
-- These MySQL commands are supported
SHOW DATABASES;
SHOW TABLES;
SHOW COLUMNS FROM orders;
SHOW INDEX FROM orders;
SHOW CREATE TABLE orders;

-- Some may have limited output
SHOW PROCESSLIST;  -- Shows active connections
SHOW STATUS;       -- Limited statistics
```

---

## Part 8: Testing and Validation

### Schema Validation

```sql
-- Compare table structure
SHOW COLUMNS FROM customers;

-- Or via information_schema
SELECT column_name, data_type, is_nullable, column_default
FROM information_schema.columns
WHERE table_name = 'customers'
ORDER BY ordinal_position;

-- Compare indexes
SHOW INDEX FROM customers;
```

### Data Validation

```sql
-- Row counts
SELECT 'customers' AS table_name, COUNT(*) AS row_count FROM customers
UNION ALL SELECT 'orders', COUNT(*) FROM orders
UNION ALL SELECT 'products', COUNT(*) FROM products;

-- Checksums
SELECT
    COUNT(*) AS row_count,
    SUM(id) AS id_sum,
    COUNT(DISTINCT email) AS unique_emails,
    MIN(created_at) AS earliest,
    MAX(created_at) AS latest
FROM customers;
```

### Functional Testing

```sql
-- Test queries
SELECT * FROM orders WHERE customer_id = 123 LIMIT 10;

-- Test functions
SELECT calculate_discount(1500.00);

-- Test insert with auto-increment
INSERT INTO customers (name, email) VALUES ('Test', 'test@example.com');
SELECT LAST_INSERT_ID();

-- Test transactions
START TRANSACTION;
UPDATE accounts SET balance = balance - 100 WHERE id = 1;
UPDATE accounts SET balance = balance + 100 WHERE id = 2;
COMMIT;
```

---

## Part 9: Performance Optimization

### Index Optimization

```sql
-- Rebuild indexes after migration
-- Via MySQL syntax
OPTIMIZE TABLE customers;
OPTIMIZE TABLE orders;

-- Via PostgreSQL protocol (more options)
REINDEX TABLE customers;
ANALYZE customers;
```

### Query Analysis

```sql
-- Explain queries
EXPLAIN SELECT * FROM orders WHERE customer_id = 123;

-- Extended explain
EXPLAIN EXTENDED SELECT * FROM orders WHERE customer_id = 123;

-- Or via PostgreSQL protocol
EXPLAIN (ANALYZE, BUFFERS) SELECT * FROM orders WHERE customer_id = 123;
```

### Configuration

```ini
# sb_server.conf

[memory]
shared_buffers = 256MB
work_mem = 4MB

[mysql]
# MySQL-specific settings
max_connections = 150
query_cache_size = 64MB
```

---

## Part 10: Quick Reference

### Command Mapping

| MySQL | ScratchBird |
|-------|-------------|
| `mysql` | `mysql` (via port 3306) or `sb_isql` |
| `mysqldump` | `mysqldump` or `sb_backup` |
| `mysqlimport` | `mysqlimport` or `sb_restore` |
| `mysqladmin` | `sb_admin` |

### SQL Compatibility

| Feature | MySQL | ScratchBird | Notes |
|---------|-------|-------------|-------|
| AUTO_INCREMENT | Native | Supported | Maps to SEQUENCE |
| LIMIT offset,count | Native | Supported | MySQL syntax works |
| Backtick quoting | Native | Supported | \`table\` works |
| ON DUPLICATE KEY | Native | Use ON CONFLICT | PostgreSQL syntax |
| REPLACE INTO | Native | Use ON CONFLICT | PostgreSQL syntax |
| GROUP_CONCAT | Native | STRING_AGG | Different function |
| IFNULL | Native | COALESCE | Standard SQL |

### Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| Storage engine error | ENGINE clause | Remove ENGINE specification |
| AUTO_INCREMENT not working | Sequence syntax | Use SERIAL or GENERATED |
| Procedure syntax error | MySQL procedure syntax | Convert to SBLR |
| FULLTEXT error | FULLTEXT not supported | Use alternative search |
| UNSIGNED error | Unsigned types | Use CHECK constraint |
| DATE_FORMAT error | Different format codes | Use TO_CHAR |

---

## Migration Checklist

- [ ] Analyze MySQL database (storage engines, procedures, triggers)
- [ ] Export schema with mysqldump --no-data
- [ ] Review and transform schema
- [ ] Remove ENGINE clauses and MySQL-specific options
- [ ] Convert AUTO_INCREMENT to SERIAL/GENERATED
- [ ] Convert stored procedures to SBLR
- [ ] Convert triggers
- [ ] Create database in ScratchBird
- [ ] Import schema
- [ ] Test schema with sample queries
- [ ] Export data from MySQL
- [ ] Import data to ScratchBird
- [ ] Validate row counts
- [ ] Validate data checksums
- [ ] Test functions and triggers
- [ ] Optimize tables
- [ ] Update application connection strings
- [ ] Test application functionality
- [ ] Performance testing
- [ ] Go live

---

## See Also

- [Migration Overview](Migration-Overview.md)
- [Migration Checklist](Migration-Checklist.md)
- [MySQL SQL Reference](../language-guides/mysql/)
- [Troubleshooting](../troubleshooting/)

