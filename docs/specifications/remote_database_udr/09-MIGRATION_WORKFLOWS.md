# Remote Database UDR - Migration Workflows

> Reference-only: Canonical UDR and live-migration behavior now lives in
> `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
> and `ScratchBird/docs/specifications/Alpha Phase 2/11h-Live-Migration-Emulated-Listener.md`.

## 1. Overview

This document describes common migration patterns and workflows using the Remote Database UDR plugin to migrate from legacy databases to ScratchBird.

---

## 2. Migration Strategies

### 2.1 Strategy Comparison

| Strategy | Downtime | Complexity | Risk | Use Case |
|----------|----------|------------|------|----------|
| Big Bang | Hours-Days | Low | High | Small databases, weekend windows |
| Parallel Run | None | High | Low | Critical systems, risk-averse |
| Incremental | Minimal | Medium | Medium | Large databases, continuous ops |
| Strangler Fig | None | Medium | Low | Gradual feature migration |

---

## 3. Big Bang Migration

### 3.1 Process

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Legacy DB      │ ──► │  Data Transfer  │ ──► │  ScratchBird    │
│  (Read-only)    │     │  via FDW        │     │  (New Primary)  │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

### 3.2 Steps

```sql
-- 1. Create foreign server connection
CREATE SERVER legacy_db
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (host 'legacy.example.com', port '5432', dbname 'production');

CREATE USER MAPPING FOR migration_user
    SERVER legacy_db
    OPTIONS (user 'readonly', password 'migration_pwd');

-- 2. Import schema
IMPORT FOREIGN SCHEMA public
    FROM SERVER legacy_db
    INTO legacy_import;

-- 3. Create target tables (copy structure)
CREATE TABLE users AS
SELECT * FROM legacy_import.users WHERE false;

CREATE TABLE orders AS
SELECT * FROM legacy_import.orders WHERE false;

-- 4. Stop writes on legacy (application downtime starts)
-- SET legacy_db to read-only mode

-- 5. Transfer data
INSERT INTO users
SELECT * FROM legacy_import.users;

INSERT INTO orders
SELECT * FROM legacy_import.orders;

-- 6. Create indexes and constraints
CREATE INDEX idx_users_email ON users(email);
ALTER TABLE orders ADD FOREIGN KEY (user_id) REFERENCES users(id);

-- 7. Verify counts
SELECT
    (SELECT COUNT(*) FROM legacy_import.users) AS legacy_users,
    (SELECT COUNT(*) FROM users) AS new_users,
    (SELECT COUNT(*) FROM legacy_import.orders) AS legacy_orders,
    (SELECT COUNT(*) FROM orders) AS new_orders;

-- 8. Switch application to new database
-- (Application downtime ends)

-- 9. Cleanup
DROP FOREIGN TABLE legacy_import.users;
DROP FOREIGN TABLE legacy_import.orders;
DROP SERVER legacy_db CASCADE;
```

---

## 4. Incremental Migration

### 4.1 Process

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Legacy DB      │ ◄──►│  Both DBs       │ ──► │  ScratchBird    │
│  (Still Active) │     │  Synchronized   │     │  (Growing)      │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

### 4.2 Steps

```sql
-- 1. Setup foreign server (as before)
CREATE SERVER legacy_db
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (host 'legacy.example.com', port '5432', dbname 'production');

-- 2. Import schema with read-write access
IMPORT FOREIGN SCHEMA public
    FROM SERVER legacy_db
    INTO legacy_sync
    OPTIONS (updatable 'true');

-- 3. Create local tables with tracking columns
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username VARCHAR(255),
    email VARCHAR(255),
    created_at TIMESTAMP,
    -- Migration tracking
    migrated_at TIMESTAMP DEFAULT NULL,
    source_db VARCHAR(50) DEFAULT 'legacy'
);

-- 4. Initial bulk load (historical data)
INSERT INTO users (id, username, email, created_at, migrated_at, source_db)
SELECT id, username, email, created_at, CURRENT_TIMESTAMP, 'legacy'
FROM legacy_sync.users
WHERE created_at < '2024-01-01';  -- Older records

-- 5. Create sync job for ongoing changes
CREATE PROCEDURE sync_users_from_legacy()
AS $$
DECLARE
    last_sync TIMESTAMP;
    batch_size INTEGER := 1000;
BEGIN
    -- Get last sync time
    SELECT COALESCE(MAX(migrated_at), '1970-01-01')
    INTO last_sync
    FROM users
    WHERE source_db = 'legacy';

    -- Sync new/updated records
    INSERT INTO users (id, username, email, created_at, migrated_at, source_db)
    SELECT id, username, email, created_at, CURRENT_TIMESTAMP, 'legacy'
    FROM legacy_sync.users
    WHERE created_at > last_sync
       OR updated_at > last_sync
    ON CONFLICT (id) DO UPDATE SET
        username = EXCLUDED.username,
        email = EXCLUDED.email,
        migrated_at = CURRENT_TIMESTAMP;

    -- Log sync
    INSERT INTO migration_log (table_name, records_synced, sync_time)
    VALUES ('users', @@ROWCOUNT, CURRENT_TIMESTAMP);
END;
$$ LANGUAGE plpgsql;

-- 6. Schedule periodic sync (every 5 minutes)
CREATE SCHEDULE sync_legacy_users
    EVERY 5 MINUTES
    CALL sync_users_from_legacy();

-- 7. Gradually shift new writes to ScratchBird
-- (Application changes required)

-- 8. Final sync and cutover
CALL sync_users_from_legacy();
-- Verify counts match
-- Switch application to ScratchBird only

-- 9. Cleanup
DROP SCHEDULE sync_legacy_users;
DROP SERVER legacy_db CASCADE;
```

---

## 5. Parallel Run Migration

### 5.1 Architecture

```
                    ┌──────────────────┐
                    │  Application     │
                    │  (Dual Write)    │
                    └────────┬─────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
              ▼                             ▼
     ┌─────────────────┐          ┌─────────────────┐
     │  Legacy DB      │          │  ScratchBird    │
     │  (Primary)      │          │  (Shadow)       │
     └─────────────────┘          └─────────────────┘
              │                             │
              │          Compare            │
              └──────────────┬──────────────┘
                             ▼
                    ┌─────────────────┐
                    │  Validation     │
                    │  Service        │
                    └─────────────────┘
```

### 5.2 Validation Queries

```sql
-- Create validation view
CREATE VIEW migration_validation AS
SELECT
    'users' AS table_name,
    (SELECT COUNT(*) FROM legacy_sync.users) AS legacy_count,
    (SELECT COUNT(*) FROM users) AS local_count,
    (SELECT COUNT(*) FROM legacy_sync.users) -
        (SELECT COUNT(*) FROM users) AS count_diff,
    (SELECT MAX(updated_at) FROM legacy_sync.users) AS legacy_max_update,
    (SELECT MAX(migrated_at) FROM users) AS local_max_update
UNION ALL
SELECT
    'orders',
    (SELECT COUNT(*) FROM legacy_sync.orders),
    (SELECT COUNT(*) FROM orders),
    (SELECT COUNT(*) FROM legacy_sync.orders) - (SELECT COUNT(*) FROM orders),
    (SELECT MAX(order_date) FROM legacy_sync.orders),
    (SELECT MAX(migrated_at) FROM orders);

-- Check for data discrepancies
SELECT u1.id, u1.email AS legacy_email, u2.email AS local_email
FROM legacy_sync.users u1
JOIN users u2 ON u1.id = u2.id
WHERE u1.email != u2.email;

-- Identify missing records
SELECT id FROM legacy_sync.users
WHERE id NOT IN (SELECT id FROM users);
```

### 5.3 Conflict Resolution

```sql
-- When both databases have different values, use rules:
CREATE PROCEDURE resolve_conflicts()
AS $$
BEGIN
    -- Rule 1: Newer timestamp wins
    UPDATE users u
    SET
        username = l.username,
        email = l.email,
        migrated_at = CURRENT_TIMESTAMP
    FROM legacy_sync.users l
    WHERE u.id = l.id
      AND l.updated_at > u.migrated_at;

    -- Rule 2: Log conflicts that need manual review
    INSERT INTO conflict_log (table_name, record_id, legacy_value, local_value)
    SELECT 'users', u.id, l.email, u.email
    FROM users u
    JOIN legacy_sync.users l ON u.id = l.id
    WHERE u.email != l.email
      AND ABS(EXTRACT(EPOCH FROM l.updated_at - u.migrated_at)) < 60;
END;
$$ LANGUAGE plpgsql;
```

---

## 6. Strangler Fig Migration

### 6.1 Concept

Gradually replace legacy functionality by routing queries through ScratchBird, which either serves data locally or proxies to legacy.

### 6.2 Implementation

```sql
-- 1. Create view that unifies local and remote data
CREATE VIEW unified_users AS
SELECT id, username, email, created_at, 'local' AS data_source
FROM users  -- Local ScratchBird table
WHERE id >= 1000000  -- New records

UNION ALL

SELECT id, username, email, created_at, 'legacy' AS data_source
FROM legacy_sync.users  -- Foreign table
WHERE id < 1000000;  -- Old records

-- 2. Insert new data locally
INSERT INTO users (id, username, email, created_at)
SELECT NEXTVAL('user_id_seq'), 'newuser', 'new@example.com', CURRENT_TIMESTAMP;

-- 3. Application queries unified view
SELECT * FROM unified_users WHERE email = 'user@example.com';

-- 4. Gradually migrate old data
INSERT INTO users
SELECT * FROM legacy_sync.users
WHERE id BETWEEN 900000 AND 999999;

-- 5. Update view to reduce legacy range
CREATE OR REPLACE VIEW unified_users AS
SELECT id, username, email, created_at, 'local' AS data_source
FROM users
WHERE id >= 900000  -- Expanded local range

UNION ALL

SELECT id, username, email, created_at, 'legacy' AS data_source
FROM legacy_sync.users
WHERE id < 900000;  -- Reduced legacy range

-- 6. Eventually, view becomes fully local
CREATE OR REPLACE VIEW unified_users AS
SELECT id, username, email, created_at, 'local' AS data_source
FROM users;
```

---

## 7. Schema Migration Patterns

### 7.1 Column Type Changes

```sql
-- Legacy has VARCHAR(50), we want TEXT
CREATE FOREIGN TABLE legacy_products (
    id INTEGER,
    name VARCHAR(50),  -- Legacy constraint
    description VARCHAR(2000)
)
SERVER legacy_db
OPTIONS (schema_name 'public', table_name 'products');

-- Local table with improved types
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,  -- Improved type
    description TEXT
);

-- Migration with type conversion
INSERT INTO products (id, name, description)
SELECT id, name::TEXT, description::TEXT
FROM legacy_products;
```

### 7.2 Normalization

```sql
-- Legacy denormalized table
CREATE FOREIGN TABLE legacy_orders (
    order_id INTEGER,
    customer_name VARCHAR(100),  -- Denormalized
    customer_email VARCHAR(100),  -- Denormalized
    product_name VARCHAR(100),   -- Denormalized
    quantity INTEGER,
    price DECIMAL(10,2)
)
SERVER legacy_db;

-- New normalized structure
CREATE TABLE customers (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    email VARCHAR(100) UNIQUE
);

CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE orders (
    id SERIAL PRIMARY KEY,
    customer_id INTEGER REFERENCES customers(id),
    product_id INTEGER REFERENCES products(id),
    quantity INTEGER,
    price DECIMAL(10,2)
);

-- Migrate with normalization
INSERT INTO customers (name, email)
SELECT DISTINCT customer_name, customer_email
FROM legacy_orders;

INSERT INTO products (name)
SELECT DISTINCT product_name
FROM legacy_orders;

INSERT INTO orders (customer_id, product_id, quantity, price)
SELECT c.id, p.id, lo.quantity, lo.price
FROM legacy_orders lo
JOIN customers c ON lo.customer_email = c.email
JOIN products p ON lo.product_name = p.name;
```

---

## 8. Performance Optimization

### 8.1 Batch Processing

```sql
-- Migration procedure with batching
CREATE PROCEDURE migrate_table_batched(
    source_table VARCHAR,
    target_table VARCHAR,
    batch_size INTEGER DEFAULT 10000,
    id_column VARCHAR DEFAULT 'id'
)
AS $$
DECLARE
    min_id BIGINT;
    max_id BIGINT;
    current_id BIGINT;
    rows_migrated BIGINT := 0;
BEGIN
    -- Get ID range
    EXECUTE format('SELECT MIN(%I), MAX(%I) FROM legacy_sync.%I',
                   id_column, id_column, source_table)
    INTO min_id, max_id;

    current_id := min_id;

    WHILE current_id <= max_id LOOP
        -- Migrate batch
        EXECUTE format('
            INSERT INTO %I
            SELECT * FROM legacy_sync.%I
            WHERE %I >= $1 AND %I < $2
        ', target_table, source_table, id_column, id_column)
        USING current_id, current_id + batch_size;

        rows_migrated := rows_migrated + @@ROWCOUNT;

        -- Progress logging
        RAISE NOTICE 'Migrated % to %, total: %',
            current_id, current_id + batch_size, rows_migrated;

        current_id := current_id + batch_size;

        -- Checkpoint to prevent long-running transaction
        COMMIT;
    END LOOP;

    RAISE NOTICE 'Migration complete: % rows', rows_migrated;
END;
$$ LANGUAGE plpgsql;

-- Usage
CALL migrate_table_batched('users', 'users', 50000, 'id');
```

### 8.2 Parallel Migration

```sql
-- Split migration across workers
CREATE TABLE migration_tasks (
    task_id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    min_id BIGINT,
    max_id BIGINT,
    status VARCHAR(20) DEFAULT 'pending',
    worker_id VARCHAR(50),
    started_at TIMESTAMP,
    completed_at TIMESTAMP,
    rows_migrated BIGINT
);

-- Create tasks
INSERT INTO migration_tasks (table_name, min_id, max_id)
SELECT
    'large_table',
    (n - 1) * 100000,
    n * 100000
FROM generate_series(1, 100) n;  -- 100 chunks of 100k rows

-- Worker procedure
CREATE PROCEDURE run_migration_task(p_worker_id VARCHAR)
AS $$
DECLARE
    task RECORD;
BEGIN
    -- Claim a task
    UPDATE migration_tasks
    SET status = 'running', worker_id = p_worker_id, started_at = CURRENT_TIMESTAMP
    WHERE task_id = (
        SELECT task_id FROM migration_tasks
        WHERE status = 'pending'
        ORDER BY task_id
        LIMIT 1
        FOR UPDATE SKIP LOCKED
    )
    RETURNING * INTO task;

    IF task IS NULL THEN
        RAISE NOTICE 'No tasks available';
        RETURN;
    END IF;

    -- Execute migration
    EXECUTE format('
        INSERT INTO %I
        SELECT * FROM legacy_sync.%I
        WHERE id >= $1 AND id < $2
    ', task.table_name, task.table_name)
    USING task.min_id, task.max_id;

    -- Mark complete
    UPDATE migration_tasks
    SET status = 'completed',
        completed_at = CURRENT_TIMESTAMP,
        rows_migrated = @@ROWCOUNT
    WHERE task_id = task.task_id;
END;
$$ LANGUAGE plpgsql;
```

---

## 9. Rollback Procedures

```sql
-- Create rollback snapshot before migration
CREATE TABLE users_rollback_20240301 AS
SELECT * FROM users;

-- Rollback procedure
CREATE PROCEDURE rollback_migration(snapshot_date DATE)
AS $$
DECLARE
    snapshot_table VARCHAR;
BEGIN
    snapshot_table := 'users_rollback_' || TO_CHAR(snapshot_date, 'YYYYMMDD');

    -- Restore from snapshot
    TRUNCATE users;
    EXECUTE format('INSERT INTO users SELECT * FROM %I', snapshot_table);

    -- Log rollback
    INSERT INTO migration_log (action, table_name, timestamp)
    VALUES ('ROLLBACK', 'users', CURRENT_TIMESTAMP);
END;
$$ LANGUAGE plpgsql;
```

---

## 10. Monitoring Dashboard

```sql
-- Migration progress view
CREATE VIEW migration_progress AS
SELECT
    source_table,
    (SELECT COUNT(*) FROM legacy_sync.users) AS source_count,
    (SELECT COUNT(*) FROM users) AS target_count,
    ROUND(100.0 * (SELECT COUNT(*) FROM users) /
          NULLIF((SELECT COUNT(*) FROM legacy_sync.users), 0), 2) AS percent_complete,
    (SELECT MAX(migrated_at) FROM users) AS last_sync,
    CASE
        WHEN (SELECT COUNT(*) FROM users) =
             (SELECT COUNT(*) FROM legacy_sync.users)
        THEN 'COMPLETE'
        ELSE 'IN PROGRESS'
    END AS status
FROM (VALUES ('users')) AS t(source_table);

-- Query the dashboard
SELECT * FROM migration_progress;
```
