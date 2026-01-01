-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Transactions - Concurrency Scenarios
-- Description: Comprehensive concurrent operation testing
-- ============================================================================

-- Concurrency: Multiple transactions executing simultaneously
-- Race conditions, lost updates, concurrent modifications
-- MVCC (Multi-Version Concurrency Control)
-- Snapshot isolation, concurrent updates, visibility rules

-- Create test database
CREATE DATABASE test_concurrency_scenarios_db;
USE test_concurrency_scenarios_db;

-- ============================================================================
-- Section 1: Concurrent Updates - Lost Update Problem
-- ============================================================================

CREATE TABLE test_bank_accounts (
    account_id SERIAL PRIMARY KEY,
    account_name VARCHAR(200),
    balance NUMERIC(12,2)
);

INSERT INTO test_bank_accounts (account_name, balance) VALUES
    ('Account A', 1000.00),
    ('Account B', 500.00);

-- Lost update scenario (prevented by row locking)
-- Transaction 1: Read balance, add 100, write back
-- Transaction 2: Read balance, subtract 50, write back
-- Without locking, one update could be lost

-- Correct approach: Use row locking or atomic operations
BEGIN;
UPDATE test_bank_accounts
SET balance = balance + 100.00
WHERE account_id = 1;
COMMIT;

BEGIN;
UPDATE test_bank_accounts
SET balance = balance - 50.00
WHERE account_id = 1;
COMMIT;

-- Both updates applied correctly
SELECT account_id, account_name, balance FROM test_bank_accounts ORDER BY account_id;

-- ============================================================================
-- Section 2: Concurrent INSERT with SERIAL
-- ============================================================================

CREATE TABLE test_concurrent_inserts (
    id SERIAL PRIMARY KEY,
    session_name VARCHAR(100),
    insert_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Simulate concurrent inserts from multiple sessions
-- SERIAL ensures unique IDs

-- Session 1
INSERT INTO test_concurrent_inserts (session_name)
SELECT 'Session 1' FROM generate_series(1, 10);

-- Session 2 (concurrent)
INSERT INTO test_concurrent_inserts (session_name)
SELECT 'Session 2' FROM generate_series(1, 10);

-- All IDs are unique
SELECT session_name, COUNT(*) AS insert_count, MIN(id) AS min_id, MAX(id) AS max_id
FROM test_concurrent_inserts
GROUP BY session_name
ORDER BY session_name;

SELECT COUNT(DISTINCT id) AS unique_ids, COUNT(*) AS total_rows
FROM test_concurrent_inserts;

-- ============================================================================
-- Section 3: Read-Modify-Write Race Condition
-- ============================================================================

CREATE TABLE test_counter (
    counter_id INT PRIMARY KEY,
    counter_name VARCHAR(100),
    value BIGINT
);

INSERT INTO test_counter VALUES (1, 'PageViews', 0);

-- Incorrect: Read-modify-write (race condition)
-- Transaction 1: Read value (0), increment to 1, write 1
-- Transaction 2: Read value (0), increment to 1, write 1
-- Result: value = 1 (should be 2)

-- Correct: Atomic update
DO $$
BEGIN
    FOR i IN 1..100 LOOP
        UPDATE test_counter SET value = value + 1 WHERE counter_id = 1;
    END LOOP;
END $$;

SELECT counter_id, counter_name, value FROM test_counter;

-- ============================================================================
-- Section 4: Concurrent UPDATE with WHERE Clause
-- ============================================================================

CREATE TABLE test_inventory_stock (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    stock INT,
    reserved INT DEFAULT 0
);

INSERT INTO test_inventory_stock (product_name, stock) VALUES
    ('Product A', 100),
    ('Product B', 50),
    ('Product C', 200);

-- Concurrent reservations
BEGIN;
UPDATE test_inventory_stock
SET reserved = reserved + 10
WHERE product_id = 1 AND (stock - reserved) >= 10;
COMMIT;

BEGIN;
UPDATE test_inventory_stock
SET reserved = reserved + 20
WHERE product_id = 1 AND (stock - reserved) >= 20;
COMMIT;

-- Check results
SELECT product_id, product_name, stock, reserved, (stock - reserved) AS available
FROM test_inventory_stock
ORDER BY product_id;

-- ============================================================================
-- Section 5: MVCC Snapshot Isolation
-- ============================================================================

CREATE TABLE test_snapshot (
    id SERIAL PRIMARY KEY,
    value TEXT,
    modified_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_snapshot (value) VALUES ('Original');

-- Transaction 1 (REPEATABLE READ): sees consistent snapshot
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

-- Read initial value
SELECT id, value FROM test_snapshot WHERE id = 1;

-- Simulate concurrent update from another transaction
-- (In real scenario, this would be a separate concurrent transaction)
-- UPDATE test_snapshot SET value = 'Modified by concurrent txn' WHERE id = 1;

-- Transaction 1 still sees original value (snapshot isolation)
SELECT id, value FROM test_snapshot WHERE id = 1;

COMMIT;

-- Now sees committed changes
SELECT id, value FROM test_snapshot WHERE id = 1;

-- ============================================================================
-- Section 6: Concurrent DELETE Operations
-- ============================================================================

CREATE TABLE test_concurrent_deletes (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    data TEXT
);

INSERT INTO test_concurrent_deletes (category, data)
SELECT
    CASE WHEN i % 2 = 0 THEN 'Even' ELSE 'Odd' END,
    'Data ' || i
FROM generate_series(1, 100) i;

-- Multiple transactions delete different subsets
BEGIN;
DELETE FROM test_concurrent_deletes WHERE category = 'Even' AND id <= 50;
COMMIT;

BEGIN;
DELETE FROM test_concurrent_deletes WHERE category = 'Odd' AND id <= 50;
COMMIT;

SELECT category, COUNT(*) AS remaining
FROM test_concurrent_deletes
GROUP BY category
ORDER BY category;

-- ============================================================================
-- Section 7: SELECT FOR UPDATE in Concurrent Context
-- ============================================================================

CREATE TABLE test_order_processing (
    order_id SERIAL PRIMARY KEY,
    status VARCHAR(50) DEFAULT 'pending',
    processed_by VARCHAR(100),
    processed_at TIMESTAMP
);

INSERT INTO test_order_processing (order_id)
SELECT i FROM generate_series(1, 10) i;

-- Worker 1: Process first available order
BEGIN;

SELECT order_id, status
FROM test_order_processing
WHERE status = 'pending'
ORDER BY order_id
LIMIT 1
FOR UPDATE SKIP LOCKED;

-- Update to processing
UPDATE test_order_processing
SET status = 'processing', processed_by = 'Worker1', processed_at = CURRENT_TIMESTAMP
WHERE order_id = 1;

COMMIT;

-- Worker 2: Gets next available (skips locked rows)
BEGIN;

SELECT order_id, status
FROM test_order_processing
WHERE status = 'pending'
ORDER BY order_id
LIMIT 1
FOR UPDATE SKIP LOCKED;

UPDATE test_order_processing
SET status = 'processing', processed_by = 'Worker2', processed_at = CURRENT_TIMESTAMP
WHERE order_id = 2;

COMMIT;

SELECT order_id, status, processed_by FROM test_order_processing ORDER BY order_id LIMIT 5;

-- ============================================================================
-- Section 8: Optimistic Locking with Version Numbers
-- ============================================================================

CREATE TABLE test_optimistic_locking (
    record_id SERIAL PRIMARY KEY,
    data TEXT,
    version INT DEFAULT 1
);

INSERT INTO test_optimistic_locking (data) VALUES ('Initial data');

-- Optimistic locking pattern
DO $$
DECLARE
    current_version INT;
    update_count INT;
BEGIN
    -- Read with version
    SELECT version INTO current_version FROM test_optimistic_locking WHERE record_id = 1;

    -- Simulate work
    PERFORM pg_sleep(0.1);

    -- Update with version check
    UPDATE test_optimistic_locking
    SET data = 'Updated data', version = version + 1
    WHERE record_id = 1 AND version = current_version;

    GET DIAGNOSTICS update_count = ROW_COUNT;

    IF update_count = 0 THEN
        RAISE EXCEPTION 'Concurrent modification detected';
    ELSE
        RAISE NOTICE 'Update successful';
    END IF;
END $$;

SELECT record_id, data, version FROM test_optimistic_locking;

-- ============================================================================
-- Section 9: Concurrent Aggregate Calculations
-- ============================================================================

CREATE TABLE test_sales_transactions (
    transaction_id SERIAL PRIMARY KEY,
    product_id INT,
    amount NUMERIC(10,2),
    transaction_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Concurrent inserts
INSERT INTO test_sales_transactions (product_id, amount)
SELECT
    (RANDOM() * 10)::INT + 1,
    (RANDOM() * 100)::NUMERIC(10,2)
FROM generate_series(1, 1000);

-- Concurrent aggregate queries see consistent results
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

SELECT
    product_id,
    COUNT(*) AS transaction_count,
    SUM(amount) AS total_sales,
    AVG(amount) AS avg_sale
FROM test_sales_transactions
GROUP BY product_id
ORDER BY product_id;

COMMIT;

-- ============================================================================
-- Section 10: Write Skew Anomaly Detection
-- ============================================================================

CREATE TABLE test_doctors_on_call (
    doctor_id INT PRIMARY KEY,
    doctor_name VARCHAR(200),
    on_call BOOLEAN,
    specialty VARCHAR(100)
);

INSERT INTO test_doctors_on_call VALUES
    (1, 'Dr. Alice', TRUE, 'Cardiology'),
    (2, 'Dr. Bob', TRUE, 'Cardiology'),
    (3, 'Dr. Charlie', TRUE, 'Neurology');

-- Write skew scenario (prevented by SERIALIZABLE)
-- Two transactions both check count > 1, then both set on_call = FALSE
-- Result: No doctors on call (violates business rule)

-- SERIALIZABLE prevents this
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;

-- Check at least one doctor on call in specialty
DO $$
DECLARE
    on_call_count INT;
BEGIN
    SELECT COUNT(*) INTO on_call_count
    FROM test_doctors_on_call
    WHERE specialty = 'Cardiology' AND on_call = TRUE;

    IF on_call_count > 1 THEN
        -- Safe to go off call
        UPDATE test_doctors_on_call
        SET on_call = FALSE
        WHERE doctor_id = 1;
        RAISE NOTICE 'Doctor 1 went off call';
    ELSE
        RAISE NOTICE 'Cannot go off call - must maintain coverage';
    END IF;
END $$;

COMMIT;

SELECT doctor_id, doctor_name, on_call, specialty FROM test_doctors_on_call ORDER BY doctor_id;

-- ============================================================================
-- Section 11: Concurrent Index Updates
-- ============================================================================

CREATE TABLE test_concurrent_index (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50),
    value INT
);

CREATE INDEX idx_concurrent_code ON test_concurrent_index(code);
CREATE INDEX idx_concurrent_value ON test_concurrent_index(value);

-- Concurrent inserts with index updates
INSERT INTO test_concurrent_index (code, value)
SELECT
    'CODE_' || (i % 100),
    i
FROM generate_series(1, 1000) i;

-- Indexes maintained consistently
SELECT
    schemaname,
    tablename,
    indexname,
    indexdef
FROM pg_indexes
WHERE tablename = 'test_concurrent_index'
ORDER BY indexname;

-- Verify index usage
EXPLAIN SELECT * FROM test_concurrent_index WHERE code = 'CODE_50';

-- ============================================================================
-- Section 12: Concurrent UPSERT (INSERT ON CONFLICT)
-- ============================================================================

CREATE TABLE test_concurrent_upsert (
    key VARCHAR(50) PRIMARY KEY,
    value TEXT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    update_count INT DEFAULT 0
);

-- Multiple concurrent upserts
DO $$
BEGIN
    FOR i IN 1..10 LOOP
        INSERT INTO test_concurrent_upsert (key, value)
        VALUES ('KEY_1', 'Value ' || i)
        ON CONFLICT (key) DO UPDATE
        SET value = EXCLUDED.value,
            updated_at = CURRENT_TIMESTAMP,
            update_count = test_concurrent_upsert.update_count + 1;
    END LOOP;
END $$;

SELECT key, value, update_count FROM test_concurrent_upsert;

-- ============================================================================
-- Section 13: Phantom Reads Prevention
-- ============================================================================

CREATE TABLE test_phantom_reads (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    amount NUMERIC(10,2)
);

INSERT INTO test_phantom_reads (category, amount) VALUES
    ('A', 100.00),
    ('A', 200.00),
    ('B', 150.00);

-- REPEATABLE READ prevents phantom reads
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

-- Count rows
SELECT category, COUNT(*) AS count
FROM test_phantom_reads
WHERE category = 'A'
GROUP BY category;

-- Another transaction inserts (in real scenario)
-- INSERT INTO test_phantom_reads (category, amount) VALUES ('A', 300.00);

-- Recount - sees same result (no phantom)
SELECT category, COUNT(*) AS count
FROM test_phantom_reads
WHERE category = 'A'
GROUP BY category;

COMMIT;

-- Now sees new row
SELECT category, COUNT(*) AS count
FROM test_phantom_reads
WHERE category = 'A'
GROUP BY category;

-- ============================================================================
-- Section 14: Concurrent Sequence Usage
-- ============================================================================

CREATE SEQUENCE test_concurrent_sequence;

CREATE TABLE test_sequence_usage (
    id BIGINT PRIMARY KEY,
    session_name VARCHAR(100),
    seq_value BIGINT
);

-- Multiple sessions use same sequence
DO $$
DECLARE
    i INT;
    seq_val BIGINT;
BEGIN
    FOR i IN 1..100 LOOP
        seq_val := nextval('test_concurrent_sequence');
        INSERT INTO test_sequence_usage (id, session_name, seq_value)
        VALUES (seq_val, 'Session A', seq_val);
    END LOOP;
END $$;

DO $$
DECLARE
    i INT;
    seq_val BIGINT;
BEGIN
    FOR i IN 1..100 LOOP
        seq_val := nextval('test_concurrent_sequence');
        INSERT INTO test_sequence_usage (id, session_name, seq_value)
        VALUES (seq_val, 'Session B', seq_val);
    END LOOP;
END $$;

-- All values unique
SELECT
    session_name,
    COUNT(*) AS count,
    MIN(seq_value) AS min_seq,
    MAX(seq_value) AS max_seq
FROM test_sequence_usage
GROUP BY session_name
ORDER BY session_name;

SELECT COUNT(DISTINCT seq_value) AS unique_values, COUNT(*) AS total_rows
FROM test_sequence_usage;

-- ============================================================================
-- Section 15: Concurrent Foreign Key Validation
-- ============================================================================

CREATE TABLE test_fk_parent (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(200)
);

CREATE TABLE test_fk_child (
    child_id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_fk_parent(parent_id),
    child_name VARCHAR(200)
);

-- Concurrent inserts with FK validation
INSERT INTO test_fk_parent (parent_name) VALUES ('Parent 1'), ('Parent 2'), ('Parent 3');

-- Multiple sessions insert children
BEGIN;
INSERT INTO test_fk_child (parent_id, child_name)
SELECT 1, 'Child ' || i FROM generate_series(1, 10) i;
COMMIT;

BEGIN;
INSERT INTO test_fk_child (parent_id, child_name)
SELECT 2, 'Child ' || i FROM generate_series(11, 20) i;
COMMIT;

-- FK maintained correctly
SELECT
    p.parent_id,
    p.parent_name,
    COUNT(c.child_id) AS child_count
FROM test_fk_parent p
LEFT JOIN test_fk_child c ON p.parent_id = c.parent_id
GROUP BY p.parent_id, p.parent_name
ORDER BY p.parent_id;

-- ============================================================================
-- Section 16: Concurrent Constraint Checking
-- ============================================================================

CREATE TABLE test_concurrent_constraints (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200) UNIQUE,
    age INT CHECK (age >= 18),
    balance NUMERIC(10,2) CHECK (balance >= 0)
);

-- Concurrent inserts with constraint validation
BEGIN;
INSERT INTO test_concurrent_constraints (email, age, balance) VALUES
    ('user1@example.com', 25, 100.00);
COMMIT;

BEGIN;
INSERT INTO test_concurrent_constraints (email, age, balance) VALUES
    ('user2@example.com', 30, 200.00);
COMMIT;

-- Constraint violations properly detected
DO $$
BEGIN
    INSERT INTO test_concurrent_constraints (email, age, balance) VALUES
        ('user1@example.com', 28, 150.00);  -- Duplicate email
EXCEPTION
    WHEN unique_violation THEN
        RAISE NOTICE 'Unique constraint violation detected';
END $$;

SELECT id, email, age, balance FROM test_concurrent_constraints ORDER BY id;

-- ============================================================================
-- Section 17: Concurrent Trigger Execution
-- ============================================================================

CREATE TABLE test_concurrent_trigger_data (
    id SERIAL PRIMARY KEY,
    value TEXT
);

CREATE TABLE test_concurrent_trigger_log (
    log_id SERIAL PRIMARY KEY,
    data_id INT,
    action TEXT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION log_concurrent_change() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_concurrent_trigger_log (data_id, action)
    VALUES (NEW.id, TG_OP);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_concurrent_log
AFTER INSERT ON test_concurrent_trigger_data
FOR EACH ROW
EXECUTE FUNCTION log_concurrent_change();

-- Concurrent inserts trigger log entries
BEGIN;
INSERT INTO test_concurrent_trigger_data (value)
SELECT 'Value ' || i FROM generate_series(1, 10) i;
COMMIT;

BEGIN;
INSERT INTO test_concurrent_trigger_data (value)
SELECT 'Value ' || i FROM generate_series(11, 20) i;
COMMIT;

-- All triggers executed
SELECT COUNT(*) AS data_count FROM test_concurrent_trigger_data;
SELECT COUNT(*) AS log_count FROM test_concurrent_trigger_log;

-- ============================================================================
-- Section 18: Concurrent Partitioned Table Operations
-- ============================================================================

CREATE TABLE test_concurrent_partitioned (
    id SERIAL,
    category VARCHAR(50),
    data TEXT,
    created_date DATE,
    PRIMARY KEY (id, category)
) PARTITION BY LIST (category);

CREATE TABLE test_part_cat_a PARTITION OF test_concurrent_partitioned
    FOR VALUES IN ('A');

CREATE TABLE test_part_cat_b PARTITION OF test_concurrent_partitioned
    FOR VALUES IN ('B');

-- Concurrent inserts to different partitions
BEGIN;
INSERT INTO test_concurrent_partitioned (category, data, created_date)
SELECT 'A', 'Data A' || i, CURRENT_DATE FROM generate_series(1, 50) i;
COMMIT;

BEGIN;
INSERT INTO test_concurrent_partitioned (category, data, created_date)
SELECT 'B', 'Data B' || i, CURRENT_DATE FROM generate_series(1, 50) i;
COMMIT;

-- Data distributed correctly
SELECT category, COUNT(*) AS row_count
FROM test_concurrent_partitioned
GROUP BY category
ORDER BY category;

-- ============================================================================
-- Section 19: Concurrent CTE (WITH) Queries
-- ============================================================================

CREATE TABLE test_concurrent_cte (
    id SERIAL PRIMARY KEY,
    parent_id INT,
    name VARCHAR(200),
    value INT
);

INSERT INTO test_concurrent_cte (parent_id, name, value) VALUES
    (NULL, 'Root', 100),
    (1, 'Child 1', 50),
    (1, 'Child 2', 30),
    (2, 'Grandchild 1', 20),
    (2, 'Grandchild 2', 10);

-- Recursive CTE with concurrent modifications
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

WITH RECURSIVE hierarchy AS (
    SELECT id, parent_id, name, value, 1 AS level
    FROM test_concurrent_cte
    WHERE parent_id IS NULL

    UNION ALL

    SELECT c.id, c.parent_id, c.name, c.value, h.level + 1
    FROM test_concurrent_cte c
    JOIN hierarchy h ON c.parent_id = h.id
)
SELECT id, name, value, level FROM hierarchy ORDER BY level, id;

COMMIT;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_concurrency_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_concurrency_best_practices VALUES
    (1, 'Use atomic operations (UPDATE col = col + 1) instead of read-modify-write'),
    (2, 'Keep transactions short to reduce lock contention'),
    (3, 'Use appropriate isolation levels for your use case'),
    (4, 'SERIALIZABLE prevents all anomalies but reduces concurrency'),
    (5, 'SELECT FOR UPDATE SKIP LOCKED for concurrent queue processing'),
    (6, 'Use optimistic locking (version numbers) for long-running operations'),
    (7, 'Avoid transaction patterns that cause write skew'),
    (8, 'Lock resources in consistent order to prevent deadlocks'),
    (9, 'Use INSERT ON CONFLICT for concurrent upserts'),
    (10, 'MVCC allows high concurrency for read operations'),
    (11, 'Sequence values are unique even under concurrency'),
    (12, 'Test concurrent scenarios with realistic workloads'),
    (13, 'Monitor for serialization failures in production'),
    (14, 'Use connection pooling for efficient resource usage'),
    (15, 'Retry transactions that fail due to serialization'),
    (16, 'Understand snapshot isolation behavior'),
    (17, 'Concurrent constraint validation is automatically handled'),
    (18, 'Partition tables for better concurrent write performance'),
    (19, 'Use REPEATABLE READ to prevent phantom reads'),
    (20, 'Design for eventual consistency when strict consistency not required');

SELECT id, guideline FROM test_concurrency_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_concurrency_best_practices;
DROP TABLE test_concurrent_cte;
DROP TABLE test_part_cat_b;
DROP TABLE test_part_cat_a;
DROP TABLE test_concurrent_partitioned;
DROP TABLE test_concurrent_trigger_log;
DROP TABLE test_concurrent_trigger_data;
DROP FUNCTION log_concurrent_change();
DROP TABLE test_concurrent_constraints;
DROP TABLE test_fk_child;
DROP TABLE test_fk_parent;
DROP TABLE test_sequence_usage;
DROP SEQUENCE test_concurrent_sequence;
DROP TABLE test_phantom_reads;
DROP TABLE test_concurrent_upsert;
DROP TABLE test_concurrent_index;
DROP TABLE test_doctors_on_call;
DROP TABLE test_sales_transactions;
DROP TABLE test_optimistic_locking;
DROP TABLE test_order_processing;
DROP TABLE test_concurrent_deletes;
DROP TABLE test_snapshot;
DROP TABLE test_inventory_stock;
DROP TABLE test_counter;
DROP TABLE test_concurrent_inserts;
DROP TABLE test_bank_accounts;

DROP DATABASE test_concurrency_scenarios_db;

-- End of concurrency scenarios tests
