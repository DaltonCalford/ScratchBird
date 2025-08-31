### EXPLAIN and EXPLAIN ANALYZE

**What it is**

EXPLAIN and EXPLAIN ANALYZE are diagnostic tools that reveal how the database query planner and executor process SQL statements. EXPLAIN shows the execution plan without running the query, while EXPLAIN ANALYZE executes the query and provides actual runtime statistics. These tools are essential for understanding query performance and optimization opportunities.

**Why it matters**

- **Performance Tuning**: Identify bottlenecks and inefficient operations
- **Index Optimization**: Verify index usage and effectiveness
- **Query Optimization**: Understand join strategies and data flow
- **Cost Estimation**: Compare estimated vs actual row counts and costs
- **Resource Usage**: Monitor buffer usage and I/O operations

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
```

### Basic EXPLAIN

```sql
-- Show query plan without execution
EXPLAIN SELECT * FROM customers WHERE city = 'New York';

-- Output:
-- Seq Scan on customers  (cost=0.00..155.00 rows=50 width=84)
--   Filter: (city = 'New York'::text)
```

### EXPLAIN ANALYZE

```sql
-- Execute query and show actual statistics
EXPLAIN ANALYZE SELECT * FROM customers WHERE city = 'New York';

-- Output:
-- Seq Scan on customers  (cost=0.00..155.00 rows=50 width=84) (actual time=0.015..1.234 rows=47 loops=1)
--   Filter: (city = 'New York'::text)
--   Rows Removed by Filter: 953
-- Planning Time: 0.123 ms
-- Execution Time: 1.456 ms
```

## Understanding Query Plans

### Plan Node Types

```sql
-- Sequential Scan
EXPLAIN SELECT * FROM large_table;
-- Seq Scan on large_table

-- Index Scan
EXPLAIN SELECT * FROM users WHERE id = 123;
-- Index Scan using users_pkey on users

-- Index Only Scan
EXPLAIN SELECT id FROM users WHERE id < 100;
-- Index Only Scan using users_pkey on users

-- Bitmap Scan
EXPLAIN SELECT * FROM orders WHERE status IN ('pending', 'processing');
-- Bitmap Heap Scan on orders
--   ->  Bitmap Index Scan on idx_orders_status

-- Filter
EXPLAIN SELECT * FROM products WHERE price > 100 AND category = 'Electronics';
-- Seq Scan on products
--   Filter: ((price > 100) AND (category = 'Electronics'))
```

### Join Operations

```sql
-- Nested Loop Join
EXPLAIN SELECT * FROM orders o 
JOIN customers c ON o.customer_id = c.id 
WHERE c.id = 123;
-- Nested Loop
--   ->  Index Scan using customers_pkey on customers c
--   ->  Index Scan using idx_orders_customer on orders o

-- Hash Join
EXPLAIN SELECT * FROM orders o 
JOIN products p ON o.product_id = p.id;
-- Hash Join
--   Hash Cond: (o.product_id = p.id)
--   ->  Seq Scan on orders o
--   ->  Hash
--         ->  Seq Scan on products p

-- Merge Join
EXPLAIN SELECT * FROM sorted_table1 t1
JOIN sorted_table2 t2 ON t1.id = t2.id;
-- Merge Join
--   Merge Cond: (t1.id = t2.id)
--   ->  Index Scan using sorted_table1_pkey on sorted_table1 t1
--   ->  Index Scan using sorted_table2_pkey on sorted_table2 t2
```

### Aggregation and Grouping

```sql
-- HashAggregate
EXPLAIN SELECT category, COUNT(*) 
FROM products 
GROUP BY category;
-- HashAggregate
--   Group Key: category
--   ->  Seq Scan on products

-- GroupAggregate (sorted)
EXPLAIN SELECT customer_id, SUM(total)
FROM orders
GROUP BY customer_id
ORDER BY customer_id;
-- GroupAggregate
--   Group Key: customer_id
--   ->  Index Scan using idx_orders_customer on orders

-- WindowAgg
EXPLAIN SELECT name, salary,
       AVG(salary) OVER (PARTITION BY department)
FROM employees;
-- WindowAgg
--   ->  Sort
--         Sort Key: department
--         ->  Seq Scan on employees
```

## Cost Estimation

### Understanding Costs

```sql
EXPLAIN SELECT * FROM users WHERE age > 25;
-- Seq Scan on users  (cost=0.00..155.00 rows=500 width=84)
--                           ^startup ^total ^estimated rows ^avg row size

-- cost=0.00..155.00
-- - First number: Startup cost (before first row returned)
-- - Second number: Total cost (arbitrary units)

-- rows=500
-- - Estimated number of rows returned

-- width=84
-- - Average row size in bytes
```

### Cost Factors

```sql
-- Sequential scan cost
EXPLAIN SELECT * FROM large_table;
-- Cost = seq_page_cost * pages + cpu_tuple_cost * rows

-- Index scan cost
EXPLAIN SELECT * FROM users WHERE id = 123;
-- Cost = random_page_cost * index_pages + cpu_index_tuple_cost * index_tuples

-- Join cost comparison
EXPLAIN SELECT * FROM t1 JOIN t2 ON t1.id = t2.id;
-- Planner chooses lowest cost among:
-- - Nested Loop: O(n*m) comparisons
-- - Hash Join: O(n+m) with hash table overhead
-- - Merge Join: O(n log n + m log m) sorting cost
```

## EXPLAIN ANALYZE Details

### Actual vs Estimated

```sql
EXPLAIN ANALYZE SELECT * FROM orders WHERE order_date = '2024-01-15';

-- Seq Scan on orders  (cost=0.00..250.00 rows=10 width=100) 
--                      (actual time=0.025..5.123 rows=45 loops=1)
--   Filter: (order_date = '2024-01-15'::date)
--   Rows Removed by Filter: 9955

-- Comparison:
-- Estimated: 10 rows
-- Actual: 45 rows
-- This indicates statistics may need updating
```

### Timing Information

```sql
EXPLAIN (ANALYZE, TIMING) SELECT * FROM products WHERE price > 100;

-- Seq Scan on products  (actual time=0.015..2.345 rows=234 loops=1)
--                              ^first row  ^total time
--   Filter: (price > 100)
--   Rows Removed by Filter: 766
-- Planning Time: 0.234 ms
-- Execution Time: 2.567 ms
```

### Buffer Usage

```sql
EXPLAIN (ANALYZE, BUFFERS) SELECT * FROM large_table WHERE status = 'active';

-- Seq Scan on large_table  (actual time=0.025..10.234 rows=1234 loops=1)
--   Filter: (status = 'active')
--   Rows Removed by Filter: 8766
--   Buffers: shared hit=85 read=15
--            ^from cache  ^from disk
-- Planning Time: 0.345 ms
-- Execution Time: 10.567 ms
```

## Output Formats

### JSON Format

```sql
EXPLAIN (FORMAT JSON) SELECT * FROM users WHERE id = 123;

-- [
--   {
--     "Plan": {
--       "Node Type": "Index Scan",
--       "Scan Direction": "Forward",
--       "Index Name": "users_pkey",
--       "Relation Name": "users",
--       "Startup Cost": 0.28,
--       "Total Cost": 8.29,
--       "Plan Rows": 1,
--       "Plan Width": 84
--     }
--   }
-- ]
```

### VERBOSE Output

```sql
EXPLAIN (VERBOSE) SELECT id, name FROM users WHERE age > 25;

-- Seq Scan on public.users  (cost=0.00..155.00 rows=500 width=36)
--   Output: id, name
--   Filter: (users.age > 25)
```

## Optimization Examples

### Missing Index

```sql
-- Before optimization
EXPLAIN ANALYZE SELECT * FROM orders WHERE customer_id = 123;
-- Seq Scan on orders  (cost=0.00..2500.00 rows=50 width=100) 
--                      (actual time=0.025..25.123 rows=47 loops=1)
--   Filter: (customer_id = 123)
--   Rows Removed by Filter: 99953

-- After adding index
CREATE INDEX idx_orders_customer ON orders(customer_id);

EXPLAIN ANALYZE SELECT * FROM orders WHERE customer_id = 123;
-- Index Scan using idx_orders_customer  (cost=0.42..51.23 rows=50 width=100)
--                                       (actual time=0.015..0.234 rows=47 loops=1)
```

### Join Order Optimization

```sql
-- Inefficient join order
EXPLAIN ANALYZE
SELECT * FROM large_table l
JOIN small_table s ON l.id = s.large_id
WHERE s.status = 'active';
-- Hash Join (cost=10000.00..50000.00 rows=100)
--   ->  Seq Scan on large_table l (1M rows)
--   ->  Hash
--         ->  Seq Scan on small_table s
--               Filter: (status = 'active')

-- Better with proper statistics
ANALYZE large_table, small_table;

-- Now planner chooses better order
EXPLAIN ANALYZE
SELECT * FROM large_table l
JOIN small_table s ON l.id = s.large_id
WHERE s.status = 'active';
-- Nested Loop (cost=0.42..500.00 rows=100)
--   ->  Index Scan on small_table s
--         Filter: (status = 'active')
--   ->  Index Scan on large_table l
--         Index Cond: (id = s.large_id)
```

### Covering Index

```sql
-- Without covering index
EXPLAIN ANALYZE SELECT id, name, email FROM users WHERE status = 'active';
-- Index Scan using idx_users_status  (cost=0.42..234.56 rows=500)
--   -- Requires heap fetches for name and email

-- With covering index
CREATE INDEX idx_users_status_covering ON users(status) INCLUDE (name, email);

EXPLAIN ANALYZE SELECT id, name, email FROM users WHERE status = 'active';
-- Index Only Scan using idx_users_status_covering  (cost=0.42..134.56 rows=500)
--   -- No heap fetches needed
```

## Common Patterns

### N+1 Query Problem

```sql
-- Inefficient: N+1 queries
EXPLAIN ANALYZE
SELECT * FROM orders WHERE customer_id = 123;
-- Then for each order:
EXPLAIN ANALYZE
SELECT * FROM order_items WHERE order_id = ?;

-- Better: Single query with join
EXPLAIN ANALYZE
SELECT o.*, oi.*
FROM orders o
JOIN order_items oi ON o.id = oi.order_id
WHERE o.customer_id = 123;
```

### Pagination Optimization

```sql
-- Inefficient for large offsets
EXPLAIN ANALYZE
SELECT * FROM products ORDER BY created_at LIMIT 10 OFFSET 10000;
-- Sort  (cost=2500.00..2750.00 rows=10)
--   ->  Seq Scan on products (reads all 10010 rows)

-- Better with keyset pagination
EXPLAIN ANALYZE
SELECT * FROM products 
WHERE created_at > '2024-01-15 12:00:00'
ORDER BY created_at 
LIMIT 10;
-- Index Scan using idx_products_created  (cost=0.42..50.00 rows=10)
```

## Monitoring and Maintenance

### Update Statistics

```sql
-- Update table statistics for better plans
ANALYZE customers;
ANALYZE orders;

-- Verbose analyze
ANALYZE VERBOSE products;
-- INFO:  analyzing "public.products"
-- INFO:  "products": scanned 300 of 300 pages, containing 10000 live rows
```

### Plan Cache

```sql
-- View cached plans (if available)
SELECT * FROM pg_prepared_statements;

-- Reset plan cache
DISCARD PLANS;
```

## Best Practices

1. **Regular ANALYZE**: Keep statistics up-to-date
2. **Test with Production Data**: Plans differ with data volume
3. **Check Both EXPLAIN and EXPLAIN ANALYZE**: Estimates vs reality
4. **Monitor Slow Queries**: Log and analyze slow query plans
5. **Index Wisely**: Balance read performance vs write overhead

## Implementation Details

**Parser** (`src/engine/parser_session.cpp`):
- Routes EXPLAIN statements
- Captures ANALYZE option

**Planner** (`src/engine/query_planner.cpp`):
- Generates logical query plans
- Estimates costs and row counts

**Executor** (`src/engine/executor*.cpp`):
- Instruments nodes for ANALYZE
- Collects runtime statistics

**Output** (`tests/explain_analyze_tests.cpp`):
- Text and JSON formatters
- Node type representations

**Code Anchors**:
- EXPLAIN parser: `src/engine/parser_session.cpp`
- Query planner: `src/engine/query_planner.cpp`
- Executor instrumentation: `src/engine/executor.cpp`
- Test examples: `tests/explain_analyze_tests.cpp`

## See also

- [SELECT Queries](./sql-select.md) - Query construction
- [Indexes](./ddl-indexes.md) - Index types and usage
- [Session & Transaction](./session-and-transaction.md) - SET options for planning
- [Configuration](./configuration.md) - Planner configuration
- [Performance](./missing-and-future.md) - Performance tuning