-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - Type and Domain Operations
-- Description: Comprehensive CREATE, ALTER, DROP TYPE/DOMAIN testing
-- ============================================================================

-- Type/Domain DDL: Custom data types
-- CREATE TYPE: Composite types, enum types, range types
-- CREATE DOMAIN: Constrained types
-- ALTER TYPE/DOMAIN: Modify types
-- DROP TYPE/DOMAIN: Remove types

-- Create test database
CREATE DATABASE test_type_domain_ddl_db;
USE test_type_domain_ddl_db;

-- ============================================================================
-- Section 1: CREATE ENUM TYPE
-- ============================================================================

-- Create enumerated type
CREATE TYPE test_status_enum AS ENUM ('pending', 'active', 'completed', 'cancelled');

-- Use enum in table
CREATE TABLE test_enum_table (
    id SERIAL PRIMARY KEY,
    status test_status_enum
);

INSERT INTO test_enum_table (status) VALUES
    ('pending'),
    ('active'),
    ('completed');

SELECT * FROM test_enum_table;

-- Query enum values
SELECT enumtypid::regtype AS enum_type, enumlabel AS enum_value
FROM pg_enum
WHERE enumtypid = 'test_status_enum'::regtype
ORDER BY enumsortorder;

-- ============================================================================
-- Section 2: ALTER ENUM TYPE - Add Value
-- ============================================================================

-- Add new enum value
ALTER TYPE test_status_enum ADD VALUE 'archived';

-- Add value before existing value
ALTER TYPE test_status_enum ADD VALUE 'processing' BEFORE 'active';

-- Add value after existing value
ALTER TYPE test_status_enum ADD VALUE 'failed' AFTER 'cancelled';

-- Query updated enum values
SELECT enumlabel AS enum_value
FROM pg_enum
WHERE enumtypid = 'test_status_enum'::regtype
ORDER BY enumsortorder;

-- ============================================================================
-- Section 3: CREATE COMPOSITE TYPE
-- ============================================================================

-- Create composite type (record type)
CREATE TYPE test_address_type AS (
    street VARCHAR(200),
    city VARCHAR(100),
    state VARCHAR(50),
    zip_code VARCHAR(20),
    country VARCHAR(100)
);

-- Use composite type in table
CREATE TABLE test_composite_table (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    address test_address_type
);

-- Insert data with composite type
INSERT INTO test_composite_table (name, address) VALUES
    ('Person 1', ROW('123 Main St', 'New York', 'NY', '10001', 'USA')::test_address_type),
    ('Person 2', ROW('456 Oak Ave', 'Los Angeles', 'CA', '90001', 'USA')::test_address_type);

-- Query composite type fields
SELECT
    name,
    (address).street,
    (address).city,
    (address).state
FROM test_composite_table;

-- ============================================================================
-- Section 4: ALTER COMPOSITE TYPE
-- ============================================================================

-- Add attribute to composite type
ALTER TYPE test_address_type ADD ATTRIBUTE apartment VARCHAR(20);

-- Rename attribute
ALTER TYPE test_address_type RENAME ATTRIBUTE zip_code TO postal_code;

-- Change attribute type
ALTER TYPE test_address_type ALTER ATTRIBUTE state TYPE VARCHAR(100);

-- Drop attribute
ALTER TYPE test_address_type DROP ATTRIBUTE apartment;

-- Query type attributes
SELECT
    a.attname AS attribute_name,
    format_type(a.atttypid, a.atttypmod) AS data_type
FROM pg_type t
JOIN pg_attribute a ON a.attrelid = t.typrelid
WHERE t.typname = 'test_address_type'
  AND a.attnum > 0
  AND NOT a.attisdropped
ORDER BY a.attnum;

-- ============================================================================
-- Section 5: CREATE DOMAIN - Basic
-- ============================================================================

-- Create domain with constraint
CREATE DOMAIN test_email_domain AS VARCHAR(200)
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}$');

-- Use domain in table
CREATE TABLE test_domain_table (
    id SERIAL PRIMARY KEY,
    email test_email_domain
);

-- Valid email
INSERT INTO test_domain_table (email) VALUES ('user@example.com');

-- Invalid email (fails check constraint)
DO $$
BEGIN
    INSERT INTO test_domain_table (email) VALUES ('invalid-email');
EXCEPTION
    WHEN check_violation THEN
        RAISE NOTICE 'Domain constraint violation';
END $$;

SELECT * FROM test_domain_table;

-- ============================================================================
-- Section 6: CREATE DOMAIN with Multiple Constraints
-- ============================================================================

CREATE DOMAIN test_age_domain AS INT
    CHECK (VALUE >= 0)
    CHECK (VALUE <= 150);

CREATE DOMAIN test_positive_amount AS NUMERIC(10,2)
    CHECK (VALUE > 0)
    NOT NULL
    DEFAULT 0.00;

-- Use domains
CREATE TABLE test_multi_constraint_domain (
    id SERIAL PRIMARY KEY,
    age test_age_domain,
    amount test_positive_amount
);

INSERT INTO test_multi_constraint_domain (age, amount) VALUES (25, 100.50);

-- Test constraint
DO $$
BEGIN
    INSERT INTO test_multi_constraint_domain (age, amount) VALUES (200, 50.00);
EXCEPTION
    WHEN check_violation THEN
        RAISE NOTICE 'Age domain constraint violated';
END $$;

-- ============================================================================
-- Section 7: ALTER DOMAIN - Add/Drop Constraint
-- ============================================================================

CREATE DOMAIN test_alter_domain AS VARCHAR(100);

-- Add constraint
ALTER DOMAIN test_alter_domain ADD CONSTRAINT check_length CHECK (LENGTH(VALUE) >= 5);

-- Add NOT NULL
ALTER DOMAIN test_alter_domain SET NOT NULL;

-- Set default
ALTER DOMAIN test_alter_domain SET DEFAULT 'default_value';

-- Drop constraint
ALTER DOMAIN test_alter_domain DROP CONSTRAINT check_length;

-- Drop NOT NULL
ALTER DOMAIN test_alter_domain DROP NOT NULL;

-- Drop default
ALTER DOMAIN test_alter_domain DROP DEFAULT;

-- Query domain constraints
SELECT
    conname AS constraint_name,
    pg_get_constraintdef(oid) AS definition
FROM pg_constraint
WHERE contypid = 'test_alter_domain'::regtype;

-- ============================================================================
-- Section 8: CREATE RANGE TYPE
-- ============================================================================

-- Range types (built-in: int4range, int8range, numrange, tsrange, etc.)

CREATE TABLE test_range_types (
    id SERIAL PRIMARY KEY,
    int_range INT4RANGE,
    num_range NUMRANGE,
    date_range DATERANGE,
    timestamp_range TSRANGE
);

INSERT INTO test_range_types (int_range, num_range, date_range, timestamp_range) VALUES
    ('[1,10]'::INT4RANGE, '[0.0,100.0)'::NUMRANGE, '[2024-01-01,2024-12-31]'::DATERANGE, '[2024-01-01 00:00:00,2024-01-31 23:59:59]'::TSRANGE);

-- Query range bounds
SELECT
    int_range,
    lower(int_range) AS int_lower,
    upper(int_range) AS int_upper,
    num_range,
    lower(num_range) AS num_lower,
    upper(num_range) AS num_upper
FROM test_range_types;

-- ============================================================================
-- Section 9: Custom Range Type
-- ============================================================================

-- Create custom range type
CREATE TYPE test_float_range AS RANGE (
    subtype = FLOAT8,
    subtype_diff = float8mi
);

CREATE TABLE test_custom_range (
    id SERIAL PRIMARY KEY,
    price_range test_float_range
);

INSERT INTO test_custom_range (price_range) VALUES
    ('[10.5,99.99]'::test_float_range),
    ('[100.00,500.00)'::test_float_range);

SELECT * FROM test_custom_range;

-- ============================================================================
-- Section 10: DROP DOMAIN
-- ============================================================================

CREATE DOMAIN test_drop_domain AS VARCHAR(50);

CREATE TABLE test_drop_domain_table (
    id SERIAL PRIMARY KEY,
    value test_drop_domain
);

-- Cannot drop domain in use
-- DROP DOMAIN test_drop_domain;  -- Would fail

-- Drop with CASCADE (removes column using domain)
DROP DOMAIN test_drop_domain CASCADE;

-- Verify table altered
SELECT
    column_name,
    data_type
FROM information_schema.columns
WHERE table_name = 'test_drop_domain_table';

-- ============================================================================
-- Section 11: DROP TYPE
-- ============================================================================

CREATE TYPE test_drop_enum AS ENUM ('a', 'b', 'c');

-- Drop enum
DROP TYPE test_drop_enum;

-- Verify dropped
SELECT COUNT(*) AS type_count
FROM pg_type
WHERE typname = 'test_drop_enum';

-- Drop if exists
DROP TYPE IF EXISTS test_nonexistent_type;

-- ============================================================================
-- Section 12: Domain vs CHECK Constraint
-- ============================================================================

-- Domain (reusable across tables)
CREATE DOMAIN test_username_domain AS VARCHAR(100)
    CHECK (LENGTH(VALUE) >= 3 AND LENGTH(VALUE) <= 50);

CREATE TABLE test_users_domain (
    user_id SERIAL PRIMARY KEY,
    username test_username_domain
);

-- Equivalent with CHECK constraint (not reusable)
CREATE TABLE test_users_check (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100) CHECK (LENGTH(username) >= 3 AND LENGTH(username) <= 50)
);

-- Domain benefit: Centralized constraint definition
-- Changing domain updates all tables using it

-- ============================================================================
-- Section 13: Composite Type in Array
-- ============================================================================

CREATE TYPE test_phone_type AS (
    phone_type VARCHAR(20),  -- mobile, home, work
    number VARCHAR(20)
);

CREATE TABLE test_contacts (
    contact_id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    phones test_phone_type[]
);

INSERT INTO test_contacts (name, phones) VALUES
    ('Contact 1', ARRAY[
        ROW('mobile', '555-1234')::test_phone_type,
        ROW('work', '555-5678')::test_phone_type
    ]);

SELECT
    name,
    phones
FROM test_contacts;

-- ============================================================================
-- Section 14: Type Information Queries
-- ============================================================================

-- Query all user-defined types
SELECT
    t.typname AS type_name,
    CASE t.typtype
        WHEN 'c' THEN 'Composite'
        WHEN 'e' THEN 'Enum'
        WHEN 'd' THEN 'Domain'
        WHEN 'r' THEN 'Range'
        ELSE 'Other'
    END AS type_category,
    pg_catalog.format_type(t.oid, NULL) AS full_type_name
FROM pg_type t
LEFT JOIN pg_namespace n ON n.oid = t.typnamespace
WHERE (t.typrelid = 0 OR (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid))
  AND NOT EXISTS(SELECT 1 FROM pg_catalog.pg_type el WHERE el.oid = t.typelem AND el.typarray = t.oid)
  AND n.nspname = 'public'
ORDER BY type_category, type_name;

-- ============================================================================
-- Section 15: Domain Inheritance and Casting
-- ============================================================================

CREATE DOMAIN test_base_domain AS TEXT;
CREATE DOMAIN test_child_domain AS test_base_domain
    CHECK (LENGTH(VALUE) <= 100);

-- Child domain inherits base but adds more constraints
CREATE TABLE test_domain_inheritance (
    id SERIAL PRIMARY KEY,
    base_value test_base_domain,
    child_value test_child_domain
);

-- ============================================================================
-- Section 16: Enum Type Comparison
-- ============================================================================

CREATE TYPE test_priority_enum AS ENUM ('low', 'medium', 'high', 'critical');

CREATE TABLE test_enum_comparison (
    task_id SERIAL PRIMARY KEY,
    task_name VARCHAR(200),
    priority test_priority_enum
);

INSERT INTO test_enum_comparison (task_name, priority) VALUES
    ('Task 1', 'low'),
    ('Task 2', 'high'),
    ('Task 3', 'medium'),
    ('Task 4', 'critical');

-- Enum values are ordered
SELECT task_name, priority
FROM test_enum_comparison
WHERE priority > 'medium'
ORDER BY priority;

-- ============================================================================
-- Section 17: Composite Type as Function Return
-- ============================================================================

CREATE TYPE test_person_type AS (
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    age INT
);

CREATE FUNCTION get_person(p_id INT) RETURNS test_person_type AS $$
BEGIN
    RETURN ROW('John', 'Doe', 30)::test_person_type;
END;
$$ LANGUAGE plpgsql;

SELECT (get_person(1)).*;

-- ============================================================================
-- Section 18: Domain with Custom Collation
-- ============================================================================

CREATE DOMAIN test_collated_domain AS TEXT
    COLLATE "C";

CREATE TABLE test_collation_domain (
    id SERIAL PRIMARY KEY,
    sorted_text test_collated_domain
);

INSERT INTO test_collation_domain (sorted_text) VALUES ('b'), ('A'), ('c'), ('B');

-- Sorted with C collation (case-sensitive)
SELECT sorted_text
FROM test_collation_domain
ORDER BY sorted_text;

-- ============================================================================
-- Section 19: Type Dependencies
-- ============================================================================

-- Query type dependencies
SELECT
    t.typname AS type_name,
    COUNT(a.attrelid) AS tables_using_type
FROM pg_type t
LEFT JOIN pg_attribute a ON a.atttypid = t.oid
WHERE t.typnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'public')
  AND NOT a.attisdropped
GROUP BY t.typname
HAVING COUNT(a.attrelid) > 0
ORDER BY tables_using_type DESC, type_name;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_type_domain_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_type_domain_best_practices VALUES
    (1, 'Use ENUMs for fixed, small sets of values'),
    (2, 'Domains enforce constraints across multiple tables'),
    (3, 'Composite types group related attributes'),
    (4, 'Enum values cannot be removed (only added)'),
    (5, 'Add enum values with BEFORE/AFTER for ordering'),
    (6, 'Domains can have multiple CHECK constraints'),
    (7, 'Use domains for email, phone, ZIP codes validation'),
    (8, 'Composite types are useful for address, contact info'),
    (9, 'Range types for date ranges, price ranges, etc.'),
    (10, 'ALTER TYPE ADD VALUE not transactional (commit immediately)'),
    (11, 'DROP TYPE CASCADE removes dependent objects'),
    (12, 'Domains vs CHECK: domains are reusable'),
    (13, 'Query pg_type for all user-defined types'),
    (14, 'Enum comparison uses definition order'),
    (15, 'Composite types can be array elements'),
    (16, 'Composite types can be function return types'),
    (17, 'Test type changes in development first'),
    (18, 'Document type purpose and constraints'),
    (19, 'Consider migration impact when changing types'),
    (20, 'Use descriptive type names (avoid generic names)');

SELECT id, guideline FROM test_type_domain_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_type_domain_best_practices;
DROP TABLE test_collation_domain;
DROP DOMAIN test_collated_domain;
DROP FUNCTION get_person(INT);
DROP TYPE test_person_type;
DROP TABLE test_enum_comparison;
DROP TYPE test_priority_enum;
DROP TABLE test_domain_inheritance;
DROP DOMAIN test_child_domain;
DROP DOMAIN test_base_domain;
DROP TABLE test_contacts;
DROP TYPE test_phone_type;
DROP TABLE test_users_check;
DROP TABLE test_users_domain;
DROP DOMAIN test_username_domain;
DROP TABLE test_drop_domain_table;
DROP TABLE test_custom_range;
DROP TYPE test_float_range;
DROP TABLE test_range_types;
DROP DOMAIN test_alter_domain;
DROP TABLE test_multi_constraint_domain;
DROP DOMAIN test_positive_amount;
DROP DOMAIN test_age_domain;
DROP TABLE test_domain_table;
DROP DOMAIN test_email_domain;
DROP TABLE test_composite_table;
DROP TYPE test_address_type;
DROP TABLE test_enum_table;
DROP TYPE test_status_enum;

DROP DATABASE test_type_domain_ddl_db;

-- End of type and domain DDL tests
