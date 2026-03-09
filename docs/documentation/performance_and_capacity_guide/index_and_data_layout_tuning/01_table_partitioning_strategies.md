# Table Partitioning

[Index and Data Layout Tuning README](../README.md) | [Performance Guide README](../../README.md)

## Synopsis

Partitioning splits large tables into smaller, more manageable pieces while maintaining a unified query interface.

## Partitioning Types

### Range Partitioning

Divide by continuous ranges (dates, IDs).

```sql
-- Range partitioned table
CREATE TABLE events (
    id UUID DEFAULT gen_random_uuid(),
    event_time TIMESTAMPTZ NOT NULL,
    data JSONB
) PARTITION BY RANGE (event_time);

-- Create partitions
CREATE TABLE events_2024_q1 PARTITION OF events
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');

CREATE TABLE events_2024_q2 PARTITION OF events
    FOR VALUES FROM ('2024-04-01') TO ('2024-07-01');

CREATE TABLE events_2024_q3 PARTITION OF events
    FOR VALUES FROM ('2024-07-01') TO ('2024-10-01');

CREATE TABLE events_2024_q4 PARTITION OF events
    FOR VALUES FROM ('2024-10-01') TO ('2025-01-01');

-- Default partition for overflow
CREATE TABLE events_default PARTITION OF events DEFAULT;
```

### List Partitioning

Divide by discrete values (regions, categories).

```sql
-- List partitioned table
CREATE TABLE sales (
    id UUID DEFAULT gen_random_uuid(),
    region TEXT NOT NULL,
    amount DECIMAL(10,2)
) PARTITION BY LIST (region);

-- Create partitions
CREATE TABLE sales_north PARTITION OF sales
    FOR VALUES IN ('Northeast', 'Northwest', 'North');

CREATE TABLE sales_south PARTITION OF sales
    FOR VALUES IN ('Southeast', 'Southwest', 'South');

CREATE TABLE sales_central PARTITION OF sales
    FOR VALUES IN ('Central', 'Midwest');
```

### Hash Partitioning

Divide by hash for even distribution.

```sql
-- Hash partitioned table
CREATE TABLE users (
    id UUID DEFAULT gen_random_uuid(),
    email TEXT NOT NULL,
    created_at TIMESTAMPTZ
) PARTITION BY HASH (id);

-- Create partitions
CREATE TABLE users_p0 PARTITION OF users
    FOR VALUES WITH (MODULUS 4, REMAINDER 0);

CREATE TABLE users_p1 PARTITION OF users
    FOR VALUES WITH (MODULUS 4, REMAINDER 1);

CREATE TABLE users_p2 PARTITION OF users
    FOR VALUES WITH (MODULUS 4, REMAINDER 2);

CREATE TABLE users_p3 PARTITION OF users
    FOR VALUES WITH (MODULUS 4, REMAINDER 3);
```

## Partition Maintenance

### Adding Partitions

```sql
-- Add new range partition
CREATE TABLE events_2025_q1 PARTITION OF events
    FOR VALUES FROM ('2025-01-01') TO ('2025-04-01');
```

### Detaching Partitions

```sql
-- Detach for maintenance
ALTER TABLE events DETACH PARTITION events_2023_q1;

-- Detach concurrently (non-blocking)
ALTER TABLE events DETACH PARTITION events_2023_q1 CONCURRENTLY;

-- Finalize after concurrent detach
ALTER TABLE events DETACH PARTITION events_2023_q1 FINALIZE;
```

### Attaching Partitions

```sql
-- Attach existing table as partition
CREATE TABLE events_2023_q4 (LIKE events INCLUDING ALL);

-- Load data into events_2023_q4

-- Attach to partitioned table
ALTER TABLE events ATTACH PARTITION events_2023_q4
    FOR VALUES FROM ('2023-10-01') TO ('2024-01-01');
```

### Dropping Partitions

```sql
-- Drop specific partition (fast)
DROP TABLE events_2023_q1;

-- Or detach then drop
ALTER TABLE events DETACH PARTITION events_2023_q1;
DROP TABLE events_2023_q1;
```

## Query Optimization

### Partition Pruning

```sql
-- Only scans relevant partitions
SELECT * FROM events 
WHERE event_time BETWEEN '2024-02-01' AND '2024-02-15';
-- -> Only scans events_2024_q1

EXPLAIN SELECT * FROM events 
WHERE event_time >= '2024-06-01';
-- Shows partition pruning in plan
```

### Indexing Partitions

```sql
-- Create index on all partitions
CREATE INDEX idx_events_data ON events USING GIN (data);

-- Create index on specific partition
CREATE INDEX idx_events_q1_time ON events_2024_q1 (event_time);
```

### Constraints on Partitions

```sql
-- Partition-specific constraints
ALTER TABLE events_2024_q1 
    ADD CONSTRAINT check_q1 
    CHECK (event_time >= '2024-01-01' AND event_time < '2024-04-01');
```

## Best Practices

### 1. Choose Partition Key Wisely

```sql
-- Good: Time-based for time-series
PARTITION BY RANGE (created_at)

-- Good: Hash for even distribution
PARTITION BY HASH (user_id)

-- Bad: Low cardinality
PARTITION BY LIST (status)  -- Only 3-4 values
```

### 2. Number of Partitions

| Table Size | Recommended Partitions |
|------------|----------------------|
| < 10M rows | 4-8 |
| 10M-100M | 8-32 |
| 100M-1B | 32-128 |
| > 1B | 128+ |

### 3. Partition Size

Target partition size: 100MB - 10GB

```sql
-- Check partition sizes
SELECT 
    schemaname,
    tablename,
    pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename)) as size
FROM pg_tables
WHERE tablename LIKE 'events_%'
ORDER BY tablename;
```

### 4. Automated Partition Management

```sql
-- Create function to add monthly partitions
CREATE OR REPLACE FUNCTION create_monthly_partition(
    p_table TEXT,
    p_year INTEGER,
    p_month INTEGER
) RETURNS VOID AS $$
DECLARE
    partition_name TEXT;
    start_date DATE;
    end_date DATE;
BEGIN
    partition_name := p_table || '_' || p_year || '_' || LPAD(p_month::TEXT, 2, '0');
    start_date := make_date(p_year, p_month, 1);
    end_date := start_date + INTERVAL '1 month';
    
    EXECUTE format(
        'CREATE TABLE IF NOT EXISTS %I PARTITION OF %I FOR VALUES FROM (%L) TO (%L)',
        partition_name, p_table, start_date, end_date
    );
END;
$$ LANGUAGE plpgsql;

-- Schedule monthly partition creation
SELECT cron.schedule('create-monthly-partition', '0 0 25 * *', 
    $$SELECT create_monthly_partition('events', 
        EXTRACT(YEAR FROM NOW() + INTERVAL '1 month')::INTEGER,
        EXTRACT(MONTH FROM NOW() + INTERVAL '1 month')::INTEGER)$$);
```

## Partitioning Strategies by Use Case

### Time-Series Data (IoT, Logs)

```sql
-- Monthly partitions for time-series
CREATE TABLE sensor_data (
    sensor_id INTEGER,
    reading_time TIMESTAMPTZ NOT NULL,
    value DOUBLE PRECISION
) PARTITION BY RANGE (reading_time);

-- Create partitions for current and next month
-- Automate partition creation
-- Drop old partitions after retention period
```

### Multi-Tenant SaaS

```sql
-- Hash partition by tenant for even distribution
CREATE TABLE tenant_data (
    tenant_id INTEGER,
    user_id INTEGER,
    data JSONB
) PARTITION BY HASH (tenant_id);

-- 16 partitions for 16 tenants per partition on average
```

### Geographic Distribution

```sql
-- List partition by region
CREATE TABLE orders (
    order_id UUID,
    region TEXT,
    amount DECIMAL(10,2)
) PARTITION BY LIST (region);

-- Partitions per region for locality
```

## Monitoring Partitions

```sql
-- View partition information
SELECT 
    parent.relname AS parent_table,
    child.relname AS partition_name,
    pg_get_expr(child.relpartbound, child.oid) AS partition_constraint
FROM pg_inherits
JOIN pg_class parent ON pg_inherits.inhparent = parent.oid
JOIN pg_class child ON pg_inherits.inhrelid = child.oid
WHERE parent.relname = 'events';

-- Check partition sizes
SELECT 
    relname as partition,
    pg_size_pretty(pg_total_relation_size(oid)) as size,
    pg_size_pretty(pg_indexes_size(oid)) as index_size
FROM pg_class
WHERE relname LIKE 'events_%'
ORDER BY relname;
```

## See Also

- [CREATE TABLE](../../language_reference/syntax_guide/ddl/table_and_constraints/01_create_table.md)
- [Index Tuning](02_index_tuning_and_maintenance.md)
