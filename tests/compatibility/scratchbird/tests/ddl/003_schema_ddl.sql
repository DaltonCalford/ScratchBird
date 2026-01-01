-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - Schema Operations
-- Description: Comprehensive CREATE, ALTER, DROP SCHEMA testing
-- ============================================================================

-- Schema DDL: Namespace management
-- CREATE SCHEMA: Create new schemas
-- ALTER SCHEMA: Modify schemas
-- DROP SCHEMA: Remove schemas
-- Schema search path, permissions, organization

-- Create test database
CREATE DATABASE test_schema_ddl_db;
USE test_schema_ddl_db;

-- ============================================================================
-- Section 1: Basic CREATE SCHEMA
-- ============================================================================

-- Create simple schema
CREATE SCHEMA test_basic_schema;

-- Verify schema exists
SELECT
    schema_name,
    schema_owner
FROM information_schema.schemata
WHERE schema_name = 'test_basic_schema';

-- Create table in schema
CREATE TABLE test_basic_schema.test_table (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Query tables in schema
SELECT
    table_schema,
    table_name
FROM information_schema.tables
WHERE table_schema = 'test_basic_schema';

-- ============================================================================
-- Section 2: CREATE SCHEMA with Authorization
-- ============================================================================

-- Create schema owned by current user
CREATE SCHEMA test_auth_schema AUTHORIZATION CURRENT_USER;

-- Verify owner
SELECT
    schema_name,
    schema_owner
FROM information_schema.schemata
WHERE schema_name = 'test_auth_schema';

-- ============================================================================
-- Section 3: CREATE SCHEMA with Objects
-- ============================================================================

-- Create schema with tables in one statement
CREATE SCHEMA test_objects_schema
    CREATE TABLE employees (
        emp_id SERIAL PRIMARY KEY,
        emp_name VARCHAR(200)
    )
    CREATE TABLE departments (
        dept_id SERIAL PRIMARY KEY,
        dept_name VARCHAR(200)
    );

-- Verify objects created
SELECT
    table_schema,
    table_name
FROM information_schema.tables
WHERE table_schema = 'test_objects_schema'
ORDER BY table_name;

-- ============================================================================
-- Section 4: CREATE SCHEMA IF NOT EXISTS
-- ============================================================================

-- Create schema if doesn't exist (no error if exists)
CREATE SCHEMA IF NOT EXISTS test_idempotent_schema;

-- Try again (no error)
CREATE SCHEMA IF NOT EXISTS test_idempotent_schema;

-- Verify single schema created
SELECT COUNT(*) AS schema_count
FROM information_schema.schemata
WHERE schema_name = 'test_idempotent_schema';

-- ============================================================================
-- Section 5: ALTER SCHEMA RENAME
-- ============================================================================

CREATE SCHEMA test_old_schema_name;

-- Rename schema
ALTER SCHEMA test_old_schema_name RENAME TO test_new_schema_name;

-- Verify rename
SELECT schema_name
FROM information_schema.schemata
WHERE schema_name IN ('test_old_schema_name', 'test_new_schema_name');

-- ============================================================================
-- Section 6: ALTER SCHEMA OWNER
-- ============================================================================

CREATE SCHEMA test_owner_schema;

-- Change owner (to current user, for demonstration)
ALTER SCHEMA test_owner_schema OWNER TO CURRENT_USER;

-- Verify owner
SELECT
    schema_name,
    schema_owner
FROM information_schema.schemata
WHERE schema_name = 'test_owner_schema';

-- ============================================================================
-- Section 7: DROP SCHEMA
-- ============================================================================

CREATE SCHEMA test_drop_schema;

-- Drop empty schema
DROP SCHEMA test_drop_schema;

-- Verify dropped
SELECT COUNT(*) AS schema_count
FROM information_schema.schemata
WHERE schema_name = 'test_drop_schema';

-- ============================================================================
-- Section 8: DROP SCHEMA CASCADE
-- ============================================================================

CREATE SCHEMA test_drop_cascade_schema;

CREATE TABLE test_drop_cascade_schema.table1 (id INT);
CREATE TABLE test_drop_cascade_schema.table2 (id INT);

-- Drop schema with all objects
DROP SCHEMA test_drop_cascade_schema CASCADE;

-- Verify dropped
SELECT COUNT(*) AS schema_count
FROM information_schema.schemata
WHERE schema_name = 'test_drop_cascade_schema';

-- ============================================================================
-- Section 9: DROP SCHEMA IF EXISTS
-- ============================================================================

-- Drop if exists (no error if doesn't exist)
DROP SCHEMA IF EXISTS test_nonexistent_schema;

-- Create and drop
CREATE SCHEMA test_temp_schema;
DROP SCHEMA IF EXISTS test_temp_schema;

-- Try again (no error)
DROP SCHEMA IF EXISTS test_temp_schema;

-- ============================================================================
-- Section 10: Schema Search Path
-- ============================================================================

-- Check current search path
SHOW search_path;

CREATE SCHEMA test_search_path_a;
CREATE SCHEMA test_search_path_b;

CREATE TABLE test_search_path_a.test_table (id INT, source VARCHAR(10) DEFAULT 'schema_a');
CREATE TABLE test_search_path_b.test_table (id INT, source VARCHAR(10) DEFAULT 'schema_b');

-- Set search path
SET search_path TO test_search_path_a, test_search_path_b, public;

-- Unqualified query uses first schema in path
-- INSERT INTO test_table (id) VALUES (1);
-- SELECT * FROM test_table;

-- Reset search path
SET search_path TO public;

-- Query with qualified names
SELECT * FROM test_search_path_a.test_table;
SELECT * FROM test_search_path_b.test_table;

-- ============================================================================
-- Section 11: Schema for Organizing Related Objects
-- ============================================================================

-- Organize by application module
CREATE SCHEMA sales;
CREATE SCHEMA inventory;
CREATE SCHEMA hr;

-- Sales tables
CREATE TABLE sales.orders (
    order_id SERIAL PRIMARY KEY,
    customer_id INT,
    order_date DATE
);

CREATE TABLE sales.order_items (
    item_id SERIAL PRIMARY KEY,
    order_id INT,
    product_id INT,
    quantity INT
);

-- Inventory tables
CREATE TABLE inventory.products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    stock_quantity INT
);

-- HR tables
CREATE TABLE hr.employees (
    employee_id SERIAL PRIMARY KEY,
    employee_name VARCHAR(200),
    hire_date DATE
);

-- Query schemas
SELECT
    table_schema,
    COUNT(*) AS table_count
FROM information_schema.tables
WHERE table_schema IN ('sales', 'inventory', 'hr')
GROUP BY table_schema
ORDER BY table_schema;

-- ============================================================================
-- Section 12: Schema for Multi-Tenancy
-- ============================================================================

-- Each tenant gets own schema
CREATE SCHEMA tenant_company_a;
CREATE SCHEMA tenant_company_b;

-- Same table structure in each tenant schema
CREATE TABLE tenant_company_a.users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200)
);

CREATE TABLE tenant_company_b.users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200)
);

-- Tenant data is isolated
INSERT INTO tenant_company_a.users (username, email) VALUES ('alice', 'alice@companya.com');
INSERT INTO tenant_company_b.users (username, email) VALUES ('bob', 'bob@companyb.com');

-- Query per tenant
SELECT * FROM tenant_company_a.users;
SELECT * FROM tenant_company_b.users;

-- ============================================================================
-- Section 13: Schema Privileges
-- ============================================================================

CREATE SCHEMA test_privileges_schema;

-- Grant usage on schema
GRANT USAGE ON SCHEMA test_privileges_schema TO PUBLIC;

-- Grant create privilege
GRANT CREATE ON SCHEMA test_privileges_schema TO CURRENT_USER;

-- Revoke privileges
REVOKE CREATE ON SCHEMA test_privileges_schema FROM CURRENT_USER;

-- Query schema privileges
SELECT
    grantee,
    privilege_type
FROM information_schema.schema_privileges
WHERE schema_name = 'test_privileges_schema'
ORDER BY grantee, privilege_type;

-- ============================================================================
-- Section 14: Default Schema Privileges
-- ============================================================================

CREATE SCHEMA test_default_privileges;

-- Set default privileges for future objects
ALTER DEFAULT PRIVILEGES IN SCHEMA test_default_privileges
GRANT SELECT ON TABLES TO PUBLIC;

-- Future tables will have SELECT granted to PUBLIC
CREATE TABLE test_default_privileges.test_table (id INT);

-- ============================================================================
-- Section 15: Schema for Versioning
-- ============================================================================

-- Version schemas for backward compatibility
CREATE SCHEMA api_v1;
CREATE SCHEMA api_v2;

CREATE TABLE api_v1.endpoints (
    endpoint_id SERIAL PRIMARY KEY,
    path VARCHAR(200),
    method VARCHAR(10)
);

CREATE TABLE api_v2.endpoints (
    endpoint_id SERIAL PRIMARY KEY,
    path VARCHAR(200),
    method VARCHAR(10),
    version VARCHAR(20)  -- New column in v2
);

-- Applications can target specific version
SELECT * FROM api_v1.endpoints;
SELECT * FROM api_v2.endpoints;

-- ============================================================================
-- Section 16: Listing All Schemas
-- ============================================================================

-- Query all schemas in database
SELECT
    schema_name,
    schema_owner,
    CASE
        WHEN schema_name LIKE 'pg_%' THEN 'System'
        WHEN schema_name = 'information_schema' THEN 'System'
        WHEN schema_name = 'public' THEN 'Default'
        ELSE 'User'
    END AS schema_type
FROM information_schema.schemata
ORDER BY schema_type, schema_name;

-- Count schemas by type
SELECT
    CASE
        WHEN schema_name LIKE 'pg_%' THEN 'System'
        WHEN schema_name = 'information_schema' THEN 'System'
        WHEN schema_name = 'public' THEN 'Default'
        ELSE 'User'
    END AS schema_type,
    COUNT(*) AS schema_count
FROM information_schema.schemata
GROUP BY schema_type
ORDER BY schema_type;

-- ============================================================================
-- Section 17: Schema Size and Statistics
-- ============================================================================

CREATE SCHEMA test_schema_stats;

CREATE TABLE test_schema_stats.large_table (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_schema_stats.large_table (data)
SELECT 'Data ' || i FROM generate_series(1, 10000) i;

-- Query schema size
SELECT
    schemaname,
    COUNT(*) AS table_count,
    pg_size_pretty(SUM(pg_total_relation_size(schemaname||'.'||tablename))::BIGINT) AS total_size
FROM pg_tables
WHERE schemaname = 'test_schema_stats'
GROUP BY schemaname;

-- ============================================================================
-- Section 18: Cross-Schema References
-- ============================================================================

CREATE SCHEMA schema_a;
CREATE SCHEMA schema_b;

CREATE TABLE schema_a.categories (
    category_id SERIAL PRIMARY KEY,
    category_name VARCHAR(100)
);

-- Reference table in different schema
CREATE TABLE schema_b.products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    category_id INT REFERENCES schema_a.categories(category_id)
);

-- Insert data
INSERT INTO schema_a.categories (category_name) VALUES ('Electronics'), ('Books');
INSERT INTO schema_b.products (product_name, category_id) VALUES ('Laptop', 1), ('Novel', 2);

-- Cross-schema join
SELECT
    p.product_name,
    c.category_name
FROM schema_b.products p
JOIN schema_a.categories c ON p.category_id = c.category_id;

-- ============================================================================
-- Section 19: Schema Migration Pattern
-- ============================================================================

-- Blue-green deployment pattern
CREATE SCHEMA app_blue;  -- Current production
CREATE SCHEMA app_green; -- New version

-- Deploy to green
CREATE TABLE app_green.users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200),
    new_feature_column TEXT  -- New feature
);

-- After testing, swap schemas
-- Application connects to app_green instead of app_blue

-- Keep app_blue for rollback
-- After confidence period, drop app_blue

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_schema_ddl_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_schema_ddl_best_practices VALUES
    (1, 'Use schemas to organize related objects logically'),
    (2, 'Default "public" schema for simple applications'),
    (3, 'Separate schemas for different application modules'),
    (4, 'Schema-per-tenant for multi-tenant applications'),
    (5, 'Use schemas for API versioning (v1, v2, etc.)'),
    (6, 'Set search_path to avoid fully qualified names'),
    (7, 'Be careful with search_path (security implications)'),
    (8, 'Use IF NOT EXISTS to avoid errors'),
    (9, 'DROP SCHEMA CASCADE removes all objects in schema'),
    (10, 'Grant appropriate privileges on schemas'),
    (11, 'Use ALTER DEFAULT PRIVILEGES for new objects'),
    (12, 'Document schema purpose and ownership'),
    (13, 'Consider schema for blue-green deployments'),
    (14, 'Cross-schema references are supported with foreign keys'),
    (15, 'Monitor schema sizes with pg_tables'),
    (16, 'Avoid hardcoding schema names in application code'),
    (17, 'Use schemas to separate test and production data'),
    (18, 'Backup and restore can be done per-schema'),
    (19, 'Schema names are case-sensitive when quoted'),
    (20, 'System schemas (pg_*, information_schema) are reserved');

SELECT id, guideline FROM test_schema_ddl_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_schema_ddl_best_practices;
DROP SCHEMA app_green CASCADE;
DROP SCHEMA app_blue CASCADE;
DROP SCHEMA schema_b CASCADE;
DROP SCHEMA schema_a CASCADE;
DROP SCHEMA test_schema_stats CASCADE;
DROP SCHEMA api_v2 CASCADE;
DROP SCHEMA api_v1 CASCADE;
DROP SCHEMA test_default_privileges CASCADE;
DROP SCHEMA test_privileges_schema CASCADE;
DROP SCHEMA tenant_company_b CASCADE;
DROP SCHEMA tenant_company_a CASCADE;
DROP SCHEMA hr CASCADE;
DROP SCHEMA inventory CASCADE;
DROP SCHEMA sales CASCADE;
DROP SCHEMA test_search_path_b CASCADE;
DROP SCHEMA test_search_path_a CASCADE;
DROP SCHEMA test_idempotent_schema CASCADE;
DROP SCHEMA test_objects_schema CASCADE;
DROP SCHEMA test_auth_schema CASCADE;
DROP SCHEMA test_new_schema_name CASCADE;
DROP SCHEMA test_owner_schema CASCADE;
DROP SCHEMA test_basic_schema CASCADE;

DROP DATABASE test_schema_ddl_db;

-- End of schema DDL tests
