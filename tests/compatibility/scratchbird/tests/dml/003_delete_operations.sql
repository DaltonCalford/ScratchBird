-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - DELETE Operations
-- Description: Comprehensive DELETE statement testing
-- ============================================================================

-- DELETE DML: Removing data from tables
-- DELETE with WHERE, joins, subqueries
-- DELETE RETURNING, CASCADE, TRUNCATE
-- Soft deletes, performance patterns

-- Create test database
CREATE DATABASE test_delete_operations_db;
USE test_delete_operations_db;

-- ============================================================================
-- Section 1: Basic DELETE
-- ============================================================================

CREATE TABLE test_basic_delete (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    status VARCHAR(50)
);

INSERT INTO test_basic_delete (name, status) VALUES
    ('Item 1', 'active'),
    ('Item 2', 'inactive'),
    ('Item 3', 'active'),
    ('Item 4', 'deleted');

-- Delete single row
DELETE FROM test_basic_delete WHERE id = 1;

-- Delete multiple rows
DELETE FROM test_basic_delete WHERE status = 'inactive';

SELECT * FROM test_basic_delete ORDER BY id;

-- ============================================================================
-- Section 2: DELETE All Rows
-- ============================================================================

CREATE TABLE test_delete_all (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_delete_all (data) VALUES ('Row 1'), ('Row 2'), ('Row 3');

-- Delete all rows (no WHERE clause - be careful!)
DELETE FROM test_delete_all;

SELECT COUNT(*) AS remaining_rows FROM test_delete_all;

-- ============================================================================
-- Section 3: DELETE with Multiple Conditions
-- ============================================================================

CREATE TABLE test_delete_conditions (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    value INT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_delete_conditions (category, value) VALUES
    ('A', 10),
    ('A', 20),
    ('B', 15),
    ('B', 25),
    ('C', 30);

-- Delete with AND conditions
DELETE FROM test_delete_conditions
WHERE category = 'A' AND value < 15;

-- Delete with OR conditions
DELETE FROM test_delete_conditions
WHERE category = 'B' OR value > 25;

SELECT * FROM test_delete_conditions ORDER BY id;

-- ============================================================================
-- Section 4: DELETE with USING (JOIN)
-- ============================================================================

CREATE TABLE test_orders (
    order_id SERIAL PRIMARY KEY,
    customer_id INT,
    order_date DATE
);

CREATE TABLE test_customers (
    customer_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    is_active BOOLEAN
);

INSERT INTO test_customers (customer_name, is_active) VALUES
    ('Customer A', TRUE),
    ('Customer B', FALSE),
    ('Customer C', TRUE);

INSERT INTO test_orders (customer_id, order_date) VALUES
    (1, '2024-01-01'),
    (2, '2024-01-02'),
    (3, '2024-01-03');

-- Delete orders for inactive customers
DELETE FROM test_orders
USING test_customers
WHERE test_orders.customer_id = test_customers.customer_id
  AND test_customers.is_active = FALSE;

SELECT * FROM test_orders ORDER BY order_id;

-- ============================================================================
-- Section 5: DELETE with Subquery
-- ============================================================================

CREATE TABLE test_products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2)
);

INSERT INTO test_products (product_name, price) VALUES
    ('Product A', 100.00),
    ('Product B', 200.00),
    ('Product C', 50.00),
    ('Product D', 150.00);

-- Delete products above average price
DELETE FROM test_products
WHERE price > (SELECT AVG(price) FROM test_products);

SELECT * FROM test_products ORDER BY product_id;

-- ============================================================================
-- Section 6: DELETE RETURNING
-- ============================================================================

CREATE TABLE test_delete_returning (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    value INT
);

INSERT INTO test_delete_returning (name, value) VALUES
    ('Item 1', 10),
    ('Item 2', 20),
    ('Item 3', 30);

-- Return deleted rows
DELETE FROM test_delete_returning
WHERE value > 15
RETURNING *;

-- Verify remaining rows
SELECT * FROM test_delete_returning ORDER BY id;

-- ============================================================================
-- Section 7: DELETE with NOT EXISTS
-- ============================================================================

CREATE TABLE test_parent_records (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(100)
);

CREATE TABLE test_child_records (
    child_id SERIAL PRIMARY KEY,
    parent_id INT,
    child_name VARCHAR(100)
);

INSERT INTO test_parent_records (parent_name) VALUES
    ('Parent 1'),
    ('Parent 2'),
    ('Parent 3');

INSERT INTO test_child_records (parent_id, child_name) VALUES
    (1, 'Child 1'),
    (1, 'Child 2');

-- Delete parents with no children
DELETE FROM test_parent_records p
WHERE NOT EXISTS (
    SELECT 1 FROM test_child_records c
    WHERE c.parent_id = p.parent_id
);

SELECT * FROM test_parent_records ORDER BY parent_id;

-- ============================================================================
-- Section 8: DELETE with IN Subquery
-- ============================================================================

CREATE TABLE test_active_users (
    user_id INT PRIMARY KEY,
    username VARCHAR(100)
);

CREATE TABLE test_inactive_list (
    user_id INT
);

INSERT INTO test_active_users VALUES
    (1, 'user1'),
    (2, 'user2'),
    (3, 'user3'),
    (4, 'user4');

INSERT INTO test_inactive_list VALUES (2), (4);

-- Delete users in inactive list
DELETE FROM test_active_users
WHERE user_id IN (SELECT user_id FROM test_inactive_list);

SELECT * FROM test_active_users ORDER BY user_id;

-- ============================================================================
-- Section 9: CASCADE DELETE (Foreign Key)
-- ============================================================================

CREATE TABLE test_cascade_parent (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(100)
);

CREATE TABLE test_cascade_child (
    child_id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_cascade_parent(parent_id) ON DELETE CASCADE,
    child_name VARCHAR(100)
);

INSERT INTO test_cascade_parent (parent_name) VALUES ('Parent 1'), ('Parent 2');
INSERT INTO test_cascade_child (parent_id, child_name) VALUES
    (1, 'Child 1A'),
    (1, 'Child 1B'),
    (2, 'Child 2A');

-- Delete parent (cascades to children)
DELETE FROM test_cascade_parent WHERE parent_id = 1;

-- Verify cascade
SELECT * FROM test_cascade_parent ORDER BY parent_id;
SELECT * FROM test_cascade_child ORDER BY child_id;

-- ============================================================================
-- Section 10: Soft DELETE Pattern
-- ============================================================================

CREATE TABLE test_soft_delete (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    is_deleted BOOLEAN DEFAULT FALSE,
    deleted_at TIMESTAMP
);

INSERT INTO test_soft_delete (name) VALUES
    ('Item 1'),
    ('Item 2'),
    ('Item 3');

-- Soft delete (mark as deleted, don't remove)
UPDATE test_soft_delete
SET is_deleted = TRUE,
    deleted_at = CURRENT_TIMESTAMP
WHERE id = 2;

-- Query active records only
SELECT * FROM test_soft_delete WHERE NOT is_deleted ORDER BY id;

-- ============================================================================
-- Section 11: DELETE with Common Table Expression (CTE)
-- ============================================================================

CREATE TABLE test_cte_delete (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    value INT
);

INSERT INTO test_cte_delete (category, value) VALUES
    ('A', 10),
    ('A', 20),
    ('B', 15),
    ('B', 25),
    ('C', 30);

-- Delete using CTE
WITH rows_to_delete AS (
    SELECT id
    FROM test_cte_delete
    WHERE category = 'A' AND value < 15
)
DELETE FROM test_cte_delete
WHERE id IN (SELECT id FROM rows_to_delete);

SELECT * FROM test_cte_delete ORDER BY id;

-- ============================================================================
-- Section 12: DELETE with Date/Time Conditions
-- ============================================================================

CREATE TABLE test_delete_datetime (
    id SERIAL PRIMARY KEY,
    data TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_delete_datetime (data, created_at) VALUES
    ('Old data', CURRENT_TIMESTAMP - INTERVAL '60 days'),
    ('Recent data', CURRENT_TIMESTAMP - INTERVAL '5 days'),
    ('New data', CURRENT_TIMESTAMP);

-- Delete old records (older than 30 days)
DELETE FROM test_delete_datetime
WHERE created_at < CURRENT_TIMESTAMP - INTERVAL '30 days';

SELECT * FROM test_delete_datetime ORDER BY id;

-- ============================================================================
-- Section 13: TRUNCATE TABLE (Fast Delete All)
-- ============================================================================

CREATE TABLE test_truncate (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_truncate (data)
SELECT 'Data ' || i FROM generate_series(1, 10000) i;

-- Truncate (faster than DELETE for all rows)
TRUNCATE TABLE test_truncate;

-- Verify truncated
SELECT COUNT(*) AS row_count FROM test_truncate;

-- Sequence reset with RESTART IDENTITY
INSERT INTO test_truncate (data) VALUES ('After truncate');
SELECT * FROM test_truncate;  -- id starts from 1 again (if RESTART IDENTITY used)

-- ============================================================================
-- Section 14: TRUNCATE CASCADE
-- ============================================================================

CREATE TABLE test_trunc_parent (
    parent_id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE TABLE test_trunc_child (
    child_id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_trunc_parent(parent_id)
);

INSERT INTO test_trunc_parent (data) VALUES ('Parent 1'), ('Parent 2');
INSERT INTO test_trunc_child (parent_id) VALUES (1), (2);

-- Truncate with cascade
TRUNCATE TABLE test_trunc_parent CASCADE;

-- Both tables truncated
SELECT COUNT(*) AS parent_count FROM test_trunc_parent;
SELECT COUNT(*) AS child_count FROM test_trunc_child;

-- ============================================================================
-- Section 15: DELETE vs TRUNCATE Performance
-- ============================================================================

CREATE TABLE test_delete_perf (
    id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE TABLE test_truncate_perf (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Populate both tables
INSERT INTO test_delete_perf (data)
SELECT 'Data ' || i FROM generate_series(1, 10000) i;

INSERT INTO test_truncate_perf (data)
SELECT 'Data ' || i FROM generate_series(1, 10000) i;

-- DELETE (slower, creates dead tuples)
-- DELETE FROM test_delete_perf;

-- TRUNCATE (faster, no dead tuples)
TRUNCATE TABLE test_truncate_perf;

-- ============================================================================
-- Section 16: DELETE in Partitioned Table
-- ============================================================================

CREATE TABLE test_partitioned_delete (
    id SERIAL,
    category VARCHAR(50),
    value INT,
    PRIMARY KEY (id, category)
) PARTITION BY LIST (category);

CREATE TABLE test_part_a PARTITION OF test_partitioned_delete
    FOR VALUES IN ('A');

CREATE TABLE test_part_b PARTITION OF test_partitioned_delete
    FOR VALUES IN ('B');

INSERT INTO test_partitioned_delete (category, value) VALUES
    ('A', 10),
    ('A', 20),
    ('B', 30);

-- Delete from partitioned table
DELETE FROM test_partitioned_delete WHERE value < 15;

SELECT * FROM test_partitioned_delete ORDER BY id;

-- ============================================================================
-- Section 17: DELETE with Self-Referencing Table
-- ============================================================================

CREATE TABLE test_self_reference (
    id SERIAL PRIMARY KEY,
    parent_id INT,
    name VARCHAR(100),
    FOREIGN KEY (parent_id) REFERENCES test_self_reference(id) ON DELETE CASCADE
);

INSERT INTO test_self_reference (id, parent_id, name) VALUES
    (1, NULL, 'Root'),
    (2, 1, 'Child 1'),
    (3, 1, 'Child 2'),
    (4, 2, 'Grandchild');

-- Delete cascades through hierarchy
DELETE FROM test_self_reference WHERE id = 1;

SELECT * FROM test_self_reference ORDER BY id;

-- ============================================================================
-- Section 18: DELETE with Array/JSONB Conditions
-- ============================================================================

CREATE TABLE test_delete_complex (
    id SERIAL PRIMARY KEY,
    tags TEXT[],
    metadata JSONB
);

INSERT INTO test_delete_complex (tags, metadata) VALUES
    (ARRAY['tag1', 'tag2'], '{"status": "active"}'::JSONB),
    (ARRAY['tag3'], '{"status": "inactive"}'::JSONB),
    (ARRAY['tag1', 'tag3'], '{"status": "active"}'::JSONB);

-- Delete by array containment
DELETE FROM test_delete_complex
WHERE 'tag2' = ANY(tags);

-- Delete by JSONB field
DELETE FROM test_delete_complex
WHERE metadata->>'status' = 'inactive';

SELECT * FROM test_delete_complex ORDER BY id;

-- ============================================================================
-- Section 19: Batch DELETE Pattern
-- ============================================================================

CREATE TABLE test_batch_delete (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    processed BOOLEAN DEFAULT FALSE
);

INSERT INTO test_batch_delete (created_at)
SELECT CURRENT_TIMESTAMP - (i || ' days')::INTERVAL
FROM generate_series(1, 100) i;

-- Delete in batches (to avoid long locks)
DO $$
DECLARE
    deleted_count INT;
    batch_size INT := 10;
BEGIN
    LOOP
        DELETE FROM test_batch_delete
        WHERE id IN (
            SELECT id FROM test_batch_delete
            WHERE created_at < CURRENT_TIMESTAMP - INTERVAL '30 days'
            LIMIT batch_size
        );

        GET DIAGNOSTICS deleted_count = ROW_COUNT;
        EXIT WHEN deleted_count = 0;

        RAISE NOTICE 'Deleted % rows', deleted_count;
    END LOOP;
END $$;

SELECT COUNT(*) AS remaining_rows FROM test_batch_delete;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_delete_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_delete_best_practices VALUES
    (1, 'Always use WHERE clause (avoid accidental full table delete)'),
    (2, 'Test DELETE with SELECT first to verify affected rows'),
    (3, 'Use transactions for safety (can rollback if needed)'),
    (4, 'DELETE RETURNING for auditing deleted data'),
    (5, 'CASCADE deletes propagate through foreign keys'),
    (6, 'Soft delete pattern for data retention'),
    (7, 'TRUNCATE for deleting all rows (faster than DELETE)'),
    (8, 'TRUNCATE RESTART IDENTITY to reset sequences'),
    (9, 'Batch deletes for large datasets (avoid long locks)'),
    (10, 'Index columns used in WHERE clause'),
    (11, 'VACUUM after large deletes (reclaim space)'),
    (12, 'DELETE creates dead tuples (MVCC overhead)'),
    (13, 'NOT EXISTS for deleting orphaned records'),
    (14, 'Archive old data before deleting'),
    (15, 'Monitor DELETE performance'),
    (16, 'Consider partitioning for easier data lifecycle'),
    (17, 'Use CTEs for complex delete logic'),
    (18, 'Backup before major deletions in production'),
    (19, 'ON DELETE CASCADE vs manual cascade (control)'),
    (20, 'Document delete policies and retention periods');

SELECT id, guideline FROM test_delete_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_delete_best_practices;
DROP TABLE test_batch_delete;
DROP TABLE test_delete_complex;
DROP TABLE test_self_reference;
DROP TABLE test_part_b;
DROP TABLE test_part_a;
DROP TABLE test_partitioned_delete;
DROP TABLE test_truncate_perf;
DROP TABLE test_delete_perf;
DROP TABLE test_trunc_child;
DROP TABLE test_trunc_parent;
DROP TABLE test_truncate;
DROP TABLE test_delete_datetime;
DROP TABLE test_cte_delete;
DROP TABLE test_soft_delete;
DROP TABLE test_cascade_child;
DROP TABLE test_cascade_parent;
DROP TABLE test_inactive_list;
DROP TABLE test_active_users;
DROP TABLE test_child_records;
DROP TABLE test_parent_records;
DROP TABLE test_delete_returning;
DROP TABLE test_products;
DROP TABLE test_orders;
DROP TABLE test_customers;
DROP TABLE test_delete_conditions;
DROP TABLE test_delete_all;
DROP TABLE test_basic_delete;

DROP DATABASE test_delete_operations_db;

-- End of DELETE operations tests
