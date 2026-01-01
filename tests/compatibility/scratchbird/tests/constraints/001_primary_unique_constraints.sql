-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Constraints - PRIMARY KEY and UNIQUE
-- Description: Comprehensive PRIMARY KEY and UNIQUE constraint testing
-- ============================================================================

-- PRIMARY KEY: Uniquely identifies each row, NOT NULL, creates unique index
-- UNIQUE: Ensures column values are unique, allows NULLs, creates unique index
-- Both can be single-column or multi-column (composite)

-- Create test database
CREATE DATABASE test_pk_unique_constraints_db;
USE test_pk_unique_constraints_db;

-- ============================================================================
-- Section 1: Basic PRIMARY KEY
-- ============================================================================

CREATE TABLE test_basic_pk (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200)
);

INSERT INTO test_basic_pk (name) VALUES ('Item 1'), ('Item 2'), ('Item 3');

-- Valid inserts
SELECT id, name FROM test_basic_pk ORDER BY id;

-- Invalid: duplicate primary key (would fail)
-- INSERT INTO test_basic_pk (id, name) VALUES (1, 'Duplicate');

-- Invalid: NULL primary key (would fail)
-- INSERT INTO test_basic_pk (id, name) VALUES (NULL, 'Null ID');

-- ============================================================================
-- Section 2: Named PRIMARY KEY Constraint
-- ============================================================================

CREATE TABLE test_named_pk (
    employee_id INT,
    name VARCHAR(200),
    CONSTRAINT pk_employee PRIMARY KEY (employee_id)
);

INSERT INTO test_named_pk (employee_id, name) VALUES
    (1, 'Alice'),
    (2, 'Bob'),
    (3, 'Charlie');

SELECT employee_id, name FROM test_named_pk ORDER BY employee_id;

-- Query constraint information
SELECT conname, contype, conrelid::regclass
FROM pg_constraint
WHERE conname = 'pk_employee';

-- ============================================================================
-- Section 3: Composite PRIMARY KEY
-- ============================================================================

CREATE TABLE test_composite_pk (
    order_id INT,
    line_number INT,
    product VARCHAR(200),
    quantity INT,
    PRIMARY KEY (order_id, line_number)
);

INSERT INTO test_composite_pk (order_id, line_number, product, quantity) VALUES
    (1, 1, 'Widget A', 5),
    (1, 2, 'Widget B', 3),
    (2, 1, 'Gadget A', 2),
    (2, 2, 'Gadget B', 4);

SELECT order_id, line_number, product, quantity FROM test_composite_pk ORDER BY order_id, line_number;

-- Valid: Same order_id with different line_number
INSERT INTO test_composite_pk (order_id, line_number, product, quantity) VALUES (1, 3, 'Widget C', 1);

-- Invalid: Duplicate composite key (would fail)
-- INSERT INTO test_composite_pk (order_id, line_number, product, quantity) VALUES (1, 1, 'Duplicate', 10);

-- ============================================================================
-- Section 4: Basic UNIQUE Constraint
-- ============================================================================

CREATE TABLE test_basic_unique (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100) UNIQUE,
    email VARCHAR(200) UNIQUE
);

INSERT INTO test_basic_unique (username, email) VALUES
    ('alice', 'alice@example.com'),
    ('bob', 'bob@example.com'),
    ('charlie', 'charlie@example.com');

SELECT user_id, username, email FROM test_basic_unique ORDER BY user_id;

-- Invalid: Duplicate username (would fail)
-- INSERT INTO test_basic_unique (username, email) VALUES ('alice', 'alice2@example.com');

-- Valid: NULL username (UNIQUE allows NULLs)
INSERT INTO test_basic_unique (username, email) VALUES (NULL, 'nulluser@example.com');
INSERT INTO test_basic_unique (username, email) VALUES (NULL, 'nulluser2@example.com');

SELECT user_id, username, email FROM test_basic_unique WHERE username IS NULL;

-- ============================================================================
-- Section 5: Named UNIQUE Constraint
-- ============================================================================

CREATE TABLE test_named_unique (
    product_id SERIAL PRIMARY KEY,
    sku VARCHAR(50),
    barcode VARCHAR(100),
    CONSTRAINT uq_product_sku UNIQUE (sku),
    CONSTRAINT uq_product_barcode UNIQUE (barcode)
);

INSERT INTO test_named_unique (sku, barcode) VALUES
    ('SKU-001', 'BAR-001'),
    ('SKU-002', 'BAR-002'),
    ('SKU-003', 'BAR-003');

SELECT product_id, sku, barcode FROM test_named_unique ORDER BY product_id;

-- Query constraint names
SELECT conname, contype
FROM pg_constraint
WHERE conrelid = 'test_named_unique'::regclass
  AND contype = 'u'
ORDER BY conname;

-- ============================================================================
-- Section 6: Composite UNIQUE Constraint
-- ============================================================================

CREATE TABLE test_composite_unique (
    id SERIAL PRIMARY KEY,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    birth_date DATE,
    CONSTRAINT uq_person UNIQUE (first_name, last_name, birth_date)
);

INSERT INTO test_composite_unique (first_name, last_name, birth_date) VALUES
    ('John', 'Smith', '1990-01-15'),
    ('John', 'Doe', '1990-01-15'),  -- Different last name, OK
    ('John', 'Smith', '1985-06-20'),  -- Different birth date, OK
    ('Jane', 'Smith', '1990-01-15');  -- Different first name, OK

SELECT id, first_name, last_name, birth_date FROM test_composite_unique ORDER BY id;

-- Invalid: Duplicate composite unique (would fail)
-- INSERT INTO test_composite_unique (first_name, last_name, birth_date) VALUES ('John', 'Smith', '1990-01-15');

-- ============================================================================
-- Section 7: UNIQUE with NULL Handling
-- ============================================================================

CREATE TABLE test_unique_nulls (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50) UNIQUE,
    description TEXT
);

-- Multiple NULLs are allowed in UNIQUE columns
INSERT INTO test_unique_nulls (code, description) VALUES
    ('CODE1', 'First item'),
    (NULL, 'Second item'),
    (NULL, 'Third item'),
    (NULL, 'Fourth item');

SELECT id, code, description FROM test_unique_nulls ORDER BY id;

-- Non-NULL values must still be unique
-- INSERT INTO test_unique_nulls (code, description) VALUES ('CODE1', 'Duplicate');  -- Would fail

-- ============================================================================
-- Section 8: UNIQUE NULLS NOT DISTINCT (PostgreSQL 15+)
-- ============================================================================

-- This would treat NULLs as equal (only one NULL allowed)
-- CREATE TABLE test_unique_nulls_not_distinct (
--     id SERIAL PRIMARY KEY,
--     code VARCHAR(50) UNIQUE NULLS NOT DISTINCT
-- );

-- For older PostgreSQL, use partial unique index:
CREATE TABLE test_unique_nulls_workaround (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50)
);

CREATE UNIQUE INDEX uq_code_not_null ON test_unique_nulls_workaround (code) WHERE code IS NOT NULL;

INSERT INTO test_unique_nulls_workaround (code) VALUES ('CODE1'), (NULL), (NULL);

SELECT id, code FROM test_unique_nulls_workaround ORDER BY id;

-- ============================================================================
-- Section 9: Adding PRIMARY KEY to Existing Table
-- ============================================================================

CREATE TABLE test_add_pk (
    id INT,
    name VARCHAR(200)
);

INSERT INTO test_add_pk (id, name) VALUES (1, 'Item 1'), (2, 'Item 2');

-- Add primary key
ALTER TABLE test_add_pk ADD PRIMARY KEY (id);

-- Verify constraint
SELECT conname, contype FROM pg_constraint WHERE conrelid = 'test_add_pk'::regclass;

-- ============================================================================
-- Section 10: Adding UNIQUE to Existing Table
-- ============================================================================

CREATE TABLE test_add_unique (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200)
);

INSERT INTO test_add_unique (email) VALUES ('user1@example.com'), ('user2@example.com');

-- Add unique constraint
ALTER TABLE test_add_unique ADD CONSTRAINT uq_email UNIQUE (email);

-- Verify constraint
SELECT conname, contype FROM pg_constraint WHERE conrelid = 'test_add_unique'::regclass AND contype = 'u';

-- ============================================================================
-- Section 11: Dropping and Recreating Constraints
-- ============================================================================

CREATE TABLE test_drop_recreate (
    id INT PRIMARY KEY,
    code VARCHAR(50) UNIQUE,
    name VARCHAR(200)
);

INSERT INTO test_drop_recreate (id, code, name) VALUES (1, 'A001', 'Item 1');

-- Drop unique constraint
ALTER TABLE test_drop_recreate DROP CONSTRAINT test_drop_recreate_code_key;

-- Now duplicate codes are allowed
INSERT INTO test_drop_recreate (id, code, name) VALUES (2, 'A001', 'Item 2');

SELECT id, code, name FROM test_drop_recreate ORDER BY id;

-- Recreate unique constraint (must remove duplicates first)
DELETE FROM test_drop_recreate WHERE id = 2;
ALTER TABLE test_drop_recreate ADD CONSTRAINT uq_code UNIQUE (code);

-- ============================================================================
-- Section 12: UNIQUE Index vs UNIQUE Constraint
-- ============================================================================

CREATE TABLE test_unique_index (
    id SERIAL PRIMARY KEY,
    value1 VARCHAR(100),
    value2 VARCHAR(100)
);

-- UNIQUE constraint creates an index automatically
ALTER TABLE test_unique_index ADD CONSTRAINT uq_value1 UNIQUE (value1);

-- UNIQUE index (similar but different catalog storage)
CREATE UNIQUE INDEX uq_idx_value2 ON test_unique_index (value2);

-- Both enforce uniqueness
INSERT INTO test_unique_index (value1, value2) VALUES ('A', 'X'), ('B', 'Y');

-- Query indexes
SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'test_unique_index'
ORDER BY indexname;

-- ============================================================================
-- Section 13: PRIMARY KEY with Different Data Types
-- ============================================================================

-- Integer PK
CREATE TABLE test_pk_int (id INT PRIMARY KEY, data TEXT);
INSERT INTO test_pk_int VALUES (1, 'Data 1'), (2, 'Data 2');

-- UUID PK
CREATE TABLE test_pk_uuid (id UUID PRIMARY KEY, data TEXT);
INSERT INTO test_pk_uuid VALUES
    ('a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11', 'Data 1'),
    ('b1eebc99-9c0b-4ef8-bb6d-6bb9bd380a12', 'Data 2');

-- VARCHAR PK
CREATE TABLE test_pk_varchar (code VARCHAR(50) PRIMARY KEY, data TEXT);
INSERT INTO test_pk_varchar VALUES ('CODE1', 'Data 1'), ('CODE2', 'Data 2');

-- Composite PK with mixed types
CREATE TABLE test_pk_mixed (
    year INT,
    month INT,
    code VARCHAR(50),
    data TEXT,
    PRIMARY KEY (year, month, code)
);

INSERT INTO test_pk_mixed VALUES (2024, 1, 'A001', 'Data 1'), (2024, 1, 'A002', 'Data 2');

-- ============================================================================
-- Section 14: Constraint Validation on INSERT
-- ============================================================================

CREATE TABLE test_constraint_validation (
    id INT PRIMARY KEY,
    unique_code VARCHAR(50) UNIQUE,
    data TEXT
);

-- Valid insert
INSERT INTO test_constraint_validation VALUES (1, 'CODE1', 'Valid data');

-- Test various constraint violations
BEGIN;
-- Duplicate PK
INSERT INTO test_constraint_validation VALUES (1, 'CODE2', 'Duplicate PK');
EXCEPTION WHEN unique_violation THEN
    RAISE NOTICE 'Caught duplicate PK error';
ROLLBACK;

BEGIN;
-- Duplicate UNIQUE
INSERT INTO test_constraint_validation VALUES (2, 'CODE1', 'Duplicate unique');
EXCEPTION WHEN unique_violation THEN
    RAISE NOTICE 'Caught duplicate unique error';
ROLLBACK;

SELECT id, unique_code, data FROM test_constraint_validation;

-- ============================================================================
-- Section 15: Constraint Validation on UPDATE
-- ============================================================================

CREATE TABLE test_update_validation (
    id INT PRIMARY KEY,
    code VARCHAR(50) UNIQUE,
    name VARCHAR(200)
);

INSERT INTO test_update_validation VALUES
    (1, 'A001', 'Item 1'),
    (2, 'A002', 'Item 2'),
    (3, 'A003', 'Item 3');

-- Valid update
UPDATE test_update_validation SET name = 'Updated Item 1' WHERE id = 1;

-- Invalid: Update to duplicate code (would fail)
-- UPDATE test_update_validation SET code = 'A002' WHERE id = 1;

-- Valid: Update to NULL (UNIQUE allows NULL)
UPDATE test_update_validation SET code = NULL WHERE id = 1;

SELECT id, code, name FROM test_update_validation ORDER BY id;

-- ============================================================================
-- Section 16: Multi-Column UNIQUE with Partial NULLs
-- ============================================================================

CREATE TABLE test_partial_null_unique (
    id SERIAL PRIMARY KEY,
    category VARCHAR(100),
    subcategory VARCHAR(100),
    item_code VARCHAR(50),
    CONSTRAINT uq_category_item UNIQUE (category, subcategory, item_code)
);

-- With composite UNIQUE, if ANY column is NULL, the combination is allowed multiple times
INSERT INTO test_partial_null_unique (category, subcategory, item_code) VALUES
    ('Electronics', 'Computers', 'COMP001'),
    ('Electronics', NULL, 'COMP001'),  -- OK: subcategory is NULL
    ('Electronics', NULL, 'COMP001'),  -- OK: Another NULL subcategory
    (NULL, 'Computers', 'COMP001'),    -- OK: category is NULL
    (NULL, 'Computers', 'COMP001');    -- OK: Another NULL category

SELECT id, category, subcategory, item_code FROM test_partial_null_unique ORDER BY id;

-- ============================================================================
-- Section 17: Constraint Error Messages
-- ============================================================================

CREATE TABLE test_error_messages (
    id INT CONSTRAINT pk_test_error PRIMARY KEY,
    email VARCHAR(200) CONSTRAINT uq_test_email UNIQUE
);

-- Attempt duplicate PK and capture error
DO $$
BEGIN
    INSERT INTO test_error_messages VALUES (1, 'test1@example.com');
    INSERT INTO test_error_messages VALUES (1, 'test2@example.com');  -- Fails
EXCEPTION
    WHEN unique_violation THEN
        RAISE NOTICE 'PK violation: %', SQLERRM;
END $$;

-- Attempt duplicate UNIQUE and capture error
DO $$
BEGIN
    INSERT INTO test_error_messages VALUES (2, 'test1@example.com');  -- Fails
EXCEPTION
    WHEN unique_violation THEN
        RAISE NOTICE 'UNIQUE violation: %', SQLERRM;
END $$;

-- ============================================================================
-- Section 18: Performance with Constraints
-- ============================================================================

CREATE TABLE test_performance_pk (
    id INT PRIMARY KEY,
    data TEXT
);

CREATE TABLE test_performance_no_pk (
    id INT,
    data TEXT
);

-- Insert same data into both
INSERT INTO test_performance_pk SELECT i, 'Data ' || i FROM generate_series(1, 1000) i;
INSERT INTO test_performance_no_pk SELECT i, 'Data ' || i FROM generate_series(1, 1000) i;

-- PK lookup is fast (uses index)
SELECT * FROM test_performance_pk WHERE id = 500;

-- Without PK, requires table scan
SELECT * FROM test_performance_no_pk WHERE id = 500;

-- ============================================================================
-- Section 19: Constraint Dependencies
-- ============================================================================

CREATE TABLE test_constraint_deps (
    id INT PRIMARY KEY,
    parent_id INT UNIQUE,
    data TEXT
);

-- Cannot drop columns involved in constraints
-- ALTER TABLE test_constraint_deps DROP COLUMN parent_id;  -- Would fail

-- Must drop constraint first
ALTER TABLE test_constraint_deps DROP CONSTRAINT test_constraint_deps_parent_id_key;

-- Now can drop column
ALTER TABLE test_constraint_deps DROP COLUMN parent_id;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_pk_unique_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_pk_unique_best_practices VALUES
    (1, 'Always use PRIMARY KEY for main identifier'),
    (2, 'PRIMARY KEY creates NOT NULL and UNIQUE constraints'),
    (3, 'Use SERIAL or IDENTITY for auto-incrementing PKs'),
    (4, 'Name constraints explicitly for better error messages'),
    (5, 'Use composite PKs for many-to-many junction tables'),
    (6, 'UNIQUE constraints allow multiple NULLs by default'),
    (7, 'Use partial indexes for unique non-NULL values only'),
    (8, 'Both PK and UNIQUE create indexes automatically'),
    (9, 'Consider performance impact of composite keys'),
    (10, 'Use natural keys when stable, surrogate keys otherwise'),
    (11, 'Test constraint violations in your application'),
    (12, 'Document business rules behind unique constraints'),
    (13, 'Consider UUID PKs for distributed systems'),
    (14, 'Validate data before inserting to avoid constraint errors'),
    (15, 'Monitor constraint violation rates in production');

SELECT id, guideline FROM test_pk_unique_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_pk_unique_best_practices;
DROP TABLE test_constraint_deps;
DROP TABLE test_performance_no_pk;
DROP TABLE test_performance_pk;
DROP TABLE test_error_messages;
DROP TABLE test_partial_null_unique;
DROP TABLE test_update_validation;
DROP TABLE test_constraint_validation;
DROP TABLE test_pk_mixed;
DROP TABLE test_pk_varchar;
DROP TABLE test_pk_uuid;
DROP TABLE test_pk_int;
DROP TABLE test_unique_index;
DROP TABLE test_drop_recreate;
DROP TABLE test_add_unique;
DROP TABLE test_add_pk;
DROP TABLE test_unique_nulls_workaround;
DROP TABLE test_unique_nulls;
DROP TABLE test_composite_unique;
DROP TABLE test_named_unique;
DROP TABLE test_basic_unique;
DROP TABLE test_composite_pk;
DROP TABLE test_named_pk;
DROP TABLE test_basic_pk;

DROP DATABASE test_pk_unique_constraints_db;

-- End of PRIMARY KEY and UNIQUE constraint tests
