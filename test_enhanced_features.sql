-- ScratchBird v0.5 Enhanced Features Test Script
-- Test hierarchical schemas, schema-aware database links, and enhanced roles

-- Enable SQL Dialect 4 for enhanced schema support
SET SQL DIALECT 4;

-- Test 1: Create hierarchical schema structure
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;

-- Test 2: Create schema-aware enhanced role
CREATE ROLE finance_manager 
  SCHEMA finance 
  HOME_SCHEMA finance.accounting
  DEFAULT_PRIVILEGES SELECT, INSERT, UPDATE, DELETE;

-- Test 3: Create table in nested schema
CREATE TABLE finance.accounting.reports.monthly_summary (
    id INTEGER PRIMARY KEY,
    month_name VARCHAR(20),
    total_amount DECIMAL(15,2),
    created_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Test 4: Create synonym for easier access
CREATE SYNONYM monthly_reports 
  FOR finance.accounting.reports.monthly_summary;

-- Test 5: Test hierarchical schema view
SELECT 
    rdb$schema_name,
    rdb$parent_schema_name,
    rdb$schema_path,
    rdb$schema_level
FROM rdb$schema_hierarchy
WHERE rdb$schema_path STARTING WITH 'finance';

-- Test 6: Grant role with schema context
GRANT finance_manager TO USER accounting_user;

-- Test 7: Create schema-aware database link (example syntax)
-- CREATE DATABASE LINK hr_link 
--   TO 'remote_server:hr_database'
--   USER 'remote_user' PASSWORD 'remote_pass'
--   SCHEMA_MODE HIERARCHICAL
--   LOCAL_SCHEMA 'finance'
--   REMOTE_SCHEMA 'human_resources';

-- Test 8: Query role hierarchy
SELECT 
    rdb$role_name,
    rdb$schema_name,
    rdb$home_schema,
    rdb$default_privileges
FROM rdb$roles
WHERE rdb$role_name = 'FINANCE_MANAGER';

-- Test 9: Insert test data
INSERT INTO finance.accounting.reports.monthly_summary 
    (id, month_name, total_amount)
VALUES 
    (1, 'January 2025', 125000.50),
    (2, 'February 2025', 132750.75);

-- Test 10: Query using synonym
SELECT * FROM monthly_reports;

-- Test 11: Show enhanced schema information
SELECT 
    'ENHANCED FEATURES TEST RESULTS' as test_status,
    COUNT(*) as schema_count
FROM rdb$schemas
WHERE rdb$schema_name STARTING WITH 'finance';

COMMIT;