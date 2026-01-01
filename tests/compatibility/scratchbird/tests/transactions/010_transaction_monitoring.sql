-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - Monitoring and Troubleshooting
-- Description: Comprehensive transaction monitoring and debugging
-- ============================================================================

-- Transaction Monitoring: Activity views, statistics, logging,
-- performance analysis, troubleshooting, diagnostic queries,
-- transaction tracking, audit trails

-- Create test database
CREATE DATABASE test_transaction_monitoring_db;
USE test_transaction_monitoring_db;

-- ============================================================================
-- Section 1: Active Transaction Monitoring
-- ============================================================================

CREATE TABLE test_active_monitoring (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Monitor active transactions
SELECT
    pid,
    usename AS username,
    application_name,
    client_addr,
    backend_start,
    xact_start,
    query_start,
    state_change,
    state,
    query
FROM pg_stat_activity
WHERE state = 'active'
  AND xact_start IS NOT NULL
ORDER BY xact_start;

-- ============================================================================
-- Section 2: Long-Running Transaction Detection
-- ============================================================================

-- Identify long-running transactions
SELECT
    pid,
    usename,
    application_name,
    state,
    age(NOW(), xact_start) AS transaction_duration,
    age(NOW(), query_start) AS query_duration,
    query
FROM pg_stat_activity
WHERE xact_start IS NOT NULL
  AND age(NOW(), xact_start) > interval '5 minutes'
ORDER BY xact_start;

-- Alert on transactions exceeding threshold
DO $$
DECLARE
    long_txn_count INT;
BEGIN
    SELECT COUNT(*) INTO long_txn_count
    FROM pg_stat_activity
    WHERE xact_start IS NOT NULL
      AND age(NOW(), xact_start) > interval '10 minutes';

    IF long_txn_count > 0 THEN
        RAISE WARNING '% long-running transactions detected', long_txn_count;
    END IF;
END $$;

-- ============================================================================
-- Section 3: Idle in Transaction Monitoring
-- ============================================================================

-- Identify idle in transaction sessions (holding locks, preventing VACUUM)
SELECT
    pid,
    usename,
    application_name,
    client_addr,
    state,
    age(NOW(), xact_start) AS idle_in_transaction_duration,
    query
FROM pg_stat_activity
WHERE state = 'idle in transaction'
ORDER BY xact_start;

-- Count idle in transaction by application
SELECT
    application_name,
    COUNT(*) AS idle_count,
    MAX(age(NOW(), xact_start)) AS max_idle_duration
FROM pg_stat_activity
WHERE state = 'idle in transaction'
GROUP BY application_name
ORDER BY idle_count DESC;

-- ============================================================================
-- Section 4: Transaction Statistics by Database
-- ============================================================================

-- Query transaction statistics
SELECT
    datname AS database_name,
    xact_commit AS commits,
    xact_rollback AS rollbacks,
    ROUND(100.0 * xact_rollback / NULLIF(xact_commit + xact_rollback, 0), 2) AS rollback_percentage,
    blks_read AS disk_blocks_read,
    blks_hit AS cache_blocks_hit,
    ROUND(100.0 * blks_hit / NULLIF(blks_read + blks_hit, 0), 2) AS cache_hit_ratio,
    tup_returned,
    tup_fetched,
    tup_inserted,
    tup_updated,
    tup_deleted,
    conflicts,
    temp_files,
    temp_bytes,
    deadlocks
FROM pg_stat_database
WHERE datname = current_database();

-- ============================================================================
-- Section 5: Transaction Commit/Rollback Ratio
-- ============================================================================

CREATE TABLE test_commit_rollback_ratio (
    id SERIAL PRIMARY KEY,
    value INT CHECK (value > 0)
);

-- Successful commits
BEGIN;
INSERT INTO test_commit_rollback_ratio (value) VALUES (10);
COMMIT;

BEGIN;
INSERT INTO test_commit_rollback_ratio (value) VALUES (20);
COMMIT;

-- Rollback due to error
BEGIN;
INSERT INTO test_commit_rollback_ratio (value) VALUES (-5);  -- Fails CHECK
EXCEPTION WHEN OTHERS THEN
    ROLLBACK;
END;

-- Query commit/rollback statistics
SELECT
    xact_commit AS total_commits,
    xact_rollback AS total_rollbacks,
    xact_commit + xact_rollback AS total_transactions,
    ROUND(100.0 * xact_commit / NULLIF(xact_commit + xact_rollback, 0), 2) AS commit_percentage
FROM pg_stat_database
WHERE datname = current_database();

-- ============================================================================
-- Section 6: Blocking Query Detection
-- ============================================================================

CREATE TABLE test_blocking_detection (
    id SERIAL PRIMARY KEY,
    value INT
);

INSERT INTO test_blocking_detection (value) VALUES (100);

-- Query to find blocking queries
SELECT
    blocked_locks.pid AS blocked_pid,
    blocked_activity.usename AS blocked_user,
    blocking_locks.pid AS blocking_pid,
    blocking_activity.usename AS blocking_user,
    blocked_activity.query AS blocked_query,
    blocking_activity.query AS blocking_query,
    blocked_activity.application_name AS blocked_app,
    blocking_activity.application_name AS blocking_app
FROM pg_catalog.pg_locks blocked_locks
JOIN pg_catalog.pg_stat_activity blocked_activity ON blocked_activity.pid = blocked_locks.pid
JOIN pg_catalog.pg_locks blocking_locks
    ON blocking_locks.locktype = blocked_locks.locktype
    AND blocking_locks.database IS NOT DISTINCT FROM blocked_locks.database
    AND blocking_locks.relation IS NOT DISTINCT FROM blocked_locks.relation
    AND blocking_locks.page IS NOT DISTINCT FROM blocked_locks.page
    AND blocking_locks.tuple IS NOT DISTINCT FROM blocked_locks.tuple
    AND blocking_locks.virtualxid IS NOT DISTINCT FROM blocked_locks.virtualxid
    AND blocking_locks.transactionid IS NOT DISTINCT FROM blocked_locks.transactionid
    AND blocking_locks.classid IS NOT DISTINCT FROM blocked_locks.classid
    AND blocking_locks.objid IS NOT DISTINCT FROM blocked_locks.objid
    AND blocking_locks.objsubid IS NOT DISTINCT FROM blocked_locks.objsubid
    AND blocking_locks.pid != blocked_locks.pid
JOIN pg_catalog.pg_stat_activity blocking_activity ON blocking_activity.pid = blocking_locks.pid
WHERE NOT blocked_locks.granted;

-- ============================================================================
-- Section 7: Lock Monitoring and Analysis
-- ============================================================================

-- Current locks in database
SELECT
    locktype,
    database,
    relation::regclass AS table_name,
    page,
    tuple,
    virtualxid,
    transactionid,
    mode,
    granted,
    pid
FROM pg_locks
WHERE database = (SELECT oid FROM pg_database WHERE datname = current_database())
ORDER BY pid, locktype;

-- Lock counts by type
SELECT
    locktype,
    mode,
    COUNT(*) AS lock_count
FROM pg_locks
WHERE database = (SELECT oid FROM pg_database WHERE datname = current_database())
GROUP BY locktype, mode
ORDER BY lock_count DESC;

-- ============================================================================
-- Section 8: Transaction ID Monitoring
-- ============================================================================

-- Current transaction ID
SELECT txid_current() AS current_transaction_id;

-- Transaction ID wraparound risk assessment
SELECT
    datname,
    age(datfrozenxid) AS xid_age,
    pg_size_pretty(pg_database_size(datname)) AS database_size
FROM pg_database
WHERE datname = current_database();

-- Check for approaching wraparound
DO $$
DECLARE
    max_age INT;
    current_age INT;
    warning_threshold INT := 1000000000;  -- 1 billion
BEGIN
    SELECT setting::INT INTO max_age
    FROM pg_settings
    WHERE name = 'autovacuum_freeze_max_age';

    SELECT age(datfrozenxid) INTO current_age
    FROM pg_database
    WHERE datname = current_database();

    IF current_age > warning_threshold THEN
        RAISE WARNING 'Transaction ID age is %, approaching freeze limit %', current_age, max_age;
    END IF;
END $$;

-- ============================================================================
-- Section 9: Deadlock Monitoring
-- ============================================================================

-- Query deadlock statistics
SELECT
    datname,
    deadlocks,
    deadlocks::FLOAT / NULLIF(xact_commit + xact_rollback, 0) * 100 AS deadlock_percentage
FROM pg_stat_database
WHERE datname = current_database();

-- Deadlock logging configuration
SHOW deadlock_timeout;
SHOW log_lock_waits;

-- ============================================================================
-- Section 10: Prepared Transaction Monitoring
-- ============================================================================

-- Monitor prepared transactions
SELECT
    gid AS global_transaction_id,
    prepared,
    owner,
    database,
    transaction,
    age(NOW(), prepared) AS time_in_prepared_state
FROM pg_prepared_xacts
ORDER BY prepared;

-- Alert on old prepared transactions
DO $$
DECLARE
    old_prepared_count INT;
BEGIN
    SELECT COUNT(*) INTO old_prepared_count
    FROM pg_prepared_xacts
    WHERE age(NOW(), prepared) > interval '1 hour';

    IF old_prepared_count > 0 THEN
        RAISE WARNING '% prepared transactions older than 1 hour', old_prepared_count;
    END IF;
END $$;

-- ============================================================================
-- Section 11: Transaction Log (WAL) Monitoring
-- ============================================================================

-- WAL statistics
SELECT
    pg_current_wal_lsn() AS current_wal_location,
    pg_wal_lsn_diff(pg_current_wal_lsn(), '0/0') AS wal_bytes_generated;

-- WAL generation rate
SELECT
    checkpoints_timed,
    checkpoints_req AS checkpoints_requested,
    checkpoint_write_time,
    checkpoint_sync_time,
    buffers_checkpoint,
    buffers_clean,
    buffers_backend,
    buffers_backend_fsync,
    buffers_alloc,
    stats_reset
FROM pg_stat_bgwriter;

-- ============================================================================
-- Section 12: Table-Level Transaction Statistics
-- ============================================================================

CREATE TABLE test_table_statistics (
    id SERIAL PRIMARY KEY,
    value INT
);

-- Generate activity
INSERT INTO test_table_statistics (value)
SELECT i FROM generate_series(1, 1000) i;

UPDATE test_table_statistics SET value = value + 1 WHERE id <= 100;

DELETE FROM test_table_statistics WHERE id <= 10;

-- Query table statistics
SELECT
    relname AS table_name,
    n_tup_ins AS inserts,
    n_tup_upd AS updates,
    n_tup_del AS deletes,
    n_tup_hot_upd AS hot_updates,
    n_live_tup AS live_tuples,
    n_dead_tup AS dead_tuples,
    n_mod_since_analyze AS modifications_since_analyze,
    last_vacuum,
    last_autovacuum,
    last_analyze,
    last_autoanalyze,
    vacuum_count,
    autovacuum_count,
    analyze_count,
    autoanalyze_count
FROM pg_stat_user_tables
WHERE relname = 'test_table_statistics';

-- ============================================================================
-- Section 13: Transaction Audit Trail
-- ============================================================================

CREATE TABLE test_transaction_audit (
    audit_id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    operation VARCHAR(10),
    old_data JSONB,
    new_data JSONB,
    changed_by VARCHAR(100),
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    transaction_id BIGINT DEFAULT txid_current()
);

CREATE TABLE test_audited_table (
    id SERIAL PRIMARY KEY,
    value TEXT,
    status VARCHAR(50)
);

-- Audit trigger function
CREATE FUNCTION audit_transaction_changes() RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        INSERT INTO test_transaction_audit (table_name, operation, new_data, changed_by)
        VALUES (TG_TABLE_NAME, 'INSERT', row_to_json(NEW)::JSONB, current_user);
    ELSIF TG_OP = 'UPDATE' THEN
        INSERT INTO test_transaction_audit (table_name, operation, old_data, new_data, changed_by)
        VALUES (TG_TABLE_NAME, 'UPDATE', row_to_json(OLD)::JSONB, row_to_json(NEW)::JSONB, current_user);
    ELSIF TG_OP = 'DELETE' THEN
        INSERT INTO test_transaction_audit (table_name, operation, old_data, changed_by)
        VALUES (TG_TABLE_NAME, 'DELETE', row_to_json(OLD)::JSONB, current_user);
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_audit_changes
AFTER INSERT OR UPDATE OR DELETE ON test_audited_table
FOR EACH ROW
EXECUTE FUNCTION audit_transaction_changes();

-- Generate audited activity
BEGIN;
INSERT INTO test_audited_table (value, status) VALUES ('Data 1', 'active');
INSERT INTO test_audited_table (value, status) VALUES ('Data 2', 'active');
COMMIT;

BEGIN;
UPDATE test_audited_table SET status = 'inactive' WHERE id = 1;
COMMIT;

BEGIN;
DELETE FROM test_audited_table WHERE id = 2;
COMMIT;

-- Query audit trail
SELECT
    audit_id,
    table_name,
    operation,
    changed_by,
    changed_at,
    transaction_id,
    old_data,
    new_data
FROM test_transaction_audit
ORDER BY audit_id;

-- ============================================================================
-- Section 14: Statement-Level Statistics
-- ============================================================================

-- Enable pg_stat_statements extension (if available)
-- CREATE EXTENSION IF NOT EXISTS pg_stat_statements;

-- Query statement statistics
-- SELECT
--     query,
--     calls,
--     total_time,
--     mean_time,
--     min_time,
--     max_time,
--     rows
-- FROM pg_stat_statements
-- ORDER BY total_time DESC
-- LIMIT 20;

-- ============================================================================
-- Section 15: Connection Pool Monitoring
-- ============================================================================

-- Active connections by application
SELECT
    application_name,
    state,
    COUNT(*) AS connection_count,
    MAX(age(NOW(), backend_start)) AS oldest_connection_age
FROM pg_stat_activity
GROUP BY application_name, state
ORDER BY connection_count DESC;

-- Connection limits
SELECT
    setting::INT AS max_connections,
    (SELECT COUNT(*) FROM pg_stat_activity) AS current_connections,
    setting::INT - (SELECT COUNT(*) FROM pg_stat_activity) AS available_connections
FROM pg_settings
WHERE name = 'max_connections';

-- ============================================================================
-- Section 16: Slow Query Logging
-- ============================================================================

-- Slow query configuration
SHOW log_min_duration_statement;
SHOW log_statement;

-- Set slow query logging (session level)
SET log_min_duration_statement = '1s';

-- Queries taking > 1s will be logged

-- Reset
SET log_min_duration_statement = '-1';

-- ============================================================================
-- Section 17: Transaction Isolation Level Monitoring
-- ============================================================================

-- Current isolation level
SHOW default_transaction_isolation;

-- Isolation level distribution of active transactions
SELECT
    CASE
        WHEN query LIKE '%SERIALIZABLE%' THEN 'SERIALIZABLE'
        WHEN query LIKE '%REPEATABLE READ%' THEN 'REPEATABLE READ'
        WHEN query LIKE '%READ COMMITTED%' THEN 'READ COMMITTED'
        ELSE 'DEFAULT'
    END AS isolation_level,
    COUNT(*) AS transaction_count
FROM pg_stat_activity
WHERE xact_start IS NOT NULL
GROUP BY isolation_level;

-- ============================================================================
-- Section 18: Troubleshooting Toolkit
-- ============================================================================

CREATE TABLE test_troubleshooting_queries (
    query_id INT PRIMARY KEY,
    category VARCHAR(50),
    description TEXT,
    query_template TEXT
);

INSERT INTO test_troubleshooting_queries VALUES
    (1, 'Active', 'Find all active transactions', 'SELECT * FROM pg_stat_activity WHERE state = ''active'''),
    (2, 'Blocking', 'Find blocking queries', 'SELECT blocked.pid, blocking.pid FROM pg_locks blocked JOIN pg_locks blocking ...'),
    (3, 'Long-running', 'Find transactions > 5 min', 'SELECT * FROM pg_stat_activity WHERE age(NOW(), xact_start) > interval ''5 min'''),
    (4, 'Idle', 'Find idle in transaction', 'SELECT * FROM pg_stat_activity WHERE state = ''idle in transaction'''),
    (5, 'Locks', 'Current lock count by type', 'SELECT locktype, COUNT(*) FROM pg_locks GROUP BY locktype'),
    (6, 'Deadlocks', 'Deadlock statistics', 'SELECT datname, deadlocks FROM pg_stat_database'),
    (7, 'Prepared', 'Prepared transactions', 'SELECT * FROM pg_prepared_xacts'),
    (8, 'Connections', 'Connection counts', 'SELECT application_name, COUNT(*) FROM pg_stat_activity GROUP BY application_name'),
    (9, 'Cache Hit', 'Database cache hit ratio', 'SELECT blks_hit::FLOAT / (blks_hit + blks_read) FROM pg_stat_database'),
    (10, 'Bloat', 'Table bloat (dead tuples)', 'SELECT relname, n_dead_tup FROM pg_stat_user_tables ORDER BY n_dead_tup DESC');

SELECT query_id, category, description FROM test_troubleshooting_queries ORDER BY query_id;

-- ============================================================================
-- Section 19: Performance Dashboard Query
-- ============================================================================

-- Comprehensive performance snapshot
SELECT
    'Transaction Stats' AS metric_category,
    jsonb_build_object(
        'commits', d.xact_commit,
        'rollbacks', d.xact_rollback,
        'active_transactions', (SELECT COUNT(*) FROM pg_stat_activity WHERE xact_start IS NOT NULL),
        'idle_in_transaction', (SELECT COUNT(*) FROM pg_stat_activity WHERE state = 'idle in transaction')
    ) AS metrics
FROM pg_stat_database d
WHERE d.datname = current_database()

UNION ALL

SELECT
    'Lock Stats' AS metric_category,
    jsonb_build_object(
        'total_locks', (SELECT COUNT(*) FROM pg_locks),
        'granted_locks', (SELECT COUNT(*) FROM pg_locks WHERE granted = true),
        'waiting_locks', (SELECT COUNT(*) FROM pg_locks WHERE granted = false)
    ) AS metrics

UNION ALL

SELECT
    'Cache Stats' AS metric_category,
    jsonb_build_object(
        'cache_hit_ratio', ROUND(100.0 * d.blks_hit / NULLIF(d.blks_hit + d.blks_read, 0), 2),
        'disk_blocks_read', d.blks_read,
        'cache_blocks_hit', d.blks_hit
    ) AS metrics
FROM pg_stat_database d
WHERE d.datname = current_database();

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_monitoring_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_monitoring_best_practices VALUES
    (1, 'Monitor pg_stat_activity for long-running transactions'),
    (2, 'Alert on "idle in transaction" sessions (waste resources)'),
    (3, 'Track commit/rollback ratio for application health'),
    (4, 'Monitor blocking queries to identify contention'),
    (5, 'Check transaction ID age to prevent wraparound'),
    (6, 'Monitor deadlock statistics and investigate causes'),
    (7, 'Alert on old prepared transactions (> 1 hour)'),
    (8, 'Track WAL generation rate for capacity planning'),
    (9, 'Monitor table-level statistics (inserts, updates, deletes)'),
    (10, 'Implement audit trail for critical tables'),
    (11, 'Use pg_stat_statements for query performance analysis'),
    (12, 'Monitor connection pool utilization'),
    (13, 'Enable slow query logging (log_min_duration_statement)'),
    (14, 'Track isolation level usage'),
    (15, 'Create troubleshooting toolkit with common queries'),
    (16, 'Build performance dashboard with key metrics'),
    (17, 'Set up alerts for resource limits (connections, locks)'),
    (18, 'Monitor cache hit ratio (should be > 95%)'),
    (19, 'Track checkpoint frequency and I/O impact'),
    (20, 'Regularly review and analyze database logs');

SELECT id, guideline FROM test_monitoring_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_monitoring_best_practices;
DROP TABLE test_troubleshooting_queries;
DROP TABLE test_transaction_audit;
DROP TABLE test_audited_table;
DROP FUNCTION audit_transaction_changes();
DROP TABLE test_table_statistics;
DROP TABLE test_blocking_detection;
DROP TABLE test_commit_rollback_ratio;
DROP TABLE test_active_monitoring;

DROP DATABASE test_transaction_monitoring_db;

-- End of transaction monitoring tests
