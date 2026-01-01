-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Sequences - Advanced Features and Edge Cases
-- Description: Comprehensive advanced sequence testing
-- ============================================================================

-- Advanced features: Sequence caching, concurrent access,
-- sequence functions, metadata queries, performance optimization,
-- edge cases, troubleshooting

-- Create test database
CREATE DATABASE test_advanced_sequences_db;
USE test_advanced_sequences_db;

-- ============================================================================
-- Section 1: Sequence Caching Performance
-- ============================================================================

CREATE SEQUENCE test_seq_no_cache CACHE 1;
CREATE SEQUENCE test_seq_with_cache CACHE 100;

CREATE TABLE test_cache_comparison (
    id SERIAL PRIMARY KEY,
    seq_no_cache BIGINT DEFAULT nextval('test_seq_no_cache'),
    seq_with_cache BIGINT DEFAULT nextval('test_seq_with_cache'),
    data TEXT
);

-- Insert batch - cached sequence performs better
INSERT INTO test_cache_comparison (data)
SELECT 'Data ' || i FROM generate_series(1, 1000) i;

SELECT COUNT(*) FROM test_cache_comparison;

-- Query cache values
SELECT sequencename, cache_value FROM pg_sequences
WHERE sequencename IN ('test_seq_no_cache', 'test_seq_with_cache');

-- ============================================================================
-- Section 2: Sequence Functions - lastval()
-- ============================================================================

CREATE SEQUENCE test_seq_lastval;

-- lastval() returns last value from any sequence in session
SELECT nextval('test_seq_lastval') AS next_val;
SELECT lastval() AS last_from_any_seq;  -- Returns value from test_seq_lastval

CREATE SEQUENCE test_seq_another;
SELECT nextval('test_seq_another') AS next_val;
SELECT lastval() AS last_from_any_seq;  -- Now returns value from test_seq_another

-- ============================================================================
-- Section 3: Sequence with OWNED BY and Dependencies
-- ============================================================================

CREATE TABLE test_owner_table (
    id INT PRIMARY KEY,
    data TEXT
);

CREATE SEQUENCE test_owned_seq;

-- Before ownership
SELECT COUNT(*) FROM pg_depend
WHERE refobjid = 'test_owner_table'::regclass
  AND objid = 'test_owned_seq'::regclass;  -- Returns 0

-- Set ownership
ALTER SEQUENCE test_owned_seq OWNED BY test_owner_table.id;

-- After ownership
SELECT COUNT(*) FROM pg_depend
WHERE refobjid = 'test_owner_table'::regclass
  AND objid = 'test_owned_seq'::regclass;  -- Returns 1

-- Dropping table also drops owned sequence
DROP TABLE test_owner_table;  -- Also drops test_owned_seq

-- Verify sequence dropped
SELECT sequencename FROM pg_sequences WHERE sequencename = 'test_owned_seq';

-- ============================================================================
-- Section 4: Concurrent Sequence Access
-- ============================================================================

CREATE SEQUENCE test_concurrent_seq;

CREATE TABLE test_concurrent_table (
    id BIGINT PRIMARY KEY DEFAULT nextval('test_concurrent_seq'),
    session_id INT,
    data TEXT
);

-- Simulate concurrent inserts from multiple sessions
-- Each gets unique value from sequence
INSERT INTO test_concurrent_table (session_id, data)
SELECT 1, 'Session 1 - ' || i FROM generate_series(1, 100) i;

INSERT INTO test_concurrent_table (session_id, data)
SELECT 2, 'Session 2 - ' || i FROM generate_series(1, 100) i;

-- All IDs are unique
SELECT COUNT(*) AS total, COUNT(DISTINCT id) AS unique_ids
FROM test_concurrent_table;

-- IDs may not be perfectly sequential due to concurrency
SELECT MIN(id) AS min_id, MAX(id) AS max_id, MAX(id) - MIN(id) + 1 AS range
FROM test_concurrent_table;

-- ============================================================================
-- Section 5: Sequence Wraparound/Cycling
-- ============================================================================

CREATE SEQUENCE test_wraparound
    START WITH 1
    INCREMENT BY 1
    MINVALUE 1
    MAXVALUE 10
    CYCLE;

-- Get values through multiple cycles
CREATE TEMP TABLE test_wraparound_values (val BIGINT);

INSERT INTO test_wraparound_values
SELECT nextval('test_wraparound') FROM generate_series(1, 25);

SELECT val, COUNT(*) AS occurrences
FROM test_wraparound_values
GROUP BY val
ORDER BY val;

-- ============================================================================
-- Section 6: Sequence Gaps and Transaction Rollback
-- ============================================================================

CREATE SEQUENCE test_gap_seq;

CREATE TABLE test_gap_table (
    id BIGINT PRIMARY KEY DEFAULT nextval('test_gap_seq'),
    value TEXT
);

-- Successful insert
INSERT INTO test_gap_table (value) VALUES ('Value 1');

-- Failed transaction creates gap
BEGIN;
INSERT INTO test_gap_table (value) VALUES ('Value 2');
-- Sequence advanced but transaction rolled back
ROLLBACK;

-- Next insert has gap
INSERT INTO test_gap_table (value) VALUES ('Value 3');

-- Gap at ID 2
SELECT id, value FROM test_gap_table ORDER BY id;

SELECT currval('test_gap_seq') AS current_seq_value;

-- ============================================================================
-- Section 7: Sequence Maximum Value Handling
-- ============================================================================

CREATE SEQUENCE test_max_handling
    START WITH 1
    MAXVALUE 5
    NO CYCLE;

-- Consume sequence values
SELECT nextval('test_max_handling') FROM generate_series(1, 5);

-- Next call would exceed max (fails)
DO $$
BEGIN
    PERFORM nextval('test_max_handling');
    RAISE NOTICE 'Sequence did not fail';
EXCEPTION
    WHEN SQLSTATE '2200H' THEN  -- sequence_generator_limit_exceeded
        RAISE NOTICE 'Caught sequence limit exceeded error';
END $$;

-- ============================================================================
-- Section 8: Negative Increment Sequences
-- ============================================================================

CREATE SEQUENCE test_negative_inc
    START WITH 100
    INCREMENT BY -5
    MINVALUE 1
    MAXVALUE 100;

SELECT nextval('test_negative_inc') AS val FROM generate_series(1, 5);
-- Returns: 100, 95, 90, 85, 80

-- ============================================================================
-- Section 9: Sequence Metadata Functions
-- ============================================================================

CREATE SEQUENCE test_metadata_seq
    START WITH 1000
    INCREMENT BY 10
    MINVALUE 1000
    MAXVALUE 10000
    CACHE 50
    CYCLE;

-- Get sequence parameters
SELECT
    pg_sequence_parameters('test_metadata_seq'::regclass) AS params;

-- Query all sequence properties
SELECT
    sequencename,
    last_value,
    start_value,
    increment_by,
    max_value,
    min_value,
    cache_value,
    cycle,
    data_type
FROM pg_sequences
WHERE sequencename = 'test_metadata_seq';

-- ============================================================================
-- Section 10: Sequence Privileges and Security
-- ============================================================================

CREATE SEQUENCE test_priv_seq;

CREATE TABLE test_priv_table (
    id BIGINT PRIMARY KEY DEFAULT nextval('test_priv_seq'),
    data TEXT
);

-- Query sequence privileges
SELECT
    grantee,
    privilege_type
FROM information_schema.usage_privileges
WHERE object_name = 'test_priv_seq'
ORDER BY grantee, privilege_type;

-- ============================================================================
-- Section 11: Renaming Sequences
-- ============================================================================

CREATE SEQUENCE test_seq_old_name;

-- Rename sequence
ALTER SEQUENCE test_seq_old_name RENAME TO test_seq_new_name;

-- Verify rename
SELECT sequencename FROM pg_sequences WHERE sequencename = 'test_seq_new_name';
SELECT sequencename FROM pg_sequences WHERE sequencename = 'test_seq_old_name';  -- Empty

-- Use renamed sequence
SELECT nextval('test_seq_new_name') AS val;

-- ============================================================================
-- Section 12: Sequence Schema Changes
-- ============================================================================

CREATE SCHEMA test_schema_old;
CREATE SCHEMA test_schema_new;

CREATE SEQUENCE test_schema_old.my_seq;

-- Move sequence to different schema
ALTER SEQUENCE test_schema_old.my_seq SET SCHEMA test_schema_new;

-- Verify move
SELECT schemaname, sequencename
FROM pg_sequences
WHERE sequencename = 'my_seq';

SELECT nextval('test_schema_new.my_seq') AS val;

-- ============================================================================
-- Section 13: Sequence with Very Large Numbers
-- ============================================================================

CREATE SEQUENCE test_large_numbers
    START WITH 9223372036854775800
    INCREMENT BY 1
    MAXVALUE 9223372036854775807;  -- BIGINT max

SELECT nextval('test_large_numbers') AS large_val;

-- ============================================================================
-- Section 14: Sequence Synchronization Issues
-- ============================================================================

CREATE TABLE test_sync_issues (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Insert with explicit IDs (creates synchronization issue)
INSERT INTO test_sync_issues (id, data) VALUES (1000, 'Data 1000');
INSERT INTO test_sync_issues (id, data) VALUES (1001, 'Data 1001');

-- Auto-generated ID is behind
INSERT INTO test_sync_issues (data) VALUES ('Data Auto 1');  -- Gets ID 1

-- Fix synchronization
SELECT setval(
    pg_get_serial_sequence('test_sync_issues', 'id'),
    (SELECT MAX(id) FROM test_sync_issues)
);

-- Now auto-generation works correctly
INSERT INTO test_sync_issues (data) VALUES ('Data Auto 2');  -- Gets ID 1002

SELECT id, data FROM test_sync_issues ORDER BY id;

-- ============================================================================
-- Section 15: Sequence Performance Tuning
-- ============================================================================

-- High cache value for high-throughput tables
CREATE SEQUENCE test_high_throughput_seq CACHE 1000;

-- Low cache for sequences where gaps must be minimal
CREATE SEQUENCE test_low_gap_seq CACHE 1;

CREATE TABLE test_perf_table (
    id BIGINT PRIMARY KEY DEFAULT nextval('test_high_throughput_seq'),
    data TEXT
);

-- Bulk insert performance
INSERT INTO test_perf_table (data)
SELECT 'Data ' || i FROM generate_series(1, 10000) i;

SELECT COUNT(*) FROM test_perf_table;

-- ============================================================================
-- Section 16: Sequence Information Functions
-- ============================================================================

CREATE SEQUENCE test_info_seq START WITH 100 INCREMENT BY 5;

-- Get next value without consuming
SELECT last_value FROM test_info_seq;

-- Consume a value
SELECT nextval('test_info_seq') AS consumed;

-- Check if value is defined in session
SELECT currval('test_info_seq') AS current;

-- ============================================================================
-- Section 17: Temporary Sequences
-- ============================================================================

CREATE TEMP SEQUENCE test_temp_seq;

SELECT nextval('test_temp_seq') AS temp_val;

-- Temporary sequence exists only for this session
SELECT sequencename FROM pg_sequences WHERE sequencename = 'test_temp_seq';

-- ============================================================================
-- Section 18: Sequence ALTER Operations
-- ============================================================================

CREATE SEQUENCE test_alter_ops START WITH 1 INCREMENT BY 1;

-- Change multiple properties
ALTER SEQUENCE test_alter_ops
    INCREMENT BY 10
    MINVALUE 1
    MAXVALUE 1000
    CACHE 50
    CYCLE;

-- Restart from different value
ALTER SEQUENCE test_alter_ops RESTART WITH 500;

SELECT nextval('test_alter_ops') AS val;  -- Returns 500

-- Query updated properties
SELECT increment_by, max_value, cache_value, cycle
FROM pg_sequences
WHERE sequencename = 'test_alter_ops';

-- ============================================================================
-- Section 19: Detecting Sequence Exhaustion
-- ============================================================================

CREATE SEQUENCE test_exhaustion
    START WITH 1
    MAXVALUE 3
    NO CYCLE;

CREATE FUNCTION test_safe_nextval(seq_name TEXT) RETURNS BIGINT AS $$
DECLARE
    next_val BIGINT;
    max_val BIGINT;
    last_val BIGINT;
BEGIN
    SELECT max_value, last_value
    INTO max_val, last_val
    FROM pg_sequences
    WHERE sequencename = seq_name;

    IF last_val >= max_val THEN
        RAISE EXCEPTION 'Sequence % is exhausted', seq_name;
    END IF;

    EXECUTE format('SELECT nextval(%L)', seq_name) INTO next_val;
    RETURN next_val;
END;
$$ LANGUAGE plpgsql;

SELECT test_safe_nextval('test_exhaustion') FROM generate_series(1, 3);

-- Would fail safely
-- SELECT test_safe_nextval('test_exhaustion');

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_sequence_advanced_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_sequence_advanced_best_practices VALUES
    (1, 'Use CACHE for high-throughput sequences (reduces I/O)'),
    (2, 'Default CACHE is 1, increase for better performance'),
    (3, 'Higher CACHE means larger gaps after server restart'),
    (4, 'Sequences are transaction-safe and lock-free'),
    (5, 'nextval() consumes value even if transaction rolls back'),
    (6, 'Gaps in sequences are normal and by design'),
    (7, 'Use setval() to synchronize sequences after manual inserts'),
    (8, 'CYCLE carefully - can cause PK violations'),
    (9, 'Monitor sequence approaching MAXVALUE'),
    (10, 'Use BIGINT sequences for high-volume tables'),
    (11, 'Sequence ownership ensures automatic cleanup'),
    (12, 'Concurrent access is safe without explicit locking'),
    (13, 'lastval() returns last value from any sequence in session'),
    (14, 'Query pg_sequences for sequence metadata'),
    (15, 'Consider UUID for distributed systems instead of sequences');

SELECT id, guideline FROM test_sequence_advanced_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_sequence_advanced_best_practices;
DROP FUNCTION test_safe_nextval(TEXT);
DROP SEQUENCE test_exhaustion;
DROP SEQUENCE test_alter_ops;
DROP SEQUENCE test_info_seq;
DROP TABLE test_perf_table;
DROP SEQUENCE test_low_gap_seq;
DROP SEQUENCE test_high_throughput_seq;
DROP TABLE test_sync_issues;
DROP SEQUENCE test_large_numbers;
DROP SEQUENCE test_schema_new.my_seq;
DROP SCHEMA test_schema_new;
DROP SCHEMA test_schema_old;
DROP SEQUENCE test_seq_new_name;
DROP TABLE test_priv_table;
DROP SEQUENCE test_priv_seq;
DROP SEQUENCE test_metadata_seq;
DROP SEQUENCE test_negative_inc;
DROP SEQUENCE test_max_handling;
DROP TABLE test_gap_table;
DROP SEQUENCE test_gap_seq;
DROP SEQUENCE test_wraparound;
DROP TABLE test_concurrent_table;
DROP SEQUENCE test_concurrent_seq;
DROP SEQUENCE test_seq_another;
DROP SEQUENCE test_seq_lastval;
DROP TABLE test_cache_comparison;
DROP SEQUENCE test_seq_with_cache;
DROP SEQUENCE test_seq_no_cache;

DROP DATABASE test_advanced_sequences_db;

-- End of advanced sequence tests
