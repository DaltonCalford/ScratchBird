/*
 * ScratchBird v0.6.0 Hierarchical Schema Runtime Testing
 * 
 * Test script to verify hierarchical schema creation functionality
 * Tests nested schema creation, validation, and basic operations
 */

-- Test 1: Verify SYSTEM schema exists from bootstrap
SELECT 'Test 1: SYSTEM schema verification' AS test_name;
SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_PATH, RDB$SCHEMA_LEVEL, RDB$SYS_FLAG
FROM RDB$SCHEMAS 
WHERE RDB$SCHEMA_NAME = 'SYSTEM';

-- Test 2: Create root-level schema
SELECT 'Test 2: Create root schema' AS test_name;
CREATE SCHEMA finance;

-- Verify schema was created
SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_PATH, RDB$SCHEMA_LEVEL, RDB$PARENT_SCHEMA_NAME
FROM RDB$SCHEMAS 
WHERE RDB$SCHEMA_NAME = 'finance';

-- Test 3: Create second-level nested schema
SELECT 'Test 3: Create nested schema (level 2)' AS test_name;
CREATE SCHEMA finance.accounting;

-- Verify nested schema was created with correct hierarchy
SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_PATH, RDB$SCHEMA_LEVEL, RDB$PARENT_SCHEMA_NAME
FROM RDB$SCHEMAS 
WHERE RDB$SCHEMA_NAME = 'accounting' AND RDB$PARENT_SCHEMA_NAME = 'finance';

-- Test 4: Create third-level nested schema
SELECT 'Test 4: Create deep nested schema (level 3)' AS test_name;
CREATE SCHEMA finance.accounting.reports;

-- Verify deep nested schema with full path
SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_PATH, RDB$SCHEMA_LEVEL, RDB$PARENT_SCHEMA_NAME
FROM RDB$SCHEMAS 
WHERE RDB$SCHEMA_PATH = 'finance.accounting.reports';

-- Test 5: Create table in nested schema
SELECT 'Test 5: Create table in nested schema' AS test_name;
CREATE TABLE finance.accounting.reports.monthly_summary (
    id INTEGER NOT NULL PRIMARY KEY,
    month_name VARCHAR(20),
    total_amount DECIMAL(15,2),
    created_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Verify table exists in correct schema context
SELECT RDB$RELATION_NAME, RDB$SCHEMA_NAME
FROM RDB$RELATIONS 
WHERE RDB$RELATION_NAME = 'monthly_summary';

-- Test 6: Query table using full hierarchical path
SELECT 'Test 6: Query table with hierarchical path' AS test_name;
-- Insert test data
INSERT INTO finance.accounting.reports.monthly_summary (id, month_name, total_amount)
VALUES (1, 'January', 125000.50);

-- Query using full path
SELECT id, month_name, total_amount 
FROM finance.accounting.reports.monthly_summary
WHERE id = 1;

-- Test 7: Test schema path resolution
SELECT 'Test 7: Schema path resolution verification' AS test_name;
-- Verify all created schemas have correct paths
SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_PATH, RDB$SCHEMA_LEVEL
FROM RDB$SCHEMAS 
WHERE RDB$SCHEMA_PATH LIKE 'finance%'
ORDER BY RDB$SCHEMA_LEVEL, RDB$SCHEMA_PATH;

-- Test 8: Test schema hierarchy constraints
SELECT 'Test 8: Schema hierarchy validation' AS test_name;
-- This should fail - cannot create schema with non-existent parent
-- CREATE SCHEMA nonexistent.child.schema;
-- (Comment out to avoid test failure, but documents expected behavior)

-- Test 9: Test maximum schema depth (should work up to 8 levels)
SELECT 'Test 9: Test deep schema hierarchy' AS test_name;
CREATE SCHEMA finance.accounting.reports.quarterly;
CREATE SCHEMA finance.accounting.reports.quarterly.q1;
CREATE SCHEMA finance.accounting.reports.quarterly.q1.january;

-- Verify deep hierarchy
SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_PATH, RDB$SCHEMA_LEVEL
FROM RDB$SCHEMAS 
WHERE RDB$SCHEMA_PATH LIKE 'finance.accounting.reports.quarterly%'
ORDER BY RDB$SCHEMA_LEVEL;

-- Test 10: Cleanup test data
SELECT 'Test 10: Cleanup test schemas' AS test_name;
-- Drop table first
DROP TABLE finance.accounting.reports.monthly_summary;

-- Drop schemas in reverse dependency order
DROP SCHEMA finance.accounting.reports.quarterly.q1.january;
DROP SCHEMA finance.accounting.reports.quarterly.q1;
DROP SCHEMA finance.accounting.reports.quarterly;
DROP SCHEMA finance.accounting.reports;
DROP SCHEMA finance.accounting;
DROP SCHEMA finance;

-- Verify cleanup
SELECT COUNT(*) AS remaining_test_schemas
FROM RDB$SCHEMAS 
WHERE RDB$SCHEMA_PATH LIKE 'finance%';

SELECT 'Hierarchical Schema Runtime Tests Complete' AS final_status;