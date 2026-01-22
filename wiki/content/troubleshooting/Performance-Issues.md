# Performance Issues

**Status:** Complete
**Last Updated:** 2026-01-20

---

## Overview

This guide helps diagnose and resolve performance issues in ScratchBird. Performance problems typically fall into these categories:

- **Slow queries** - Individual queries taking too long
- **Low throughput** - System can't handle expected load
- **High latency** - Response times are inconsistent
- **Resource exhaustion** - CPU, memory, or I/O bottlenecks

---

## Quick Performance Check

Run these queries to get a quick health snapshot:

```sql
-- 1. Check database size and activity
SELECT
    datname as database,
    pg_size_pretty(pg_database_size(datname)) as size,
    numbackends as connections,
    xact_commit as commits,
    xact_rollback as rollbacks,
    blks_read,
    blks_hit,
    CASE WHEN blks_hit + blks_read > 0
         THEN round(100.0 * blks_hit / (blks_hit + blks_read), 2)
         ELSE 0
    END as cache_hit_ratio
FROM pg_stat_database
WHERE datname = current_database();

-- 2. Check currently running queries
SELECT pid, usename, state,
       EXTRACT(EPOCH FROM (now() - query_start))::int as runtime_secs,
       LEFT(query, 80) as query_preview
FROM pg_stat_activity
WHERE state != 'idle'
AND query NOT LIKE '%pg_stat_activity%'
ORDER BY query_start;

-- 3. Check for locks
SELECT blocked.pid as blocked_pid,
       blocked.usename as blocked_user,
       blocking.pid as blocking_pid,
       blocking.usename as blocking_user,
       LEFT(blocked.query, 50) as blocked_query
FROM pg_stat_activity blocked
JOIN pg_stat_activity blocking ON blocking.pid = ANY(pg_blocking_pids(blocked.pid))
WHERE blocked.pid != blocking.pid;
```

---

## Slow Queries

### Symptoms

- Individual queries take longer than expected
- Application timeouts
- Users report slow page loads

### Step 1: Identify Slow Queries

**Using pg_stat_statements:**
```sql
-- Top 10 slowest queries by total time
SELECT
    calls,
    round(total_exec_time::numeric, 2) as total_time_ms,
    round(mean_exec_time::numeric, 2) as avg_time_ms,
    round(stddev_exec_time::numeric, 2) as stddev_ms,
    rows,
    LEFT(query, 100) as query
FROM pg_stat_statements
ORDER BY total_exec_time DESC
LIMIT 10;

-- Top queries by execution count
SELECT
    calls,
    round(total_exec_time::numeric, 2) as total_time_ms,
    round(mean_exec_time::numeric, 2) as avg_time_ms,
    LEFT(query, 100) as query
FROM pg_stat_statements
ORDER BY calls DESC
LIMIT 10;
```

**Using slow query log:**
```ini
# sb_server.conf
[logging]
log_min_duration_statement = 1000  # Log queries over 1 second
log_statement = 'none'             # Don't log all statements
```

### Step 2: Analyze Query Plan

**Basic EXPLAIN:**
```sql
EXPLAIN SELECT * FROM orders WHERE customer_id = 123;
```

**EXPLAIN with execution stats:**
```sql
EXPLAIN (ANALYZE, BUFFERS, FORMAT TEXT)
SELECT * FROM orders WHERE customer_id = 123;
```

**Understanding EXPLAIN output:**

| Indicator | Meaning | Action |
|-----------|---------|--------|
| Seq Scan | Full table scan | Add index on filter columns |
| Index Scan | Using index | Good, but check rows estimate |
| Nested Loop | Join method | Good for small tables |
| Hash Join | Join method | Better for larger tables |
| Sort | Data being sorted | Consider index with ORDER BY columns |
| Rows (actual vs estimated) | Row estimate accuracy | Run ANALYZE if very different |
| Buffers: shared hit | Pages from cache | Good - data was cached |
| Buffers: shared read | Pages from disk | Poor - needed disk I/O |

### Step 3: Common Fixes

**Missing Index:**
```sql
-- Check if column is indexed
SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'orders'
AND indexdef LIKE '%customer_id%';

-- Create index if missing
CREATE INDEX CONCURRENTLY idx_orders_customer_id
ON orders(customer_id);
```

**Outdated Statistics:**
```sql
-- Update statistics for a table
ANALYZE orders;

-- Update all statistics
ANALYZE;

-- Check when table was last analyzed
SELECT relname, last_analyze, last_autoanalyze
FROM pg_stat_user_tables
WHERE relname = 'orders';
```

**Inefficient Query Pattern:**
```sql
-- Bad: SELECT * when you only need a few columns
SELECT * FROM orders WHERE customer_id = 123;

-- Better: Select only needed columns
SELECT id, order_date, total FROM orders WHERE customer_id = 123;

-- Bad: LIKE with leading wildcard (can't use index)
SELECT * FROM customers WHERE name LIKE '%smith';

-- Better: Full-text search or trigram index
CREATE INDEX idx_customers_name_trgm ON customers
USING gin (name gin_trgm_ops);
SELECT * FROM customers WHERE name ILIKE '%smith%';
```

**N+1 Query Problem:**
```sql
-- Bad: Loop executing queries
-- Application code: for each order, SELECT customer
SELECT * FROM orders WHERE id = 1;
SELECT * FROM customers WHERE id = 101;
SELECT * FROM orders WHERE id = 2;
SELECT * FROM customers WHERE id = 102;
-- ... repeated N times

-- Better: JOIN in single query
SELECT o.*, c.name as customer_name
FROM orders o
JOIN customers c ON o.customer_id = c.id
WHERE o.id IN (1, 2, 3, ...);
```

---

## Index Problems

### Missing Indexes

**Find tables with sequential scans:**
```sql
SELECT
    schemaname,
    relname as table_name,
    seq_scan,
    idx_scan,
    CASE WHEN idx_scan > 0
         THEN round(100.0 * idx_scan / (seq_scan + idx_scan), 2)
         ELSE 0
    END as index_usage_pct,
    n_live_tup as row_count
FROM pg_stat_user_tables
WHERE seq_scan > idx_scan
AND n_live_tup > 10000
ORDER BY seq_scan DESC;
```

**Find columns that should be indexed:**
```sql
-- Look at WHERE clauses in slow queries, then check if indexed
SELECT
    t.relname as table_name,
    a.attname as column_name,
    pg_catalog.format_type(a.atttypid, a.atttypmod) as data_type
FROM pg_catalog.pg_attribute a
JOIN pg_catalog.pg_class t ON a.attrelid = t.oid
WHERE t.relname = 'orders'
AND a.attnum > 0
AND NOT a.attisdropped
AND NOT EXISTS (
    SELECT 1 FROM pg_index i
    WHERE i.indrelid = t.oid
    AND a.attnum = ANY(i.indkey)
);
```

### Unused Indexes

```sql
-- Find indexes that are never used
SELECT
    schemaname,
    relname as table_name,
    indexrelname as index_name,
    idx_scan as times_used,
    pg_size_pretty(pg_relation_size(indexrelid)) as index_size
FROM pg_stat_user_indexes
WHERE idx_scan = 0
AND indexrelname NOT LIKE '%pkey%'
ORDER BY pg_relation_size(indexrelid) DESC;
```

**Drop unused indexes (saves write overhead):**
```sql
-- Verify the index is really unused, then drop
DROP INDEX CONCURRENTLY idx_unused_index;
```

### Index Bloat

```sql
-- Check index bloat (approximate)
SELECT
    nspname as schema,
    relname as index_name,
    round(100 * pg_relation_size(indexrelid) / pg_relation_size(indrelid))::numeric as index_ratio,
    pg_size_pretty(pg_relation_size(indexrelid)) as index_size,
    pg_size_pretty(pg_relation_size(indrelid)) as table_size
FROM pg_index
JOIN pg_class ON pg_class.oid = pg_index.indexrelid
JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace
WHERE pg_relation_size(indrelid) > 0
ORDER BY pg_relation_size(indexrelid) DESC
LIMIT 20;
```

**Rebuild bloated index:**
```sql
-- REINDEX (locks table)
REINDEX INDEX idx_orders_customer_id;

-- Or rebuild concurrently (PostgreSQL 12+, no lock)
REINDEX INDEX CONCURRENTLY idx_orders_customer_id;
```

---

## Buffer Pool Issues

### Low Cache Hit Ratio

**Check cache hit ratio:**
```sql
SELECT
    sum(blks_hit) as cache_hits,
    sum(blks_read) as disk_reads,
    round(100.0 * sum(blks_hit) / nullif(sum(blks_hit) + sum(blks_read), 0), 2) as hit_ratio
FROM pg_stat_database;
```

**Target: 99%+ cache hit ratio**

**If ratio is low:**
```ini
# sb_server.conf - increase buffer pool
[memory]
buffer_pool_size = 4GB  # 25% of system RAM is typical
effective_cache_size = 12GB  # 75% of system RAM
```

### Buffer Pool Too Small

**Check if working set fits in memory:**
```sql
-- Total database size vs buffer_pool_size
SELECT
    pg_size_pretty(pg_database_size(current_database())) as database_size,
    pg_size_pretty(setting::bigint * 8192) as buffer_pool_size
FROM pg_settings
WHERE name = 'buffer_pool_size';

-- Size of frequently accessed tables
SELECT
    relname as table_name,
    pg_size_pretty(pg_total_relation_size(relid)) as total_size,
    n_live_tup as row_count,
    seq_scan + idx_scan as total_scans
FROM pg_stat_user_tables
ORDER BY seq_scan + idx_scan DESC
LIMIT 20;
```

---

## Lock Contention

### Identifying Lock Problems

**View current locks:**
```sql
SELECT
    locktype,
    relation::regclass as table_name,
    mode,
    granted,
    pid,
    (SELECT usename FROM pg_stat_activity WHERE pid = l.pid) as user
FROM pg_locks l
WHERE relation IS NOT NULL
ORDER BY relation;
```

**Find blocked queries:**
```sql
SELECT
    blocked.pid as blocked_pid,
    blocked.usename as blocked_user,
    blocking.pid as blocking_pid,
    blocking.usename as blocking_user,
    blocked.query as blocked_query,
    blocking.query as blocking_query,
    now() - blocked.query_start as blocked_duration
FROM pg_stat_activity blocked
JOIN pg_stat_activity blocking ON blocking.pid = ANY(pg_blocking_pids(blocked.pid))
WHERE blocked.state = 'active';
```

### Resolving Lock Issues

**Kill blocking query (if safe):**
```sql
-- Terminate the blocking backend
SELECT pg_terminate_backend(blocking_pid);

-- Or cancel just the query (gentler)
SELECT pg_cancel_backend(blocking_pid);
```

**Reduce lock duration:**
```sql
-- Bad: Long-running transaction holding locks
BEGIN;
SELECT * FROM orders FOR UPDATE;  -- Locks rows
-- ... application does slow processing ...
UPDATE orders SET status = 'processed' WHERE id = 1;
COMMIT;

-- Better: Minimize lock duration
BEGIN;
-- Do minimal work inside transaction
UPDATE orders SET status = 'processed' WHERE id = 1;
COMMIT;
```

**Use appropriate isolation level:**
```sql
-- Default is READ COMMITTED (usually fine)
BEGIN ISOLATION LEVEL READ COMMITTED;

-- SERIALIZABLE can cause more conflicts
-- Only use when truly needed
BEGIN ISOLATION LEVEL SERIALIZABLE;
```

---

## Memory Issues

### Check Memory Usage

```sql
-- Memory configuration
SELECT name, setting, unit, context
FROM pg_settings
WHERE name IN (
    'buffer_pool_size',
    'work_mem',
    'maintenance_work_mem',
    'effective_cache_size',
    'temp_buffers'
);
```

### work_mem Too Low

Queries spill to disk when sorting or hashing large result sets.

**Detect disk spills:**
```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM large_table ORDER BY some_column;
-- Look for: "Sort Method: external merge  Disk: XXXXkB"
```

**Increase work_mem:**
```ini
# sb_server.conf
[memory]
work_mem = 64MB  # Per-operation, not per-connection
```

**Or set per-session:**
```sql
SET work_mem = '128MB';
-- Run your query
RESET work_mem;
```

### Too Many Connections

Each connection uses memory.

**Check connection memory impact:**
```sql
SELECT count(*) as connections,
       (SELECT setting FROM pg_settings WHERE name = 'max_connections') as max_conn,
       pg_size_pretty(count(*) * 10 * 1024 * 1024) as approx_memory_used
FROM pg_stat_activity;
```

**Solution: Use connection pooling (PgBouncer):**
```ini
# pgbouncer.ini
[databases]
scratchbird = host=127.0.0.1 port=5432 dbname=scratchbird

[pgbouncer]
pool_mode = transaction
max_client_conn = 1000
default_pool_size = 25
```

---

## Disk I/O Problems

### Identify I/O Bottlenecks

**Check for disk-heavy operations:**
```sql
-- Tables with most I/O
SELECT
    relname as table_name,
    heap_blks_read as disk_reads,
    heap_blks_hit as cache_hits,
    CASE WHEN heap_blks_hit + heap_blks_read > 0
         THEN round(100.0 * heap_blks_hit / (heap_blks_hit + heap_blks_read), 2)
         ELSE 0
    END as hit_ratio
FROM pg_statio_user_tables
ORDER BY heap_blks_read DESC
LIMIT 10;
```

**Check temp file usage:**
```sql
SELECT
    datname,
    temp_files,
    pg_size_pretty(temp_bytes) as temp_size
FROM pg_stat_database
WHERE temp_files > 0;
```

### Solutions

**Increase buffer_pool_size:**
```ini
# sb_server.conf
[memory]
buffer_pool_size = 8GB  # Increase to cache more data
```

**Use faster storage:**
- Move data directory to SSD
- Use NVMe for best performance
- Consider RAID for throughput

**Partition large tables:**
```sql
-- Partition by date range
CREATE TABLE orders (
    id SERIAL,
    order_date DATE,
    customer_id INT,
    total DECIMAL
) PARTITION BY RANGE (order_date);

CREATE TABLE orders_2026_q1 PARTITION OF orders
FOR VALUES FROM ('2026-01-01') TO ('2026-04-01');
```

---

## Connection Problems

### Too Many Connections

**Check connection count:**
```sql
SELECT
    state,
    count(*) as count,
    max(EXTRACT(EPOCH FROM (now() - state_change)))::int as max_idle_secs
FROM pg_stat_activity
GROUP BY state;
```

**Find connection-heavy applications:**
```sql
SELECT
    application_name,
    client_addr,
    count(*) as connections
FROM pg_stat_activity
GROUP BY application_name, client_addr
ORDER BY count(*) DESC;
```

### Connection Pool Exhaustion

**Symptoms:**
- "FATAL: too many connections"
- Application hangs waiting for connection

**Solutions:**

1. **Increase max_connections:**
```ini
# sb_server.conf
[connections]
max_connections = 200
```

2. **Use connection pooler (recommended):**
```ini
# pgbouncer.ini
[pgbouncer]
pool_mode = transaction
default_pool_size = 25
max_client_conn = 1000
```

3. **Close idle connections:**
```sql
-- Set idle timeout
ALTER SYSTEM SET idle_in_transaction_session_timeout = '5min';
SELECT pg_reload_conf();

-- Manually terminate idle connections
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE state = 'idle'
AND state_change < now() - interval '30 minutes';
```

---

## Query Optimization Patterns

### Use LIMIT for Large Results

```sql
-- Bad: Fetch millions of rows
SELECT * FROM audit_log WHERE created_at > '2026-01-01';

-- Better: Paginate results
SELECT * FROM audit_log
WHERE created_at > '2026-01-01'
ORDER BY id
LIMIT 100 OFFSET 0;

-- Best: Keyset pagination (no OFFSET)
SELECT * FROM audit_log
WHERE created_at > '2026-01-01'
AND id > 12345  -- Last seen ID
ORDER BY id
LIMIT 100;
```

### Avoid SELECT *

```sql
-- Bad: Fetches all columns
SELECT * FROM customers WHERE id = 123;

-- Better: Only fetch needed columns
SELECT id, name, email FROM customers WHERE id = 123;

-- Benefits:
-- - Less data transferred
-- - Can use covering indexes
-- - Clearer intent
```

### Use Covering Indexes

```sql
-- Query
SELECT id, email FROM customers WHERE status = 'active';

-- Covering index (includes all selected columns)
CREATE INDEX idx_customers_status_covering
ON customers(status) INCLUDE (id, email);

-- Result: Index-only scan (no table lookup)
```

### Batch Operations

```sql
-- Bad: Individual inserts
INSERT INTO logs (message) VALUES ('msg1');
INSERT INTO logs (message) VALUES ('msg2');
INSERT INTO logs (message) VALUES ('msg3');

-- Better: Multi-value insert
INSERT INTO logs (message) VALUES
    ('msg1'),
    ('msg2'),
    ('msg3');

-- Best for bulk: COPY
COPY logs(message) FROM STDIN;
msg1
msg2
msg3
\.
```

---

## Monitoring Setup

### Essential Metrics to Track

| Metric | Query/Source | Alert Threshold |
|--------|--------------|-----------------|
| Cache hit ratio | `pg_stat_database` | < 95% |
| Active connections | `pg_stat_activity` | > 80% max |
| Long-running queries | `pg_stat_activity` | > 60 seconds |
| Locks waiting | `pg_locks` | > 10 |
| Temp file usage | `pg_stat_database` | > 1GB |
| Transaction wraparound | `pg_database` | < 1M from wrap |

### Quick Monitoring Query

```sql
-- All-in-one health check
SELECT
    (SELECT count(*) FROM pg_stat_activity WHERE state = 'active') as active_queries,
    (SELECT count(*) FROM pg_stat_activity WHERE state = 'idle') as idle_connections,
    (SELECT count(*) FROM pg_stat_activity WHERE wait_event_type = 'Lock') as waiting_on_locks,
    (SELECT round(100.0 * blks_hit / nullif(blks_hit + blks_read, 0), 2)
     FROM pg_stat_database WHERE datname = current_database()) as cache_hit_pct,
    (SELECT pg_size_pretty(sum(temp_bytes)) FROM pg_stat_database) as temp_usage,
    (SELECT count(*) FROM pg_stat_user_tables WHERE n_dead_tup > 10000) as tables_need_vacuum;
```

### Enable pg_stat_statements

```ini
# sb_server.conf
[extensions]
shared_preload_libraries = 'pg_stat_statements'

[pg_stat_statements]
pg_stat_statements.track = all
pg_stat_statements.max = 10000
```

```sql
-- After restart, create extension
CREATE EXTENSION IF NOT EXISTS pg_stat_statements;

-- Query top statements
SELECT * FROM pg_stat_statements ORDER BY total_exec_time DESC LIMIT 10;
```

---

## Performance Tuning Checklist

### Quick Wins

- [ ] Check cache hit ratio (target 99%+)
- [ ] Look for missing indexes on WHERE/JOIN columns
- [ ] Run ANALYZE on tables with stale statistics
- [ ] Check for long-running queries
- [ ] Review connection count vs max_connections

### Configuration Review

- [ ] buffer_pool_size = 25% of RAM
- [ ] effective_cache_size = 75% of RAM
- [ ] work_mem = 64MB-256MB (depends on concurrency)
- [ ] maintenance_work_mem = 1GB-2GB
- [ ] random_page_cost = 1.1 (for SSD)

### Query Optimization

- [ ] EXPLAIN ANALYZE slow queries
- [ ] Add missing indexes
- [ ] Remove unused indexes
- [ ] Use LIMIT with pagination
- [ ] Avoid SELECT * when possible
- [ ] Use batch operations for bulk data

### Regular Maintenance

- [ ] VACUUM ANALYZE runs regularly
- [ ] Monitor table/index bloat
- [ ] Archive old data
- [ ] Review and reset pg_stat_statements

---

## Quick Reference: Configuration Tuning

### Memory Settings

| Setting | Typical Value | Purpose |
|---------|---------------|---------|
| buffer_pool_size | 25% of RAM | Main data cache |
| effective_cache_size | 75% of RAM | Planner hint for OS cache |
| work_mem | 64MB-256MB | Per-operation sort/hash memory |
| maintenance_work_mem | 1GB-2GB | VACUUM, CREATE INDEX |

### Disk Settings

| Setting | SSD Value | HDD Value |
|---------|-----------|-----------|
| random_page_cost | 1.1 | 4.0 |
| effective_io_concurrency | 200 | 2 |
| seq_page_cost | 1.0 | 1.0 |

### Connection Settings

| Setting | Small Server | Large Server |
|---------|--------------|--------------|
| max_connections | 100 | 300 |
| superuser_reserved_connections | 3 | 5 |

---

## See Also

- [Connection Problems](Connection-Problems.md) - Connection troubleshooting
- [Common Errors](Common-Errors.md) - Error code reference
- [Performance Tuning Guide](../user-guides/Performance-Tuning.md) - Detailed tuning guide
- [Monitoring Guide](../admin/monitoring.md) - Set up monitoring
