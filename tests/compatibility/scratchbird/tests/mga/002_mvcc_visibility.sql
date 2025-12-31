-- ScratchBird Native Test: MVCC Visibility
-- Test ID: mga_002
-- Description: Multi-version concurrency control, tuple visibility, xmin/xmax

-- Create database
CREATE DATABASE test_mvcc;
\c test_mvcc;

-- Create test table
CREATE TABLE inventory (
    item_id INTEGER PRIMARY KEY,
    item_name VARCHAR(100),
    quantity INTEGER,
    last_update TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert initial data
INSERT INTO inventory VALUES
    (1, 'Widget A', 100, CURRENT_TIMESTAMP),
    (2, 'Widget B', 150, CURRENT_TIMESTAMP),
    (3, 'Widget C', 200, CURRENT_TIMESTAMP);

-- Test 1: Own Changes Visibility
-- Changes made in the current transaction should be visible
BEGIN TRANSACTION;
UPDATE inventory SET quantity = 90 WHERE item_id = 1;
SELECT item_id, quantity FROM inventory WHERE item_id = 1;
-- Should show 90
COMMIT;

-- Test 2: Committed Changes Visibility (READ COMMITTED)
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN TRANSACTION;
SELECT quantity FROM inventory WHERE item_id = 2;
-- In a concurrent session, item_id 2 would be updated and committed
-- READ COMMITTED would see the new value on next query
COMMIT;

-- Test 3: Snapshot Isolation (REPEATABLE READ)
-- Set up initial state
UPDATE inventory SET quantity = 150 WHERE item_id = 2;

SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
BEGIN TRANSACTION;
SELECT quantity FROM inventory WHERE item_id = 2;
-- Shows 150

-- Simulate concurrent update (in reality from another session)
-- UPDATE inventory SET quantity = 175 WHERE item_id = 2;
-- COMMIT;

-- Re-read in REPEATABLE READ - should still see 150
SELECT quantity FROM inventory WHERE item_id = 2;
COMMIT;

-- Verify the actual value is what was last committed
SELECT quantity FROM inventory WHERE item_id = 2;

-- Test 4: Phantom Reads Prevention (SERIALIZABLE)
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN TRANSACTION;
SELECT COUNT(*) AS count_before FROM inventory WHERE quantity > 100;

-- Concurrent session inserts new row: INSERT INTO inventory VALUES (4, 'Widget D', 250, CURRENT_TIMESTAMP);

-- Re-query - SERIALIZABLE should prevent phantom reads
SELECT COUNT(*) AS count_after FROM inventory WHERE quantity > 100;
COMMIT;

-- Test 5: UPDATE Visibility
BEGIN TRANSACTION;
UPDATE inventory SET quantity = quantity + 10;
SELECT SUM(quantity) AS total_before_commit FROM inventory;
ROLLBACK;

-- Verify rollback worked
SELECT SUM(quantity) AS total_after_rollback FROM inventory;

-- Test 6: DELETE and Visibility
BEGIN TRANSACTION;
DELETE FROM inventory WHERE item_id = 3;
SELECT COUNT(*) AS count_in_transaction FROM inventory;
-- Should show 2
ROLLBACK;

SELECT COUNT(*) AS count_after_rollback FROM inventory;
-- Should show 3

-- Test 7: Multiple Concurrent Updates (Demonstration)
-- Transaction T1 updates row 1
BEGIN TRANSACTION;
UPDATE inventory SET quantity = 95 WHERE item_id = 1;

-- T1 sees its own change
SELECT quantity FROM inventory WHERE item_id = 1;

COMMIT;

-- Test 8: Row Locking and Visibility
BEGIN TRANSACTION;
UPDATE inventory SET quantity = 105 WHERE item_id = 1;

-- Other transaction trying to update same row would block
-- until this transaction commits or rollbacks

SELECT quantity FROM inventory WHERE item_id = 1;
COMMIT;

-- Test 9: Multi-version Read
-- Create multiple versions of a row
BEGIN TRANSACTION;
UPDATE inventory SET quantity = 110 WHERE item_id = 1;
SAVEPOINT sp1;

UPDATE inventory SET quantity = 115 WHERE item_id = 1;
SAVEPOINT sp2;

UPDATE inventory SET quantity = 120 WHERE item_id = 1;

-- Check current value
SELECT quantity FROM inventory WHERE item_id = 1;

-- Rollback to sp2
ROLLBACK TO SAVEPOINT sp2;
SELECT quantity FROM inventory WHERE item_id = 1;

-- Rollback to sp1
ROLLBACK TO SAVEPOINT sp1;
SELECT quantity FROM inventory WHERE item_id = 1;

COMMIT;

-- Final state
SELECT * FROM inventory ORDER BY item_id;

-- Test 10: Vacuum/Garbage Collection Impact
-- After many updates, old tuple versions should be garbage collected
-- This is a demonstration; actual GC happens asynchronously

-- Generate multiple versions
BEGIN TRANSACTION;
UPDATE inventory SET quantity = quantity + 1 WHERE item_id = 1;
COMMIT;

BEGIN TRANSACTION;
UPDATE inventory SET quantity = quantity + 1 WHERE item_id = 1;
COMMIT;

BEGIN TRANSACTION;
UPDATE inventory SET quantity = quantity + 1 WHERE item_id = 1;
COMMIT;

BEGIN TRANSACTION;
UPDATE inventory SET quantity = quantity + 1 WHERE item_id = 1;
COMMIT;

BEGIN TRANSACTION;
UPDATE inventory SET quantity = quantity + 1 WHERE item_id = 1;
COMMIT;

-- Trigger manual vacuum (if supported)
-- VACUUM inventory;

SELECT * FROM inventory WHERE item_id = 1;

-- Cleanup
DROP TABLE inventory;
DROP DATABASE test_mvcc;
