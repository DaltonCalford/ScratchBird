### EXPLAIN ANALYZE

**What it is**

EXPLAIN ANALYZE is a powerful diagnostic tool that reveals how ScratchBird executes queries. EXPLAIN shows the query execution plan without running the query, while EXPLAIN ANALYZE actually executes the query and provides real runtime statistics. This includes row counts, timing information, buffer usage, and optimization decisions, helping you understand and optimize query performance.

**Why it matters**

- **Performance Tuning**: Identify bottlenecks and inefficient operations
- **Index Optimization**: Discover missing indexes or unused existing ones
- **Query Optimization**: Understand join orders and algorithm choices
- **Resource Planning**: Estimate memory and I/O requirements
- **Debugging**: Diagnose why queries perform differently than expected

**How to use it**

Prefix any SELECT, INSERT, UPDATE, or DELETE statement with EXPLAIN to see the query plan. Use EXPLAIN ANALYZE to get actual execution metrics. Review the output to identify sequential scans that could use indexes, inefficient join orders, or inaccurate row estimates.

## EXPLAIN Basics

### Syntax

```sql
EXPLAIN [options] statement;
EXPLAIN ANALYZE [options] statement;

-- Options:
-- VERBOSE - Show additional details
-- BUFFERS - Include buffer usage statistics
-- FORMAT {TEXT|JSON|XML|YAML} - Output format
-- BYTECODE - Show compiled SBLR bytecode
-- ADAPTIVE - Show adaptive optimization status
```

### Output Types

- **Text Format**: Human-readable tree structure (default)
- **JSON Format**: Machine-parseable detailed information
- **EXPLAIN ANALYZE**: Includes actual execution metrics
- **EXPLAIN BYTECODE**: Shows compiled SBLR bytecode for the query
- **EXPLAIN ADAPTIVE**: Shows adaptive specialization and JIT status

### Basic Examples

```sql
-- Standard query plan
EXPLAIN SELECT name FROM employees WHERE id > 1;

-- Plan with execution statistics
EXPLAIN ANALYZE SELECT name FROM employees WHERE id > 1;

-- Verbose output with buffer information
EXPLAIN (ANALYZE, VERBOSE, BUFFERS) 
SELECT * FROM orders WHERE customer_id = 123;

-- JSON format for programmatic analysis
EXPLAIN (FORMAT JSON) 
SELECT * FROM products WHERE price > 100;

-- Show bytecode compilation
EXPLAIN BYTECODE SELECT name FROM employees WHERE id > 1;
-- Output: SBLR bytecode instructions for the query

-- Show adaptive optimization status
EXPLAIN ADAPTIVE SELECT name FROM employees WHERE id > 1;
-- Output: Specialization statistics, JIT compilation status
```

## Understanding Query Plans

### Plan Nodes

Each operation in the query plan is represented as a node:

```sql
EXPLAIN SELECT e.name, d.dept_name
FROM employees e
JOIN departments d ON e.dept_id = d.id
WHERE e.salary > 50000;

-- Output:
Hash Join  (cost=25.45..53.25 rows=40 width=64)
  Hash Cond: (e.dept_id = d.id)
  ->  Seq Scan on employees e  (cost=0.00..22.50 rows=100 width=36)
        Filter: (salary > 50000)
  ->  Hash  (cost=15.20..15.20 rows=20 width=36)
        ->  Seq Scan on departments d  (cost=0.00..15.20 rows=20 width=36)
```

### Cost Estimates

- **startup cost**: Cost before first row can be returned
- **total cost**: Total cost to return all rows
- **rows**: Estimated number of rows
- **width**: Average row width in bytes

### Common Node Types

#### Sequential Scan
```sql
EXPLAIN SELECT * FROM large_table;

-- Output:
Seq Scan on large_table  (cost=0.00..1234.00 rows=50000 width=100)
```

#### Index Scan
```sql
EXPLAIN SELECT * FROM users WHERE id = 42;

-- Output:
Index Scan using users_pkey on users  (cost=0.29..8.31 rows=1 width=100)
  Index Cond: (id = 42)
```

#### Nested Loop Join
```sql
EXPLAIN SELECT * FROM orders o, customers c 
WHERE o.customer_id = c.id AND c.country = 'USA';

-- Output:
Nested Loop  (cost=0.29..1678.00 rows=500 width=200)
  ->  Seq Scan on customers c  (cost=0.00..25.00 rows=50 width=100)
        Filter: (country = 'USA'::text)
  ->  Index Scan using idx_orders_customer on orders o  (cost=0.29..32.50 rows=10 width=100)
        Index Cond: (customer_id = c.id)
```

## EXPLAIN ANALYZE Details

### Actual vs Estimated

EXPLAIN ANALYZE shows both estimates and actual values:

```sql
EXPLAIN ANALYZE SELECT * FROM products WHERE price > 100;

-- Output:
Seq Scan on products  (cost=0.00..25.00 rows=100 width=50) 
                      (actual time=0.015..0.201 rows=87 loops=1)
  Filter: (price > 100)
  Rows Removed by Filter: 413
Planning Time: 0.082 ms
Execution Time: 0.234 ms
```

Key metrics:
- **actual time**: Actual time in milliseconds (startup..total)
- **rows**: Actual rows returned
- **loops**: Number of times node was executed
- **Rows Removed**: Rows filtered out

### Buffer Usage

With BUFFERS option:

```sql
EXPLAIN (ANALYZE, BUFFERS) SELECT * FROM large_table WHERE status = 'active';

-- Output:
Seq Scan on large_table  (cost=0.00..5234.00 rows=25000 width=100)
                         (actual time=0.023..45.234 rows=24567 loops=1)
  Filter: (status = 'active'::text)
  Rows Removed by Filter: 75433
  Buffers: shared hit=234 read=4567
    I/O Timings: read=35.234
Planning Time: 0.123 ms
Execution Time: 47.567 ms
```

Buffer metrics:
- **shared hit**: Pages found in buffer cache
- **read**: Pages read from disk
- **written**: Pages written
- **I/O Timings**: Time spent on I/O operations

## Optimization Techniques

### Index Usage Analysis

```sql
-- Check if index is being used
EXPLAIN ANALYZE SELECT * FROM orders 
WHERE order_date BETWEEN '2024-01-01' AND '2024-01-31';

-- If showing Seq Scan, consider creating index:
CREATE INDEX idx_orders_date ON orders(order_date);

-- Re-run EXPLAIN to verify index usage
```

### Join Order Optimization

```sql
-- Inefficient join order
EXPLAIN ANALYZE
SELECT *
FROM small_table s
JOIN large_table l ON s.id = l.small_id
JOIN huge_table h ON l.id = h.large_id;

-- Force better join order with explicit JOIN syntax or hints
EXPLAIN ANALYZE
SELECT *
FROM huge_table h
JOIN large_table l ON h.large_id = l.id
JOIN small_table s ON l.small_id = s.id;
```

### Subquery vs JOIN

```sql
-- Subquery approach
EXPLAIN ANALYZE
SELECT name FROM employees
WHERE dept_id IN (SELECT id FROM departments WHERE location = 'NYC');

-- JOIN approach (often more efficient)
EXPLAIN ANALYZE
SELECT DISTINCT e.name 
FROM employees e
JOIN departments d ON e.dept_id = d.id
WHERE d.location = 'NYC';
```

## Advanced Features

### Parallel Query Execution

```sql
EXPLAIN (ANALYZE, VERBOSE)
SELECT count(*), category
FROM large_products_table
GROUP BY category;

-- Output might show:
Finalize GroupAggregate  (cost=...) (actual time=...)
  ->  Gather Merge  (cost=...) (actual time=...)
        Workers Planned: 2
        Workers Launched: 2
        ->  Partial GroupAggregate  (cost=...) (actual time=...)
              ->  Parallel Seq Scan on large_products_table
```

### Partition Pruning

```sql
-- Table partitioned by date
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM sales_partitioned
WHERE sale_date >= '2024-01-01' AND sale_date < '2024-02-01';

-- Output shows only relevant partitions scanned:
Append  (cost=0.00..123.45 rows=1000 width=100)
  ->  Seq Scan on sales_2024_01  (cost=0.00..123.45 rows=1000 width=100)
        Filter: ((sale_date >= '2024-01-01') AND (sale_date < '2024-02-01'))
-- Note: Other partitions not listed (pruned)
```

### CTE Optimization

```sql
EXPLAIN ANALYZE
WITH regional_sales AS (
    SELECT region, SUM(amount) as total
    FROM sales
    GROUP BY region
)
SELECT * FROM regional_sales WHERE total > 10000;

-- Check if CTE is materialized or inlined
-- Output shows CTE Scan or inlined operations
```

## Bytecode Analysis

### EXPLAIN BYTECODE

Shows the compiled SBLR bytecode:

```sql
EXPLAIN BYTECODE SELECT id, name FROM users WHERE age > 18;

-- Output:
SBLR Bytecode:
0000: SBLR_RSE           ; Start record selection
0001: SBLR_RELATION2     ; Table: users
0005: SBLR_BOOLEAN       ; WHERE clause
0006: SBLR_FIELD2        ; Field: age
0009: SBLR_LITERAL       ; Value: 18
000D: SBLR_GTR           ; Greater than comparison
000E: SBLR_PROJECT       ; SELECT clause
000F: SBLR_FIELD2        ; Field: id
0012: SBLR_FIELD2        ; Field: name
0015: SBLR_END           ; End of bytecode
```

### EXPLAIN ADAPTIVE

Shows runtime optimization status:

```sql
EXPLAIN ADAPTIVE SELECT * FROM orders WHERE status = 'pending';

-- Output:
Adaptive Optimization Status:
- Execution Count: 1523
- Type Specialization: ENABLED
  - Field 'status': STRING_FAST (100% string type)
  - Comparison: CMP_STRING_FAST
- JIT Compilation: PENDING (threshold: 2000)
- Cache Hits: 1520/1523 (99.8%)
- Specialized Instructions: 3
```

## Performance Patterns

### Common Anti-patterns

1. **Missing Index on Foreign Keys**
```sql
-- Slow: Full table scan for each parent row
EXPLAIN ANALYZE
SELECT * FROM parent p
JOIN child c ON p.id = c.parent_id;
-- Look for: Nested Loop with Seq Scan on child
```

2. **Function Calls on Indexed Columns**
```sql
-- Index not used due to function
EXPLAIN ANALYZE
SELECT * FROM users WHERE UPPER(email) = 'USER@EXAMPLE.COM';
-- Better: Use functional index or case-insensitive collation
```

3. **OR Conditions Preventing Index Use**
```sql
-- May not use index efficiently
EXPLAIN ANALYZE
SELECT * FROM products 
WHERE category = 'Electronics' OR price > 1000;
-- Consider: UNION of two indexed queries
```

### Optimization Checklist

1. **Check Seq Scans on Large Tables**
   - Add appropriate indexes
   - Consider partial indexes for filtered queries

2. **Review Join Methods**
   - Nested Loop: Good for small result sets
   - Hash Join: Good for larger unsorted data
   - Merge Join: Good for pre-sorted data

3. **Analyze Row Estimates**
   - Large discrepancies indicate stale statistics
   - Run ANALYZE to update statistics

4. **Monitor Buffer Usage**
   - High disk reads indicate insufficient cache
   - Consider increasing shared_buffers

5. **Look for Sort Operations**
   - Add indexes for ORDER BY columns
   - Consider covering indexes

## Implementation Details

**Code Anchors**:
- Query Planner: `src/engine/planner.cpp`
- Plan Executor: `src/engine/executor.cpp`
- Statistics: `src/engine/statistics.cpp`
- Cost Model: `src/engine/cost_model.cpp`
- EXPLAIN Handler: `src/engine/explain.cpp`

## See Also

- [SQL SELECT](./sql-select.md) - Query syntax and features
- [DDL Indexes](./ddl-indexes.md) - Index creation and management
- [Configuration](./configuration.md) - Performance tuning parameters
- [Dev Tools](./dev-tools.md) - Query profiling tools
- [Complete SBLR/BLR Specification](/workspace/docs/scratchbird-bytecode-complete-specification.md) - Bytecode details