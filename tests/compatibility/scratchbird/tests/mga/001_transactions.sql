-- ScratchBird Native Test: MGA Transactions
-- Test ID: mga_001
-- Description: Transaction isolation, COMMIT, ROLLBACK, SAVEPOINT

-- Create database
CREATE DATABASE test_transactions;
\c test_transactions;

-- Create test table
CREATE TABLE accounts (
    account_id INTEGER PRIMARY KEY,
    account_name VARCHAR(100),
    balance DECIMAL(10, 2)
);

-- Test Basic Transaction - COMMIT
BEGIN TRANSACTION;
INSERT INTO accounts VALUES (1, 'Alice', 1000.00);
INSERT INTO accounts VALUES (2, 'Bob', 1500.00);
COMMIT;

SELECT * FROM accounts ORDER BY account_id;

-- Test Transaction - ROLLBACK
BEGIN TRANSACTION;
UPDATE accounts SET balance = balance - 100.00 WHERE account_id = 1;
UPDATE accounts SET balance = balance + 100.00 WHERE account_id = 2;
-- Check changes before rollback
SELECT * FROM accounts ORDER BY account_id;
ROLLBACK;

-- Verify rollback
SELECT * FROM accounts ORDER BY account_id;

-- Test SAVEPOINT
BEGIN TRANSACTION;
INSERT INTO accounts VALUES (3, 'Charlie', 2000.00);
SAVEPOINT sp1;

UPDATE accounts SET balance = balance + 50.00 WHERE account_id = 1;
SAVEPOINT sp2;

UPDATE accounts SET balance = balance - 100.00 WHERE account_id = 3;
SELECT * FROM accounts ORDER BY account_id;

-- Rollback to sp2
ROLLBACK TO SAVEPOINT sp2;
SELECT * FROM accounts ORDER BY account_id;

-- Rollback to sp1
ROLLBACK TO SAVEPOINT sp1;
SELECT * FROM accounts ORDER BY account_id;

COMMIT;
SELECT * FROM accounts ORDER BY account_id;

-- Test Nested Transactions (SAVEPOINT)
BEGIN TRANSACTION;
UPDATE accounts SET balance = 500.00 WHERE account_id = 1;
SAVEPOINT level1;

UPDATE accounts SET balance = 750.00 WHERE account_id = 2;
SAVEPOINT level2;

UPDATE accounts SET balance = 1000.00 WHERE account_id = 3;
SELECT * FROM accounts ORDER BY account_id;

-- Rollback inner savepoint
ROLLBACK TO SAVEPOINT level2;
SELECT * FROM accounts ORDER BY account_id;

COMMIT;

-- Test COMMIT RETAIN (Firebird-style)
-- Start transaction, commit but retain transaction context
BEGIN TRANSACTION;
INSERT INTO accounts VALUES (4, 'David', 3000.00);
COMMIT RETAIN;

-- Transaction is still active, can continue
INSERT INTO accounts VALUES (5, 'Eve', 2500.00);
COMMIT;

SELECT * FROM accounts ORDER BY account_id;

-- Test ROLLBACK RETAIN (Firebird-style)
BEGIN TRANSACTION;
UPDATE accounts SET balance = 0.00 WHERE account_id = 1;
SELECT balance FROM accounts WHERE account_id = 1;
ROLLBACK RETAIN;

-- Transaction is still active, previous changes rolled back
SELECT balance FROM accounts WHERE account_id = 1;
UPDATE accounts SET balance = balance + 100.00 WHERE account_id = 1;
COMMIT;

SELECT balance FROM accounts WHERE account_id = 1;

-- Test READ COMMITTED isolation
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN TRANSACTION;
SELECT * FROM accounts WHERE account_id = 1;
-- In another session, account 1 would be updated
-- This session would see the committed changes
COMMIT;

-- Test REPEATABLE READ isolation
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
BEGIN TRANSACTION;
SELECT COUNT(*) AS initial_count FROM accounts;
-- In another session, new accounts would be inserted
-- This session would NOT see the new rows (repeatable read)
SELECT COUNT(*) AS final_count FROM accounts;
COMMIT;

-- Test SERIALIZABLE isolation
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN TRANSACTION;
SELECT SUM(balance) AS total_balance FROM accounts;
-- Any conflicting concurrent transaction would cause serialization failure
COMMIT;

-- Test Transaction with Constraint Violation
BEGIN TRANSACTION;
INSERT INTO accounts VALUES (10, 'Frank', 5000.00);
-- This should violate primary key constraint and rollback
INSERT INTO accounts VALUES (10, 'George', 4000.00);
-- Transaction should be aborted
COMMIT;

-- Verify Frank was not inserted due to transaction abort
SELECT * FROM accounts WHERE account_id = 10;

-- Test Explicit ROLLBACK after error
BEGIN TRANSACTION;
INSERT INTO accounts VALUES (11, 'Hannah', 6000.00);
-- Intentional error
INSERT INTO accounts VALUES (11, 'Ian', 5500.00);
-- Explicitly rollback
ROLLBACK;

SELECT * FROM accounts WHERE account_id = 11;

-- Cleanup
DROP TABLE accounts;
DROP DATABASE test_transactions;
