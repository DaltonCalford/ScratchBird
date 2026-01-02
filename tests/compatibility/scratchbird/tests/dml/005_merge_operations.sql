-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - MERGE Operations
-- Description: Comprehensive MERGE statement testing (PostgreSQL 15+)
-- ============================================================================

-- MERGE DML: Conditional INSERT, UPDATE, DELETE in one statement
-- MERGE with WHEN MATCHED, WHEN NOT MATCHED
-- Synchronization, upsert patterns, data reconciliation
-- Alternative patterns for older PostgreSQL versions

-- Create test database
CREATE DATABASE test_merge_operations_db;
USE test_merge_operations_db;

-- ============================================================================
-- Section 1: Basic MERGE (INSERT or UPDATE)
-- ============================================================================

CREATE TABLE test_target (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    value INT,
    last_updated TIMESTAMP
);

CREATE TABLE test_source (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    value INT
);

INSERT INTO test_target VALUES
    (1, 'Record 1', 100, CURRENT_TIMESTAMP),
    (2, 'Record 2', 200, CURRENT_TIMESTAMP);

INSERT INTO test_source VALUES
    (2, 'Record 2 Updated', 250),
    (3, 'Record 3 New', 300);

-- Basic MERGE (PostgreSQL 15+)
MERGE INTO test_target t
USING test_source s
ON t.id = s.id
WHEN MATCHED THEN
    UPDATE SET
        name = s.name,
        value = s.value,
        last_updated = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (id, name, value, last_updated)
    VALUES (s.id, s.name, s.value, CURRENT_TIMESTAMP);

SELECT * FROM test_target ORDER BY id;

-- ============================================================================
-- Section 2: MERGE with DELETE
-- ============================================================================

CREATE TABLE test_merge_delete_target (
    id INT PRIMARY KEY,
    status VARCHAR(50),
    value INT
);

CREATE TABLE test_merge_delete_source (
    id INT PRIMARY KEY,
    status VARCHAR(50),
    value INT
);

INSERT INTO test_merge_delete_target VALUES
    (1, 'active', 100),
    (2, 'active', 200),
    (3, 'inactive', 300);

INSERT INTO test_merge_delete_source VALUES
    (1, 'active', 150),
    (2, 'deleted', 0);

-- MERGE with conditional DELETE
MERGE INTO test_merge_delete_target t
USING test_merge_delete_source s
ON t.id = s.id
WHEN MATCHED AND s.status = 'deleted' THEN
    DELETE
WHEN MATCHED THEN
    UPDATE SET
        status = s.status,
        value = s.value
WHEN NOT MATCHED THEN
    INSERT (id, status, value)
    VALUES (s.id, s.status, s.value);

SELECT * FROM test_merge_delete_target ORDER BY id;

-- ============================================================================
-- Section 3: MERGE with Multiple Conditions
-- ============================================================================

CREATE TABLE test_inventory (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    stock INT,
    price NUMERIC(10,2)
);

CREATE TABLE test_inventory_updates (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    stock_change INT,
    new_price NUMERIC(10,2)
);

INSERT INTO test_inventory VALUES
    (1, 'Product A', 100, 50.00),
    (2, 'Product B', 50, 75.00),
    (3, 'Product C', 25, 100.00);

INSERT INTO test_inventory_updates VALUES
    (1, 'Product A', 20, 55.00),
    (2, 'Product B', -10, 75.00),
    (4, 'Product D', 30, 45.00);

-- MERGE with different actions based on conditions
MERGE INTO test_inventory t
USING test_inventory_updates s
ON t.product_id = s.product_id
WHEN MATCHED AND s.stock_change < 0 THEN
    UPDATE SET
        stock = t.stock + s.stock_change,
        price = s.new_price
WHEN MATCHED AND s.stock_change >= 0 THEN
    UPDATE SET
        stock = t.stock + s.stock_change,
        price = s.new_price
WHEN NOT MATCHED THEN
    INSERT (product_id, product_name, stock, price)
    VALUES (s.product_id, s.product_name, s.stock_change, s.new_price);

SELECT * FROM test_inventory ORDER BY product_id;

-- ============================================================================
-- Section 4: MERGE with Subquery Source
-- ============================================================================

CREATE TABLE test_employee_salaries (
    emp_id INT PRIMARY KEY,
    emp_name VARCHAR(100),
    salary NUMERIC(10,2),
    last_raise_date DATE
);

CREATE TABLE test_salary_adjustments (
    emp_id INT,
    adjustment_pct NUMERIC(5,2),
    adjustment_date DATE
);

INSERT INTO test_employee_salaries VALUES
    (1, 'Alice', 60000, '2023-01-01'),
    (2, 'Bob', 65000, '2023-01-01'),
    (3, 'Charlie', 70000, '2023-01-01');

INSERT INTO test_salary_adjustments VALUES
    (1, 10.00, '2024-01-01'),
    (2, 5.00, '2024-01-01'),
    (4, 15.00, '2024-01-01');

-- MERGE with aggregated source
MERGE INTO test_employee_salaries t
USING (
    SELECT
        emp_id,
        MAX(adjustment_pct) AS max_adjustment,
        MAX(adjustment_date) AS latest_date
    FROM test_salary_adjustments
    GROUP BY emp_id
) s
ON t.emp_id = s.emp_id
WHEN MATCHED THEN
    UPDATE SET
        salary = t.salary * (1 + s.max_adjustment / 100),
        last_raise_date = s.latest_date;

SELECT * FROM test_employee_salaries ORDER BY emp_id;

-- ============================================================================
-- Section 5: MERGE with CTE
-- ============================================================================

CREATE TABLE test_product_catalog (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    category VARCHAR(50),
    price NUMERIC(10,2)
);

CREATE TABLE test_price_changes (
    product_id INT,
    new_price NUMERIC(10,2),
    effective_date DATE
);

INSERT INTO test_product_catalog VALUES
    (1, 'Widget A', 'Widgets', 10.00),
    (2, 'Widget B', 'Widgets', 15.00),
    (3, 'Gadget A', 'Gadgets', 25.00);

INSERT INTO test_price_changes VALUES
    (1, 12.00, '2024-01-01'),
    (2, 18.00, '2024-01-01'),
    (4, 30.00, '2024-01-01');

-- MERGE with CTE
WITH latest_prices AS (
    SELECT
        product_id,
        new_price,
        ROW_NUMBER() OVER (PARTITION BY product_id ORDER BY effective_date DESC) AS rn
    FROM test_price_changes
)
MERGE INTO test_product_catalog t
USING (SELECT product_id, new_price FROM latest_prices WHERE rn = 1) s
ON t.product_id = s.product_id
WHEN MATCHED THEN
    UPDATE SET price = s.new_price;

SELECT * FROM test_product_catalog ORDER BY product_id;

-- ============================================================================
-- Section 6: MERGE RETURNING
-- ============================================================================

CREATE TABLE test_merge_returning (
    id INT PRIMARY KEY,
    data VARCHAR(100),
    updated_at TIMESTAMP
);

CREATE TABLE test_merge_returning_source (
    id INT,
    data VARCHAR(100)
);

INSERT INTO test_merge_returning VALUES
    (1, 'Original 1', CURRENT_TIMESTAMP),
    (2, 'Original 2', CURRENT_TIMESTAMP);

INSERT INTO test_merge_returning_source VALUES
    (2, 'Updated 2'),
    (3, 'New 3');

-- MERGE with RETURNING clause (PostgreSQL 17+)
-- Note: RETURNING support in MERGE may vary by version
/*
MERGE INTO test_merge_returning t
USING test_merge_returning_source s
ON t.id = s.id
WHEN MATCHED THEN
    UPDATE SET data = s.data, updated_at = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (id, data, updated_at)
    VALUES (s.id, s.data, CURRENT_TIMESTAMP)
RETURNING id, data, updated_at;
*/

-- Query result
SELECT * FROM test_merge_returning ORDER BY id;

-- ============================================================================
-- Section 7: MERGE Data Synchronization Pattern
-- ============================================================================

CREATE TABLE test_main_customers (
    customer_id INT PRIMARY KEY,
    customer_name VARCHAR(200),
    email VARCHAR(200),
    status VARCHAR(50),
    last_sync TIMESTAMP
);

CREATE TABLE test_staging_customers (
    customer_id INT PRIMARY KEY,
    customer_name VARCHAR(200),
    email VARCHAR(200),
    status VARCHAR(50),
    source_system VARCHAR(50)
);

INSERT INTO test_main_customers VALUES
    (1, 'Alice Smith', 'alice@example.com', 'active', CURRENT_TIMESTAMP - INTERVAL '1 day'),
    (2, 'Bob Jones', 'bob@example.com', 'active', CURRENT_TIMESTAMP - INTERVAL '1 day');

INSERT INTO test_staging_customers VALUES
    (1, 'Alice Smith-Johnson', 'alice.new@example.com', 'active', 'CRM'),
    (2, 'Bob Jones', 'bob@example.com', 'inactive', 'CRM'),
    (3, 'Charlie Brown', 'charlie@example.com', 'active', 'CRM');

-- Synchronize staging to main
MERGE INTO test_main_customers t
USING test_staging_customers s
ON t.customer_id = s.customer_id
WHEN MATCHED AND (
    t.customer_name <> s.customer_name OR
    t.email <> s.email OR
    t.status <> s.status
) THEN
    UPDATE SET
        customer_name = s.customer_name,
        email = s.email,
        status = s.status,
        last_sync = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (customer_id, customer_name, email, status, last_sync)
    VALUES (s.customer_id, s.customer_name, s.email, s.status, CURRENT_TIMESTAMP);

SELECT * FROM test_main_customers ORDER BY customer_id;

-- ============================================================================
-- Section 8: MERGE with Complex Join Conditions
-- ============================================================================

CREATE TABLE test_orders_master (
    order_id INT PRIMARY KEY,
    customer_id INT,
    order_date DATE,
    status VARCHAR(50),
    total NUMERIC(10,2)
);

CREATE TABLE test_orders_updates (
    order_id INT,
    customer_id INT,
    new_status VARCHAR(50),
    additional_amount NUMERIC(10,2)
);

INSERT INTO test_orders_master VALUES
    (1, 100, '2024-01-01', 'pending', 150.00),
    (2, 101, '2024-01-02', 'pending', 200.00),
    (3, 102, '2024-01-03', 'shipped', 300.00);

INSERT INTO test_orders_updates VALUES
    (1, 100, 'shipped', 10.00),
    (2, 101, 'cancelled', 0.00),
    (4, 103, 'pending', 250.00);

-- MERGE with compound join condition
MERGE INTO test_orders_master t
USING test_orders_updates s
ON t.order_id = s.order_id AND t.customer_id = s.customer_id
WHEN MATCHED THEN
    UPDATE SET
        status = s.new_status,
        total = t.total + s.additional_amount
WHEN NOT MATCHED THEN
    INSERT (order_id, customer_id, order_date, status, total)
    VALUES (s.order_id, s.customer_id, CURRENT_DATE, s.new_status, s.additional_amount);

SELECT * FROM test_orders_master ORDER BY order_id;

-- ============================================================================
-- Section 9: Alternative Pattern: INSERT ON CONFLICT (Upsert)
-- ============================================================================

CREATE TABLE test_upsert_alternative (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    value INT,
    updated_at TIMESTAMP
);

CREATE TABLE test_upsert_source (
    id INT,
    name VARCHAR(100),
    value INT
);

INSERT INTO test_upsert_alternative VALUES
    (1, 'Record 1', 100, CURRENT_TIMESTAMP),
    (2, 'Record 2', 200, CURRENT_TIMESTAMP);

INSERT INTO test_upsert_source VALUES
    (2, 'Record 2 Updated', 250),
    (3, 'Record 3 New', 300);

-- Alternative to MERGE using INSERT ON CONFLICT
INSERT INTO test_upsert_alternative (id, name, value, updated_at)
SELECT id, name, value, CURRENT_TIMESTAMP
FROM test_upsert_source
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    value = EXCLUDED.value,
    updated_at = CURRENT_TIMESTAMP;

SELECT * FROM test_upsert_alternative ORDER BY id;

-- ============================================================================
-- Section 10: Alternative Pattern: Manual UPDATE then INSERT
-- ============================================================================

CREATE TABLE test_manual_merge (
    id INT PRIMARY KEY,
    data VARCHAR(100),
    modified_at TIMESTAMP
);

CREATE TABLE test_manual_source (
    id INT,
    data VARCHAR(100)
);

INSERT INTO test_manual_merge VALUES
    (1, 'Original 1', CURRENT_TIMESTAMP),
    (2, 'Original 2', CURRENT_TIMESTAMP);

INSERT INTO test_manual_source VALUES
    (2, 'Updated 2'),
    (3, 'New 3');

-- Alternative: UPDATE first
UPDATE test_manual_merge t
SET
    data = s.data,
    modified_at = CURRENT_TIMESTAMP
FROM test_manual_source s
WHERE t.id = s.id;

-- Then INSERT new records
INSERT INTO test_manual_merge (id, data, modified_at)
SELECT s.id, s.data, CURRENT_TIMESTAMP
FROM test_manual_source s
WHERE NOT EXISTS (
    SELECT 1 FROM test_manual_merge t WHERE t.id = s.id
);

SELECT * FROM test_manual_merge ORDER BY id;

-- ============================================================================
-- Section 11: MERGE with Partitioned Tables
-- ============================================================================

CREATE TABLE test_partitioned_merge (
    id SERIAL,
    category VARCHAR(50),
    data VARCHAR(100),
    value INT,
    PRIMARY KEY (id, category)
) PARTITION BY LIST (category);

CREATE TABLE test_part_a PARTITION OF test_partitioned_merge
    FOR VALUES IN ('A');

CREATE TABLE test_part_b PARTITION OF test_partitioned_merge
    FOR VALUES IN ('B');

CREATE TABLE test_partition_source (
    id INT,
    category VARCHAR(50),
    data VARCHAR(100),
    value INT
);

INSERT INTO test_partitioned_merge (id, category, data, value) VALUES
    (1, 'A', 'Data A1', 100),
    (2, 'B', 'Data B1', 200);

INSERT INTO test_partition_source VALUES
    (1, 'A', 'Data A1 Updated', 150),
    (3, 'A', 'Data A3 New', 300);

-- MERGE on partitioned table
MERGE INTO test_partitioned_merge t
USING test_partition_source s
ON t.id = s.id AND t.category = s.category
WHEN MATCHED THEN
    UPDATE SET
        data = s.data,
        value = s.value
WHEN NOT MATCHED THEN
    INSERT (id, category, data, value)
    VALUES (s.id, s.category, s.data, s.value);

SELECT * FROM test_partitioned_merge ORDER BY id;

-- ============================================================================
-- Section 12: MERGE Performance Considerations
-- ============================================================================

CREATE TABLE test_merge_performance (
    id INT PRIMARY KEY,
    data VARCHAR(100),
    counter INT DEFAULT 0
);

CREATE TABLE test_merge_perf_source (
    id INT PRIMARY KEY,
    data VARCHAR(100)
);

-- Populate with test data
INSERT INTO test_merge_performance (id, data)
SELECT i, 'Data ' || i FROM generate_series(1, 10000) i;

INSERT INTO test_merge_perf_source (id, data)
SELECT i, 'Updated ' || i FROM generate_series(5000, 15000) i;

-- Index on join column (critical for performance)
CREATE INDEX idx_perf_source_id ON test_merge_perf_source(id);

-- MERGE with large dataset
MERGE INTO test_merge_performance t
USING test_merge_perf_source s
ON t.id = s.id
WHEN MATCHED THEN
    UPDATE SET
        data = s.data,
        counter = t.counter + 1
WHEN NOT MATCHED THEN
    INSERT (id, data)
    VALUES (s.id, s.data);

-- Verify counts
SELECT
    COUNT(*) AS total_rows,
    COUNT(*) FILTER (WHERE counter > 0) AS updated_rows,
    COUNT(*) FILTER (WHERE counter = 0) AS inserted_rows
FROM test_merge_performance;

-- ============================================================================
-- Section 13: MERGE with Transaction Isolation
-- ============================================================================

CREATE TABLE test_merge_isolation (
    id INT PRIMARY KEY,
    value INT,
    version INT DEFAULT 1
);

CREATE TABLE test_merge_iso_source (
    id INT,
    value INT
);

INSERT INTO test_merge_isolation VALUES (1, 100, 1), (2, 200, 1);
INSERT INTO test_merge_iso_source VALUES (1, 150), (3, 300);

-- MERGE in transaction with isolation level
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;

MERGE INTO test_merge_isolation t
USING test_merge_iso_source s
ON t.id = s.id
WHEN MATCHED THEN
    UPDATE SET
        value = s.value,
        version = t.version + 1
WHEN NOT MATCHED THEN
    INSERT (id, value)
    VALUES (s.id, s.value);

COMMIT;

SELECT * FROM test_merge_isolation ORDER BY id;

-- ============================================================================
-- Section 14: MERGE Error Handling
-- ============================================================================

CREATE TABLE test_merge_errors (
    id INT PRIMARY KEY,
    value INT CHECK (value > 0),
    data VARCHAR(100)
);

CREATE TABLE test_merge_err_source (
    id INT,
    value INT,
    data VARCHAR(100)
);

INSERT INTO test_merge_errors VALUES (1, 100, 'Valid');
INSERT INTO test_merge_err_source VALUES
    (1, 150, 'Updated'),
    (2, -50, 'Invalid');  -- This will violate CHECK constraint

-- MERGE with error handling
DO $$
BEGIN
    MERGE INTO test_merge_errors t
    USING test_merge_err_source s
    ON t.id = s.id
    WHEN MATCHED THEN
        UPDATE SET value = s.value, data = s.data
    WHEN NOT MATCHED THEN
        INSERT (id, value, data)
        VALUES (s.id, s.value, s.data);

    RAISE NOTICE 'MERGE completed successfully';
EXCEPTION
    WHEN check_violation THEN
        RAISE NOTICE 'MERGE failed: Check constraint violation';
    WHEN OTHERS THEN
        RAISE NOTICE 'MERGE failed: %', SQLERRM;
END $$;

SELECT * FROM test_merge_errors ORDER BY id;

-- ============================================================================
-- Section 15: MERGE Audit Trail Pattern
-- ============================================================================

CREATE TABLE test_merge_audit_target (
    id INT PRIMARY KEY,
    data VARCHAR(100),
    value INT,
    updated_at TIMESTAMP
);

CREATE TABLE test_merge_audit_source (
    id INT,
    data VARCHAR(100),
    value INT
);

CREATE TABLE test_merge_audit_log (
    log_id SERIAL PRIMARY KEY,
    operation VARCHAR(20),
    record_id INT,
    old_value INT,
    new_value INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_merge_audit_target VALUES
    (1, 'Record 1', 100, CURRENT_TIMESTAMP),
    (2, 'Record 2', 200, CURRENT_TIMESTAMP);

INSERT INTO test_merge_audit_source VALUES
    (2, 'Record 2 Updated', 250),
    (3, 'Record 3', 300);

-- MERGE with audit logging (using triggers or manual logging)
-- Manual approach using UPDATE then INSERT pattern with logging
WITH updated AS (
    UPDATE test_merge_audit_target t
    SET
        data = s.data,
        value = s.value,
        updated_at = CURRENT_TIMESTAMP
    FROM test_merge_audit_source s
    WHERE t.id = s.id
    RETURNING t.id, t.value AS old_value, s.value AS new_value
)
INSERT INTO test_merge_audit_log (operation, record_id, old_value, new_value)
SELECT 'UPDATE', id, old_value, new_value FROM updated;

INSERT INTO test_merge_audit_target (id, data, value, updated_at)
SELECT s.id, s.data, s.value, CURRENT_TIMESTAMP
FROM test_merge_audit_source s
WHERE NOT EXISTS (
    SELECT 1 FROM test_merge_audit_target t WHERE t.id = s.id
);

SELECT * FROM test_merge_audit_log;

-- ============================================================================
-- Section 16: MERGE with NULL Handling
-- ============================================================================

CREATE TABLE test_merge_nulls (
    id INT PRIMARY KEY,
    nullable_field VARCHAR(100),
    not_null_field VARCHAR(100) NOT NULL
);

CREATE TABLE test_merge_null_source (
    id INT,
    nullable_field VARCHAR(100),
    not_null_field VARCHAR(100)
);

INSERT INTO test_merge_nulls VALUES (1, 'Value 1', 'Required 1');
INSERT INTO test_merge_null_source VALUES
    (1, NULL, 'Required 1 Updated'),
    (2, NULL, 'Required 2');

-- MERGE with NULL values
MERGE INTO test_merge_nulls t
USING test_merge_null_source s
ON t.id = s.id
WHEN MATCHED THEN
    UPDATE SET
        nullable_field = s.nullable_field,
        not_null_field = s.not_null_field
WHEN NOT MATCHED THEN
    INSERT (id, nullable_field, not_null_field)
    VALUES (s.id, s.nullable_field, s.not_null_field);

SELECT * FROM test_merge_nulls ORDER BY id;

-- ============================================================================
-- Section 17: MERGE with Conditional Logic
-- ============================================================================

CREATE TABLE test_merge_conditional (
    id INT PRIMARY KEY,
    balance NUMERIC(10,2),
    status VARCHAR(50),
    last_updated TIMESTAMP
);

CREATE TABLE test_merge_cond_source (
    id INT,
    transaction_amount NUMERIC(10,2),
    transaction_type VARCHAR(20)
);

INSERT INTO test_merge_conditional VALUES
    (1, 1000.00, 'active', CURRENT_TIMESTAMP),
    (2, 500.00, 'active', CURRENT_TIMESTAMP);

INSERT INTO test_merge_cond_source VALUES
    (1, -200.00, 'debit'),
    (2, 100.00, 'credit'),
    (3, 500.00, 'credit');

-- MERGE with conditional updates based on business logic
MERGE INTO test_merge_conditional t
USING test_merge_cond_source s
ON t.id = s.id
WHEN MATCHED AND s.transaction_type = 'debit' THEN
    UPDATE SET
        balance = t.balance + s.transaction_amount,
        status = CASE WHEN (t.balance + s.transaction_amount) < 0 THEN 'overdrawn' ELSE t.status END,
        last_updated = CURRENT_TIMESTAMP
WHEN MATCHED AND s.transaction_type = 'credit' THEN
    UPDATE SET
        balance = t.balance + s.transaction_amount,
        last_updated = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (id, balance, status, last_updated)
    VALUES (s.id, s.transaction_amount, 'active', CURRENT_TIMESTAMP);

SELECT * FROM test_merge_conditional ORDER BY id;

-- ============================================================================
-- Section 18: MERGE Idempotency Pattern
-- ============================================================================

CREATE TABLE test_merge_idempotent (
    id INT PRIMARY KEY,
    data VARCHAR(100),
    checksum VARCHAR(64),
    updated_at TIMESTAMP
);

CREATE TABLE test_merge_idem_source (
    id INT,
    data VARCHAR(100),
    checksum VARCHAR(64)
);

INSERT INTO test_merge_idempotent VALUES
    (1, 'Data 1', md5('Data 1'), CURRENT_TIMESTAMP),
    (2, 'Data 2', md5('Data 2'), CURRENT_TIMESTAMP);

INSERT INTO test_merge_idem_source VALUES
    (1, 'Data 1', md5('Data 1')),  -- Same checksum, no update
    (2, 'Data 2 Updated', md5('Data 2 Updated')),  -- Different checksum, update
    (3, 'Data 3', md5('Data 3'));

-- MERGE with checksum to avoid unnecessary updates
MERGE INTO test_merge_idempotent t
USING test_merge_idem_source s
ON t.id = s.id
WHEN MATCHED AND t.checksum <> s.checksum THEN
    UPDATE SET
        data = s.data,
        checksum = s.checksum,
        updated_at = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (id, data, checksum, updated_at)
    VALUES (s.id, s.data, s.checksum, CURRENT_TIMESTAMP);

SELECT * FROM test_merge_idempotent ORDER BY id;

-- ============================================================================
-- Section 19: MERGE with Foreign Keys
-- ============================================================================

CREATE TABLE test_merge_fk_parent (
    parent_id INT PRIMARY KEY,
    parent_name VARCHAR(100)
);

CREATE TABLE test_merge_fk_child (
    child_id INT PRIMARY KEY,
    parent_id INT REFERENCES test_merge_fk_parent(parent_id),
    child_name VARCHAR(100)
);

CREATE TABLE test_merge_fk_source (
    child_id INT,
    parent_id INT,
    child_name VARCHAR(100)
);

INSERT INTO test_merge_fk_parent VALUES (1, 'Parent 1'), (2, 'Parent 2');
INSERT INTO test_merge_fk_child VALUES (1, 1, 'Child 1');
INSERT INTO test_merge_fk_source VALUES
    (1, 2, 'Child 1 Updated'),
    (2, 1, 'Child 2');

-- MERGE with foreign key constraints
MERGE INTO test_merge_fk_child t
USING test_merge_fk_source s
ON t.child_id = s.child_id
WHEN MATCHED THEN
    UPDATE SET
        parent_id = s.parent_id,
        child_name = s.child_name
WHEN NOT MATCHED THEN
    INSERT (child_id, parent_id, child_name)
    VALUES (s.child_id, s.parent_id, s.child_name);

SELECT * FROM test_merge_fk_child ORDER BY child_id;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_merge_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_merge_best_practices VALUES
    (1, 'MERGE available in PostgreSQL 15+ only'),
    (2, 'Use INSERT ON CONFLICT for older versions (upsert pattern)'),
    (3, 'Index join columns in both source and target tables'),
    (4, 'MERGE is atomic (entire operation succeeds or fails)'),
    (5, 'Use WHEN MATCHED AND condition for selective updates'),
    (6, 'MERGE can combine INSERT, UPDATE, DELETE in one statement'),
    (7, 'Source can be table, view, subquery, or CTE'),
    (8, 'MERGE useful for data synchronization patterns'),
    (9, 'Consider transaction isolation level for concurrent access'),
    (10, 'MERGE locks target table (plan for concurrency)'),
    (11, 'Use checksums or versions to avoid unnecessary updates'),
    (12, 'Test MERGE with constraints (FK, CHECK, NOT NULL)'),
    (13, 'MERGE respects all table constraints and triggers'),
    (14, 'For large datasets, consider batching MERGE operations'),
    (15, 'Monitor MERGE performance (analyze execution plans)'),
    (16, 'MERGE in partitioned tables supported'),
    (17, 'Use audit logging for tracking MERGE operations'),
    (18, 'Handle NULL values explicitly in WHEN conditions'),
    (19, 'RETURNING clause support varies by PostgreSQL version'),
    (20, 'Document MERGE logic for data flow understanding');

SELECT id, guideline FROM test_merge_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_merge_best_practices;
DROP TABLE test_merge_fk_child;
DROP TABLE test_merge_fk_source;
DROP TABLE test_merge_fk_parent;
DROP TABLE test_merge_idempotent;
DROP TABLE test_merge_idem_source;
DROP TABLE test_merge_conditional;
DROP TABLE test_merge_cond_source;
DROP TABLE test_merge_nulls;
DROP TABLE test_merge_null_source;
DROP TABLE test_merge_audit_log;
DROP TABLE test_merge_audit_source;
DROP TABLE test_merge_audit_target;
DROP TABLE test_merge_errors;
DROP TABLE test_merge_err_source;
DROP TABLE test_merge_isolation;
DROP TABLE test_merge_iso_source;
DROP TABLE test_merge_performance;
DROP TABLE test_merge_perf_source;
DROP TABLE test_part_b;
DROP TABLE test_part_a;
DROP TABLE test_partitioned_merge;
DROP TABLE test_partition_source;
DROP TABLE test_manual_merge;
DROP TABLE test_manual_source;
DROP TABLE test_upsert_alternative;
DROP TABLE test_upsert_source;
DROP TABLE test_orders_master;
DROP TABLE test_orders_updates;
DROP TABLE test_main_customers;
DROP TABLE test_staging_customers;
DROP TABLE test_merge_returning;
DROP TABLE test_merge_returning_source;
DROP TABLE test_product_catalog;
DROP TABLE test_price_changes;
DROP TABLE test_employee_salaries;
DROP TABLE test_salary_adjustments;
DROP TABLE test_inventory;
DROP TABLE test_inventory_updates;
DROP TABLE test_merge_delete_target;
DROP TABLE test_merge_delete_source;
DROP TABLE test_target;
DROP TABLE test_source;

DROP DATABASE test_merge_operations_db;

-- End of MERGE operations tests
