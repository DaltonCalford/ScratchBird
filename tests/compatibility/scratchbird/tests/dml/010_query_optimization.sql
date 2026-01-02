-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - Query Optimization and Performance
-- Description: Query optimization techniques and performance patterns
-- ============================================================================

-- Query Optimization: Performance tuning and best practices
-- EXPLAIN, indexes, query rewriting
-- Execution plans, statistics, optimization hints
-- Performance monitoring and troubleshooting

-- Create test database
CREATE DATABASE test_query_optimization_db;
USE test_query_optimization_db;

-- ============================================================================
-- Section 1: EXPLAIN and EXPLAIN ANALYZE
-- ============================================================================

CREATE TABLE test_customers (
    customer_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    email VARCHAR(200),
    city VARCHAR(100),
    signup_date DATE
);

-- Generate test data
INSERT INTO test_customers (customer_name, email, city, signup_date)
SELECT
    'Customer ' || i,
    'customer' || i || '@example.com',
    CASE (i % 5)
        WHEN 0 THEN 'New York'
        WHEN 1 THEN 'Los Angeles'
        WHEN 2 THEN 'Chicago'
        WHEN 3 THEN 'Houston'
        ELSE 'Phoenix'
    END,
    CURRENT_DATE - (i || ' days')::INTERVAL
FROM generate_series(1, 10000) i;

-- Basic EXPLAIN
EXPLAIN
SELECT * FROM test_customers WHERE city = 'New York';

-- EXPLAIN ANALYZE (actually executes)
EXPLAIN ANALYZE
SELECT * FROM test_customers WHERE city = 'New York';

-- EXPLAIN with all options
EXPLAIN (ANALYZE, BUFFERS, VERBOSE, COSTS, TIMING)
SELECT customer_name, email
FROM test_customers
WHERE city = 'New York'
  AND signup_date > CURRENT_DATE - INTERVAL '30 days'
ORDER BY signup_date DESC
LIMIT 100;

-- ============================================================================
-- Section 2: Index Impact on Performance
-- ============================================================================

-- Query without index (sequential scan)
EXPLAIN ANALYZE
SELECT * FROM test_customers WHERE city = 'Chicago';

-- Create index
CREATE INDEX idx_customers_city ON test_customers(city);

-- Same query with index (index scan)
EXPLAIN ANALYZE
SELECT * FROM test_customers WHERE city = 'Chicago';

-- Composite index
CREATE INDEX idx_customers_city_date ON test_customers(city, signup_date);

-- Query benefiting from composite index
EXPLAIN ANALYZE
SELECT * FROM test_customers
WHERE city = 'Houston'
  AND signup_date > '2024-01-01'
ORDER BY signup_date DESC;

-- ============================================================================
-- Section 3: Covering Indexes
-- ============================================================================

-- Query requiring table access
EXPLAIN ANALYZE
SELECT customer_id, customer_name, city
FROM test_customers
WHERE city = 'Los Angeles';

-- Create covering index (includes all needed columns)
CREATE INDEX idx_customers_city_covering ON test_customers(city) INCLUDE (customer_id, customer_name);

-- Query now using index-only scan
EXPLAIN ANALYZE
SELECT customer_id, customer_name, city
FROM test_customers
WHERE city = 'Los Angeles';

-- ============================================================================
-- Section 4: Partial Indexes
-- ============================================================================

-- Partial index (only indexes subset of rows)
CREATE INDEX idx_customers_recent ON test_customers(signup_date)
WHERE signup_date > '2024-01-01';

-- Query matching partial index condition
EXPLAIN ANALYZE
SELECT * FROM test_customers
WHERE signup_date > '2024-01-01'
ORDER BY signup_date DESC;

-- ============================================================================
-- Section 5: Expression Indexes
-- ============================================================================

-- Query with function on column (cannot use regular index)
EXPLAIN ANALYZE
SELECT * FROM test_customers WHERE LOWER(email) = 'customer100@example.com';

-- Create expression index
CREATE INDEX idx_customers_email_lower ON test_customers(LOWER(email));

-- Query now uses expression index
EXPLAIN ANALYZE
SELECT * FROM test_customers WHERE LOWER(email) = 'customer100@example.com';

-- ============================================================================
-- Section 6: JOIN Optimization
-- ============================================================================

CREATE TABLE test_orders (
    order_id SERIAL PRIMARY KEY,
    customer_id INT,
    order_date DATE,
    total NUMERIC(10,2)
);

-- Generate orders
INSERT INTO test_orders (customer_id, order_date, total)
SELECT
    (random() * 9999 + 1)::INT,
    CURRENT_DATE - (random() * 365)::INT,
    (random() * 1000)::NUMERIC(10,2)
FROM generate_series(1, 50000);

-- JOIN without foreign key index
EXPLAIN ANALYZE
SELECT c.customer_name, o.order_date, o.total
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id
WHERE c.city = 'New York'
LIMIT 100;

-- Create index on foreign key
CREATE INDEX idx_orders_customer_id ON test_orders(customer_id);

-- JOIN with index (faster)
EXPLAIN ANALYZE
SELECT c.customer_name, o.order_date, o.total
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id
WHERE c.city = 'New York'
LIMIT 100;

-- ============================================================================
-- Section 7: Subquery vs JOIN Performance
-- ============================================================================

-- Correlated subquery (potentially slow)
EXPLAIN ANALYZE
SELECT
    c.customer_name,
    (SELECT COUNT(*) FROM test_orders o WHERE o.customer_id = c.customer_id) AS order_count
FROM test_customers c
WHERE c.city = 'Phoenix'
LIMIT 100;

-- Rewritten as JOIN (typically faster)
EXPLAIN ANALYZE
SELECT
    c.customer_name,
    COUNT(o.order_id) AS order_count
FROM test_customers c
LEFT JOIN test_orders o ON c.customer_id = o.customer_id
WHERE c.city = 'Phoenix'
GROUP BY c.customer_id, c.customer_name
LIMIT 100;

-- ============================================================================
-- Section 8: EXISTS vs IN Performance
-- ============================================================================

-- IN with subquery
EXPLAIN ANALYZE
SELECT customer_name
FROM test_customers
WHERE customer_id IN (
    SELECT customer_id FROM test_orders WHERE order_date > '2024-01-01'
)
LIMIT 100;

-- EXISTS (often more efficient)
EXPLAIN ANALYZE
SELECT customer_name
FROM test_customers c
WHERE EXISTS (
    SELECT 1 FROM test_orders o
    WHERE o.customer_id = c.customer_id
      AND o.order_date > '2024-01-01'
)
LIMIT 100;

-- ============================================================================
-- Section 9: LIMIT and Pagination Optimization
-- ============================================================================

-- Efficient pagination with LIMIT/OFFSET (small offset)
EXPLAIN ANALYZE
SELECT * FROM test_customers
ORDER BY customer_id
LIMIT 100 OFFSET 0;

-- Less efficient pagination (large offset)
EXPLAIN ANALYZE
SELECT * FROM test_customers
ORDER BY customer_id
LIMIT 100 OFFSET 9000;

-- Keyset pagination (more efficient for large offsets)
EXPLAIN ANALYZE
SELECT * FROM test_customers
WHERE customer_id > 9000
ORDER BY customer_id
LIMIT 100;

-- ============================================================================
-- Section 10: Aggregate Optimization
-- ============================================================================

-- Aggregates with GROUP BY
EXPLAIN ANALYZE
SELECT city, COUNT(*) AS customer_count
FROM test_customers
GROUP BY city;

-- Index for GROUP BY
CREATE INDEX idx_customers_city_only ON test_customers(city);

-- Aggregates using index
EXPLAIN ANALYZE
SELECT city, COUNT(*) AS customer_count
FROM test_customers
GROUP BY city;

-- Filtered aggregates
EXPLAIN ANALYZE
SELECT
    COUNT(*) FILTER (WHERE city = 'New York') AS ny_count,
    COUNT(*) FILTER (WHERE city = 'Los Angeles') AS la_count,
    COUNT(*) AS total_count
FROM test_customers;

-- ============================================================================
-- Section 11: Window Function Optimization
-- ============================================================================

-- Create index for window function
CREATE INDEX idx_customers_city_signup ON test_customers(city, signup_date DESC);

-- Window function using index
EXPLAIN ANALYZE
SELECT
    customer_name,
    city,
    signup_date,
    ROW_NUMBER() OVER (PARTITION BY city ORDER BY signup_date DESC) AS row_num
FROM test_customers
WHERE city IN ('New York', 'Chicago')
LIMIT 1000;

-- ============================================================================
-- Section 12: CTE Optimization (Materialized vs Non-Materialized)
-- ============================================================================

-- Materialized CTE (PostgreSQL 12+)
EXPLAIN ANALYZE
WITH recent_customers AS MATERIALIZED (
    SELECT customer_id, customer_name, city
    FROM test_customers
    WHERE signup_date > '2024-01-01'
)
SELECT c.*, COUNT(o.order_id) AS order_count
FROM recent_customers c
LEFT JOIN test_orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name, c.city
LIMIT 100;

-- Non-materialized CTE (can be inlined)
EXPLAIN ANALYZE
WITH recent_customers AS NOT MATERIALIZED (
    SELECT customer_id, customer_name, city
    FROM test_customers
    WHERE signup_date > '2024-01-01'
)
SELECT c.*, COUNT(o.order_id) AS order_count
FROM recent_customers c
LEFT JOIN test_orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name, c.city
LIMIT 100;

-- ============================================================================
-- Section 13: DISTINCT Optimization
-- ============================================================================

-- DISTINCT (may require sort)
EXPLAIN ANALYZE
SELECT DISTINCT city FROM test_customers;

-- GROUP BY alternative (can use index)
EXPLAIN ANALYZE
SELECT city FROM test_customers GROUP BY city;

-- DISTINCT ON (PostgreSQL specific)
EXPLAIN ANALYZE
SELECT DISTINCT ON (city) city, customer_name
FROM test_customers
ORDER BY city, signup_date DESC;

-- ============================================================================
-- Section 14: OR vs UNION Optimization
-- ============================================================================

-- OR condition (may not use index efficiently)
EXPLAIN ANALYZE
SELECT * FROM test_customers
WHERE city = 'New York' OR city = 'Los Angeles'
LIMIT 100;

-- IN clause (usually better than OR)
EXPLAIN ANALYZE
SELECT * FROM test_customers
WHERE city IN ('New York', 'Los Angeles')
LIMIT 100;

-- UNION ALL (can use index for each branch)
EXPLAIN ANALYZE
SELECT * FROM test_customers WHERE city = 'New York'
UNION ALL
SELECT * FROM test_customers WHERE city = 'Los Angeles'
LIMIT 100;

-- ============================================================================
-- Section 15: Function Calls in WHERE Clause
-- ============================================================================

-- Function on indexed column (prevents index use)
EXPLAIN ANALYZE
SELECT * FROM test_customers
WHERE EXTRACT(YEAR FROM signup_date) = 2024
LIMIT 100;

-- Rewritten without function (can use index)
EXPLAIN ANALYZE
SELECT * FROM test_customers
WHERE signup_date >= '2024-01-01' AND signup_date < '2025-01-01'
LIMIT 100;

-- ============================================================================
-- Section 16: Index-Only Scans
-- ============================================================================

-- Create index with included columns
CREATE INDEX idx_customers_city_include_all ON test_customers(city)
INCLUDE (customer_id, customer_name, email);

-- Index-only scan (all data from index)
EXPLAIN ANALYZE
SELECT customer_id, customer_name, email
FROM test_customers
WHERE city = 'Houston';

-- VACUUM for visibility map (enables index-only scans)
VACUUM ANALYZE test_customers;

-- ============================================================================
-- Section 17: Parallel Query Execution
-- ============================================================================

-- Query that may use parallel workers
EXPLAIN ANALYZE
SELECT city, COUNT(*) AS customer_count, AVG(customer_id) AS avg_id
FROM test_customers
GROUP BY city;

-- Control parallel workers
SET max_parallel_workers_per_gather = 4;

EXPLAIN ANALYZE
SELECT city, COUNT(*) AS customer_count
FROM test_customers
GROUP BY city;

-- Reset
RESET max_parallel_workers_per_gather;

-- ============================================================================
-- Section 18: Statistics and ANALYZE
-- ============================================================================

-- View table statistics
SELECT
    schemaname,
    tablename,
    n_live_tup AS live_tuples,
    n_dead_tup AS dead_tuples,
    last_vacuum,
    last_autovacuum,
    last_analyze,
    last_autoanalyze
FROM pg_stat_user_tables
WHERE tablename IN ('test_customers', 'test_orders');

-- Update statistics
ANALYZE test_customers;
ANALYZE test_orders;

-- View column statistics
SELECT
    tablename,
    attname AS column_name,
    n_distinct,
    correlation
FROM pg_stats
WHERE tablename = 'test_customers'
ORDER BY attname;

-- ============================================================================
-- Section 19: Query Plan Node Types
-- ============================================================================

-- Sequential Scan
EXPLAIN
SELECT * FROM test_customers WHERE customer_name LIKE '%Test%';

-- Index Scan
EXPLAIN
SELECT * FROM test_customers WHERE city = 'New York';

-- Index Only Scan
EXPLAIN
SELECT city FROM test_customers WHERE city = 'Chicago';

-- Bitmap Index Scan (for multiple matching rows)
EXPLAIN
SELECT * FROM test_customers WHERE city IN ('New York', 'Los Angeles', 'Chicago');

-- Nested Loop Join
EXPLAIN
SELECT c.customer_name, o.total
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id
WHERE c.customer_id < 10;

-- Hash Join
EXPLAIN
SELECT c.customer_name, o.total
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id;

-- Merge Join (requires sorted input)
EXPLAIN
SELECT c.customer_id, o.order_id
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id
ORDER BY c.customer_id;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_optimization_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_optimization_best_practices VALUES
    (1, 'Use EXPLAIN ANALYZE to understand query execution'),
    (2, 'Index foreign key columns for JOIN performance'),
    (3, 'Create covering indexes for frequently accessed columns'),
    (4, 'Use partial indexes for filtered queries'),
    (5, 'Index columns used in WHERE, JOIN, ORDER BY'),
    (6, 'Avoid functions on indexed columns in WHERE'),
    (7, 'EXISTS often faster than IN for large result sets'),
    (8, 'Keyset pagination better than OFFSET for large offsets'),
    (9, 'Rewrite correlated subqueries as JOINs when possible'),
    (10, 'Use LIMIT to reduce result set size'),
    (11, 'Regular VACUUM and ANALYZE maintain statistics'),
    (12, 'Monitor pg_stat_user_tables for table health'),
    (13, 'Index-only scans fastest (all data from index)'),
    (14, 'Bitmap scans efficient for moderate selectivity'),
    (15, 'Hash joins good for large equijoins'),
    (16, 'Nested loop joins good for small result sets'),
    (17, 'Parallel queries leverage multiple CPU cores'),
    (18, 'Materialized CTEs can improve multi-reference queries'),
    (19, 'GROUP BY can be faster than DISTINCT'),
    (20, 'Monitor slow queries with pg_stat_statements extension');

SELECT id, guideline FROM test_optimization_best_practices ORDER BY id;

-- ============================================================================
-- Performance Monitoring Queries
-- ============================================================================

-- Slow queries (requires pg_stat_statements extension)
-- SELECT
--     query,
--     calls,
--     total_exec_time,
--     mean_exec_time,
--     max_exec_time
-- FROM pg_stat_statements
-- ORDER BY mean_exec_time DESC
-- LIMIT 10;

-- Table sizes
SELECT
    schemaname,
    tablename,
    pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename)) AS total_size,
    pg_size_pretty(pg_relation_size(schemaname||'.'||tablename)) AS table_size,
    pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename) -
                   pg_relation_size(schemaname||'.'||tablename)) AS indexes_size
FROM pg_tables
WHERE schemaname NOT IN ('pg_catalog', 'information_schema')
ORDER BY pg_total_relation_size(schemaname||'.'||tablename) DESC;

-- Index usage statistics
SELECT
    schemaname,
    tablename,
    indexname,
    idx_scan AS index_scans,
    idx_tup_read AS tuples_read,
    idx_tup_fetch AS tuples_fetched
FROM pg_stat_user_indexes
WHERE schemaname NOT IN ('pg_catalog', 'information_schema')
ORDER BY idx_scan DESC;

-- Unused indexes (candidates for removal)
SELECT
    schemaname,
    tablename,
    indexname,
    idx_scan
FROM pg_stat_user_indexes
WHERE idx_scan = 0
  AND indexrelname NOT LIKE '%_pkey'
ORDER BY schemaname, tablename, indexname;

-- Cache hit ratio (should be > 90%)
SELECT
    sum(heap_blks_read) AS heap_read,
    sum(heap_blks_hit) AS heap_hit,
    sum(heap_blks_hit) / NULLIF((sum(heap_blks_hit) + sum(heap_blks_read)), 0) * 100 AS cache_hit_ratio
FROM pg_statio_user_tables;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_optimization_best_practices;
DROP TABLE test_orders;
DROP TABLE test_customers;

DROP DATABASE test_query_optimization_db;

-- End of query optimization tests
