-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Constraints - EXCLUSION and Advanced Features
-- Description: Comprehensive EXCLUSION constraint and advanced features
-- ============================================================================

-- EXCLUSION constraints: Generalized uniqueness using operators
-- Prevents rows from overlapping based on custom conditions
-- Requires btree_gist or other index access methods
-- Useful for temporal data, ranges, scheduling

-- Create test database
CREATE DATABASE test_exclusion_advanced_db;
USE test_exclusion_advanced_db;

-- Enable required extensions
CREATE EXTENSION IF NOT EXISTS btree_gist;

-- ============================================================================
-- Section 1: Basic EXCLUSION Constraint
-- ============================================================================

CREATE TABLE test_room_bookings (
    booking_id SERIAL PRIMARY KEY,
    room_number INT,
    booking_date DATE,
    EXCLUDE USING gist (room_number WITH =, booking_date WITH =)
);

INSERT INTO test_room_bookings (room_number, booking_date) VALUES
    (101, '2024-01-15'),
    (101, '2024-01-16'),  -- OK: different date
    (102, '2024-01-15');  -- OK: different room

SELECT booking_id, room_number, booking_date FROM test_room_bookings ORDER BY booking_id;

-- Invalid: Same room, same date (would fail)
-- INSERT INTO test_room_bookings (room_number, booking_date) VALUES (101, '2024-01-15');

-- ============================================================================
-- Section 2: EXCLUSION with Range Types
-- ============================================================================

CREATE TABLE test_meeting_rooms (
    meeting_id SERIAL PRIMARY KEY,
    room_name VARCHAR(100),
    time_range TSRANGE,
    EXCLUDE USING gist (room_name WITH =, time_range WITH &&)
);

INSERT INTO test_meeting_rooms (room_name, time_range) VALUES
    ('Conference A', '[2024-01-15 09:00, 2024-01-15 10:00)'),
    ('Conference A', '[2024-01-15 10:00, 2024-01-15 11:00)'),  -- OK: no overlap
    ('Conference B', '[2024-01-15 09:00, 2024-01-15 10:00)');  -- OK: different room

SELECT meeting_id, room_name, time_range FROM test_meeting_rooms ORDER BY meeting_id;

-- Invalid: Overlapping time range for same room (would fail)
-- INSERT INTO test_meeting_rooms (room_name, time_range)
-- VALUES ('Conference A', '[2024-01-15 09:30, 2024-01-15 10:30)');

-- ============================================================================
-- Section 3: EXCLUSION with Multiple Columns
-- ============================================================================

CREATE TABLE test_employee_schedule (
    schedule_id SERIAL PRIMARY KEY,
    employee_id INT,
    dept_id INT,
    work_period TSRANGE,
    EXCLUDE USING gist (
        employee_id WITH =,
        work_period WITH &&
    )
);

INSERT INTO test_employee_schedule (employee_id, dept_id, work_period) VALUES
    (1, 10, '[2024-01-15 08:00, 2024-01-15 17:00)'),
    (1, 10, '[2024-01-16 08:00, 2024-01-16 17:00)'),  -- OK: different day
    (2, 10, '[2024-01-15 08:00, 2024-01-15 17:00)');  -- OK: different employee

SELECT schedule_id, employee_id, dept_id, work_period FROM test_employee_schedule ORDER BY schedule_id;

-- ============================================================================
-- Section 4: EXCLUSION with WHERE Clause
-- ============================================================================

CREATE TABLE test_reservations (
    reservation_id SERIAL PRIMARY KEY,
    resource_id INT,
    reserved_date DATE,
    status VARCHAR(20),
    EXCLUDE USING gist (
        resource_id WITH =,
        reserved_date WITH =
    ) WHERE (status != 'cancelled')
);

INSERT INTO test_reservations (resource_id, reserved_date, status) VALUES
    (1, '2024-01-15', 'confirmed'),
    (1, '2024-01-15', 'cancelled'),  -- OK: cancelled doesn't count
    (1, '2024-01-15', 'cancelled'),  -- OK: multiple cancelled OK
    (2, '2024-01-15', 'confirmed');  -- OK: different resource

SELECT reservation_id, resource_id, reserved_date, status FROM test_reservations ORDER BY reservation_id;

-- ============================================================================
-- Section 5: Named EXCLUSION Constraint
-- ============================================================================

CREATE TABLE test_named_exclusion (
    id SERIAL PRIMARY KEY,
    project_id INT,
    task_period DATERANGE,
    CONSTRAINT excl_project_overlap
        EXCLUDE USING gist (project_id WITH =, task_period WITH &&)
);

INSERT INTO test_named_exclusion (project_id, task_period) VALUES
    (1, '[2024-01-01, 2024-01-31]'),
    (1, '[2024-02-01, 2024-02-28]'),  -- OK: no overlap
    (2, '[2024-01-15, 2024-01-20]');  -- OK: different project

-- Query constraint
SELECT conname, pg_get_constraintdef(oid) AS definition
FROM pg_constraint
WHERE conname = 'excl_project_overlap';

-- ============================================================================
-- Section 6: EXCLUSION vs UNIQUE
-- ============================================================================

-- UNIQUE: Prevents exact duplicates
CREATE TABLE test_unique_comparison (
    id SERIAL PRIMARY KEY,
    code INT,
    UNIQUE (code)
);

-- EXCLUSION: Prevents based on operator (can be more flexible)
CREATE TABLE test_exclusion_comparison (
    id SERIAL PRIMARY KEY,
    code INT,
    EXCLUDE USING gist (code WITH =)  -- Similar to UNIQUE for =
);

INSERT INTO test_unique_comparison (code) VALUES (1), (2), (3);
INSERT INTO test_exclusion_comparison (code) VALUES (1), (2), (3);

-- Both prevent duplicates
-- INSERT INTO test_unique_comparison (code) VALUES (1);  -- Fails
-- INSERT INTO test_exclusion_comparison (code) VALUES (1);  -- Fails

-- ============================================================================
-- Section 7: EXCLUSION for Non-Overlapping Ranges
-- ============================================================================

CREATE TABLE test_ip_allocations (
    allocation_id SERIAL PRIMARY KEY,
    network_name VARCHAR(100),
    ip_range INET4RANGE,
    EXCLUDE USING gist (ip_range WITH &&)
);

-- Note: INET4RANGE might not be available, using numrange as example
CREATE TABLE test_number_ranges (
    range_id SERIAL PRIMARY KEY,
    range_name VARCHAR(100),
    num_range INT4RANGE,
    EXCLUDE USING gist (num_range WITH &&)
);

INSERT INTO test_number_ranges (range_name, num_range) VALUES
    ('Range A', '[1, 100]'),
    ('Range B', '[101, 200]'),  -- OK: no overlap
    ('Range C', '[201, 300]');  -- OK: no overlap

SELECT range_id, range_name, num_range FROM test_number_ranges ORDER BY range_id;

-- Invalid: Overlapping range (would fail)
-- INSERT INTO test_number_ranges (range_name, num_range) VALUES ('Range D', '[50, 150]');

-- ============================================================================
-- Section 8: Temporal Validity with EXCLUSION
-- ============================================================================

CREATE TABLE test_product_prices (
    price_id SERIAL PRIMARY KEY,
    product_id INT,
    price NUMERIC(10,2),
    valid_period DATERANGE,
    EXCLUDE USING gist (
        product_id WITH =,
        valid_period WITH &&
    )
);

INSERT INTO test_product_prices (product_id, price, valid_period) VALUES
    (1, 29.99, '[2024-01-01, 2024-03-31]'),
    (1, 24.99, '[2024-04-01, 2024-06-30]'),  -- OK: different period
    (2, 49.99, '[2024-01-01, 2024-12-31]');  -- OK: different product

SELECT price_id, product_id, price, valid_period FROM test_product_prices ORDER BY price_id;

-- Query current price for a product
SELECT price
FROM test_product_prices
WHERE product_id = 1
  AND valid_period @> CURRENT_DATE;

-- ============================================================================
-- Section 9: Constraint Combination Examples
-- ============================================================================

CREATE TABLE test_comprehensive_constraints (
    id SERIAL PRIMARY KEY,  -- PK
    code VARCHAR(50) NOT NULL UNIQUE,  -- NOT NULL + UNIQUE
    category VARCHAR(50) NOT NULL CHECK (category IN ('A', 'B', 'C')),  -- NOT NULL + CHECK
    priority INT NOT NULL DEFAULT 5 CHECK (priority BETWEEN 1 AND 10),  -- NOT NULL + DEFAULT + CHECK
    parent_id INT REFERENCES test_comprehensive_constraints(id),  -- FK (self-reference)
    start_date DATE NOT NULL,
    end_date DATE NOT NULL,
    CONSTRAINT chk_date_order CHECK (end_date >= start_date),  -- Multi-column CHECK
    EXCLUDE USING gist (code WITH =, daterange(start_date, end_date, '[]') WITH &&)  -- EXCLUSION
);

INSERT INTO test_comprehensive_constraints (code, category, priority, start_date, end_date) VALUES
    ('CODE1', 'A', 8, '2024-01-01', '2024-01-31'),
    ('CODE2', 'B', 5, '2024-01-01', '2024-01-15'),
    ('CODE1', 'A', 8, '2024-02-01', '2024-02-28');  -- OK: different date range for same code

SELECT id, code, category, priority, start_date, end_date FROM test_comprehensive_constraints ORDER BY id;

-- ============================================================================
-- Section 10: DEFERRABLE Constraints
-- ============================================================================

CREATE TABLE test_deferred_parent (
    id INT PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE test_deferred_child (
    id INT PRIMARY KEY,
    parent_id INT,
    CONSTRAINT fk_deferred_parent FOREIGN KEY (parent_id)
        REFERENCES test_deferred_parent(id)
        DEFERRABLE INITIALLY DEFERRED
);

BEGIN;
-- Can insert child before parent when deferred
INSERT INTO test_deferred_child VALUES (1, 100);
INSERT INTO test_deferred_parent VALUES (100, 'Parent');
-- Constraint checked at COMMIT
COMMIT;

SELECT c.id, c.parent_id, p.name
FROM test_deferred_child c
JOIN test_deferred_parent p ON c.parent_id = p.id;

-- ============================================================================
-- Section 11: IMMEDIATE vs DEFERRED
-- ============================================================================

CREATE TABLE test_immediate_check (
    id INT PRIMARY KEY,
    value INT CHECK (value > 0) NOT DEFERRABLE  -- Checked immediately
);

CREATE TABLE test_deferred_check (
    id INT PRIMARY KEY,
    value INT,
    CONSTRAINT chk_value CHECK (value > 0) DEFERRABLE INITIALLY DEFERRED
);

-- Immediate: Fails on INSERT
BEGIN;
-- INSERT INTO test_immediate_check VALUES (1, -5);  -- Fails immediately
ROLLBACK;

-- Deferred: Checked at COMMIT
BEGIN;
SET CONSTRAINTS ALL DEFERRED;
INSERT INTO test_deferred_check VALUES (1, -5);  -- Allowed during transaction
-- UPDATE test_deferred_check SET value = 5 WHERE id = 1;  -- Fix before commit
-- COMMIT;  -- Would fail if not fixed
ROLLBACK;

-- ============================================================================
-- Section 12: Constraint Validation Modes
-- ============================================================================

CREATE TABLE test_validation_modes (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200),
    status VARCHAR(20)
);

-- Add constraint without validating existing data
ALTER TABLE test_validation_modes
    ADD CONSTRAINT chk_status_valid
    CHECK (status IN ('active', 'inactive', 'pending'))
    NOT VALID;

-- Insert new data - constraint is enforced
INSERT INTO test_validation_modes (email, status) VALUES ('user@test.com', 'active');

-- Invalid new insert (would fail)
-- INSERT INTO test_validation_modes (email, status) VALUES ('user2@test.com', 'invalid');

-- Validate existing data
ALTER TABLE test_validation_modes VALIDATE CONSTRAINT chk_status_valid;

-- ============================================================================
-- Section 13: Constraint Inheritance
-- ============================================================================

CREATE TABLE test_parent_table (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50) NOT NULL CHECK (code ~ '^[A-Z]{3}$')
);

-- Child inherits constraints
CREATE TABLE test_child_table (
    extra_field VARCHAR(100)
) INHERITS (test_parent_table);

-- Inherited CHECK constraint applies
INSERT INTO test_child_table (code, extra_field) VALUES ('ABC', 'Extra');

-- Would fail CHECK constraint
-- INSERT INTO test_child_table (code, extra_field) VALUES ('abc', 'Extra');

SELECT id, code, extra_field FROM test_child_table;

-- ============================================================================
-- Section 14: Partial Constraints with Indexes
-- ============================================================================

CREATE TABLE test_partial_constraints (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200),
    status VARCHAR(20),
    deleted_at TIMESTAMP
);

-- Unique email for non-deleted records only
CREATE UNIQUE INDEX uq_email_not_deleted
ON test_partial_constraints (email)
WHERE deleted_at IS NULL;

INSERT INTO test_partial_constraints (email, status) VALUES
    ('user@test.com', 'active'),
    ('admin@test.com', 'active');

-- Soft delete
UPDATE test_partial_constraints SET deleted_at = CURRENT_TIMESTAMP WHERE email = 'user@test.com';

-- Now can reuse the email
INSERT INTO test_partial_constraints (email, status) VALUES ('user@test.com', 'active');

SELECT id, email, status, deleted_at FROM test_partial_constraints ORDER BY id;

-- ============================================================================
-- Section 15: Constraint Error Handling
-- ============================================================================

CREATE TABLE test_error_handling (
    id SERIAL PRIMARY KEY,
    age INT CHECK (age >= 18),
    email VARCHAR(200) UNIQUE NOT NULL,
    category VARCHAR(20) CHECK (category IN ('A', 'B', 'C'))
);

-- Handle various constraint violations
DO $$
DECLARE
    error_count INT := 0;
BEGIN
    -- Age check violation
    BEGIN
        INSERT INTO test_error_handling (age, email, category) VALUES (15, 'test1@example.com', 'A');
    EXCEPTION
        WHEN check_violation THEN
            error_count := error_count + 1;
            RAISE NOTICE 'Caught check violation: %', SQLERRM;
    END;

    -- Unique violation
    BEGIN
        INSERT INTO test_error_handling (age, email, category) VALUES (25, 'test2@example.com', 'A');
        INSERT INTO test_error_handling (age, email, category) VALUES (30, 'test2@example.com', 'B');
    EXCEPTION
        WHEN unique_violation THEN
            error_count := error_count + 1;
            RAISE NOTICE 'Caught unique violation: %', SQLERRM;
    END;

    -- NOT NULL violation
    BEGIN
        INSERT INTO test_error_handling (age, email, category) VALUES (25, NULL, 'A');
    EXCEPTION
        WHEN not_null_violation THEN
            error_count := error_count + 1;
            RAISE NOTICE 'Caught NOT NULL violation: %', SQLERRM;
    END;

    RAISE NOTICE 'Total errors caught: %', error_count;
END $$;

-- ============================================================================
-- Section 16: System Catalog Queries for Constraints
-- ============================================================================

-- Query all constraints on a table
SELECT
    conname AS constraint_name,
    contype AS constraint_type,
    CASE contype
        WHEN 'c' THEN 'CHECK'
        WHEN 'f' THEN 'FOREIGN KEY'
        WHEN 'p' THEN 'PRIMARY KEY'
        WHEN 'u' THEN 'UNIQUE'
        WHEN 'x' THEN 'EXCLUSION'
    END AS type_desc,
    pg_get_constraintdef(oid) AS definition
FROM pg_constraint
WHERE conrelid = 'test_comprehensive_constraints'::regclass
ORDER BY contype, conname;

-- ============================================================================
-- Section 17: Performance Impact of Constraints
-- ============================================================================

CREATE TABLE test_perf_no_constraints (
    id INT,
    email VARCHAR(200),
    value INT
);

CREATE TABLE test_perf_with_constraints (
    id INT PRIMARY KEY,
    email VARCHAR(200) UNIQUE NOT NULL,
    value INT CHECK (value > 0)
);

-- Insert performance comparison
INSERT INTO test_perf_no_constraints
SELECT i, 'user' || i || '@test.com', i
FROM generate_series(1, 1000) i;

INSERT INTO test_perf_with_constraints
SELECT i, 'user' || i || '@test.com', i
FROM generate_series(1, 1000) i;

-- Constraints add overhead but provide data integrity

-- ============================================================================
-- Section 18: Constraint Maintenance
-- ============================================================================

CREATE TABLE test_constraint_maintenance (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50)
);

-- Add constraint
ALTER TABLE test_constraint_maintenance
    ADD CONSTRAINT uq_code UNIQUE (code);

-- Rename constraint
ALTER TABLE test_constraint_maintenance
    RENAME CONSTRAINT uq_code TO uq_code_new;

-- Query renamed constraint
SELECT conname FROM pg_constraint
WHERE conrelid = 'test_constraint_maintenance'::regclass AND contype = 'u';

-- Drop constraint
ALTER TABLE test_constraint_maintenance
    DROP CONSTRAINT uq_code_new;

-- ============================================================================
-- Section 19: Constraint Dependencies and Cascades
-- ============================================================================

CREATE TABLE test_cascade_parent (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL
);

CREATE TABLE test_cascade_child (
    id SERIAL PRIMARY KEY,
    parent_id INT NOT NULL,
    FOREIGN KEY (parent_id) REFERENCES test_cascade_parent(id) ON DELETE CASCADE
);

CREATE TABLE test_cascade_grandchild (
    id SERIAL PRIMARY KEY,
    child_id INT NOT NULL,
    FOREIGN KEY (child_id) REFERENCES test_cascade_child(id) ON DELETE CASCADE
);

INSERT INTO test_cascade_parent (id, name) VALUES (1, 'Parent');
INSERT INTO test_cascade_child (id, parent_id) VALUES (1, 1);
INSERT INTO test_cascade_grandchild (id, child_id) VALUES (1, 1);

-- Delete parent cascades to child and grandchild
DELETE FROM test_cascade_parent WHERE id = 1;

SELECT COUNT(*) FROM test_cascade_child;  -- Should be 0
SELECT COUNT(*) FROM test_cascade_grandchild;  -- Should be 0

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_exclusion_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_exclusion_best_practices VALUES
    (1, 'Use EXCLUSION for preventing overlapping ranges'),
    (2, 'EXCLUSION requires GiST or SP-GiST index method'),
    (3, 'Enable btree_gist extension for basic type operators'),
    (4, 'EXCLUSION with && prevents overlapping periods'),
    (5, 'Combine EXCLUSION with WHERE for conditional constraints'),
    (6, 'Use DEFERRABLE for circular dependencies'),
    (7, 'NOT VALID allows adding constraints without immediate validation'),
    (8, 'Validate constraints with ALTER TABLE VALIDATE CONSTRAINT'),
    (9, 'Name all constraints explicitly for maintenance'),
    (10, 'Query pg_constraint for constraint metadata'),
    (11, 'Test constraint combinations thoroughly'),
    (12, 'Consider performance impact of constraint checks'),
    (13, 'Use partial indexes for unique non-NULL values'),
    (14, 'Handle constraint violations gracefully in application'),
    (15, 'Document business rules behind all constraints');

SELECT id, guideline FROM test_exclusion_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_exclusion_best_practices;
DROP TABLE test_cascade_grandchild;
DROP TABLE test_cascade_child;
DROP TABLE test_cascade_parent;
DROP TABLE test_constraint_maintenance;
DROP TABLE test_perf_with_constraints;
DROP TABLE test_perf_no_constraints;
DROP TABLE test_error_handling;
DROP TABLE test_partial_constraints;
DROP TABLE test_child_table;
DROP TABLE test_parent_table;
DROP TABLE test_validation_modes;
DROP TABLE test_deferred_check;
DROP TABLE test_immediate_check;
DROP TABLE test_deferred_child;
DROP TABLE test_deferred_parent;
DROP TABLE test_comprehensive_constraints;
DROP TABLE test_product_prices;
DROP TABLE test_number_ranges;
DROP TABLE test_ip_allocations;
DROP TABLE test_exclusion_comparison;
DROP TABLE test_unique_comparison;
DROP TABLE test_named_exclusion;
DROP TABLE test_reservations;
DROP TABLE test_employee_schedule;
DROP TABLE test_meeting_rooms;
DROP TABLE test_room_bookings;

DROP EXTENSION IF EXISTS btree_gist;

DROP DATABASE test_exclusion_advanced_db;

-- End of EXCLUSION and advanced constraint tests
