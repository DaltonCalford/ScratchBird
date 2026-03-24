-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - Performance and Optimization
-- Description: Comprehensive transaction performance testing
-- ============================================================================

-- Transaction Performance: Optimization patterns, batch operations,
-- connection pooling, statement batching, prepared statements,
-- bulk operations, performance monitoring, tuning

-- Create test database
CREATE DATABASE test_transaction_performance_db;
USE test_transaction_performance_db;

-- ============================================================================
-- Section 1: Single vs Batch Inserts
-- ============================================================================

CREATE TABLE test_single_vs_batch (
    id SERIAL PRIMARY KEY,
    data TEXT,
    batch_type VARCHAR(50)
);

-- Single transaction per insert (slow)
DO $$
DECLARE
    start_time TIMESTAMP;
    end_time TIMESTAMP;
    i INT;
BEGIN
    start_time := clock_timestamp();

    FOR i IN 1..100 LOOP
        BEGIN;
        INSERT INTO test_single_vs_batch (data, batch_type) VALUES ('Single txn', 'single');
        COMMIT;
    END LOOP;

    end_time := clock_timestamp();
    RAISE NOTICE 'Single transactions: % ms', EXTRACT(MILLISECONDS FROM (end_time - start_time));
END $$;

-- Batch insert in single transaction (fast)
DO $$
DECLARE
    start_time TIMESTAMP;
    end_time TIMESTAMP;
BEGIN
    start_time := clock_timestamp();

    BEGIN;
    INSERT INTO test_single_vs_batch (data, batch_type)
    SELECT 'Batch txn', 'batch' FROM generate_series(1, 100);
    COMMIT;

    end_time := clock_timestamp();
    RAISE NOTICE 'Batch transaction: % ms', EXTRACT(MILLISECONDS FROM (end_time - start_time));
END $$;

SELECT batch_type, COUNT(*) FROM test_single_vs_batch GROUP BY batch_type;

-- ============================================================================
-- Section 2: COPY vs INSERT Performance
-- ============================================================================

CREATE TABLE test_copy_vs_insert (
    id SERIAL PRIMARY KEY,
    col1 INT,
    col2 TEXT,
    col3 NUMERIC(10,2),
    load_method VARCHAR(50)
);

-- INSERT with multiple VALUES (medium speed)
DO $$
DECLARE
    start_time TIMESTAMP;
    end_time TIMESTAMP;
BEGIN
    start_time := clock_timestamp();

    INSERT INTO test_copy_vs_insert (col1, col2, col3, load_method)
    SELECT i, 'Data ' || i, i * 1.5, 'INSERT' FROM generate_series(1, 10000) i;

    end_time := clock_timestamp();
    RAISE NOTICE 'INSERT time: % ms', EXTRACT(MILLISECONDS FROM (end_time - start_time));
END $$;

-- COPY is fastest for bulk loading
-- COPY test_copy_vs_insert FROM '/path/to/file.csv' WITH (FORMAT csv);

SELECT load_method, COUNT(*) FROM test_copy_vs_insert GROUP BY load_method;

-- ============================================================================
-- Section 3: Transaction Size Optimization
-- ============================================================================

CREATE TABLE test_txn_size (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Very large transaction (can cause bloat, long locks)
-- Avoid transactions > 100K rows when possible

-- Optimal: Break into smaller transactions
DO $$
DECLARE
    batch_size INT := 1000;
    total INT := 10000;
    i INT;
BEGIN
    FOR i IN 1..total BY batch_size LOOP
        BEGIN;
        INSERT INTO test_txn_size (data)
        SELECT 'Batch ' || j FROM generate_series(i, LEAST(i + batch_size - 1, total)) j;
        COMMIT;
    END LOOP;
END $$;

SELECT COUNT(*) FROM test_txn_size;

-- ============================================================================
-- Section 4: Prepared Statement Performance
-- ============================================================================

CREATE TABLE test_prepared_stmt (
    id SERIAL PRIMARY KEY,
    key VARCHAR(100),
    value INT
);

-- Regular statement (re-parsed each time)
DO $$
DECLARE
    i INT;
BEGIN
    FOR i IN 1..100 LOOP
        EXECUTE format('INSERT INTO test_prepared_stmt (key, value) VALUES (%L, %s)', 'key_' || i, i);
    END LOOP;
END $$;

-- Prepared statement (parsed once, executed many times)
DO $$
DECLARE
    i INT;
BEGIN
    -- In real application, use client library prepared statements
    -- Example: PREPARE insert_stmt (TEXT, INT) AS
    --          INSERT INTO test_prepared_stmt (key, value) VALUES ($1, $2);

    FOR i IN 1..100 LOOP
        INSERT INTO test_prepared_stmt (key, value) VALUES ('prep_key_' || i, i);
    END LOOP;
END $$;

SELECT COUNT(*) FROM test_prepared_stmt;

-- ============================================================================
-- Section 5: Connection Pooling Simulation
-- ============================================================================

CREATE TABLE test_connection_pool (
    id SERIAL PRIMARY KEY,
    session_id INT,
    operation VARCHAR(50),
    executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Simulate multiple sessions from connection pool
DO $$
DECLARE
    pool_size INT := 10;
    ops_per_session INT := 100;
    session INT;
    op INT;
BEGIN
    FOR session IN 1..pool_size LOOP
        FOR op IN 1..ops_per_session LOOP
            BEGIN;
            INSERT INTO test_connection_pool (session_id, operation)
            VALUES (session, 'Operation ' || op);
            COMMIT;
        END LOOP;
    END LOOP;
END $$;

SELECT
    session_id,
    COUNT(*) AS operations,
    MIN(executed_at) AS first_op,
    MAX(executed_at) AS last_op
FROM test_connection_pool
GROUP BY session_id
ORDER BY session_id;

-- ============================================================================
-- Section 6: Synchronous vs Asynchronous Commit
-- ============================================================================

-- synchronous_commit controls durability vs performance trade-off
SHOW synchronous_commit;

CREATE TABLE test_sync_commit (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Synchronous commit (default, durable)
SET synchronous_commit = 'on';

BEGIN;
INSERT INTO test_sync_commit (data) VALUES ('Sync commit data');
COMMIT;
-- Waits for WAL to be flushed to disk

-- Asynchronous commit (faster, small window of data loss on crash)
SET synchronous_commit = 'off';

BEGIN;
INSERT INTO test_sync_commit (data) VALUES ('Async commit data');
COMMIT;
-- Returns immediately, WAL flushed in background

-- Reset to default
SET synchronous_commit = 'on';

SELECT id, data FROM test_sync_commit;

-- ============================================================================
-- Section 7: PostgreSQL compatibility setting names
-- ============================================================================

-- These rows exercise PostgreSQL-compatible pg_settings names for migration and
-- donor-surface analysis only. They are not ScratchBird engine recovery truth;
-- ScratchBird Alpha durability remains MGA/state based without write-ahead redo.

-- WAL configuration affects transaction performance
SELECT
    name,
    setting,
    unit,
    short_desc
FROM pg_settings
WHERE name IN (
    'wal_level',
    'wal_buffers',
    'checkpoint_timeout',
    'max_wal_size',
    'min_wal_size',
    'wal_writer_delay'
)
ORDER BY name;

-- Higher wal_buffers can improve performance for write-heavy workloads

-- ============================================================================
-- Section 8: Transaction Commit Delay
-- ============================================================================

-- commit_delay: Delay before writing WAL (allows batching)
SHOW commit_delay;
SHOW commit_siblings;

CREATE TABLE test_commit_delay (
    id SERIAL PRIMARY KEY,
    value INT
);

-- With commit_delay, multiple concurrent commits can be batched
-- Improves throughput at cost of slight latency

BEGIN;
INSERT INTO test_commit_delay (value) VALUES (1);
COMMIT;

-- commit_siblings: Only delay if this many transactions active
-- Prevents delay for single-user workloads

-- ============================================================================
-- Section 9: Bulk UPDATE Performance
-- ============================================================================

CREATE TABLE test_bulk_update (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    value INT
);

INSERT INTO test_bulk_update (status, value)
SELECT 'pending', i FROM generate_series(1, 10000) i;

-- Single UPDATE for all rows (efficient)
DO $$
DECLARE
    start_time TIMESTAMP;
    end_time TIMESTAMP;
BEGIN
    start_time := clock_timestamp();

    BEGIN;
    UPDATE test_bulk_update SET status = 'processed' WHERE status = 'pending';
    COMMIT;

    end_time := clock_timestamp();
    RAISE NOTICE 'Bulk UPDATE time: % ms', EXTRACT(MILLISECONDS FROM (end_time - start_time));
END $$;

-- Avoid row-by-row updates when possible
SELECT status, COUNT(*) FROM test_bulk_update GROUP BY status;

-- ============================================================================
-- Section 10: Index Impact on INSERT Performance
-- ============================================================================

CREATE TABLE test_index_impact_noindex (
    id SERIAL PRIMARY KEY,
    col1 INT,
    col2 TEXT,
    col3 NUMERIC
);

CREATE TABLE test_index_impact_indexed (
    id SERIAL PRIMARY KEY,
    col1 INT,
    col2 TEXT,
    col3 NUMERIC
);

CREATE INDEX idx_impact_col1 ON test_index_impact_indexed(col1);
CREATE INDEX idx_impact_col2 ON test_index_impact_indexed(col2);
CREATE INDEX idx_impact_col3 ON test_index_impact_indexed(col3);

-- Insert without indexes (faster)
DO $$
DECLARE
    start_time TIMESTAMP;
    end_time TIMESTAMP;
BEGIN
    start_time := clock_timestamp();

    INSERT INTO test_index_impact_noindex (col1, col2, col3)
    SELECT i, 'Data ' || i, i * 1.5 FROM generate_series(1, 10000) i;

    end_time := clock_timestamp();
    RAISE NOTICE 'INSERT without indexes: % ms', EXTRACT(MILLISECONDS FROM (end_time - start_time));
END $$;

-- Insert with indexes (slower, but needed for queries)
DO $$
DECLARE
    start_time TIMESTAMP;
    end_time TIMESTAMP;
BEGIN
    start_time := clock_timestamp();

    INSERT INTO test_index_impact_indexed (col1, col2, col3)
    SELECT i, 'Data ' || i, i * 1.5 FROM generate_series(1, 10000) i;

    end_time := clock_timestamp();
    RAISE NOTICE 'INSERT with indexes: % ms', EXTRACT(MILLISECONDS FROM (end_time - start_time));
END $$;

-- For bulk loading: Drop indexes, load data, recreate indexes

-- ============================================================================
-- Section 11: Transaction Log Monitoring
-- ============================================================================

-- Monitor transaction activity
SELECT
    datname AS database,
    xact_commit AS commits,
    xact_rollback AS rollbacks,
    blks_read,
    blks_hit,
    tup_inserted,
    tup_updated,
    tup_deleted
FROM pg_stat_database
WHERE datname = current_database();

-- ============================================================================
-- Section 12: Long Transaction Impact
-- ============================================================================

CREATE TABLE test_long_txn_impact (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Long-running transaction prevents VACUUM from reclaiming tuples
-- Causes bloat in other tables

-- Query long-running transactions
SELECT
    pid,
    usename,
    application_name,
    state,
    age(NOW(), xact_start) AS transaction_age,
    age(NOW(), query_start) AS query_age,
    query
FROM pg_stat_activity
WHERE state = 'active'
  AND xact_start IS NOT NULL
ORDER BY xact_start;

-- ============================================================================
-- Section 13: Checkpoint Performance Impact
-- ============================================================================

-- Checkpoints can cause I/O spikes
SELECT
    checkpoints_timed,
    checkpoints_req AS checkpoints_requested,
    checkpoint_write_time,
    checkpoint_sync_time,
    buffers_checkpoint,
    buffers_clean,
    buffers_backend
FROM pg_stat_bgwriter;

-- Tune checkpoint frequency and smoothing
-- checkpoint_completion_target: Spread checkpoint I/O over time

-- ============================================================================
-- Section 14: Vacuum Impact on Transaction Performance
-- ============================================================================

CREATE TABLE test_vacuum_impact (
    id SERIAL PRIMARY KEY,
    value INT
);

INSERT INTO test_vacuum_impact (value) SELECT i FROM generate_series(1, 100000) i;

-- Many updates create dead tuples
UPDATE test_vacuum_impact SET value = value + 1;

-- Check bloat
SELECT
    pg_size_pretty(pg_total_relation_size('test_vacuum_impact')) AS total_size,
    n_live_tup,
    n_dead_tup
FROM pg_stat_user_tables
WHERE relname = 'test_vacuum_impact';

-- VACUUM reclaims space
VACUUM ANALYZE test_vacuum_impact;

-- After vacuum
SELECT
    pg_size_pretty(pg_total_relation_size('test_vacuum_impact')) AS total_size,
    n_dead_tup
FROM pg_stat_user_tables
WHERE relname = 'test_vacuum_impact';

-- autovacuum runs automatically, but manual VACUUM may be needed for heavy write workloads

-- ============================================================================
-- Section 15: Constraint Validation Performance
-- ============================================================================

CREATE TABLE test_constraint_perf_nocheck (
    id SERIAL PRIMARY KEY,
    value INT
);

CREATE TABLE test_constraint_perf_check (
    id SERIAL PRIMARY KEY,
    value INT CHECK (value > 0)
);

-- Insert without constraint (faster)
INSERT INTO test_constraint_perf_nocheck (value)
SELECT i FROM generate_series(1, 10000) i;

-- Insert with constraint (slower, validates each row)
INSERT INTO test_constraint_perf_check (value)
SELECT i FROM generate_series(1, 10000) i;

-- For bulk loading: Add constraints AFTER loading data

-- ============================================================================
-- Section 16: UNLOGGED Tables for Performance
-- ============================================================================

-- UNLOGGED tables: No WAL logging, faster but not crash-safe
CREATE UNLOGGED TABLE test_unlogged (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Fast inserts (no WAL overhead)
INSERT INTO test_unlogged (data)
SELECT 'Data ' || i FROM generate_series(1, 10000) i;

-- Use for temporary/staging data that can be regenerated
-- NOT for production data (lost on crash)

SELECT COUNT(*) FROM test_unlogged;

-- ============================================================================
-- Section 17: Partitioning for Performance
-- ============================================================================

CREATE TABLE test_partitioned_perf (
    id SERIAL,
    category VARCHAR(50),
    data TEXT,
    created_date DATE,
    PRIMARY KEY (id, category)
) PARTITION BY LIST (category);

CREATE TABLE test_part_cat_a PARTITION OF test_partitioned_perf
    FOR VALUES IN ('A');

CREATE TABLE test_part_cat_b PARTITION OF test_partitioned_perf
    FOR VALUES IN ('B');

CREATE TABLE test_part_cat_c PARTITION OF test_partitioned_perf
    FOR VALUES IN ('C');

-- Inserts distributed across partitions (better concurrency)
INSERT INTO test_partitioned_perf (category, data, created_date)
SELECT
    CASE (i % 3)
        WHEN 0 THEN 'A'
        WHEN 1 THEN 'B'
        ELSE 'C'
    END,
    'Data ' || i,
    CURRENT_DATE
FROM generate_series(1, 10000) i;

-- Query partition counts
SELECT tableoid::regclass AS partition, COUNT(*)
FROM test_partitioned_perf
GROUP BY partition
ORDER BY partition;

-- ============================================================================
-- Section 18: Parallel Query Performance
-- ============================================================================

-- Parallel queries can improve read performance
SHOW max_parallel_workers_per_gather;
SHOW parallel_setup_cost;

CREATE TABLE test_parallel_query (
    id SERIAL PRIMARY KEY,
    value INT
);

INSERT INTO test_parallel_query (value)
SELECT i FROM generate_series(1, 1000000) i;

ANALYZE test_parallel_query;

-- Query with parallel workers
EXPLAIN (ANALYZE, BUFFERS)
SELECT COUNT(*), AVG(value) FROM test_parallel_query;

-- Parallel queries reduce transaction time for large scans

-- ============================================================================
-- Section 19: Connection Overhead Measurement
-- ============================================================================

-- Connection setup has overhead
-- Connection pooling reduces overhead

CREATE TABLE test_connection_overhead (
    id SERIAL PRIMARY KEY,
    connection_num INT,
    data TEXT
);

-- Simulate many short connections (expensive)
DO $$
DECLARE
    i INT;
BEGIN
    FOR i IN 1..100 LOOP
        -- In real app, this would be separate connections
        INSERT INTO test_connection_overhead (connection_num, data)
        VALUES (i, 'Connection ' || i);
    END LOOP;
END $$;

-- Connection pooling reuses connections (efficient)

SELECT COUNT(*) FROM test_connection_overhead;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_transaction_performance_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_transaction_performance_best_practices VALUES
    (1, 'Batch inserts in single transaction instead of individual transactions'),
    (2, 'Use COPY for bulk loading (fastest method)'),
    (3, 'Keep transactions short to reduce lock contention'),
    (4, 'Use prepared statements for repeated queries'),
    (5, 'Connection pooling reduces connection overhead'),
    (6, 'Set synchronous_commit=off for non-critical data (faster commits)'),
    (7, 'Tune wal_buffers for write-heavy workloads'),
    (8, 'Use commit_delay to batch commits in multi-user scenarios'),
    (9, 'Bulk UPDATE is faster than row-by-row updates'),
    (10, 'Indexes slow inserts - consider dropping for bulk load, recreate after'),
    (11, 'Monitor long-running transactions (cause bloat, prevent VACUUM)'),
    (12, 'Tune checkpoint frequency to spread I/O load'),
    (13, 'Regular VACUUM prevents bloat and maintains performance'),
    (14, 'Add constraints AFTER bulk loading data'),
    (15, 'Use UNLOGGED tables for temporary/staging data'),
    (16, 'Partition large tables for better concurrency and query performance'),
    (17, 'Enable parallel queries for large scans'),
    (18, 'Monitor pg_stat_database and pg_stat_bgwriter'),
    (19, 'Avoid very large transactions (> 100K rows)'),
    (20, 'Benchmark before and after optimization changes');

SELECT id, guideline FROM test_transaction_performance_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_transaction_performance_best_practices;
DROP TABLE test_connection_overhead;
DROP TABLE test_parallel_query;
DROP TABLE test_part_cat_c;
DROP TABLE test_part_cat_b;
DROP TABLE test_part_cat_a;
DROP TABLE test_partitioned_perf;
DROP TABLE test_unlogged;
DROP TABLE test_constraint_perf_check;
DROP TABLE test_constraint_perf_nocheck;
DROP TABLE test_vacuum_impact;
DROP TABLE test_long_txn_impact;
DROP TABLE test_index_impact_indexed;
DROP TABLE test_index_impact_noindex;
DROP TABLE test_bulk_update;
DROP TABLE test_commit_delay;
DROP TABLE test_sync_commit;
DROP TABLE test_connection_pool;
DROP TABLE test_prepared_stmt;
DROP TABLE test_txn_size;
DROP TABLE test_copy_vs_insert;
DROP TABLE test_single_vs_batch;

DROP DATABASE test_transaction_performance_db;

-- End of transaction performance tests
