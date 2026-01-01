-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - Multicolumn Indexes
-- Description: Comprehensive multicolumn (composite) index testing
-- ============================================================================

-- Multicolumn indexes combine multiple columns into a single index
-- Column order matters: leftmost columns must be used for index to apply
-- Best for: queries filtering/sorting on multiple columns together
-- Note: Can be used for queries on leading columns only

-- Create test database
CREATE DATABASE test_multicolumn_idx_db;
USE test_multicolumn_idx_db;

-- ============================================================================
-- Section 1: Basic Multicolumn Index
-- ============================================================================

CREATE TABLE test_multicolumn_basic (
    id INT PRIMARY KEY,
    customer_id INT,
    order_date DATE,
    status VARCHAR(20),
    amount NUMERIC(10,2)
);

-- Multicolumn index: customer_id, order_date
CREATE INDEX idx_customer_date ON test_multicolumn_basic (customer_id, order_date);

INSERT INTO test_multicolumn_basic
SELECT i,
       (i % 100) + 1,
       DATE '2024-01-01' + (i % 365 || ' days')::INTERVAL,
       CASE (i % 3) WHEN 0 THEN 'pending' WHEN 1 THEN 'completed' ELSE 'shipped' END,
       (random() * 1000)::NUMERIC(10,2)
FROM generate_series(1, 10000) i;

-- Uses index: filters on both columns
SELECT id, order_date, amount FROM test_multicolumn_basic
WHERE customer_id = 50 AND order_date >= '2024-06-01'
ORDER BY order_date
LIMIT 10;

-- Uses index: filters on first column only
SELECT id, order_date FROM test_multicolumn_basic
WHERE customer_id = 25
LIMIT 10;

-- Does NOT use index efficiently: filters on second column only
SELECT id, customer_id FROM test_multicolumn_basic
WHERE order_date = '2024-03-15'
LIMIT 10;

-- ============================================================================
-- Section 2: Column Order Matters
-- ============================================================================

CREATE TABLE test_multicolumn_order (
    id INT PRIMARY KEY,
    state VARCHAR(2),
    city VARCHAR(100),
    zip_code VARCHAR(10),
    address TEXT
);

-- Index 1: state, city, zip_code (good for state-first queries)
CREATE INDEX idx_state_city_zip ON test_multicolumn_order (state, city, zip_code);

-- Index 2: zip_code, state (good for zip-first queries)
CREATE INDEX idx_zip_state ON test_multicolumn_order (zip_code, state);

INSERT INTO test_multicolumn_order
SELECT i,
       CASE (i % 5) WHEN 0 THEN 'CA' WHEN 1 THEN 'NY' WHEN 2 THEN 'TX' WHEN 3 THEN 'FL' ELSE 'WA' END,
       'City' || (i % 20),
       LPAD((10000 + i % 90000)::TEXT, 5, '0'),
       'Address ' || i
FROM generate_series(1, 10000) i;

-- Uses idx_state_city_zip: starts with state
SELECT id, city, zip_code FROM test_multicolumn_order
WHERE state = 'CA' AND city = 'City5'
LIMIT 10;

-- Uses idx_zip_state: starts with zip_code
SELECT id, state FROM test_multicolumn_order
WHERE zip_code = '10500'
LIMIT 10;

-- Uses idx_state_city_zip: state and city
SELECT id FROM test_multicolumn_order
WHERE state = 'NY' AND city = 'City10'
LIMIT 10;

-- Uses idx_state_city_zip: all three columns
SELECT id FROM test_multicolumn_order
WHERE state = 'TX' AND city = 'City8' AND zip_code = '12345';

-- ============================================================================
-- Section 3: Multicolumn with Different Data Types
-- ============================================================================

CREATE TABLE test_multicolumn_types (
    record_id INT PRIMARY KEY,
    user_id INT,
    created_timestamp TIMESTAMP,
    category VARCHAR(50),
    is_active BOOLEAN,
    priority INT
);

CREATE INDEX idx_user_time_cat ON test_multicolumn_types (user_id, created_timestamp, category);
CREATE INDEX idx_cat_active_priority ON test_multicolumn_types (category, is_active, priority);

INSERT INTO test_multicolumn_types
SELECT i,
       (i % 1000) + 1,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' minutes')::INTERVAL,
       'category_' || (i % 10),
       (i % 2 = 0),
       (i % 5) + 1
FROM generate_series(1, 50000) i;

-- Uses idx_user_time_cat
SELECT record_id, created_timestamp FROM test_multicolumn_types
WHERE user_id = 500 AND created_timestamp >= '2024-01-15 00:00:00'
ORDER BY created_timestamp
LIMIT 10;

-- Uses idx_cat_active_priority
SELECT record_id, priority FROM test_multicolumn_types
WHERE category = 'category_5' AND is_active = true AND priority >= 3
LIMIT 10;

-- ============================================================================
-- Section 4: Three-Column Index Best Practices
-- ============================================================================

CREATE TABLE test_multicolumn_three (
    id SERIAL PRIMARY KEY,
    department_id INT,
    project_id INT,
    task_date DATE,
    task_status VARCHAR(20)
);

-- Three-column index
CREATE INDEX idx_dept_proj_date ON test_multicolumn_three (department_id, project_id, task_date);

INSERT INTO test_multicolumn_three (department_id, project_id, task_date, task_status)
SELECT (i % 10) + 1,
       (i % 50) + 1,
       DATE '2024-01-01' + (i % 180 || ' days')::INTERVAL,
       CASE (i % 4) WHEN 0 THEN 'todo' WHEN 1 THEN 'in_progress' WHEN 2 THEN 'done' ELSE 'blocked' END
FROM generate_series(1, 10000) i;

-- Uses index: all three columns
SELECT id, task_date, task_status FROM test_multicolumn_three
WHERE department_id = 5 AND project_id = 25 AND task_date >= '2024-03-01'
LIMIT 10;

-- Uses index: first two columns
SELECT id, task_date FROM test_multicolumn_three
WHERE department_id = 3 AND project_id = 10
LIMIT 10;

-- Uses index: first column only
SELECT COUNT(*) FROM test_multicolumn_three
WHERE department_id = 7;

-- Does NOT use index efficiently: skips first column
SELECT id FROM test_multicolumn_three
WHERE project_id = 20 AND task_date = '2024-04-15'
LIMIT 5;

-- ============================================================================
-- Section 5: Multicolumn Index with ORDER BY
-- ============================================================================

CREATE TABLE test_multicolumn_ordering (
    id INT PRIMARY KEY,
    account_id INT,
    transaction_time TIMESTAMP,
    amount NUMERIC(10,2)
);

-- Index matching query order
CREATE INDEX idx_acct_time_desc ON test_multicolumn_ordering (account_id, transaction_time DESC);

INSERT INTO test_multicolumn_ordering
SELECT i,
       (i % 500) + 1,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' seconds')::INTERVAL,
       (random() * 5000)::NUMERIC(10,2)
FROM generate_series(1, 100000) i;

-- Index perfectly matches: account_id WHERE, transaction_time DESC ORDER
SELECT id, transaction_time, amount FROM test_multicolumn_ordering
WHERE account_id = 250
ORDER BY transaction_time DESC
LIMIT 10;

-- Get most recent transactions
SELECT id, transaction_time, amount FROM test_multicolumn_ordering
WHERE account_id = 100
ORDER BY transaction_time DESC
LIMIT 5;

-- ============================================================================
-- Section 6: Covering Multicolumn Index (Index-Only Scan)
-- ============================================================================

CREATE TABLE test_multicolumn_covering (
    product_id INT PRIMARY KEY,
    category_id INT,
    subcategory_id INT,
    product_name VARCHAR(200),
    price NUMERIC(10,2),
    stock_quantity INT
);

-- Multicolumn index including extra columns for covering
CREATE INDEX idx_cat_subcat_covering ON test_multicolumn_covering (category_id, subcategory_id) INCLUDE (price, stock_quantity);

INSERT INTO test_multicolumn_covering
SELECT i,
       (i % 20) + 1,
       (i % 100) + 1,
       'Product ' || i,
       (random() * 500)::NUMERIC(10,2),
       (random() * 100)::INT
FROM generate_series(1, 10000) i;

-- Index-only scan: all needed columns in index
SELECT category_id, subcategory_id, price, stock_quantity
FROM test_multicolumn_covering
WHERE category_id = 10 AND subcategory_id = 50;

-- ============================================================================
-- Section 7: Multicolumn UNIQUE Index
-- ============================================================================

CREATE TABLE test_multicolumn_unique (
    reservation_id SERIAL PRIMARY KEY,
    room_id INT,
    check_in_date DATE,
    check_out_date DATE,
    guest_name VARCHAR(100)
);

-- Unique multicolumn index: one reservation per room per check-in date
CREATE UNIQUE INDEX idx_room_checkin_unique ON test_multicolumn_unique (room_id, check_in_date);

INSERT INTO test_multicolumn_unique (room_id, check_in_date, check_out_date, guest_name) VALUES
    (101, '2024-01-15', '2024-01-17', 'Alice Smith'),
    (101, '2024-01-18', '2024-01-20', 'Bob Jones'),
    (102, '2024-01-15', '2024-01-16', 'Charlie Brown'),
    (103, '2024-01-15', '2024-01-19', 'Diana Prince');

-- This would fail: duplicate (room_id, check_in_date)
-- INSERT INTO test_multicolumn_unique (room_id, check_in_date, check_out_date, guest_name)
-- VALUES (101, '2024-01-15', '2024-01-20', 'Eve Williams');

SELECT reservation_id, room_id, check_in_date, guest_name FROM test_multicolumn_unique
WHERE room_id = 101
ORDER BY check_in_date;

-- ============================================================================
-- Section 8: Four-Column Index (Maximum Practicality)
-- ============================================================================

CREATE TABLE test_multicolumn_four (
    id SERIAL PRIMARY KEY,
    region VARCHAR(50),
    country VARCHAR(50),
    state VARCHAR(50),
    city VARCHAR(100),
    population INT
);

-- Four-column index (rarely needed, but supported)
CREATE INDEX idx_region_country_state_city ON test_multicolumn_four (region, country, state, city);

INSERT INTO test_multicolumn_four (region, country, state, city, population)
SELECT 'Region' || (i % 3),
       'Country' || (i % 10),
       'State' || (i % 20),
       'City' || i,
       (random() * 1000000)::INT
FROM generate_series(1, 1000) i;

-- Uses all four columns
SELECT id, city, population FROM test_multicolumn_four
WHERE region = 'Region1' AND country = 'Country5' AND state = 'State10' AND city = 'City500';

-- Uses first three columns
SELECT id, city FROM test_multicolumn_four
WHERE region = 'Region2' AND country = 'Country3' AND state = 'State7'
LIMIT 5;

-- ============================================================================
-- Section 9: Multicolumn vs Multiple Single-Column Indexes
-- ============================================================================

CREATE TABLE test_multicolumn_vs_single (
    id INT PRIMARY KEY,
    col_a INT,
    col_b INT,
    col_c VARCHAR(50)
);

-- Multicolumn index (better for queries using both columns)
CREATE INDEX idx_a_b_multi ON test_multicolumn_vs_single (col_a, col_b);

-- Separate single-column indexes (can be bitmap combined, but less efficient)
CREATE INDEX idx_a_single ON test_multicolumn_vs_single (col_a);
CREATE INDEX idx_b_single ON test_multicolumn_vs_single (col_b);

INSERT INTO test_multicolumn_vs_single
SELECT i, (i % 100), (i % 50), 'value' || (i % 20)
FROM generate_series(1, 10000) i;

-- Multicolumn index is better for this
SELECT id FROM test_multicolumn_vs_single WHERE col_a = 50 AND col_b = 25;

-- Either single index works
SELECT id FROM test_multicolumn_vs_single WHERE col_a = 30 LIMIT 5;

-- ============================================================================
-- Section 10: Multicolumn with Partial Index
-- ============================================================================

CREATE TABLE test_multicolumn_partial (
    order_id INT PRIMARY KEY,
    customer_id INT,
    order_status VARCHAR(20),
    order_date DATE,
    total NUMERIC(10,2)
);

-- Multicolumn partial index: only for pending/processing orders
CREATE INDEX idx_cust_date_active ON test_multicolumn_partial (customer_id, order_date)
WHERE order_status IN ('pending', 'processing');

INSERT INTO test_multicolumn_partial
SELECT i,
       (i % 500) + 1,
       CASE (i % 5) WHEN 0 THEN 'pending' WHEN 1 THEN 'processing' WHEN 2 THEN 'completed' WHEN 3 THEN 'shipped' ELSE 'cancelled' END,
       DATE '2024-01-01' + (i % 180 || ' days')::INTERVAL,
       (random() * 1000)::NUMERIC(10,2)
FROM generate_series(1, 10000) i;

-- Uses partial index (status in pending/processing)
SELECT order_id, order_date, total FROM test_multicolumn_partial
WHERE customer_id = 250 AND order_status = 'pending'
ORDER BY order_date
LIMIT 10;

-- ============================================================================
-- Section 11: Multicolumn Index for Joins
-- ============================================================================

CREATE TABLE test_multicolumn_orders (
    order_id INT PRIMARY KEY,
    customer_id INT,
    product_id INT,
    order_timestamp TIMESTAMP
);

CREATE TABLE test_multicolumn_customers (
    customer_id INT PRIMARY KEY,
    customer_name VARCHAR(100)
);

-- Multicolumn index for join optimization
CREATE INDEX idx_cust_prod ON test_multicolumn_orders (customer_id, product_id);

INSERT INTO test_multicolumn_customers
SELECT i, 'Customer ' || i FROM generate_series(1, 1000) i;

INSERT INTO test_multicolumn_orders
SELECT i,
       (i % 1000) + 1,
       (i % 100) + 1,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' minutes')::INTERVAL
FROM generate_series(1, 50000) i;

-- Join using multicolumn index
SELECT c.customer_name, o.order_id, o.product_id
FROM test_multicolumn_customers c
JOIN test_multicolumn_orders o ON o.customer_id = c.customer_id
WHERE c.customer_id = 500 AND o.product_id = 75
LIMIT 10;

-- ============================================================================
-- Section 12: Multicolumn Index Statistics
-- ============================================================================

CREATE TABLE test_multicolumn_stats (
    id INT PRIMARY KEY,
    col1 INT,
    col2 INT,
    col3 INT
);

CREATE INDEX idx_col1_col2_col3 ON test_multicolumn_stats (col1, col2, col3);

INSERT INTO test_multicolumn_stats
SELECT i, (i % 10), (i % 20), (i % 30)
FROM generate_series(1, 10000) i;

-- Analyze to update statistics
ANALYZE test_multicolumn_stats;

-- Query using index
SELECT id FROM test_multicolumn_stats
WHERE col1 = 5 AND col2 = 10 AND col3 = 15;

-- ============================================================================
-- Section 13: Real-World Example - Event Logging
-- ============================================================================

CREATE TABLE test_multicolumn_event_log (
    event_id BIGSERIAL PRIMARY KEY,
    application_id INT,
    event_type VARCHAR(50),
    event_timestamp TIMESTAMP,
    severity VARCHAR(20),
    event_message TEXT
);

-- Multicolumn index for common log queries
CREATE INDEX idx_app_type_time ON test_multicolumn_event_log (application_id, event_type, event_timestamp DESC);
CREATE INDEX idx_app_severity_time ON test_multicolumn_event_log (application_id, severity, event_timestamp DESC);

INSERT INTO test_multicolumn_event_log (application_id, event_type, event_timestamp, severity, event_message)
SELECT (i % 20) + 1,
       CASE (i % 5) WHEN 0 THEN 'login' WHEN 1 THEN 'logout' WHEN 2 THEN 'error' WHEN 3 THEN 'warning' ELSE 'info' END,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' seconds')::INTERVAL,
       CASE (i % 4) WHEN 0 THEN 'critical' WHEN 1 THEN 'error' WHEN 2 THEN 'warning' ELSE 'info' END,
       'Event message ' || i
FROM generate_series(1, 100000) i;

-- Query by app and event type
SELECT event_id, event_timestamp, severity FROM test_multicolumn_event_log
WHERE application_id = 10 AND event_type = 'error'
ORDER BY event_timestamp DESC
LIMIT 20;

-- Query by app and severity
SELECT event_id, event_type, event_timestamp FROM test_multicolumn_event_log
WHERE application_id = 5 AND severity = 'critical'
ORDER BY event_timestamp DESC
LIMIT 10;

-- ============================================================================
-- Section 14: Index Size Comparison
-- ============================================================================

CREATE TABLE test_multicolumn_size (
    id INT PRIMARY KEY,
    a INT,
    b INT,
    c INT,
    d INT
);

CREATE INDEX idx_single_a ON test_multicolumn_size (a);
CREATE INDEX idx_single_b ON test_multicolumn_size (b);
CREATE INDEX idx_multi_ab ON test_multicolumn_size (a, b);
CREATE INDEX idx_multi_abc ON test_multicolumn_size (a, b, c);
CREATE INDEX idx_multi_abcd ON test_multicolumn_size (a, b, c, d);

INSERT INTO test_multicolumn_size
SELECT i, i % 100, i % 200, i % 300, i % 400
FROM generate_series(1, 10000) i;

-- Compare index sizes
SELECT pg_size_pretty(pg_relation_size('idx_single_a')) AS single_a_size,
       pg_size_pretty(pg_relation_size('idx_single_b')) AS single_b_size,
       pg_size_pretty(pg_relation_size('idx_multi_ab')) AS multi_ab_size,
       pg_size_pretty(pg_relation_size('idx_multi_abc')) AS multi_abc_size,
       pg_size_pretty(pg_relation_size('idx_multi_abcd')) AS multi_abcd_size;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_multicolumn_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_multicolumn_best_practices VALUES
    (1, 'Column order matters: most selective columns first (usually)'),
    (2, 'Index can be used for queries on leading columns only'),
    (3, 'WHERE col1=X uses index (col1, col2)'),
    (4, 'WHERE col2=X does NOT use index (col1, col2) efficiently'),
    (5, 'Maximum practical columns in index: 3-4'),
    (6, 'More columns = larger index size'),
    (7, 'Use INCLUDE for covering indexes (PostgreSQL 11+)'),
    (8, 'Match ORDER BY column order to index for sorted results'),
    (9, 'Consider DESC option for descending sort optimization'),
    (10, 'Multicolumn indexes support UNIQUE constraints'),
    (11, 'Combine with WHERE clause for partial multicolumn indexes'),
    (12, 'One good multicolumn index > multiple single-column indexes'),
    (13, 'Analyze query patterns before deciding column order'),
    (14, 'Use EXPLAIN to verify index usage'),
    (15, 'Update statistics with ANALYZE after bulk inserts');

SELECT id, guideline FROM test_multicolumn_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_multicolumn_best_practices;
DROP TABLE test_multicolumn_size;
DROP TABLE test_multicolumn_event_log;
DROP TABLE test_multicolumn_stats;
DROP TABLE test_multicolumn_customers;
DROP TABLE test_multicolumn_orders;
DROP TABLE test_multicolumn_partial;
DROP TABLE test_multicolumn_vs_single;
DROP TABLE test_multicolumn_four;
DROP TABLE test_multicolumn_unique;
DROP TABLE test_multicolumn_covering;
DROP TABLE test_multicolumn_ordering;
DROP TABLE test_multicolumn_three;
DROP TABLE test_multicolumn_types;
DROP TABLE test_multicolumn_order;
DROP TABLE test_multicolumn_basic;

DROP DATABASE test_multicolumn_idx_db;

-- End of multicolumn index tests
