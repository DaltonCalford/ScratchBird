-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Sequences - Basic Sequences
-- Description: Comprehensive sequence testing
-- ============================================================================

-- Sequences: Database objects that generate unique numeric values
-- Used for auto-incrementing IDs
-- Support START, INCREMENT, MINVALUE, MAXVALUE, CYCLE options
-- Thread-safe and efficient

-- Create test database
CREATE DATABASE test_basic_sequences_db;
USE test_basic_sequences_db;

-- ============================================================================
-- Section 1: Creating Basic Sequence
-- ============================================================================

CREATE SEQUENCE test_seq_basic;

-- Get next value
SELECT nextval('test_seq_basic') AS next_val;
SELECT nextval('test_seq_basic') AS next_val;
SELECT nextval('test_seq_basic') AS next_val;

-- Get current value (last value returned by nextval)
SELECT currval('test_seq_basic') AS current_val;

-- Peek at next value without consuming it
SELECT last_value FROM test_seq_basic;

-- ============================================================================
-- Section 2: Sequence with START Value
-- ============================================================================

CREATE SEQUENCE test_seq_start START WITH 100;

SELECT nextval('test_seq_start') AS next_val;  -- Returns 100
SELECT nextval('test_seq_start') AS next_val;  -- Returns 101
SELECT nextval('test_seq_start') AS next_val;  -- Returns 102

SELECT currval('test_seq_start') AS current_val;

-- ============================================================================
-- Section 3: Sequence with INCREMENT BY
-- ============================================================================

CREATE SEQUENCE test_seq_increment INCREMENT BY 5;

SELECT nextval('test_seq_increment') AS next_val;  -- Returns 1
SELECT nextval('test_seq_increment') AS next_val;  -- Returns 6
SELECT nextval('test_seq_increment') AS next_val;  -- Returns 11
SELECT nextval('test_seq_increment') AS next_val;  -- Returns 16

SELECT currval('test_seq_increment') AS current_val;

-- ============================================================================
-- Section 4: Sequence with MINVALUE and MAXVALUE
-- ============================================================================

CREATE SEQUENCE test_seq_bounds
    START WITH 1
    INCREMENT BY 1
    MINVALUE 1
    MAXVALUE 10;

-- Generate values up to max
SELECT nextval('test_seq_bounds') AS val FROM generate_series(1, 10);

-- Next value would exceed MAXVALUE (would fail)
-- SELECT nextval('test_seq_bounds');  -- ERROR: reached maximum value

-- ============================================================================
-- Section 5: Sequence with CYCLE
-- ============================================================================

CREATE SEQUENCE test_seq_cycle
    START WITH 1
    INCREMENT BY 1
    MINVALUE 1
    MAXVALUE 5
    CYCLE;

-- Generate values that cycle
SELECT nextval('test_seq_cycle') AS val FROM generate_series(1, 12);
-- Returns: 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2

-- ============================================================================
-- Section 6: Descending Sequence
-- ============================================================================

CREATE SEQUENCE test_seq_descending
    START WITH 100
    INCREMENT BY -1
    MINVALUE 1
    MAXVALUE 100;

SELECT nextval('test_seq_descending') AS val;  -- Returns 100
SELECT nextval('test_seq_descending') AS val;  -- Returns 99
SELECT nextval('test_seq_descending') AS val;  -- Returns 98

SELECT currval('test_seq_descending') AS current_val;

-- ============================================================================
-- Section 7: Sequence with CACHE
-- ============================================================================

CREATE SEQUENCE test_seq_cache
    START WITH 1
    INCREMENT BY 1
    CACHE 20;  -- Pre-allocate 20 values for performance

SELECT nextval('test_seq_cache') AS val FROM generate_series(1, 5);

-- Query sequence metadata
SELECT
    sequencename,
    last_value,
    start_value,
    increment_by,
    max_value,
    min_value,
    cache_value,
    cycle
FROM pg_sequences
WHERE sequencename = 'test_seq_cache';

-- ============================================================================
-- Section 8: Using Sequence in Table
-- ============================================================================

CREATE SEQUENCE test_user_id_seq;

CREATE TABLE test_users (
    user_id INT PRIMARY KEY DEFAULT nextval('test_user_id_seq'),
    username VARCHAR(100) NOT NULL
);

-- Bind sequence to table (for automatic cleanup)
ALTER SEQUENCE test_user_id_seq OWNED BY test_users.user_id;

INSERT INTO test_users (username) VALUES ('alice'), ('bob'), ('charlie');

SELECT user_id, username FROM test_users ORDER BY user_id;

-- ============================================================================
-- Section 9: Multiple Sequences
-- ============================================================================

CREATE SEQUENCE test_order_seq START WITH 1000;
CREATE SEQUENCE test_invoice_seq START WITH 5000;

CREATE TABLE test_orders (
    order_id INT PRIMARY KEY DEFAULT nextval('test_order_seq'),
    invoice_id INT DEFAULT nextval('test_invoice_seq'),
    description TEXT
);

ALTER SEQUENCE test_order_seq OWNED BY test_orders.order_id;
ALTER SEQUENCE test_invoice_seq OWNED BY test_orders.invoice_id;

INSERT INTO test_orders (description) VALUES ('Order 1'), ('Order 2');

SELECT order_id, invoice_id, description FROM test_orders ORDER BY order_id;

-- ============================================================================
-- Section 10: setval() Function
-- ============================================================================

CREATE SEQUENCE test_seq_setval;

-- Set current value (next nextval will return value+1)
SELECT setval('test_seq_setval', 50);

SELECT nextval('test_seq_setval') AS next_val;  -- Returns 51

-- Set current value with is_called parameter
SELECT setval('test_seq_setval', 100, true);  -- Next returns 101
SELECT nextval('test_seq_setval') AS next_val;

SELECT setval('test_seq_setval', 200, false);  -- Next returns 200
SELECT nextval('test_seq_setval') AS next_val;

-- ============================================================================
-- Section 11: Resetting Sequence
-- ============================================================================

CREATE SEQUENCE test_seq_reset START WITH 1;

SELECT nextval('test_seq_reset') FROM generate_series(1, 5);

-- Reset to start value
ALTER SEQUENCE test_seq_reset RESTART;

SELECT nextval('test_seq_reset') AS after_reset;  -- Returns 1

-- Reset to specific value
ALTER SEQUENCE test_seq_reset RESTART WITH 100;

SELECT nextval('test_seq_reset') AS after_restart;  -- Returns 100

-- ============================================================================
-- Section 12: ALTER SEQUENCE
-- ============================================================================

CREATE SEQUENCE test_seq_alter START WITH 1 INCREMENT BY 1;

SELECT nextval('test_seq_alter') FROM generate_series(1, 3);

-- Change increment
ALTER SEQUENCE test_seq_alter INCREMENT BY 5;

SELECT nextval('test_seq_alter') AS next_val;  -- Returns 4 + 5 = 9

-- Change max value
ALTER SEQUENCE test_seq_alter MAXVALUE 50;

-- Change to cycle
ALTER SEQUENCE test_seq_alter CYCLE;

-- ============================================================================
-- Section 13: Sequence Ownership
-- ============================================================================

CREATE SEQUENCE test_seq_owned;

CREATE TABLE test_items (
    item_id INT PRIMARY KEY DEFAULT nextval('test_seq_owned'),
    item_name VARCHAR(200)
);

-- Before ownership
SELECT pg_get_serial_sequence('test_items', 'item_id');  -- Returns NULL

-- Set ownership
ALTER SEQUENCE test_seq_owned OWNED BY test_items.item_id;

-- After ownership
SELECT pg_get_serial_sequence('test_items', 'item_id');  -- Returns sequence name

-- When table is dropped, owned sequence is automatically dropped
DROP TABLE test_items;  -- Also drops test_seq_owned

-- Verify sequence is gone
SELECT sequencename FROM pg_sequences WHERE sequencename = 'test_seq_owned';

-- ============================================================================
-- Section 14: Sequence in Different Schemas
-- ============================================================================

CREATE SCHEMA test_schema_a;
CREATE SCHEMA test_schema_b;

CREATE SEQUENCE test_schema_a.seq_a START WITH 1000;
CREATE SEQUENCE test_schema_b.seq_b START WITH 2000;

SELECT nextval('test_schema_a.seq_a') AS seq_a_val;
SELECT nextval('test_schema_b.seq_b') AS seq_b_val;

-- ============================================================================
-- Section 15: Concurrent Sequence Access
-- ============================================================================

CREATE SEQUENCE test_seq_concurrent;

CREATE TABLE test_concurrent_inserts (
    id INT PRIMARY KEY DEFAULT nextval('test_seq_concurrent'),
    data TEXT
);

-- Simulate concurrent inserts (sequences are transaction-safe)
INSERT INTO test_concurrent_inserts (data)
SELECT 'Data ' || i FROM generate_series(1, 100) i;

-- All IDs are unique and sequential
SELECT COUNT(*) AS total_count, COUNT(DISTINCT id) AS unique_ids
FROM test_concurrent_inserts;

SELECT MIN(id) AS min_id, MAX(id) AS max_id FROM test_concurrent_inserts;

-- ============================================================================
-- Section 16: Sequence Gaps
-- ============================================================================

CREATE SEQUENCE test_seq_gaps;

CREATE TABLE test_gap_demo (
    id INT PRIMARY KEY DEFAULT nextval('test_seq_gaps'),
    value TEXT
);

-- Successful insert
INSERT INTO test_gap_demo (value) VALUES ('Record 1');

-- Failed insert (creates gap)
BEGIN;
INSERT INTO test_gap_demo (value) VALUES ('Record 2');
ROLLBACK;

-- Next successful insert (gap exists)
INSERT INTO test_gap_demo (value) VALUES ('Record 3');

-- IDs: 1, 3 (gap at 2)
SELECT id, value FROM test_gap_demo ORDER BY id;

-- ============================================================================
-- Section 17: Last Value vs Current Value
-- ============================================================================

CREATE SEQUENCE test_seq_values;

-- last_value is visible in sequence metadata
SELECT last_value FROM test_seq_values;  -- Returns initial value

-- currval requires prior nextval in session
BEGIN;
-- SELECT currval('test_seq_values');  -- Would fail: currval not yet defined
SELECT nextval('test_seq_values') AS first_next;
SELECT currval('test_seq_values') AS current;  -- Now works
COMMIT;

-- ============================================================================
-- Section 18: Sequence Information Queries
-- ============================================================================

-- List all sequences
SELECT
    schemaname,
    sequencename,
    last_value,
    start_value,
    increment_by,
    max_value,
    min_value,
    cache_value,
    cycle
FROM pg_sequences
WHERE schemaname = 'public'
ORDER BY sequencename
LIMIT 5;

-- Get sequence definition
SELECT pg_get_serial_sequence('test_users', 'user_id') AS sequence_name;

-- ============================================================================
-- Section 19: Dropping Sequences
-- ============================================================================

CREATE SEQUENCE test_seq_drop;

-- Verify exists
SELECT sequencename FROM pg_sequences WHERE sequencename = 'test_seq_drop';

-- Drop sequence
DROP SEQUENCE test_seq_drop;

-- Verify dropped
SELECT sequencename FROM pg_sequences WHERE sequencename = 'test_seq_drop';

-- Drop with CASCADE (drops dependent objects)
CREATE SEQUENCE test_seq_cascade;

CREATE TABLE test_cascade_table (
    id INT DEFAULT nextval('test_seq_cascade')
);

-- Cannot drop sequence (has dependent default)
-- DROP SEQUENCE test_seq_cascade;  -- Would fail

-- Drop with CASCADE
DROP SEQUENCE test_seq_cascade CASCADE;

-- Table still exists but default is removed
\d test_cascade_table

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_sequence_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_sequence_best_practices VALUES
    (1, 'Use sequences for auto-incrementing primary keys'),
    (2, 'Set START value to avoid initial gaps'),
    (3, 'Use CACHE for high-volume inserts (reduces I/O)'),
    (4, 'CYCLE carefully - usually not needed for PKs'),
    (5, 'Set OWNED BY to auto-cleanup sequences with tables'),
    (6, 'Sequences are transaction-safe and concurrent'),
    (7, 'nextval() consumes value even if transaction rolls back'),
    (8, 'Gaps in sequences are normal and acceptable'),
    (9, 'Use setval() to manually adjust sequence position'),
    (10, 'currval() requires prior nextval() in same session'),
    (11, 'ALTER SEQUENCE to change properties after creation'),
    (12, 'Use INCREMENT BY for non-standard increments'),
    (13, 'MINVALUE/MAXVALUE prevent unbounded growth'),
    (14, 'Query pg_sequences for sequence metadata'),
    (15, 'Consider BIGINT for high-volume tables');

SELECT id, guideline FROM test_sequence_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_sequence_best_practices;
DROP TABLE test_cascade_table;
DROP SEQUENCE test_schema_b.seq_b;
DROP SEQUENCE test_schema_a.seq_a;
DROP SCHEMA test_schema_b;
DROP SCHEMA test_schema_a;
DROP TABLE test_concurrent_inserts;
DROP SEQUENCE test_seq_concurrent;
DROP TABLE test_gap_demo;
DROP SEQUENCE test_seq_gaps;
DROP SEQUENCE test_seq_values;
DROP SEQUENCE test_seq_alter;
DROP SEQUENCE test_seq_reset;
DROP SEQUENCE test_seq_setval;
DROP TABLE test_orders;
DROP SEQUENCE test_invoice_seq CASCADE;
DROP SEQUENCE test_order_seq CASCADE;
DROP TABLE test_users;
DROP SEQUENCE test_user_id_seq CASCADE;
DROP SEQUENCE test_seq_cache;
DROP SEQUENCE test_seq_descending;
DROP SEQUENCE test_seq_cycle;
DROP SEQUENCE test_seq_bounds;
DROP SEQUENCE test_seq_increment;
DROP SEQUENCE test_seq_start;
DROP SEQUENCE test_seq_basic;

DROP DATABASE test_basic_sequences_db;

-- End of basic sequence tests
