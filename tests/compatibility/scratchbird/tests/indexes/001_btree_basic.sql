-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - B-tree Basic
-- Description: Comprehensive B-tree index testing (basic operations)
-- ============================================================================

-- B-tree is the default index type in PostgreSQL
-- Supports: <, <=, =, >=, >, BETWEEN, IN, IS NULL, IS NOT NULL
-- Used for: most equality and range queries
-- Best for: high cardinality columns, sorted data access

-- Create test database
CREATE DATABASE test_btree_basic_db;
USE test_btree_basic_db;

-- ============================================================================
-- Section 1: Basic B-tree Index Creation
-- ============================================================================

CREATE TABLE test_btree_simple (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    age INT,
    email VARCHAR(200)
);

-- Create B-tree indexes (USING BTREE is default, can be omitted)
CREATE INDEX idx_name ON test_btree_simple (name);
CREATE INDEX idx_age USING BTREE ON test_btree_simple (age);
CREATE INDEX idx_email ON test_btree_simple (email);

-- Insert test data
INSERT INTO test_btree_simple VALUES
    (1, 'Alice Johnson', 28, 'alice@example.com'),
    (2, 'Bob Smith', 35, 'bob@example.com'),
    (3, 'Charlie Brown', 42, 'charlie@example.com'),
    (4, 'Diana Prince', 30, 'diana@example.com'),
    (5, 'Eve Anderson', 25, 'eve@example.com');

-- Equality queries (uses index)
SELECT id, name FROM test_btree_simple WHERE name = 'Alice Johnson';
SELECT id, age FROM test_btree_simple WHERE age = 30;

-- Range queries (uses index)
SELECT id, name, age FROM test_btree_simple WHERE age > 30 ORDER BY age;
SELECT id, name FROM test_btree_simple WHERE age BETWEEN 25 AND 35 ORDER BY age;

-- ============================================================================
-- Section 2: B-tree with Different Data Types
-- ============================================================================

CREATE TABLE test_btree_types (
    id INT PRIMARY KEY,
    int_col INT,
    bigint_col BIGINT,
    decimal_col DECIMAL(10, 2),
    text_col TEXT,
    varchar_col VARCHAR(100),
    date_col DATE,
    timestamp_col TIMESTAMP,
    boolean_col BOOLEAN
);

-- Create indexes on different types
CREATE INDEX idx_int ON test_btree_types (int_col);
CREATE INDEX idx_bigint ON test_btree_types (bigint_col);
CREATE INDEX idx_decimal ON test_btree_types (decimal_col);
CREATE INDEX idx_text ON test_btree_types (text_col);
CREATE INDEX idx_varchar ON test_btree_types (varchar_col);
CREATE INDEX idx_date ON test_btree_types (date_col);
CREATE INDEX idx_timestamp ON test_btree_types (timestamp_col);
CREATE INDEX idx_boolean ON test_btree_types (boolean_col);

INSERT INTO test_btree_types VALUES
    (1, 100, 1000000, 99.99, 'Sample text', 'Varchar data', '2024-01-15', '2024-01-15 10:00:00', true),
    (2, 200, 2000000, 199.99, 'Another sample', 'More varchar', '2024-02-15', '2024-02-15 11:00:00', false),
    (3, 150, 1500000, 149.99, 'Text content', 'Varchar content', '2024-01-20', '2024-01-20 12:00:00', true);

-- Query each indexed column
SELECT id FROM test_btree_types WHERE int_col = 100;
SELECT id FROM test_btree_types WHERE decimal_col > 100.00 ORDER BY decimal_col;
SELECT id FROM test_btree_types WHERE date_col >= '2024-01-15' ORDER BY date_col;
SELECT id FROM test_btree_types WHERE boolean_col = true;

-- ============================================================================
-- Section 3: Unique B-tree Indexes
-- ============================================================================

CREATE TABLE test_btree_unique (
    user_id INT PRIMARY KEY,
    username VARCHAR(50),
    email VARCHAR(200),
    phone VARCHAR(20)
);

-- Unique indexes enforce uniqueness
CREATE UNIQUE INDEX idx_unique_username ON test_btree_unique (username);
CREATE UNIQUE INDEX idx_unique_email ON test_btree_unique (email);

INSERT INTO test_btree_unique VALUES
    (1, 'alice', 'alice@example.com', '555-0001'),
    (2, 'bob', 'bob@example.com', '555-0002'),
    (3, 'charlie', 'charlie@example.com', '555-0003');

-- This would fail: duplicate username
-- INSERT INTO test_btree_unique VALUES (4, 'alice', 'alice2@example.com', '555-0004');

-- Unique constraint queries
SELECT user_id, username FROM test_btree_unique WHERE username = 'bob';
SELECT user_id, email FROM test_btree_unique WHERE email = 'charlie@example.com';

-- ============================================================================
-- Section 4: Multicolumn B-tree Indexes
-- ============================================================================

CREATE TABLE test_btree_multicolumn (
    order_id INT PRIMARY KEY,
    customer_id INT,
    order_date DATE,
    status VARCHAR(20),
    total_amount DECIMAL(10, 2)
);

-- Multicolumn index (order matters!)
CREATE INDEX idx_customer_date ON test_btree_multicolumn (customer_id, order_date);
CREATE INDEX idx_status_date ON test_btree_multicolumn (status, order_date);

INSERT INTO test_btree_multicolumn VALUES
    (1, 100, '2024-01-15', 'completed', 99.99),
    (2, 100, '2024-01-20', 'pending', 149.99),
    (3, 101, '2024-01-18', 'completed', 199.99),
    (4, 100, '2024-02-01', 'completed', 299.99),
    (5, 102, '2024-01-25', 'cancelled', 79.99);

-- Uses idx_customer_date (both columns)
SELECT order_id, order_date FROM test_btree_multicolumn
WHERE customer_id = 100 AND order_date > '2024-01-15'
ORDER BY order_date;

-- Uses idx_customer_date (first column only)
SELECT order_id FROM test_btree_multicolumn
WHERE customer_id = 100
ORDER BY order_date;

-- Uses idx_status_date
SELECT order_id FROM test_btree_multicolumn
WHERE status = 'completed'
ORDER BY order_date;

-- ============================================================================
-- Section 5: NULL Handling in B-tree Indexes
-- ============================================================================

CREATE TABLE test_btree_nulls (
    id INT PRIMARY KEY,
    nullable_col VARCHAR(100),
    required_col VARCHAR(100) NOT NULL
);

CREATE INDEX idx_nullable ON test_btree_nulls (nullable_col);
CREATE INDEX idx_required ON test_btree_nulls (required_col);

INSERT INTO test_btree_nulls VALUES
    (1, 'value1', 'required1'),
    (2, NULL, 'required2'),
    (3, 'value3', 'required3'),
    (4, NULL, 'required4');

-- B-tree indexes store NULLs
SELECT id FROM test_btree_nulls WHERE nullable_col IS NULL ORDER BY id;
SELECT id FROM test_btree_nulls WHERE nullable_col IS NOT NULL ORDER BY id;

-- ============================================================================
-- Section 6: Sorting with B-tree Indexes
-- ============================================================================

CREATE TABLE test_btree_sorting (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    price DECIMAL(10, 2),
    stock_quantity INT
);

-- Index for sorting
CREATE INDEX idx_price_asc ON test_btree_sorting (price ASC);
CREATE INDEX idx_price_desc ON test_btree_sorting (price DESC);

INSERT INTO test_btree_sorting VALUES
    (1, 'Widget A', 29.99, 100),
    (2, 'Widget B', 49.99, 50),
    (3, 'Widget C', 19.99, 200),
    (4, 'Widget D', 99.99, 25),
    (5, 'Widget E', 39.99, 75);

-- Uses idx_price_asc for ascending sort
SELECT product_id, product_name, price FROM test_btree_sorting
ORDER BY price ASC;

-- Uses idx_price_desc for descending sort
SELECT product_id, product_name, price FROM test_btree_sorting
ORDER BY price DESC;

-- ============================================================================
-- Section 7: LIKE and Pattern Matching
-- ============================================================================

CREATE TABLE test_btree_like (
    id INT PRIMARY KEY,
    code VARCHAR(50),
    description TEXT
);

-- B-tree index helps with left-anchored LIKE
CREATE INDEX idx_code ON test_btree_like (code);

INSERT INTO test_btree_like VALUES
    (1, 'ABC-001', 'First item'),
    (2, 'ABC-002', 'Second item'),
    (3, 'XYZ-001', 'Third item'),
    (4, 'ABC-003', 'Fourth item'),
    (5, 'XYZ-002', 'Fifth item');

-- Uses index (left-anchored)
SELECT id, code FROM test_btree_like WHERE code LIKE 'ABC%' ORDER BY code;

-- Cannot use index efficiently (not left-anchored)
SELECT id, code FROM test_btree_like WHERE code LIKE '%002' ORDER BY code;

-- ============================================================================
-- Section 8: IN and NOT IN Queries
-- ============================================================================

CREATE TABLE test_btree_in (
    id INT PRIMARY KEY,
    category VARCHAR(50),
    value INT
);

CREATE INDEX idx_category ON test_btree_in (category);
CREATE INDEX idx_value ON test_btree_in (value);

INSERT INTO test_btree_in VALUES
    (1, 'A', 10),
    (2, 'B', 20),
    (3, 'C', 30),
    (4, 'A', 15),
    (5, 'B', 25);

-- IN uses index
SELECT id, category FROM test_btree_in
WHERE category IN ('A', 'B')
ORDER BY id;

SELECT id, value FROM test_btree_in
WHERE value IN (10, 20, 30)
ORDER BY value;

-- NOT IN uses index
SELECT id, category FROM test_btree_in
WHERE category NOT IN ('C')
ORDER BY id;

-- ============================================================================
-- Section 9: Index on Foreign Keys
-- ============================================================================

CREATE TABLE test_btree_customers (
    customer_id INT PRIMARY KEY,
    customer_name VARCHAR(100)
);

CREATE TABLE test_btree_orders (
    order_id INT PRIMARY KEY,
    customer_id INT,
    order_date DATE,
    FOREIGN KEY (customer_id) REFERENCES test_btree_customers(customer_id)
);

-- Index on foreign key for join performance
CREATE INDEX idx_orders_customer ON test_btree_orders (customer_id);

INSERT INTO test_btree_customers VALUES
    (1, 'Customer A'),
    (2, 'Customer B'),
    (3, 'Customer C');

INSERT INTO test_btree_orders VALUES
    (101, 1, '2024-01-15'),
    (102, 1, '2024-01-20'),
    (103, 2, '2024-01-18'),
    (104, 3, '2024-01-22');

-- Join uses index on customer_id
SELECT c.customer_name, o.order_id, o.order_date
FROM test_btree_customers c
JOIN test_btree_orders o ON c.customer_id = o.customer_id
WHERE c.customer_id = 1
ORDER BY o.order_date;

-- ============================================================================
-- Section 10: Index Statistics and Monitoring
-- ============================================================================

CREATE TABLE test_btree_stats (
    id INT PRIMARY KEY,
    data VARCHAR(100)
);

CREATE INDEX idx_data ON test_btree_stats (data);

-- Insert data
INSERT INTO test_btree_stats
SELECT i, 'data_' || i
FROM generate_series(1, 1000) i;

-- Query using index
SELECT id FROM test_btree_stats WHERE data = 'data_500';
SELECT id FROM test_btree_stats WHERE data > 'data_900' ORDER BY data LIMIT 10;

-- Index size
SELECT pg_size_pretty(pg_relation_size('idx_data')) AS index_size;

-- ============================================================================
-- Section 11: Covering Indexes (Index-Only Scans)
-- ============================================================================

CREATE TABLE test_btree_covering (
    user_id INT PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200),
    last_login TIMESTAMP
);

-- Index that covers the query (includes all needed columns)
CREATE INDEX idx_username_email ON test_btree_covering (username, email);

INSERT INTO test_btree_covering VALUES
    (1, 'alice', 'alice@example.com', '2024-01-15 10:00:00'),
    (2, 'bob', 'bob@example.com', '2024-01-16 11:00:00'),
    (3, 'charlie', 'charlie@example.com', '2024-01-17 12:00:00');

-- Index-only scan (all columns in index)
SELECT username, email FROM test_btree_covering WHERE username = 'alice';

-- ============================================================================
-- Section 12: Index Maintenance - REINDEX
-- ============================================================================

CREATE TABLE test_btree_maintenance (
    id INT PRIMARY KEY,
    value VARCHAR(100)
);

CREATE INDEX idx_value_maint ON test_btree_maintenance (value);

INSERT INTO test_btree_maintenance
SELECT i, 'value_' || i FROM generate_series(1, 100) i;

-- Reindex to rebuild index
REINDEX INDEX idx_value_maint;
REINDEX TABLE test_btree_maintenance;

-- ============================================================================
-- Section 13: Conditional B-tree Indexes (Partial Indexes)
-- ============================================================================

CREATE TABLE test_btree_partial (
    transaction_id INT PRIMARY KEY,
    amount DECIMAL(10, 2),
    status VARCHAR(20),
    created_at TIMESTAMP
);

-- Index only active transactions
CREATE INDEX idx_active_transactions ON test_btree_partial (created_at)
WHERE status = 'active';

INSERT INTO test_btree_partial VALUES
    (1, 100.00, 'active', '2024-01-15 10:00:00'),
    (2, 200.00, 'completed', '2024-01-16 11:00:00'),
    (3, 150.00, 'active', '2024-01-17 12:00:00'),
    (4, 300.00, 'cancelled', '2024-01-18 13:00:00');

-- Uses partial index
SELECT transaction_id, amount FROM test_btree_partial
WHERE status = 'active' AND created_at > '2024-01-16'
ORDER BY created_at;

-- ============================================================================
-- Section 14: Expression Indexes
-- ============================================================================

CREATE TABLE test_btree_expression (
    id INT PRIMARY KEY,
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    email VARCHAR(200)
);

-- Index on expression (lowercase email)
CREATE INDEX idx_email_lower ON test_btree_expression (LOWER(email));

INSERT INTO test_btree_expression VALUES
    (1, 'Alice', 'Johnson', 'Alice.Johnson@Example.COM'),
    (2, 'Bob', 'Smith', 'BOB.SMITH@example.com'),
    (3, 'Charlie', 'Brown', 'charlie.brown@EXAMPLE.com');

-- Uses expression index
SELECT id, email FROM test_btree_expression
WHERE LOWER(email) = 'alice.johnson@example.com';

-- ============================================================================
-- Section 15: Index Performance Comparison
-- ============================================================================

CREATE TABLE test_btree_performance (
    id INT PRIMARY KEY,
    indexed_col INT,
    non_indexed_col INT
);

CREATE INDEX idx_indexed ON test_btree_performance (indexed_col);

-- Insert test data
INSERT INTO test_btree_performance
SELECT i, i % 100, i % 1000
FROM generate_series(1, 10000) i;

-- Query with index (fast)
SELECT id FROM test_btree_performance WHERE indexed_col = 50 LIMIT 10;

-- Query without index (slower)
SELECT id FROM test_btree_performance WHERE non_indexed_col = 500 LIMIT 10;

-- ============================================================================
-- Section 16: B-tree Index Bloat
-- ============================================================================

CREATE TABLE test_btree_bloat (
    id INT PRIMARY KEY,
    data VARCHAR(100)
);

CREATE INDEX idx_data_bloat ON test_btree_bloat (data);

-- Insert and delete to create bloat
INSERT INTO test_btree_bloat SELECT i, 'data_' || i FROM generate_series(1, 1000) i;
DELETE FROM test_btree_bloat WHERE id % 2 = 0;
INSERT INTO test_btree_bloat SELECT i, 'data_' || i FROM generate_series(1001, 2000) i;

-- VACUUM to reduce bloat
VACUUM test_btree_bloat;

-- ============================================================================
-- Section 17: Best Practices
-- ============================================================================

CREATE TABLE test_btree_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_btree_best_practices VALUES
    (1, 'B-tree is default index type, optimal for most use cases'),
    (2, 'Use for equality (=) and range (<, <=, >, >=, BETWEEN) queries'),
    (3, 'Create indexes on frequently queried columns'),
    (4, 'Index foreign key columns for join performance'),
    (5, 'Multicolumn indexes: order matters (most selective first)'),
    (6, 'Use UNIQUE indexes to enforce uniqueness constraints'),
    (7, 'B-tree supports sorting (ORDER BY uses index)'),
    (8, 'Partial indexes for conditional queries (saves space)'),
    (9, 'Expression indexes for queries on transformed columns'),
    (10, 'Left-anchored LIKE (col LIKE ''prefix%'') can use index'),
    (11, 'Index-only scans avoid table access (include all needed columns)'),
    (12, 'Too many indexes slow down INSERT/UPDATE/DELETE'),
    (13, 'Monitor index usage with pg_stat_user_indexes'),
    (14, 'REINDEX periodically to reduce bloat'),
    (15, 'Consider index size vs query performance trade-off');

SELECT id, guideline FROM test_btree_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_btree_best_practices;
DROP TABLE test_btree_bloat;
DROP TABLE test_btree_performance;
DROP TABLE test_btree_expression;
DROP TABLE test_btree_partial;
DROP TABLE test_btree_maintenance;
DROP TABLE test_btree_covering;
DROP TABLE test_btree_stats;
DROP TABLE test_btree_orders;
DROP TABLE test_btree_customers;
DROP TABLE test_btree_in;
DROP TABLE test_btree_like;
DROP TABLE test_btree_sorting;
DROP TABLE test_btree_nulls;
DROP TABLE test_btree_multicolumn;
DROP TABLE test_btree_unique;
DROP TABLE test_btree_types;
DROP TABLE test_btree_simple;

DROP DATABASE test_btree_basic_db;

-- End of B-tree basic index tests
