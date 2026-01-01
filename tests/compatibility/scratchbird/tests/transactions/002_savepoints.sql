-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - SAVEPOINT
-- Description: Comprehensive SAVEPOINT testing
-- ============================================================================

-- SAVEPOINT: Create intermediate checkpoint in transaction
-- ROLLBACK TO SAVEPOINT: Undo to savepoint
-- RELEASE SAVEPOINT: Remove savepoint
-- Allows partial rollback within transaction

-- Create test database
CREATE DATABASE test_savepoints_db;
USE test_savepoints_db;

-- ============================================================================
-- Section 1: Basic SAVEPOINT
-- ============================================================================

CREATE TABLE test_employees (
    emp_id SERIAL PRIMARY KEY,
    emp_name VARCHAR(200),
    salary NUMERIC(12,2)
);

BEGIN;

INSERT INTO test_employees (emp_name, salary) VALUES ('Alice', 75000);

SAVEPOINT sp1;

INSERT INTO test_employees (emp_name, salary) VALUES ('Bob', 65000);

SAVEPOINT sp2;

INSERT INTO test_employees (emp_name, salary) VALUES ('Charlie', 55000);

-- Rollback to sp2 (removes Charlie)
ROLLBACK TO SAVEPOINT sp2;

-- Rollback to sp1 (removes Bob)
ROLLBACK TO SAVEPOINT sp1;

COMMIT;

-- Only Alice exists
SELECT emp_id, emp_name, salary FROM test_employees ORDER BY emp_id;

-- ============================================================================
-- Section 2: Multiple Savepoints
-- ============================================================================

BEGIN;

INSERT INTO test_employees (emp_name, salary) VALUES ('Diana', 85000);
SAVEPOINT sp_diana;

INSERT INTO test_employees (emp_name, salary) VALUES ('Eve', 60000);
SAVEPOINT sp_eve;

INSERT INTO test_employees (emp_name, salary) VALUES ('Frank', 70000);
SAVEPOINT sp_frank;

-- Rollback just Frank
ROLLBACK TO SAVEPOINT sp_frank;

COMMIT;

-- Diana and Eve exist, Frank doesn't
SELECT emp_name FROM test_employees WHERE emp_name IN ('Diana', 'Eve', 'Frank') ORDER BY emp_name;

-- ============================================================================
-- Section 3: RELEASE SAVEPOINT
-- ============================================================================

BEGIN;

INSERT INTO test_employees (emp_name, salary) VALUES ('Grace', 80000);
SAVEPOINT sp_grace;

INSERT INTO test_employees (emp_name, salary) VALUES ('Henry', 90000);
SAVEPOINT sp_henry;

-- Release savepoint (can't rollback to it anymore)
RELEASE SAVEPOINT sp_grace;

-- Can still rollback to sp_henry
ROLLBACK TO SAVEPOINT sp_henry;

COMMIT;

-- Grace exists, Henry doesn't
SELECT emp_name FROM test_employees WHERE emp_name IN ('Grace', 'Henry') ORDER BY emp_name;

-- ============================================================================
-- Section 4: Nested Savepoints
-- ============================================================================

BEGIN;

INSERT INTO test_employees (emp_name, salary) VALUES ('Level1', 50000);
SAVEPOINT level1;

INSERT INTO test_employees (emp_name, salary) VALUES ('Level2', 60000);
SAVEPOINT level2;

INSERT INTO test_employees (emp_name, salary) VALUES ('Level3', 70000);
SAVEPOINT level3;

-- Rollback innermost
ROLLBACK TO SAVEPOINT level3;

-- Rollback to level1 (also removes level2)
ROLLBACK TO SAVEPOINT level1;

COMMIT;

-- Only Level1 exists
SELECT emp_name FROM test_employees WHERE emp_name LIKE 'Level%' ORDER BY emp_name;

-- ============================================================================
-- Section 5: Savepoint with Error Recovery
-- ============================================================================

CREATE TABLE test_error_recovery (
    id SERIAL PRIMARY KEY,
    value INT CHECK (value > 0)
);

BEGIN;

INSERT INTO test_error_recovery (value) VALUES (10);
SAVEPOINT sp_before_error;

-- Attempt invalid insert
BEGIN
    INSERT INTO test_error_recovery (value) VALUES (-5);  -- Fails CHECK
EXCEPTION
    WHEN check_violation THEN
        ROLLBACK TO SAVEPOINT sp_before_error;
        RAISE NOTICE 'Error recovered, rolling back to savepoint';
END;

-- Continue with valid data
INSERT INTO test_error_recovery (value) VALUES (20);

COMMIT;

SELECT id, value FROM test_error_recovery ORDER BY id;

-- ============================================================================
-- Section 6: Savepoint Reuse
-- ============================================================================

BEGIN;

INSERT INTO test_employees (emp_name, salary) VALUES ('Reuse1', 55000);
SAVEPOINT sp_reuse;

INSERT INTO test_employees (emp_name, salary) VALUES ('Reuse2', 65000);
ROLLBACK TO SAVEPOINT sp_reuse;

-- Reuse same savepoint name
INSERT INTO test_employees (emp_name, salary) VALUES ('Reuse3', 75000);
SAVEPOINT sp_reuse;  -- Overwrites previous sp_reuse

INSERT INTO test_employees (emp_name, salary) VALUES ('Reuse4', 85000);
ROLLBACK TO SAVEPOINT sp_reuse;  -- Rolls back to Reuse3

COMMIT;

SELECT emp_name FROM test_employees WHERE emp_name LIKE 'Reuse%' ORDER BY emp_name;

-- ============================================================================
-- Section 7: Savepoint with DDL
-- ============================================================================

BEGIN;

CREATE TABLE test_ddl_savepoint (id INT);
SAVEPOINT sp_after_create;

ALTER TABLE test_ddl_savepoint ADD COLUMN name VARCHAR(100);
SAVEPOINT sp_after_alter;

-- Rollback ALTER
ROLLBACK TO SAVEPOINT sp_after_alter;

-- Table exists but without name column
INSERT INTO test_ddl_savepoint (id) VALUES (1);

COMMIT;

SELECT id FROM test_ddl_savepoint;

-- Cleanup
DROP TABLE test_ddl_savepoint;

-- ============================================================================
-- Section 8: Savepoint with UPDATE
-- ============================================================================

CREATE TABLE test_savepoint_update (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    updated_at TIMESTAMP
);

INSERT INTO test_savepoint_update (status) VALUES ('draft'), ('review'), ('published');

BEGIN;

UPDATE test_savepoint_update SET status = 'archived', updated_at = CURRENT_TIMESTAMP WHERE id = 1;
SAVEPOINT sp_first_update;

UPDATE test_savepoint_update SET status = 'deleted', updated_at = CURRENT_TIMESTAMP WHERE id = 2;
SAVEPOINT sp_second_update;

-- Rollback second update only
ROLLBACK TO SAVEPOINT sp_second_update;

COMMIT;

SELECT id, status FROM test_savepoint_update ORDER BY id;

-- ============================================================================
-- Section 9: Savepoint with DELETE
-- ============================================================================

CREATE TABLE test_savepoint_delete (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_savepoint_delete (data) VALUES ('Data 1'), ('Data 2'), ('Data 3');

BEGIN;

DELETE FROM test_savepoint_delete WHERE id = 1;
SAVEPOINT sp_after_delete;

DELETE FROM test_savepoint_delete WHERE id = 2;

-- Rollback second delete
ROLLBACK TO SAVEPOINT sp_after_delete;

COMMIT;

-- Data 1 deleted, Data 2 and 3 remain
SELECT id, data FROM test_savepoint_delete ORDER BY id;

-- ============================================================================
-- Section 10: Complex Transaction with Multiple Savepoints
-- ============================================================================

CREATE TABLE test_orders (
    order_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    total NUMERIC(10,2),
    status VARCHAR(50)
);

CREATE TABLE test_order_items (
    item_id SERIAL PRIMARY KEY,
    order_id INT REFERENCES test_orders(order_id),
    product VARCHAR(200),
    quantity INT
);

BEGIN;

-- Create order
INSERT INTO test_orders (customer_name, total, status) VALUES ('Customer A', 0, 'pending') RETURNING order_id INTO order_id_var;
SAVEPOINT sp_order_created;

-- Add items
INSERT INTO test_order_items (order_id, product, quantity) VALUES (1, 'Widget', 2);
INSERT INTO test_order_items (order_id, product, quantity) VALUES (1, 'Gadget', 1);
SAVEPOINT sp_items_added;

-- Update total
UPDATE test_orders SET total = 99.99 WHERE order_id = 1;
SAVEPOINT sp_total_updated;

-- Update status
UPDATE test_orders SET status = 'confirmed' WHERE order_id = 1;

-- Decision point: rollback status update
ROLLBACK TO SAVEPOINT sp_total_updated;

COMMIT;

SELECT o.order_id, o.customer_name, o.total, o.status, COUNT(i.item_id) AS item_count
FROM test_orders o
LEFT JOIN test_order_items i ON o.order_id = i.order_id
WHERE o.order_id = 1
GROUP BY o.order_id, o.customer_name, o.total, o.status;

-- ============================================================================
-- Section 11: Savepoint Hierarchy
-- ============================================================================

BEGIN;

INSERT INTO test_employees (emp_name, salary) VALUES ('SP_A', 50000);
SAVEPOINT sp_a;

INSERT INTO test_employees (emp_name, salary) VALUES ('SP_B', 60000);
SAVEPOINT sp_b;

INSERT INTO test_employees (emp_name, salary) VALUES ('SP_C', 70000);
SAVEPOINT sp_c;

-- Rollback to sp_a removes sp_b and sp_c
ROLLBACK TO SAVEPOINT sp_a;

-- Cannot rollback to sp_b (it was removed)
-- ROLLBACK TO SAVEPOINT sp_b;  -- Would fail

COMMIT;

SELECT emp_name FROM test_employees WHERE emp_name LIKE 'SP_%' ORDER BY emp_name;

-- ============================================================================
-- Section 12: Savepoint with RETURNING Clause
-- ============================================================================

CREATE TABLE test_sp_returning (
    id SERIAL PRIMARY KEY,
    value TEXT
);

BEGIN;

INSERT INTO test_sp_returning (value) VALUES ('Value 1') RETURNING id;
SAVEPOINT sp_first;

INSERT INTO test_sp_returning (value) VALUES ('Value 2') RETURNING id;
SAVEPOINT sp_second;

ROLLBACK TO SAVEPOINT sp_first;

COMMIT;

SELECT id, value FROM test_sp_returning ORDER BY id;

-- ============================================================================
-- Section 13: Savepoint Memory Management
-- ============================================================================

-- Many savepoints in single transaction
BEGIN;

DO $$
DECLARE
    i INT;
BEGIN
    FOR i IN 1..100 LOOP
        EXECUTE format('SAVEPOINT sp_%s', i);
        INSERT INTO test_employees (emp_name, salary) VALUES ('SP_' || i, 40000 + i * 100);
    END LOOP;

    -- Rollback to middle savepoint
    ROLLBACK TO SAVEPOINT sp_50;
END $$;

COMMIT;

-- Only first 50 employees from this batch exist
SELECT COUNT(*) FROM test_employees WHERE emp_name LIKE 'SP_%';

-- ============================================================================
-- Section 14: Savepoint with Sequences
-- ============================================================================

CREATE SEQUENCE test_sp_seq;

BEGIN;

SELECT nextval('test_sp_seq') AS val1;
SAVEPOINT sp_seq;

SELECT nextval('test_sp_seq') AS val2;
SELECT nextval('test_sp_seq') AS val3;

-- Rollback savepoint
ROLLBACK TO SAVEPOINT sp_seq;

-- Sequence values are NOT rolled back (consumed)
SELECT currval('test_sp_seq') AS current_val;

COMMIT;

-- ============================================================================
-- Section 15: Savepoint Naming
-- ============================================================================

BEGIN;

-- Descriptive savepoint names
SAVEPOINT before_critical_update;
UPDATE test_employees SET salary = salary * 1.1 WHERE emp_name = 'Alice';

SAVEPOINT after_critical_update;
UPDATE test_employees SET salary = salary * 1.05 WHERE emp_name != 'Alice';

-- Rollback non-Alice updates
ROLLBACK TO SAVEPOINT after_critical_update;

COMMIT;

-- ============================================================================
-- Section 16: Savepoint with Foreign Keys
-- ============================================================================

CREATE TABLE test_sp_parent (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE test_sp_child (
    id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_sp_parent(id),
    data TEXT
);

BEGIN;

INSERT INTO test_sp_parent (name) VALUES ('Parent 1');
SAVEPOINT sp_parent;

INSERT INTO test_sp_child (parent_id, data) VALUES (1, 'Child 1');
SAVEPOINT sp_child;

-- Rollback child insert
ROLLBACK TO SAVEPOINT sp_child;

COMMIT;

SELECT p.name, COUNT(c.id) AS child_count
FROM test_sp_parent p
LEFT JOIN test_sp_child c ON p.id = c.parent_id
GROUP BY p.name;

-- ============================================================================
-- Section 17: Savepoint with Triggers
-- ============================================================================

CREATE TABLE test_sp_trigger_log (
    log_id SERIAL PRIMARY KEY,
    emp_name VARCHAR(200),
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION log_emp_insert() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_sp_trigger_log (emp_name) VALUES (NEW.emp_name);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_emp
AFTER INSERT ON test_employees
FOR EACH ROW
EXECUTE FUNCTION log_emp_insert();

BEGIN;

INSERT INTO test_employees (emp_name, salary) VALUES ('Trigger1', 55000);
SAVEPOINT sp_trigger;

INSERT INTO test_employees (emp_name, salary) VALUES ('Trigger2', 65000);

-- Rollback Trigger2
ROLLBACK TO SAVEPOINT sp_trigger;

COMMIT;

-- Trigger actions are also rolled back
SELECT emp_name FROM test_sp_trigger_log ORDER BY log_id;

-- ============================================================================
-- Section 18: Savepoint Error Scenarios
-- ============================================================================

-- Cannot release non-existent savepoint
BEGIN;
DO $$
BEGIN
    RELEASE SAVEPOINT non_existent_sp;
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'Cannot release non-existent savepoint';
END $$;
COMMIT;

-- Cannot rollback to non-existent savepoint
BEGIN;
DO $$
BEGIN
    ROLLBACK TO SAVEPOINT non_existent_sp;
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'Cannot rollback to non-existent savepoint';
END $$;
COMMIT;

-- ============================================================================
-- Section 19: Savepoint with Cursors
-- ============================================================================

BEGIN;

SAVEPOINT sp_before_cursor;

DECLARE emp_cursor CURSOR FOR SELECT emp_name, salary FROM test_employees;

SAVEPOINT sp_after_cursor;

-- Fetch some rows
FETCH NEXT FROM emp_cursor;

-- Rollback doesn't affect cursor
ROLLBACK TO SAVEPOINT sp_after_cursor;

CLOSE emp_cursor;

COMMIT;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_savepoint_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_savepoint_best_practices VALUES
    (1, 'Use SAVEPOINT for partial rollback within transaction'),
    (2, 'Create savepoints before risky operations'),
    (3, 'ROLLBACK TO SAVEPOINT undoes changes to that point'),
    (4, 'RELEASE SAVEPOINT removes checkpoint'),
    (5, 'Released savepoints cannot be rolled back to'),
    (6, 'Nested savepoints create hierarchy'),
    (7, 'Rolling back to outer savepoint removes inner ones'),
    (8, 'Savepoint names can be reused (overwrites previous)'),
    (9, 'Use descriptive savepoint names'),
    (10, 'Savepoints work with DDL in PostgreSQL'),
    (11, 'Savepoints good for error recovery'),
    (12, 'Sequence values NOT rolled back by savepoints'),
    (13, 'Trigger actions ARE rolled back with savepoints'),
    (14, 'Keep savepoint usage reasonable (memory)'),
    (15, 'Test savepoint rollback paths thoroughly');

SELECT id, guideline FROM test_savepoint_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_savepoint_best_practices;
DROP TRIGGER trg_log_emp ON test_employees;
DROP FUNCTION log_emp_insert();
DROP TABLE test_sp_trigger_log;
DROP TABLE test_sp_child;
DROP TABLE test_sp_parent;
DROP SEQUENCE test_sp_seq;
DROP TABLE test_sp_returning;
DROP TABLE test_order_items;
DROP TABLE test_orders;
DROP TABLE test_savepoint_delete;
DROP TABLE test_savepoint_update;
DROP TABLE test_error_recovery;
DROP TABLE test_employees;

DROP DATABASE test_savepoints_db;

-- End of savepoint tests
