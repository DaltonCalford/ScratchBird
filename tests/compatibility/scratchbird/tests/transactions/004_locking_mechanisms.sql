-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - Locking Mechanisms
-- Description: Comprehensive locking and concurrency control testing
-- ============================================================================

-- Locking mechanisms: Row-level locks, table-level locks, advisory locks
-- SELECT FOR UPDATE: Lock rows for update
-- SELECT FOR SHARE: Lock rows for read (share lock)
-- LOCK TABLE: Explicit table-level locking
-- Advisory locks: Application-level coordination

-- Create test database
CREATE DATABASE test_locking_mechanisms_db;
USE test_locking_mechanisms_db;

-- ============================================================================
-- Section 1: SELECT FOR UPDATE Basic
-- ============================================================================

CREATE TABLE test_inventory (
    item_id SERIAL PRIMARY KEY,
    item_name VARCHAR(200),
    quantity INT,
    reserved INT DEFAULT 0
);

INSERT INTO test_inventory (item_name, quantity) VALUES
    ('Widget A', 100),
    ('Widget B', 50),
    ('Widget C', 200);

-- Lock row for update
BEGIN;

SELECT item_id, item_name, quantity
FROM test_inventory
WHERE item_id = 1
FOR UPDATE;

-- Other transactions trying to update this row will wait
-- Simulate update
UPDATE test_inventory SET reserved = 10 WHERE item_id = 1;

COMMIT;

SELECT item_id, item_name, quantity, reserved FROM test_inventory ORDER BY item_id;

-- ============================================================================
-- Section 2: SELECT FOR UPDATE with Multiple Rows
-- ============================================================================

BEGIN;

-- Lock multiple rows
SELECT item_id, item_name, quantity
FROM test_inventory
WHERE quantity > 50
FOR UPDATE;

-- Update locked rows
UPDATE test_inventory SET reserved = reserved + 5 WHERE quantity > 50;

COMMIT;

SELECT item_id, item_name, quantity, reserved FROM test_inventory ORDER BY item_id;

-- ============================================================================
-- Section 3: SELECT FOR SHARE (Shared Lock)
-- ============================================================================

CREATE TABLE test_products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2),
    stock INT
);

INSERT INTO test_products (product_name, price, stock) VALUES
    ('Product A', 29.99, 100),
    ('Product B', 49.99, 50);

-- Shared lock allows concurrent reads
BEGIN;

SELECT product_id, product_name, price
FROM test_products
WHERE product_id = 1
FOR SHARE;

-- Other transactions can also acquire FOR SHARE on same row
-- But cannot acquire FOR UPDATE or modify the row

COMMIT;

-- ============================================================================
-- Section 4: SELECT FOR UPDATE NOWAIT
-- ============================================================================

CREATE TABLE test_reservations (
    reservation_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    seat_number INT,
    status VARCHAR(50)
);

INSERT INTO test_reservations (customer_name, seat_number, status) VALUES
    ('Alice', 101, 'pending'),
    ('Bob', 102, 'pending');

-- Non-blocking lock attempt
BEGIN;

-- Try to lock row, fail immediately if already locked
DO $$
BEGIN
    PERFORM * FROM test_reservations WHERE reservation_id = 1 FOR UPDATE NOWAIT;
    RAISE NOTICE 'Lock acquired';
EXCEPTION
    WHEN lock_not_available THEN
        RAISE NOTICE 'Row is locked, cannot proceed';
END $$;

COMMIT;

-- ============================================================================
-- Section 5: SELECT FOR UPDATE SKIP LOCKED
-- ============================================================================

CREATE TABLE test_work_queue (
    task_id SERIAL PRIMARY KEY,
    task_name VARCHAR(200),
    status VARCHAR(50) DEFAULT 'pending',
    assigned_to VARCHAR(100)
);

INSERT INTO test_work_queue (task_name) VALUES
    ('Task 1'),
    ('Task 2'),
    ('Task 3'),
    ('Task 4'),
    ('Task 5');

-- Worker pattern: get first available unlocked task
BEGIN;

-- Skip locked rows, get first available
SELECT task_id, task_name
FROM test_work_queue
WHERE status = 'pending'
ORDER BY task_id
LIMIT 1
FOR UPDATE SKIP LOCKED;

-- Simulate task assignment
UPDATE test_work_queue
SET status = 'processing', assigned_to = 'Worker1'
WHERE task_id = 1;

COMMIT;

SELECT task_id, task_name, status, assigned_to FROM test_work_queue ORDER BY task_id;

-- ============================================================================
-- Section 6: LOCK TABLE Explicit Locking
-- ============================================================================

CREATE TABLE test_batch_processing (
    batch_id SERIAL PRIMARY KEY,
    batch_name VARCHAR(200),
    processed BOOLEAN DEFAULT FALSE
);

INSERT INTO test_batch_processing (batch_name) VALUES
    ('Batch 1'),
    ('Batch 2'),
    ('Batch 3');

-- Explicit table lock
BEGIN;

-- Lock entire table in ACCESS EXCLUSIVE mode
LOCK TABLE test_batch_processing IN ACCESS EXCLUSIVE MODE;

-- Exclusive access to table
UPDATE test_batch_processing SET processed = TRUE;

COMMIT;

SELECT batch_id, batch_name, processed FROM test_batch_processing ORDER BY batch_id;

-- ============================================================================
-- Section 7: LOCK TABLE with Different Lock Modes
-- ============================================================================

CREATE TABLE test_lock_modes (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_lock_modes (data) VALUES ('Data 1'), ('Data 2');

-- ROW SHARE: Compatible with most operations
BEGIN;
LOCK TABLE test_lock_modes IN ROW SHARE MODE;
SELECT * FROM test_lock_modes;
COMMIT;

-- ROW EXCLUSIVE: For INSERT, UPDATE, DELETE
BEGIN;
LOCK TABLE test_lock_modes IN ROW EXCLUSIVE MODE;
UPDATE test_lock_modes SET data = 'Updated' WHERE id = 1;
COMMIT;

-- SHARE: For read-only access preventing writes
BEGIN;
LOCK TABLE test_lock_modes IN SHARE MODE;
SELECT * FROM test_lock_modes;
COMMIT;

-- EXCLUSIVE: Prevents all concurrent access except SELECT
BEGIN;
LOCK TABLE test_lock_modes IN EXCLUSIVE MODE;
UPDATE test_lock_modes SET data = 'Exclusive Update' WHERE id = 2;
COMMIT;

SELECT id, data FROM test_lock_modes ORDER BY id;

-- ============================================================================
-- Section 8: Advisory Locks - Session Level
-- ============================================================================

-- Advisory locks: Application-level coordination
-- Lock ID: arbitrary integer

-- Acquire session-level advisory lock
SELECT pg_advisory_lock(12345) AS lock_acquired;

-- Check if lock is held
SELECT objid, classid, locktype
FROM pg_locks
WHERE locktype = 'advisory' AND objid = 12345;

-- Release advisory lock
SELECT pg_advisory_unlock(12345) AS lock_released;

-- Verify release
SELECT COUNT(*) FROM pg_locks WHERE locktype = 'advisory' AND objid = 12345;

-- ============================================================================
-- Section 9: Advisory Locks - Transaction Level
-- ============================================================================

BEGIN;

-- Transaction-level advisory lock (auto-released at commit/rollback)
SELECT pg_advisory_xact_lock(54321) AS xact_lock_acquired;

-- Do work with lock held
CREATE TEMP TABLE temp_advisory_work (id INT, data TEXT);
INSERT INTO temp_advisory_work VALUES (1, 'Protected data');

-- Lock automatically released at commit
COMMIT;

-- Verify lock released
SELECT COUNT(*) FROM pg_locks WHERE locktype = 'advisory' AND objid = 54321;

-- ============================================================================
-- Section 10: Advisory Locks - Try Lock (Non-blocking)
-- ============================================================================

-- Try to acquire lock, return immediately
SELECT pg_try_advisory_lock(99999) AS lock_acquired;  -- Returns true

-- Check lock
SELECT COUNT(*) FROM pg_locks WHERE locktype = 'advisory' AND objid = 99999;

-- Try to acquire again (same session - succeeds)
SELECT pg_try_advisory_lock(99999) AS second_attempt;  -- Returns true

-- Release
SELECT pg_advisory_unlock(99999);
SELECT pg_advisory_unlock(99999);  -- Need to unlock twice (recursive)

-- ============================================================================
-- Section 11: Deadlock Detection
-- ============================================================================

CREATE TABLE test_account_a (
    account_id INT PRIMARY KEY,
    balance NUMERIC(12,2)
);

CREATE TABLE test_account_b (
    account_id INT PRIMARY KEY,
    balance NUMERIC(12,2)
);

INSERT INTO test_account_a VALUES (1, 1000.00);
INSERT INTO test_account_b VALUES (1, 500.00);

-- Deadlock scenario (simulated with single transaction showing pattern)
-- Transaction 1: Lock A, then try to lock B
-- Transaction 2: Lock B, then try to lock A
-- PostgreSQL detects deadlock and aborts one transaction

DO $$
BEGIN
    BEGIN
        -- Simulate deadlock scenario
        -- In real scenario, this would be two concurrent transactions

        -- Transaction 1 pattern:
        -- SELECT * FROM test_account_a WHERE account_id = 1 FOR UPDATE;
        -- SELECT * FROM test_account_b WHERE account_id = 1 FOR UPDATE;

        -- Transaction 2 pattern (reverse order):
        -- SELECT * FROM test_account_b WHERE account_id = 1 FOR UPDATE;
        -- SELECT * FROM test_account_a WHERE account_id = 1 FOR UPDATE;

        RAISE NOTICE 'Deadlock would occur with concurrent transactions';

    EXCEPTION
        WHEN deadlock_detected THEN
            RAISE NOTICE 'Deadlock detected and resolved';
    END;
END $$;

-- ============================================================================
-- Section 12: Lock Timeout Configuration
-- ============================================================================

CREATE TABLE test_timeout_locks (
    id SERIAL PRIMARY KEY,
    value TEXT
);

INSERT INTO test_timeout_locks (value) VALUES ('Value 1'), ('Value 2');

-- Set lock timeout
BEGIN;
SET LOCAL lock_timeout = '2s';

-- Try to acquire lock (would timeout after 2 seconds if unavailable)
SELECT * FROM test_timeout_locks WHERE id = 1 FOR UPDATE;

COMMIT;

-- ============================================================================
-- Section 13: Statement Timeout vs Lock Timeout
-- ============================================================================

BEGIN;

-- Statement timeout: entire statement duration
SET LOCAL statement_timeout = '5s';

-- Lock timeout: time waiting for lock
SET LOCAL lock_timeout = '2s';

SELECT * FROM test_timeout_locks LIMIT 1;

COMMIT;

-- Query current settings
SHOW statement_timeout;
SHOW lock_timeout;

-- ============================================================================
-- Section 14: Monitoring Locks
-- ============================================================================

CREATE TABLE test_lock_monitoring (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_lock_monitoring (data) VALUES ('Monitored data');

-- Query current locks
SELECT
    locktype,
    relation::regclass AS table_name,
    mode,
    granted,
    pid
FROM pg_locks
WHERE relation = 'test_lock_monitoring'::regclass;

-- Query blocking locks
SELECT
    blocked_locks.pid AS blocked_pid,
    blocking_locks.pid AS blocking_pid,
    blocked_activity.usename AS blocked_user,
    blocking_activity.usename AS blocking_user,
    blocked_activity.query AS blocked_query,
    blocking_activity.query AS blocking_query
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
-- Section 15: FOR UPDATE OF Specific Tables
-- ============================================================================

CREATE TABLE test_orders (
    order_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    total NUMERIC(10,2)
);

CREATE TABLE test_order_details (
    detail_id SERIAL PRIMARY KEY,
    order_id INT REFERENCES test_orders(order_id),
    product VARCHAR(200),
    quantity INT
);

INSERT INTO test_orders (customer_name, total) VALUES ('Customer A', 100.00);
INSERT INTO test_order_details (order_id, product, quantity) VALUES (1, 'Product X', 2);

-- Lock only specific table in join
BEGIN;

SELECT o.order_id, o.customer_name, d.product
FROM test_orders o
JOIN test_order_details d ON o.order_id = d.order_id
WHERE o.order_id = 1
FOR UPDATE OF o;  -- Lock only test_orders, not test_order_details

UPDATE test_orders SET total = 150.00 WHERE order_id = 1;

COMMIT;

SELECT o.order_id, o.customer_name, o.total, d.product
FROM test_orders o
JOIN test_order_details d ON o.order_id = d.order_id;

-- ============================================================================
-- Section 16: Lock Compatibility Matrix
-- ============================================================================

CREATE TABLE test_lock_compatibility (
    lock_mode VARCHAR(50) PRIMARY KEY,
    access_share BOOLEAN,
    row_share BOOLEAN,
    row_exclusive BOOLEAN,
    share_update_exclusive BOOLEAN,
    share BOOLEAN,
    share_row_exclusive BOOLEAN,
    exclusive BOOLEAN,
    access_exclusive BOOLEAN
);

-- Lock compatibility (TRUE = compatible, FALSE = conflicts)
INSERT INTO test_lock_compatibility VALUES
    ('ACCESS SHARE', TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE),
    ('ROW SHARE', TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE),
    ('ROW EXCLUSIVE', TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE),
    ('SHARE UPDATE EXCLUSIVE', TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE),
    ('SHARE', TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE),
    ('SHARE ROW EXCLUSIVE', TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE),
    ('EXCLUSIVE', FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE),
    ('ACCESS EXCLUSIVE', FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);

SELECT * FROM test_lock_compatibility ORDER BY lock_mode;

-- ============================================================================
-- Section 17: Row-Level Lock Conflicts
-- ============================================================================

CREATE TABLE test_row_conflicts (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    updated_at TIMESTAMP
);

INSERT INTO test_row_conflicts (status) VALUES ('pending'), ('pending'), ('pending');

-- Demonstrate row-level conflict
BEGIN;

-- Lock row 1
SELECT * FROM test_row_conflicts WHERE id = 1 FOR UPDATE;

-- Other rows can still be locked by other transactions
-- Only row 1 is blocked

UPDATE test_row_conflicts SET status = 'processing', updated_at = CURRENT_TIMESTAMP WHERE id = 1;

COMMIT;

SELECT id, status, updated_at FROM test_row_conflicts ORDER BY id;

-- ============================================================================
-- Section 18: Advisory Lock Use Cases
-- ============================================================================

-- Use Case 1: Singleton process (ensure only one instance)
CREATE FUNCTION ensure_singleton_process(process_name TEXT) RETURNS BOOLEAN AS $$
DECLARE
    lock_id BIGINT;
BEGIN
    -- Hash process name to integer
    lock_id := hashtext(process_name)::BIGINT;

    -- Try to acquire lock
    IF pg_try_advisory_lock(lock_id) THEN
        RAISE NOTICE 'Process % started', process_name;
        RETURN TRUE;
    ELSE
        RAISE NOTICE 'Process % already running', process_name;
        RETURN FALSE;
    END IF;
END;
$$ LANGUAGE plpgsql;

SELECT ensure_singleton_process('batch_processor');
SELECT ensure_singleton_process('batch_processor');  -- Already running

-- Cleanup
SELECT pg_advisory_unlock(hashtext('batch_processor')::BIGINT);

-- Use Case 2: Coordinated batch processing
CREATE TABLE test_batch_jobs (
    job_id SERIAL PRIMARY KEY,
    job_name VARCHAR(200),
    lock_key BIGINT UNIQUE
);

INSERT INTO test_batch_jobs (job_name, lock_key) VALUES
    ('Job 1', 1001),
    ('Job 2', 1002),
    ('Job 3', 1003);

-- Worker tries to acquire job
DO $$
DECLARE
    job_rec RECORD;
BEGIN
    FOR job_rec IN SELECT * FROM test_batch_jobs LOOP
        IF pg_try_advisory_lock(job_rec.lock_key) THEN
            RAISE NOTICE 'Worker acquired job: %', job_rec.job_name;
            -- Process job...
            PERFORM pg_advisory_unlock(job_rec.lock_key);
            EXIT;
        ELSE
            RAISE NOTICE 'Job % already locked', job_rec.job_name;
        END IF;
    END LOOP;
END $$;

-- ============================================================================
-- Section 19: Lock Contention Analysis
-- ============================================================================

CREATE TABLE test_contention (
    id SERIAL PRIMARY KEY,
    value INT,
    updated_count INT DEFAULT 0
);

INSERT INTO test_contention (value)
SELECT i FROM generate_series(1, 100) i;

-- Simulate high contention scenario
CREATE FUNCTION simulate_contention_update(row_id INT) RETURNS VOID AS $$
BEGIN
    UPDATE test_contention
    SET value = value + 1, updated_count = updated_count + 1
    WHERE id = row_id;
END;
$$ LANGUAGE plpgsql;

-- Single row updated multiple times (high contention)
SELECT simulate_contention_update(1) FROM generate_series(1, 10);

-- Check contention result
SELECT id, value, updated_count FROM test_contention WHERE id = 1;

-- Query lock wait statistics
SELECT
    relname,
    COUNT(*) AS lock_count,
    mode,
    granted
FROM pg_locks l
JOIN pg_class c ON l.relation = c.oid
WHERE relname LIKE 'test_%'
GROUP BY relname, mode, granted
ORDER BY relname, mode;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_locking_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_locking_best_practices VALUES
    (1, 'Use SELECT FOR UPDATE to lock rows for update'),
    (2, 'Use SELECT FOR SHARE for shared read locks'),
    (3, 'FOR UPDATE NOWAIT fails immediately if row is locked'),
    (4, 'FOR UPDATE SKIP LOCKED useful for work queue patterns'),
    (5, 'LOCK TABLE for bulk operations requiring table-level lock'),
    (6, 'Keep transactions short to minimize lock duration'),
    (7, 'Always lock in same order to prevent deadlocks'),
    (8, 'Use lock_timeout to prevent indefinite waiting'),
    (9, 'Advisory locks for application-level coordination'),
    (10, 'Session advisory locks must be explicitly released'),
    (11, 'Transaction advisory locks auto-release at commit/rollback'),
    (12, 'Monitor pg_locks view to diagnose lock contention'),
    (13, 'Deadlocks are automatically detected and resolved'),
    (14, 'Row-level locks are preferred over table-level locks'),
    (15, 'Use FOR UPDATE OF to lock specific tables in joins'),
    (16, 'Lock compatibility determines which operations can run concurrently'),
    (17, 'Advisory locks useful for singleton processes'),
    (18, 'SKIP LOCKED enables concurrent queue processing'),
    (19, 'Set appropriate lock_timeout and statement_timeout'),
    (20, 'Analyze lock contention to optimize transaction patterns');

SELECT id, guideline FROM test_locking_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_locking_best_practices;
DROP FUNCTION simulate_contention_update(INT);
DROP TABLE test_contention;
DROP TABLE test_batch_jobs;
DROP FUNCTION ensure_singleton_process(TEXT);
DROP TABLE test_row_conflicts;
DROP TABLE test_lock_compatibility;
DROP TABLE test_order_details;
DROP TABLE test_orders;
DROP TABLE test_lock_monitoring;
DROP TABLE test_timeout_locks;
DROP TABLE test_account_b;
DROP TABLE test_account_a;
DROP TABLE test_lock_modes;
DROP TABLE test_batch_processing;
DROP TABLE test_work_queue;
DROP TABLE test_reservations;
DROP TABLE test_products;
DROP TABLE test_inventory;

DROP DATABASE test_locking_mechanisms_db;

-- End of locking mechanisms tests
