# Query Optimization Fundamentals

[Planner and Query Tuning README](../README.md) | [Performance Guide README](../../README.md)

## Synopsis

Query optimization techniques for ScratchBird's cost-based query planner.

## Understanding Query Plans

### EXPLAIN

```sql
-- Basic execution plan
EXPLAIN SELECT * FROM users WHERE id = 1;

-- With actual execution statistics
EXPLAIN ANALYZE SELECT * FROM users WHERE id = 1;

-- Verbose output
EXPLAIN (VERBOSE, ANALYZE) SELECT * FROM users;

-- JSON format for tooling
EXPLAIN (FORMAT JSON, ANALYZE) SELECT * FROM users;
```

### Plan Components

```
EXPLAIN SELECT * FROM users WHERE email = 'test@example.com';

                         QUERY PLAN
------------------------------------------------------------
 Index Scan using idx_users_email on users
   Index Cond: (email = 'test@example.com')
   Cost: 0.29..8.30 rows=1 width=100)
```

| Component | Description |
|-----------|-------------|
| `Cost: X..Y` | Startup cost..Total cost |
| `rows=N` | Estimated rows returned |
| `width=N` | Average row width (bytes) |

## Common Plan Types

### Sequential Scan

```sql
-- Full table read (slow for large tables)
Seq Scan on users
  Filter: (age > 25)
```

**When acceptable:**
- Small tables (< 1000 rows)
- Most rows match filter
- No suitable index

### Index Scan

```sql
-- Index lookup + heap fetch
Index Scan using idx_users_age on users
  Index Cond: (age = 30)
```

**Use when:**
- Selective queries (few rows match)
- Index exists on filter columns

### Index Only Scan

```sql
-- Data satisfied by index alone (no heap access)
Index Only Scan using idx_users_email on users
  Index Cond: (email = 'test@example.com')
```

**Requirements:**
- All selected columns in index
- Table recently vacuumed

### Bitmap Index Scan

```sql
-- Bitmap for multiple index matches
Bitmap Heap Scan on users
  Recheck Cond: (age > 25)
  ->  Bitmap Index Scan on idx_users_age
        Index Cond: (age > 25)
```

**Use when:**
- Many rows match
- Multiple indexes combined

## Optimization Techniques

### 1. Index Usage

```sql
-- Create appropriate indexes
CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_users_created ON users(created_at);

-- Composite index for multi-column queries
CREATE INDEX idx_users_name_email ON users(name, email);

-- Partial index for common filters
CREATE INDEX idx_active_users ON users(email) WHERE status = 'active';
```

### 2. Covering Indexes

```sql
-- Include additional columns to avoid heap access
CREATE INDEX idx_users_covering ON users(email) 
    INCLUDE (name, created_at);

-- Query uses index only
SELECT name, created_at FROM users WHERE email = 'test@example.com';
```

### 3. Statistics

```sql
-- Update statistics for accurate planning
ANALYZE users;
ANALYZE orders;

-- Update specific columns
ANALYZE users (email, status);
```

### 4. Query Rewriting

```sql
-- Avoid functions on indexed columns (bad)
SELECT * FROM users WHERE LOWER(email) = 'test@example.com';

-- Use functional index or case-insensitive type (good)
SELECT * FROM users WHERE email = 'test@example.com';

-- Or create functional index
CREATE INDEX idx_users_lower_email ON users(LOWER(email));
```

### 5. LIMIT Optimization

```sql
-- Fast for ordered indexed column
SELECT * FROM orders ORDER BY created_at DESC LIMIT 10;

-- Slow (must sort entire table)
SELECT * FROM orders ORDER BY random() LIMIT 10;
```

### 6. Join Optimization

```sql
-- Prefer joins over subqueries
-- Bad:
SELECT * FROM users 
WHERE id IN (SELECT user_id FROM orders WHERE amount > 100);

-- Good:
SELECT DISTINCT u.* FROM users u
JOIN orders o ON u.id = o.user_id
WHERE o.amount > 100;

-- Ensure join columns are indexed
CREATE INDEX idx_orders_user_id ON orders(user_id);
```

## Cost Configuration

### Planner Settings

```ini
# Enable/disable plan types
enable_seqscan = on        # Sequential scans
enable_indexscan = on      # Index scans
enable_bitmapscan = on     # Bitmap scans
enable_hashjoin = on       # Hash joins
enable_nestloop = on       # Nested loop joins
enable_mergejoin = on      # Merge joins

# Cost factors (arbitrary units)
seq_page_cost = 1.0        # Sequential page read
random_page_cost = 4.0     # Random page read
cpu_tuple_cost = 0.01      # CPU per tuple
cpu_index_tuple_cost = 0.005  # CPU per index tuple
cpu_operator_cost = 0.0025    # CPU per operator
```

### Adjusting for SSD

```ini
# For SSD storage, random reads are faster
random_page_cost = 1.1    # Close to sequential
```

## Analyzing Slow Queries

### 1. Identify Slow Queries

```sql
-- From pg_stat_statements (if enabled)
SELECT 
    query,
    calls,
    total_time,
    mean_time,
    rows
FROM pg_stat_statements
ORDER BY mean_time DESC
LIMIT 10;
```

### 2. Check Plan Changes

```sql
-- Compare estimated vs actual rows
EXPLAIN (ANALYZE, BUFFERS) SELECT * FROM large_table WHERE ...;

-- Look for:
-- - Rows estimate much different from actual
-- - High buffer reads
-- - Seq scans on large tables
```

### 3. Common Problems

| Problem | Symptom | Solution |
|---------|---------|----------|
| Missing index | Seq scan on large table | Create index |
| Stale stats | Bad row estimates | Run ANALYZE |
| Function on column | Can't use index | Functional index |
| Too many rows | High buffer reads | Add LIMIT or filter |
| Lock contention | Slow writes | Reduce transaction time |

## Query Hints

```sql
-- Force index use
SET enable_seqscan = off;
SELECT * FROM users WHERE email = 'test@example.com';
SET enable_seqscan = on;

-- Join order hint
SET join_collapse_limit = 1;
-- Prevents join reordering
```

## Parallel Query

```ini
# Enable parallel query
max_parallel_workers_per_gather = 4
parallel_tuple_cost = 0.1
parallel_setup_cost = 1000
min_parallel_table_scan_size = 8MB
min_parallel_index_scan_size = 512kB
```

```sql
-- Check parallel plan
EXPLAIN (ANYZE) SELECT COUNT(*) FROM large_table;
-- Should show "Workers Planned: N"
```

## Caching Strategies

### Result Cache

```sql
-- Materialized view for expensive queries
CREATE MATERIALIZED VIEW monthly_sales AS
SELECT 
    DATE_TRUNC('month', order_date) as month,
    SUM(amount) as total
FROM orders
GROUP BY 1;

-- Refresh periodically
REFRESH MATERIALIZED VIEW monthly_sales;
```

### Prepared Statements

```sql
-- Prepare once, execute many
PREPARE user_lookup(TEXT) AS
    SELECT * FROM users WHERE email = $1;

EXECUTE user_lookup('test@example.com');
DEALLOCATE user_lookup;
```

## Monitoring Query Performance

```sql
-- Active queries
SELECT 
    pid,
    now() - query_start as duration,
    query
FROM pg_stat_activity
WHERE state = 'active'
ORDER BY duration DESC;

-- Lock waits
SELECT * FROM pg_locks WHERE NOT granted;
```

## See Also

- [Index Tuning](../index_and_data_layout_tuning/02_index_tuning_and_maintenance.md)
- [EXPLAIN Documentation](05_explain_plan_analyze_surfaces.md)
