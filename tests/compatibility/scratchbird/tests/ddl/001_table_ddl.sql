-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - Table Operations
-- Description: Comprehensive CREATE, ALTER, DROP TABLE testing
-- ============================================================================

-- DDL (Data Definition Language): Schema modification operations
-- CREATE TABLE: Create new tables
-- ALTER TABLE: Modify existing tables
-- DROP TABLE: Remove tables
-- Table structure, columns, constraints, inheritance

-- Create test database
CREATE DATABASE test_table_ddl_db;
USE test_table_ddl_db;

-- ============================================================================
-- Section 1: Basic CREATE TABLE
-- ============================================================================

-- Simple table creation
CREATE TABLE test_basic_table (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Verify table exists
SELECT
    table_name,
    table_type
FROM information_schema.tables
WHERE table_schema = 'public'
  AND table_name = 'test_basic_table';

-- Query column information
SELECT
    column_name,
    data_type,
    is_nullable,
    column_default
FROM information_schema.columns
WHERE table_name = 'test_basic_table'
ORDER BY ordinal_position;

-- ============================================================================
-- Section 2: CREATE TABLE with All Data Types
-- ============================================================================

CREATE TABLE test_all_datatypes (
    -- Numeric types
    col_smallint SMALLINT,
    col_integer INTEGER,
    col_bigint BIGINT,
    col_decimal DECIMAL(10,2),
    col_numeric NUMERIC(12,4),
    col_real REAL,
    col_double DOUBLE PRECISION,
    col_serial SERIAL,
    col_bigserial BIGSERIAL,

    -- Character types
    col_char CHAR(10),
    col_varchar VARCHAR(200),
    col_text TEXT,

    -- Binary types
    col_bytea BYTEA,

    -- Date/time types
    col_date DATE,
    col_time TIME,
    col_timestamp TIMESTAMP,
    col_timestamptz TIMESTAMPTZ,
    col_interval INTERVAL,

    -- Boolean
    col_boolean BOOLEAN,

    -- UUID
    col_uuid UUID,

    -- JSON
    col_json JSON,
    col_jsonb JSONB,

    -- Arrays
    col_int_array INT[],
    col_text_array TEXT[],

    -- Network types
    col_inet INET,
    col_cidr CIDR,
    col_macaddr MACADDR,

    PRIMARY KEY (col_serial)
);

-- Count columns
SELECT COUNT(*) AS column_count
FROM information_schema.columns
WHERE table_name = 'test_all_datatypes';

-- ============================================================================
-- Section 3: ALTER TABLE ADD COLUMN
-- ============================================================================

CREATE TABLE test_alter_add_column (
    id SERIAL PRIMARY KEY,
    original_column VARCHAR(100)
);

-- Add single column
ALTER TABLE test_alter_add_column
ADD COLUMN new_column1 INT;

-- Add column with default
ALTER TABLE test_alter_add_column
ADD COLUMN new_column2 VARCHAR(50) DEFAULT 'default_value';

-- Add column with NOT NULL and default
ALTER TABLE test_alter_add_column
ADD COLUMN new_column3 TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP;

-- Add multiple columns
ALTER TABLE test_alter_add_column
ADD COLUMN new_column4 TEXT,
ADD COLUMN new_column5 BOOLEAN DEFAULT FALSE;

-- Verify columns added
SELECT column_name, data_type, column_default, is_nullable
FROM information_schema.columns
WHERE table_name = 'test_alter_add_column'
ORDER BY ordinal_position;

-- ============================================================================
-- Section 4: ALTER TABLE DROP COLUMN
-- ============================================================================

CREATE TABLE test_alter_drop_column (
    id SERIAL PRIMARY KEY,
    col1 VARCHAR(100),
    col2 INT,
    col3 TEXT,
    col4 BOOLEAN,
    col5 DATE
);

-- Drop single column
ALTER TABLE test_alter_drop_column
DROP COLUMN col3;

-- Drop multiple columns
ALTER TABLE test_alter_drop_column
DROP COLUMN col4,
DROP COLUMN col5;

-- Verify columns dropped
SELECT column_name
FROM information_schema.columns
WHERE table_name = 'test_alter_drop_column'
ORDER BY ordinal_position;

-- ============================================================================
-- Section 5: ALTER TABLE RENAME COLUMN
-- ============================================================================

CREATE TABLE test_alter_rename_column (
    id SERIAL PRIMARY KEY,
    old_name VARCHAR(100),
    another_column INT
);

-- Rename column
ALTER TABLE test_alter_rename_column
RENAME COLUMN old_name TO new_name;

-- Verify rename
SELECT column_name
FROM information_schema.columns
WHERE table_name = 'test_alter_rename_column'
ORDER BY ordinal_position;

-- ============================================================================
-- Section 6: ALTER TABLE ALTER COLUMN (Type, Default, NULL)
-- ============================================================================

CREATE TABLE test_alter_column (
    id SERIAL PRIMARY KEY,
    col1 VARCHAR(50),
    col2 INT,
    col3 TEXT
);

-- Change column type
ALTER TABLE test_alter_column
ALTER COLUMN col1 TYPE VARCHAR(200);

-- Set default value
ALTER TABLE test_alter_column
ALTER COLUMN col2 SET DEFAULT 100;

-- Drop default value
ALTER TABLE test_alter_column
ALTER COLUMN col2 DROP DEFAULT;

-- Set NOT NULL
ALTER TABLE test_alter_column
ALTER COLUMN col3 SET NOT NULL;

-- Drop NOT NULL
ALTER TABLE test_alter_column
ALTER COLUMN col3 DROP NOT NULL;

-- Query column properties
SELECT
    column_name,
    data_type,
    character_maximum_length,
    column_default,
    is_nullable
FROM information_schema.columns
WHERE table_name = 'test_alter_column'
ORDER BY ordinal_position;

-- ============================================================================
-- Section 7: ALTER TABLE RENAME TABLE
-- ============================================================================

CREATE TABLE test_old_table_name (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Rename table
ALTER TABLE test_old_table_name
RENAME TO test_new_table_name;

-- Verify rename
SELECT table_name
FROM information_schema.tables
WHERE table_schema = 'public'
  AND table_name IN ('test_old_table_name', 'test_new_table_name');

-- ============================================================================
-- Section 8: DROP TABLE Basic
-- ============================================================================

CREATE TABLE test_drop_basic (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Drop table
DROP TABLE test_drop_basic;

-- Verify dropped
SELECT COUNT(*)
FROM information_schema.tables
WHERE table_name = 'test_drop_basic';

-- ============================================================================
-- Section 9: DROP TABLE CASCADE
-- ============================================================================

CREATE TABLE test_drop_parent (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE test_drop_child (
    id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_drop_parent(id),
    data TEXT
);

-- Drop with CASCADE (removes dependent objects)
DROP TABLE test_drop_parent CASCADE;

-- Both tables dropped
SELECT table_name
FROM information_schema.tables
WHERE table_name IN ('test_drop_parent', 'test_drop_child');

-- ============================================================================
-- Section 10: DROP TABLE IF EXISTS
-- ============================================================================

-- Drop if exists (no error if doesn't exist)
DROP TABLE IF EXISTS test_nonexistent_table;

-- Create and drop
CREATE TABLE test_temp_drop (id INT);
DROP TABLE IF EXISTS test_temp_drop;

-- Try again (no error)
DROP TABLE IF EXISTS test_temp_drop;

-- ============================================================================
-- Section 11: CREATE TABLE AS SELECT (CTAS)
-- ============================================================================

CREATE TABLE test_source_data (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    value INT
);

INSERT INTO test_source_data (category, value) VALUES
    ('A', 100),
    ('B', 200),
    ('A', 150);

-- Create table from query
CREATE TABLE test_ctas AS
SELECT category, SUM(value) AS total
FROM test_source_data
GROUP BY category;

SELECT * FROM test_ctas ORDER BY category;

-- ============================================================================
-- Section 12: CREATE TABLE LIKE
-- ============================================================================

CREATE TABLE test_template (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Create table with same structure
CREATE TABLE test_like_table (LIKE test_template INCLUDING ALL);

-- Verify structure copied
SELECT column_name, data_type, is_nullable, column_default
FROM information_schema.columns
WHERE table_name = 'test_like_table'
ORDER BY ordinal_position;

-- ============================================================================
-- Section 13: CREATE TEMPORARY TABLE
-- ============================================================================

-- Temporary table (session-scoped)
CREATE TEMP TABLE test_temp_session (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_temp_session (data) VALUES ('Temp data 1'), ('Temp data 2');

SELECT * FROM test_temp_session;

-- Temporary table dropped at session end

-- ============================================================================
-- Section 14: CREATE UNLOGGED TABLE
-- ============================================================================

-- Unlogged table (faster, not crash-safe)
CREATE UNLOGGED TABLE test_unlogged_table (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_unlogged_table (data)
SELECT 'Data ' || i FROM generate_series(1, 1000) i;

-- Query table persistence
SELECT
    relname,
    relpersistence
FROM pg_class
WHERE relname = 'test_unlogged_table';
-- relpersistence = 'u' for unlogged

-- ============================================================================
-- Section 15: Table Inheritance
-- ============================================================================

CREATE TABLE test_parent_table (
    id SERIAL PRIMARY KEY,
    common_field VARCHAR(100)
);

CREATE TABLE test_child_table (
    child_specific_field INT
) INHERITS (test_parent_table);

-- Insert data
INSERT INTO test_parent_table (common_field) VALUES ('Parent data');
INSERT INTO test_child_table (common_field, child_specific_field) VALUES ('Child data', 42);

-- Query parent (includes child data)
SELECT * FROM test_parent_table;

-- Query child only
SELECT * FROM ONLY test_parent_table;

-- ============================================================================
-- Section 16: Partitioned Tables (RANGE)
-- ============================================================================

CREATE TABLE test_sales_partitioned (
    sale_id SERIAL,
    sale_date DATE NOT NULL,
    amount NUMERIC(10,2),
    PRIMARY KEY (sale_id, sale_date)
) PARTITION BY RANGE (sale_date);

-- Create partitions
CREATE TABLE test_sales_2024_q1 PARTITION OF test_sales_partitioned
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');

CREATE TABLE test_sales_2024_q2 PARTITION OF test_sales_partitioned
    FOR VALUES FROM ('2024-04-01') TO ('2024-07-01');

-- Insert data
INSERT INTO test_sales_partitioned (sale_date, amount) VALUES
    ('2024-02-15', 100.00),
    ('2024-05-20', 150.00);

-- Query partitioned table
SELECT tableoid::regclass AS partition, * FROM test_sales_partitioned;

-- ============================================================================
-- Section 17: Partitioned Tables (LIST)
-- ============================================================================

CREATE TABLE test_customers_partitioned (
    customer_id SERIAL,
    region VARCHAR(50) NOT NULL,
    name VARCHAR(200),
    PRIMARY KEY (customer_id, region)
) PARTITION BY LIST (region);

CREATE TABLE test_customers_north PARTITION OF test_customers_partitioned
    FOR VALUES IN ('North', 'Northeast');

CREATE TABLE test_customers_south PARTITION OF test_customers_partitioned
    FOR VALUES IN ('South', 'Southeast');

INSERT INTO test_customers_partitioned (region, name) VALUES
    ('North', 'Customer A'),
    ('South', 'Customer B');

SELECT tableoid::regclass AS partition, * FROM test_customers_partitioned;

-- ============================================================================
-- Section 18: Table Storage Parameters
-- ============================================================================

CREATE TABLE test_storage_params (
    id SERIAL PRIMARY KEY,
    data TEXT
) WITH (
    fillfactor = 70,
    autovacuum_enabled = true
);

-- Query storage parameters
SELECT
    reloptions
FROM pg_class
WHERE relname = 'test_storage_params';

-- Alter storage parameters
ALTER TABLE test_storage_params
SET (fillfactor = 80);

-- ============================================================================
-- Section 19: Table Constraints in CREATE TABLE
-- ============================================================================

CREATE TABLE test_table_constraints (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200) UNIQUE NOT NULL,
    age INT CHECK (age >= 18 AND age <= 120),
    balance NUMERIC(10,2) CHECK (balance >= 0) DEFAULT 0.00,
    status VARCHAR(20) CHECK (status IN ('active', 'inactive', 'suspended')),
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT check_email_format CHECK (email LIKE '%@%.%')
);

-- Query constraints
SELECT
    conname AS constraint_name,
    contype AS constraint_type,
    pg_get_constraintdef(oid) AS definition
FROM pg_constraint
WHERE conrelid = 'test_table_constraints'::regclass
ORDER BY conname;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_table_ddl_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_table_ddl_best_practices VALUES
    (1, 'Use SERIAL or IDENTITY for auto-incrementing primary keys'),
    (2, 'Always define PRIMARY KEY for tables'),
    (3, 'Use descriptive table and column names'),
    (4, 'Choose appropriate data types (don''t use VARCHAR(MAX) everywhere)'),
    (5, 'Set NOT NULL for required columns'),
    (6, 'Use DEFAULT values where appropriate'),
    (7, 'Add constraints at table creation when possible'),
    (8, 'Use TEMP tables for session-specific data'),
    (9, 'Use UNLOGGED tables only for data that can be regenerated'),
    (10, 'Partition large tables for better performance and maintenance'),
    (11, 'Use IF EXISTS/IF NOT EXISTS to avoid errors'),
    (12, 'Test DDL changes in development before production'),
    (13, 'ALTER TABLE operations may lock the table'),
    (14, 'DROP TABLE CASCADE removes dependent objects'),
    (15, 'Use LIKE to copy table structure without data'),
    (16, 'CREATE TABLE AS SELECT creates table from query results'),
    (17, 'Consider fillfactor for tables with frequent updates'),
    (18, 'Document table purpose and column meanings'),
    (19, 'Use CHECK constraints for data validation'),
    (20, 'Back up before making DDL changes in production');

SELECT id, guideline FROM test_table_ddl_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_table_ddl_best_practices;
DROP TABLE test_table_constraints;
DROP TABLE test_storage_params;
DROP TABLE test_customers_south;
DROP TABLE test_customers_north;
DROP TABLE test_customers_partitioned;
DROP TABLE test_sales_2024_q2;
DROP TABLE test_sales_2024_q1;
DROP TABLE test_sales_partitioned;
DROP TABLE test_child_table;
DROP TABLE test_parent_table;
DROP TABLE test_unlogged_table;
DROP TABLE test_like_table;
DROP TABLE test_template;
DROP TABLE test_ctas;
DROP TABLE test_source_data;
DROP TABLE test_new_table_name;
DROP TABLE test_alter_column;
DROP TABLE test_alter_rename_column;
DROP TABLE test_alter_drop_column;
DROP TABLE test_alter_add_column;
DROP TABLE test_all_datatypes;
DROP TABLE test_basic_table;

DROP DATABASE test_table_ddl_db;

-- End of table DDL tests
