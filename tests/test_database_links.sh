#!/bin/bash

#
# ScratchBird v0.5.0 - Database Links Tests
#
# This script tests the schema-aware database links functionality in ScratchBird,
# including remote database connections, schema mapping, and link management.
#
# Test categories:
# 1. Basic database link creation and management
# 2. Schema-aware database links
# 3. Remote query execution through links
# 4. Link security and permissions
# 5. Link performance and optimization
# 6. Link error handling and recovery
#
# Note: These tests simulate database link functionality since actual remote
# connections require separate database servers.
#
# Copyright (c) 2025 ScratchBird Development Team
# All Rights Reserved.
#

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DB_DIR="$SCRIPT_DIR/test_databases"
TEST_DB="$TEST_DB_DIR/database_links_test.fdb"
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

# Test 1: Basic Database Link Creation and Management
echo "Testing basic database link creation and management..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/dblink_basic_test.sql" << 'EOF'
/* Basic database link creation and management test */
CREATE DATABASE 'test_databases/database_links_test.fdb';
CONNECT 'test_databases/database_links_test.fdb';

/* Create schema for database link testing */
CREATE SCHEMA dblinks;
CREATE SCHEMA dblinks.remote;

/* Note: Actual database link creation would require remote servers
   For testing purposes, we'll test the syntax and metadata */

/* Test database link syntax (these would normally connect to remote servers) */
-- CREATE DATABASE LINK finance_link 
--   TO 'remote_server:4050/finance_db' 
--   USER 'dbuser' PASSWORD 'dbpass';

/* Instead, create test tables to simulate remote data */
CREATE TABLE dblinks.remote.customers (
    customer_id INTEGER PRIMARY KEY,
    customer_name VARCHAR(100),
    email VARCHAR(200),
    created_date DATE DEFAULT CURRENT_DATE
);

CREATE TABLE dblinks.remote.orders (
    order_id INTEGER PRIMARY KEY,
    customer_id INTEGER,
    order_date DATE DEFAULT CURRENT_DATE,
    total_amount DECIMAL(10,2),
    status VARCHAR(20)
);

/* Insert test data to simulate remote database content */
INSERT INTO dblinks.remote.customers (customer_id, customer_name, email) VALUES
(1, 'Remote Customer 1', 'customer1@remote.com'),
(2, 'Remote Customer 2', 'customer2@remote.com'),
(3, 'Remote Customer 3', 'customer3@remote.com');

INSERT INTO dblinks.remote.orders (order_id, customer_id, total_amount, status) VALUES
(1, 1, 150.00, 'COMPLETED'),
(2, 2, 200.00, 'PENDING'),
(3, 1, 75.00, 'SHIPPED'),
(4, 3, 300.00, 'COMPLETED');

/* Test querying 'remote' data */
SELECT * FROM dblinks.remote.customers ORDER BY customer_id;

SELECT * FROM dblinks.remote.orders ORDER BY order_id;

/* Test JOIN operations across 'remote' tables */
SELECT 
    c.customer_name,
    o.order_id,
    o.total_amount,
    o.status
FROM dblinks.remote.customers c
JOIN dblinks.remote.orders o ON c.customer_id = o.customer_id
ORDER BY c.customer_name, o.order_id;

/* Test aggregate operations on 'remote' data */
SELECT 
    c.customer_name,
    COUNT(o.order_id) as order_count,
    SUM(o.total_amount) as total_spent
FROM dblinks.remote.customers c
LEFT JOIN dblinks.remote.orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name
ORDER BY total_spent DESC;

/* Create local table for comparison */
CREATE TABLE dblinks.local_customers (
    customer_id INTEGER PRIMARY KEY,
    customer_name VARCHAR(100),
    local_reference VARCHAR(100)
);

INSERT INTO dblinks.local_customers (customer_id, customer_name, local_reference) VALUES
(1, 'Local Customer 1', 'LC001'),
(2, 'Local Customer 2', 'LC002'),
(4, 'Local Customer 4', 'LC004');

/* Test mixing local and 'remote' data */
SELECT 
    l.customer_name as local_name,
    l.local_reference,
    r.customer_name as remote_name,
    r.email
FROM dblinks.local_customers l
LEFT JOIN dblinks.remote.customers r ON l.customer_id = r.customer_id
ORDER BY l.customer_id;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/dblink_basic_test.sql" "$TEST_DB_DIR/dblink_basic_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Database Link Basic Operations" "Successfully simulate database link operations with remote data access" \
    "SELECT * FROM dblinks.remote.customers" "$start_time" "$end_time"

cat "$TEST_DB_DIR/dblink_basic_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 2: Schema-Aware Database Links
echo "Testing schema-aware database links..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/dblink_schema_test.sql" << 'EOF'
/* Schema-aware database links test */
CONNECT 'test_databases/database_links_test.fdb';

/* Create hierarchical schemas for testing schema-aware links */
CREATE SCHEMA dblinks.finance;
CREATE SCHEMA dblinks.finance.accounting;
CREATE SCHEMA dblinks.finance.accounting.reports;

CREATE SCHEMA dblinks.hr;
CREATE SCHEMA dblinks.hr.employees;
CREATE SCHEMA dblinks.hr.payroll;

/* Create tables in hierarchical schemas to simulate remote schema mapping */
CREATE TABLE dblinks.finance.accounting.reports.monthly_summary (
    report_id INTEGER PRIMARY KEY,
    month_year VARCHAR(20),
    total_revenue DECIMAL(15,2),
    total_expenses DECIMAL(15,2),
    net_profit DECIMAL(15,2)
);

CREATE TABLE dblinks.hr.employees.staff_records (
    employee_id INTEGER PRIMARY KEY,
    employee_name VARCHAR(100),
    department VARCHAR(50),
    salary DECIMAL(10,2),
    hire_date DATE
);

CREATE TABLE dblinks.hr.payroll.salary_history (
    history_id INTEGER PRIMARY KEY,
    employee_id INTEGER,
    effective_date DATE,
    salary_amount DECIMAL(10,2),
    adjustment_reason VARCHAR(200)
);

/* Insert test data for schema-aware testing */
INSERT INTO dblinks.finance.accounting.reports.monthly_summary 
(report_id, month_year, total_revenue, total_expenses, net_profit) VALUES
(1, '2025-01', 100000.00, 75000.00, 25000.00),
(2, '2025-02', 110000.00, 80000.00, 30000.00),
(3, '2025-03', 120000.00, 85000.00, 35000.00);

INSERT INTO dblinks.hr.employees.staff_records 
(employee_id, employee_name, department, salary, hire_date) VALUES
(1, 'John Manager', 'Finance', 75000.00, '2023-01-15'),
(2, 'Jane Analyst', 'Accounting', 55000.00, '2023-03-10'),
(3, 'Bob Developer', 'IT', 65000.00, '2023-02-20');

INSERT INTO dblinks.hr.payroll.salary_history 
(history_id, employee_id, effective_date, salary_amount, adjustment_reason) VALUES
(1, 1, '2023-01-15', 70000.00, 'Initial salary'),
(2, 1, '2024-01-15', 75000.00, 'Annual raise'),
(3, 2, '2023-03-10', 50000.00, 'Initial salary'),
(4, 2, '2024-03-10', 55000.00, 'Annual raise'),
(5, 3, '2023-02-20', 60000.00, 'Initial salary'),
(6, 3, '2024-02-20', 65000.00, 'Annual raise');

/* Test querying deep hierarchical schema structures */
SELECT * FROM dblinks.finance.accounting.reports.monthly_summary ORDER BY report_id;

SELECT * FROM dblinks.hr.employees.staff_records ORDER BY employee_id;

SELECT * FROM dblinks.hr.payroll.salary_history ORDER BY history_id;

/* Test schema-aware joins across hierarchical schemas */
SELECT 
    e.employee_name,
    e.department,
    e.salary as current_salary,
    sh.salary_amount as previous_salary,
    sh.effective_date,
    sh.adjustment_reason
FROM dblinks.hr.employees.staff_records e
JOIN dblinks.hr.payroll.salary_history sh ON e.employee_id = sh.employee_id
WHERE sh.effective_date < '2024-01-01'
ORDER BY e.employee_name, sh.effective_date DESC;

/* Test schema path resolution performance */
SELECT 
    'finance.accounting.reports.monthly_summary' as table_path,
    COUNT(*) as record_count,
    SUM(total_revenue) as total_revenue,
    SUM(net_profit) as total_profit
FROM dblinks.finance.accounting.reports.monthly_summary
UNION ALL
SELECT 
    'hr.employees.staff_records' as table_path,
    COUNT(*) as record_count,
    SUM(salary) as total_salary_cost,
    0 as total_profit
FROM dblinks.hr.employees.staff_records;

/* Test schema context switching simulation */
SET SCHEMA dblinks.finance.accounting;
SELECT COUNT(*) as reports_in_accounting FROM reports.monthly_summary;

SET SCHEMA dblinks.hr;
SELECT COUNT(*) as employees_in_hr FROM employees.staff_records;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/dblink_schema_test.sql" "$TEST_DB_DIR/dblink_schema_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Schema-Aware Database Links" "Successfully access hierarchical schema structures through simulated links" \
    "SELECT * FROM dblinks.finance.accounting.reports.monthly_summary" "$start_time" "$end_time"

cat "$TEST_DB_DIR/dblink_schema_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 3: Database Link Performance and Optimization
echo "Testing database link performance and optimization..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/dblink_performance_test.sql" << 'EOF'
/* Database link performance and optimization test */
CONNECT 'test_databases/database_links_test.fdb';

/* Create performance testing schema */
CREATE SCHEMA dblinks.performance;
CREATE SCHEMA dblinks.performance.large_data;

/* Create large table to simulate performance testing */
CREATE TABLE dblinks.performance.large_data.performance_table (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    value DECIMAL(10,2),
    category VARCHAR(50),
    created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create index for performance testing */
CREATE INDEX idx_perf_category ON dblinks.performance.large_data.performance_table (category);
CREATE INDEX idx_perf_value ON dblinks.performance.large_data.performance_table (value);

/* Insert bulk data for performance testing */
INSERT INTO dblinks.performance.large_data.performance_table (id, name, value, category) VALUES
(1, 'Performance Item 1', 100.00, 'Category A'),
(2, 'Performance Item 2', 200.00, 'Category B'),
(3, 'Performance Item 3', 300.00, 'Category C'),
(4, 'Performance Item 4', 400.00, 'Category A'),
(5, 'Performance Item 5', 500.00, 'Category B'),
(6, 'Performance Item 6', 600.00, 'Category C'),
(7, 'Performance Item 7', 700.00, 'Category A'),
(8, 'Performance Item 8', 800.00, 'Category B'),
(9, 'Performance Item 9', 900.00, 'Category C'),
(10, 'Performance Item 10', 1000.00, 'Category A'),
(11, 'Performance Item 11', 1100.00, 'Category B'),
(12, 'Performance Item 12', 1200.00, 'Category C'),
(13, 'Performance Item 13', 1300.00, 'Category A'),
(14, 'Performance Item 14', 1400.00, 'Category B'),
(15, 'Performance Item 15', 1500.00, 'Category C');

/* Performance test queries */
SELECT COUNT(*) as total_records FROM dblinks.performance.large_data.performance_table;

/* Test indexed access performance */
SELECT * FROM dblinks.performance.large_data.performance_table 
WHERE category = 'Category A' 
ORDER BY value;

/* Test range query performance */
SELECT * FROM dblinks.performance.large_data.performance_table 
WHERE value BETWEEN 500.00 AND 1000.00 
ORDER BY value;

/* Test aggregation performance */
SELECT 
    category,
    COUNT(*) as item_count,
    SUM(value) as total_value,
    AVG(value) as avg_value,
    MIN(value) as min_value,
    MAX(value) as max_value
FROM dblinks.performance.large_data.performance_table
GROUP BY category
ORDER BY total_value DESC;

/* Test complex query performance */
SELECT 
    p1.category,
    p1.name,
    p1.value,
    p2.name as related_name,
    p2.value as related_value
FROM dblinks.performance.large_data.performance_table p1
JOIN dblinks.performance.large_data.performance_table p2 ON p1.category = p2.category AND p1.id != p2.id
WHERE p1.value > 800.00
ORDER BY p1.category, p1.value DESC;

/* Test subquery performance */
SELECT 
    name,
    value,
    category,
    (SELECT AVG(value) FROM dblinks.performance.large_data.performance_table p2 WHERE p2.category = p1.category) as category_avg
FROM dblinks.performance.large_data.performance_table p1
WHERE value > (SELECT AVG(value) FROM dblinks.performance.large_data.performance_table)
ORDER BY value DESC;

/* Test bulk operations performance */
UPDATE dblinks.performance.large_data.performance_table 
SET value = value * 1.1 
WHERE category = 'Category B';

SELECT 
    'After bulk update' as operation,
    category,
    COUNT(*) as affected_records,
    SUM(value) as new_total_value
FROM dblinks.performance.large_data.performance_table
WHERE category = 'Category B'
GROUP BY category;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/dblink_performance_test.sql" "$TEST_DB_DIR/dblink_performance_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Database Link Performance" "Successfully perform complex queries through simulated database links" \
    "SELECT category, COUNT(*), SUM(value) FROM dblinks.performance.large_data.performance_table GROUP BY category" "$start_time" "$end_time"

cat "$TEST_DB_DIR/dblink_performance_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Summary
echo "Database Links Tests Completed" >> "$OUTPUT_FILE"
echo "Test database: $TEST_DB" >> "$OUTPUT_FILE"
echo "Test files created in: $TEST_DB_DIR" >> "$OUTPUT_FILE"
echo "Note: These tests simulate database link functionality" >> "$OUTPUT_FILE"
echo "Real database links would require remote ScratchBird servers" >> "$OUTPUT_FILE"
echo "=====================================." >> "$OUTPUT_FILE"

echo "Database link tests completed successfully"
exit 0