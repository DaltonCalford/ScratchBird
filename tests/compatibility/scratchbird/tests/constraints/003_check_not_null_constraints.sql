-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Constraints - CHECK and NOT NULL
-- Description: Comprehensive CHECK and NOT NULL constraint testing
-- ============================================================================

-- CHECK: Validates data against boolean expression
-- NOT NULL: Ensures column cannot contain NULL values
-- Both can be column-level or table-level
-- CHECK can reference multiple columns

-- Create test database
CREATE DATABASE test_check_not_null_db;
USE test_check_not_null_db;

-- ============================================================================
-- Section 1: Basic NOT NULL Constraint
-- ============================================================================

CREATE TABLE test_basic_not_null (
    id SERIAL PRIMARY KEY,
    username VARCHAR(100) NOT NULL,
    email VARCHAR(200) NOT NULL,
    bio TEXT  -- Nullable
);

INSERT INTO test_basic_not_null (username, email, bio) VALUES
    ('alice', 'alice@example.com', 'Alice bio'),
    ('bob', 'bob@example.com', NULL);  -- NULL bio is OK

SELECT id, username, email, bio FROM test_basic_not_null ORDER BY id;

-- Invalid: NULL username (would fail)
-- INSERT INTO test_basic_not_null (username, email) VALUES (NULL, 'test@example.com');

-- Invalid: NULL email (would fail)
-- INSERT INTO test_basic_not_null (username, email) VALUES ('charlie', NULL);

-- ============================================================================
-- Section 2: Basic CHECK Constraint
-- ============================================================================

CREATE TABLE test_basic_check (
    id SERIAL PRIMARY KEY,
    age INT CHECK (age >= 0 AND age <= 150),
    price NUMERIC(10,2) CHECK (price > 0),
    status VARCHAR(20) CHECK (status IN ('active', 'inactive', 'pending'))
);

INSERT INTO test_basic_check (age, price, status) VALUES
    (25, 29.99, 'active'),
    (30, 49.99, 'inactive'),
    (0, 10.00, 'pending');  -- age=0 is valid

SELECT id, age, price, status FROM test_basic_check ORDER BY id;

-- Invalid: age out of range (would fail)
-- INSERT INTO test_basic_check (age, price, status) VALUES (200, 10.00, 'active');

-- Invalid: negative price (would fail)
-- INSERT INTO test_basic_check (age, price, status) VALUES (25, -5.00, 'active');

-- Invalid: invalid status (would fail)
-- INSERT INTO test_basic_check (age, price, status) VALUES (25, 10.00, 'deleted');

-- ============================================================================
-- Section 3: Named CHECK Constraint
-- ============================================================================

CREATE TABLE test_named_check (
    id SERIAL PRIMARY KEY,
    score INT,
    grade VARCHAR(2),
    CONSTRAINT chk_score_range CHECK (score >= 0 AND score <= 100),
    CONSTRAINT chk_grade_valid CHECK (grade IN ('A', 'B', 'C', 'D', 'F'))
);

INSERT INTO test_named_check (score, grade) VALUES
    (95, 'A'),
    (85, 'B'),
    (55, 'F');

-- Query constraint information
SELECT conname, contype, pg_get_constraintdef(oid) AS definition
FROM pg_constraint
WHERE conrelid = 'test_named_check'::regclass AND contype = 'c'
ORDER BY conname;

-- ============================================================================
-- Section 4: Multi-Column CHECK Constraint
-- ============================================================================

CREATE TABLE test_multi_column_check (
    id SERIAL PRIMARY KEY,
    start_date DATE NOT NULL,
    end_date DATE NOT NULL,
    min_value INT,
    max_value INT,
    CONSTRAINT chk_date_range CHECK (end_date >= start_date),
    CONSTRAINT chk_value_range CHECK (max_value > min_value)
);

INSERT INTO test_multi_column_check (start_date, end_date, min_value, max_value) VALUES
    ('2024-01-01', '2024-12-31', 10, 100),
    ('2024-06-01', '2024-06-30', 50, 200);

SELECT id, start_date, end_date, min_value, max_value FROM test_multi_column_check ORDER BY id;

-- Invalid: end_date before start_date (would fail)
-- INSERT INTO test_multi_column_check (start_date, end_date, min_value, max_value)
-- VALUES ('2024-12-31', '2024-01-01', 10, 100);

-- Invalid: max <= min (would fail)
-- INSERT INTO test_multi_column_check (start_date, end_date, min_value, max_value)
-- VALUES ('2024-01-01', '2024-12-31', 100, 50);

-- ============================================================================
-- Section 5: CHECK with String Patterns
-- ============================================================================

CREATE TABLE test_check_patterns (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200) CHECK (email ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$'),
    phone VARCHAR(20) CHECK (phone ~ '^\d{3}-\d{3}-\d{4}$'),
    zip_code VARCHAR(10) CHECK (zip_code ~ '^\d{5}(-\d{4})?$')
);

INSERT INTO test_check_patterns (email, phone, zip_code) VALUES
    ('user@example.com', '555-123-4567', '12345'),
    ('admin@test.org', '555-987-6543', '54321-1234');

SELECT id, email, phone, zip_code FROM test_check_patterns ORDER BY id;

-- Invalid: bad email format (would fail)
-- INSERT INTO test_check_patterns (email, phone, zip_code) VALUES ('invalid-email', '555-123-4567', '12345');

-- Invalid: bad phone format (would fail)
-- INSERT INTO test_check_patterns (email, phone, zip_code) VALUES ('user@test.com', '5551234567', '12345');

-- ============================================================================
-- Section 6: CHECK with CASE Expressions
-- ============================================================================

CREATE TABLE test_check_case (
    id SERIAL PRIMARY KEY,
    employee_type VARCHAR(20) CHECK (employee_type IN ('full_time', 'part_time', 'contractor')),
    hours_per_week INT,
    CONSTRAINT chk_hours_by_type CHECK (
        CASE employee_type
            WHEN 'full_time' THEN hours_per_week = 40
            WHEN 'part_time' THEN hours_per_week BETWEEN 10 AND 30
            WHEN 'contractor' THEN hours_per_week BETWEEN 1 AND 40
        END
    )
);

INSERT INTO test_check_case (employee_type, hours_per_week) VALUES
    ('full_time', 40),
    ('part_time', 20),
    ('contractor', 15);

SELECT id, employee_type, hours_per_week FROM test_check_case ORDER BY id;

-- Invalid: part_time with 40 hours (would fail)
-- INSERT INTO test_check_case (employee_type, hours_per_week) VALUES ('part_time', 40);

-- ============================================================================
-- Section 7: NOT NULL with DEFAULT
-- ============================================================================

CREATE TABLE test_not_null_default (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    status VARCHAR(20) NOT NULL DEFAULT 'pending',
    priority INT NOT NULL DEFAULT 5
);

-- Insert without specifying NOT NULL columns (uses defaults)
INSERT INTO test_not_null_default (id) VALUES (DEFAULT);
INSERT INTO test_not_null_default (id, status) VALUES (DEFAULT, 'active');

SELECT id, created_at, status, priority FROM test_not_null_default ORDER BY id;

-- ============================================================================
-- Section 8: Adding NOT NULL to Existing Column
-- ============================================================================

CREATE TABLE test_add_not_null (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    email VARCHAR(200)
);

INSERT INTO test_add_not_null (name, email) VALUES ('Alice', 'alice@example.com');
INSERT INTO test_add_not_null (name, email) VALUES ('Bob', NULL);  -- NULL email

-- Cannot add NOT NULL while NULL values exist
-- ALTER TABLE test_add_not_null ALTER COLUMN email SET NOT NULL;  -- Would fail

-- Update NULL values first
UPDATE test_add_not_null SET email = 'unknown@example.com' WHERE email IS NULL;

-- Now can add NOT NULL
ALTER TABLE test_add_not_null ALTER COLUMN email SET NOT NULL;

SELECT id, name, email FROM test_add_not_null ORDER BY id;

-- ============================================================================
-- Section 9: Removing NOT NULL Constraint
-- ============================================================================

CREATE TABLE test_drop_not_null (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50) NOT NULL,
    description TEXT
);

INSERT INTO test_drop_not_null (code, description) VALUES ('CODE1', 'Description 1');

-- Remove NOT NULL constraint
ALTER TABLE test_drop_not_null ALTER COLUMN code DROP NOT NULL;

-- Now NULL codes are allowed
INSERT INTO test_drop_not_null (code, description) VALUES (NULL, 'Description 2');

SELECT id, code, description FROM test_drop_not_null ORDER BY id;

-- ============================================================================
-- Section 10: Adding CHECK to Existing Table
-- ============================================================================

CREATE TABLE test_add_check (
    id SERIAL PRIMARY KEY,
    quantity INT,
    price NUMERIC(10,2)
);

INSERT INTO test_add_check (quantity, price) VALUES (10, 29.99), (5, 19.99);

-- Add CHECK constraints
ALTER TABLE test_add_check ADD CONSTRAINT chk_quantity_positive CHECK (quantity > 0);
ALTER TABLE test_add_check ADD CONSTRAINT chk_price_positive CHECK (price > 0);

-- Query constraints
SELECT conname, pg_get_constraintdef(oid) AS definition
FROM pg_constraint
WHERE conrelid = 'test_add_check'::regclass AND contype = 'c';

-- ============================================================================
-- Section 11: Dropping CHECK Constraint
-- ============================================================================

CREATE TABLE test_drop_check (
    id SERIAL PRIMARY KEY,
    value INT CONSTRAINT chk_value_range CHECK (value BETWEEN 1 AND 100)
);

INSERT INTO test_drop_check (value) VALUES (50);

-- Drop CHECK constraint
ALTER TABLE test_drop_check DROP CONSTRAINT chk_value_range;

-- Now values outside range are allowed
INSERT INTO test_drop_check (value) VALUES (200), (-50);

SELECT id, value FROM test_drop_check ORDER BY id;

-- ============================================================================
-- Section 12: CHECK with NULL Handling
-- ============================================================================

CREATE TABLE test_check_nulls (
    id SERIAL PRIMARY KEY,
    age INT CHECK (age > 0 OR age IS NULL),  -- NULL is allowed
    score INT CHECK (score BETWEEN 0 AND 100)  -- NULL also passes this CHECK
);

-- CHECK constraints with NULL values evaluate to TRUE
INSERT INTO test_check_nulls (age, score) VALUES
    (25, 95),
    (NULL, 85),  -- NULL age passes CHECK
    (30, NULL);  -- NULL score passes CHECK

SELECT id, age, score FROM test_check_nulls ORDER BY id;

-- ============================================================================
-- Section 13: Complex CHECK Expressions
-- ============================================================================

CREATE TABLE test_complex_check (
    id SERIAL PRIMARY KEY,
    discount_percent NUMERIC(5,2),
    discount_amount NUMERIC(10,2),
    original_price NUMERIC(10,2) NOT NULL,
    final_price NUMERIC(10,2) NOT NULL,
    CONSTRAINT chk_discount_exclusive CHECK (
        (discount_percent IS NOT NULL AND discount_amount IS NULL) OR
        (discount_percent IS NULL AND discount_amount IS NOT NULL) OR
        (discount_percent IS NULL AND discount_amount IS NULL)
    ),
    CONSTRAINT chk_discount_range CHECK (
        discount_percent IS NULL OR (discount_percent >= 0 AND discount_percent <= 100)
    ),
    CONSTRAINT chk_final_price CHECK (
        final_price <= original_price
    )
);

INSERT INTO test_complex_check (discount_percent, discount_amount, original_price, final_price) VALUES
    (10, NULL, 100.00, 90.00),
    (NULL, 5.00, 50.00, 45.00),
    (NULL, NULL, 25.00, 25.00);

SELECT id, discount_percent, discount_amount, original_price, final_price FROM test_complex_check ORDER BY id;

-- ============================================================================
-- Section 14: CHECK with Subqueries (PostgreSQL Extension)
-- ============================================================================

-- Note: CHECK constraints with subqueries are not standard SQL
-- PostgreSQL allows them but they're not enforced on UPDATE/DELETE of referenced tables

CREATE TABLE test_valid_categories (
    category_id SERIAL PRIMARY KEY,
    category_name VARCHAR(100) NOT NULL
);

INSERT INTO test_valid_categories (category_name) VALUES ('Electronics'), ('Books'), ('Toys');

-- This CHECK is evaluated per row but not re-validated if valid_categories changes
CREATE TABLE test_check_subquery (
    id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    category VARCHAR(100),
    CONSTRAINT chk_category_exists CHECK (
        category IN (SELECT category_name FROM test_valid_categories)
    )
);

INSERT INTO test_check_subquery (product_name, category) VALUES
    ('Widget', 'Electronics'),
    ('Novel', 'Books');

SELECT id, product_name, category FROM test_check_subquery ORDER BY id;

-- ============================================================================
-- Section 15: CHECK Constraint Validation on UPDATE
-- ============================================================================

CREATE TABLE test_check_update (
    id SERIAL PRIMARY KEY,
    status VARCHAR(20) CHECK (status IN ('draft', 'published', 'archived')),
    view_count INT CHECK (view_count >= 0)
);

INSERT INTO test_check_update (status, view_count) VALUES
    ('draft', 0),
    ('published', 100);

-- Valid updates
UPDATE test_check_update SET status = 'archived' WHERE id = 1;
UPDATE test_check_update SET view_count = 150 WHERE id = 2;

-- Invalid: bad status (would fail)
-- UPDATE test_check_update SET status = 'deleted' WHERE id = 1;

-- Invalid: negative view_count (would fail)
-- UPDATE test_check_update SET view_count = -10 WHERE id = 2;

SELECT id, status, view_count FROM test_check_update ORDER BY id;

-- ============================================================================
-- Section 16: NOT NULL vs CHECK IS NOT NULL
-- ============================================================================

CREATE TABLE test_not_null_comparison (
    id SERIAL PRIMARY KEY,
    col1 VARCHAR(100) NOT NULL,  -- Column constraint
    col2 VARCHAR(100) CHECK (col2 IS NOT NULL),  -- CHECK constraint
    col3 VARCHAR(100)  -- Nullable
);

-- Both col1 and col2 require non-NULL
INSERT INTO test_not_null_comparison (col1, col2, col3) VALUES ('A', 'B', 'C');

-- NOT NULL is more efficient than CHECK IS NOT NULL
-- NOT NULL is clearer in intent

-- Query to see the difference
SELECT
    a.attname,
    a.attnotnull AS has_not_null
FROM pg_attribute a
WHERE a.attrelid = 'test_not_null_comparison'::regclass
  AND a.attname IN ('col1', 'col2', 'col3')
ORDER BY a.attname;

-- ============================================================================
-- Section 17: Constraint Validation Error Messages
-- ============================================================================

CREATE TABLE test_constraint_errors (
    id SERIAL PRIMARY KEY,
    age INT CONSTRAINT chk_valid_age CHECK (age >= 18),
    email VARCHAR(200) NOT NULL CONSTRAINT chk_email_format
        CHECK (email ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$')
);

-- Test constraint violations
DO $$
BEGIN
    INSERT INTO test_constraint_errors (age, email) VALUES (15, 'valid@example.com');
EXCEPTION
    WHEN check_violation THEN
        RAISE NOTICE 'Age check violation: %', SQLERRM;
END $$;

DO $$
BEGIN
    INSERT INTO test_constraint_errors (age, email) VALUES (25, 'invalid-email');
EXCEPTION
    WHEN check_violation THEN
        RAISE NOTICE 'Email check violation: %', SQLERRM;
END $$;

DO $$
BEGIN
    INSERT INTO test_constraint_errors (age, email) VALUES (25, NULL);
EXCEPTION
    WHEN not_null_violation THEN
        RAISE NOTICE 'NOT NULL violation: %', SQLERRM;
END $$;

-- ============================================================================
-- Section 18: Performance Considerations
-- ============================================================================

CREATE TABLE test_check_performance (
    id SERIAL PRIMARY KEY,
    value INT CHECK (value > 0)  -- Simple CHECK, very fast
);

CREATE TABLE test_complex_check_performance (
    id SERIAL PRIMARY KEY,
    data TEXT CHECK (LENGTH(data) < 10000 AND data ~ '^[A-Z]')  -- More expensive CHECK
);

-- Simple checks have minimal overhead
INSERT INTO test_check_performance SELECT i FROM generate_series(1, 1000) i;

-- Complex checks add more overhead per row
INSERT INTO test_complex_check_performance (data)
SELECT 'A' || REPEAT('x', (RANDOM() * 100)::INT)
FROM generate_series(1, 100);

-- ============================================================================
-- Section 19: Constraints with Domains
-- ============================================================================

CREATE DOMAIN positive_int AS INT CHECK (VALUE > 0);
CREATE DOMAIN email_address AS VARCHAR(200)
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');

CREATE TABLE test_domain_constraints (
    id SERIAL PRIMARY KEY,
    quantity positive_int NOT NULL,
    contact_email email_address NOT NULL
);

INSERT INTO test_domain_constraints (quantity, contact_email) VALUES
    (10, 'user@example.com'),
    (25, 'admin@test.org');

-- Invalid: violates domain CHECK (would fail)
-- INSERT INTO test_domain_constraints (quantity, contact_email) VALUES (-5, 'user@example.com');

SELECT id, quantity, contact_email FROM test_domain_constraints ORDER BY id;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_check_not_null_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_check_not_null_best_practices VALUES
    (1, 'Use NOT NULL for columns that must always have values'),
    (2, 'Combine NOT NULL with DEFAULT for required fields'),
    (3, 'Use CHECK for business rules and data validation'),
    (4, 'Name CHECK constraints explicitly for clear error messages'),
    (5, 'Keep CHECK expressions simple for better performance'),
    (6, 'CHECK constraints evaluate to TRUE for NULL values'),
    (7, 'Use regex patterns in CHECK for format validation'),
    (8, 'Multi-column CHECKs validate relationships between fields'),
    (9, 'Test all boundary conditions in CHECK constraints'),
    (10, 'Document CHECK constraint business rules'),
    (11, 'Consider domains for reusable CHECK constraints'),
    (12, 'Avoid subqueries in CHECK (not re-validated on referenced changes)'),
    (13, 'NOT NULL is more efficient than CHECK IS NOT NULL'),
    (14, 'Validate data in application layer before database'),
    (15, 'Monitor constraint violation rates in production');

SELECT id, guideline FROM test_check_not_null_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_check_not_null_best_practices;
DROP TABLE test_domain_constraints;
DROP DOMAIN email_address;
DROP DOMAIN positive_int;
DROP TABLE test_complex_check_performance;
DROP TABLE test_check_performance;
DROP TABLE test_constraint_errors;
DROP TABLE test_not_null_comparison;
DROP TABLE test_check_update;
DROP TABLE test_check_subquery;
DROP TABLE test_valid_categories;
DROP TABLE test_complex_check;
DROP TABLE test_check_nulls;
DROP TABLE test_drop_check;
DROP TABLE test_add_check;
DROP TABLE test_drop_not_null;
DROP TABLE test_add_not_null;
DROP TABLE test_not_null_default;
DROP TABLE test_check_case;
DROP TABLE test_check_patterns;
DROP TABLE test_multi_column_check;
DROP TABLE test_named_check;
DROP TABLE test_basic_check;
DROP TABLE test_basic_not_null;

DROP DATABASE test_check_not_null_db;

-- End of CHECK and NOT NULL constraint tests
