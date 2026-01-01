-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - Comment Operations
-- Description: Comprehensive COMMENT ON testing for all object types
-- ============================================================================

-- Comment DDL: Documentation for database objects
-- COMMENT ON: Add comments to objects
-- Comments on tables, columns, schemas, databases, functions, etc.
-- Metadata documentation, self-documenting databases

-- Create test database
CREATE DATABASE test_comments_db;
USE test_comments_db;

-- ============================================================================
-- Section 1: COMMENT ON TABLE
-- ============================================================================

CREATE TABLE test_comment_table (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Add table comment
COMMENT ON TABLE test_comment_table IS 'Test table for comment demonstration';

-- Query table comment
SELECT
    c.relname AS table_name,
    pg_catalog.obj_description(c.oid, 'pg_class') AS table_comment
FROM pg_catalog.pg_class c
WHERE c.relname = 'test_comment_table'
  AND c.relkind = 'r';

-- Update comment
COMMENT ON TABLE test_comment_table IS 'Updated comment for test table';

-- Remove comment
COMMENT ON TABLE test_comment_table IS NULL;

-- Verify removed
SELECT
    c.relname AS table_name,
    pg_catalog.obj_description(c.oid, 'pg_class') AS table_comment
FROM pg_catalog.pg_class c
WHERE c.relname = 'test_comment_table';

-- ============================================================================
-- Section 2: COMMENT ON COLUMN
-- ============================================================================

CREATE TABLE test_column_comments (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP
);

-- Add column comments
COMMENT ON COLUMN test_column_comments.user_id IS 'Unique identifier for user';
COMMENT ON COLUMN test_column_comments.username IS 'User login name (unique)';
COMMENT ON COLUMN test_column_comments.email IS 'User email address';
COMMENT ON COLUMN test_column_comments.created_at IS 'Timestamp when user account was created';
COMMENT ON COLUMN test_column_comments.last_login IS 'Timestamp of most recent login';

-- Query column comments
SELECT
    a.attname AS column_name,
    pg_catalog.col_description(c.oid, a.attnum) AS column_comment
FROM pg_catalog.pg_class c
JOIN pg_catalog.pg_attribute a ON c.oid = a.attrelid
WHERE c.relname = 'test_column_comments'
  AND a.attnum > 0
  AND NOT a.attisdropped
ORDER BY a.attnum;

-- ============================================================================
-- Section 3: COMMENT ON INDEX
-- ============================================================================

CREATE TABLE test_index_comments (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50),
    name VARCHAR(200)
);

CREATE INDEX idx_code ON test_index_comments(code);
CREATE INDEX idx_name_lower ON test_index_comments(LOWER(name));

-- Add index comments
COMMENT ON INDEX idx_code IS 'Index on code column for fast lookup';
COMMENT ON INDEX idx_name_lower IS 'Case-insensitive index on name column';

-- Query index comments
SELECT
    c.relname AS index_name,
    pg_catalog.obj_description(c.oid, 'pg_class') AS index_comment
FROM pg_catalog.pg_class c
WHERE c.relkind = 'i'
  AND c.relname LIKE 'idx_%'
ORDER BY c.relname;

-- ============================================================================
-- Section 4: COMMENT ON SCHEMA
-- ============================================================================

CREATE SCHEMA test_commented_schema;

-- Add schema comment
COMMENT ON SCHEMA test_commented_schema IS 'Schema for test data and temporary operations';

-- Query schema comment
SELECT
    n.nspname AS schema_name,
    pg_catalog.obj_description(n.oid, 'pg_namespace') AS schema_comment
FROM pg_catalog.pg_namespace n
WHERE n.nspname = 'test_commented_schema';

-- ============================================================================
-- Section 5: COMMENT ON DATABASE
-- ============================================================================

-- Add database comment
COMMENT ON DATABASE test_comments_db IS 'Test database for comment functionality';

-- Query database comment
SELECT
    d.datname AS database_name,
    pg_catalog.shobj_description(d.oid, 'pg_database') AS database_comment
FROM pg_catalog.pg_database d
WHERE d.datname = 'test_comments_db';

-- ============================================================================
-- Section 6: COMMENT ON SEQUENCE
-- ============================================================================

CREATE SEQUENCE test_commented_sequence;

-- Add sequence comment
COMMENT ON SEQUENCE test_commented_sequence IS 'Sequence for generating unique identifiers';

-- Query sequence comment
SELECT
    c.relname AS sequence_name,
    pg_catalog.obj_description(c.oid, 'pg_class') AS sequence_comment
FROM pg_catalog.pg_class c
WHERE c.relkind = 'S'
  AND c.relname = 'test_commented_sequence';

-- ============================================================================
-- Section 7: COMMENT ON VIEW
-- ============================================================================

CREATE TABLE test_view_base (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    amount NUMERIC(10,2)
);

CREATE VIEW test_commented_view AS
SELECT id, status, amount
FROM test_view_base
WHERE status = 'active';

-- Add view comment
COMMENT ON VIEW test_commented_view IS 'View showing only active records';

-- Query view comment
SELECT
    c.relname AS view_name,
    pg_catalog.obj_description(c.oid, 'pg_class') AS view_comment
FROM pg_catalog.pg_class c
WHERE c.relkind = 'v'
  AND c.relname = 'test_commented_view';

-- ============================================================================
-- Section 8: COMMENT ON FUNCTION
-- ============================================================================

CREATE FUNCTION test_calculate_total(p_quantity INT, p_price NUMERIC)
RETURNS NUMERIC AS $$
BEGIN
    RETURN p_quantity * p_price;
END;
$$ LANGUAGE plpgsql;

-- Add function comment
COMMENT ON FUNCTION test_calculate_total(INT, NUMERIC) IS 'Calculates total amount from quantity and unit price';

-- Query function comment
SELECT
    p.proname AS function_name,
    pg_catalog.obj_description(p.oid, 'pg_proc') AS function_comment
FROM pg_catalog.pg_proc p
WHERE p.proname = 'test_calculate_total';

-- ============================================================================
-- Section 9: COMMENT ON TRIGGER
-- ============================================================================

CREATE TABLE test_trigger_comments (
    id SERIAL PRIMARY KEY,
    data TEXT,
    modified_at TIMESTAMP
);

CREATE FUNCTION update_modified_timestamp() RETURNS TRIGGER AS $$
BEGIN
    NEW.modified_at := CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_modified
BEFORE UPDATE ON test_trigger_comments
FOR EACH ROW
EXECUTE FUNCTION update_modified_timestamp();

-- Add trigger comment
COMMENT ON TRIGGER trg_update_modified ON test_trigger_comments IS 'Updates modified_at timestamp on every row update';

-- Query trigger comment
SELECT
    t.tgname AS trigger_name,
    pg_catalog.obj_description(t.oid, 'pg_trigger') AS trigger_comment
FROM pg_catalog.pg_trigger t
JOIN pg_catalog.pg_class c ON t.tgrelid = c.oid
WHERE c.relname = 'test_trigger_comments'
  AND t.tgname = 'trg_update_modified';

-- ============================================================================
-- Section 10: COMMENT ON CONSTRAINT
-- ============================================================================

CREATE TABLE test_constraint_comments (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200) UNIQUE,
    age INT,
    CONSTRAINT check_age CHECK (age >= 18 AND age <= 120)
);

-- Add constraint comments
COMMENT ON CONSTRAINT test_constraint_comments_pkey ON test_constraint_comments IS 'Primary key constraint';
COMMENT ON CONSTRAINT test_constraint_comments_email_key ON test_constraint_comments IS 'Unique constraint on email';
COMMENT ON CONSTRAINT check_age ON test_constraint_comments IS 'Ensures age is between 18 and 120';

-- Query constraint comments
SELECT
    con.conname AS constraint_name,
    pg_catalog.obj_description(con.oid, 'pg_constraint') AS constraint_comment
FROM pg_catalog.pg_constraint con
JOIN pg_catalog.pg_class c ON con.conrelid = c.oid
WHERE c.relname = 'test_constraint_comments'
ORDER BY con.conname;

-- ============================================================================
-- Section 11: COMMENT ON TYPE
-- ============================================================================

CREATE TYPE test_status_type AS ENUM ('pending', 'active', 'completed');

-- Add type comment
COMMENT ON TYPE test_status_type IS 'Enumerated type for status values';

-- Query type comment
SELECT
    t.typname AS type_name,
    pg_catalog.obj_description(t.oid, 'pg_type') AS type_comment
FROM pg_catalog.pg_type t
WHERE t.typname = 'test_status_type';

-- ============================================================================
-- Section 12: COMMENT ON DOMAIN
-- ============================================================================

CREATE DOMAIN test_email_domain AS VARCHAR(200)
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}$');

-- Add domain comment
COMMENT ON DOMAIN test_email_domain IS 'Email address domain with regex validation';

-- Query domain comment
SELECT
    t.typname AS domain_name,
    pg_catalog.obj_description(t.oid, 'pg_type') AS domain_comment
FROM pg_catalog.pg_type t
WHERE t.typname = 'test_email_domain'
  AND t.typtype = 'd';

-- ============================================================================
-- Section 13: COMMENT ON ROLE
-- ============================================================================

CREATE ROLE test_commented_role;

-- Add role comment
COMMENT ON ROLE test_commented_role IS 'Application read-only role';

-- Query role comment
SELECT
    r.rolname AS role_name,
    pg_catalog.shobj_description(r.oid, 'pg_authid') AS role_comment
FROM pg_catalog.pg_roles r
WHERE r.rolname = 'test_commented_role';

-- ============================================================================
-- Section 14: COMMENT ON TABLESPACE
-- ============================================================================

-- Query tablespace comments
SELECT
    ts.spcname AS tablespace_name,
    pg_catalog.shobj_description(ts.oid, 'pg_tablespace') AS tablespace_comment
FROM pg_catalog.pg_tablespace ts
ORDER BY ts.spcname;

-- Add tablespace comment (requires superuser and tablespace)
-- COMMENT ON TABLESPACE pg_default IS 'Default tablespace for user data';

-- ============================================================================
-- Section 15: COMMENT ON EXTENSION
-- ============================================================================

-- Query extension comments (if any extensions installed)
SELECT
    e.extname AS extension_name,
    pg_catalog.obj_description(e.oid, 'pg_extension') AS extension_comment
FROM pg_catalog.pg_extension e
ORDER BY e.extname;

-- Add extension comment (example)
-- CREATE EXTENSION IF NOT EXISTS pg_trgm;
-- COMMENT ON EXTENSION pg_trgm IS 'Text similarity measurement and index searching';

-- ============================================================================
-- Section 16: Documentation Best Practice - Complete Table Documentation
-- ============================================================================

CREATE TABLE test_fully_documented_table (
    -- Primary key
    product_id SERIAL PRIMARY KEY,

    -- Product information
    product_code VARCHAR(50) UNIQUE NOT NULL,
    product_name VARCHAR(200) NOT NULL,
    description TEXT,

    -- Pricing
    unit_price NUMERIC(10,2) NOT NULL,
    discount_percentage NUMERIC(5,2) DEFAULT 0.00,

    -- Inventory
    stock_quantity INT NOT NULL DEFAULT 0,
    reorder_level INT,

    -- Metadata
    category VARCHAR(100),
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP
);

-- Table comment
COMMENT ON TABLE test_fully_documented_table IS
'Product catalog table containing all product information, pricing, and inventory levels';

-- Column comments
COMMENT ON COLUMN test_fully_documented_table.product_id IS 'Unique identifier for product (auto-generated)';
COMMENT ON COLUMN test_fully_documented_table.product_code IS 'Human-readable product code (e.g., SKU)';
COMMENT ON COLUMN test_fully_documented_table.product_name IS 'Display name of product';
COMMENT ON COLUMN test_fully_documented_table.description IS 'Detailed product description (markdown supported)';
COMMENT ON COLUMN test_fully_documented_table.unit_price IS 'Base price per unit (before discount)';
COMMENT ON COLUMN test_fully_documented_table.discount_percentage IS 'Current discount percentage (0-100)';
COMMENT ON COLUMN test_fully_documented_table.stock_quantity IS 'Current available inventory';
COMMENT ON COLUMN test_fully_documented_table.reorder_level IS 'Inventory level triggering reorder';
COMMENT ON COLUMN test_fully_documented_table.category IS 'Product category for grouping';
COMMENT ON COLUMN test_fully_documented_table.is_active IS 'Whether product is currently available for sale';
COMMENT ON COLUMN test_fully_documented_table.created_at IS 'Timestamp when product was added to catalog';
COMMENT ON COLUMN test_fully_documented_table.updated_at IS 'Timestamp of last update';

-- Index comments
CREATE INDEX idx_product_code ON test_fully_documented_table(product_code);
COMMENT ON INDEX idx_product_code IS 'Unique index for fast lookup by product code';

CREATE INDEX idx_category_active ON test_fully_documented_table(category, is_active);
COMMENT ON INDEX idx_category_active IS 'Composite index for filtering active products by category';

-- ============================================================================
-- Section 17: Query All Comments in Database
-- ============================================================================

-- Tables with comments
SELECT
    'TABLE' AS object_type,
    c.relname AS object_name,
    pg_catalog.obj_description(c.oid, 'pg_class') AS comment
FROM pg_catalog.pg_class c
WHERE c.relkind = 'r'
  AND pg_catalog.obj_description(c.oid, 'pg_class') IS NOT NULL
  AND c.relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'public')
ORDER BY c.relname;

-- Columns with comments
SELECT
    'COLUMN' AS object_type,
    c.relname || '.' || a.attname AS object_name,
    pg_catalog.col_description(c.oid, a.attnum) AS comment
FROM pg_catalog.pg_class c
JOIN pg_catalog.pg_attribute a ON c.oid = a.attrelid
WHERE c.relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'public')
  AND a.attnum > 0
  AND NOT a.attisdropped
  AND pg_catalog.col_description(c.oid, a.attnum) IS NOT NULL
ORDER BY c.relname, a.attname;

-- Functions with comments
SELECT
    'FUNCTION' AS object_type,
    p.proname AS object_name,
    pg_catalog.obj_description(p.oid, 'pg_proc') AS comment
FROM pg_catalog.pg_proc p
WHERE pg_catalog.obj_description(p.oid, 'pg_proc') IS NOT NULL
  AND p.pronamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'public')
ORDER BY p.proname;

-- ============================================================================
-- Section 18: Comment Templates and Standards
-- ============================================================================

CREATE TABLE test_comment_standards (
    id INT PRIMARY KEY,
    object_type VARCHAR(50),
    comment_template TEXT
);

INSERT INTO test_comment_standards VALUES
    (1, 'TABLE', '[Purpose] - [Key relationships] - [Business rules]'),
    (2, 'COLUMN', '[Description] - [Format/constraints] - [Default behavior]'),
    (3, 'INDEX', '[Purpose] - [Columns indexed] - [Query pattern]'),
    (4, 'FUNCTION', '[Purpose] - [Parameters] - [Return value] - [Side effects]'),
    (5, 'VIEW', '[Data source] - [Filtering logic] - [Intended use]'),
    (6, 'TRIGGER', '[Timing] - [Action] - [Purpose]'),
    (7, 'CONSTRAINT', '[Type] - [Business rule enforced]'),
    (8, 'SCHEMA', '[Purpose] - [Owner/team] - [Contents]'),
    (9, 'TYPE', '[Purpose] - [Valid values] - [Usage pattern]'),
    (10, 'SEQUENCE', '[Purpose] - [Associated table/column]');

SELECT * FROM test_comment_standards ORDER BY id;

-- ============================================================================
-- Section 19: Multi-line Comments
-- ============================================================================

CREATE TABLE test_multiline_comments (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Multi-line comment with formatting
COMMENT ON TABLE test_multiline_comments IS
'Multi-line comment example:

Purpose: Stores test data for demonstration
Owner: Development Team
Created: 2024-01-01
Dependencies: None

Usage Notes:
- Used in test suite only
- Cleared daily
- No production data';

SELECT
    c.relname,
    pg_catalog.obj_description(c.oid, 'pg_class') AS comment
FROM pg_catalog.pg_class c
WHERE c.relname = 'test_multiline_comments';

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_comment_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_comment_best_practices VALUES
    (1, 'Add comments to all public API tables, views, and functions'),
    (2, 'Document column purpose, format, and constraints'),
    (3, 'Explain complex business rules in comments'),
    (4, 'Keep comments current with schema changes'),
    (5, 'Use consistent comment format/template across project'),
    (6, 'Include examples in function comments'),
    (7, 'Document trigger timing and purpose'),
    (8, 'Explain index purpose and query patterns'),
    (9, 'Multi-line comments for detailed documentation'),
    (10, 'Comment constraints to explain business rules'),
    (11, 'Use COMMENT ON, not inline comments'),
    (12, 'Document schema organization and purpose'),
    (13, 'Include owner/team in database comments'),
    (14, 'Document dependencies and relationships'),
    (15, 'Update comments during refactoring'),
    (16, 'Export comments for documentation generation'),
    (17, 'Review comments during code review'),
    (18, 'Use NULL to remove outdated comments'),
    (19, 'Document expected data ranges and formats'),
    (20, 'Self-documenting databases reduce knowledge silos');

SELECT id, guideline FROM test_comment_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_comment_best_practices;
DROP TABLE test_multiline_comments;
DROP TABLE test_comment_standards;
DROP INDEX idx_category_active;
DROP INDEX idx_product_code;
DROP TABLE test_fully_documented_table;
DROP ROLE test_commented_role;
DROP DOMAIN test_email_domain;
DROP TYPE test_status_type;
DROP TABLE test_constraint_comments;
DROP TABLE test_trigger_comments;
DROP FUNCTION update_modified_timestamp();
DROP FUNCTION test_calculate_total(INT, NUMERIC);
DROP VIEW test_commented_view;
DROP TABLE test_view_base;
DROP SEQUENCE test_commented_sequence;
DROP SCHEMA test_commented_schema CASCADE;
DROP INDEX idx_name_lower;
DROP INDEX idx_code;
DROP TABLE test_index_comments;
DROP TABLE test_column_comments;
DROP TABLE test_comment_table;

DROP DATABASE test_comments_db;

-- End of comment tests
