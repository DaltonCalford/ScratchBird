-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - Isolation Levels
-- Description: Comprehensive transaction isolation level testing
-- ============================================================================

-- Isolation Levels (SQL Standard):
-- - READ UNCOMMITTED: Lowest isolation, dirty reads possible
-- - READ COMMITTED: Default in PostgreSQL, no dirty reads
-- - REPEATABLE READ: No non-repeatable reads
-- - SERIALIZABLE: Highest isolation, prevents all anomalies

-- PostgreSQL specifics:
-- - READ UNCOMMITTED behaves like READ COMMITTED
-- - Uses MVCC (Multi-Version Concurrency Control)

-- Create test database
CREATE DATABASE test_isolation_levels_db;
USE test_isolation_levels_db;

-- ============================================================================
-- Section 1: Default Isolation Level (READ COMMITTED)
-- ============================================================================

CREATE TABLE test_accounts (
    account_id SERIAL PRIMARY KEY,
    account_name VARCHAR(200),
    balance NUMERIC(12,2)
);

INSERT INTO test_accounts (account_name, balance) VALUES
    ('Alice', 1000.00),
    ('Bob', 500.00);

-- Check default isolation level
SHOW default_transaction_isolation;

-- Default behavior: READ COMMITTED
BEGIN;
SELECT balance FROM test_accounts WHERE account_name = 'Alice';
-- Sees committed changes from other transactions
COMMIT;

-- ============================================================================
-- Section 2: Setting READ COMMITTED
-- ============================================================================

BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED;

SELECT account_name, balance FROM test_accounts WHERE account_name = 'Alice';

-- Changes from other committed transactions are visible
-- (simulated by understanding behavior)

COMMIT;

-- ============================================================================
-- Section 3: READ UNCOMMITTED (Behaves as READ COMMITTED)
-- ============================================================================

-- PostgreSQL doesn't implement true READ UNCOMMITTED
-- It behaves identically to READ COMMITTED

BEGIN TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;

SELECT account_name, balance FROM test_accounts;

-- No dirty reads possible (PostgreSQL MVCC)

COMMIT;

-- ============================================================================
-- Section 4: REPEATABLE READ Basic
-- ============================================================================

-- REPEATABLE READ: Sees snapshot at transaction start
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

-- Initial read
SELECT balance FROM test_accounts WHERE account_name = 'Alice';

-- Even if another transaction commits changes,
-- this transaction sees the snapshot from its start

-- Demonstrate with same transaction
SELECT balance FROM test_accounts WHERE account_name = 'Alice';

COMMIT;

-- ============================================================================
-- Section 5: REPEATABLE READ Phantom Reads
-- ============================================================================

CREATE TABLE test_products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2)
);

INSERT INTO test_products (product_name, price) VALUES
    ('Widget A', 29.99),
    ('Widget B', 49.99);

-- REPEATABLE READ prevents phantom reads
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

SELECT COUNT(*) FROM test_products WHERE price > 20;

-- Another transaction inserts a product (in real scenario)
-- This transaction still sees original count

SELECT COUNT(*) FROM test_products WHERE price > 20;

COMMIT;

-- ============================================================================
-- Section 6: SERIALIZABLE Isolation
-- ============================================================================

-- SERIALIZABLE: Strictest isolation
-- Prevents all anomalies including serialization anomalies

BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;

SELECT SUM(balance) FROM test_accounts;

-- Concurrent modifications may cause serialization failure
-- Transaction would need to retry

COMMIT;

-- ============================================================================
-- Section 7: Isolation Level for Single Transaction
-- ============================================================================

-- Set for specific transaction
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;

BEGIN;
SELECT * FROM test_accounts;
COMMIT;

-- Next transaction uses default again
BEGIN;
SHOW transaction_isolation;
COMMIT;

-- ============================================================================
-- Section 8: Session-Level Isolation Level
-- ============================================================================

-- Set for entire session
SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL REPEATABLE READ;

BEGIN;
SHOW transaction_isolation;
COMMIT;

-- Reset to default
SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ COMMITTED;

-- ============================================================================
-- Section 9: Non-Repeatable Read Prevention
-- ============================================================================

-- READ COMMITTED allows non-repeatable reads
-- REPEATABLE READ prevents them

CREATE TABLE test_inventory (
    item_id SERIAL PRIMARY KEY,
    item_name VARCHAR(200),
    quantity INT
);

INSERT INTO test_inventory (item_name, quantity) VALUES ('Item A', 100);

-- Scenario 1: READ COMMITTED (non-repeatable read possible)
BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED;

SELECT quantity FROM test_inventory WHERE item_id = 1;
-- Returns 100

-- Another transaction updates (in real scenario)
-- UPDATE test_inventory SET quantity = 150 WHERE item_id = 1;

-- Read again in same transaction
SELECT quantity FROM test_inventory WHERE item_id = 1;
-- Could return 150 (non-repeatable read)

COMMIT;

-- Scenario 2: REPEATABLE READ (prevents non-repeatable read)
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

SELECT quantity FROM test_inventory WHERE item_id = 1;
-- Returns committed value at transaction start

-- Even if another transaction commits UPDATE,
-- this transaction still sees original value

SELECT quantity FROM test_inventory WHERE item_id = 1;
-- Returns same value (repeatable)

COMMIT;

-- ============================================================================
-- Section 10: Write Skew Anomaly
-- ============================================================================

CREATE TABLE test_on_call (
    doctor_id INT PRIMARY KEY,
    doctor_name VARCHAR(200),
    on_call BOOLEAN
);

INSERT INTO test_on_call VALUES
    (1, 'Dr. Alice', TRUE),
    (2, 'Dr. Bob', TRUE);

-- SERIALIZABLE prevents write skew
-- (where two transactions read same data, make decisions, write conflicting data)

BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;

-- Check at least one doctor on call
SELECT COUNT(*) FROM test_on_call WHERE on_call = TRUE;
-- Returns 2

-- If count > 1, can go off call
-- UPDATE test_on_call SET on_call = FALSE WHERE doctor_id = 1;

COMMIT;

-- ============================================================================
-- Section 11: Lost Update Prevention
-- ============================================================================

CREATE TABLE test_counters (
    counter_id INT PRIMARY KEY,
    counter_value BIGINT
);

INSERT INTO test_counters VALUES (1, 100);

-- READ COMMITTED: lost update possible (without explicit locking)
-- REPEATABLE READ/SERIALIZABLE: prevents lost update

BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

SELECT counter_value FROM test_counters WHERE counter_id = 1;
-- Read value: 100

-- Increment
UPDATE test_counters SET counter_value = counter_value + 1 WHERE counter_id = 1;

COMMIT;

-- ============================================================================
-- Section 12: Dirty Read Prevention
-- ============================================================================

-- All PostgreSQL isolation levels prevent dirty reads

CREATE TABLE test_dirty_read (
    id SERIAL PRIMARY KEY,
    value TEXT
);

INSERT INTO test_dirty_read (value) VALUES ('Initial');

-- Transaction 1 (simulated)
-- BEGIN;
-- UPDATE test_dirty_read SET value = 'Modified' WHERE id = 1;
-- -- Not committed yet

-- Transaction 2: Cannot see uncommitted changes
BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED;
SELECT value FROM test_dirty_read WHERE id = 1;
-- Returns 'Initial' (no dirty read)
COMMIT;

-- ============================================================================
-- Section 13: Serialization Failure Handling
-- ============================================================================

CREATE TABLE test_serialization (
    id INT PRIMARY KEY,
    value INT
);

INSERT INTO test_serialization VALUES (1, 100), (2, 200);

-- SERIALIZABLE may cause serialization failures
DO $$
BEGIN
    BEGIN
        SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;

        -- Read and write
        PERFORM * FROM test_serialization WHERE id = 1;
        UPDATE test_serialization SET value = value + 10 WHERE id = 1;

    EXCEPTION
        WHEN serialization_failure THEN
            RAISE NOTICE 'Serialization failure, would retry';
    END;
END $$;

-- ============================================================================
-- Section 14: Isolation Level Effects on SELECT FOR UPDATE
-- ============================================================================

CREATE TABLE test_select_for_update (
    id SERIAL PRIMARY KEY,
    data TEXT,
    locked_at TIMESTAMP
);

INSERT INTO test_select_for_update (data) VALUES ('Data 1'), ('Data 2');

-- SELECT FOR UPDATE behavior with different isolation levels
BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED;

-- Lock rows for update
SELECT * FROM test_select_for_update WHERE id = 1 FOR UPDATE;

-- Other transactions wait for lock

UPDATE test_select_for_update SET data = 'Updated', locked_at = CURRENT_TIMESTAMP WHERE id = 1;

COMMIT;

-- ============================================================================
-- Section 15: Deferred Constraint Checking with Isolation
-- ============================================================================

CREATE TABLE test_deferred_iso (
    id INT PRIMARY KEY,
    parent_id INT,
    CONSTRAINT fk_parent FOREIGN KEY (parent_id) REFERENCES test_deferred_iso(id)
        DEFERRABLE INITIALLY DEFERRED
);

BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;

SET CONSTRAINTS ALL DEFERRED;

-- Can temporarily violate constraint
INSERT INTO test_deferred_iso VALUES (2, 1);
INSERT INTO test_deferred_iso VALUES (1, NULL);

-- Constraint checked at commit
COMMIT;

SELECT id, parent_id FROM test_deferred_iso ORDER BY id;

-- ============================================================================
-- Section 16: Isolation Level Comparison Table
-- ============================================================================

CREATE TABLE test_isolation_comparison (
    isolation_level VARCHAR(50) PRIMARY KEY,
    dirty_read BOOLEAN,
    nonrepeatable_read BOOLEAN,
    phantom_read BOOLEAN,
    serialization_anomaly BOOLEAN
);

INSERT INTO test_isolation_comparison VALUES
    ('READ UNCOMMITTED', FALSE, TRUE, TRUE, TRUE),  -- PostgreSQL: same as READ COMMITTED
    ('READ COMMITTED', FALSE, TRUE, TRUE, TRUE),
    ('REPEATABLE READ', FALSE, FALSE, FALSE, TRUE),
    ('SERIALIZABLE', FALSE, FALSE, FALSE, FALSE);

SELECT * FROM test_isolation_comparison ORDER BY
    CASE isolation_level
        WHEN 'READ UNCOMMITTED' THEN 1
        WHEN 'READ COMMITTED' THEN 2
        WHEN 'REPEATABLE READ' THEN 3
        WHEN 'SERIALIZABLE' THEN 4
    END;

-- ============================================================================
-- Section 17: Checking Current Isolation Level
-- ============================================================================

BEGIN;

-- Query current transaction's isolation level
SHOW transaction_isolation;

-- Also available via function
SELECT current_setting('transaction_isolation') AS current_isolation;

COMMIT;

-- ============================================================================
-- Section 18: Mixing Isolation Levels
-- ============================================================================

-- Different transactions can use different isolation levels

-- Transaction 1: READ COMMITTED
BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED;
SELECT * FROM test_accounts;
COMMIT;

-- Transaction 2: SERIALIZABLE
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SELECT * FROM test_accounts;
COMMIT;

-- ============================================================================
-- Section 19: Isolation Level Performance Impact
-- ============================================================================

CREATE TABLE test_perf_isolation (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_perf_isolation (data)
SELECT 'Data ' || i FROM generate_series(1, 1000) i;

-- READ COMMITTED: Better concurrency, lower isolation
BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED;
SELECT COUNT(*) FROM test_perf_isolation;
COMMIT;

-- SERIALIZABLE: Lower concurrency, higher isolation
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SELECT COUNT(*) FROM test_perf_isolation;
COMMIT;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_isolation_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_isolation_best_practices VALUES
    (1, 'READ COMMITTED is default and suitable for most applications'),
    (2, 'PostgreSQL READ UNCOMMITTED behaves like READ COMMITTED'),
    (3, 'REPEATABLE READ prevents non-repeatable reads'),
    (4, 'SERIALIZABLE prevents all anomalies but may reduce concurrency'),
    (5, 'Choose lowest isolation level that meets requirements'),
    (6, 'Higher isolation = lower concurrency, more conflicts'),
    (7, 'SERIALIZABLE may require retry logic for failures'),
    (8, 'Test with realistic concurrent workloads'),
    (9, 'PostgreSQL uses MVCC for efficient isolation'),
    (10, 'Use SELECT FOR UPDATE for explicit row locking'),
    (11, 'Set isolation level at transaction start'),
    (12, 'Session-level setting affects all transactions'),
    (13, 'Monitor serialization failures in production'),
    (14, 'READ COMMITTED suitable for most OLTP workloads'),
    (15, 'SERIALIZABLE for critical financial transactions');

SELECT id, guideline FROM test_isolation_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_isolation_best_practices;
DROP TABLE test_perf_isolation;
DROP TABLE test_isolation_comparison;
DROP TABLE test_deferred_iso;
DROP TABLE test_select_for_update;
DROP TABLE test_serialization;
DROP TABLE test_dirty_read;
DROP TABLE test_counters;
DROP TABLE test_on_call;
DROP TABLE test_inventory;
DROP TABLE test_products;
DROP TABLE test_accounts;

DROP DATABASE test_isolation_levels_db;

-- End of isolation level tests
