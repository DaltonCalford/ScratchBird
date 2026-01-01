-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - Covering Indexes (INCLUDE clause)
-- Description: Comprehensive covering index testing with INCLUDE
-- ============================================================================

-- Covering indexes include extra columns for index-only scans
-- INCLUDE clause (PostgreSQL 11+) adds columns without ordering them
-- Best for: queries that fetch indexed + additional columns
-- Benefit: Index-only scans avoid table access, faster queries

-- Create test database
CREATE DATABASE test_covering_idx_db;
USE test_covering_idx_db;

-- ============================================================================
-- Section 1: Basic Covering Index with INCLUDE
-- ============================================================================

CREATE TABLE test_covering_basic (
    id INT PRIMARY KEY,
    user_id INT,
    username VARCHAR(100),
    email VARCHAR(200),
    full_name VARCHAR(200),
    created_at TIMESTAMP
);

-- Index on user_id, include username and email
CREATE INDEX idx_user_covering ON test_covering_basic (user_id) INCLUDE (username, email);

INSERT INTO test_covering_basic
SELECT i,
       (i % 1000) + 1,
       'user' || i,
       'user' || i || '@example.com',
       'User ' || i || ' Name',
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' hours')::INTERVAL
FROM generate_series(1, 10000) i;

-- Index-only scan: user_id (indexed) + username, email (included)
SELECT user_id, username, email FROM test_covering_basic
WHERE user_id = 500;

-- Index-only scan for all user_id 500 records
SELECT username, email FROM test_covering_basic
WHERE user_id = 500
ORDER BY user_id
LIMIT 10;

-- ============================================================================
-- Section 2: Covering Index vs Regular Index
-- ============================================================================

CREATE TABLE test_covering_comparison (
    record_id INT PRIMARY KEY,
    category VARCHAR(50),
    subcategory VARCHAR(50),
    name VARCHAR(200),
    description TEXT,
    price NUMERIC(10,2)
);

-- Regular index (requires table access for name, price)
CREATE INDEX idx_category_regular ON test_covering_comparison (category);

-- Covering index (includes name and price)
CREATE INDEX idx_category_covering ON test_covering_comparison (category) INCLUDE (name, price);

INSERT INTO test_covering_comparison
SELECT i,
       'category_' || (i % 20),
       'subcategory_' || (i % 50),
       'Product ' || i,
       'Description for product ' || i,
       (random() * 500)::NUMERIC(10,2)
FROM generate_series(1, 10000) i;

-- Regular index: must access table for name, price
SELECT category, name, price FROM test_covering_comparison
WHERE category = 'category_5'
LIMIT 10;

-- Covering index: index-only scan (no table access)
-- Query fetches: category (indexed), name, price (included)

-- ============================================================================
-- Section 3: Multicolumn Covering Index
-- ============================================================================

CREATE TABLE test_covering_multicolumn (
    order_id INT PRIMARY KEY,
    customer_id INT,
    order_date DATE,
    order_status VARCHAR(20),
    total_amount NUMERIC(12,2),
    items_count INT
);

-- Index on customer_id and order_date, include status and amount
CREATE INDEX idx_cust_date_covering ON test_covering_multicolumn (customer_id, order_date)
INCLUDE (order_status, total_amount);

INSERT INTO test_covering_multicolumn
SELECT i,
       (i % 500) + 1,
       DATE '2024-01-01' + (i % 365 || ' days')::INTERVAL,
       CASE (i % 4) WHEN 0 THEN 'pending' WHEN 1 THEN 'processing' WHEN 2 THEN 'shipped' ELSE 'delivered' END,
       (random() * 1000)::NUMERIC(12,2),
       (random() * 10)::INT + 1
FROM generate_series(1, 50000) i;

-- Index-only scan
SELECT customer_id, order_date, order_status, total_amount
FROM test_covering_multicolumn
WHERE customer_id = 250 AND order_date >= '2024-06-01'
ORDER BY order_date
LIMIT 10;

-- ============================================================================
-- Section 4: Unique Covering Index
-- ============================================================================

CREATE TABLE test_covering_unique (
    user_id INT PRIMARY KEY,
    email VARCHAR(200),
    username VARCHAR(100),
    full_name VARCHAR(200),
    phone VARCHAR(20),
    created_at TIMESTAMP
);

-- Unique index on email, include username and full_name
CREATE UNIQUE INDEX idx_email_unique_covering ON test_covering_unique (email)
INCLUDE (username, full_name);

INSERT INTO test_covering_unique
SELECT i,
       'user' || i || '@example.com',
       'user' || i,
       'User ' || i || ' Name',
       '555-' || LPAD(i::TEXT, 7, '0'),
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' minutes')::INTERVAL
FROM generate_series(1, 10000) i;

-- Index-only scan with unique lookup
SELECT email, username, full_name FROM test_covering_unique
WHERE email = 'user5000@example.com';

-- ============================================================================
-- Section 5: Covering Index for Aggregations
-- ============================================================================

CREATE TABLE test_covering_aggregations (
    transaction_id BIGSERIAL PRIMARY KEY,
    account_id INT,
    transaction_date DATE,
    amount NUMERIC(12,2),
    transaction_type VARCHAR(20),
    description TEXT
);

-- Index on account_id and date, include amount for aggregations
CREATE INDEX idx_account_date_amount ON test_covering_aggregations (account_id, transaction_date)
INCLUDE (amount, transaction_type);

INSERT INTO test_covering_aggregations (account_id, transaction_date, amount, transaction_type, description)
SELECT (i % 1000) + 1,
       DATE '2024-01-01' + (i % 365 || ' days')::INTERVAL,
       (random() * 5000)::NUMERIC(12,2),
       CASE (i % 3) WHEN 0 THEN 'debit' WHEN 1 THEN 'credit' ELSE 'transfer' END,
       'Transaction ' || i
FROM generate_series(1, 100000) i;

-- Index-only scan for aggregations
SELECT account_id, SUM(amount) AS total, COUNT(*) AS txn_count
FROM test_covering_aggregations
WHERE account_id = 500 AND transaction_date >= '2024-06-01'
GROUP BY account_id;

-- ============================================================================
-- Section 6: Covering Index with ORDER BY
-- ============================================================================

CREATE TABLE test_covering_ordering (
    product_id INT PRIMARY KEY,
    category_id INT,
    product_name VARCHAR(200),
    price NUMERIC(10,2),
    stock_quantity INT,
    last_updated TIMESTAMP
);

-- Index on category, include price and stock for sorted results
CREATE INDEX idx_category_price_covering ON test_covering_ordering (category_id, price DESC)
INCLUDE (product_name, stock_quantity);

INSERT INTO test_covering_ordering
SELECT i,
       (i % 50) + 1,
       'Product ' || i,
       (random() * 1000)::NUMERIC(10,2),
       (random() * 100)::INT,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' hours')::INTERVAL
FROM generate_series(1, 10000) i;

-- Index-only scan with ordering
SELECT category_id, product_name, price, stock_quantity
FROM test_covering_ordering
WHERE category_id = 25
ORDER BY price DESC
LIMIT 10;

-- ============================================================================
-- Section 7: Covering Index for Joins
-- ============================================================================

CREATE TABLE test_covering_orders (
    order_id INT PRIMARY KEY,
    customer_id INT,
    order_total NUMERIC(12,2),
    order_status VARCHAR(20)
);

CREATE TABLE test_covering_customers (
    customer_id INT PRIMARY KEY,
    customer_name VARCHAR(200)
);

-- Covering index for join optimization
CREATE INDEX idx_customer_covering ON test_covering_orders (customer_id)
INCLUDE (order_total, order_status);

INSERT INTO test_covering_customers
SELECT i, 'Customer ' || i FROM generate_series(1, 1000) i;

INSERT INTO test_covering_orders
SELECT i,
       (i % 1000) + 1,
       (random() * 5000)::NUMERIC(12,2),
       CASE (i % 4) WHEN 0 THEN 'pending' WHEN 1 THEN 'shipped' ELSE 'delivered' END
FROM generate_series(1, 50000) i;

-- Join using covering index
SELECT c.customer_name, o.order_total, o.order_status
FROM test_covering_customers c
JOIN test_covering_orders o ON o.customer_id = c.customer_id
WHERE c.customer_id = 500
LIMIT 10;

-- ============================================================================
-- Section 8: Partial Covering Index
-- ============================================================================

CREATE TABLE test_covering_partial (
    log_id BIGSERIAL PRIMARY KEY,
    log_level VARCHAR(20),
    log_timestamp TIMESTAMP,
    log_message TEXT,
    user_id INT,
    request_id VARCHAR(100)
);

-- Partial covering index: only ERROR/WARN logs, include message and user_id
CREATE INDEX idx_error_logs_covering ON test_covering_partial (log_timestamp DESC)
WHERE log_level IN ('ERROR', 'WARN')
INCLUDE (log_message, user_id);

INSERT INTO test_covering_partial (log_level, log_timestamp, log_message, user_id, request_id)
SELECT CASE (i % 5) WHEN 0 THEN 'ERROR' WHEN 1 THEN 'WARN' ELSE 'INFO' END,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' seconds')::INTERVAL,
       'Log message ' || i,
       (i % 100) + 1,
       'req-' || i
FROM generate_series(1, 100000) i;

-- Index-only scan for error logs
SELECT log_timestamp, log_message, user_id FROM test_covering_partial
WHERE log_level = 'ERROR' AND log_timestamp >= '2024-01-10 00:00:00'
ORDER BY log_timestamp DESC
LIMIT 10;

-- ============================================================================
-- Section 9: Covering Index with Expression
-- ============================================================================

CREATE TABLE test_covering_expression (
    user_id INT PRIMARY KEY,
    email VARCHAR(200),
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    status VARCHAR(20)
);

-- Expression index on LOWER(email), include names
CREATE INDEX idx_email_lower_covering ON test_covering_expression (LOWER(email))
INCLUDE (first_name, last_name);

INSERT INTO test_covering_expression
SELECT i,
       'User' || i || '@Example.COM',
       'FirstName' || i,
       'LastName' || i,
       CASE WHEN i % 3 = 0 THEN 'active' ELSE 'inactive' END
FROM generate_series(1, 10000) i;

-- Index-only scan with case-insensitive search
SELECT email, first_name, last_name FROM test_covering_expression
WHERE LOWER(email) = 'user500@example.com';

-- ============================================================================
-- Section 10: Covering Index Size vs Performance
-- ============================================================================

CREATE TABLE test_covering_size (
    id INT PRIMARY KEY,
    search_key INT,
    col1 VARCHAR(100),
    col2 VARCHAR(100),
    col3 VARCHAR(100),
    col4 VARCHAR(100),
    col5 TEXT
);

-- Regular index
CREATE INDEX idx_key_regular ON test_covering_size (search_key);

-- Covering index with 2 columns
CREATE INDEX idx_key_cover2 ON test_covering_size (search_key) INCLUDE (col1, col2);

-- Covering index with 4 columns
CREATE INDEX idx_key_cover4 ON test_covering_size (search_key) INCLUDE (col1, col2, col3, col4);

INSERT INTO test_covering_size
SELECT i,
       (i % 100),
       'Data1-' || i,
       'Data2-' || i,
       'Data3-' || i,
       'Data4-' || i,
       'Large text data ' || REPEAT('x', 100)
FROM generate_series(1, 10000) i;

-- Compare index sizes
SELECT pg_size_pretty(pg_relation_size('idx_key_regular')) AS regular_size,
       pg_size_pretty(pg_relation_size('idx_key_cover2')) AS cover2_size,
       pg_size_pretty(pg_relation_size('idx_key_cover4')) AS cover4_size;

-- More included columns = larger index, but potentially faster queries

-- ============================================================================
-- Section 11: Covering Index for Pagination
-- ============================================================================

CREATE TABLE test_covering_pagination (
    article_id BIGSERIAL PRIMARY KEY,
    category VARCHAR(50),
    published_date TIMESTAMP,
    title VARCHAR(200),
    author VARCHAR(100),
    view_count INT
);

-- Index for pagination: category + published_date (ordered), include title and author
CREATE INDEX idx_category_published_covering ON test_covering_pagination (category, published_date DESC)
INCLUDE (title, author, view_count);

INSERT INTO test_covering_pagination (category, published_date, title, author, view_count)
SELECT 'category_' || (i % 20),
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' hours')::INTERVAL,
       'Article Title ' || i,
       'Author ' || (i % 100),
       (random() * 10000)::INT
FROM generate_series(1, 50000) i;

-- Efficient pagination with index-only scan
SELECT category, published_date, title, author, view_count
FROM test_covering_pagination
WHERE category = 'category_10'
ORDER BY published_date DESC
LIMIT 20 OFFSET 0;

-- Next page
SELECT category, published_date, title, author, view_count
FROM test_covering_pagination
WHERE category = 'category_10'
ORDER BY published_date DESC
LIMIT 20 OFFSET 20;

-- ============================================================================
-- Section 12: Covering Index for Analytics Queries
-- ============================================================================

CREATE TABLE test_covering_analytics (
    event_id BIGSERIAL PRIMARY KEY,
    user_id INT,
    event_type VARCHAR(50),
    event_date DATE,
    metric_value NUMERIC(10,2),
    dimension1 VARCHAR(50),
    dimension2 VARCHAR(50)
);

-- Covering index for analytics: user_id + event_date, include metrics
CREATE INDEX idx_analytics_covering ON test_covering_analytics (user_id, event_date)
INCLUDE (event_type, metric_value, dimension1);

INSERT INTO test_covering_analytics (user_id, event_type, event_date, metric_value, dimension1, dimension2)
SELECT (i % 1000) + 1,
       CASE (i % 5) WHEN 0 THEN 'login' WHEN 1 THEN 'purchase' WHEN 2 THEN 'view' WHEN 3 THEN 'click' ELSE 'logout' END,
       DATE '2024-01-01' + (i % 180 || ' days')::INTERVAL,
       random() * 100,
       'dim1_' || (i % 10),
       'dim2_' || (i % 20)
FROM generate_series(1, 100000) i;

-- Analytics query with index-only scan
SELECT event_date, event_type, SUM(metric_value) AS total_value
FROM test_covering_analytics
WHERE user_id = 500 AND event_date >= '2024-03-01'
GROUP BY event_date, event_type
ORDER BY event_date
LIMIT 20;

-- ============================================================================
-- Section 13: Covering Index with All Query Columns
-- ============================================================================

CREATE TABLE test_covering_complete (
    id INT PRIMARY KEY,
    status VARCHAR(20),
    priority INT,
    assigned_to VARCHAR(100),
    title VARCHAR(200),
    created_date DATE
);

-- Include ALL columns needed by common query
CREATE INDEX idx_status_priority_all ON test_covering_complete (status, priority DESC)
INCLUDE (assigned_to, title, created_date);

INSERT INTO test_covering_complete
SELECT i,
       CASE (i % 4) WHEN 0 THEN 'open' WHEN 1 THEN 'in_progress' WHEN 2 THEN 'review' ELSE 'closed' END,
       (i % 5) + 1,
       'User' || (i % 50),
       'Task ' || i,
       DATE '2024-01-01' + (i % 180 || ' days')::INTERVAL
FROM generate_series(1, 10000) i;

-- Completely covered by index (index-only scan)
SELECT status, priority, assigned_to, title, created_date
FROM test_covering_complete
WHERE status = 'open'
ORDER BY priority DESC
LIMIT 10;

-- ============================================================================
-- Section 14: Covering Index Maintenance
-- ============================================================================

CREATE TABLE test_covering_maintenance (
    id SERIAL PRIMARY KEY,
    lookup_key VARCHAR(100),
    data1 VARCHAR(100),
    data2 VARCHAR(100),
    updated_at TIMESTAMP
);

CREATE INDEX idx_lookup_covering_maint ON test_covering_maintenance (lookup_key)
INCLUDE (data1, data2, updated_at);

INSERT INTO test_covering_maintenance (lookup_key, data1, data2, updated_at)
SELECT 'key_' || i,
       'data1_' || i,
       'data2_' || i,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' hours')::INTERVAL
FROM generate_series(1, 10000) i;

-- Reindex
REINDEX INDEX idx_lookup_covering_maint;

-- Vacuum
VACUUM test_covering_maintenance;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_covering_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_covering_best_practices VALUES
    (1, 'Use INCLUDE to add non-indexed columns to index'),
    (2, 'Enables index-only scans (no table access needed)'),
    (3, 'INCLUDE columns are NOT part of index key structure'),
    (4, 'Cannot ORDER BY or filter on INCLUDE columns using index'),
    (5, 'Perfect for SELECT columns that arent in WHERE/ORDER BY'),
    (6, 'Covering indexes are larger than regular indexes'),
    (7, 'More INCLUDE columns = larger index size'),
    (8, 'Balance: query performance vs index size'),
    (9, 'Great for pagination queries'),
    (10, 'Ideal for analytics and reporting queries'),
    (11, 'Works with UNIQUE, partial, and expression indexes'),
    (12, 'Monitor index usage with pg_stat_user_indexes'),
    (13, 'Include only frequently queried columns'),
    (14, 'Update cost is higher (more data in index)'),
    (15, 'Use EXPLAIN to verify index-only scans');

SELECT id, guideline FROM test_covering_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_covering_best_practices;
DROP TABLE test_covering_maintenance;
DROP TABLE test_covering_complete;
DROP TABLE test_covering_analytics;
DROP TABLE test_covering_pagination;
DROP TABLE test_covering_size;
DROP TABLE test_covering_expression;
DROP TABLE test_covering_partial;
DROP TABLE test_covering_customers;
DROP TABLE test_covering_orders;
DROP TABLE test_covering_ordering;
DROP TABLE test_covering_aggregations;
DROP TABLE test_covering_unique;
DROP TABLE test_covering_multicolumn;
DROP TABLE test_covering_comparison;
DROP TABLE test_covering_basic;

DROP DATABASE test_covering_idx_db;

-- End of covering index tests
