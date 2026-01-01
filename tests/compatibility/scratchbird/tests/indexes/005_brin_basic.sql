-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - BRIN Basic
-- Description: Comprehensive BRIN (Block Range INdex) testing
-- ============================================================================

-- BRIN indexes are designed for very large tables with naturally ordered data
-- Supports: <, <=, =, >=, > operators
-- Best for: time-series data, log tables, append-only tables, large sequential data
-- Characteristics: Extremely small index size, lossy (requires table scan within blocks)
-- Note: Most efficient when data is naturally ordered (e.g., timestamps)

-- Create test database
CREATE DATABASE test_brin_basic_db;
USE test_brin_basic_db;

-- ============================================================================
-- Section 1: Basic BRIN Index Creation
-- ============================================================================

CREATE TABLE test_brin_simple (
    id INT PRIMARY KEY,
    created_at TIMESTAMP,
    measurement NUMERIC,
    status VARCHAR(50)
);

-- BRIN index on timestamp (naturally ordered)
CREATE INDEX idx_created_brin ON test_brin_simple USING BRIN (created_at);

-- BRIN index on numeric column
CREATE INDEX idx_measurement_brin ON test_brin_simple USING BRIN (measurement);

INSERT INTO test_brin_simple
SELECT i,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' seconds')::INTERVAL,
       random() * 100,
       CASE WHEN i % 3 = 0 THEN 'active' ELSE 'inactive' END
FROM generate_series(1, 1000) i;

-- Range queries (uses BRIN index)
SELECT id, created_at FROM test_brin_simple
WHERE created_at >= '2024-01-01 00:10:00' AND created_at < '2024-01-01 00:15:00'
LIMIT 5;

SELECT id FROM test_brin_simple WHERE measurement BETWEEN 50 AND 60 LIMIT 5;

-- ============================================================================
-- Section 2: BRIN for Time-Series Data
-- ============================================================================

CREATE TABLE test_brin_timeseries (
    sensor_id INT,
    timestamp TIMESTAMP,
    temperature NUMERIC(5,2),
    humidity NUMERIC(5,2),
    pressure NUMERIC(6,2)
);

CREATE INDEX idx_timestamp_brin ON test_brin_timeseries USING BRIN (timestamp);
CREATE INDEX idx_temperature_brin ON test_brin_timeseries USING BRIN (temperature);

-- Insert time-series data (naturally ordered by timestamp)
INSERT INTO test_brin_timeseries
SELECT (i % 10) + 1,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' minutes')::INTERVAL,
       20 + random() * 15,
       40 + random() * 30,
       980 + random() * 40
FROM generate_series(1, 100000) i;

-- Time range queries (very efficient with BRIN)
SELECT sensor_id, timestamp, temperature
FROM test_brin_timeseries
WHERE timestamp >= '2024-01-01 12:00:00' AND timestamp < '2024-01-01 13:00:00'
LIMIT 10;

-- Temperature range query
SELECT COUNT(*) FROM test_brin_timeseries WHERE temperature > 30;

-- ============================================================================
-- Section 3: BRIN for Log Tables
-- ============================================================================

CREATE TABLE test_brin_logs (
    log_id SERIAL PRIMARY KEY,
    log_timestamp TIMESTAMP,
    log_level VARCHAR(20),
    log_message TEXT,
    user_id INT
);

CREATE INDEX idx_log_timestamp_brin ON test_brin_logs USING BRIN (log_timestamp);

-- Insert log entries (append-only, naturally ordered)
INSERT INTO test_brin_logs (log_timestamp, log_level, log_message, user_id)
SELECT TIMESTAMP '2024-01-01 00:00:00' + (i || ' seconds')::INTERVAL,
       CASE (i % 5) WHEN 0 THEN 'ERROR' WHEN 1 THEN 'WARN' ELSE 'INFO' END,
       'Log message ' || i,
       (i % 100) + 1
FROM generate_series(1, 50000) i;

-- Query logs by time range
SELECT log_id, log_timestamp, log_level, log_message
FROM test_brin_logs
WHERE log_timestamp >= '2024-01-01 06:00:00' AND log_timestamp < '2024-01-01 07:00:00'
  AND log_level = 'ERROR'
LIMIT 10;

-- Recent logs
SELECT log_id, log_timestamp, log_message FROM test_brin_logs
WHERE log_timestamp >= CURRENT_TIMESTAMP - INTERVAL '1 hour'
LIMIT 5;

-- ============================================================================
-- Section 4: BRIN vs B-tree Comparison
-- ============================================================================

CREATE TABLE test_brin_vs_btree (
    id INT PRIMARY KEY,
    event_time TIMESTAMP,
    value NUMERIC
);

-- Create both BRIN and B-tree indexes
CREATE INDEX idx_event_brin ON test_brin_vs_btree USING BRIN (event_time);
CREATE INDEX idx_event_btree ON test_brin_vs_btree USING BTREE (event_time);

INSERT INTO test_brin_vs_btree
SELECT i,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' seconds')::INTERVAL,
       random() * 1000
FROM generate_series(1, 100000) i;

-- Range query (both indexes can be used)
SELECT id, event_time FROM test_brin_vs_btree
WHERE event_time >= '2024-01-01 12:00:00' AND event_time < '2024-01-01 13:00:00'
LIMIT 5;

-- Check index sizes
SELECT pg_size_pretty(pg_relation_size('idx_event_brin')) AS brin_size,
       pg_size_pretty(pg_relation_size('idx_event_btree')) AS btree_size;

-- BRIN is typically 100-1000x smaller than B-tree for large tables
-- B-tree is faster for lookups, BRIN trades speed for tiny index size

-- ============================================================================
-- Section 5: BRIN Pages Per Range
-- ============================================================================

CREATE TABLE test_brin_pages_per_range (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMP,
    data TEXT
);

-- BRIN with default pages_per_range (128)
CREATE INDEX idx_default_ppr ON test_brin_pages_per_range USING BRIN (created_at);

-- BRIN with smaller pages_per_range (more precise, larger index)
CREATE INDEX idx_small_ppr ON test_brin_pages_per_range USING BRIN (created_at) WITH (pages_per_range = 32);

-- BRIN with larger pages_per_range (less precise, smaller index)
CREATE INDEX idx_large_ppr ON test_brin_pages_per_range USING BRIN (created_at) WITH (pages_per_range = 256);

INSERT INTO test_brin_pages_per_range (created_at, data)
SELECT TIMESTAMP '2024-01-01 00:00:00' + (i || ' minutes')::INTERVAL,
       'Data row ' || i
FROM generate_series(1, 10000) i;

-- All three indexes can be used for queries
SELECT id, created_at FROM test_brin_pages_per_range
WHERE created_at >= '2024-01-05 00:00:00'
LIMIT 5;

-- Compare index sizes
SELECT pg_size_pretty(pg_relation_size('idx_default_ppr')) AS default_size,
       pg_size_pretty(pg_relation_size('idx_small_ppr')) AS small_ppr_size,
       pg_size_pretty(pg_relation_size('idx_large_ppr')) AS large_ppr_size;

-- ============================================================================
-- Section 6: BRIN for IoT Sensor Data
-- ============================================================================

CREATE TABLE test_brin_iot_sensors (
    reading_id BIGSERIAL PRIMARY KEY,
    device_id INT,
    reading_timestamp TIMESTAMP,
    temperature NUMERIC(5,2),
    battery_level INT,
    signal_strength INT
);

CREATE INDEX idx_reading_time_brin ON test_brin_iot_sensors USING BRIN (reading_timestamp);
CREATE INDEX idx_device_time_brin ON test_brin_iot_sensors USING BRIN (device_id, reading_timestamp);

-- Insert IoT sensor readings (time-ordered)
INSERT INTO test_brin_iot_sensors (device_id, reading_timestamp, temperature, battery_level, signal_strength)
SELECT (i % 1000) + 1,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' seconds')::INTERVAL,
       15 + random() * 20,
       50 + (i % 50),
       -90 + random() * 30
FROM generate_series(1, 500000) i;

-- Query recent readings for a device
SELECT reading_timestamp, temperature, battery_level
FROM test_brin_iot_sensors
WHERE device_id = 500
  AND reading_timestamp >= '2024-01-05 00:00:00'
LIMIT 10;

-- Query by time range across all devices
SELECT device_id, AVG(temperature) AS avg_temp
FROM test_brin_iot_sensors
WHERE reading_timestamp >= '2024-01-03 00:00:00' AND reading_timestamp < '2024-01-04 00:00:00'
GROUP BY device_id
LIMIT 10;

-- ============================================================================
-- Section 7: BRIN for Financial Transactions
-- ============================================================================

CREATE TABLE test_brin_transactions (
    transaction_id BIGSERIAL PRIMARY KEY,
    account_id INT,
    transaction_date TIMESTAMP,
    amount NUMERIC(12,2),
    transaction_type VARCHAR(20)
);

CREATE INDEX idx_txn_date_brin ON test_brin_transactions USING BRIN (transaction_date);
CREATE INDEX idx_amount_brin ON test_brin_transactions USING BRIN (amount);

INSERT INTO test_brin_transactions (account_id, transaction_date, amount, transaction_type)
SELECT (i % 10000) + 1,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' seconds')::INTERVAL,
       (random() * 10000)::NUMERIC(12,2),
       CASE (i % 3) WHEN 0 THEN 'debit' WHEN 1 THEN 'credit' ELSE 'transfer' END
FROM generate_series(1, 1000000) i;

-- Query transactions in date range
SELECT transaction_id, account_id, transaction_date, amount
FROM test_brin_transactions
WHERE transaction_date >= '2024-01-10 00:00:00' AND transaction_date < '2024-01-11 00:00:00'
LIMIT 10;

-- Large transaction queries
SELECT transaction_id, account_id, amount FROM test_brin_transactions
WHERE amount > 9000
LIMIT 10;

-- Monthly summary
SELECT DATE_TRUNC('day', transaction_date) AS day, COUNT(*), SUM(amount)
FROM test_brin_transactions
WHERE transaction_date >= '2024-01-15 00:00:00' AND transaction_date < '2024-01-20 00:00:00'
GROUP BY day
ORDER BY day
LIMIT 10;

-- ============================================================================
-- Section 8: BRIN for Sequential Data
-- ============================================================================

CREATE TABLE test_brin_sequential (
    seq_id BIGSERIAL PRIMARY KEY,
    sequence_number BIGINT,
    data_timestamp TIMESTAMP,
    payload TEXT
);

CREATE INDEX idx_seq_num_brin ON test_brin_sequential USING BRIN (sequence_number);
CREATE INDEX idx_data_time_brin ON test_brin_sequential USING BRIN (data_timestamp);

INSERT INTO test_brin_sequential (sequence_number, data_timestamp, payload)
SELECT i,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' seconds')::INTERVAL,
       'Payload ' || i
FROM generate_series(1, 100000) i;

-- Range query on sequence number
SELECT seq_id, sequence_number FROM test_brin_sequential
WHERE sequence_number >= 50000 AND sequence_number < 60000
LIMIT 10;

-- Time range query
SELECT seq_id, data_timestamp FROM test_brin_sequential
WHERE data_timestamp >= '2024-01-01 12:00:00'
LIMIT 10;

-- ============================================================================
-- Section 9: BRIN Autosummarize
-- ============================================================================

CREATE TABLE test_brin_autosummarize (
    id SERIAL PRIMARY KEY,
    event_time TIMESTAMP,
    event_data TEXT
);

-- BRIN with autosummarize enabled (automatically summarizes new pages)
CREATE INDEX idx_autosummarize_brin ON test_brin_autosummarize
USING BRIN (event_time) WITH (autosummarize = on);

INSERT INTO test_brin_autosummarize (event_time, event_data)
SELECT TIMESTAMP '2024-01-01 00:00:00' + (i || ' minutes')::INTERVAL,
       'Event data ' || i
FROM generate_series(1, 10000) i;

-- Query with autosummarized index
SELECT id, event_time FROM test_brin_autosummarize
WHERE event_time >= '2024-01-05 00:00:00'
LIMIT 5;

-- ============================================================================
-- Section 10: BRIN Index Maintenance
-- ============================================================================

CREATE TABLE test_brin_maintenance (
    id SERIAL PRIMARY KEY,
    created_timestamp TIMESTAMP,
    value INT
);

CREATE INDEX idx_maint_brin ON test_brin_maintenance USING BRIN (created_timestamp);

INSERT INTO test_brin_maintenance (created_timestamp, value)
SELECT TIMESTAMP '2024-01-01 00:00:00' + (i || ' hours')::INTERVAL, i
FROM generate_series(1, 1000) i;

-- Summarize new pages that haven't been indexed yet
SELECT brin_summarize_new_values('idx_maint_brin');

-- Desummarize a range (useful for data that was updated)
-- SELECT brin_desummarize_range('idx_maint_brin', 0);

-- Re-summarize
SELECT brin_summarize_new_values('idx_maint_brin');

-- Reindex
REINDEX INDEX idx_maint_brin;

-- ============================================================================
-- Section 11: BRIN for Append-Only Tables
-- ============================================================================

CREATE TABLE test_brin_append_only (
    entry_id BIGSERIAL PRIMARY KEY,
    insert_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metric_value NUMERIC,
    category VARCHAR(50)
);

CREATE INDEX idx_insert_time_brin ON test_brin_append_only USING BRIN (insert_time);
CREATE INDEX idx_metric_brin ON test_brin_append_only USING BRIN (metric_value);

-- Simulate append-only inserts
INSERT INTO test_brin_append_only (metric_value, category)
SELECT random() * 1000, 'category_' || (i % 10)
FROM generate_series(1, 100000) i;

-- Recent data queries (very efficient)
SELECT entry_id, insert_time, metric_value
FROM test_brin_append_only
WHERE insert_time >= CURRENT_TIMESTAMP - INTERVAL '1 hour'
LIMIT 10;

-- Metric range queries
SELECT COUNT(*) FROM test_brin_append_only WHERE metric_value BETWEEN 500 AND 600;

-- ============================================================================
-- Section 12: BRIN Multicolumn Index
-- ============================================================================

CREATE TABLE test_brin_multicolumn (
    id BIGSERIAL PRIMARY KEY,
    timestamp TIMESTAMP,
    region_id INT,
    amount NUMERIC
);

-- Multicolumn BRIN index
CREATE INDEX idx_multi_brin ON test_brin_multicolumn USING BRIN (timestamp, region_id);

INSERT INTO test_brin_multicolumn (timestamp, region_id, amount)
SELECT TIMESTAMP '2024-01-01 00:00:00' + (i || ' minutes')::INTERVAL,
       (i % 10) + 1,
       random() * 1000
FROM generate_series(1, 100000) i;

-- Query using both columns
SELECT id, timestamp, region_id, amount
FROM test_brin_multicolumn
WHERE timestamp >= '2024-01-05 00:00:00' AND region_id = 5
LIMIT 10;

-- ============================================================================
-- Section 13: BRIN for Partitioned Data
-- ============================================================================

CREATE TABLE test_brin_partitioned (
    event_id BIGSERIAL,
    event_date DATE,
    event_type VARCHAR(50),
    event_data TEXT
);

CREATE INDEX idx_event_date_brin ON test_brin_partitioned USING BRIN (event_date);

INSERT INTO test_brin_partitioned (event_date, event_type, event_data)
SELECT DATE '2024-01-01' + (i || ' days')::INTERVAL,
       'type_' || (i % 5),
       'Data for event ' || i
FROM generate_series(1, 10000) i;

-- Query by date range
SELECT event_id, event_date, event_type
FROM test_brin_partitioned
WHERE event_date >= '2024-02-01' AND event_date < '2024-03-01'
LIMIT 10;

-- ============================================================================
-- Section 14: BRIN Performance Characteristics
-- ============================================================================

CREATE TABLE test_brin_performance (
    id BIGSERIAL PRIMARY KEY,
    timestamp_col TIMESTAMP,
    int_col BIGINT,
    numeric_col NUMERIC
);

CREATE INDEX idx_timestamp_perf_brin ON test_brin_performance USING BRIN (timestamp_col);
CREATE INDEX idx_int_perf_brin ON test_brin_performance USING BRIN (int_col);

-- Insert large dataset
INSERT INTO test_brin_performance (timestamp_col, int_col, numeric_col)
SELECT TIMESTAMP '2024-01-01 00:00:00' + (i || ' seconds')::INTERVAL,
       i,
       random() * 10000
FROM generate_series(1, 1000000) i;

-- Range queries (BRIN efficient for naturally ordered data)
SELECT id, timestamp_col FROM test_brin_performance
WHERE timestamp_col >= '2024-01-10 00:00:00' AND timestamp_col < '2024-01-11 00:00:00'
LIMIT 5;

SELECT id FROM test_brin_performance WHERE int_col BETWEEN 500000 AND 600000 LIMIT 5;

-- Check table and index sizes
SELECT pg_size_pretty(pg_relation_size('test_brin_performance')) AS table_size,
       pg_size_pretty(pg_relation_size('idx_timestamp_perf_brin')) AS brin_index_size;

-- ============================================================================
-- Section 15: BRIN Best Practices
-- ============================================================================

CREATE TABLE test_brin_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_brin_best_practices VALUES
    (1, 'Use BRIN for very large tables (100GB+) with naturally ordered data'),
    (2, 'Ideal for time-series data, logs, and append-only tables'),
    (3, 'BRIN indexes are 100-1000x smaller than B-tree indexes'),
    (4, 'Best when data is physically ordered (INSERT order matches index column)'),
    (5, 'BRIN is lossy - stores min/max per block range, requires table scans'),
    (6, 'Use autosummarize=on for append-only tables to auto-index new pages'),
    (7, 'Lower pages_per_range = more precise (larger index), higher = smaller index'),
    (8, 'Default pages_per_range is 128 pages'),
    (9, 'Use brin_summarize_new_values() to manually index new pages'),
    (10, 'BRIN works best with range queries (>, <, BETWEEN)'),
    (11, 'Not suitable for randomly ordered data or frequent updates'),
    (12, 'BRIN on timestamp columns is extremely effective for time-based queries'),
    (13, 'Consider BRIN when table scan cost outweighs index size savings'),
    (14, 'Reindex BRIN periodically if data order changes due to updates'),
    (15, 'Monitor block range summaries to ensure they remain effective');

SELECT id, guideline FROM test_brin_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_brin_best_practices;
DROP TABLE test_brin_performance;
DROP TABLE test_brin_partitioned;
DROP TABLE test_brin_multicolumn;
DROP TABLE test_brin_append_only;
DROP TABLE test_brin_maintenance;
DROP TABLE test_brin_autosummarize;
DROP TABLE test_brin_sequential;
DROP TABLE test_brin_transactions;
DROP TABLE test_brin_iot_sensors;
DROP TABLE test_brin_pages_per_range;
DROP TABLE test_brin_vs_btree;
DROP TABLE test_brin_logs;
DROP TABLE test_brin_timeseries;
DROP TABLE test_brin_simple;

DROP DATABASE test_brin_basic_db;

-- End of BRIN index tests
