# Migrating from PostgreSQL

**Last Updated:** 2026-01-28

---

## Overview

PostgreSQL is one of the most common migration paths to ScratchBird. Thanks to ScratchBird's PostgreSQL protocol emulation, most applications can connect using their existing PostgreSQL drivers (psycopg2, pg, JDBC, etc.) with minimal changes.

**Topics covered:**
- Compatibility overview
- Data type mapping
- Schema migration
- Stored procedure conversion
- Data export and import
- Application connection changes

---

## Part 1: Compatibility Overview

### How PostgreSQL Emulation Works

ScratchBird provides PostgreSQL compatibility at two levels:

1. **Wire Protocol** - Port 5432 speaks PostgreSQL protocol
2. **SQL Parser** - PostgreSQL SQL syntax is parsed and converted to SBLR bytecode

```
┌─────────────────────────────────────────────────────────────┐
│                PostgreSQL Application                        │
│              (psycopg2, pg, JDBC, etc.)                     │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              ScratchBird PostgreSQL Listener                 │
│                      (Port 5432)                            │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              PostgreSQL SQL Parser                          │
│              (PostgreSQL SQL → SBLR)                        │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              SBLR Executor + MGA Storage                    │
└─────────────────────────────────────────────────────────────┘
```

### What Works Automatically

**SQL syntax (DML):**
- SELECT with all clauses (WHERE, GROUP BY, HAVING, ORDER BY, LIMIT, OFFSET)
- JOINs (INNER, LEFT, RIGHT, FULL, CROSS, NATURAL)
- Subqueries (scalar, derived tables, EXISTS, IN, ANY, ALL)
- Common Table Expressions (WITH, WITH RECURSIVE)
- Set operations (UNION, INTERSECT, EXCEPT)
- INSERT, UPDATE, DELETE with RETURNING
- MERGE/UPSERT (INSERT ... ON CONFLICT)

**SQL syntax (DDL):**
- CREATE/ALTER/DROP TABLE
- All constraint types (PRIMARY KEY, FOREIGN KEY, UNIQUE, CHECK, EXCLUDE)
- Indexes (B-tree, Hash, partial indexes)
- Sequences (CREATE SEQUENCE, SERIAL types)
- Views and materialized views
- Schemas

**Data types:**
- All standard SQL types
- PostgreSQL-specific types (SERIAL, TEXT, BYTEA, arrays)
- JSON/JSONB
- UUID
- Date/time types with time zones

**Transaction control:**
- BEGIN, COMMIT, ROLLBACK
- SAVEPOINT, ROLLBACK TO, RELEASE
- Transaction isolation levels
- FOR UPDATE/FOR SHARE locking

### What Needs Attention

| Feature | PostgreSQL | ScratchBird | Migration Action |
|---------|------------|-------------|------------------|
| PL/pgSQL | Native | Convert to SBLR | Rewrite procedures |
| Extensions | 100+ available | Limited | Check availability |
| pg_catalog | Full | Emulated subset | Test catalog queries |
| Replication | Streaming/logical | Native | Reconfigure |
| Foreign Data Wrappers | postgres_fdw, etc. | Not supported | Remove or replace |
| Large Objects | pg_largeobject | Use BYTEA | Convert storage |
| Custom Types | CREATE TYPE | Limited | Simplify |
| Tablespaces | Multiple | Single | Remove references |
| Partitioning | Declarative/inheritance | Declarative only | May need adjustment |

---

## Part 2: Data Type Mapping

### Exact Mappings

These types work identically:

| PostgreSQL Type | ScratchBird Type | Notes |
|-----------------|------------------|-------|
| SMALLINT | SMALLINT | 16-bit signed |
| INTEGER / INT | INTEGER | 32-bit signed |
| BIGINT | BIGINT | 64-bit signed |
| REAL | REAL | 4-byte float |
| DOUBLE PRECISION | DOUBLE PRECISION | 8-byte float |
| NUMERIC(p,s) | NUMERIC(p,s) | Exact numeric |
| DECIMAL(p,s) | DECIMAL(p,s) | Exact numeric |
| CHAR(n) | CHAR(n) | Fixed length |
| VARCHAR(n) | VARCHAR(n) | Variable length |
| TEXT | TEXT | Unlimited text |
| BYTEA | BYTEA | Binary data |
| BOOLEAN | BOOLEAN | true/false |
| DATE | DATE | Date only |
| TIME | TIME | Time only |
| TIMESTAMP | TIMESTAMP | Date + time |
| TIMESTAMPTZ | TIMESTAMPTZ | With time zone |
| INTERVAL | INTERVAL | Time interval |
| UUID | UUID | Universally unique ID |
| JSON | JSON | JSON text |
| JSONB | JSONB | Binary JSON |

### Serial Types

```sql
-- PostgreSQL SERIAL types
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,           -- 32-bit auto-increment
    order_num BIGSERIAL,             -- 64-bit auto-increment
    small_id SMALLSERIAL             -- 16-bit auto-increment
);

-- ScratchBird supports SERIAL syntax
-- Internally creates sequence + default
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,
    order_num BIGSERIAL,
    small_id SMALLSERIAL
);

-- Or use explicit GENERATED
CREATE TABLE orders (
    id INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
    order_num BIGINT GENERATED BY DEFAULT AS IDENTITY,
    small_id SMALLINT GENERATED BY DEFAULT AS IDENTITY
);
```

### Array Types

```sql
-- PostgreSQL arrays work in ScratchBird
CREATE TABLE matrix (
    id INTEGER,
    int_values INTEGER[],
    text_values TEXT[],
    multi_dim INTEGER[][]
);

INSERT INTO matrix (id, int_values, text_values)
VALUES (1, ARRAY[1, 2, 3], ARRAY['a', 'b', 'c']);

-- Array operations supported
SELECT * FROM matrix WHERE 2 = ANY(int_values);
SELECT int_values[1] FROM matrix;
SELECT array_length(int_values, 1) FROM matrix;
```

### JSON/JSONB

```sql
-- JSON types work as expected
CREATE TABLE documents (
    id INTEGER PRIMARY KEY,
    data JSON,
    metadata JSONB
);

-- JSON operators
SELECT data->>'title' FROM documents;
SELECT data->'author'->>'name' FROM documents;
SELECT * FROM documents WHERE metadata @> '{"status": "active"}';

-- JSONB-specific operators
SELECT * FROM documents WHERE metadata ? 'tags';
SELECT * FROM documents WHERE metadata ?& ARRAY['title', 'author'];
```

### Types with Limited Support

```sql
-- PostgreSQL geometric types - limited support
-- POINT, LINE, CIRCLE, etc. - basic support, some operations missing

-- Range types - supported
CREATE TABLE events (
    id INTEGER,
    during TSRANGE,
    available INT4RANGE
);

-- Network types - basic support
-- INET, CIDR, MACADDR

-- Full-text search types - limited
-- TSVECTOR, TSQUERY - basic support, use JSONB for complex search
```

---

## Part 3: Schema Migration

### Step 1: Export Schema from PostgreSQL

```bash
# Schema only (no data)
pg_dump -h source-host -U user -d mydb --schema-only -f schema.sql

# With specific schemas
pg_dump -h source-host -U user -d mydb --schema-only \
    -n public -n app -f schema.sql

# Exclude problematic objects
pg_dump -h source-host -U user -d mydb --schema-only \
    --exclude-table='pg_*' \
    --no-publications \
    --no-subscriptions \
    -f schema.sql
```

### Step 2: Review and Transform Schema

**Remove PostgreSQL-specific features:**

```sql
-- Remove tablespace references
-- Before
CREATE TABLE users (...) TABLESPACE fast_storage;

-- After
CREATE TABLE users (...);

-- Remove storage parameters not supported
-- Before
CREATE TABLE logs (...) WITH (fillfactor=70, autovacuum_enabled=false);

-- After (ScratchBird has different options)
CREATE TABLE logs (...);
```

**Handle extensions:**

```sql
-- Check which extensions are used
-- In PostgreSQL:
SELECT * FROM pg_extension;

-- Common extensions and alternatives:
-- uuid-ossp: ScratchBird has built-in UUID support
-- Before
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
SELECT uuid_generate_v4();

-- After (built-in)
SELECT gen_random_uuid();

-- pg_trgm: Limited, use JSONB for text search
-- hstore: Use JSONB instead
-- PostGIS: Not available, use external GIS if needed
```

**Simplify custom types:**

```sql
-- PostgreSQL composite type
CREATE TYPE address AS (
    street VARCHAR(100),
    city VARCHAR(50),
    zip VARCHAR(10)
);

-- ScratchBird: Use JSONB or separate table
CREATE TABLE addresses (
    id INTEGER PRIMARY KEY,
    street VARCHAR(100),
    city VARCHAR(50),
    zip VARCHAR(10)
);

-- Or use JSONB
ALTER TABLE customers ADD COLUMN address JSONB;
```

### Step 3: Transform Constraints

```sql
-- Exclusion constraints - may need alternative
-- PostgreSQL
CREATE TABLE reservations (
    room_id INTEGER,
    during TSRANGE,
    EXCLUDE USING gist (room_id WITH =, during WITH &&)
);

-- ScratchBird - use trigger for validation
CREATE TABLE reservations (
    room_id INTEGER,
    during TSRANGE
);

CREATE FUNCTION check_reservation_overlap() RETURNS TRIGGER
LANGUAGE SBLR AS $$
BEGIN
    IF EXISTS (
        SELECT 1 FROM reservations
        WHERE room_id = NEW.room_id
        AND during && NEW.during
        AND id != COALESCE(NEW.id, -1)
    ) THEN
        RAISE EXCEPTION 'Overlapping reservation';
    END IF;
    RETURN NEW;
END;
$$;

CREATE TRIGGER trg_reservation_check
BEFORE INSERT OR UPDATE ON reservations
FOR EACH ROW EXECUTE FUNCTION check_reservation_overlap();
```

### Step 4: Import Schema to ScratchBird

```bash
# Connect via PostgreSQL protocol
psql -h localhost -p 5432 -U admin -d mydb -f schema.sql

# Or using sb_isql
sb_isql -U admin -d mydb -f schema.sql
```

---

## Part 4: Stored Procedure Migration

### PL/pgSQL to SBLR Conversion

**Basic function:**

```sql
-- PostgreSQL PL/pgSQL
CREATE FUNCTION get_user_count() RETURNS INTEGER AS $$
DECLARE
    user_count INTEGER;
BEGIN
    SELECT COUNT(*) INTO user_count FROM users;
    RETURN user_count;
END;
$$ LANGUAGE plpgsql;

-- ScratchBird SBLR
CREATE FUNCTION get_user_count() RETURNS INTEGER
LANGUAGE SBLR AS $$
DECLARE
    user_count INTEGER;
BEGIN
    SELECT COUNT(*) INTO user_count FROM users;
    RETURN user_count;
END;
$$;
```

**Function with parameters:**

```sql
-- PostgreSQL
CREATE FUNCTION get_user_orders(p_user_id INTEGER, p_status VARCHAR DEFAULT NULL)
RETURNS TABLE(order_id INTEGER, total NUMERIC) AS $$
BEGIN
    RETURN QUERY
    SELECT id, total_amount
    FROM orders
    WHERE user_id = p_user_id
    AND (p_status IS NULL OR status = p_status);
END;
$$ LANGUAGE plpgsql;

-- ScratchBird SBLR
CREATE FUNCTION get_user_orders(
    p_user_id INTEGER,
    p_status VARCHAR DEFAULT NULL
)
RETURNS TABLE(order_id INTEGER, total NUMERIC)
LANGUAGE SBLR AS $$
BEGIN
    RETURN QUERY
    SELECT id, total_amount
    FROM orders
    WHERE user_id = p_user_id
    AND (p_status IS NULL OR status = p_status);
END;
$$;
```

**Procedure (PostgreSQL 11+):**

```sql
-- PostgreSQL PROCEDURE
CREATE PROCEDURE transfer_funds(
    p_from_account INTEGER,
    p_to_account INTEGER,
    p_amount NUMERIC
) AS $$
BEGIN
    UPDATE accounts SET balance = balance - p_amount WHERE id = p_from_account;
    UPDATE accounts SET balance = balance + p_amount WHERE id = p_to_account;
    COMMIT;
END;
$$ LANGUAGE plpgsql;

-- ScratchBird PROCEDURE
CREATE PROCEDURE transfer_funds(
    p_from_account INTEGER,
    p_to_account INTEGER,
    p_amount NUMERIC
)
LANGUAGE SBLR AS $$
BEGIN
    UPDATE accounts SET balance = balance - p_amount WHERE id = p_from_account;
    UPDATE accounts SET balance = balance + p_amount WHERE id = p_to_account;
    COMMIT;
END;
$$;
```

### Exception Handling

```sql
-- PostgreSQL
CREATE FUNCTION safe_divide(a NUMERIC, b NUMERIC) RETURNS NUMERIC AS $$
BEGIN
    RETURN a / b;
EXCEPTION
    WHEN division_by_zero THEN
        RETURN NULL;
    WHEN OTHERS THEN
        RAISE NOTICE 'Error: %', SQLERRM;
        RETURN NULL;
END;
$$ LANGUAGE plpgsql;

-- ScratchBird SBLR
CREATE FUNCTION safe_divide(a NUMERIC, b NUMERIC) RETURNS NUMERIC
LANGUAGE SBLR AS $$
BEGIN
    RETURN a / b;
EXCEPTION
    WHEN division_by_zero THEN
        RETURN NULL;
    WHEN OTHERS THEN
        RAISE NOTICE 'Error: %', SQLERRM;
        RETURN NULL;
END;
$$;
```

### Triggers

```sql
-- PostgreSQL trigger function
CREATE FUNCTION update_modified_timestamp() RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER set_timestamp
BEFORE UPDATE ON users
FOR EACH ROW
EXECUTE FUNCTION update_modified_timestamp();

-- ScratchBird (similar syntax)
CREATE FUNCTION update_modified_timestamp() RETURNS TRIGGER
LANGUAGE SBLR AS $$
BEGIN
    NEW.updated_at := CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$;

CREATE TRIGGER set_timestamp
BEFORE UPDATE ON users
FOR EACH ROW
EXECUTE FUNCTION update_modified_timestamp();
```

---

## Part 5: Data Migration

### Method 1: pg_dump with psql (Recommended)

**Export from PostgreSQL:**

```bash
# Full database dump
pg_dump -h source-host -U user -d mydb -F p -f full_dump.sql

# Data only (if schema already migrated)
pg_dump -h source-host -U user -d mydb --data-only -F p -f data.sql

# Specific tables
pg_dump -h source-host -U user -d mydb -t orders -t customers -F p -f tables.sql

# With insert statements (slower but more compatible)
pg_dump -h source-host -U user -d mydb --inserts -f data_inserts.sql
```

**Import to ScratchBird:**

```bash
# Via PostgreSQL protocol
psql -h scratchbird-host -p 5432 -U admin -d mydb -f full_dump.sql

# With error handling
psql -h scratchbird-host -p 5432 -U admin -d mydb \
    --set ON_ERROR_STOP=on \
    -f full_dump.sql
```

### Method 2: COPY Command

**Fast bulk loading:**

```bash
# Export from PostgreSQL
psql -h source-host -U user -d mydb -c \
    "COPY customers TO STDOUT WITH CSV HEADER" > customers.csv

# Import to ScratchBird
psql -h scratchbird-host -p 5432 -U admin -d mydb -c \
    "COPY customers FROM STDIN WITH CSV HEADER" < customers.csv
```

**Direct transfer (if both accessible):**

```bash
# Pipe between databases
psql -h source -U user -d mydb -c "COPY orders TO STDOUT" | \
psql -h scratchbird -p 5432 -U admin -d mydb -c "COPY orders FROM STDIN"
```

### Method 3: pg_dump Custom Format

**For large databases:**

```bash
# Export with compression
pg_dump -h source-host -U user -d mydb -F c -f backup.dump

# Restore to ScratchBird
# Note: sb_restore can handle pg_dump custom format
sb_restore --from-postgresql backup.dump -U admin -d mydb

# Or use pg_restore if compatible
pg_restore -h scratchbird-host -p 5432 -U admin -d mydb backup.dump
```

### Method 4: Parallel Data Transfer

**Using parallel jobs:**

```bash
# Export with parallel jobs
pg_dump -h source-host -U user -d mydb -F d -j 4 -f backup_dir/

# Import with parallel jobs
pg_restore -h scratchbird-host -p 5432 -U admin -d mydb -j 4 backup_dir/
```

**Python script for large tables:**

```python
import psycopg2
from concurrent.futures import ThreadPoolExecutor

source_conn = psycopg2.connect(host='source', database='mydb', user='user')
target_conn = psycopg2.connect(host='scratchbird', port=5432, database='mydb', user='admin')

def migrate_table(table_name):
    src_cur = source_conn.cursor()
    tgt_cur = target_conn.cursor()

    # Get row count for batching
    src_cur.execute(f"SELECT COUNT(*) FROM {table_name}")
    total_rows = src_cur.fetchone()[0]

    batch_size = 10000
    for offset in range(0, total_rows, batch_size):
        src_cur.execute(f"""
            SELECT * FROM {table_name}
            ORDER BY id
            LIMIT {batch_size} OFFSET {offset}
        """)
        rows = src_cur.fetchall()

        # Get column names
        cols = [desc[0] for desc in src_cur.description]
        placeholders = ','.join(['%s'] * len(cols))

        tgt_cur.executemany(
            f"INSERT INTO {table_name} ({','.join(cols)}) VALUES ({placeholders})",
            rows
        )
        target_conn.commit()
        print(f"{table_name}: {offset + len(rows)}/{total_rows}")

# Migrate tables in parallel
tables = ['customers', 'orders', 'products', 'order_items']
with ThreadPoolExecutor(max_workers=4) as executor:
    executor.map(migrate_table, tables)
```

---

## Part 6: System Catalog Migration

### pg_catalog Considerations

ScratchBird emulates common pg_catalog views, but not all:

**Supported views:**

```sql
-- These work in ScratchBird
SELECT * FROM pg_catalog.pg_tables;
SELECT * FROM pg_catalog.pg_indexes;
SELECT * FROM pg_catalog.pg_views;
SELECT * FROM pg_catalog.pg_columns;
SELECT * FROM pg_stat_activity;
SELECT * FROM pg_stat_database;
```

**May need adjustment:**

```sql
-- Some system functions may differ
-- Before (PostgreSQL)
SELECT pg_relation_size('orders');
SELECT pg_total_relation_size('orders');

-- ScratchBird equivalent (check function availability)
SELECT pg_relation_size('orders');  -- Usually works
```

### Information Schema

```sql
-- information_schema is generally compatible
SELECT table_name, column_name, data_type
FROM information_schema.columns
WHERE table_schema = 'public';

SELECT constraint_name, table_name, constraint_type
FROM information_schema.table_constraints
WHERE table_schema = 'public';
```

---

## Part 7: Application Changes

### Connection String Changes

**psycopg2 (Python):**

```python
# Before (PostgreSQL)
conn = psycopg2.connect(
    host='postgres-server',
    port=5432,
    database='mydb',
    user='appuser',
    password='secret'
)

# After (ScratchBird - just change host)
conn = psycopg2.connect(
    host='scratchbird-server',
    port=5432,
    database='mydb',
    user='appuser',
    password='secret'
)
```

**node-postgres (Node.js):**

```javascript
// Before
const pool = new Pool({
  host: 'postgres-server',
  port: 5432,
  database: 'mydb',
  user: 'appuser',
  password: 'secret'
});

// After (just change host)
const pool = new Pool({
  host: 'scratchbird-server',
  port: 5432,
  database: 'mydb',
  user: 'appuser',
  password: 'secret'
});
```

**JDBC (Java):**

```java
// Before
String url = "jdbc:postgresql://postgres-server:5432/mydb";

// After (just change host)
String url = "jdbc:postgresql://scratchbird-server:5432/mydb";
```

**Django:**

```python
# settings.py
DATABASES = {
    'default': {
        'ENGINE': 'django.db.backends.postgresql',
        'HOST': 'scratchbird-server',  # Changed from postgres-server
        'PORT': '5432',
        'NAME': 'mydb',
        'USER': 'appuser',
        'PASSWORD': 'secret',
    }
}
```

**Rails:**

```yaml
# database.yml
production:
  adapter: postgresql
  host: scratchbird-server  # Changed from postgres-server
  port: 5432
  database: mydb
  username: appuser
  password: secret
```

### Code Changes

**1. Replace unsupported functions:**

```python
# Before - using PostgreSQL-specific function
cursor.execute("SELECT uuid_generate_v4()")

# After - use standard function
cursor.execute("SELECT gen_random_uuid()")
```

**2. Handle LISTEN/NOTIFY differences:**

```python
# PostgreSQL LISTEN/NOTIFY works, but check for differences
# ScratchBird uses native event system internally

# Connection setup for notifications
conn.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
cursor.execute("LISTEN order_created")

# Receiving (same as PostgreSQL)
conn.poll()
while conn.notifies:
    notify = conn.notifies.pop(0)
    print(f"Got notification: {notify.payload}")
```

**3. Advisory locks:**

```sql
-- Advisory locks are supported but check behavior
SELECT pg_advisory_lock(123);
-- Do work...
SELECT pg_advisory_unlock(123);
```

---

## Part 8: Testing and Validation

### Schema Validation

```sql
-- Compare table counts
SELECT
    (SELECT COUNT(*) FROM pg_tables WHERE schemaname = 'public') AS pg_tables,
    (SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = 'public') AS info_tables;

-- Compare column counts per table
SELECT table_name, COUNT(*) as column_count
FROM information_schema.columns
WHERE table_schema = 'public'
GROUP BY table_name
ORDER BY table_name;
```

### Data Validation

```sql
-- Row counts
SELECT 'customers' AS table_name, COUNT(*) FROM customers
UNION ALL SELECT 'orders', COUNT(*) FROM orders
UNION ALL SELECT 'products', COUNT(*) FROM products;

-- Checksums (compare between source and target)
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
-- Test functions
SELECT get_user_count();
SELECT * FROM get_user_orders(123, 'active');

-- Test triggers
UPDATE users SET name = 'Test' WHERE id = 1;
SELECT updated_at FROM users WHERE id = 1;  -- Should be updated

-- Test transactions
BEGIN;
INSERT INTO orders (customer_id, total) VALUES (1, 100.00);
SAVEPOINT sp1;
INSERT INTO order_items (order_id, product_id) VALUES (currval('orders_id_seq'), 1);
ROLLBACK TO SAVEPOINT sp1;
COMMIT;
```

---

## Part 9: Performance Optimization

### Index Recreation

```sql
-- Rebuild all indexes after migration
REINDEX DATABASE mydb;

-- Or specific indexes
REINDEX INDEX idx_customers_email;
REINDEX TABLE orders;
```

### Statistics Update

```sql
-- Update statistics for query optimizer
ANALYZE;

-- Or specific tables
ANALYZE customers;
ANALYZE orders;

-- Verbose analysis
ANALYZE VERBOSE customers;
```

### Query Performance

```sql
-- Check query plans
EXPLAIN (ANALYZE, BUFFERS) SELECT * FROM orders WHERE customer_id = 123;

-- Enable timing
\timing on

-- Compare query performance between PostgreSQL and ScratchBird
```

### Configuration Tuning

```ini
# sb_server.conf

[memory]
buffer_pool_size = 256MB          # Similar to PostgreSQL
work_mem = 4MB                  # Per-operation memory
maintenance_work_mem = 64MB     # For VACUUM, CREATE INDEX

[wal]
wal_buffers = 16MB
checkpoint_completion_target = 0.9

[query]
effective_cache_size = 1GB
random_page_cost = 1.1          # For SSD storage
```

---

## Part 10: Quick Reference

### Command Mapping

| PostgreSQL | ScratchBird |
|------------|-------------|
| `psql` | `psql` (via port 5432) or `sb_isql` |
| `pg_dump` | `pg_dump` or `sb_backup` |
| `pg_restore` | `pg_restore` or `sb_restore` |
| `createdb` | `createdb` or `sb_admin --create` |
| `dropdb` | `dropdb` or `sb_admin --drop` |
| `pg_ctl` | `systemctl` or `sb_server` |

### Feature Availability

| Feature | PostgreSQL | ScratchBird | Notes |
|---------|------------|-------------|-------|
| JSONB | Full | Full | Same operators |
| Arrays | Full | Full | Same syntax |
| CTEs | Full | Full | Including RECURSIVE |
| Window Functions | Full | Full | All standard functions |
| UPSERT | Full | Full | ON CONFLICT clause |
| Partitioning | Declarative | Declarative | Range, list, hash |
| Full-Text Search | tsvector | Limited | Use JSONB alternatives |
| PostGIS | Extension | Not available | External GIS needed |
| Replication | Streaming/Logical | Native | Different setup |

### Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| Function not found | PL/pgSQL not converted | Convert to SBLR |
| Extension missing | Extension not available | Use alternative or remove |
| Type not found | Custom type | Use JSONB or table |
| Tablespace error | Tablespaces not supported | Remove tablespace clause |
| FDW error | Foreign data wrappers | Remove or use application-level |

---

## Migration Checklist

- [ ] Analyze PostgreSQL database (extensions, custom types, functions)
- [ ] Export schema with pg_dump --schema-only
- [ ] Review and transform schema
- [ ] Remove unsupported features (tablespaces, FDW, etc.)
- [ ] Convert PL/pgSQL functions to SBLR
- [ ] Create database in ScratchBird
- [ ] Import schema
- [ ] Test schema with sample queries
- [ ] Export data from PostgreSQL
- [ ] Import data to ScratchBird
- [ ] Validate row counts
- [ ] Validate data checksums
- [ ] Test functions and triggers
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
- [PostgreSQL SQL Reference](../language-guides/postgresql/)
- [Troubleshooting](../troubleshooting/)

