# Data Migration Project

**Last Updated:** 2026-01-28

---

## Overview

This tutorial walks through a complete data migration project, moving a production database from PostgreSQL to ScratchBird. You'll learn assessment, schema mapping, data transfer, validation, and cutover strategies applicable to any source database.

**What you'll learn:**
- Migration planning and assessment
- Schema and type mapping
- Data export and bulk loading
- Validation and verification
- Zero-downtime cutover strategies

**Scenario:** Migrating a 50GB e-commerce database with users, products, orders, and inventory tables from PostgreSQL to ScratchBird.

---

## Part 1: Assessment and Planning

### Inventory Source Schema

First, analyze your source database structure.

**Connect to source PostgreSQL:**
```bash
psql -h source-server -U admin -d ecommerce
```

**Get table list with sizes:**
```sql
SELECT
    schemaname,
    tablename,
    pg_size_pretty(pg_total_relation_size(schemaname || '.' || tablename)) AS total_size,
    n_live_tup AS row_count
FROM pg_stat_user_tables
ORDER BY pg_total_relation_size(schemaname || '.' || tablename) DESC;
```

Example output:
```
 schemaname |    tablename    | total_size | row_count
------------+-----------------+------------+-----------
 public     | order_items     | 18 GB      | 45000000
 public     | orders          | 12 GB      | 12000000
 public     | products        | 8 GB       | 500000
 public     | inventory_log   | 6 GB       | 30000000
 public     | users           | 3 GB       | 2000000
 public     | categories      | 50 MB      | 5000
 public     | shipping_zones  | 10 MB      | 500
```

**Export schema definition:**
```bash
pg_dump -h source-server -U admin -d ecommerce --schema-only > schema.sql
```

**Document constraints and indexes:**
```sql
-- Foreign keys
SELECT
    tc.table_name,
    kcu.column_name,
    ccu.table_name AS foreign_table,
    ccu.column_name AS foreign_column
FROM information_schema.table_constraints tc
JOIN information_schema.key_column_usage kcu
    ON tc.constraint_name = kcu.constraint_name
JOIN information_schema.constraint_column_usage ccu
    ON ccu.constraint_name = tc.constraint_name
WHERE tc.constraint_type = 'FOREIGN KEY';

-- Indexes
SELECT
    tablename,
    indexname,
    indexdef
FROM pg_indexes
WHERE schemaname = 'public';
```

### Create Migration Plan Document

```markdown
# E-Commerce Migration Plan

## Source Database
- PostgreSQL 15 on AWS RDS
- Size: 50 GB
- Tables: 7
- Total rows: ~90 million

## Migration Window
- Start: Saturday 2:00 AM
- Maximum downtime: 4 hours
- Rollback deadline: Sunday 6:00 AM

## Table Migration Order
1. categories (no dependencies)
2. shipping_zones (no dependencies)
3. users (no dependencies)
4. products (depends on categories)
5. orders (depends on users, shipping_zones)
6. order_items (depends on orders, products)
7. inventory_log (depends on products)

## Risk Assessment
- Large tables (order_items): Use parallel loading
- Foreign keys: Load in dependency order
- Sequences: Reset after migration
```

---

## Part 2: Schema Mapping

### Type Mapping Reference

| PostgreSQL | ScratchBird | Notes |
|------------|-------------|-------|
| `SERIAL` | `SERIAL` | Direct mapping |
| `BIGSERIAL` | `BIGSERIAL` | Direct mapping |
| `INTEGER` | `INTEGER` | Direct mapping |
| `BIGINT` | `BIGINT` | Direct mapping |
| `NUMERIC(p,s)` | `NUMERIC(p,s)` | Direct mapping |
| `VARCHAR(n)` | `VARCHAR(n)` | Direct mapping |
| `TEXT` | `TEXT` | Direct mapping |
| `BOOLEAN` | `BOOLEAN` | Direct mapping |
| `TIMESTAMP` | `TIMESTAMP` | Direct mapping |
| `TIMESTAMPTZ` | `TIMESTAMP WITH TIME ZONE` | Direct mapping |
| `DATE` | `DATE` | Direct mapping |
| `JSON` | `JSON` | Direct mapping |
| `JSONB` | `JSONB` | Direct mapping |
| `UUID` | `UUID` | Direct mapping |
| `BYTEA` | `BLOB` | Name differs |
| `ARRAY[]` | `ARRAY[]` | Direct mapping |
| `INET` | `INET` | Direct mapping |
| `CIDR` | `CIDR` | Direct mapping |

### Convert Schema

**Original PostgreSQL schema (excerpt):**
```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMPTZ,
    metadata JSONB DEFAULT '{}'
);

CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    sku VARCHAR(50) UNIQUE NOT NULL,
    name VARCHAR(255) NOT NULL,
    description TEXT,
    price NUMERIC(10, 2) NOT NULL,
    category_id INTEGER REFERENCES categories(id),
    attributes JSONB DEFAULT '{}',
    images TEXT[],
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE orders (
    id BIGSERIAL PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    status VARCHAR(20) DEFAULT 'pending',
    total NUMERIC(12, 2) NOT NULL,
    shipping_address JSONB NOT NULL,
    shipping_zone_id INTEGER REFERENCES shipping_zones(id),
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
    shipped_at TIMESTAMPTZ,
    delivered_at TIMESTAMPTZ
);
```

**ScratchBird schema (compatible):**
```sql
-- Connect to ScratchBird
sb_isql -U admin -d ecommerce -p 3092

-- Create tables (schema is compatible)
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP WITH TIME ZONE,
    metadata JSONB DEFAULT '{}'
);

CREATE TABLE categories (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    parent_id INTEGER REFERENCES categories(id),
    slug VARCHAR(100) UNIQUE NOT NULL
);

CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    sku VARCHAR(50) UNIQUE NOT NULL,
    name VARCHAR(255) NOT NULL,
    description TEXT,
    price NUMERIC(10, 2) NOT NULL,
    category_id INTEGER REFERENCES categories(id),
    attributes JSONB DEFAULT '{}',
    images TEXT[],
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE shipping_zones (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    countries TEXT[] NOT NULL,
    base_rate NUMERIC(8, 2) NOT NULL
);

CREATE TABLE orders (
    id BIGSERIAL PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    status VARCHAR(20) DEFAULT 'pending',
    total NUMERIC(12, 2) NOT NULL,
    shipping_address JSONB NOT NULL,
    shipping_zone_id INTEGER REFERENCES shipping_zones(id),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    shipped_at TIMESTAMP WITH TIME ZONE,
    delivered_at TIMESTAMP WITH TIME ZONE
);

CREATE TABLE order_items (
    id BIGSERIAL PRIMARY KEY,
    order_id BIGINT NOT NULL REFERENCES orders(id),
    product_id INTEGER NOT NULL REFERENCES products(id),
    quantity INTEGER NOT NULL,
    unit_price NUMERIC(10, 2) NOT NULL,
    total_price NUMERIC(12, 2) NOT NULL
);

CREATE TABLE inventory_log (
    id BIGSERIAL PRIMARY KEY,
    product_id INTEGER NOT NULL REFERENCES products(id),
    change_type VARCHAR(20) NOT NULL,
    quantity_change INTEGER NOT NULL,
    reason TEXT,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
```

### Handle Special Cases

**PostgreSQL-specific features to convert:**

```sql
-- PostgreSQL: Partial unique index
CREATE UNIQUE INDEX idx_users_email_active
ON users(email) WHERE deleted_at IS NULL;

-- ScratchBird: Same syntax supported
CREATE UNIQUE INDEX idx_users_email_active
ON users(email) WHERE deleted_at IS NULL;

-- PostgreSQL: Generated column
ALTER TABLE products ADD COLUMN
    search_vector TSVECTOR GENERATED ALWAYS AS (
        to_tsvector('english', name || ' ' || COALESCE(description, ''))
    ) STORED;

-- ScratchBird: Same syntax supported
ALTER TABLE products ADD COLUMN
    search_vector TSVECTOR GENERATED ALWAYS AS (
        to_tsvector('english', name || ' ' || COALESCE(description, ''))
    ) STORED;
```

---

## Part 3: Data Export

### Export Methods

**Method 1: CSV Export (Recommended for large tables)**

```bash
# Export each table to CSV
psql -h source-server -U admin -d ecommerce -c "\copy users TO '/tmp/users.csv' WITH CSV HEADER"
psql -h source-server -U admin -d ecommerce -c "\copy categories TO '/tmp/categories.csv' WITH CSV HEADER"
psql -h source-server -U admin -d ecommerce -c "\copy products TO '/tmp/products.csv' WITH CSV HEADER"
psql -h source-server -U admin -d ecommerce -c "\copy shipping_zones TO '/tmp/shipping_zones.csv' WITH CSV HEADER"
psql -h source-server -U admin -d ecommerce -c "\copy orders TO '/tmp/orders.csv' WITH CSV HEADER"
psql -h source-server -U admin -d ecommerce -c "\copy order_items TO '/tmp/order_items.csv' WITH CSV HEADER"
psql -h source-server -U admin -d ecommerce -c "\copy inventory_log TO '/tmp/inventory_log.csv' WITH CSV HEADER"
```

**Method 2: pg_dump for smaller tables**

```bash
# Export data only (no schema)
pg_dump -h source-server -U admin -d ecommerce \
    --data-only \
    --table=categories \
    --table=shipping_zones \
    > small_tables.sql
```

**Method 3: Parallel export for large tables**

Create `export_parallel.sh`:
```bash
#!/bin/bash
TABLE=$1
CHUNKS=$2
OUTPUT_DIR=$3

# Get row count
COUNT=$(psql -h source-server -U admin -d ecommerce -t -c "SELECT COUNT(*) FROM $TABLE")
CHUNK_SIZE=$((COUNT / CHUNKS + 1))

for i in $(seq 0 $((CHUNKS - 1))); do
    OFFSET=$((i * CHUNK_SIZE))
    psql -h source-server -U admin -d ecommerce -c \
        "\copy (SELECT * FROM $TABLE ORDER BY id OFFSET $OFFSET LIMIT $CHUNK_SIZE) TO '$OUTPUT_DIR/${TABLE}_${i}.csv' WITH CSV" &
done
wait
echo "Export complete: $TABLE"
```

Run parallel export:
```bash
chmod +x export_parallel.sh
./export_parallel.sh order_items 10 /tmp/exports
./export_parallel.sh inventory_log 8 /tmp/exports
```

### Compress Exports

```bash
# Compress all CSV files
cd /tmp
for f in *.csv; do
    gzip -9 "$f" &
done
wait

# Transfer to target server
rsync -avz --progress /tmp/*.csv.gz target-server:/tmp/migration/
```

---

## Part 4: Data Loading

### Prepare Target Database

```bash
# Connect to ScratchBird
sb_isql -U admin -d ecommerce -p 3092
```

```sql
-- Disable foreign key checks for bulk load
SET session_replication_role = 'replica';

-- Increase work memory for sorting
SET work_mem = '256MB';

-- Disable auto-vacuum during load
ALTER TABLE users SET (autovacuum_enabled = false);
ALTER TABLE products SET (autovacuum_enabled = false);
ALTER TABLE orders SET (autovacuum_enabled = false);
ALTER TABLE order_items SET (autovacuum_enabled = false);
ALTER TABLE inventory_log SET (autovacuum_enabled = false);
```

### Load Data with COPY

**Load in dependency order:**

```bash
# Decompress on target
cd /tmp/migration
gunzip *.csv.gz

# Load independent tables first
sb_isql -U admin -d ecommerce -p 3092 -c "\copy categories FROM '/tmp/migration/categories.csv' WITH CSV HEADER"
sb_isql -U admin -d ecommerce -p 3092 -c "\copy shipping_zones FROM '/tmp/migration/shipping_zones.csv' WITH CSV HEADER"
sb_isql -U admin -d ecommerce -p 3092 -c "\copy users FROM '/tmp/migration/users.csv' WITH CSV HEADER"

# Load dependent tables
sb_isql -U admin -d ecommerce -p 3092 -c "\copy products FROM '/tmp/migration/products.csv' WITH CSV HEADER"
sb_isql -U admin -d ecommerce -p 3092 -c "\copy orders FROM '/tmp/migration/orders.csv' WITH CSV HEADER"
sb_isql -U admin -d ecommerce -p 3092 -c "\copy order_items FROM '/tmp/migration/order_items.csv' WITH CSV HEADER"
sb_isql -U admin -d ecommerce -p 3092 -c "\copy inventory_log FROM '/tmp/migration/inventory_log.csv' WITH CSV HEADER"
```

### Parallel Loading Script

Create `load_parallel.py`:
```python
#!/usr/bin/env python3
"""Parallel data loader for ScratchBird migration."""

import subprocess
import sys
import os
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

DB_HOST = "localhost"
DB_PORT = "3092"
DB_USER = "admin"
DB_NAME = "ecommerce"

def load_file(csv_path: Path) -> tuple[str, bool, str]:
    """Load a single CSV file into ScratchBird."""
    table_name = csv_path.stem.split('_')[0]  # Handle chunked files

    cmd = [
        "sb_isql",
        "-H", DB_HOST,
        "-p", DB_PORT,
        "-U", DB_USER,
        "-d", DB_NAME,
        "-c", f"\\copy {table_name} FROM '{csv_path}' WITH CSV HEADER"
    ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)
        if result.returncode == 0:
            return (str(csv_path), True, "OK")
        else:
            return (str(csv_path), False, result.stderr)
    except Exception as e:
        return (str(csv_path), False, str(e))

def main():
    if len(sys.argv) < 2:
        print("Usage: load_parallel.py <directory> [workers]")
        sys.exit(1)

    directory = Path(sys.argv[1])
    workers = int(sys.argv[2]) if len(sys.argv) > 2 else 4

    csv_files = sorted(directory.glob("*.csv"))
    print(f"Found {len(csv_files)} CSV files to load")

    results = {"success": 0, "failed": 0}

    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = {executor.submit(load_file, f): f for f in csv_files}

        for future in as_completed(futures):
            path, success, message = future.result()
            if success:
                results["success"] += 1
                print(f"[OK] {path}")
            else:
                results["failed"] += 1
                print(f"[FAIL] {path}: {message}")

    print(f"\nComplete: {results['success']} succeeded, {results['failed']} failed")
    sys.exit(0 if results["failed"] == 0 else 1)

if __name__ == "__main__":
    main()
```

Run parallel load:
```bash
python3 load_parallel.py /tmp/migration 8
```

### Reset Sequences

After loading data, reset sequences to continue from the highest ID:

```sql
-- Reset all sequences
SELECT setval('users_id_seq', (SELECT MAX(id) FROM users));
SELECT setval('categories_id_seq', (SELECT MAX(id) FROM categories));
SELECT setval('products_id_seq', (SELECT MAX(id) FROM products));
SELECT setval('shipping_zones_id_seq', (SELECT MAX(id) FROM shipping_zones));
SELECT setval('orders_id_seq', (SELECT MAX(id) FROM orders));
SELECT setval('order_items_id_seq', (SELECT MAX(id) FROM order_items));
SELECT setval('inventory_log_id_seq', (SELECT MAX(id) FROM inventory_log));
```

### Re-enable Constraints

```sql
-- Re-enable foreign key checks
SET session_replication_role = 'origin';

-- Re-enable auto-vacuum
ALTER TABLE users SET (autovacuum_enabled = true);
ALTER TABLE products SET (autovacuum_enabled = true);
ALTER TABLE orders SET (autovacuum_enabled = true);
ALTER TABLE order_items SET (autovacuum_enabled = true);
ALTER TABLE inventory_log SET (autovacuum_enabled = true);

-- Run initial vacuum and analyze
VACUUM ANALYZE users;
VACUUM ANALYZE categories;
VACUUM ANALYZE products;
VACUUM ANALYZE shipping_zones;
VACUUM ANALYZE orders;
VACUUM ANALYZE order_items;
VACUUM ANALYZE inventory_log;
```

---

## Part 5: Create Indexes

Create indexes after data load for better performance:

```sql
-- Users indexes
CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_users_created_at ON users(created_at);

-- Products indexes
CREATE INDEX idx_products_category ON products(category_id);
CREATE INDEX idx_products_sku ON products(sku);
CREATE INDEX idx_products_price ON products(price);
CREATE INDEX idx_products_created_at ON products(created_at);

-- Orders indexes
CREATE INDEX idx_orders_user ON orders(user_id);
CREATE INDEX idx_orders_status ON orders(status);
CREATE INDEX idx_orders_created_at ON orders(created_at);
CREATE INDEX idx_orders_user_status ON orders(user_id, status);

-- Order items indexes
CREATE INDEX idx_order_items_order ON order_items(order_id);
CREATE INDEX idx_order_items_product ON order_items(product_id);

-- Inventory log indexes
CREATE INDEX idx_inventory_log_product ON inventory_log(product_id);
CREATE INDEX idx_inventory_log_created_at ON inventory_log(created_at);

-- Full-text search index
CREATE INDEX idx_products_search ON products USING GIN(search_vector);
```

---

## Part 6: Validation

### Row Count Verification

Create `validate_counts.sql`:
```sql
-- Compare row counts
WITH source_counts AS (
    SELECT 'users' AS table_name, 2000000 AS source_count
    UNION ALL SELECT 'categories', 5000
    UNION ALL SELECT 'products', 500000
    UNION ALL SELECT 'shipping_zones', 500
    UNION ALL SELECT 'orders', 12000000
    UNION ALL SELECT 'order_items', 45000000
    UNION ALL SELECT 'inventory_log', 30000000
),
target_counts AS (
    SELECT 'users' AS table_name, COUNT(*) AS target_count FROM users
    UNION ALL SELECT 'categories', COUNT(*) FROM categories
    UNION ALL SELECT 'products', COUNT(*) FROM products
    UNION ALL SELECT 'shipping_zones', COUNT(*) FROM shipping_zones
    UNION ALL SELECT 'orders', COUNT(*) FROM orders
    UNION ALL SELECT 'order_items', COUNT(*) FROM order_items
    UNION ALL SELECT 'inventory_log', COUNT(*) FROM inventory_log
)
SELECT
    s.table_name,
    s.source_count,
    t.target_count,
    CASE
        WHEN s.source_count = t.target_count THEN 'MATCH'
        ELSE 'MISMATCH'
    END AS status
FROM source_counts s
JOIN target_counts t ON s.table_name = t.table_name
ORDER BY s.table_name;
```

Expected output:
```
  table_name   | source_count | target_count |  status
---------------+--------------+--------------+---------
 categories    |         5000 |         5000 | MATCH
 inventory_log |     30000000 |     30000000 | MATCH
 order_items   |     45000000 |     45000000 | MATCH
 orders        |     12000000 |     12000000 | MATCH
 products      |       500000 |       500000 | MATCH
 shipping_zones|          500 |          500 | MATCH
 users         |      2000000 |      2000000 | MATCH
```

### Checksum Verification

For critical tables, verify data integrity with checksums:

**On source PostgreSQL:**
```sql
-- Generate checksum for users table
SELECT MD5(STRING_AGG(
    id::text || email || password_hash || COALESCE(full_name, ''),
    '' ORDER BY id
)) AS checksum
FROM users
WHERE id BETWEEN 1 AND 10000;
```

**On target ScratchBird:**
```sql
-- Same query on target
SELECT MD5(STRING_AGG(
    id::text || email || password_hash || COALESCE(full_name, ''),
    '' ORDER BY id
)) AS checksum
FROM users
WHERE id BETWEEN 1 AND 10000;
```

### Spot Check Queries

Verify specific records match:

```sql
-- Check specific user
SELECT id, email, full_name, created_at
FROM users
WHERE id = 12345;

-- Check order totals
SELECT
    o.id,
    o.total AS order_total,
    SUM(oi.total_price) AS calculated_total
FROM orders o
JOIN order_items oi ON o.id = oi.order_id
WHERE o.id IN (1000, 5000, 10000, 50000)
GROUP BY o.id, o.total;

-- Verify foreign key relationships
SELECT COUNT(*) AS orphaned_items
FROM order_items oi
LEFT JOIN orders o ON oi.order_id = o.id
WHERE o.id IS NULL;  -- Should be 0
```

### Functional Verification

Run application-level queries to verify functionality:

```sql
-- Test: Get user with recent orders
SELECT
    u.id,
    u.email,
    COUNT(o.id) AS order_count,
    SUM(o.total) AS total_spent
FROM users u
LEFT JOIN orders o ON u.id = o.user_id
WHERE u.id = 42
GROUP BY u.id, u.email;

-- Test: Product search
SELECT id, sku, name, price
FROM products
WHERE search_vector @@ to_tsquery('english', 'laptop & gaming')
ORDER BY price DESC
LIMIT 10;

-- Test: Order status report
SELECT
    status,
    COUNT(*) AS count,
    SUM(total) AS revenue
FROM orders
WHERE created_at >= CURRENT_DATE - INTERVAL '30 days'
GROUP BY status
ORDER BY count DESC;

-- Test: Inventory check
SELECT
    p.sku,
    p.name,
    SUM(il.quantity_change) AS current_stock
FROM products p
LEFT JOIN inventory_log il ON p.id = il.product_id
WHERE p.id IN (100, 200, 300)
GROUP BY p.id, p.sku, p.name;
```

---

## Part 7: Performance Testing

### Baseline Queries

Run standard queries and record execution times:

```sql
-- Enable timing
\timing on

-- Query 1: Simple lookup
SELECT * FROM users WHERE email = 'test@example.com';
-- Expected: < 5ms

-- Query 2: Join query
SELECT o.*, u.email
FROM orders o
JOIN users u ON o.user_id = u.id
WHERE o.id = 1000000;
-- Expected: < 10ms

-- Query 3: Aggregation
SELECT
    DATE_TRUNC('day', created_at) AS day,
    COUNT(*) AS orders,
    SUM(total) AS revenue
FROM orders
WHERE created_at >= CURRENT_DATE - INTERVAL '7 days'
GROUP BY DATE_TRUNC('day', created_at)
ORDER BY day;
-- Expected: < 500ms

-- Query 4: Full-text search
SELECT id, name, price
FROM products
WHERE search_vector @@ to_tsquery('english', 'wireless & headphones')
ORDER BY price
LIMIT 20;
-- Expected: < 100ms
```

### Load Testing

Create `load_test.py`:
```python
#!/usr/bin/env python3
"""Simple load test for ScratchBird migration validation."""

import psycopg2
from psycopg2.pool import ThreadedConnectionPool
import random
import time
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass

@dataclass
class TestResult:
    query_name: str
    success: int
    failed: int
    avg_time_ms: float
    max_time_ms: float

# Connection pool
pool = ThreadedConnectionPool(
    minconn=5,
    maxconn=50,
    host="localhost",
    port=5432,  # Using PostgreSQL protocol
    database="ecommerce",
    user="admin",
    password="password"
)

QUERIES = {
    "user_lookup": "SELECT * FROM users WHERE id = %s",
    "order_history": """
        SELECT o.id, o.status, o.total, o.created_at
        FROM orders o
        WHERE o.user_id = %s
        ORDER BY o.created_at DESC
        LIMIT 10
    """,
    "product_search": """
        SELECT id, name, price
        FROM products
        WHERE category_id = %s
        ORDER BY price
        LIMIT 20
    """,
}

def run_query(query_name: str, param: int) -> tuple[bool, float]:
    """Run a single query and return success status and time."""
    conn = pool.getconn()
    try:
        start = time.perf_counter()
        with conn.cursor() as cur:
            cur.execute(QUERIES[query_name], (param,))
            cur.fetchall()
        elapsed = (time.perf_counter() - start) * 1000
        return True, elapsed
    except Exception as e:
        print(f"Error in {query_name}: {e}")
        return False, 0
    finally:
        pool.putconn(conn)

def load_test(query_name: str, iterations: int, workers: int) -> TestResult:
    """Run load test for a specific query."""
    times = []
    success = 0
    failed = 0

    def run_iteration(_):
        param = random.randint(1, 100000)
        return run_query(query_name, param)

    with ThreadPoolExecutor(max_workers=workers) as executor:
        results = list(executor.map(run_iteration, range(iterations)))

    for ok, elapsed in results:
        if ok:
            success += 1
            times.append(elapsed)
        else:
            failed += 1

    return TestResult(
        query_name=query_name,
        success=success,
        failed=failed,
        avg_time_ms=sum(times) / len(times) if times else 0,
        max_time_ms=max(times) if times else 0
    )

def main():
    print("Starting load test...\n")

    for query_name in QUERIES:
        result = load_test(query_name, iterations=1000, workers=20)
        print(f"{result.query_name}:")
        print(f"  Success: {result.success}, Failed: {result.failed}")
        print(f"  Avg time: {result.avg_time_ms:.2f}ms")
        print(f"  Max time: {result.max_time_ms:.2f}ms")
        print()

    pool.closeall()

if __name__ == "__main__":
    main()
```

---

## Part 8: Cutover Strategy

### Option A: Maintenance Window (Simplest)

1. **Announce maintenance window**
2. **Stop application servers**
3. **Export final delta from source**
4. **Load delta into target**
5. **Update application connection strings**
6. **Start application servers**
7. **Verify functionality**

```bash
# 1. Stop application
systemctl stop myapp

# 2. Export changes since last sync (using timestamp)
LAST_SYNC="2026-01-18 02:00:00"
psql -h source -U admin -d ecommerce -c "\copy (
    SELECT * FROM orders WHERE created_at > '$LAST_SYNC'
) TO '/tmp/orders_delta.csv' WITH CSV HEADER"

# 3. Load delta
sb_isql -U admin -d ecommerce -c "\copy orders FROM '/tmp/orders_delta.csv' WITH CSV HEADER"

# 4. Update config and restart
sed -i 's/source-server/scratchbird-server/' /etc/myapp/database.conf
systemctl start myapp
```

### Option B: Blue-Green Deployment

```yaml
# docker-compose.yml for blue-green
version: '3.8'

services:
  app-blue:
    image: myapp:latest
    environment:
      DB_HOST: postgresql-source
      DB_PORT: 5432
    labels:
      - "traefik.http.routers.app.rule=Host(`myapp.com`)"
    profiles: ["blue"]

  app-green:
    image: myapp:latest
    environment:
      DB_HOST: scratchbird
      DB_PORT: 5432
    labels:
      - "traefik.http.routers.app.rule=Host(`myapp.com`)"
    profiles: ["green"]

  scratchbird:
    image: scratchbird/scratchbird:latest
    volumes:
      - scratchbird_data:/var/lib/scratchbird
```

Cutover process:
```bash
# 1. Start green (ScratchBird) alongside blue
docker compose --profile green up -d

# 2. Test green deployment
curl -H "Host: myapp.com" http://green-app:3000/health

# 3. Switch traffic to green
docker compose --profile blue down

# 4. Monitor and verify
docker compose logs -f app-green
```

### Option C: Gradual Migration with Read Replicas

For zero-downtime migration of read-heavy workloads:

1. **Set up ScratchBird as read replica target**
2. **Route read queries to ScratchBird**
3. **Monitor performance**
4. **Gradually increase read traffic**
5. **Finally switch writes**

```python
# Application-level routing
class DatabaseRouter:
    def __init__(self):
        self.write_pool = create_pool("postgresql-source", 5432)
        self.read_pool = create_pool("scratchbird", 5432)
        self.read_percent = 0  # Gradually increase

    def get_connection(self, is_write: bool):
        if is_write:
            return self.write_pool.getconn()

        # Route reads based on percentage
        if random.random() * 100 < self.read_percent:
            return self.read_pool.getconn()
        return self.write_pool.getconn()
```

---

## Part 9: Rollback Plan

### Prepare Rollback

Before cutover, ensure you can roll back:

```bash
# 1. Keep source database running (read-only)
psql -h source-server -U admin -d ecommerce -c "ALTER DATABASE ecommerce SET default_transaction_read_only = on"

# 2. Document connection strings
echo "Source: postgresql://admin@source-server:5432/ecommerce" > /tmp/rollback-config.txt
echo "Target: postgresql://admin@scratchbird:5432/ecommerce" >> /tmp/rollback-config.txt

# 3. Create rollback script
cat > /tmp/rollback.sh << 'EOF'
#!/bin/bash
echo "Rolling back to source database..."

# Update application config
sed -i 's/scratchbird/source-server/' /etc/myapp/database.conf

# Re-enable writes on source
psql -h source-server -U admin -d ecommerce -c "ALTER DATABASE ecommerce SET default_transaction_read_only = off"

# Restart application
systemctl restart myapp

echo "Rollback complete"
EOF
chmod +x /tmp/rollback.sh
```

### Rollback Triggers

Define conditions that trigger rollback:

- Error rate > 5% for 5 minutes
- Response time > 2x baseline for 10 minutes
- Any data corruption detected
- Application crashes
- User-reported critical issues

### Post-Rollback

If rollback is executed:

1. **Analyze failure cause**
2. **Export any new data from ScratchBird**
3. **Merge new data into source**
4. **Fix issues**
5. **Plan next migration attempt**

---

## Part 10: Post-Migration Tasks

### Monitoring Setup

```sql
-- Create monitoring views
CREATE VIEW migration_health AS
SELECT
    'connections' AS metric,
    COUNT(*) AS value
FROM pg_stat_activity
UNION ALL
SELECT
    'active_queries',
    COUNT(*)
FROM pg_stat_activity
WHERE state = 'active'
UNION ALL
SELECT
    'cache_hit_ratio',
    ROUND(
        SUM(heap_blks_hit) * 100.0 /
        NULLIF(SUM(heap_blks_hit) + SUM(heap_blks_read), 0),
        2
    )
FROM pg_statio_user_tables;
```

### Documentation Update

Update runbooks and documentation:

- Connection strings
- Backup procedures
- Monitoring dashboards
- On-call playbooks

### Decommission Source

After successful migration (typically 1-2 weeks):

1. **Final backup of source**
2. **Export any remaining audit data**
3. **Stop source database**
4. **Archive source data**
5. **Release source resources**

---

## Checklist

### Pre-Migration
- [ ] Schema exported and converted
- [ ] Data volumes assessed
- [ ] Migration window scheduled
- [ ] Rollback plan documented
- [ ] Team notified

### Migration
- [ ] Target schema created
- [ ] Data exported from source
- [ ] Data loaded to target
- [ ] Sequences reset
- [ ] Indexes created
- [ ] Constraints enabled

### Validation
- [ ] Row counts match
- [ ] Checksums verified
- [ ] Spot checks passed
- [ ] Functional tests passed
- [ ] Performance acceptable

### Cutover
- [ ] Application updated
- [ ] Traffic switched
- [ ] Monitoring active
- [ ] Rollback ready

### Post-Migration
- [ ] Documentation updated
- [ ] Monitoring configured
- [ ] Source archived
- [ ] Lessons learned documented

---

## See Also

- [Migration Overview](../migration/Migration-Overview.md)
- [From PostgreSQL](../migration/From-PostgreSQL.md)
- [From MySQL](../migration/From-MySQL.md)
- [From Firebird](../migration/From-Firebird.md)
- [Performance Tuning](../user-guides/Performance-Tuning.md)
- [Backup and Restore](../admin/backup-restore.md)

