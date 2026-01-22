# Performance Tuning

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

---

## Overview

This guide covers performance tuning for ScratchBird from an end-user and developer perspective. Learn how to identify bottlenecks, optimize queries, configure indexes, and tune your application for optimal performance.

**Topics covered:**
- Query optimization
- Index strategies
- Configuration tuning
- Monitoring performance
- Common performance issues

For server-level tuning and monitoring infrastructure, see the [Administration Guide](../admin/monitoring.md).

---

## Part 1: Understanding Performance

### Key Performance Metrics

| Metric | Good | Warning | Critical |
|--------|------|---------|----------|
| Query response time | < 100ms | 100-500ms | > 500ms |
| Cache hit ratio | > 99% | 95-99% | < 95% |
| Connection count | < 80% max | 80-95% max | > 95% max |
| Disk I/O wait | < 10% | 10-30% | > 30% |
| CPU usage | < 70% | 70-90% | > 90% |

### Quick Health Check

```sql
-- Database health overview
SELECT
    'Connections' AS metric,
    COUNT(*)::text AS value
FROM pg_stat_activity
WHERE backend_type = 'client backend'
UNION ALL
SELECT
    'Active queries',
    COUNT(*)::text
FROM pg_stat_activity
WHERE state = 'active'
UNION ALL
SELECT
    'Cache hit ratio',
    ROUND(
        blks_hit * 100.0 / NULLIF(blks_hit + blks_read, 0), 2
    )::text || '%'
FROM pg_stat_database
WHERE datname = current_database()
UNION ALL
SELECT
    'Database size',
    pg_size_pretty(pg_database_size(current_database()));
```

---

## Part 2: Query Optimization

### Using EXPLAIN

**Basic explain:**
```sql
-- Show query plan
EXPLAIN SELECT * FROM orders WHERE customer_id = 123;

-- Output example:
-- Index Scan using idx_orders_customer on orders
--   Index Cond: (customer_id = 123)
```

**Explain with execution stats:**
```sql
-- ANALYZE actually runs the query
EXPLAIN (ANALYZE, BUFFERS, FORMAT TEXT)
SELECT * FROM orders WHERE customer_id = 123;

-- Output includes:
-- Execution Time: 0.5ms
-- Buffers: shared hit=10
```

**Understanding plan output:**

| Term | Meaning |
|------|---------|
| Seq Scan | Full table scan (often slow) |
| Index Scan | Using index (usually good) |
| Index Only Scan | Data from index only (fastest) |
| Bitmap Scan | Multiple index conditions |
| Nested Loop | Join method for small sets |
| Hash Join | Join method for larger sets |
| Sort | Sorting required |
| Rows | Estimated/actual row count |
| Cost | Relative execution cost |

### Common Query Problems

**Problem 1: Sequential scan on large table**
```sql
-- Slow: Full table scan
EXPLAIN SELECT * FROM orders WHERE status = 'pending';
-- Seq Scan on orders (cost=0.00..15000.00 rows=5000)
--   Filter: (status = 'pending')

-- Solution: Add index
CREATE INDEX idx_orders_status ON orders(status);

-- Fast: Index scan
EXPLAIN SELECT * FROM orders WHERE status = 'pending';
-- Index Scan using idx_orders_status on orders (cost=0.00..100.00 rows=5000)
```

**Problem 2: Missing index on join column**
```sql
-- Slow: Nested loop with sequential scan
EXPLAIN SELECT o.*, c.name
FROM orders o
JOIN customers c ON o.customer_id = c.id;

-- Solution: Ensure indexes on join columns
CREATE INDEX idx_orders_customer ON orders(customer_id);
-- customers.id should already have primary key index
```

**Problem 3: Inefficient OR conditions**
```sql
-- Slow: Multiple OR conditions
SELECT * FROM orders
WHERE status = 'pending' OR status = 'processing' OR status = 'shipped';

-- Better: Use IN
SELECT * FROM orders
WHERE status IN ('pending', 'processing', 'shipped');

-- Best: If querying most statuses, consider exclusion
SELECT * FROM orders
WHERE status != 'delivered';
```

**Problem 4: Function on indexed column**
```sql
-- Slow: Index cannot be used
SELECT * FROM customers WHERE LOWER(email) = 'user@example.com';

-- Solution 1: Store normalized data
ALTER TABLE customers ADD COLUMN email_lower VARCHAR(255);
UPDATE customers SET email_lower = LOWER(email);
CREATE INDEX idx_customers_email_lower ON customers(email_lower);

-- Solution 2: Expression index
CREATE INDEX idx_customers_email_lower ON customers(LOWER(email));
```

### Query Rewriting Tips

**Use EXISTS instead of IN for subqueries:**
```sql
-- Slower with large subquery result
SELECT * FROM customers
WHERE id IN (SELECT customer_id FROM orders WHERE total > 1000);

-- Faster with EXISTS
SELECT * FROM customers c
WHERE EXISTS (
    SELECT 1 FROM orders o
    WHERE o.customer_id = c.id AND o.total > 1000
);
```

**Limit results early:**
```sql
-- Slow: Sorts all rows, then limits
SELECT * FROM orders ORDER BY created_at DESC LIMIT 10;

-- Faster with index on created_at
CREATE INDEX idx_orders_created ON orders(created_at DESC);
SELECT * FROM orders ORDER BY created_at DESC LIMIT 10;
```

**Avoid SELECT *:**
```sql
-- Slower: Retrieves all columns
SELECT * FROM orders WHERE customer_id = 123;

-- Faster: Only needed columns
SELECT id, total, status FROM orders WHERE customer_id = 123;
```

---

## Part 3: Index Strategies

### When to Create Indexes

**Good candidates for indexing:**
- Columns in WHERE clauses
- Columns in JOIN conditions
- Columns in ORDER BY clauses
- Columns with high selectivity (many unique values)
- Foreign key columns

**Poor candidates for indexing:**
- Very small tables (< 1000 rows)
- Columns with low selectivity (few unique values)
- Columns rarely used in queries
- Tables with frequent writes and few reads

### Index Types

**B-tree (default):**
```sql
-- Best for: Equality and range queries
CREATE INDEX idx_orders_date ON orders(created_at);

-- Supports: =, <, >, <=, >=, BETWEEN, IN
SELECT * FROM orders WHERE created_at > '2026-01-01';
SELECT * FROM orders WHERE created_at BETWEEN '2026-01-01' AND '2026-01-31';
```

**Hash index:**
```sql
-- Best for: Equality comparisons only
CREATE INDEX idx_users_email ON users USING HASH (email);

-- Supports: = only
SELECT * FROM users WHERE email = 'user@example.com';
```

**Composite index:**
```sql
-- For queries with multiple columns
CREATE INDEX idx_orders_customer_date ON orders(customer_id, created_at);

-- Works for:
SELECT * FROM orders WHERE customer_id = 123;  -- Uses first column
SELECT * FROM orders WHERE customer_id = 123 AND created_at > '2026-01-01';  -- Uses both

-- Does NOT work for (order matters):
SELECT * FROM orders WHERE created_at > '2026-01-01';  -- Can't use index
```

**Partial index:**
```sql
-- Index only rows matching condition
CREATE INDEX idx_orders_pending ON orders(created_at)
WHERE status = 'pending';

-- Smaller index, faster for specific queries
SELECT * FROM orders WHERE status = 'pending' ORDER BY created_at;
```

**Covering index (Index-Only Scan):**
```sql
-- Include additional columns in index
CREATE INDEX idx_orders_customer_covering ON orders(customer_id)
INCLUDE (status, total);

-- Query can be satisfied from index alone
SELECT status, total FROM orders WHERE customer_id = 123;
```

### Index Maintenance

**Check index usage:**
```sql
SELECT
    schemaname,
    tablename,
    indexname,
    idx_scan AS times_used,
    idx_tup_read AS tuples_read,
    idx_tup_fetch AS tuples_fetched
FROM pg_stat_user_indexes
ORDER BY idx_scan DESC;
```

**Find unused indexes:**
```sql
SELECT
    schemaname || '.' || tablename AS table_name,
    indexname,
    pg_size_pretty(pg_relation_size(indexrelid)) AS index_size
FROM pg_stat_user_indexes
WHERE idx_scan = 0
AND indexrelname NOT LIKE '%_pkey'  -- Don't drop primary keys
ORDER BY pg_relation_size(indexrelid) DESC;
```

**Rebuild indexes:**
```sql
-- Rebuild specific index
REINDEX INDEX idx_orders_customer;

-- Rebuild all indexes on table
REINDEX TABLE orders;

-- Rebuild all indexes in database
REINDEX DATABASE mydb;
```

**Index bloat check:**
```sql
-- Check index size vs data size
SELECT
    c.relname AS table_name,
    pg_size_pretty(pg_relation_size(c.oid)) AS table_size,
    pg_size_pretty(pg_indexes_size(c.oid)) AS indexes_size,
    ROUND(
        pg_indexes_size(c.oid)::numeric /
        NULLIF(pg_relation_size(c.oid), 0) * 100, 2
    ) AS index_ratio
FROM pg_class c
JOIN pg_namespace n ON n.oid = c.relnamespace
WHERE c.relkind = 'r' AND n.nspname = 'public'
ORDER BY pg_relation_size(c.oid) DESC
LIMIT 20;
```

---

## Part 4: Configuration Tuning

### Memory Settings

```ini
# sb_server.conf

[memory]
# Shared buffer pool - start with 25% of RAM (max 8GB for most workloads)
buffer_pool_size = 2GB

# Per-operation memory for sorts, joins, etc.
# Higher = faster complex queries, but uses more RAM per connection
work_mem = 16MB

# Memory for maintenance operations (VACUUM, CREATE INDEX)
maintenance_work_mem = 256MB

# Memory for temp tables before spilling to disk
temp_buffers = 32MB
```

**Guidelines:**

| System RAM | buffer_pool_size | work_mem | maintenance_work_mem |
|------------|----------------|----------|----------------------|
| 2 GB | 512 MB | 4 MB | 64 MB |
| 4 GB | 1 GB | 8 MB | 128 MB |
| 8 GB | 2 GB | 16 MB | 256 MB |
| 16 GB | 4 GB | 32 MB | 512 MB |
| 32+ GB | 8 GB | 64 MB | 1 GB |

### Connection Settings

```ini
[server]
# Maximum concurrent connections
max_connections = 100

# Reserved for superuser
superuser_reserved_connections = 3

# Connection timeout in seconds
connection_timeout = 30

# Idle session timeout (0 = disabled)
idle_in_transaction_session_timeout = 300000  # 5 minutes
```

**Connection pool sizing:**
```
Optimal connections = (CPU cores * 2) + disk spindles
```

For most systems: 20-100 connections with connection pooling.

### Query Planner Settings

```ini
[query]
# Cost estimates for sequential vs random page access
# Lower random_page_cost for SSD storage
seq_page_cost = 1.0
random_page_cost = 1.1  # Use 1.1-1.5 for SSD, 4.0 for HDD

# Estimated size of disk cache
effective_cache_size = 6GB  # 50-75% of total RAM

# Enable/disable plan types (usually leave enabled)
enable_seqscan = on
enable_indexscan = on
enable_hashjoin = on
enable_mergejoin = on
```

### Checkpoint and WAL Settings

```ini
[wal]
# Checkpoint frequency
checkpoint_completion_target = 0.9
checkpoint_timeout = 5min

# WAL buffers
wal_buffers = 64MB

# Minimum WAL for crash recovery
min_wal_size = 1GB
max_wal_size = 4GB
```

---

## Part 5: Table Maintenance

### VACUUM and ANALYZE

**When to run:**
- VACUUM: Reclaims space from deleted/updated rows
- ANALYZE: Updates statistics for query planner

```sql
-- Basic vacuum (non-blocking)
VACUUM orders;

-- Vacuum with space reclamation (blocks table)
VACUUM FULL orders;

-- Update statistics
ANALYZE orders;

-- Both together
VACUUM ANALYZE orders;

-- Verbose output
VACUUM VERBOSE ANALYZE orders;
```

**Autovacuum settings:**
```ini
[autovacuum]
autovacuum = on
autovacuum_vacuum_threshold = 50
autovacuum_vacuum_scale_factor = 0.2
autovacuum_analyze_threshold = 50
autovacuum_analyze_scale_factor = 0.1
```

### Table Bloat

**Check for table bloat:**
```sql
SELECT
    schemaname || '.' || relname AS table_name,
    n_live_tup AS live_rows,
    n_dead_tup AS dead_rows,
    ROUND(n_dead_tup * 100.0 / NULLIF(n_live_tup + n_dead_tup, 0), 2) AS dead_pct,
    last_vacuum,
    last_autovacuum,
    last_analyze
FROM pg_stat_user_tables
WHERE n_dead_tup > 1000
ORDER BY n_dead_tup DESC;
```

**Fix bloated tables:**
```sql
-- For moderate bloat (non-blocking)
VACUUM orders;

-- For severe bloat (blocks writes)
VACUUM FULL orders;

-- Alternative: CLUSTER (reorders table by index)
CLUSTER orders USING idx_orders_created;
```

### Partitioning for Large Tables

**Range partitioning example:**
```sql
-- Create partitioned table
CREATE TABLE orders (
    id BIGINT,
    customer_id INTEGER,
    created_at TIMESTAMP,
    total DECIMAL(10,2)
) PARTITION BY RANGE (created_at);

-- Create partitions
CREATE TABLE orders_2025 PARTITION OF orders
    FOR VALUES FROM ('2025-01-01') TO ('2026-01-01');

CREATE TABLE orders_2026 PARTITION OF orders
    FOR VALUES FROM ('2026-01-01') TO ('2027-01-01');

-- Queries automatically use appropriate partition
SELECT * FROM orders WHERE created_at >= '2026-01-01';
-- Only scans orders_2026 partition
```

---

## Part 6: Application-Level Optimization

### Connection Pooling

**Why pooling matters:**
- Database connections are expensive to create
- Limited number of connections available
- Pooling reuses connections

**PgBouncer example:**
```ini
# pgbouncer.ini
[databases]
mydb = host=localhost port=5432 dbname=mydb

[pgbouncer]
listen_port = 6432
listen_addr = *
auth_type = scram-sha-256
pool_mode = transaction
max_client_conn = 1000
default_pool_size = 20
```

**Application pooling (Python):**
```python
from psycopg2 import pool

# Create pool once at startup
connection_pool = pool.ThreadedConnectionPool(
    minconn=5,
    maxconn=20,
    host='localhost',
    port=5432,
    database='mydb',
    user='app_user',
    password='password'
)

# Get connection from pool
conn = connection_pool.getconn()
try:
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM orders WHERE id = %s", (order_id,))
    result = cursor.fetchone()
finally:
    # Return connection to pool
    connection_pool.putconn(conn)
```

### Prepared Statements

**Reuse query plans:**
```sql
-- Prepare statement once
PREPARE get_order AS
SELECT * FROM orders WHERE id = $1;

-- Execute multiple times
EXECUTE get_order(123);
EXECUTE get_order(456);
EXECUTE get_order(789);

-- Deallocate when done
DEALLOCATE get_order;
```

**In application code (Python):**
```python
# Using parameterized queries (automatically prepared)
cursor.execute(
    "SELECT * FROM orders WHERE customer_id = %s AND status = %s",
    (customer_id, status)
)
```

### Batch Operations

**Batch inserts:**
```python
# Slow: One insert at a time
for order in orders:
    cursor.execute(
        "INSERT INTO orders (customer_id, total) VALUES (%s, %s)",
        (order['customer_id'], order['total'])
    )

# Fast: executemany
cursor.executemany(
    "INSERT INTO orders (customer_id, total) VALUES (%s, %s)",
    [(o['customer_id'], o['total']) for o in orders]
)

# Fastest: COPY
from io import StringIO
buffer = StringIO()
for order in orders:
    buffer.write(f"{order['customer_id']},{order['total']}\n")
buffer.seek(0)
cursor.copy_from(buffer, 'orders', columns=('customer_id', 'total'), sep=',')
```

### Caching Strategies

**Query result caching:**
```python
import redis
import json
import hashlib

redis_client = redis.Redis(host='localhost', port=6379)

def get_customer_orders(customer_id):
    # Generate cache key
    cache_key = f"orders:customer:{customer_id}"

    # Check cache
    cached = redis_client.get(cache_key)
    if cached:
        return json.loads(cached)

    # Query database
    cursor.execute(
        "SELECT * FROM orders WHERE customer_id = %s",
        (customer_id,)
    )
    orders = cursor.fetchall()

    # Cache result (expire in 5 minutes)
    redis_client.setex(cache_key, 300, json.dumps(orders))

    return orders
```

---

## Part 7: Monitoring Performance

### Real-Time Monitoring Queries

**Active queries:**
```sql
SELECT
    pid,
    usename,
    state,
    NOW() - query_start AS duration,
    LEFT(query, 80) AS query
FROM pg_stat_activity
WHERE state != 'idle'
AND query NOT LIKE '%pg_stat_activity%'
ORDER BY duration DESC;
```

**Slow queries:**
```sql
-- Currently running slow queries
SELECT
    pid,
    NOW() - query_start AS duration,
    state,
    LEFT(query, 100) AS query
FROM pg_stat_activity
WHERE state = 'active'
AND query_start < NOW() - INTERVAL '10 seconds'
ORDER BY duration DESC;
```

**Lock contention:**
```sql
SELECT
    blocked.pid AS blocked_pid,
    blocked.usename AS blocked_user,
    blocking.pid AS blocking_pid,
    blocking.usename AS blocking_user,
    LEFT(blocked.query, 50) AS blocked_query
FROM pg_stat_activity blocked
JOIN pg_locks bl ON blocked.pid = bl.pid
JOIN pg_locks kl ON bl.locktype = kl.locktype
    AND bl.database IS NOT DISTINCT FROM kl.database
    AND bl.relation IS NOT DISTINCT FROM kl.relation
    AND bl.pid != kl.pid
JOIN pg_stat_activity blocking ON kl.pid = blocking.pid
WHERE NOT bl.granted;
```

### Table Statistics

```sql
-- Table access patterns
SELECT
    schemaname || '.' || relname AS table_name,
    seq_scan,
    seq_tup_read,
    idx_scan,
    idx_tup_fetch,
    n_tup_ins AS inserts,
    n_tup_upd AS updates,
    n_tup_del AS deletes
FROM pg_stat_user_tables
ORDER BY seq_tup_read DESC
LIMIT 20;
```

### Cache Performance

```sql
-- Buffer cache hit ratio by table
SELECT
    c.relname AS table_name,
    pg_size_pretty(pg_relation_size(c.oid)) AS size,
    ROUND(
        100.0 * heap_blks_hit / NULLIF(heap_blks_hit + heap_blks_read, 0), 2
    ) AS cache_hit_ratio
FROM pg_statio_user_tables s
JOIN pg_class c ON s.relid = c.oid
WHERE heap_blks_hit + heap_blks_read > 0
ORDER BY heap_blks_read DESC
LIMIT 20;
```

---

## Part 8: Common Performance Issues

### Issue: Slow SELECT Queries

**Diagnosis:**
```sql
EXPLAIN (ANALYZE, BUFFERS) SELECT ...your query...;
```

**Common causes and solutions:**

| Symptom | Cause | Solution |
|---------|-------|----------|
| Seq Scan on large table | Missing index | Add appropriate index |
| High Buffers read | Data not cached | Increase buffer_pool_size |
| Sort on disk | work_mem too low | Increase work_mem |
| Nested Loop slow | Poor join order | Check statistics, ANALYZE |

### Issue: Slow INSERT/UPDATE

**Diagnosis:**
```sql
-- Check for locks
SELECT * FROM pg_locks WHERE NOT granted;

-- Check index count
SELECT COUNT(*) FROM pg_indexes WHERE tablename = 'orders';
```

**Solutions:**
- Disable indexes during bulk loads
- Use batch inserts
- Check for trigger overhead
- Consider partitioning

### Issue: High CPU Usage

**Diagnosis:**
```sql
-- Find CPU-intensive queries
SELECT
    pid,
    usename,
    NOW() - query_start AS duration,
    state,
    LEFT(query, 80) AS query
FROM pg_stat_activity
WHERE state = 'active'
ORDER BY duration DESC;
```

**Solutions:**
- Optimize slow queries
- Add missing indexes
- Check for inefficient functions
- Consider connection pooling

### Issue: High Memory Usage

**Diagnosis:**
```sql
-- Check work_mem intensive operations
EXPLAIN (ANALYZE, BUFFERS) SELECT ...;
-- Look for "Sort Method: external merge"
```

**Solutions:**
- Lower work_mem if too high per-connection
- Reduce max_connections
- Optimize queries to reduce memory needs

---

## Quick Reference

### Essential Tuning Commands

| Task | Command |
|------|---------|
| Explain query | `EXPLAIN (ANALYZE, BUFFERS) SELECT...` |
| Update statistics | `ANALYZE tablename` |
| Reclaim space | `VACUUM tablename` |
| Rebuild index | `REINDEX INDEX indexname` |
| Check table size | `SELECT pg_size_pretty(pg_relation_size('tablename'))` |
| Check index usage | `SELECT * FROM pg_stat_user_indexes` |

### Performance Checklist

- [ ] Queries have appropriate indexes
- [ ] No sequential scans on large tables
- [ ] Cache hit ratio > 99%
- [ ] No long-running queries
- [ ] No lock contention
- [ ] Tables are regularly vacuumed
- [ ] Statistics are up to date
- [ ] Connection pooling is configured
- [ ] work_mem is appropriate for workload
- [ ] buffer_pool_size is 25% of RAM (max 8GB)

---

## See Also

- [Indexes Guide](Indexes.md) - Detailed index documentation
- [Administration Monitoring](../admin/monitoring.md) - Server monitoring
- [Troubleshooting](../troubleshooting/Performance-Issues.md) - Performance issues
- [Query Optimization](../tutorials/) - Query tuning tutorials

