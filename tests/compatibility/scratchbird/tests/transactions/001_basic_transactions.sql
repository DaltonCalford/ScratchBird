-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - Basic Transaction Control
-- Description: Comprehensive basic transaction testing
-- ============================================================================

-- Transactions: ACID properties (Atomicity, Consistency, Isolation, Durability)
-- BEGIN/START TRANSACTION: Start transaction
-- COMMIT: Save all changes
-- ROLLBACK: Discard all changes
-- Transactions ensure data integrity

-- Create test database
CREATE DATABASE test_basic_transactions_db;
USE test_basic_transactions_db;

-- ============================================================================
-- Section 1: Basic COMMIT
-- ============================================================================

CREATE TABLE test_accounts (
    account_id SERIAL PRIMARY KEY,
    account_name VARCHAR(200),
    balance NUMERIC(12,2)
);

-- Transaction with COMMIT
BEGIN;
INSERT INTO test_accounts (account_name, balance) VALUES ('Alice', 1000.00);
INSERT INTO test_accounts (account_name, balance) VALUES ('Bob', 500.00);
COMMIT;

-- Changes are saved
SELECT account_id, account_name, balance FROM test_accounts ORDER BY account_id;

-- ============================================================================
-- Section 2: Basic ROLLBACK
-- ============================================================================

-- Transaction with ROLLBACK
BEGIN;
INSERT INTO test_accounts (account_name, balance) VALUES ('Charlie', 750.00);
INSERT INTO test_accounts (account_name, balance) VALUES ('Diana', 1200.00);
ROLLBACK;

-- Changes are discarded
SELECT account_id, account_name, balance FROM test_accounts ORDER BY account_id;

-- ============================================================================
-- Section 3: BEGIN vs START TRANSACTION
-- ============================================================================

-- BEGIN syntax
BEGIN;
INSERT INTO test_accounts (account_name, balance) VALUES ('Eve', 300.00);
COMMIT;

-- START TRANSACTION syntax (equivalent)
START TRANSACTION;
INSERT INTO test_accounts (account_name, balance) VALUES ('Frank', 800.00);
COMMIT;

SELECT account_id, account_name, balance FROM test_accounts ORDER BY account_id;

-- ============================================================================
-- Section 4: Implicit vs Explicit Transactions
-- ============================================================================

-- Implicit transaction (auto-commit)
INSERT INTO test_accounts (account_name, balance) VALUES ('Grace', 600.00);
-- Automatically committed

-- Explicit transaction
BEGIN;
INSERT INTO test_accounts (account_name, balance) VALUES ('Henry', 900.00);
-- Not committed yet
COMMIT;

SELECT COUNT(*) FROM test_accounts;

-- ============================================================================
-- Section 5: Transaction with Multiple Operations
-- ============================================================================

CREATE TABLE test_transfers (
    transfer_id SERIAL PRIMARY KEY,
    from_account INT,
    to_account INT,
    amount NUMERIC(12,2),
    transfer_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Multi-operation transaction
BEGIN;

-- Debit from Alice
UPDATE test_accounts SET balance = balance - 100.00 WHERE account_name = 'Alice';

-- Credit to Bob
UPDATE test_accounts SET balance = balance + 100.00 WHERE account_name = 'Bob';

-- Log transfer
INSERT INTO test_transfers (from_account, to_account, amount)
VALUES (1, 2, 100.00);

COMMIT;

-- Verify all operations succeeded
SELECT account_name, balance FROM test_accounts WHERE account_name IN ('Alice', 'Bob') ORDER BY account_name;
SELECT transfer_id, from_account, to_account, amount FROM test_transfers;

-- ============================================================================
-- Section 6: Transaction Atomicity
-- ============================================================================

-- All or nothing: If any operation fails, all are rolled back
BEGIN;

UPDATE test_accounts SET balance = balance - 50.00 WHERE account_name = 'Alice';

-- This will fail (insufficient funds simulation)
-- Simulate error
DO $$
BEGIN
    IF (SELECT balance FROM test_accounts WHERE account_name = 'Bob') < 10000 THEN
        RAISE EXCEPTION 'Simulated error';
    END IF;
END $$;

-- Transaction fails, so ROLLBACK
ROLLBACK;

-- Alice's balance unchanged (atomicity)
SELECT account_name, balance FROM test_accounts WHERE account_name = 'Alice';

-- ============================================================================
-- Section 7: Transaction Read Committed
-- ============================================================================

-- Default isolation level: READ COMMITTED
BEGIN;
SELECT account_name, balance FROM test_accounts WHERE account_name = 'Alice';
-- Sees committed data from other transactions
COMMIT;

-- ============================================================================
-- Section 8: Nested BEGIN (Not Supported)
-- ============================================================================

-- PostgreSQL doesn't support nested transactions (use SAVEPOINT instead)
BEGIN;
INSERT INTO test_accounts (account_name, balance) VALUES ('Test1', 100.00);

-- BEGIN;  -- Would give warning: already in transaction
-- Use SAVEPOINT for nested behavior (covered in savepoint tests)

COMMIT;

-- ============================================================================
-- Section 9: Transaction State Queries
-- ============================================================================

-- Check transaction state
SELECT txid_current() AS current_txid;

BEGIN;
SELECT txid_current() AS txid_in_transaction;
SELECT pg_current_xact_id() AS current_xact_id;
COMMIT;

SELECT txid_current() AS txid_after_commit;

-- ============================================================================
-- Section 10: Transaction with DDL
-- ============================================================================

-- DDL operations are transactional in PostgreSQL
BEGIN;

CREATE TABLE test_temp_ddl (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_temp_ddl (data) VALUES ('Data 1'), ('Data 2');

-- Can rollback DDL
ROLLBACK;

-- Table doesn't exist
SELECT COUNT(*) FROM pg_tables WHERE tablename = 'test_temp_ddl';

-- ============================================================================
-- Section 11: COMMIT AND CHAIN
-- ============================================================================

-- COMMIT AND CHAIN starts a new transaction immediately
BEGIN;
INSERT INTO test_accounts (account_name, balance) VALUES ('User1', 100.00);
COMMIT AND CHAIN;
-- Still in transaction
INSERT INTO test_accounts (account_name, balance) VALUES ('User2', 200.00);
COMMIT;

SELECT account_name, balance FROM test_accounts WHERE account_name LIKE 'User%' ORDER BY account_name;

-- ============================================================================
-- Section 12: ROLLBACK AND CHAIN
-- ============================================================================

BEGIN;
INSERT INTO test_accounts (account_name, balance) VALUES ('User3', 300.00);
ROLLBACK AND CHAIN;
-- Still in transaction, previous insert rolled back
INSERT INTO test_accounts (account_name, balance) VALUES ('User4', 400.00);
COMMIT;

-- Only User4 exists
SELECT account_name FROM test_accounts WHERE account_name IN ('User3', 'User4') ORDER BY account_name;

-- ============================================================================
-- Section 13: Transaction Timeout
-- ============================================================================

-- Set statement timeout
BEGIN;
SET LOCAL statement_timeout = '5s';

-- Quick operation
SELECT COUNT(*) FROM test_accounts;

-- Long operation would timeout
-- SELECT pg_sleep(10);  -- Would fail after 5 seconds

COMMIT;

-- ============================================================================
-- Section 14: Read-Only Transactions
-- ============================================================================

BEGIN READ ONLY;

-- Can read
SELECT COUNT(*) FROM test_accounts;

-- Cannot write (would fail)
-- INSERT INTO test_accounts (account_name, balance) VALUES ('ReadOnly', 100.00);

COMMIT;

-- ============================================================================
-- Section 15: Read-Write Transactions
-- ============================================================================

BEGIN READ WRITE;

-- Can read
SELECT COUNT(*) FROM test_accounts;

-- Can write
INSERT INTO test_accounts (account_name, balance) VALUES ('ReadWrite', 150.00);

COMMIT;

SELECT account_name FROM test_accounts WHERE account_name = 'ReadWrite';

-- ============================================================================
-- Section 16: Transaction with Constraint Violations
-- ============================================================================

-- Constraint violation rolls back entire transaction
BEGIN;

INSERT INTO test_accounts (account_name, balance) VALUES ('Valid1', 100.00);

-- Would fail (duplicate account_id if we try to insert with explicit ID)
-- INSERT INTO test_accounts (account_id, account_name, balance) VALUES (1, 'Duplicate', 100.00);

-- In case of error, must rollback
ROLLBACK;

-- Valid1 not inserted (atomicity)
SELECT account_name FROM test_accounts WHERE account_name = 'Valid1';

-- ============================================================================
-- Section 17: Checking Transaction Status
-- ============================================================================

-- Check if in transaction
SELECT CASE
    WHEN pg_current_xact_id_if_assigned() IS NULL THEN 'Not in transaction'
    ELSE 'In transaction'
END AS transaction_status;

BEGIN;
SELECT CASE
    WHEN pg_current_xact_id_if_assigned() IS NOT NULL THEN 'In transaction'
    ELSE 'Not in transaction'
END AS transaction_status;
COMMIT;

-- ============================================================================
-- Section 18: Transaction with Triggers
-- ============================================================================

CREATE TABLE test_audit_log (
    log_id SERIAL PRIMARY KEY,
    operation TEXT,
    account_name VARCHAR(200),
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION log_account_change() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_audit_log (operation, account_name)
    VALUES (TG_OP, NEW.account_name);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_account
AFTER INSERT ON test_accounts
FOR EACH ROW
EXECUTE FUNCTION log_account_change();

-- Transaction includes trigger actions
BEGIN;
INSERT INTO test_accounts (account_name, balance) VALUES ('Triggered', 250.00);
COMMIT;

-- Both main table and audit log updated
SELECT account_name FROM test_accounts WHERE account_name = 'Triggered';
SELECT operation, account_name FROM test_audit_log WHERE account_name = 'Triggered';

-- Rollback affects trigger actions too
BEGIN;
INSERT INTO test_accounts (account_name, balance) VALUES ('RolledBack', 350.00);
ROLLBACK;

-- Neither main table nor audit log updated
SELECT account_name FROM test_accounts WHERE account_name = 'RolledBack';
SELECT operation, account_name FROM test_audit_log WHERE account_name = 'RolledBack';

-- ============================================================================
-- Section 19: Large Transaction
-- ============================================================================

BEGIN;

-- Insert many rows in single transaction
INSERT INTO test_accounts (account_name, balance)
SELECT 'Bulk_' || i, (RANDOM() * 1000)::NUMERIC(12,2)
FROM generate_series(1, 1000) i;

COMMIT;

SELECT COUNT(*) FROM test_accounts WHERE account_name LIKE 'Bulk_%';

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_transaction_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_transaction_best_practices VALUES
    (1, 'Always use explicit transactions for multi-operation changes'),
    (2, 'Keep transactions as short as possible'),
    (3, 'COMMIT saves changes permanently'),
    (4, 'ROLLBACK discards all changes in transaction'),
    (5, 'Transactions are atomic: all or nothing'),
    (6, 'Use BEGIN or START TRANSACTION to start'),
    (7, 'Implicit transactions auto-commit each statement'),
    (8, 'DDL operations are transactional in PostgreSQL'),
    (9, 'Use READ ONLY for transactions that only query'),
    (10, 'Set statement_timeout to prevent long-running transactions'),
    (11, 'Constraint violations cause transaction rollback'),
    (12, 'Triggers execute within the transaction'),
    (13, 'Use COMMIT AND CHAIN to continue in new transaction'),
    (14, 'Monitor transaction IDs with txid_current()'),
    (15, 'Handle errors properly to avoid leaving transactions open');

SELECT id, guideline FROM test_transaction_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_transaction_best_practices;
DROP TRIGGER trg_log_account ON test_accounts;
DROP FUNCTION log_account_change();
DROP TABLE test_audit_log;
DROP TABLE test_transfers;
DROP TABLE test_accounts;

DROP DATABASE test_basic_transactions_db;

-- End of basic transaction tests
