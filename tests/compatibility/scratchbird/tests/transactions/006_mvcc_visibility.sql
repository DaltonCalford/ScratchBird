-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - MVCC and Visibility Rules
-- Description: Comprehensive MVCC (Multi-Version Concurrency Control) testing
-- ============================================================================

-- MVCC: PostgreSQL's concurrency control mechanism
-- Each transaction sees consistent snapshot of data
-- Multiple versions of rows maintained
-- Visibility determined by transaction IDs and snapshots
-- No read locks needed for SELECT queries

-- Create test database
CREATE DATABASE test_mvcc_visibility_db;
USE test_mvcc_visibility_db;

-- ============================================================================
-- Section 1: Basic MVCC Snapshot Behavior
-- ============================================================================

CREATE TABLE test_mvcc_basic (
    id SERIAL PRIMARY KEY,
    value TEXT,
    modified_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_mvcc_basic (value) VALUES ('Initial value');

-- Transaction sees snapshot at start time
BEGIN;

-- Read initial value
SELECT id, value FROM test_mvcc_basic WHERE id = 1;

-- Simulate concurrent update (in separate transaction)
-- This would be done in a separate session:
-- UPDATE test_mvcc_basic SET value = 'Updated by other transaction' WHERE id = 1;

-- Original transaction still sees initial value (snapshot isolation)
SELECT id, value FROM test_mvcc_basic WHERE id = 1;

COMMIT;

-- After commit, sees latest committed value
SELECT id, value FROM test_mvcc_basic WHERE id = 1;

-- ============================================================================
-- Section 2: Transaction ID (XID) Visibility
-- ============================================================================

CREATE TABLE test_xid_visibility (
    id SERIAL PRIMARY KEY,
    data TEXT,
    created_xid BIGINT DEFAULT pg_current_xact_id()
);

-- Insert in transaction
BEGIN;
SELECT pg_current_xact_id() AS current_xid;
INSERT INTO test_xid_visibility (data) VALUES ('Data from txn 1');
COMMIT;

BEGIN;
SELECT pg_current_xact_id() AS current_xid;
INSERT INTO test_xid_visibility (data) VALUES ('Data from txn 2');
COMMIT;

-- Query transaction IDs
SELECT id, data, created_xid FROM test_xid_visibility ORDER BY id;

-- Check current transaction ID
SELECT pg_current_xact_id() AS current_xid;

-- ============================================================================
-- Section 3: Committed vs Uncommitted Changes Visibility
-- ============================================================================

CREATE TABLE test_commit_visibility (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    value INT
);

INSERT INTO test_commit_visibility (status, value) VALUES ('committed', 100);

-- Uncommitted changes not visible to other transactions
BEGIN;

INSERT INTO test_commit_visibility (status, value) VALUES ('uncommitted', 200);

-- Within same transaction, uncommitted changes are visible
SELECT status, value FROM test_commit_visibility ORDER BY id;

-- Don't commit yet - other transactions won't see second row
-- ROLLBACK or COMMIT happens later

ROLLBACK;

-- After rollback, only committed data exists
SELECT status, value FROM test_commit_visibility ORDER BY id;

-- ============================================================================
-- Section 4: Snapshot Isolation Level Behavior
-- ============================================================================

CREATE TABLE test_snapshot_isolation (
    id SERIAL PRIMARY KEY,
    counter INT
);

INSERT INTO test_snapshot_isolation (counter) VALUES (0);

-- REPEATABLE READ: transaction sees snapshot from its start
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

-- Initial read
SELECT id, counter FROM test_snapshot_isolation WHERE id = 1;

-- Simulate concurrent update
-- Another transaction: UPDATE test_snapshot_isolation SET counter = 5 WHERE id = 1; COMMIT;

-- This transaction still sees counter = 0 (snapshot from transaction start)
SELECT id, counter FROM test_snapshot_isolation WHERE id = 1;

COMMIT;

-- Now sees updated value
SELECT id, counter FROM test_snapshot_isolation WHERE id = 1;

-- ============================================================================
-- Section 5: READ COMMITTED Visibility Behavior
-- ============================================================================

CREATE TABLE test_read_committed (
    id SERIAL PRIMARY KEY,
    value TEXT
);

INSERT INTO test_read_committed (value) VALUES ('Value 1');

-- READ COMMITTED: sees committed changes during transaction
BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED;

-- Read value
SELECT id, value FROM test_read_committed WHERE id = 1;

-- Simulate concurrent committed update
-- Another transaction: UPDATE test_read_committed SET value = 'Value 2' WHERE id = 1; COMMIT;

-- READ COMMITTED sees the committed change (non-repeatable read)
SELECT id, value FROM test_read_committed WHERE id = 1;

COMMIT;

-- ============================================================================
-- Section 6: Deleted Rows Visibility
-- ============================================================================

CREATE TABLE test_deleted_visibility (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_deleted_visibility (data) VALUES ('Row 1'), ('Row 2'), ('Row 3');

-- Transaction 1: Long-running query
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

-- Sees all 3 rows
SELECT COUNT(*) FROM test_deleted_visibility;

-- Transaction 2 deletes row (in separate session)
-- DELETE FROM test_deleted_visibility WHERE id = 2; COMMIT;

-- Transaction 1 still sees all 3 rows (snapshot isolation)
SELECT COUNT(*) FROM test_deleted_visibility;

COMMIT;

-- After commit, sees deleted row is gone
SELECT COUNT(*) FROM test_deleted_visibility;
SELECT id, data FROM test_deleted_visibility ORDER BY id;

-- ============================================================================
-- Section 7: Tuple Visibility and System Columns
-- ============================================================================

CREATE TABLE test_tuple_visibility (
    id SERIAL PRIMARY KEY,
    value TEXT
);

INSERT INTO test_tuple_visibility (value) VALUES ('Visible row');

-- Query system columns (xmin, xmax show transaction IDs)
SELECT
    id,
    value,
    xmin,  -- Transaction ID that created this tuple
    xmax,  -- Transaction ID that deleted/updated (0 if current)
    ctid   -- Physical location (page, tuple)
FROM test_tuple_visibility;

-- Update creates new tuple version
UPDATE test_tuple_visibility SET value = 'Updated value' WHERE id = 1;

-- xmin changed (new tuple version)
SELECT
    id,
    value,
    xmin,
    xmax,
    ctid
FROM test_tuple_visibility;

-- ============================================================================
-- Section 8: MVCC and UPDATE Operations
-- ============================================================================

CREATE TABLE test_mvcc_updates (
    id SERIAL PRIMARY KEY,
    counter INT,
    description TEXT
);

INSERT INTO test_mvcc_updates (counter, description) VALUES (1, 'Initial');

-- Multiple updates create multiple tuple versions
BEGIN;

SELECT xmin, xmax, counter, description FROM test_mvcc_updates WHERE id = 1;

UPDATE test_mvcc_updates SET counter = 2, description = 'Update 1' WHERE id = 1;

SELECT xmin, xmax, counter, description FROM test_mvcc_updates WHERE id = 1;

UPDATE test_mvcc_updates SET counter = 3, description = 'Update 2' WHERE id = 1;

SELECT xmin, xmax, counter, description FROM test_mvcc_updates WHERE id = 1;

COMMIT;

-- Final version visible
SELECT xmin, counter, description FROM test_mvcc_updates WHERE id = 1;

-- ============================================================================
-- Section 9: Visibility of INSERT Operations
-- ============================================================================

CREATE TABLE test_insert_visibility (
    id SERIAL PRIMARY KEY,
    session_name VARCHAR(100),
    data TEXT
);

-- Transaction 1: Long-running
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

-- Initial count
SELECT COUNT(*) FROM test_insert_visibility;

-- Transaction 2 inserts rows (in separate session)
-- INSERT INTO test_insert_visibility (session_name, data)
-- SELECT 'Session 2', 'Data ' || i FROM generate_series(1, 10) i;
-- COMMIT;

-- Transaction 1 doesn't see new rows (phantom read prevention)
SELECT COUNT(*) FROM test_insert_visibility;

COMMIT;

-- After commit, sees new rows
SELECT COUNT(*) FROM test_insert_visibility;

-- ============================================================================
-- Section 10: Vacuum and Dead Tuples
-- ============================================================================

CREATE TABLE test_vacuum_dead_tuples (
    id SERIAL PRIMARY KEY,
    value INT
);

-- Create many tuple versions
INSERT INTO test_vacuum_dead_tuples (value) SELECT i FROM generate_series(1, 100) i;

-- Multiple updates create dead tuples
UPDATE test_vacuum_dead_tuples SET value = value + 1000;
UPDATE test_vacuum_dead_tuples SET value = value + 2000;

-- Check dead tuples (requires pg_stat_all_tables)
SELECT
    n_live_tup AS live_tuples,
    n_dead_tup AS dead_tuples,
    last_autovacuum
FROM pg_stat_user_tables
WHERE relname = 'test_vacuum_dead_tuples';

-- Manual vacuum reclaims dead tuples
VACUUM test_vacuum_dead_tuples;

-- Check after vacuum
SELECT
    n_live_tup AS live_tuples,
    n_dead_tup AS dead_tuples
FROM pg_stat_user_tables
WHERE relname = 'test_vacuum_dead_tuples';

-- ============================================================================
-- Section 11: Transaction Wraparound Prevention
-- ============================================================================

CREATE TABLE test_txid_wraparound (
    id SERIAL PRIMARY KEY,
    data TEXT,
    age INT
);

INSERT INTO test_txid_wraparound (data) VALUES ('Data 1');

-- Query transaction age
SELECT
    age(relfrozenxid) AS xid_age,
    pg_size_pretty(pg_total_relation_size('test_txid_wraparound')) AS table_size
FROM pg_class
WHERE relname = 'test_txid_wraparound';

-- autovacuum prevents wraparound
SELECT
    name,
    setting,
    unit
FROM pg_settings
WHERE name IN (
    'autovacuum_freeze_max_age',
    'vacuum_freeze_min_age',
    'vacuum_freeze_table_age'
)
ORDER BY name;

-- ============================================================================
-- Section 12: Concurrent Transaction Snapshot Comparison
-- ============================================================================

CREATE TABLE test_snapshot_comparison (
    id SERIAL PRIMARY KEY,
    value INT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_snapshot_comparison (value) VALUES (100);

-- Transaction 1 (REPEATABLE READ)
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

SELECT txid_current_snapshot() AS snapshot_t1;
SELECT value FROM test_snapshot_comparison WHERE id = 1;

-- Simulate concurrent update
-- Transaction 2: UPDATE test_snapshot_comparison SET value = 200 WHERE id = 1; COMMIT;

-- Transaction 1 still sees value = 100
SELECT value FROM test_snapshot_comparison WHERE id = 1;

COMMIT;

-- ============================================================================
-- Section 13: Serializable Snapshot Isolation
-- ============================================================================

CREATE TABLE test_ssi (
    id SERIAL PRIMARY KEY,
    balance NUMERIC(10,2)
);

INSERT INTO test_ssi (balance) VALUES (1000.00), (500.00);

-- SERIALIZABLE detects conflicts
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;

-- Read sum
SELECT SUM(balance) AS total FROM test_ssi;

-- Concurrent transaction modifies
-- UPDATE test_ssi SET balance = balance + 100 WHERE id = 1;

-- Update based on read
UPDATE test_ssi SET balance = balance + 50 WHERE id = 2;

COMMIT;

SELECT id, balance FROM test_ssi ORDER BY id;

-- ============================================================================
-- Section 14: Visibility Rules for Different Operations
-- ============================================================================

CREATE TABLE test_visibility_rules (
    id SERIAL PRIMARY KEY,
    operation VARCHAR(50),
    visible_to_self BOOLEAN,
    visible_to_others_uncommitted BOOLEAN,
    visible_to_others_committed BOOLEAN
);

INSERT INTO test_visibility_rules VALUES
    (1, 'INSERT (uncommitted)', TRUE, FALSE, FALSE),
    (2, 'INSERT (committed)', TRUE, TRUE, TRUE),
    (3, 'UPDATE (uncommitted)', TRUE, FALSE, FALSE),
    (4, 'UPDATE (committed)', TRUE, TRUE, TRUE),
    (5, 'DELETE (uncommitted)', FALSE, TRUE, TRUE),
    (6, 'DELETE (committed)', FALSE, FALSE, FALSE);

SELECT
    operation,
    visible_to_self AS "Same Txn",
    visible_to_others_uncommitted AS "Other Txn (Uncommitted)",
    visible_to_others_committed AS "Other Txn (After Commit)"
FROM test_visibility_rules
ORDER BY id;

-- ============================================================================
-- Section 15: Hot Standby and Snapshot Conflicts
-- ============================================================================

-- Hot standby queries can conflict with WAL replay
CREATE TABLE test_hot_standby (
    id SERIAL PRIMARY KEY,
    data TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_hot_standby (data)
SELECT 'Data ' || i FROM generate_series(1, 100) i;

-- Query hot standby settings
SELECT
    name,
    setting,
    unit
FROM pg_settings
WHERE name IN (
    'hot_standby',
    'max_standby_archive_delay',
    'max_standby_streaming_delay'
)
ORDER BY name;

-- ============================================================================
-- Section 16: Snapshot Export and Import
-- ============================================================================

CREATE TABLE test_snapshot_export (
    id SERIAL PRIMARY KEY,
    value TEXT
);

INSERT INTO test_snapshot_export (value) VALUES ('Value at export');

-- Export snapshot (for parallel workers)
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

SELECT pg_export_snapshot() AS exported_snapshot;

-- Another session can import this snapshot
-- SET TRANSACTION SNAPSHOT 'snapshot_id';

COMMIT;

-- ============================================================================
-- Section 17: Visibility with Subtransactions
-- ============================================================================

CREATE TABLE test_subtransaction_visibility (
    id SERIAL PRIMARY KEY,
    value TEXT
);

-- Subtransaction with savepoint
BEGIN;

INSERT INTO test_subtransaction_visibility (value) VALUES ('Value 1');
SAVEPOINT sp1;

INSERT INTO test_subtransaction_visibility (value) VALUES ('Value 2');
SAVEPOINT sp2;

INSERT INTO test_subtransaction_visibility (value) VALUES ('Value 3');

-- Rollback to sp1 (removes Value 2 and Value 3)
ROLLBACK TO SAVEPOINT sp1;

INSERT INTO test_subtransaction_visibility (value) VALUES ('Value 4');

COMMIT;

-- Only Value 1 and Value 4 exist
SELECT id, value FROM test_subtransaction_visibility ORDER BY id;

-- ============================================================================
-- Section 18: MVCC Performance Implications
-- ============================================================================

CREATE TABLE test_mvcc_performance (
    id SERIAL PRIMARY KEY,
    data TEXT,
    version INT DEFAULT 1
);

-- Initial insert
INSERT INTO test_mvcc_performance (data)
SELECT 'Data ' || i FROM generate_series(1, 1000) i;

-- Many updates create many tuple versions
UPDATE test_mvcc_performance SET version = 2;
UPDATE test_mvcc_performance SET version = 3;
UPDATE test_mvcc_performance SET version = 4;

-- Check bloat
SELECT
    pg_size_pretty(pg_total_relation_size('test_mvcc_performance')) AS total_size,
    pg_size_pretty(pg_relation_size('test_mvcc_performance')) AS table_size,
    n_live_tup,
    n_dead_tup
FROM pg_stat_user_tables
WHERE relname = 'test_mvcc_performance';

-- VACUUM FULL reclaims space
VACUUM FULL test_mvcc_performance;

-- Check after vacuum
SELECT
    pg_size_pretty(pg_total_relation_size('test_mvcc_performance')) AS total_size,
    n_dead_tup
FROM pg_stat_user_tables
WHERE relname = 'test_mvcc_performance';

-- ============================================================================
-- Section 19: Visibility Map and Index-Only Scans
-- ============================================================================

CREATE TABLE test_visibility_map (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50),
    value INT
);

CREATE INDEX idx_vm_code ON test_visibility_map(code);

-- Insert data
INSERT INTO test_visibility_map (code, value)
SELECT 'CODE_' || (i % 100), i FROM generate_series(1, 10000) i;

-- VACUUM updates visibility map
VACUUM ANALYZE test_visibility_map;

-- Index-only scan possible when visibility map indicates all tuples visible
EXPLAIN (COSTS OFF)
SELECT code FROM test_visibility_map WHERE code = 'CODE_50';

-- Query visibility map pages
SELECT
    pg_size_pretty(pg_relation_size('test_visibility_map')) AS table_size,
    pg_size_pretty(pg_relation_size('idx_vm_code')) AS index_size;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_mvcc_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_mvcc_best_practices VALUES
    (1, 'MVCC allows high read concurrency without read locks'),
    (2, 'Each transaction sees consistent snapshot of data'),
    (3, 'UPDATE and DELETE create new tuple versions (old versions kept)'),
    (4, 'Dead tuples are reclaimed by VACUUM'),
    (5, 'REPEATABLE READ uses snapshot from transaction start'),
    (6, 'READ COMMITTED uses new snapshot for each statement'),
    (7, 'xmin shows transaction ID that created tuple'),
    (8, 'xmax shows transaction ID that deleted/updated tuple (0 if current)'),
    (9, 'Frequent updates cause table bloat (many dead tuples)'),
    (10, 'Regular VACUUM prevents bloat and transaction wraparound'),
    (11, 'VACUUM FULL reclaims space but requires exclusive lock'),
    (12, 'autovacuum automatically maintains tables'),
    (13, 'Visibility map enables index-only scans'),
    (14, 'Hot standby queries can conflict with WAL replay'),
    (15, 'Snapshot export enables parallel workers to share snapshot'),
    (16, 'Subtransactions (savepoints) have their own visibility'),
    (17, 'Monitor n_dead_tup to identify bloat'),
    (18, 'Long-running transactions prevent VACUUM from reclaiming tuples'),
    (19, 'SERIALIZABLE uses predicate locking to detect conflicts'),
    (20, 'MVCC overhead is acceptable trade-off for concurrency benefits');

SELECT id, guideline FROM test_mvcc_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_mvcc_best_practices;
DROP TABLE test_visibility_map;
DROP TABLE test_mvcc_performance;
DROP TABLE test_subtransaction_visibility;
DROP TABLE test_snapshot_export;
DROP TABLE test_hot_standby;
DROP TABLE test_visibility_rules;
DROP TABLE test_ssi;
DROP TABLE test_snapshot_comparison;
DROP TABLE test_txid_wraparound;
DROP TABLE test_vacuum_dead_tuples;
DROP TABLE test_insert_visibility;
DROP TABLE test_mvcc_updates;
DROP TABLE test_tuple_visibility;
DROP TABLE test_deleted_visibility;
DROP TABLE test_read_committed;
DROP TABLE test_snapshot_isolation;
DROP TABLE test_commit_visibility;
DROP TABLE test_xid_visibility;
DROP TABLE test_mvcc_basic;

DROP DATABASE test_mvcc_visibility_db;

-- End of MVCC visibility tests
