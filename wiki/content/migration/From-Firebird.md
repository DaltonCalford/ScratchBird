# Migrating from Firebird

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

---

## Overview

Firebird is the most compatible source database for ScratchBird migration due to their shared Multi-Generational Architecture (MGA) heritage. Most Firebird applications can migrate with minimal changes, and transaction semantics map cleanly between the two systems.

**Topics covered:**
- Compatibility overview
- Data type mapping
- Schema migration
- Stored procedure conversion
- Data export and import
- Application connection changes

---

## Part 1: Compatibility Overview

### Why Firebird Migration is Easy

ScratchBird and Firebird share fundamental architecture:

| Feature | Firebird | ScratchBird | Compatibility |
|---------|----------|-------------|---------------|
| Transaction model | MGA | MGA | Identical |
| Isolation levels | Snapshot, Read Committed | Snapshot, Read Committed | Identical |
| Record versioning | In-place | In-place | Identical |
| Generators/Sequences | GENERATOR | SEQUENCE (+ GENERATOR alias) | Full |
| Domains | Full support | Full support | Full |
| Nullable semantics | SQL-standard | SQL-standard | Identical |
| Character sets | Extensive | Extensive | High |
| Collations | Per-column | Per-column | High |

### What Works Automatically

**SQL syntax:**
- SELECT, INSERT, UPDATE, DELETE, MERGE
- JOINs (INNER, LEFT, RIGHT, FULL, CROSS)
- Subqueries and CTEs
- UNION, INTERSECT, EXCEPT
- CASE expressions
- Aggregate functions
- Window functions (Firebird 3.0+)

**DDL:**
- CREATE/ALTER/DROP TABLE
- PRIMARY KEY, FOREIGN KEY, UNIQUE, CHECK constraints
- Indexes (including partial indexes)
- Domains
- Generators/Sequences
- Views
- Stored procedures
- Triggers
- Exceptions

**Transaction control:**
- BEGIN, COMMIT, ROLLBACK
- SAVEPOINT
- SET TRANSACTION isolation levels

### What Needs Attention

| Feature | Firebird | ScratchBird | Migration Action |
|---------|----------|-------------|------------------|
| External tables | FILE PATH | Not supported | Use COPY or import |
| UDFs (C/C++) | ib_udf, etc. | Not supported | Rewrite as SBLR functions |
| BLOB filters | BLOB SUB_TYPE | Limited | Review usage |
| EXECUTE BLOCK | Full | Partial | Test thoroughly |
| Events | POST_EVENT | Native events | Syntax change |
| nBackup | Incremental | sb_backup | Tool change |
| GBAK | Backup/restore | sb_backup/restore | Tool change |

---

## Part 2: Data Type Mapping

### Exact Mappings

These types work identically:

| Firebird Type | ScratchBird Type | Notes |
|---------------|------------------|-------|
| SMALLINT | SMALLINT | 16-bit signed |
| INTEGER | INTEGER | 32-bit signed |
| BIGINT | BIGINT | 64-bit signed |
| FLOAT | FLOAT | IEEE single |
| DOUBLE PRECISION | DOUBLE PRECISION | IEEE double |
| DECIMAL(p,s) | DECIMAL(p,s) | Exact numeric |
| NUMERIC(p,s) | NUMERIC(p,s) | Exact numeric |
| CHAR(n) | CHAR(n) | Fixed length |
| VARCHAR(n) | VARCHAR(n) | Variable length |
| DATE | DATE | Date only |
| TIME | TIME | Time only |
| TIMESTAMP | TIMESTAMP | Date + time |
| BLOB SUB_TYPE TEXT | TEXT | Use TEXT for convenience |
| BLOB SUB_TYPE BINARY | BYTEA | Binary data |
| BOOLEAN | BOOLEAN | Firebird 3.0+ |

### Type Conversions

**BLOB handling:**
```sql
-- Firebird BLOB SUB_TYPE TEXT
CREATE TABLE docs (
    id INTEGER,
    content BLOB SUB_TYPE TEXT
);

-- ScratchBird equivalent (TEXT is easier to use)
CREATE TABLE docs (
    id INTEGER,
    content TEXT
);

-- Or keep BLOB for binary data
CREATE TABLE files (
    id INTEGER,
    data BYTEA  -- was BLOB SUB_TYPE BINARY
);
```

**Character set handling:**
```sql
-- Firebird with explicit character set
CREATE TABLE messages (
    id INTEGER,
    body VARCHAR(1000) CHARACTER SET UTF8
);

-- ScratchBird (UTF8 is default)
CREATE TABLE messages (
    id INTEGER,
    body VARCHAR(1000)  -- UTF8 by default
);

-- If you need a specific character set
CREATE TABLE legacy_data (
    id INTEGER,
    content VARCHAR(1000) CHARACTER SET LATIN1
);
```

**Array types (Firebird 2.x+):**
```sql
-- Firebird arrays
CREATE TABLE matrix (
    id INTEGER,
    values INTEGER[10]
);

-- ScratchBird (use standard SQL arrays)
CREATE TABLE matrix (
    id INTEGER,
    values INTEGER[]  -- Dynamic array
);
```

---

## Part 3: Schema Migration

### Step 1: Export Schema from Firebird

**Using isql:**
```bash
# Extract DDL only (no data)
isql -ex database.fdb -u SYSDBA -p masterkey -o schema.sql

# Or using gbak metadata-only backup
gbak -b -m -user SYSDBA -password masterkey database.fdb metadata.fbk
```

**Using FlameRobin or similar GUI:**
1. Connect to database
2. Right-click database → Extract DDL
3. Save as `schema.sql`

### Step 2: Review and Transform Schema

**Domain definitions (usually work as-is):**
```sql
-- Firebird domain
CREATE DOMAIN D_MONEY AS
    DECIMAL(18,2)
    DEFAULT 0
    NOT NULL
    CHECK (VALUE >= 0);

-- Works in ScratchBird without changes
```

**Generator to Sequence:**
```sql
-- Firebird generator
CREATE GENERATOR GEN_CUSTOMER_ID;
SET GENERATOR GEN_CUSTOMER_ID TO 1000;

-- ScratchBird (both syntaxes work)
-- Option 1: Keep Firebird syntax (supported)
CREATE GENERATOR GEN_CUSTOMER_ID;
ALTER GENERATOR GEN_CUSTOMER_ID RESTART WITH 1000;

-- Option 2: Use standard SQL
CREATE SEQUENCE GEN_CUSTOMER_ID START WITH 1000;
```

**Table with generator:**
```sql
-- Firebird table
CREATE TABLE customers (
    id INTEGER NOT NULL,
    name VARCHAR(100),
    CONSTRAINT pk_customers PRIMARY KEY (id)
);

-- Firebird trigger for auto-increment
CREATE TRIGGER bi_customers FOR customers
ACTIVE BEFORE INSERT POSITION 0
AS
BEGIN
    IF (NEW.id IS NULL) THEN
        NEW.id = GEN_ID(GEN_CUSTOMER_ID, 1);
END;

-- ScratchBird (both work, but you can simplify)
-- Option 1: Keep Firebird style (fully compatible)
-- Option 2: Use GENERATED clause
CREATE TABLE customers (
    id INTEGER GENERATED BY DEFAULT AS IDENTITY,
    name VARCHAR(100),
    CONSTRAINT pk_customers PRIMARY KEY (id)
);
```

### Step 3: Transform Indexes

```sql
-- Firebird indexes (work as-is)
CREATE INDEX idx_customers_name ON customers(name);
CREATE UNIQUE INDEX idx_customers_email ON customers(email);
CREATE DESCENDING INDEX idx_orders_date ON orders(order_date);

-- ScratchBird equivalent for descending
CREATE INDEX idx_orders_date ON orders(order_date DESC);
```

### Step 4: Import Schema to ScratchBird

```bash
# Using native protocol
sb_isql -U admin -d mydb -f schema.sql

# Or using Firebird protocol (if your schema uses Firebird syntax)
isql -H localhost -p 3050 -U SYSDBA -P masterkey mydb -i schema.sql
```

---

## Part 4: Stored Procedure Migration

### Basic Procedure Conversion

**Firebird procedure:**
```sql
CREATE PROCEDURE GET_CUSTOMER_ORDERS (
    P_CUSTOMER_ID INTEGER
)
RETURNS (
    ORDER_ID INTEGER,
    ORDER_DATE DATE,
    TOTAL DECIMAL(18,2)
)
AS
BEGIN
    FOR SELECT id, order_date, total_amount
        FROM orders
        WHERE customer_id = :P_CUSTOMER_ID
        ORDER BY order_date DESC
        INTO :ORDER_ID, :ORDER_DATE, :TOTAL
    DO
        SUSPEND;
END;
```

**ScratchBird (Firebird syntax supported):**
```sql
-- The above procedure works in ScratchBird with Firebird emulation
-- Or convert to standard SQL/SBLR:
CREATE FUNCTION get_customer_orders(p_customer_id INTEGER)
RETURNS TABLE (
    order_id INTEGER,
    order_date DATE,
    total DECIMAL(18,2)
)
LANGUAGE SBLR
AS $$
    SELECT id, order_date, total_amount
    FROM orders
    WHERE customer_id = p_customer_id
    ORDER BY order_date DESC;
$$;
```

### Variable Handling

**Firebird:**
```sql
CREATE PROCEDURE CALCULATE_DISCOUNT (
    P_AMOUNT DECIMAL(18,2)
)
RETURNS (
    DISCOUNT DECIMAL(18,2)
)
AS
DECLARE VARIABLE V_RATE DECIMAL(5,2);
BEGIN
    IF (P_AMOUNT > 1000) THEN
        V_RATE = 0.10;
    ELSE IF (P_AMOUNT > 500) THEN
        V_RATE = 0.05;
    ELSE
        V_RATE = 0;

    DISCOUNT = P_AMOUNT * V_RATE;
    SUSPEND;
END;
```

**ScratchBird SBLR:**
```sql
CREATE FUNCTION calculate_discount(p_amount DECIMAL(18,2))
RETURNS DECIMAL(18,2)
LANGUAGE SBLR
AS $$
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

### Exception Handling

**Firebird:**
```sql
CREATE EXCEPTION E_INVALID_AMOUNT 'Amount must be positive';

CREATE PROCEDURE VALIDATE_AMOUNT (P_AMOUNT DECIMAL(18,2))
AS
BEGIN
    IF (P_AMOUNT <= 0) THEN
        EXCEPTION E_INVALID_AMOUNT;
END;
```

**ScratchBird:**
```sql
-- Custom exceptions
CREATE EXCEPTION E_INVALID_AMOUNT 'Amount must be positive';

-- Or use RAISE
CREATE PROCEDURE validate_amount(p_amount DECIMAL(18,2))
LANGUAGE SBLR
AS $$
BEGIN
    IF p_amount <= 0 THEN
        RAISE EXCEPTION 'Amount must be positive';
    END IF;
END;
$$;
```

### Selectable Procedures

**Firebird selectable procedure:**
```sql
CREATE PROCEDURE LIST_PRODUCTS
RETURNS (
    ID INTEGER,
    NAME VARCHAR(100),
    PRICE DECIMAL(18,2)
)
AS
BEGIN
    FOR SELECT id, name, price FROM products
        INTO :ID, :NAME, :PRICE
    DO
        SUSPEND;
END;

-- Usage
SELECT * FROM LIST_PRODUCTS;
```

**ScratchBird:**
```sql
-- Use table-returning function
CREATE FUNCTION list_products()
RETURNS TABLE (
    id INTEGER,
    name VARCHAR(100),
    price DECIMAL(18,2)
)
LANGUAGE SBLR
AS $$
    SELECT id, name, price FROM products;
$$;

-- Usage
SELECT * FROM list_products();
```

---

## Part 5: Trigger Migration

### Before/After Triggers

**Firebird:**
```sql
CREATE TRIGGER audit_customer_update FOR customers
ACTIVE AFTER UPDATE POSITION 0
AS
BEGIN
    INSERT INTO customer_audit (
        customer_id, field_name, old_value, new_value, changed_at
    )
    VALUES (
        OLD.id, 'name', OLD.name, NEW.name, CURRENT_TIMESTAMP
    );
END;
```

**ScratchBird (compatible syntax):**
```sql
CREATE TRIGGER audit_customer_update
AFTER UPDATE ON customers
FOR EACH ROW
BEGIN
    INSERT INTO customer_audit (
        customer_id, field_name, old_value, new_value, changed_at
    )
    VALUES (
        OLD.id, 'name', OLD.name, NEW.name, CURRENT_TIMESTAMP
    );
END;
```

### Multiple Trigger Events

**Firebird:**
```sql
CREATE TRIGGER set_timestamps FOR orders
ACTIVE BEFORE INSERT OR UPDATE POSITION 0
AS
BEGIN
    IF (INSERTING) THEN
        NEW.created_at = CURRENT_TIMESTAMP;
    NEW.updated_at = CURRENT_TIMESTAMP;
END;
```

**ScratchBird:**
```sql
CREATE TRIGGER set_timestamps
BEFORE INSERT OR UPDATE ON orders
FOR EACH ROW
BEGIN
    IF (INSERTING) THEN
        NEW.created_at := CURRENT_TIMESTAMP;
    END IF;
    NEW.updated_at := CURRENT_TIMESTAMP;
END;
```

---

## Part 6: Data Migration

### Method 1: Using GBAK (Recommended)

**Export from Firebird:**
```bash
# Create transportable backup
gbak -b -t -user SYSDBA -password masterkey source.fdb backup.fbk
```

**Import to ScratchBird:**
```bash
# ScratchBird can restore Firebird backups
sb_restore --from-firebird backup.fbk -U admin -d mydb

# Or with options
sb_restore --from-firebird backup.fbk \
    -U admin \
    -d mydb \
    --convert-charsets \
    --skip-indices  # Create indices after data load
```

### Method 2: Using isql Export

**Export data as INSERT statements:**
```bash
# In isql
OUTPUT data.sql;
SET HEADING OFF;
SET COUNT OFF;
SELECT 'INSERT INTO customers VALUES ('
    || id || ', '''
    || REPLACE(name, '''', '''''') || ''', '''
    || email || ''');'
FROM customers;
OUTPUT;
```

**Import to ScratchBird:**
```bash
sb_isql -U admin -d mydb -f data.sql
```

### Method 3: Using CSV

**Export from Firebird:**
```sql
-- Using isql
OUTPUT customers.csv;
SET HEADING OFF;
SET SEPARATOR ',';
SELECT id, name, email FROM customers;
OUTPUT;
```

**Import to ScratchBird:**
```sql
COPY customers (id, name, email)
FROM '/path/to/customers.csv'
WITH (FORMAT CSV, HEADER false);
```

### Method 4: Using External Tools

**With DBeaver or similar:**
1. Connect to both databases
2. Right-click source table → Export Data
3. Choose target connection (ScratchBird)
4. Map columns and execute

**With Python script:**
```python
import fdb  # Firebird driver
import psycopg2  # PostgreSQL driver for ScratchBird

# Connect to Firebird
fb_conn = fdb.connect(
    dsn='localhost:database.fdb',
    user='SYSDBA',
    password='masterkey'
)

# Connect to ScratchBird via PostgreSQL protocol
sb_conn = psycopg2.connect(
    host='localhost',
    port=5432,
    database='mydb',
    user='admin',
    password='password'
)

fb_cur = fb_conn.cursor()
sb_cur = sb_conn.cursor()

# Migrate table
fb_cur.execute("SELECT id, name, email FROM customers")
rows = fb_cur.fetchall()

sb_cur.executemany(
    "INSERT INTO customers (id, name, email) VALUES (%s, %s, %s)",
    rows
)

sb_conn.commit()
```

---

## Part 7: Validating the Migration

### Schema Validation

```sql
-- Compare table structures
-- In ScratchBird (via Firebird protocol)
SELECT
    rf.rdb$field_name AS column_name,
    rf.rdb$field_source AS domain,
    CASE f.rdb$field_type
        WHEN 7 THEN 'SMALLINT'
        WHEN 8 THEN 'INTEGER'
        WHEN 16 THEN 'BIGINT'
        WHEN 10 THEN 'FLOAT'
        WHEN 27 THEN 'DOUBLE'
        WHEN 37 THEN 'VARCHAR'
        WHEN 14 THEN 'CHAR'
        ELSE 'OTHER'
    END AS data_type
FROM rdb$relation_fields rf
JOIN rdb$fields f ON rf.rdb$field_source = f.rdb$field_name
WHERE rf.rdb$relation_name = 'CUSTOMERS'
ORDER BY rf.rdb$field_position;
```

### Data Validation

```sql
-- Row counts
SELECT 'customers' AS table_name, COUNT(*) AS row_count FROM customers
UNION ALL
SELECT 'orders', COUNT(*) FROM orders
UNION ALL
SELECT 'order_items', COUNT(*) FROM order_items;

-- Checksum validation (compare with Firebird)
SELECT
    SUM(id) AS id_sum,
    COUNT(DISTINCT email) AS unique_emails,
    MIN(created_at) AS earliest,
    MAX(created_at) AS latest
FROM customers;
```

### Functional Testing

```sql
-- Test stored procedures
SELECT * FROM get_customer_orders(123);

-- Test triggers
INSERT INTO customers (name, email)
VALUES ('Test User', 'test@example.com');
-- Check that auto-increment worked

UPDATE customers SET name = 'Updated Name' WHERE id = 123;
-- Check audit table was populated

-- Test generators/sequences
SELECT GEN_ID(GEN_CUSTOMER_ID, 0) AS current_value;
-- Or
SELECT NEXT VALUE FOR gen_customer_id;
```

---

## Part 8: Application Changes

### Connection String Changes

**Delphi/FireDAC:**
```delphi
// Before (Firebird)
FDConnection1.DriverName := 'FB';
FDConnection1.Params.Values['Server'] := 'firebird-server';
FDConnection1.Params.Values['Database'] := '/data/mydb.fdb';

// After (ScratchBird via Firebird protocol)
FDConnection1.DriverName := 'FB';
FDConnection1.Params.Values['Server'] := 'scratchbird-server';
FDConnection1.Params.Values['Port'] := '3050';
FDConnection1.Params.Values['Database'] := 'mydb';
```

**Python (fdb):**
```python
# Before (Firebird)
conn = fdb.connect(
    dsn='firebird-server:database.fdb',
    user='SYSDBA',
    password='masterkey'
)

# After (ScratchBird)
conn = fdb.connect(
    host='scratchbird-server',
    port=3050,
    database='mydb',
    user='SYSDBA',
    password='masterkey'
)
```

**JDBC:**
```java
// Before (Firebird)
String url = "jdbc:firebirdsql://firebird-server:3050/database.fdb";

// After (ScratchBird)
String url = "jdbc:firebirdsql://scratchbird-server:3050/mydb";
```

**ADO.NET (FirebirdClient):**
```csharp
// Before
var connectionString = "Server=firebird-server;Database=/data/mydb.fdb;User=SYSDBA;Password=masterkey";

// After
var connectionString = "Server=scratchbird-server;Port=3050;Database=mydb;User=SYSDBA;Password=masterkey";
```

### Code Changes

Most Firebird application code works unchanged. Common adjustments:

**1. Database path vs name:**
```delphi
// Firebird uses file paths
Database := '/var/firebird/data/mydb.fdb';

// ScratchBird uses database names
Database := 'mydb';
```

**2. Event notification:**
```sql
-- Firebird
POST_EVENT 'order_created';

-- ScratchBird
NOTIFY order_created;
```

**3. External tables (not supported):**
```sql
-- Firebird external table
CREATE TABLE import_data EXTERNAL FILE '/data/import.csv' (
    ...
);

-- ScratchBird: Use COPY instead
COPY import_data FROM '/data/import.csv' WITH (FORMAT CSV);
```

---

## Part 9: Performance Tuning

### Index Recreation

After data migration, rebuild indexes for optimal performance:

```sql
-- Reindex all tables
REINDEX DATABASE mydb;

-- Or specific tables
REINDEX TABLE customers;
REINDEX TABLE orders;
```

### Statistics Update

```sql
-- Update statistics for query optimizer
ANALYZE;

-- Or specific tables
ANALYZE customers;
ANALYZE orders;
```

### Configuration Tuning

```ini
# sb_server.conf - tune based on workload

[memory]
# Similar to Firebird page buffers
buffer_pool_size = 256MB

# Work memory for sorts
work_mem = 4MB

[performance]
# Checkpoint settings
checkpoint_completion_target = 0.9

# Background writer
bgwriter_delay = 200ms
```

---

## Part 10: Quick Reference

### Command Mapping

| Firebird | ScratchBird |
|----------|-------------|
| `isql database.fdb` | `sb_isql -d mydb` or `isql mydb` (Firebird protocol) |
| `gbak -b` | `sb_backup` or `sb_backup --format=fbk` |
| `gbak -r` | `sb_restore` or `sb_restore --from-firebird` |
| `gfix` | `sb_admin` |
| `gstat` | `sb_stat` |

### SQL Syntax Comparison

| Operation | Firebird | ScratchBird |
|-----------|----------|-------------|
| Auto-increment | Generator + Trigger | GENERATED AS IDENTITY or Generator |
| String concat | `'a' \|\| 'b'` | `'a' \|\| 'b'` (same) |
| Current time | `CURRENT_TIMESTAMP` | `CURRENT_TIMESTAMP` (same) |
| Substring | `SUBSTRING(x FROM 1 FOR 5)` | `SUBSTRING(x FROM 1 FOR 5)` (same) |
| Coalesce | `COALESCE(a, b)` | `COALESCE(a, b)` (same) |
| Cast | `CAST(x AS INTEGER)` | `CAST(x AS INTEGER)` (same) |

### Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| Character set errors | Encoding mismatch | Set `--convert-charsets` on restore |
| Procedure errors | PSQL syntax differences | Review and adjust SBLR syntax |
| UDF not found | UDFs not supported | Rewrite as SBLR function |
| External table error | External tables not supported | Use COPY command |
| BLOB handling | Sub-type differences | Use TEXT for text, BYTEA for binary |

---

## Migration Checklist

- [ ] Export schema from Firebird (`isql -ex` or gbak)
- [ ] Review and transform schema if needed
- [ ] Create database in ScratchBird
- [ ] Import schema
- [ ] Migrate stored procedures to SBLR
- [ ] Migrate triggers
- [ ] Export data from Firebird
- [ ] Import data to ScratchBird
- [ ] Validate row counts
- [ ] Validate data checksums
- [ ] Test stored procedures
- [ ] Test triggers
- [ ] Rebuild indexes
- [ ] Update statistics
- [ ] Update application connection strings
- [ ] Test application functionality
- [ ] Performance testing
- [ ] Go live

---

## See Also

- [Migration Overview](Migration-Overview.md)
- [Migration Checklist](Migration-Checklist.md)
- [Firebird SQL Reference](../language-guides/firebirdsql/)
- [Troubleshooting](../troubleshooting/)

