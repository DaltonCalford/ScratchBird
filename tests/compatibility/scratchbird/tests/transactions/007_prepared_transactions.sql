-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - Prepared Transactions and Two-Phase Commit
-- Description: Comprehensive prepared transaction testing (2PC)
-- ============================================================================

-- Prepared Transactions: Two-phase commit protocol
-- PREPARE TRANSACTION: First phase - prepare to commit
-- COMMIT PREPARED: Second phase - commit prepared transaction
-- ROLLBACK PREPARED: Abort prepared transaction
-- Used for distributed transactions across multiple databases

-- Create test database
CREATE DATABASE test_prepared_transactions_db;
USE test_prepared_transactions_db;

-- ============================================================================
-- Section 1: Basic PREPARE TRANSACTION
-- ============================================================================

CREATE TABLE test_prepare_basic (
    id SERIAL PRIMARY KEY,
    value TEXT,
    prepared_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Basic prepared transaction
BEGIN;

INSERT INTO test_prepare_basic (value) VALUES ('Prepared value 1');

-- Prepare transaction with global ID
PREPARE TRANSACTION 'txn_global_001';

-- Transaction is prepared but not committed yet
-- Data not visible to other transactions

-- Query prepared transactions
SELECT
    gid AS global_id,
    prepared,
    owner,
    database
FROM pg_prepared_xacts
WHERE gid = 'txn_global_001';

-- Commit the prepared transaction
COMMIT PREPARED 'txn_global_001';

-- Now data is visible
SELECT id, value FROM test_prepare_basic ORDER BY id;

-- ============================================================================
-- Section 2: ROLLBACK PREPARED
-- ============================================================================

BEGIN;

INSERT INTO test_prepare_basic (value) VALUES ('Prepared value 2');

-- Prepare transaction
PREPARE TRANSACTION 'txn_global_002';

-- Check prepared state
SELECT gid, prepared FROM pg_prepared_xacts WHERE gid = 'txn_global_002';

-- Rollback instead of commit
ROLLBACK PREPARED 'txn_global_002';

-- Data not inserted
SELECT id, value FROM test_prepare_basic ORDER BY id;

-- No prepared transaction remaining
SELECT COUNT(*) FROM pg_prepared_xacts WHERE gid = 'txn_global_002';

-- ============================================================================
-- Section 3: Multiple Prepared Transactions
-- ============================================================================

CREATE TABLE test_multi_prepare (
    id SERIAL PRIMARY KEY,
    txn_id VARCHAR(50),
    data TEXT
);

-- Prepare multiple transactions
BEGIN;
INSERT INTO test_multi_prepare (txn_id, data) VALUES ('txn_001', 'Data from txn 1');
PREPARE TRANSACTION 'multi_txn_001';

BEGIN;
INSERT INTO test_multi_prepare (txn_id, data) VALUES ('txn_002', 'Data from txn 2');
PREPARE TRANSACTION 'multi_txn_002';

BEGIN;
INSERT INTO test_multi_prepare (txn_id, data) VALUES ('txn_003', 'Data from txn 3');
PREPARE TRANSACTION 'multi_txn_003';

-- Check all prepared transactions
SELECT gid, prepared, database FROM pg_prepared_xacts ORDER BY gid;

-- Commit them in order
COMMIT PREPARED 'multi_txn_001';
COMMIT PREPARED 'multi_txn_002';
COMMIT PREPARED 'multi_txn_003';

-- All data committed
SELECT id, txn_id, data FROM test_multi_prepare ORDER BY id;

-- ============================================================================
-- Section 4: Prepared Transaction with Multiple Operations
-- ============================================================================

CREATE TABLE test_accounts_2pc (
    account_id SERIAL PRIMARY KEY,
    account_name VARCHAR(200),
    balance NUMERIC(12,2)
);

CREATE TABLE test_transfers_2pc (
    transfer_id SERIAL PRIMARY KEY,
    from_account INT,
    to_account INT,
    amount NUMERIC(12,2),
    transfer_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_accounts_2pc (account_name, balance) VALUES
    ('Alice', 1000.00),
    ('Bob', 500.00);

-- Multi-operation prepared transaction
BEGIN;

-- Debit
UPDATE test_accounts_2pc SET balance = balance - 100.00 WHERE account_id = 1;

-- Credit
UPDATE test_accounts_2pc SET balance = balance + 100.00 WHERE account_id = 2;

-- Log
INSERT INTO test_transfers_2pc (from_account, to_account, amount) VALUES (1, 2, 100.00);

-- Prepare
PREPARE TRANSACTION 'transfer_txn_001';

-- Check prepared state
SELECT gid FROM pg_prepared_xacts WHERE gid = 'transfer_txn_001';

-- Commit
COMMIT PREPARED 'transfer_txn_001';

-- Verify all operations completed
SELECT account_id, account_name, balance FROM test_accounts_2pc ORDER BY account_id;
SELECT transfer_id, from_account, to_account, amount FROM test_transfers_2pc;

-- ============================================================================
-- Section 5: Prepared Transaction Configuration
-- ============================================================================

-- Query 2PC configuration
SELECT
    name,
    setting,
    unit,
    short_desc
FROM pg_settings
WHERE name IN (
    'max_prepared_transactions',
    'max_locks_per_transaction'
)
ORDER BY name;

-- max_prepared_transactions must be > 0 to use prepared transactions
-- If 0, PREPARE TRANSACTION will fail

-- ============================================================================
-- Section 6: Prepared Transaction Timeout
-- ============================================================================

CREATE TABLE test_prepare_timeout (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Prepare transaction
BEGIN;
INSERT INTO test_prepare_timeout (data) VALUES ('Timeout test');
PREPARE TRANSACTION 'timeout_txn_001';

-- Check age of prepared transaction
SELECT
    gid,
    prepared,
    age(NOW(), prepared) AS age
FROM pg_prepared_xacts
WHERE gid = 'timeout_txn_001';

-- In production, prepared transactions should be resolved quickly
-- Long-running prepared transactions hold resources

-- Clean up
COMMIT PREPARED 'timeout_txn_001';

-- ============================================================================
-- Section 7: Prepared Transaction and Locks
-- ============================================================================

CREATE TABLE test_prepare_locks (
    id SERIAL PRIMARY KEY,
    value INT
);

INSERT INTO test_prepare_locks (value) VALUES (100);

-- Prepared transaction holds locks
BEGIN;

SELECT * FROM test_prepare_locks WHERE id = 1 FOR UPDATE;

UPDATE test_prepare_locks SET value = 200 WHERE id = 1;

PREPARE TRANSACTION 'lock_txn_001';

-- Locks are held even after PREPARE
-- Other transactions trying to update this row will wait

-- Query locks
SELECT
    locktype,
    relation::regclass AS table_name,
    mode,
    granted
FROM pg_locks
WHERE pid IN (
    SELECT pid FROM pg_prepared_xacts WHERE gid = 'lock_txn_001'
);

-- Commit releases locks
COMMIT PREPARED 'lock_txn_001';

SELECT id, value FROM test_prepare_locks;

-- ============================================================================
-- Section 8: Prepared Transaction Recovery
-- ============================================================================

-- Prepared transactions survive server restart
-- Used for crash recovery in distributed transactions

CREATE TABLE test_prepare_recovery (
    id SERIAL PRIMARY KEY,
    data TEXT,
    recovered BOOLEAN DEFAULT FALSE
);

BEGIN;
INSERT INTO test_prepare_recovery (data) VALUES ('Recovery test');
PREPARE TRANSACTION 'recovery_txn_001';

-- Simulate recovery: query and commit prepared transaction
SELECT gid, database FROM pg_prepared_xacts WHERE gid = 'recovery_txn_001';

-- Recovery manager would commit or rollback
COMMIT PREPARED 'recovery_txn_001';

SELECT id, data FROM test_prepare_recovery;

-- ============================================================================
-- Section 9: Distributed Transaction Coordinator Pattern
-- ============================================================================

CREATE TABLE test_dtc_log (
    txn_id VARCHAR(100) PRIMARY KEY,
    status VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    prepared_at TIMESTAMP,
    completed_at TIMESTAMP
);

-- DTC pattern: Coordinator tracks distributed transaction
CREATE FUNCTION prepare_distributed_txn(p_txn_id TEXT) RETURNS VOID AS $$
BEGIN
    -- Log transaction
    INSERT INTO test_dtc_log (txn_id, status) VALUES (p_txn_id, 'preparing');

    -- Prepare on this database
    EXECUTE format('PREPARE TRANSACTION %L', p_txn_id);

    -- Update status
    UPDATE test_dtc_log
    SET status = 'prepared', prepared_at = CURRENT_TIMESTAMP
    WHERE txn_id = p_txn_id;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION commit_distributed_txn(p_txn_id TEXT) RETURNS VOID AS $$
BEGIN
    -- Commit prepared transaction
    EXECUTE format('COMMIT PREPARED %L', p_txn_id);

    -- Update status
    UPDATE test_dtc_log
    SET status = 'committed', completed_at = CURRENT_TIMESTAMP
    WHERE txn_id = p_txn_id;
END;
$$ LANGUAGE plpgsql;

-- Use DTC pattern
BEGIN;
INSERT INTO test_prepare_basic (value) VALUES ('DTC test');
SELECT prepare_distributed_txn('dtc_txn_001');

-- Check log
SELECT txn_id, status, prepared_at FROM test_dtc_log WHERE txn_id = 'dtc_txn_001';

-- Commit
SELECT commit_distributed_txn('dtc_txn_001');

-- Check final status
SELECT txn_id, status, completed_at FROM test_dtc_log WHERE txn_id = 'dtc_txn_001';

-- ============================================================================
-- Section 10: Error Handling in Prepared Transactions
-- ============================================================================

-- Cannot PREPARE with active cursors
CREATE TABLE test_prepare_errors (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_prepare_errors (data) VALUES ('Error test 1'), ('Error test 2');

-- Attempt prepare with cursor (fails)
DO $$
BEGIN
    BEGIN
        BEGIN;

        DECLARE cur CURSOR FOR SELECT * FROM test_prepare_errors;

        -- This will fail: cannot prepare with open cursors
        PREPARE TRANSACTION 'cursor_txn_001';

    EXCEPTION
        WHEN OTHERS THEN
            ROLLBACK;
            RAISE NOTICE 'Cannot prepare transaction with open cursors';
    END;
END $$;

-- ============================================================================
-- Section 11: Prepared Transaction Monitoring
-- ============================================================================

CREATE TABLE test_prepare_monitoring (
    id SERIAL PRIMARY KEY,
    monitor_data TEXT
);

-- Create prepared transaction for monitoring
BEGIN;
INSERT INTO test_prepare_monitoring (monitor_data) VALUES ('Monitoring test');
PREPARE TRANSACTION 'monitor_txn_001';

-- Monitor prepared transactions
SELECT
    gid AS transaction_id,
    prepared AS prepare_time,
    owner,
    database,
    age(NOW(), prepared) AS time_in_prepared_state
FROM pg_prepared_xacts
ORDER BY prepared;

-- Monitor locks held by prepared transactions
SELECT
    p.gid,
    l.locktype,
    l.mode,
    l.granted
FROM pg_prepared_xacts p
LEFT JOIN pg_locks l ON p.transaction = l.transactionid
WHERE p.gid = 'monitor_txn_001';

-- Clean up
COMMIT PREPARED 'monitor_txn_001';

-- ============================================================================
-- Section 12: Prepared Transaction Limits
-- ============================================================================

-- Check current prepared transaction count
SELECT COUNT(*) AS prepared_count FROM pg_prepared_xacts;

-- Check limit
SELECT setting::INT AS max_prepared FROM pg_settings WHERE name = 'max_prepared_transactions';

-- Cannot exceed max_prepared_transactions limit
-- Attempting to prepare more transactions than limit will fail

-- ============================================================================
-- Section 13: Two-Phase Commit with Foreign Keys
-- ============================================================================

CREATE TABLE test_2pc_parent (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(200)
);

CREATE TABLE test_2pc_child (
    child_id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_2pc_parent(parent_id),
    child_name VARCHAR(200)
);

-- Two-phase commit with FK validation
BEGIN;

INSERT INTO test_2pc_parent (parent_name) VALUES ('Parent 2PC');

INSERT INTO test_2pc_child (parent_id, child_name) VALUES (1, 'Child 2PC');

-- FK validated during prepare
PREPARE TRANSACTION 'fk_txn_001';

-- Commit
COMMIT PREPARED 'fk_txn_001';

-- Verify referential integrity
SELECT
    p.parent_id,
    p.parent_name,
    c.child_name
FROM test_2pc_parent p
JOIN test_2pc_child c ON p.parent_id = c.parent_id;

-- ============================================================================
-- Section 14: Prepared Transaction and Triggers
-- ============================================================================

CREATE TABLE test_2pc_trigger_data (
    id SERIAL PRIMARY KEY,
    value TEXT
);

CREATE TABLE test_2pc_trigger_log (
    log_id SERIAL PRIMARY KEY,
    data_id INT,
    action TEXT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION log_2pc_change() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_2pc_trigger_log (data_id, action)
    VALUES (NEW.id, TG_OP);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_2pc_log
AFTER INSERT ON test_2pc_trigger_data
FOR EACH ROW
EXECUTE FUNCTION log_2pc_change();

-- Prepared transaction with trigger
BEGIN;

INSERT INTO test_2pc_trigger_data (value) VALUES ('2PC trigger test');

-- Trigger executes but not committed yet
PREPARE TRANSACTION 'trigger_2pc_001';

-- Changes not visible until commit
SELECT COUNT(*) FROM test_2pc_trigger_data;
SELECT COUNT(*) FROM test_2pc_trigger_log;

-- Commit
COMMIT PREPARED 'trigger_2pc_001';

-- Both inserts committed
SELECT id, value FROM test_2pc_trigger_data;
SELECT data_id, action FROM test_2pc_trigger_log;

-- ============================================================================
-- Section 15: Prepared Transaction Identifier Guidelines
-- ============================================================================

CREATE TABLE test_gid_guidelines (
    guideline_id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_gid_guidelines VALUES
    (1, 'Global ID (GID) must be unique across all databases'),
    (2, 'GID max length is 200 characters'),
    (3, 'Use format: app_name:node_id:sequence'),
    (4, 'Include timestamp for debugging'),
    (5, 'Coordinator generates GID for distributed transaction'),
    (6, 'All participants use same GID'),
    (7, 'GID persists across restarts'),
    (8, 'Choose GID format that aids troubleshooting');

SELECT guideline_id, guideline FROM test_gid_guidelines ORDER BY guideline_id;

-- Example GID formats
-- 'app1:node1:seq12345'
-- 'dtc:20250101120000:txn001'
-- 'myapp:srv1:2025010112:uuid'

-- ============================================================================
-- Section 16: Coordinator Failure Recovery
-- ============================================================================

CREATE TABLE test_coordinator_recovery (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Simulate coordinator failure scenario
BEGIN;
INSERT INTO test_coordinator_recovery (data) VALUES ('Pre-failure data');
PREPARE TRANSACTION 'coord_fail_txn_001';

-- Coordinator fails before sending COMMIT PREPARED
-- Transaction remains in prepared state

-- Query orphaned prepared transactions
SELECT
    gid,
    prepared,
    age(NOW(), prepared) AS time_in_prepared
FROM pg_prepared_xacts
WHERE gid = 'coord_fail_txn_001';

-- Recovery process:
-- 1. Identify orphaned prepared transactions
-- 2. Query transaction coordinator for status
-- 3. COMMIT PREPARED or ROLLBACK PREPARED based on coordinator decision

-- Simulate recovery
COMMIT PREPARED 'coord_fail_txn_001';

-- ============================================================================
-- Section 17: Prepared Transaction Performance Impact
-- ============================================================================

CREATE TABLE test_2pc_performance (
    id SERIAL PRIMARY KEY,
    value INT
);

-- Prepared transactions are slower than regular commits
-- Additional overhead:
-- - Persist transaction state to disk
-- - Hold locks longer
-- - Require two-phase protocol

-- Measure regular commit
BEGIN;
INSERT INTO test_2pc_performance (value) SELECT i FROM generate_series(1, 100) i;
COMMIT;

-- Measure prepared transaction
BEGIN;
INSERT INTO test_2pc_performance (value) SELECT i FROM generate_series(101, 200) i;
PREPARE TRANSACTION 'perf_txn_001';
COMMIT PREPARED 'perf_txn_001';

SELECT COUNT(*) FROM test_2pc_performance;

-- Use 2PC only when necessary (distributed transactions)
-- Not needed for single-database transactions

-- ============================================================================
-- Section 18: Prepared Transaction State Persistence
-- ============================================================================

-- Prepared transactions are written to pg_twophase directory
-- Persists across server restarts
-- Recovery manager resolves prepared transactions on startup

CREATE TABLE test_2pc_persistence (
    id SERIAL PRIMARY KEY,
    data TEXT
);

BEGIN;
INSERT INTO test_2pc_persistence (data) VALUES ('Persistent prepared transaction');
PREPARE TRANSACTION 'persist_txn_001';

-- Query prepared transaction details
SELECT
    gid,
    prepared,
    owner,
    database,
    transaction
FROM pg_prepared_xacts
WHERE gid = 'persist_txn_001';

-- This transaction survives restart
-- Must be explicitly committed or rolled back

COMMIT PREPARED 'persist_txn_001';

-- ============================================================================
-- Section 19: XA Transaction Standard Compliance
-- ============================================================================

-- PostgreSQL 2PC is compatible with XA standard
-- Used by JTA (Java Transaction API), .NET System.Transactions, etc.

CREATE TABLE test_xa_compliance (
    id INT PRIMARY KEY,
    standard TEXT,
    description TEXT
);

INSERT INTO test_xa_compliance VALUES
    (1, 'XA Prepare', 'Maps to PREPARE TRANSACTION'),
    (2, 'XA Commit', 'Maps to COMMIT PREPARED'),
    (3, 'XA Rollback', 'Maps to ROLLBACK PREPARED'),
    (4, 'XA Recover', 'Maps to pg_prepared_xacts query'),
    (5, 'XID Format', 'Global transaction ID format'),
    (6, 'Resource Manager', 'PostgreSQL acts as XA resource manager');

SELECT id, standard, description FROM test_xa_compliance ORDER BY id;

-- XA transaction managers use PREPARE/COMMIT PREPARED for distributed transactions

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_2pc_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_2pc_best_practices VALUES
    (1, 'Use 2PC only for distributed transactions across multiple databases'),
    (2, 'Regular transactions are faster - use when possible'),
    (3, 'Set max_prepared_transactions > 0 to enable 2PC'),
    (4, 'Generate unique Global IDs (GID) for each transaction'),
    (5, 'Include metadata in GID for troubleshooting'),
    (6, 'Resolve prepared transactions quickly to release locks'),
    (7, 'Monitor pg_prepared_xacts for orphaned transactions'),
    (8, 'Implement coordinator recovery logic'),
    (9, 'Prepared transactions survive server restart'),
    (10, 'Cannot prepare with open cursors or temporary tables'),
    (11, 'Prepared transactions hold locks until resolved'),
    (12, '2PC adds overhead - use sparingly'),
    (13, 'XA-compliant transaction managers use 2PC'),
    (14, 'Log all prepare/commit/rollback decisions in coordinator'),
    (15, 'Set reasonable timeout for prepared transaction resolution'),
    (16, 'Test coordinator failure scenarios'),
    (17, 'Automate orphaned transaction cleanup'),
    (18, 'Prepared transactions written to pg_twophase directory'),
    (19, 'Use pg_locks to monitor locks held by prepared transactions'),
    (20, 'Document 2PC usage and recovery procedures');

SELECT id, guideline FROM test_2pc_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_2pc_best_practices;
DROP TABLE test_xa_compliance;
DROP TABLE test_2pc_persistence;
DROP TABLE test_2pc_performance;
DROP TABLE test_coordinator_recovery;
DROP TABLE test_gid_guidelines;
DROP TABLE test_2pc_trigger_log;
DROP TABLE test_2pc_trigger_data;
DROP FUNCTION log_2pc_change();
DROP TABLE test_2pc_child;
DROP TABLE test_2pc_parent;
DROP TABLE test_prepare_monitoring;
DROP TABLE test_prepare_errors;
DROP FUNCTION commit_distributed_txn(TEXT);
DROP FUNCTION prepare_distributed_txn(TEXT);
DROP TABLE test_dtc_log;
DROP TABLE test_prepare_recovery;
DROP TABLE test_prepare_locks;
DROP TABLE test_prepare_timeout;
DROP TABLE test_transfers_2pc;
DROP TABLE test_accounts_2pc;
DROP TABLE test_multi_prepare;
DROP TABLE test_prepare_basic;

DROP DATABASE test_prepared_transactions_db;

-- End of prepared transactions tests
