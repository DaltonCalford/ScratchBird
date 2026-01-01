-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - Unique Indexes
-- Description: Comprehensive unique index and constraint testing
-- ============================================================================

-- Unique indexes enforce uniqueness constraint on columns
-- Prevents duplicate values in indexed column(s)
-- Best for: primary keys, alternate keys, business constraints
-- Note: NULL values are considered distinct (multiple NULLs allowed)

-- Create test database
CREATE DATABASE test_unique_idx_db;
USE test_unique_idx_db;

-- ============================================================================
-- Section 1: Basic Unique Index
-- ============================================================================

CREATE TABLE test_unique_basic (
    id INT PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200)
);

-- Unique index on username
CREATE UNIQUE INDEX idx_username_unique ON test_unique_basic (username);

-- Unique index on email
CREATE UNIQUE INDEX idx_email_unique ON test_unique_basic (email);

INSERT INTO test_unique_basic VALUES
    (1, 'alice', 'alice@example.com'),
    (2, 'bob', 'bob@example.com'),
    (3, 'charlie', 'charlie@example.com');

-- This would fail: duplicate username
-- INSERT INTO test_unique_basic VALUES (4, 'alice', 'alice2@example.com');

-- This would fail: duplicate email
-- INSERT INTO test_unique_basic VALUES (5, 'david', 'alice@example.com');

SELECT id, username, email FROM test_unique_basic ORDER BY id;

-- ============================================================================
-- Section 2: Unique Constraint vs Unique Index
-- ============================================================================

CREATE TABLE test_unique_constraint (
    id INT PRIMARY KEY,
    ssn VARCHAR(11),
    license_number VARCHAR(20),
    passport_number VARCHAR(20)
);

-- Using UNIQUE constraint (creates unique index automatically)
ALTER TABLE test_unique_constraint ADD CONSTRAINT uq_ssn UNIQUE (ssn);

-- Explicit unique index
CREATE UNIQUE INDEX idx_license_unique ON test_unique_constraint (license_number);

INSERT INTO test_unique_constraint VALUES
    (1, '123-45-6789', 'DL123456', 'P1234567'),
    (2, '234-56-7890', 'DL234567', 'P2345678'),
    (3, '345-67-8901', 'DL345678', 'P3456789');

-- Duplicate SSN fails
-- INSERT INTO test_unique_constraint VALUES (4, '123-45-6789', 'DL999999', 'P9999999');

SELECT id, ssn, license_number FROM test_unique_constraint ORDER BY id;

-- ============================================================================
-- Section 3: Multicolumn Unique Index
-- ============================================================================

CREATE TABLE test_unique_multicolumn (
    enrollment_id SERIAL PRIMARY KEY,
    student_id INT,
    course_id INT,
    semester VARCHAR(20),
    grade VARCHAR(2)
);

-- Unique on combination: student cannot enroll in same course twice in same semester
CREATE UNIQUE INDEX idx_student_course_semester ON test_unique_multicolumn (student_id, course_id, semester);

INSERT INTO test_unique_multicolumn (student_id, course_id, semester, grade) VALUES
    (1001, 101, '2024-Spring', 'A'),
    (1001, 102, '2024-Spring', 'B'),
    (1001, 101, '2024-Fall', 'A'),  -- Same student, same course, different semester: OK
    (1002, 101, '2024-Spring', 'B'); -- Different student, same course, same semester: OK

-- This would fail: duplicate (student, course, semester)
-- INSERT INTO test_unique_multicolumn (student_id, course_id, semester, grade)
-- VALUES (1001, 101, '2024-Spring', 'A');

SELECT enrollment_id, student_id, course_id, semester, grade FROM test_unique_multicolumn
ORDER BY student_id, semester, course_id;

-- ============================================================================
-- Section 4: Unique Index with NULL Values
-- ============================================================================

CREATE TABLE test_unique_nulls (
    id SERIAL PRIMARY KEY,
    required_field VARCHAR(100) NOT NULL,
    optional_field VARCHAR(100),
    another_optional VARCHAR(100)
);

-- Unique index allows multiple NULLs
CREATE UNIQUE INDEX idx_optional_unique ON test_unique_nulls (optional_field);

INSERT INTO test_unique_nulls (required_field, optional_field, another_optional) VALUES
    ('record1', 'value1', 'data1'),
    ('record2', NULL, 'data2'),     -- NULL allowed
    ('record3', NULL, 'data3'),     -- Another NULL allowed
    ('record4', 'value2', NULL);

-- This would fail: duplicate non-NULL value
-- INSERT INTO test_unique_nulls (required_field, optional_field) VALUES ('record5', 'value1');

SELECT id, required_field, optional_field FROM test_unique_nulls ORDER BY id;

-- ============================================================================
-- Section 5: Partial Unique Index
-- ============================================================================

CREATE TABLE test_unique_partial (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200),
    is_active BOOLEAN DEFAULT TRUE
);

-- Unique email only for active users (allows duplicate emails for inactive)
CREATE UNIQUE INDEX idx_active_email_unique ON test_unique_partial (email)
WHERE is_active = TRUE;

INSERT INTO test_unique_partial (email, is_active) VALUES
    ('alice@example.com', TRUE),
    ('bob@example.com', TRUE),
    ('inactive1@example.com', FALSE),
    ('inactive1@example.com', FALSE);  -- Duplicate inactive email: OK

-- This would fail: duplicate active email
-- INSERT INTO test_unique_partial (email, is_active) VALUES ('alice@example.com', TRUE);

SELECT id, email, is_active FROM test_unique_partial ORDER BY id;

-- ============================================================================
-- Section 6: Unique Index on Expression
-- ============================================================================

CREATE TABLE test_unique_expression (
    id INT PRIMARY KEY,
    email VARCHAR(200)
);

-- Case-insensitive unique email using LOWER()
CREATE UNIQUE INDEX idx_email_lower_unique ON test_unique_expression (LOWER(email));

INSERT INTO test_unique_expression VALUES
    (1, 'Alice@Example.com'),
    (2, 'Bob@EXAMPLE.COM'),
    (3, 'charlie@example.com');

-- This would fail: LOWER('ALICE@EXAMPLE.COM') = 'alice@example.com' (duplicate)
-- INSERT INTO test_unique_expression VALUES (4, 'ALICE@EXAMPLE.COM');

SELECT id, email FROM test_unique_expression ORDER BY id;

-- ============================================================================
-- Section 7: Unique Index for Business Rules
-- ============================================================================

CREATE TABLE test_unique_business_rules (
    order_id SERIAL PRIMARY KEY,
    order_number VARCHAR(50),
    invoice_number VARCHAR(50),
    purchase_order_number VARCHAR(50),
    fiscal_year INT
);

-- Unique order number
CREATE UNIQUE INDEX idx_order_number_unique ON test_unique_business_rules (order_number);

-- Unique invoice number per fiscal year
CREATE UNIQUE INDEX idx_invoice_year_unique ON test_unique_business_rules (invoice_number, fiscal_year);

INSERT INTO test_unique_business_rules (order_number, invoice_number, purchase_order_number, fiscal_year) VALUES
    ('ORD-2024-0001', 'INV-001', 'PO-12345', 2024),
    ('ORD-2024-0002', 'INV-002', 'PO-12346', 2024),
    ('ORD-2025-0001', 'INV-001', 'PO-12347', 2025);  -- Same invoice number, different year: OK

-- This would fail: duplicate order number
-- INSERT INTO test_unique_business_rules VALUES (4, 'ORD-2024-0001', 'INV-999', 'PO-99999', 2024);

-- This would fail: duplicate invoice in same year
-- INSERT INTO test_unique_business_rules VALUES (5, 'ORD-2024-9999', 'INV-001', 'PO-99998', 2024);

SELECT order_id, order_number, invoice_number, fiscal_year FROM test_unique_business_rules
ORDER BY fiscal_year, order_number;

-- ============================================================================
-- Section 8: Unique Index for Natural Keys
-- ============================================================================

CREATE TABLE test_unique_natural_keys (
    id SERIAL PRIMARY KEY,
    country_code VARCHAR(2),
    state_code VARCHAR(3),
    city_name VARCHAR(100),
    zip_code VARCHAR(10),
    population INT
);

-- Natural key: country, state, city
CREATE UNIQUE INDEX idx_location_natural_key ON test_unique_natural_keys (country_code, state_code, city_name);

INSERT INTO test_unique_natural_keys (country_code, state_code, city_name, zip_code, population) VALUES
    ('US', 'CA', 'Los Angeles', '90001', 3900000),
    ('US', 'CA', 'San Francisco', '94102', 873965),
    ('US', 'NY', 'New York', '10001', 8336817),
    ('CA', 'ON', 'Toronto', 'M5H', 2731571);

-- This would fail: duplicate location
-- INSERT INTO test_unique_natural_keys (country_code, state_code, city_name, population)
-- VALUES ('US', 'CA', 'Los Angeles', 4000000);

SELECT id, country_code, state_code, city_name FROM test_unique_natural_keys
ORDER BY country_code, state_code, city_name;

-- ============================================================================
-- Section 9: Unique Index with Include Columns (Covering)
-- ============================================================================

CREATE TABLE test_unique_include (
    user_id INT PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200),
    full_name VARCHAR(200),
    created_at TIMESTAMP
);

-- Unique username, include additional columns for index-only scans
CREATE UNIQUE INDEX idx_username_covering ON test_unique_include (username) INCLUDE (email, full_name);

INSERT INTO test_unique_include
SELECT i,
       'user' || i,
       'user' || i || '@example.com',
       'User ' || i || ' Name',
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' days')::INTERVAL
FROM generate_series(1, 1000) i;

-- Index-only scan: username (indexed) + email, full_name (included)
SELECT username, email, full_name FROM test_unique_include
WHERE username = 'user500';

-- ============================================================================
-- Section 10: Deferrable Unique Constraints
-- ============================================================================

CREATE TABLE test_unique_deferrable (
    id INT PRIMARY KEY,
    position INT,
    name VARCHAR(100)
);

-- Deferrable unique constraint (checked at commit, not immediately)
ALTER TABLE test_unique_deferrable
ADD CONSTRAINT uq_position_deferrable UNIQUE (position) DEFERRABLE INITIALLY DEFERRED;

INSERT INTO test_unique_deferrable VALUES (1, 1, 'First'), (2, 2, 'Second'), (3, 3, 'Third');

-- Swap positions (would fail without deferrable)
BEGIN;
UPDATE test_unique_deferrable SET position = 99 WHERE position = 1;
UPDATE test_unique_deferrable SET position = 1 WHERE position = 2;
UPDATE test_unique_deferrable SET position = 2 WHERE position = 99;
COMMIT;

SELECT id, position, name FROM test_unique_deferrable ORDER BY position;

-- ============================================================================
-- Section 11: Unique Index on JSONB Fields
-- ============================================================================

CREATE TABLE test_unique_jsonb (
    id SERIAL PRIMARY KEY,
    user_data JSONB
);

-- Unique on email extracted from JSONB
CREATE UNIQUE INDEX idx_jsonb_email_unique ON test_unique_jsonb ((user_data->>'email'));

INSERT INTO test_unique_jsonb (user_data) VALUES
    ('{"email": "alice@example.com", "name": "Alice"}'::JSONB),
    ('{"email": "bob@example.com", "name": "Bob"}'::JSONB),
    ('{"email": "charlie@example.com", "name": "Charlie"}'::JSONB);

-- This would fail: duplicate email in JSONB
-- INSERT INTO test_unique_jsonb (user_data) VALUES
-- ('{"email": "alice@example.com", "name": "Alice Duplicate"}'::JSONB);

SELECT id, user_data->>'email' AS email, user_data->>'name' AS name FROM test_unique_jsonb;

-- ============================================================================
-- Section 12: Unique Index for Preventing Overlaps
-- ============================================================================

CREATE TABLE test_unique_no_overlap (
    reservation_id SERIAL PRIMARY KEY,
    room_number VARCHAR(20),
    start_date DATE,
    end_date DATE,
    guest_name VARCHAR(100)
);

-- Note: For true overlap prevention, use EXCLUDE constraint with ranges
-- This example shows single-day uniqueness
CREATE UNIQUE INDEX idx_room_date_unique ON test_unique_no_overlap (room_number, start_date);

INSERT INTO test_unique_no_overlap (room_number, start_date, end_date, guest_name) VALUES
    ('Room 101', '2024-01-15', '2024-01-17', 'Alice'),
    ('Room 101', '2024-01-18', '2024-01-20', 'Bob'),
    ('Room 102', '2024-01-15', '2024-01-16', 'Charlie');

-- This would fail: same room, same start date
-- INSERT INTO test_unique_no_overlap (room_number, start_date, end_date, guest_name)
-- VALUES ('Room 101', '2024-01-15', '2024-01-19', 'David');

SELECT reservation_id, room_number, start_date, end_date, guest_name FROM test_unique_no_overlap
ORDER BY room_number, start_date;

-- ============================================================================
-- Section 13: Unique Index Performance
-- ============================================================================

CREATE TABLE test_unique_performance (
    id INT PRIMARY KEY,
    unique_code VARCHAR(50),
    data TEXT
);

CREATE UNIQUE INDEX idx_code_unique_perf ON test_unique_performance (unique_code);

-- Insert unique codes
INSERT INTO test_unique_performance
SELECT i, 'CODE-' || LPAD(i::TEXT, 10, '0'), 'Data ' || i
FROM generate_series(1, 100000) i;

-- Fast lookup by unique code
SELECT id, unique_code, data FROM test_unique_performance
WHERE unique_code = 'CODE-0000050000';

-- ============================================================================
-- Section 14: Multiple Unique Indexes
-- ============================================================================

CREATE TABLE test_unique_multiple (
    product_id SERIAL PRIMARY KEY,
    sku VARCHAR(50),
    upc VARCHAR(20),
    internal_code VARCHAR(20),
    product_name VARCHAR(200)
);

-- Multiple unique indexes on different columns
CREATE UNIQUE INDEX idx_sku_unique ON test_unique_multiple (sku);
CREATE UNIQUE INDEX idx_upc_unique ON test_unique_multiple (upc);
CREATE UNIQUE INDEX idx_internal_code_unique ON test_unique_multiple (internal_code);

INSERT INTO test_unique_multiple (sku, upc, internal_code, product_name) VALUES
    ('SKU-001', '123456789012', 'INT-001', 'Product A'),
    ('SKU-002', '123456789013', 'INT-002', 'Product B'),
    ('SKU-003', '123456789014', 'INT-003', 'Product C');

-- Each unique field enforced independently
SELECT product_id, sku, upc, internal_code, product_name FROM test_unique_multiple
ORDER BY product_id;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_unique_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_unique_best_practices VALUES
    (1, 'Unique indexes enforce data integrity constraints'),
    (2, 'Use for natural keys and alternate keys'),
    (3, 'Multiple NULL values are allowed (NULLs are distinct)'),
    (4, 'UNIQUE constraint creates a unique index automatically'),
    (5, 'Multicolumn unique indexes check combination of values'),
    (6, 'Partial unique indexes allow conditional uniqueness'),
    (7, 'Expression unique indexes enable case-insensitive uniqueness'),
    (8, 'Use INCLUDE for covering unique indexes (index-only scans)'),
    (9, 'Deferrable constraints allow temporary violations within transaction'),
    (10, 'Unique indexes add overhead to INSERT/UPDATE operations'),
    (11, 'Consider partial unique for soft-delete patterns'),
    (12, 'Use for business rules (order numbers, invoice numbers)'),
    (13, 'Unique indexes provide fast lookups in addition to constraint'),
    (14, 'Can combine with WHERE clause for complex business rules'),
    (15, 'Document business meaning of unique constraints');

SELECT id, guideline FROM test_unique_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_unique_best_practices;
DROP TABLE test_unique_multiple;
DROP TABLE test_unique_performance;
DROP TABLE test_unique_no_overlap;
DROP TABLE test_unique_jsonb;
DROP TABLE test_unique_deferrable;
DROP TABLE test_unique_include;
DROP TABLE test_unique_natural_keys;
DROP TABLE test_unique_business_rules;
DROP TABLE test_unique_expression;
DROP TABLE test_unique_partial;
DROP TABLE test_unique_nulls;
DROP TABLE test_unique_multicolumn;
DROP TABLE test_unique_constraint;
DROP TABLE test_unique_basic;

DROP DATABASE test_unique_idx_db;

-- End of unique index tests
