-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - Error Handling and Edge Cases
-- Description: Comprehensive transaction error handling testing
-- ============================================================================

-- Transaction Error Handling: Exception handling, constraint violations,
-- deadlock recovery, statement failures, transaction aborts,
-- error recovery patterns, edge cases

-- Create test database
CREATE DATABASE test_transaction_errors_db;
USE test_transaction_errors_db;

-- ============================================================================
-- Section 1: Basic Error Handling in Transactions
-- ============================================================================

CREATE TABLE test_basic_errors (
    id SERIAL PRIMARY KEY,
    value INT CHECK (value > 0)
);

-- Error causes transaction abort
DO $$
BEGIN
    BEGIN;
    INSERT INTO test_basic_errors (value) VALUES (10);
    INSERT INTO test_basic_errors (value) VALUES (-5);  -- Violates CHECK
    COMMIT;
EXCEPTION
    WHEN check_violation THEN
        ROLLBACK;
        RAISE NOTICE 'Transaction rolled back due to constraint violation';
END $$;

-- No data inserted (transaction aborted)
SELECT COUNT(*) FROM test_basic_errors;

-- ============================================================================
-- Section 2: Exception Handling with SAVEPOINT
-- ============================================================================

-- SAVEPOINT allows partial rollback
BEGIN;

INSERT INTO test_basic_errors (value) VALUES (100);
SAVEPOINT sp1;

-- Try risky operation
DO $$
BEGIN
    INSERT INTO test_basic_errors (value) VALUES (-10);
EXCEPTION
    WHEN check_violation THEN
        ROLLBACK TO SAVEPOINT sp1;
        RAISE NOTICE 'Rolled back to savepoint, continuing transaction';
END $$;

-- Continue transaction
INSERT INTO test_basic_errors (value) VALUES (200);

COMMIT;

-- First and third inserts committed
SELECT id, value FROM test_basic_errors ORDER BY id;

-- ============================================================================
-- Section 3: Constraint Violation Handling
-- ============================================================================

CREATE TABLE test_constraint_errors (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50) UNIQUE,
    email VARCHAR(200) UNIQUE,
    age INT CHECK (age >= 18),
    balance NUMERIC(10,2) CHECK (balance >= 0)
);

INSERT INTO test_constraint_errors (code, email, age, balance)
VALUES ('CODE1', 'user1@example.com', 25, 100.00);

-- Handle UNIQUE violation
DO $$
BEGIN
    INSERT INTO test_constraint_errors (code, email, age, balance)
    VALUES ('CODE1', 'user2@example.com', 30, 200.00);
EXCEPTION
    WHEN unique_violation THEN
        RAISE NOTICE 'Code already exists: %', SQLERRM;
END $$;

-- Handle CHECK violation
DO $$
BEGIN
    INSERT INTO test_constraint_errors (code, email, age, balance)
    VALUES ('CODE2', 'user3@example.com', 15, 150.00);
EXCEPTION
    WHEN check_violation THEN
        RAISE NOTICE 'Age constraint violation: %', SQLERRM;
END $$;

-- Handle NOT NULL violation
DO $$
BEGIN
    INSERT INTO test_constraint_errors (code, email, age, balance)
    VALUES (NULL, 'user4@example.com', 25, 100.00);
EXCEPTION
    WHEN not_null_violation THEN
        RAISE NOTICE 'Required field missing: %', SQLERRM;
END $$;

SELECT COUNT(*) FROM test_constraint_errors;

-- ============================================================================
-- Section 4: Foreign Key Violation Handling
-- ============================================================================

CREATE TABLE test_fk_parent_errors (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(200)
);

CREATE TABLE test_fk_child_errors (
    child_id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_fk_parent_errors(parent_id),
    child_name VARCHAR(200)
);

INSERT INTO test_fk_parent_errors (parent_name) VALUES ('Parent 1');

-- Handle FK violation on INSERT
DO $$
BEGIN
    INSERT INTO test_fk_child_errors (parent_id, child_name)
    VALUES (999, 'Orphan child');
EXCEPTION
    WHEN foreign_key_violation THEN
        RAISE NOTICE 'Parent does not exist: %', SQLERRM;
END $$;

-- Handle FK violation on DELETE
INSERT INTO test_fk_child_errors (parent_id, child_name)
VALUES (1, 'Child 1');

DO $$
BEGIN
    DELETE FROM test_fk_parent_errors WHERE parent_id = 1;
EXCEPTION
    WHEN foreign_key_violation THEN
        RAISE NOTICE 'Cannot delete parent with children: %', SQLERRM;
END $$;

SELECT p.parent_id, p.parent_name, COUNT(c.child_id) AS children
FROM test_fk_parent_errors p
LEFT JOIN test_fk_child_errors c ON p.parent_id = c.parent_id
GROUP BY p.parent_id, p.parent_name;

-- ============================================================================
-- Section 5: Deadlock Detection and Recovery
-- ============================================================================

CREATE TABLE test_deadlock_a (
    id INT PRIMARY KEY,
    value INT
);

CREATE TABLE test_deadlock_b (
    id INT PRIMARY KEY,
    value INT
);

INSERT INTO test_deadlock_a VALUES (1, 100);
INSERT INTO test_deadlock_b VALUES (1, 200);

-- Deadlock scenario (simulated)
-- Transaction 1: Lock A then B
-- Transaction 2: Lock B then A
-- PostgreSQL detects deadlock, aborts one transaction

DO $$
DECLARE
    attempts INT := 0;
    max_attempts INT := 5;
    success BOOLEAN := FALSE;
BEGIN
    WHILE NOT success AND attempts < max_attempts LOOP
        attempts := attempts + 1;

        BEGIN
            -- Simulate transaction that might deadlock
            UPDATE test_deadlock_a SET value = value + 1 WHERE id = 1;
            PERFORM pg_sleep(0.1);
            UPDATE test_deadlock_b SET value = value + 1 WHERE id = 1;

            success := TRUE;
            RAISE NOTICE 'Transaction succeeded on attempt %', attempts;

        EXCEPTION
            WHEN deadlock_detected THEN
                RAISE NOTICE 'Deadlock detected on attempt %, retrying...', attempts;
                PERFORM pg_sleep(RANDOM() * 0.1);  -- Random backoff
        END;
    END LOOP;

    IF NOT success THEN
        RAISE EXCEPTION 'Failed after % attempts', max_attempts;
    END IF;
END $$;

SELECT * FROM test_deadlock_a;
SELECT * FROM test_deadlock_b;

-- ============================================================================
-- Section 6: Serialization Failure Handling
-- ============================================================================

CREATE TABLE test_serialization_errors (
    id SERIAL PRIMARY KEY,
    counter INT
);

INSERT INTO test_serialization_errors (counter) VALUES (0);

-- SERIALIZABLE transactions may fail with serialization error
DO $$
DECLARE
    attempts INT := 0;
    max_attempts INT := 5;
    success BOOLEAN := FALSE;
BEGIN
    WHILE NOT success AND attempts < max_attempts LOOP
        attempts := attempts + 1;

        BEGIN
            BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;

            -- Simulate operation that might conflict
            UPDATE test_serialization_errors
            SET counter = counter + 1
            WHERE id = 1;

            COMMIT;
            success := TRUE;
            RAISE NOTICE 'Serializable transaction succeeded on attempt %', attempts;

        EXCEPTION
            WHEN serialization_failure THEN
                ROLLBACK;
                RAISE NOTICE 'Serialization failure on attempt %, retrying...', attempts;
                PERFORM pg_sleep(RANDOM() * 0.1);
        END;
    END LOOP;
END $$;

SELECT id, counter FROM test_serialization_errors;

-- ============================================================================
-- Section 7: Transaction Abort on Statement Error
-- ============================================================================

CREATE TABLE test_statement_errors (
    id SERIAL PRIMARY KEY,
    value TEXT
);

-- One statement fails, whole transaction aborted
DO $$
BEGIN
    BEGIN;

    INSERT INTO test_statement_errors (value) VALUES ('Value 1');

    -- This fails (syntax error)
    EXECUTE 'INVALID SQL STATEMENT';

    INSERT INTO test_statement_errors (value) VALUES ('Value 2');

    COMMIT;
EXCEPTION
    WHEN OTHERS THEN
        ROLLBACK;
        RAISE NOTICE 'Transaction aborted due to statement error: %', SQLERRM;
END $$;

-- No data inserted
SELECT COUNT(*) FROM test_statement_errors;

-- ============================================================================
-- Section 8: Handling Division by Zero
-- ============================================================================

CREATE TABLE test_division_errors (
    id SERIAL PRIMARY KEY,
    numerator INT,
    denominator INT,
    result NUMERIC(10,2)
);

INSERT INTO test_division_errors (numerator, denominator)
VALUES (10, 2), (20, 0), (30, 3);

-- Handle division by zero
UPDATE test_division_errors
SET result = CASE
    WHEN denominator = 0 THEN NULL
    ELSE numerator::NUMERIC / denominator
END;

SELECT id, numerator, denominator, result FROM test_division_errors ORDER BY id;

-- Exception handling approach
DO $$
DECLARE
    rec RECORD;
BEGIN
    FOR rec IN SELECT * FROM test_division_errors LOOP
        BEGIN
            UPDATE test_division_errors
            SET result = rec.numerator::NUMERIC / rec.denominator
            WHERE id = rec.id;
        EXCEPTION
            WHEN division_by_zero THEN
                RAISE NOTICE 'Division by zero for id %', rec.id;
        END;
    END LOOP;
END $$;

-- ============================================================================
-- Section 9: Out of Range Errors
-- ============================================================================

CREATE TABLE test_range_errors (
    id SERIAL PRIMARY KEY,
    small_int SMALLINT,
    regular_int INT,
    big_int BIGINT
);

-- Handle numeric overflow
DO $$
BEGIN
    INSERT INTO test_range_errors (small_int) VALUES (50000);
EXCEPTION
    WHEN numeric_value_out_of_range THEN
        RAISE NOTICE 'Value exceeds SMALLINT range: %', SQLERRM;
        INSERT INTO test_range_errors (regular_int) VALUES (50000);
END $$;

SELECT id, small_int, regular_int FROM test_range_errors;

-- ============================================================================
-- Section 10: Transaction Timeout Errors
-- ============================================================================

CREATE TABLE test_timeout_errors (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Statement timeout
DO $$
BEGIN
    BEGIN;
    SET LOCAL statement_timeout = '100ms';

    INSERT INTO test_timeout_errors (data) VALUES ('Quick operation');

    -- This would timeout if it took > 100ms
    -- PERFORM pg_sleep(1);

    COMMIT;
EXCEPTION
    WHEN query_canceled THEN
        ROLLBACK;
        RAISE NOTICE 'Statement timeout: %', SQLERRM;
END $$;

SELECT COUNT(*) FROM test_timeout_errors;

-- ============================================================================
-- Section 11: Lock Timeout Errors
-- ============================================================================

CREATE TABLE test_lock_timeout_errors (
    id SERIAL PRIMARY KEY,
    value INT
);

INSERT INTO test_lock_timeout_errors (value) VALUES (100);

-- Handle lock timeout
DO $$
BEGIN
    BEGIN;
    SET LOCAL lock_timeout = '100ms';

    -- Try to acquire lock (would timeout if locked by another transaction)
    SELECT * FROM test_lock_timeout_errors WHERE id = 1 FOR UPDATE;

    COMMIT;
EXCEPTION
    WHEN lock_not_available THEN
        ROLLBACK;
        RAISE NOTICE 'Lock timeout: %', SQLERRM;
END $$;

-- ============================================================================
-- Section 12: Disk Full Errors
-- ============================================================================

-- Handle disk full scenario
CREATE TABLE test_disk_errors (
    id SERIAL PRIMARY KEY,
    large_data TEXT
);

DO $$
BEGIN
    -- Try to insert large amount of data
    INSERT INTO test_disk_errors (large_data)
    SELECT repeat('X', 1000) FROM generate_series(1, 1000);

    RAISE NOTICE 'Insert succeeded';
EXCEPTION
    WHEN disk_full THEN
        RAISE NOTICE 'Disk full error: %', SQLERRM;
    WHEN insufficient_resources THEN
        RAISE NOTICE 'Insufficient resources: %', SQLERRM;
END $$;

-- ============================================================================
-- Section 13: Connection Errors During Transaction
-- ============================================================================

-- Connection lost during transaction causes automatic rollback
-- Application must retry transaction

CREATE TABLE test_connection_errors (
    id SERIAL PRIMARY KEY,
    attempt INT,
    data TEXT
);

-- Retry pattern for connection errors
DO $$
DECLARE
    max_retries INT := 3;
    attempt INT := 0;
    success BOOLEAN := FALSE;
BEGIN
    WHILE NOT success AND attempt < max_retries LOOP
        attempt := attempt + 1;

        BEGIN
            -- Simulate transaction
            INSERT INTO test_connection_errors (attempt, data)
            VALUES (attempt, 'Attempt ' || attempt);

            success := TRUE;
            RAISE NOTICE 'Transaction succeeded on attempt %', attempt;

        EXCEPTION
            WHEN OTHERS THEN
                RAISE NOTICE 'Error on attempt %: %', attempt, SQLERRM;
                PERFORM pg_sleep(1);  -- Backoff
        END;
    END LOOP;
END $$;

SELECT attempt, data FROM test_connection_errors ORDER BY id;

-- ============================================================================
-- Section 14: Nested Exception Handling
-- ============================================================================

CREATE TABLE test_nested_exceptions (
    id SERIAL PRIMARY KEY,
    value INT CHECK (value > 0)
);

-- Nested exception blocks
DO $$
BEGIN
    -- Outer block
    BEGIN
        INSERT INTO test_nested_exceptions (value) VALUES (100);

        -- Inner block
        BEGIN
            INSERT INTO test_nested_exceptions (value) VALUES (-50);
        EXCEPTION
            WHEN check_violation THEN
                RAISE NOTICE 'Inner exception: Invalid value';
                -- Inner exception handled, outer block continues
        END;

        INSERT INTO test_nested_exceptions (value) VALUES (200);
    END;
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'Outer exception: %', SQLERRM;
END $$;

-- Both valid inserts succeeded
SELECT id, value FROM test_nested_exceptions ORDER BY id;

-- ============================================================================
-- Section 15: RAISE EXCEPTION Custom Errors
-- ============================================================================

CREATE TABLE test_custom_errors (
    id SERIAL PRIMARY KEY,
    balance NUMERIC(10,2)
);

INSERT INTO test_custom_errors (balance) VALUES (100.00);

-- Custom business logic validation
DO $$
DECLARE
    current_balance NUMERIC;
    withdrawal NUMERIC := 150.00;
BEGIN
    BEGIN;

    SELECT balance INTO current_balance FROM test_custom_errors WHERE id = 1;

    IF withdrawal > current_balance THEN
        RAISE EXCEPTION 'Insufficient funds: balance=%, withdrawal=%', current_balance, withdrawal
            USING ERRCODE = 'P0001';  -- Custom error code
    END IF;

    UPDATE test_custom_errors SET balance = balance - withdrawal WHERE id = 1;

    COMMIT;
EXCEPTION
    WHEN OTHERS THEN
        ROLLBACK;
        RAISE NOTICE 'Transaction failed: %', SQLERRM;
END $$;

SELECT id, balance FROM test_custom_errors;

-- ============================================================================
-- Section 16: Handling Aborted Transactions
-- ============================================================================

CREATE TABLE test_aborted_txn (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50)
);

-- Transaction aborted by error
DO $$
BEGIN
    BEGIN;

    INSERT INTO test_aborted_txn (status) VALUES ('processing');

    -- Error aborts transaction
    EXECUTE 'SELECT 1/0';

    -- This won't execute (transaction aborted)
    UPDATE test_aborted_txn SET status = 'completed';

    COMMIT;
EXCEPTION
    WHEN division_by_zero THEN
        ROLLBACK;
        RAISE NOTICE 'Transaction aborted and rolled back';
END $$;

-- No data inserted
SELECT COUNT(*) FROM test_aborted_txn;

-- ============================================================================
-- Section 17: Handling Invalid Data Type Conversion
-- ============================================================================

CREATE TABLE test_conversion_errors (
    id SERIAL PRIMARY KEY,
    numeric_value INT,
    text_value TEXT
);

-- Handle conversion errors
DO $$
BEGIN
    -- Try to convert invalid text to integer
    INSERT INTO test_conversion_errors (numeric_value, text_value)
    VALUES ('not_a_number'::INT, 'text');
EXCEPTION
    WHEN invalid_text_representation THEN
        RAISE NOTICE 'Invalid data type conversion: %', SQLERRM;
        INSERT INTO test_conversion_errors (text_value) VALUES ('text_only');
END $$;

SELECT id, numeric_value, text_value FROM test_conversion_errors;

-- ============================================================================
-- Section 18: Handling Dropped Objects
-- ============================================================================

-- Attempting to use dropped object causes error
CREATE TABLE test_dropped_objects (
    id SERIAL PRIMARY KEY
);

-- Drop table
DROP TABLE test_dropped_objects;

-- Handle undefined table error
DO $$
BEGIN
    SELECT * FROM test_dropped_objects;
EXCEPTION
    WHEN undefined_table THEN
        RAISE NOTICE 'Table does not exist: %', SQLERRM;
END $$;

-- ============================================================================
-- Section 19: Error Information Functions
-- ============================================================================

-- Get detailed error information
DO $$
DECLARE
    error_code TEXT;
    error_message TEXT;
    error_detail TEXT;
    error_hint TEXT;
    error_context TEXT;
BEGIN
    -- Cause an error
    EXECUTE 'SELECT 1/0';
EXCEPTION
    WHEN OTHERS THEN
        GET STACKED DIAGNOSTICS
            error_code = RETURNED_SQLSTATE,
            error_message = MESSAGE_TEXT,
            error_detail = PG_EXCEPTION_DETAIL,
            error_hint = PG_EXCEPTION_HINT,
            error_context = PG_EXCEPTION_CONTEXT;

        RAISE NOTICE 'Error Code: %', error_code;
        RAISE NOTICE 'Message: %', error_message;
        RAISE NOTICE 'Detail: %', error_detail;
        RAISE NOTICE 'Hint: %', error_hint;
        RAISE NOTICE 'Context: %', error_context;
END $$;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_error_handling_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_error_handling_best_practices VALUES
    (1, 'Always wrap risky operations in BEGIN/EXCEPTION blocks'),
    (2, 'Use SAVEPOINT for partial rollback within transaction'),
    (3, 'Handle specific exceptions (unique_violation, check_violation, etc.)'),
    (4, 'Implement retry logic for deadlock and serialization failures'),
    (5, 'Log errors with sufficient context for debugging'),
    (6, 'Use RAISE EXCEPTION for business logic validation'),
    (7, 'Set appropriate timeout values (statement_timeout, lock_timeout)'),
    (8, 'Clean up resources in EXCEPTION block (cursors, locks)'),
    (9, 'Validate input data before starting transaction'),
    (10, 'Handle constraint violations gracefully'),
    (11, 'Use GET STACKED DIAGNOSTICS for detailed error info'),
    (12, 'Implement exponential backoff for retries'),
    (13, 'Distinguish between retriable and non-retriable errors'),
    (14, 'Monitor error logs for recurring issues'),
    (15, 'Use custom error codes (ERRCODE) for application errors'),
    (16, 'Test error paths as thoroughly as success paths'),
    (17, 'Document expected error conditions'),
    (18, 'Avoid catching OTHERS unless necessary (hides bugs)'),
    (19, 'Ensure transaction is rolled back on error'),
    (20, 'Use nested exception blocks for granular error handling');

SELECT id, guideline FROM test_error_handling_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_error_handling_best_practices;
DROP TABLE test_conversion_errors;
DROP TABLE test_aborted_txn;
DROP TABLE test_custom_errors;
DROP TABLE test_nested_exceptions;
DROP TABLE test_connection_errors;
DROP TABLE test_disk_errors;
DROP TABLE test_lock_timeout_errors;
DROP TABLE test_timeout_errors;
DROP TABLE test_range_errors;
DROP TABLE test_division_errors;
DROP TABLE test_statement_errors;
DROP TABLE test_serialization_errors;
DROP TABLE test_deadlock_b;
DROP TABLE test_deadlock_a;
DROP TABLE test_fk_child_errors;
DROP TABLE test_fk_parent_errors;
DROP TABLE test_constraint_errors;
DROP TABLE test_basic_errors;

DROP DATABASE test_transaction_errors_db;

-- End of transaction error handling tests
