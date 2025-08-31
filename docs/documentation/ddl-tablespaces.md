### DDL: Tablespaces

**What it is**

Tablespaces are named locations in the file system where database files are stored. They allow administrators to control the physical layout of database objects across different storage devices, enabling performance optimization, storage management, and backup strategies. Tablespaces can be assigned to databases, tables, indexes, and other database objects.

**Why it matters**

- **Performance**: Place hot data on fast SSDs, cold data on slower storage
- **Storage Management**: Distribute data across multiple disks
- **Backup Strategy**: Separate critical data for targeted backups
- **Growth Management**: Add storage without moving existing data
- **I/O Distribution**: Balance I/O load across multiple devices

**How to use it**

Create tablespaces pointing to specific directories, then assign database objects to them during creation or move existing objects. Monitor space usage and adjust placement based on access patterns and performance requirements.

## CREATE TABLESPACE

### Basic Syntax

```sql
-- Create tablespace
CREATE TABLESPACE tablespace_name
    LOCATION '/path/to/directory';

-- Examples
CREATE TABLESPACE fast_ssd
    LOCATION '/mnt/ssd/scratchbird';

CREATE TABLESPACE archive_storage
    LOCATION '/mnt/archive/scratchbird';

CREATE TABLESPACE temp_space
    LOCATION '/mnt/temp/scratchbird';
```

### With Options

```sql
-- Tablespace with owner
CREATE TABLESPACE app_data
    OWNER app_admin
    LOCATION '/data/app';

-- Tablespace with size limits (if supported)
CREATE TABLESPACE limited_space
    LOCATION '/data/limited'
    WITH (max_size = '100GB');

-- Tablespace for specific workload
CREATE TABLESPACE analytics_space
    LOCATION '/data/analytics'
    WITH (
        random_page_cost = 1.1,  -- SSD optimized
        seq_page_cost = 1.0
    );
```

## ALTER TABLESPACE

```sql
-- Rename tablespace
ALTER TABLESPACE old_name RENAME TO new_name;

-- Change owner
ALTER TABLESPACE fast_ssd OWNER TO dba_role;

-- Set tablespace options
ALTER TABLESPACE analytics_space 
    SET (random_page_cost = 1.5);

-- Reset options to default
ALTER TABLESPACE analytics_space 
    RESET (random_page_cost);
```

## DROP TABLESPACE

```sql
-- Drop empty tablespace
DROP TABLESPACE obsolete_space;

-- Drop if exists
DROP TABLESPACE IF EXISTS temp_space;

-- Cannot drop non-empty tablespace
-- Must move or drop all objects first
```

## Using Tablespaces

### Database Level

```sql
-- Create database in specific tablespace
CREATE DATABASE analytics_db
    TABLESPACE analytics_space;

-- Move existing database
ALTER DATABASE reporting_db
    SET TABLESPACE new_location;
```

### Table Level

```sql
-- Create table in tablespace
CREATE TABLE large_logs (
    id BIGINT PRIMARY KEY,
    timestamp TIMESTAMP,
    message TEXT
) TABLESPACE archive_storage;

-- Create partitioned table with tablespaces
CREATE TABLE sales (
    id BIGINT,
    sale_date DATE,
    amount DECIMAL(10,2)
) PARTITION BY RANGE (sale_date);

-- Partitions in different tablespaces
CREATE TABLE sales_2023 
    PARTITION OF sales
    FOR VALUES FROM ('2023-01-01') TO ('2024-01-01')
    TABLESPACE archive_storage;

CREATE TABLE sales_2024
    PARTITION OF sales  
    FOR VALUES FROM ('2024-01-01') TO ('2025-01-01')
    TABLESPACE fast_ssd;

-- Move existing table
ALTER TABLE old_data
    SET TABLESPACE archive_storage;
```

### Index Level

```sql
-- Create index in tablespace
CREATE INDEX idx_users_email ON users(email)
    TABLESPACE fast_ssd;

-- Create unique index in tablespace
CREATE UNIQUE INDEX idx_orders_number ON orders(order_number)
    TABLESPACE fast_ssd;

-- Move existing index
ALTER INDEX idx_old_data
    SET TABLESPACE archive_storage;
```

### Materialized View Level

```sql
-- Create materialized view in tablespace
CREATE MATERIALIZED VIEW sales_summary
TABLESPACE fast_ssd
AS
SELECT 
    date_trunc('month', sale_date) AS month,
    SUM(amount) AS total_sales
FROM sales
GROUP BY date_trunc('month', sale_date);
```

## Tablespace Strategies

### Performance Tiering

```sql
-- Hot data on SSD
CREATE TABLESPACE hot_data
    LOCATION '/mnt/nvme/hot';

-- Warm data on SAS drives  
CREATE TABLESPACE warm_data
    LOCATION '/mnt/sas/warm';

-- Cold data on SATA drives
CREATE TABLESPACE cold_data
    LOCATION '/mnt/sata/cold';

-- Current month on fastest storage
CREATE TABLE orders_current
    TABLESPACE hot_data
AS SELECT * FROM orders 
WHERE order_date >= date_trunc('month', CURRENT_DATE);

-- Previous months on medium storage
CREATE TABLE orders_recent
    TABLESPACE warm_data  
AS SELECT * FROM orders
WHERE order_date >= CURRENT_DATE - INTERVAL '6 months'
  AND order_date < date_trunc('month', CURRENT_DATE);

-- Archive on slow storage
CREATE TABLE orders_archive
    TABLESPACE cold_data
AS SELECT * FROM orders
WHERE order_date < CURRENT_DATE - INTERVAL '6 months';
```

### Workload Separation

```sql
-- OLTP tablespace (optimized for random I/O)
CREATE TABLESPACE oltp_space
    LOCATION '/mnt/ssd/oltp'
    WITH (random_page_cost = 1.1);

-- OLAP tablespace (optimized for sequential I/O)
CREATE TABLESPACE olap_space
    LOCATION '/mnt/raid/olap'
    WITH (seq_page_cost = 0.5);

-- Temporary tablespace for sorts
CREATE TABLESPACE temp_sort_space
    LOCATION '/mnt/temp/sort';

-- Assign to appropriate objects
CREATE TABLE transactions (...) TABLESPACE oltp_space;
CREATE TABLE fact_sales (...) TABLESPACE olap_space;
SET temp_tablespaces = 'temp_sort_space';
```

### Backup and Recovery

```sql
-- Critical data tablespace (frequent backups)
CREATE TABLESPACE critical_data
    LOCATION '/mnt/critical/data';

-- Non-critical tablespace (less frequent backups)
CREATE TABLESPACE non_critical
    LOCATION '/mnt/standard/data';

-- Temporary/recreatable data (no backups needed)
CREATE TABLESPACE temp_data
    LOCATION '/mnt/temp/data';

-- Assign based on criticality
ALTER TABLE financial_transactions
    SET TABLESPACE critical_data;

ALTER TABLE user_sessions
    SET TABLESPACE non_critical;

ALTER TABLE cache_entries
    SET TABLESPACE temp_data;
```

## Monitoring Tablespaces

### Space Usage

```sql
-- Check tablespace sizes
SELECT 
    spcname AS tablespace_name,
    pg_size_pretty(pg_tablespace_size(oid)) AS size
FROM pg_tablespace
ORDER BY pg_tablespace_size(oid) DESC;

-- Objects in tablespace
SELECT 
    tablespace,
    schemaname,
    tablename,
    pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename)) AS size
FROM pg_tables
WHERE tablespace IS NOT NULL
ORDER BY tablespace, pg_total_relation_size(schemaname||'.'||tablename) DESC;

-- Indexes in tablespace  
SELECT 
    tablespace,
    schemaname,
    indexname,
    pg_size_pretty(pg_relation_size(schemaname||'.'||indexname)) AS size
FROM pg_indexes
WHERE tablespace IS NOT NULL
ORDER BY tablespace, pg_relation_size(schemaname||'.'||indexname) DESC;
```

### Growth Tracking

```sql
-- Track tablespace growth over time
CREATE TABLE tablespace_stats (
    check_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    tablespace_name TEXT,
    size_bytes BIGINT
);

-- Periodic collection
INSERT INTO tablespace_stats (tablespace_name, size_bytes)
SELECT 
    spcname,
    pg_tablespace_size(oid)
FROM pg_tablespace;

-- Growth analysis
SELECT 
    tablespace_name,
    MIN(size_bytes) AS initial_size,
    MAX(size_bytes) AS current_size,
    pg_size_pretty(MAX(size_bytes) - MIN(size_bytes)) AS growth,
    ROUND(100.0 * (MAX(size_bytes) - MIN(size_bytes)) / MIN(size_bytes), 2) AS growth_percent
FROM tablespace_stats
GROUP BY tablespace_name;
```

## Best Practices

### Planning

1. **Capacity Planning**: Size tablespaces for expected growth
2. **Performance Testing**: Benchmark different configurations
3. **Monitoring**: Track usage and performance metrics
4. **Documentation**: Document tablespace purposes and policies

```sql
-- Document tablespace purposes
COMMENT ON TABLESPACE fast_ssd IS 
    'High-performance SSD storage for hot data and critical indexes';

COMMENT ON TABLESPACE archive_storage IS
    'Cost-effective storage for historical data, backed up weekly';
```

### Maintenance

```sql
-- Regular maintenance tasks
-- 1. Check space availability
SELECT 
    spcname,
    pg_size_pretty(pg_tablespace_size(oid)) AS used_space
FROM pg_tablespace;

-- 2. Identify candidates for archival
SELECT 
    schemaname,
    tablename,
    pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename)) AS size
FROM pg_tables
WHERE tablespace = 'fast_ssd'
  AND tablename LIKE '%_old'
  OR tablename LIKE '%_archive';

-- 3. Move to appropriate tablespace
ALTER TABLE old_orders SET TABLESPACE archive_storage;
```

### Migration Example

```sql
-- Migrate table to new tablespace
BEGIN;

-- Create new tablespace
CREATE TABLESPACE new_storage
    LOCATION '/mnt/new/storage';

-- Move table (locks table)
ALTER TABLE large_table
    SET TABLESPACE new_storage;

-- Move associated indexes
ALTER INDEX idx_large_table_pk
    SET TABLESPACE new_storage;

ALTER INDEX idx_large_table_date
    SET TABLESPACE new_storage;

-- Verify migration
SELECT 
    tablename,
    tablespace
FROM pg_tables
WHERE tablename = 'large_table';

COMMIT;
```

## Implementation Details

**Parser** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_tablespace`: Handles CREATE/ALTER/DROP TABLESPACE

**AST Structure** (`include/scratchbird/engine/ast.h`):
```cpp
struct DdlTablespaceAst {
    std::string name;
    std::string location;
    std::optional<std::string> owner;
    std::unordered_map<std::string, std::string> options;
    std::string action;  // CREATE|ALTER|DROP
};
```

**Storage Manager**:
- Maps tablespace names to filesystem paths
- Manages file placement and movement
- Tracks space usage and quotas

**Code Anchors**:
- Tablespace parser: `src/engine/parser_ddl.cpp` (parse_ddl_tablespace)
- AST definitions: `include/scratchbird/engine/ast.h`
- Storage management: `src/engine/storage_manager.cpp`

## See also

- [Tables](./ddl-tables.md) - Creating tables in tablespaces
- [Indexes](./ddl-indexes.md) - Index tablespace placement
- [Materialized Views](./ddl-materialized-views.md) - MV storage options
- [Configuration](./configuration.md) - Tablespace-related settings
- [Installation](./installation.md) - Setting up tablespace directories