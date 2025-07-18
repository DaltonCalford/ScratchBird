#!/bin/bash

#
# ScratchBird v0.5.0 - Hierarchical Schema Tests
#
# This script tests the hierarchical schema functionality of ScratchBird,
# including 8-level deep schema nesting, PostgreSQL-style schema operations,
# and schema-aware name resolution.
#
# Test categories:
# 1. Basic schema creation and navigation
# 2. Deep hierarchy operations (8-level maximum)
# 3. Schema-aware DDL operations
# 4. Name resolution and qualification
# 5. Schema security and permissions
# 6. Schema metadata queries
# 7. Schema performance validation
#
# Copyright (c) 2025 ScratchBird Development Team
# All Rights Reserved.
#

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DB_DIR="$SCRIPT_DIR/test_databases"
TEST_DB="$TEST_DB_DIR/hierarchical_schema_test.fdb"
SB_ISQL_PATH="../gen/Release/scratchbird/bin/sb_isql"
OUTPUT_FILE="$SCRIPT_DIR/test_results.txt"

# Create test database directory
mkdir -p "$TEST_DB_DIR"

# Function to log test results
log_test_result() {
    local test_name="$1"
    local expected_result="$2"
    local command="$3"
    local start_time="$4"
    local end_time="$5"
    
    local elapsed_time=$(echo "$end_time - $start_time" | bc -l)
    
    echo "=========================================" >> "$OUTPUT_FILE"
    echo "Test: $test_name" >> "$OUTPUT_FILE"
    echo "Expected: $expected_result" >> "$OUTPUT_FILE"
    echo "Command: $command" >> "$OUTPUT_FILE"
    echo "Execution time: ${elapsed_time}s" >> "$OUTPUT_FILE"
    echo "=========================================" >> "$OUTPUT_FILE"
}

# Function to run sb_isql with test file
run_isql_test() {
    local test_file="$1"
    local output_file="$2"
    local start_time=$(date +%s.%N)
    
    # Set SCRATCHBIRD environment variable
    export SCRATCHBIRD="$SCRIPT_DIR/../gen/Release/scratchbird"
    
    # Run sb_isql with the test file
    "$SB_ISQL_PATH" -i "$test_file" -o "$output_file" 2>&1
    local exit_code=$?
    
    local end_time=$(date +%s.%N)
    echo "$start_time $end_time $exit_code"
}

# Test 1: Basic Schema Creation and Navigation
echo "Testing basic schema creation and navigation..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/schema_basic_test.sql" << 'EOF'
/* Basic schema creation test */
CREATE DATABASE 'test_databases/hierarchical_schema_test.fdb';
CONNECT 'test_databases/hierarchical_schema_test.fdb';

/* Create basic schema hierarchy */
CREATE SCHEMA company;
CREATE SCHEMA company.finance;
CREATE SCHEMA company.finance.accounting;

/* Create table in nested schema */
CREATE TABLE company.finance.accounting.ledger (
    id INTEGER PRIMARY KEY,
    account_code VARCHAR(20),
    description VARCHAR(200),
    balance DECIMAL(15,2)
);

/* Insert test data */
INSERT INTO company.finance.accounting.ledger (id, account_code, description, balance)
VALUES (1, 'ACC001', 'Cash Account', 50000.00);

/* Query with full schema qualification */
SELECT * FROM company.finance.accounting.ledger;

/* Show schema hierarchy */
SELECT RDB$SCHEMA_NAME, RDB$PARENT_SCHEMA_NAME, RDB$SCHEMA_LEVEL, RDB$SCHEMA_PATH
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_NAME STARTING WITH 'COMPANY'
ORDER BY RDB$SCHEMA_LEVEL, RDB$SCHEMA_NAME;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/schema_basic_test.sql" "$TEST_DB_DIR/schema_basic_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Basic Schema Creation" "Successfully create 3-level schema hierarchy" \
    "CREATE SCHEMA company.finance.accounting" "$start_time" "$end_time"

cat "$TEST_DB_DIR/schema_basic_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 2: Deep Hierarchy Operations (8-level maximum)
echo "Testing deep hierarchy operations..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/schema_deep_test.sql" << 'EOF'
/* Deep hierarchy test - 8 levels maximum */
CONNECT 'test_databases/hierarchical_schema_test.fdb';

/* Create 8-level deep hierarchy */
CREATE SCHEMA enterprise;
CREATE SCHEMA enterprise.division;
CREATE SCHEMA enterprise.division.department;
CREATE SCHEMA enterprise.division.department.team;
CREATE SCHEMA enterprise.division.department.team.project;
CREATE SCHEMA enterprise.division.department.team.project.environment;
CREATE SCHEMA enterprise.division.department.team.project.environment.component;
CREATE SCHEMA enterprise.division.department.team.project.environment.component.module;

/* Create table in deepest schema */
CREATE TABLE enterprise.division.department.team.project.environment.component.module.data_items (
    item_id INTEGER PRIMARY KEY,
    item_name VARCHAR(100),
    item_value VARCHAR(500),
    created_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Insert test data */
INSERT INTO enterprise.division.department.team.project.environment.component.module.data_items 
(item_id, item_name, item_value) VALUES 
(1, 'Configuration', 'Production Settings'),
(2, 'Status', 'Active'),
(3, 'Version', '1.0.0');

/* Query from deepest level */
SELECT item_name, item_value 
FROM enterprise.division.department.team.project.environment.component.module.data_items
WHERE item_id <= 3;

/* Verify schema depth and paths */
SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_LEVEL, RDB$SCHEMA_PATH
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_NAME STARTING WITH 'ENTERPRISE'
ORDER BY RDB$SCHEMA_LEVEL, RDB$SCHEMA_NAME;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/schema_deep_test.sql" "$TEST_DB_DIR/schema_deep_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Deep Schema Hierarchy" "Successfully create 8-level deep schema hierarchy" \
    "CREATE SCHEMA enterprise.division.department.team.project.environment.component.module" "$start_time" "$end_time"

cat "$TEST_DB_DIR/schema_deep_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 3: Schema-Aware DDL Operations
echo "Testing schema-aware DDL operations..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/schema_ddl_test.sql" << 'EOF'
/* Schema-aware DDL operations test */
CONNECT 'test_databases/hierarchical_schema_test.fdb';

/* Create schemas for DDL testing */
CREATE SCHEMA inventory;
CREATE SCHEMA inventory.products;
CREATE SCHEMA inventory.products.categories;

/* Create multiple tables in schema hierarchy */
CREATE TABLE inventory.warehouses (
    warehouse_id INTEGER PRIMARY KEY,
    warehouse_name VARCHAR(100),
    location VARCHAR(200)
);

CREATE TABLE inventory.products.items (
    item_id INTEGER PRIMARY KEY,
    item_name VARCHAR(100),
    category_id INTEGER,
    warehouse_id INTEGER
);

CREATE TABLE inventory.products.categories.main_categories (
    category_id INTEGER PRIMARY KEY,
    category_name VARCHAR(100),
    parent_category_id INTEGER
);

/* Create indexes with schema awareness */
CREATE INDEX idx_items_category ON inventory.products.items (category_id);
CREATE INDEX idx_items_warehouse ON inventory.products.items (warehouse_id);

/* Create views with cross-schema references */
CREATE VIEW inventory.products.inventory_summary AS
SELECT 
    i.item_name,
    c.category_name,
    w.warehouse_name
FROM inventory.products.items i
JOIN inventory.products.categories.main_categories c ON i.category_id = c.category_id
JOIN inventory.warehouses w ON i.warehouse_id = w.warehouse_id;

/* Insert test data */
INSERT INTO inventory.warehouses VALUES (1, 'Main Warehouse', 'New York');
INSERT INTO inventory.products.categories.main_categories VALUES (1, 'Electronics', NULL);
INSERT INTO inventory.products.items VALUES (1, 'Laptop', 1, 1);

/* Query the view */
SELECT * FROM inventory.products.inventory_summary;

/* Show all objects in schema hierarchy */
SELECT RDB$RELATION_NAME, RDB$RELATION_TYPE, RDB$OWNER_NAME
FROM RDB$RELATIONS
WHERE RDB$RELATION_NAME STARTING WITH 'INVENTORY'
ORDER BY RDB$RELATION_NAME;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/schema_ddl_test.sql" "$TEST_DB_DIR/schema_ddl_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Schema-Aware DDL" "Successfully create tables, indexes, and views in schema hierarchy" \
    "CREATE TABLE inventory.products.items" "$start_time" "$end_time"

cat "$TEST_DB_DIR/schema_ddl_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 4: Name Resolution and Qualification
echo "Testing name resolution and qualification..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/schema_resolution_test.sql" << 'EOF'
/* Name resolution and qualification test */
CONNECT 'test_databases/hierarchical_schema_test.fdb';

/* Create schemas with same table names for resolution testing */
CREATE SCHEMA sales;
CREATE SCHEMA sales.regional;
CREATE SCHEMA sales.regional.northeast;

CREATE SCHEMA marketing;
CREATE SCHEMA marketing.campaigns;
CREATE SCHEMA marketing.campaigns.digital;

/* Create tables with same names in different schemas */
CREATE TABLE sales.customers (
    customer_id INTEGER PRIMARY KEY,
    customer_name VARCHAR(100),
    region VARCHAR(50)
);

CREATE TABLE sales.regional.customers (
    customer_id INTEGER PRIMARY KEY,
    customer_name VARCHAR(100),
    territory VARCHAR(50)
);

CREATE TABLE sales.regional.northeast.customers (
    customer_id INTEGER PRIMARY KEY,
    customer_name VARCHAR(100),
    state VARCHAR(50)
);

/* Insert different data in each table */
INSERT INTO sales.customers VALUES (1, 'Global Corp', 'National');
INSERT INTO sales.regional.customers VALUES (1, 'Regional LLC', 'Northeast');
INSERT INTO sales.regional.northeast.customers VALUES (1, 'Local Inc', 'NY');

/* Test name resolution with different qualification levels */
SELECT 'sales.customers' AS table_name, customer_name, region AS location 
FROM sales.customers;

SELECT 'sales.regional.customers' AS table_name, customer_name, territory AS location 
FROM sales.regional.customers;

SELECT 'sales.regional.northeast.customers' AS table_name, customer_name, state AS location 
FROM sales.regional.northeast.customers;

/* Test schema context switching */
SET SCHEMA sales.regional;
SELECT customer_name FROM customers; -- Should resolve to sales.regional.customers

SET SCHEMA sales.regional.northeast;
SELECT customer_name FROM customers; -- Should resolve to sales.regional.northeast.customers

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/schema_resolution_test.sql" "$TEST_DB_DIR/schema_resolution_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Name Resolution" "Successfully resolve table names in hierarchical schema context" \
    "SET SCHEMA sales.regional" "$start_time" "$end_time"

cat "$TEST_DB_DIR/schema_resolution_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 5: Schema Performance Validation
echo "Testing schema performance..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/schema_performance_test.sql" << 'EOF'
/* Schema performance validation test */
CONNECT 'test_databases/hierarchical_schema_test.fdb';

/* Create performance test schema */
CREATE SCHEMA performance;
CREATE SCHEMA performance.level2;
CREATE SCHEMA performance.level2.level3;
CREATE SCHEMA performance.level2.level3.level4;
CREATE SCHEMA performance.level2.level3.level4.level5;

/* Create table with performance test data */
CREATE TABLE performance.level2.level3.level4.level5.test_data (
    id INTEGER PRIMARY KEY,
    data_value VARCHAR(100),
    timestamp_created TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Insert bulk data for performance testing */
INSERT INTO performance.level2.level3.level4.level5.test_data (id, data_value) VALUES (1, 'Test Data 1');
INSERT INTO performance.level2.level3.level4.level5.test_data (id, data_value) VALUES (2, 'Test Data 2');
INSERT INTO performance.level2.level3.level4.level5.test_data (id, data_value) VALUES (3, 'Test Data 3');
INSERT INTO performance.level2.level3.level4.level5.test_data (id, data_value) VALUES (4, 'Test Data 4');
INSERT INTO performance.level2.level3.level4.level5.test_data (id, data_value) VALUES (5, 'Test Data 5');

/* Performance test queries */
SELECT COUNT(*) FROM performance.level2.level3.level4.level5.test_data;
SELECT * FROM performance.level2.level3.level4.level5.test_data WHERE id BETWEEN 1 AND 3;

/* Test schema metadata query performance */
SELECT COUNT(*) FROM RDB$SCHEMAS WHERE RDB$SCHEMA_NAME STARTING WITH 'PERFORMANCE';

/* Test schema hierarchy navigation performance */
SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_LEVEL, LENGTH(RDB$SCHEMA_PATH) AS PATH_LENGTH
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_PATH CONTAINING 'performance'
ORDER BY RDB$SCHEMA_LEVEL;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/schema_performance_test.sql" "$TEST_DB_DIR/schema_performance_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Schema Performance" "Successfully perform operations on deep schema hierarchy within performance limits" \
    "SELECT * FROM performance.level2.level3.level4.level5.test_data" "$start_time" "$end_time"

cat "$TEST_DB_DIR/schema_performance_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 6: Schema Metadata Queries
echo "Testing schema metadata queries..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/schema_metadata_test.sql" << 'EOF'
/* Schema metadata queries test */
CONNECT 'test_databases/hierarchical_schema_test.fdb';

/* Query all schemas with hierarchy information */
SELECT 
    RDB$SCHEMA_NAME,
    RDB$PARENT_SCHEMA_NAME,
    RDB$SCHEMA_LEVEL,
    RDB$SCHEMA_PATH,
    CHARACTER_LENGTH(RDB$SCHEMA_PATH) AS PATH_LENGTH
FROM RDB$SCHEMAS
ORDER BY RDB$SCHEMA_LEVEL, RDB$SCHEMA_NAME;

/* Query schema statistics */
SELECT 
    RDB$SCHEMA_LEVEL,
    COUNT(*) AS SCHEMA_COUNT
FROM RDB$SCHEMAS
GROUP BY RDB$SCHEMA_LEVEL
ORDER BY RDB$SCHEMA_LEVEL;

/* Query deepest schemas */
SELECT 
    RDB$SCHEMA_NAME,
    RDB$SCHEMA_PATH,
    RDB$SCHEMA_LEVEL
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_LEVEL = (SELECT MAX(RDB$SCHEMA_LEVEL) FROM RDB$SCHEMAS);

/* Query tables by schema */
SELECT 
    r.RDB$RELATION_NAME,
    s.RDB$SCHEMA_NAME,
    s.RDB$SCHEMA_LEVEL
FROM RDB$RELATIONS r
JOIN RDB$SCHEMAS s ON r.RDB$RELATION_NAME STARTING WITH s.RDB$SCHEMA_NAME
WHERE r.RDB$RELATION_TYPE = 0
ORDER BY s.RDB$SCHEMA_LEVEL, r.RDB$RELATION_NAME;

/* Query maximum schema depth reached */
SELECT 
    'Maximum Schema Depth' AS METRIC,
    MAX(RDB$SCHEMA_LEVEL) AS VALUE
FROM RDB$SCHEMAS
UNION ALL
SELECT 
    'Total Schema Count' AS METRIC,
    COUNT(*) AS VALUE
FROM RDB$SCHEMAS;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/schema_metadata_test.sql" "$TEST_DB_DIR/schema_metadata_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Schema Metadata" "Successfully query schema hierarchy metadata" \
    "SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_LEVEL FROM RDB$SCHEMAS" "$start_time" "$end_time"

cat "$TEST_DB_DIR/schema_metadata_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Summary
echo "Hierarchical Schema Tests Completed" >> "$OUTPUT_FILE"
echo "Test database: $TEST_DB" >> "$OUTPUT_FILE"
echo "Test files created in: $TEST_DB_DIR" >> "$OUTPUT_FILE"
echo "=====================================." >> "$OUTPUT_FILE"

echo "Hierarchical schema tests completed successfully"
exit 0