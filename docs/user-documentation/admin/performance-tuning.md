# Performance Tuning

Optimize ScratchBird for your workload.

[Back to Admin Index](index.md) | [Back to Documentation Index](../index.md)

---

## Performance Principles

1. **Measure first** - Identify bottlenecks before optimizing
2. **One change at a time** - Understand impact of each change
3. **Test with realistic data** - Small datasets behave differently
4. **Monitor continuously** - Performance changes over time

---

## Memory Configuration

### Shared Buffers

The buffer pool caches database pages in memory.

```ini
# sb_server.conf
[memory]
shared_buffers = 4GB
```

**Guidelines:**
- Start with 25% of system RAM
- Max ~8GB (diminishing returns above)
- Leave room for OS cache and work_mem

| System RAM | shared_buffers |
|------------|---------------|
| 4GB | 1GB |
| 16GB | 4GB |
| 64GB | 8GB |
| 256GB | 8GB |

### Work Memory

Memory for sorts, hash joins, and other operations.

```ini
[memory]
work_mem = 64MB
```

**Guidelines:**
- `work_mem × max_connections` should fit in available RAM
- Increase for complex queries (sorts, aggregates)
- Monitor with `EXPLAIN ANALYZE` for "Sort Method: external"

### Maintenance Work Memory

Memory for VACUUM, CREATE INDEX, etc.

```ini
[memory]
maintenance_work_mem = 512MB
```

**Guidelines:**
- Set higher than work_mem
- Used during maintenance, not concurrent queries
- Speeds up index creation

---

## Query Optimization

### Use EXPLAIN ANALYZE

```sql
EXPLAIN ANALYZE SELECT * FROM orders WHERE customer_id = 123;
```

Look for:
- **Seq Scan** on large tables (missing index)
- **Sort Method: external** (need more work_mem)
- **Rows** estimates vs actual (outdated statistics)

### Update Statistics

```sql
-- Single table
ANALYZE customers;

-- All tables
ANALYZE;
```

Run after:
- Bulk data loads
- Major updates
- Adding indexes

### Index Strategies

**B-tree (default)** - Most queries:
```sql
CREATE INDEX idx_orders_customer ON orders(customer_id);
```

**Hash** - Equality only:
```sql
CREATE INDEX idx_users_email ON users USING HASH (email);
```

**Partial index** - Frequent filter:
```sql
CREATE INDEX idx_orders_pending ON orders(created_at)
WHERE status = 'pending';
```

**Covering index** - Index-only scans:
```sql
CREATE INDEX idx_orders_cover ON orders(customer_id)
INCLUDE (total, created_at);
```

**Composite index** - Multiple columns:
```sql
-- Order matters: most selective first
CREATE INDEX idx_orders_cust_date ON orders(customer_id, created_at);
```

### Query Patterns

**Avoid SELECT ***:
```sql
-- Bad
SELECT * FROM orders WHERE customer_id = 1;

-- Good
SELECT id, total, created_at FROM orders WHERE customer_id = 1;
```

**Use EXISTS instead of COUNT**:
```sql
-- Bad
SELECT COUNT(*) > 0 FROM orders WHERE customer_id = 1;

-- Good
SELECT EXISTS(SELECT 1 FROM orders WHERE customer_id = 1);
```

**Avoid functions on indexed columns**:
```sql
-- Bad (can't use index)
SELECT * FROM users WHERE LOWER(email) = 'user@example.com';

-- Good (expression index)
CREATE INDEX idx_users_email_lower ON users(LOWER(email));
SELECT * FROM users WHERE LOWER(email) = 'user@example.com';
```

---

## Connection Pooling

### Server-Side Limits

```ini
# sb_server.conf
[server]
max_connections = 200
idle_timeout = 600
```

### Application-Side Pooling

Configure your application's connection pool:

```python
# Python SQLAlchemy
engine = create_engine(
    "postgresql://...",
    pool_size=10,
    max_overflow=20,
    pool_timeout=30,
    pool_recycle=1800
)
```

### Pool Sizing

Formula:
```
pool_size = (cores × 2) + spindle_count
```

For SSD (no spindles):
```
pool_size = cores × 2
```

---

## Disk I/O

### Use SSDs

SSDs dramatically improve:
- Random reads (index lookups)
- Write-intensive workloads
- Concurrent queries

### Separate Data and Logs

```bash
# Data on SSD
/dev/nvme0n1  /var/lib/scratchbird

# Logs on separate disk
/dev/sdb      /var/log/scratchbird
```

### RAID Configuration

| Workload | RAID Level |
|----------|------------|
| Read-heavy | RAID 10 |
| Write-heavy | RAID 10 |
| Storage | RAID 5 (not recommended) |

---

## Analyzing Slow Queries

### Enable Slow Query Logging

```ini
# sb_server.conf
[logging]
log_slow_queries = 500  # milliseconds
```

### Find Slow Queries

```bash
grep "SLOW QUERY" /var/log/scratchbird/sb_server.log | \
    awk -F'ms' '{print $1}' | sort -rn | head -20
```

### Analyze with EXPLAIN

```sql
EXPLAIN (ANALYZE, BUFFERS, FORMAT TEXT)
SELECT ... your slow query ...;
```

Key metrics:
- **Planning Time** - Query planning overhead
- **Execution Time** - Actual execution
- **Buffers: shared hit** - Cache hits (good)
- **Buffers: shared read** - Disk reads (expensive)

---

## Bulk Operations

### Bulk INSERT

```sql
-- Slow: individual inserts
INSERT INTO data VALUES (1, 'a');
INSERT INTO data VALUES (2, 'b');
...

-- Fast: multi-row insert
INSERT INTO data VALUES
    (1, 'a'),
    (2, 'b'),
    (3, 'c'),
    ...;

-- Fastest: COPY
COPY data FROM '/path/to/file.csv' WITH CSV;
```

### Bulk UPDATE

```sql
-- Slow: row-by-row
UPDATE orders SET status = 'shipped' WHERE id = 1;
UPDATE orders SET status = 'shipped' WHERE id = 2;

-- Fast: batch update
UPDATE orders SET status = 'shipped'
WHERE id IN (1, 2, 3, 4, 5);

-- Fastest: join update
UPDATE orders o
SET status = 'shipped'
FROM shipments s
WHERE s.order_id = o.id AND s.shipped_at IS NOT NULL;
```

### Batch DELETE

```sql
-- Delete in batches to avoid lock contention
DO $$
BEGIN
    LOOP
        DELETE FROM logs
        WHERE created_at < NOW() - INTERVAL '90 days'
        LIMIT 10000;

        EXIT WHEN NOT FOUND;
        COMMIT;
    END LOOP;
END $$;
```

---

## Vacuum and Maintenance

### Manual VACUUM

```sql
-- Standard vacuum (reclaim space)
VACUUM orders;

-- Full vacuum (compacts table, locks)
VACUUM FULL orders;

-- Analyze while vacuuming
VACUUM ANALYZE orders;
```

### Autovacuum Configuration

```ini
# sb_server.conf
[autovacuum]
autovacuum = on
autovacuum_vacuum_threshold = 50
autovacuum_analyze_threshold = 50
autovacuum_vacuum_scale_factor = 0.2
autovacuum_analyze_scale_factor = 0.1
```

### Monitor Bloat

```sql
-- Table bloat
SELECT
    schemaname,
    relname,
    n_dead_tup,
    n_live_tup,
    ROUND(n_dead_tup::numeric / NULLIF(n_live_tup, 0) * 100, 2) AS dead_ratio
FROM pg_stat_user_tables
WHERE n_dead_tup > 1000
ORDER BY n_dead_tup DESC;
```

---

## Benchmark Testing

### Built-in Benchmark

```bash
# Run pgbench (compatible)
pgbench -h localhost -p 5432 -U admin -i -s 10 mydb  # Initialize
pgbench -h localhost -p 5432 -U admin -c 10 -j 2 -T 60 mydb  # Run
```

### Custom Benchmark

```bash
#!/bin/bash
# Simple read benchmark
for i in {1..1000}; do
    sb_isql -H localhost -c "SELECT * FROM orders WHERE id = $((RANDOM % 10000))" > /dev/null
done
```

---

## Configuration Profiles

### OLTP (Transaction Processing)

```ini
[memory]
shared_buffers = 4GB
work_mem = 16MB

[server]
max_connections = 500

[logging]
log_slow_queries = 100
```

### OLAP (Analytics)

```ini
[memory]
shared_buffers = 8GB
work_mem = 256MB
maintenance_work_mem = 1GB

[server]
max_connections = 50

[logging]
log_slow_queries = 5000
```

### Mixed Workload

```ini
[memory]
shared_buffers = 4GB
work_mem = 64MB

[server]
max_connections = 200
```

---

## Monitoring Performance

### Key Metrics

```sql
-- Cache hit ratio (should be > 99%)
SELECT
    ROUND(
        blks_hit::numeric / NULLIF(blks_hit + blks_read, 0) * 100, 2
    ) AS cache_hit_ratio
FROM pg_stat_database
WHERE datname = current_database();

-- Index hit ratio
SELECT
    ROUND(
        idx_blks_hit::numeric / NULLIF(idx_blks_hit + idx_blks_read, 0) * 100, 2
    ) AS index_hit_ratio
FROM pg_statio_user_indexes;

-- Transactions per second
SELECT
    xact_commit + xact_rollback AS total_xact
FROM pg_stat_database
WHERE datname = current_database();
```

### Identify Bottlenecks

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| High CPU | Complex queries | Optimize queries, add indexes |
| High I/O | Sequential scans | Add indexes, increase shared_buffers |
| High memory | Large work_mem | Reduce work_mem or connections |
| Lock waits | Contention | Optimize transactions, reduce lock duration |

---

## Next Steps

- [Monitoring](monitoring.md)
- [Index documentation](../language-guide/ddl/create-index.md)
- [Query optimization](../language-guide/dml/select.md)
