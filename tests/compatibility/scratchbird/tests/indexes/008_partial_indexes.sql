-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - Partial Indexes
-- Description: Comprehensive partial (filtered) index testing
-- ============================================================================

-- Partial indexes index only rows matching a WHERE condition
-- Smaller index size, faster updates, targeted for specific queries
-- Best for: queries with common WHERE filters, sparse data, status flags
-- Note: Query WHERE clause must match or be more restrictive than index WHERE

-- Create test database
CREATE DATABASE test_partial_idx_db;
USE test_partial_idx_db;

-- ============================================================================
-- Section 1: Basic Partial Index
-- ============================================================================

CREATE TABLE test_partial_basic (
    id INT PRIMARY KEY,
    status VARCHAR(20),
    created_date DATE,
    amount NUMERIC(10,2)
);

-- Partial index: only active records
CREATE INDEX idx_active_only ON test_partial_basic (created_date)
WHERE status = 'active';

INSERT INTO test_partial_basic
SELECT i,
       CASE WHEN i % 10 = 0 THEN 'active' ELSE 'inactive' END,
       DATE '2024-01-01' + (i % 365 || ' days')::INTERVAL,
       (random() * 1000)::NUMERIC(10,2)
FROM generate_series(1, 10000) i;

-- Uses partial index (matches WHERE condition)
SELECT id, created_date, amount FROM test_partial_basic
WHERE status = 'active' AND created_date >= '2024-06-01'
LIMIT 10;

-- Does NOT use partial index (different WHERE)
SELECT id, created_date FROM test_partial_basic
WHERE status = 'inactive' AND created_date >= '2024-06-01'
LIMIT 10;

-- ============================================================================
-- Section 2: Partial Index for Pending Orders
-- ============================================================================

CREATE TABLE test_partial_orders (
    order_id INT PRIMARY KEY,
    customer_id INT,
    order_status VARCHAR(20),
    order_date TIMESTAMP,
    total NUMERIC(12,2)
);

-- Only index pending/processing orders (small subset)
CREATE INDEX idx_pending_orders ON test_partial_orders (customer_id, order_date)
WHERE order_status IN ('pending', 'processing');

INSERT INTO test_partial_orders
SELECT i,
       (i % 1000) + 1,
       CASE (i % 10) WHEN 0 THEN 'pending' WHEN 1 THEN 'processing' ELSE 'completed' END,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' hours')::INTERVAL,
       (random() * 500)::NUMERIC(12,2)
FROM generate_series(1, 50000) i;

-- Uses partial index
SELECT order_id, order_date, total FROM test_partial_orders
WHERE order_status = 'pending' AND customer_id = 500
ORDER BY order_date
LIMIT 10;

SELECT order_id FROM test_partial_orders
WHERE order_status = 'processing' AND customer_id = 250
LIMIT 5;

-- Does NOT use partial index (completed orders)
SELECT order_id FROM test_partial_orders
WHERE order_status = 'completed' AND customer_id = 100
LIMIT 5;

-- ============================================================================
-- Section 3: Partial Index for Non-NULL Values
-- ============================================================================

CREATE TABLE test_partial_nonnull (
    record_id INT PRIMARY KEY,
    optional_field VARCHAR(100),
    data TEXT
);

-- Index only rows where optional_field is NOT NULL (sparse column)
CREATE INDEX idx_optional_nonnull ON test_partial_nonnull (optional_field)
WHERE optional_field IS NOT NULL;

INSERT INTO test_partial_nonnull
SELECT i,
       CASE WHEN i % 5 = 0 THEN 'value_' || i ELSE NULL END,
       'data_' || i
FROM generate_series(1, 10000) i;

-- Uses partial index
SELECT record_id, optional_field FROM test_partial_nonnull
WHERE optional_field IS NOT NULL AND optional_field LIKE 'value_100%'
LIMIT 10;

SELECT record_id FROM test_partial_nonnull
WHERE optional_field = 'value_1000';

-- ============================================================================
-- Section 4: Partial Index for Boolean Flags
-- ============================================================================

CREATE TABLE test_partial_boolean (
    user_id INT PRIMARY KEY,
    username VARCHAR(100),
    is_deleted BOOLEAN DEFAULT FALSE,
    is_verified BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP
);

-- Index only non-deleted users (most queries exclude deleted)
CREATE INDEX idx_active_users ON test_partial_boolean (username)
WHERE is_deleted = FALSE;

-- Index only verified users
CREATE INDEX idx_verified_users ON test_partial_boolean (created_at)
WHERE is_verified = TRUE;

INSERT INTO test_partial_boolean
SELECT i,
       'user' || i,
       (i % 100 = 0),  -- 1% deleted
       (i % 10 = 0),   -- 10% verified
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' minutes')::INTERVAL
FROM generate_series(1, 10000) i;

-- Uses idx_active_users
SELECT user_id, username FROM test_partial_boolean
WHERE is_deleted = FALSE AND username LIKE 'user500%'
LIMIT 10;

-- Uses idx_verified_users
SELECT user_id, username, created_at FROM test_partial_boolean
WHERE is_verified = TRUE AND created_at >= '2024-01-10 00:00:00'
LIMIT 10;

-- ============================================================================
-- Section 5: Partial Index with Date Ranges
-- ============================================================================

CREATE TABLE test_partial_recent (
    log_id BIGSERIAL PRIMARY KEY,
    log_timestamp TIMESTAMP,
    log_level VARCHAR(20),
    log_message TEXT
);

-- Index only recent logs (last 90 days)
CREATE INDEX idx_recent_logs ON test_partial_recent (log_timestamp, log_level)
WHERE log_timestamp >= CURRENT_TIMESTAMP - INTERVAL '90 days';

INSERT INTO test_partial_recent (log_timestamp, log_level, log_message)
SELECT TIMESTAMP '2024-01-01 00:00:00' + (i || ' hours')::INTERVAL,
       CASE (i % 4) WHEN 0 THEN 'ERROR' WHEN 1 THEN 'WARN' WHEN 2 THEN 'INFO' ELSE 'DEBUG' END,
       'Log message ' || i
FROM generate_series(1, 10000) i;

-- Uses partial index (recent logs)
SELECT log_id, log_timestamp, log_level FROM test_partial_recent
WHERE log_timestamp >= CURRENT_TIMESTAMP - INTERVAL '30 days'
  AND log_level = 'ERROR'
LIMIT 10;

-- ============================================================================
-- Section 6: Partial UNIQUE Index
-- ============================================================================

CREATE TABLE test_partial_unique (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200),
    is_active BOOLEAN DEFAULT TRUE
);

-- UNIQUE constraint only for active users (allow duplicate emails for inactive)
CREATE UNIQUE INDEX idx_active_email_unique ON test_partial_unique (email)
WHERE is_active = TRUE;

INSERT INTO test_partial_unique (email, is_active) VALUES
    ('alice@example.com', TRUE),
    ('bob@example.com', TRUE),
    ('charlie@example.com', FALSE),
    ('diana@example.com', FALSE);

-- This would fail: duplicate active email
-- INSERT INTO test_partial_unique (email, is_active) VALUES ('alice@example.com', TRUE);

-- This succeeds: duplicate inactive email is allowed
INSERT INTO test_partial_unique (email, is_active) VALUES ('charlie@example.com', FALSE);

SELECT id, email, is_active FROM test_partial_unique ORDER BY id;

-- ============================================================================
-- Section 7: Partial Index on Numeric Ranges
-- ============================================================================

CREATE TABLE test_partial_numeric (
    transaction_id BIGSERIAL PRIMARY KEY,
    account_id INT,
    amount NUMERIC(12,2),
    transaction_type VARCHAR(20)
);

-- Index only large transactions (> $10,000)
CREATE INDEX idx_large_transactions ON test_partial_numeric (account_id, amount DESC)
WHERE amount > 10000;

INSERT INTO test_partial_numeric (account_id, amount, transaction_type)
SELECT (i % 1000) + 1,
       (random() * 50000)::NUMERIC(12,2),
       CASE (i % 3) WHEN 0 THEN 'debit' WHEN 1 THEN 'credit' ELSE 'transfer' END
FROM generate_series(1, 100000) i;

-- Uses partial index
SELECT transaction_id, amount FROM test_partial_numeric
WHERE account_id = 500 AND amount > 10000
ORDER BY amount DESC
LIMIT 10;

SELECT transaction_id, amount FROM test_partial_numeric
WHERE amount > 25000
ORDER BY amount DESC
LIMIT 20;

-- ============================================================================
-- Section 8: Partial Index for Error/Exception Tables
-- ============================================================================

CREATE TABLE test_partial_errors (
    error_id BIGSERIAL PRIMARY KEY,
    error_timestamp TIMESTAMP,
    error_code VARCHAR(50),
    is_resolved BOOLEAN DEFAULT FALSE,
    error_details TEXT
);

-- Index only unresolved errors
CREATE INDEX idx_unresolved_errors ON test_partial_errors (error_timestamp DESC)
WHERE is_resolved = FALSE;

INSERT INTO test_partial_errors (error_timestamp, error_code, is_resolved, error_details)
SELECT TIMESTAMP '2024-01-01 00:00:00' + (i || ' minutes')::INTERVAL,
       'ERR_' || ((i % 20) + 100),
       (i % 5 = 0),  -- 20% resolved
       'Error details for incident ' || i
FROM generate_series(1, 10000) i;

-- Find recent unresolved errors
SELECT error_id, error_timestamp, error_code FROM test_partial_errors
WHERE is_resolved = FALSE AND error_timestamp >= '2024-01-10 00:00:00'
ORDER BY error_timestamp DESC
LIMIT 10;

-- ============================================================================
-- Section 9: Partial Index with Multiple Conditions
-- ============================================================================

CREATE TABLE test_partial_multi_condition (
    id INT PRIMARY KEY,
    category VARCHAR(50),
    status VARCHAR(20),
    priority INT,
    created_date DATE
);

-- Partial index: high priority AND pending status
CREATE INDEX idx_high_priority_pending ON test_partial_multi_condition (created_date, category)
WHERE priority >= 8 AND status = 'pending';

INSERT INTO test_partial_multi_condition
SELECT i,
       'category_' || (i % 10),
       CASE (i % 5) WHEN 0 THEN 'pending' WHEN 1 THEN 'in_progress' ELSE 'completed' END,
       (i % 10) + 1,
       DATE '2024-01-01' + (i % 180 || ' days')::INTERVAL
FROM generate_series(1, 10000) i;

-- Uses partial index
SELECT id, category, priority FROM test_partial_multi_condition
WHERE priority >= 8 AND status = 'pending' AND created_date >= '2024-03-01'
LIMIT 10;

-- ============================================================================
-- Section 10: Partial Index for Soft Deletes
-- ============================================================================

CREATE TABLE test_partial_soft_delete (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    category VARCHAR(100),
    deleted_at TIMESTAMP,
    price NUMERIC(10,2)
);

-- Index only non-deleted products
CREATE INDEX idx_available_products ON test_partial_soft_delete (category, product_name)
WHERE deleted_at IS NULL;

INSERT INTO test_partial_soft_delete
SELECT i,
       'Product ' || i,
       'Category ' || (i % 20),
       CASE WHEN i % 50 = 0 THEN TIMESTAMP '2024-01-01 00:00:00' + (i || ' hours')::INTERVAL ELSE NULL END,
       (random() * 500)::NUMERIC(10,2)
FROM generate_series(1, 10000) i;

-- Query available (non-deleted) products
SELECT product_id, product_name, price FROM test_partial_soft_delete
WHERE deleted_at IS NULL AND category = 'Category 10'
ORDER BY product_name
LIMIT 10;

-- ============================================================================
-- Section 11: Partial Index Size Comparison
-- ============================================================================

CREATE TABLE test_partial_size_comparison (
    id INT PRIMARY KEY,
    status VARCHAR(20),
    data TEXT
);

-- Full index on status
CREATE INDEX idx_status_full ON test_partial_size_comparison (status);

-- Partial index: only 'active' (10% of data)
CREATE INDEX idx_status_partial ON test_partial_size_comparison (status)
WHERE status = 'active';

INSERT INTO test_partial_size_comparison
SELECT i,
       CASE WHEN i % 10 = 0 THEN 'active' ELSE 'inactive' END,
       'Data row ' || i
FROM generate_series(1, 100000) i;

-- Compare sizes
SELECT pg_size_pretty(pg_relation_size('idx_status_full')) AS full_index_size,
       pg_size_pretty(pg_relation_size('idx_status_partial')) AS partial_index_size;

-- Partial index is ~90% smaller

-- ============================================================================
-- Section 12: Partial Index for Time-Based Archiving
-- ============================================================================

CREATE TABLE test_partial_archive (
    record_id BIGSERIAL PRIMARY KEY,
    record_date DATE,
    is_archived BOOLEAN DEFAULT FALSE,
    record_data TEXT
);

-- Index only non-archived records
CREATE INDEX idx_active_records ON test_partial_archive (record_date DESC)
WHERE is_archived = FALSE;

INSERT INTO test_partial_archive (record_date, is_archived, record_data)
SELECT DATE '2024-01-01' + (i % 365 || ' days')::INTERVAL,
       (i % 100 < 20),  -- 20% archived
       'Record data ' || i
FROM generate_series(1, 50000) i;

-- Query active (non-archived) records
SELECT record_id, record_date FROM test_partial_archive
WHERE is_archived = FALSE AND record_date >= '2024-06-01'
ORDER BY record_date DESC
LIMIT 10;

-- ============================================================================
-- Section 13: Partial Index with Expression
-- ============================================================================

CREATE TABLE test_partial_expression (
    id INT PRIMARY KEY,
    email VARCHAR(200),
    status VARCHAR(20)
);

-- Partial index on LOWER(email) for active users only
CREATE INDEX idx_active_email_lower ON test_partial_expression (LOWER(email))
WHERE status = 'active';

INSERT INTO test_partial_expression
SELECT i,
       'User' || i || '@Example.COM',
       CASE WHEN i % 3 = 0 THEN 'active' ELSE 'suspended' END
FROM generate_series(1, 10000) i;

-- Uses partial expression index
SELECT id, email FROM test_partial_expression
WHERE status = 'active' AND LOWER(email) = 'user500@example.com';

-- ============================================================================
-- Section 14: Partial Index for Premium Features
-- ============================================================================

CREATE TABLE test_partial_subscriptions (
    subscription_id INT PRIMARY KEY,
    user_id INT,
    plan_type VARCHAR(20),
    subscription_date DATE,
    expiry_date DATE
);

-- Index only premium/enterprise plans (minority of users)
CREATE INDEX idx_premium_users ON test_partial_subscriptions (user_id, expiry_date)
WHERE plan_type IN ('premium', 'enterprise');

INSERT INTO test_partial_subscriptions
SELECT i,
       i,
       CASE (i % 10) WHEN 0 THEN 'premium' WHEN 1 THEN 'enterprise' ELSE 'basic' END,
       DATE '2024-01-01' + (i % 180 || ' days')::INTERVAL,
       DATE '2024-06-01' + (i % 365 || ' days')::INTERVAL
FROM generate_series(1, 10000) i;

-- Query premium subscriptions
SELECT subscription_id, user_id, plan_type, expiry_date FROM test_partial_subscriptions
WHERE plan_type = 'premium' AND expiry_date > CURRENT_DATE
ORDER BY expiry_date
LIMIT 10;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_partial_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_partial_best_practices VALUES
    (1, 'Use partial indexes for queries filtering on specific values'),
    (2, 'Ideal for sparse columns (e.g., IS NOT NULL on mostly NULL column)'),
    (3, 'Perfect for status flags (index only active/pending, skip completed)'),
    (4, 'Query WHERE must match or be stricter than index WHERE'),
    (5, 'Partial indexes are smaller = faster updates, less storage'),
    (6, 'Great for soft deletes (index only deleted_at IS NULL)'),
    (7, 'Use for boolean flags when TRUE/FALSE distribution is skewed'),
    (8, 'Combine with UNIQUE for conditional uniqueness constraints'),
    (9, 'Index recent data only for time-based queries'),
    (10, 'Partial indexes reduce index maintenance overhead'),
    (11, 'Multiple partial indexes can cover different query patterns'),
    (12, 'Use for high-value transactions (amount > threshold)'),
    (13, 'Perfect for error tracking (index only unresolved errors)'),
    (14, 'Can combine multiple conditions in WHERE clause'),
    (15, 'Monitor index usage - remove if not used by queries');

SELECT id, guideline FROM test_partial_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_partial_best_practices;
DROP TABLE test_partial_subscriptions;
DROP TABLE test_partial_expression;
DROP TABLE test_partial_archive;
DROP TABLE test_partial_size_comparison;
DROP TABLE test_partial_soft_delete;
DROP TABLE test_partial_multi_condition;
DROP TABLE test_partial_errors;
DROP TABLE test_partial_numeric;
DROP TABLE test_partial_unique;
DROP TABLE test_partial_recent;
DROP TABLE test_partial_boolean;
DROP TABLE test_partial_nonnull;
DROP TABLE test_partial_orders;
DROP TABLE test_partial_basic;

DROP DATABASE test_partial_idx_db;

-- End of partial index tests
