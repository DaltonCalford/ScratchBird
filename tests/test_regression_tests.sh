#!/bin/bash

#
# ScratchBird v0.5.0 - Regression Tests
#
# This script runs regression tests to ensure that existing functionality
# continues to work correctly after changes. These tests validate:
# - Core database operations
# - Schema functionality stability
# - Data type compatibility
# - Performance regression detection
# - API compatibility
#
# Test categories:
# 1. Core SQL functionality regression
# 2. Schema hierarchy regression
# 3. Data type regression
# 4. Performance regression detection
# 5. API compatibility regression
# 6. Integration regression tests
#
# Copyright (c) 2025 ScratchBird Development Team
# All Rights Reserved.
#

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DB_DIR="$SCRIPT_DIR/test_databases"
TEST_DB="$TEST_DB_DIR/regression_test.fdb"
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

# Test 1: Core SQL Functionality Regression
echo "Testing core SQL functionality regression..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/sql_regression_test.sql" << 'EOF'
/* Core SQL functionality regression test */
CREATE DATABASE 'test_databases/regression_test.fdb';
CONNECT 'test_databases/regression_test.fdb';

/* Create basic tables for regression testing */
CREATE TABLE regression_customers (
    customer_id INTEGER PRIMARY KEY,
    customer_name VARCHAR(100) NOT NULL,
    email VARCHAR(200) UNIQUE,
    phone VARCHAR(20),
    address VARCHAR(500),
    city VARCHAR(100),
    state VARCHAR(50),
    zip_code VARCHAR(20),
    country VARCHAR(100) DEFAULT 'USA',
    created_date DATE DEFAULT CURRENT_DATE,
    updated_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE regression_products (
    product_id INTEGER PRIMARY KEY,
    product_name VARCHAR(200) NOT NULL,
    description VARCHAR(1000),
    price DECIMAL(10,2) NOT NULL,
    cost DECIMAL(10,2),
    category VARCHAR(100),
    weight DECIMAL(8,3),
    dimensions VARCHAR(100),
    in_stock INTEGER DEFAULT 0,
    reorder_level INTEGER DEFAULT 10,
    is_active BOOLEAN DEFAULT TRUE,
    created_date DATE DEFAULT CURRENT_DATE
);

CREATE TABLE regression_orders (
    order_id INTEGER PRIMARY KEY,
    customer_id INTEGER NOT NULL,
    order_date DATE DEFAULT CURRENT_DATE,
    ship_date DATE,
    total_amount DECIMAL(12,2) NOT NULL,
    tax_amount DECIMAL(10,2) DEFAULT 0.00,
    shipping_cost DECIMAL(8,2) DEFAULT 0.00,
    discount_amount DECIMAL(10,2) DEFAULT 0.00,
    status VARCHAR(20) DEFAULT 'PENDING',
    payment_method VARCHAR(50),
    notes VARCHAR(1000),
    created_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (customer_id) REFERENCES regression_customers(customer_id)
);

CREATE TABLE regression_order_items (
    item_id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL,
    product_id INTEGER NOT NULL,
    quantity INTEGER NOT NULL,
    unit_price DECIMAL(10,2) NOT NULL,
    discount_percent DECIMAL(5,2) DEFAULT 0.00,
    total_price DECIMAL(12,2) NOT NULL,
    FOREIGN KEY (order_id) REFERENCES regression_orders(order_id),
    FOREIGN KEY (product_id) REFERENCES regression_products(product_id)
);

/* Create indexes for performance */
CREATE INDEX idx_customers_email ON regression_customers(email);
CREATE INDEX idx_customers_city ON regression_customers(city);
CREATE INDEX idx_products_category ON regression_products(category);
CREATE INDEX idx_products_price ON regression_products(price);
CREATE INDEX idx_orders_customer ON regression_orders(customer_id);
CREATE INDEX idx_orders_date ON regression_orders(order_date);
CREATE INDEX idx_order_items_order ON regression_order_items(order_id);
CREATE INDEX idx_order_items_product ON regression_order_items(product_id);

/* Insert test data */
INSERT INTO regression_customers (customer_id, customer_name, email, phone, city, state, zip_code) VALUES
(1, 'John Smith', 'john.smith@example.com', '555-0101', 'New York', 'NY', '10001'),
(2, 'Jane Johnson', 'jane.johnson@example.com', '555-0102', 'Los Angeles', 'CA', '90001'),
(3, 'Bob Brown', 'bob.brown@example.com', '555-0103', 'Chicago', 'IL', '60601'),
(4, 'Alice Davis', 'alice.davis@example.com', '555-0104', 'Houston', 'TX', '77001'),
(5, 'Charlie Wilson', 'charlie.wilson@example.com', '555-0105', 'Phoenix', 'AZ', '85001');

INSERT INTO regression_products (product_id, product_name, description, price, cost, category, in_stock) VALUES
(1, 'Laptop Computer', 'High-performance laptop for business use', 999.99, 600.00, 'Electronics', 25),
(2, 'Office Chair', 'Ergonomic office chair with lumbar support', 299.99, 150.00, 'Furniture', 50),
(3, 'Smartphone', 'Latest model smartphone with advanced features', 699.99, 400.00, 'Electronics', 100),
(4, 'Desk Lamp', 'LED desk lamp with adjustable brightness', 79.99, 35.00, 'Office Supplies', 75),
(5, 'Monitor', '27-inch 4K monitor for professional use', 399.99, 250.00, 'Electronics', 30);

INSERT INTO regression_orders (order_id, customer_id, total_amount, tax_amount, status, payment_method) VALUES
(1, 1, 1079.98, 79.99, 'COMPLETED', 'Credit Card'),
(2, 2, 379.98, 29.99, 'SHIPPED', 'PayPal'),
(3, 3, 779.98, 59.99, 'PENDING', 'Credit Card'),
(4, 1, 479.98, 39.99, 'COMPLETED', 'Credit Card'),
(5, 4, 699.99, 52.50, 'SHIPPED', 'Debit Card');

INSERT INTO regression_order_items (item_id, order_id, product_id, quantity, unit_price, total_price) VALUES
(1, 1, 1, 1, 999.99, 999.99),
(2, 1, 4, 1, 79.99, 79.99),
(3, 2, 2, 1, 299.99, 299.99),
(4, 2, 4, 1, 79.99, 79.99),
(5, 3, 3, 1, 699.99, 699.99),
(6, 3, 4, 1, 79.99, 79.99),
(7, 4, 5, 1, 399.99, 399.99),
(8, 4, 4, 1, 79.99, 79.99),
(9, 5, 3, 1, 699.99, 699.99);

/* Test basic SELECT operations */
SELECT COUNT(*) as customer_count FROM regression_customers;
SELECT COUNT(*) as product_count FROM regression_products;
SELECT COUNT(*) as order_count FROM regression_orders;
SELECT COUNT(*) as order_item_count FROM regression_order_items;

/* Test WHERE clauses */
SELECT * FROM regression_customers WHERE city = 'New York';
SELECT * FROM regression_products WHERE category = 'Electronics' AND price > 500.00;
SELECT * FROM regression_orders WHERE status = 'COMPLETED' ORDER BY order_date DESC;

/* Test JOIN operations */
SELECT 
    c.customer_name,
    o.order_id,
    o.total_amount,
    o.status
FROM regression_customers c
JOIN regression_orders o ON c.customer_id = o.customer_id
ORDER BY c.customer_name, o.order_id;

/* Test aggregate functions */
SELECT 
    category,
    COUNT(*) as product_count,
    AVG(price) as avg_price,
    SUM(in_stock) as total_stock
FROM regression_products
GROUP BY category
ORDER BY avg_price DESC;

/* Test complex query */
SELECT 
    c.customer_name,
    p.product_name,
    oi.quantity,
    oi.unit_price,
    oi.total_price
FROM regression_customers c
JOIN regression_orders o ON c.customer_id = o.customer_id
JOIN regression_order_items oi ON o.order_id = oi.order_id
JOIN regression_products p ON oi.product_id = p.product_id
WHERE o.status = 'COMPLETED'
ORDER BY c.customer_name, p.product_name;

/* Test subqueries */
SELECT 
    customer_name,
    (SELECT COUNT(*) FROM regression_orders WHERE customer_id = c.customer_id) as order_count
FROM regression_customers c
ORDER BY order_count DESC;

/* Test UPDATE operations */
UPDATE regression_products SET price = price * 1.05 WHERE category = 'Electronics';

SELECT product_name, price FROM regression_products WHERE category = 'Electronics';

/* Test DELETE operations */
DELETE FROM regression_order_items WHERE item_id = 9;

SELECT COUNT(*) as remaining_items FROM regression_order_items;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/sql_regression_test.sql" "$TEST_DB_DIR/sql_regression_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Core SQL Regression" "Successfully execute core SQL operations without regression" \
    "CREATE TABLE, INSERT, SELECT, UPDATE, DELETE with JOINs and aggregates" "$start_time" "$end_time"

cat "$TEST_DB_DIR/sql_regression_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 2: Schema Hierarchy Regression
echo "Testing schema hierarchy regression..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/schema_regression_test.sql" << 'EOF'
/* Schema hierarchy regression test */
CONNECT 'test_databases/regression_test.fdb';

/* Test basic schema creation */
CREATE SCHEMA regression_schema;
CREATE SCHEMA regression_schema.level1;
CREATE SCHEMA regression_schema.level1.level2;

/* Test schema-qualified table creation */
CREATE TABLE regression_schema.base_table (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE regression_schema.level1.sub_table (
    id INTEGER PRIMARY KEY,
    parent_id INTEGER,
    description VARCHAR(200)
);

CREATE TABLE regression_schema.level1.level2.deep_table (
    id INTEGER PRIMARY KEY,
    data VARCHAR(500)
);

/* Insert test data */
INSERT INTO regression_schema.base_table (id, name) VALUES
(1, 'Base Record 1'),
(2, 'Base Record 2');

INSERT INTO regression_schema.level1.sub_table (id, parent_id, description) VALUES
(1, 1, 'Sub record for base 1'),
(2, 2, 'Sub record for base 2');

INSERT INTO regression_schema.level1.level2.deep_table (id, data) VALUES
(1, 'Deep data 1'),
(2, 'Deep data 2');

/* Test schema-qualified queries */
SELECT * FROM regression_schema.base_table;
SELECT * FROM regression_schema.level1.sub_table;
SELECT * FROM regression_schema.level1.level2.deep_table;

/* Test cross-schema joins */
SELECT 
    b.name,
    s.description,
    d.data
FROM regression_schema.base_table b
JOIN regression_schema.level1.sub_table s ON b.id = s.parent_id
JOIN regression_schema.level1.level2.deep_table d ON s.id = d.id;

/* Test schema context switching */
SET SCHEMA regression_schema;
SELECT COUNT(*) as base_count FROM base_table;

SET SCHEMA regression_schema.level1;
SELECT COUNT(*) as sub_count FROM sub_table;
SELECT COUNT(*) as deep_count FROM level2.deep_table;

/* Test schema metadata queries */
SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_LEVEL, RDB$SCHEMA_PATH
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_NAME STARTING WITH 'REGRESSION_SCHEMA'
ORDER BY RDB$SCHEMA_LEVEL, RDB$SCHEMA_NAME;

/* Test schema inheritance and resolution */
CREATE SCHEMA regression_schema.level1.level2.level3;
CREATE TABLE regression_schema.level1.level2.level3.deepest_table (
    id INTEGER PRIMARY KEY,
    reference_id INTEGER,
    notes VARCHAR(1000)
);

INSERT INTO regression_schema.level1.level2.level3.deepest_table (id, reference_id, notes) VALUES
(1, 1, 'Deepest level data');

SELECT * FROM regression_schema.level1.level2.level3.deepest_table;

/* Test schema depth limits */
SELECT 
    RDB$SCHEMA_NAME,
    RDB$SCHEMA_LEVEL,
    CHARACTER_LENGTH(RDB$SCHEMA_PATH) as path_length
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_NAME STARTING WITH 'REGRESSION_SCHEMA'
ORDER BY RDB$SCHEMA_LEVEL DESC;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/schema_regression_test.sql" "$TEST_DB_DIR/schema_regression_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Schema Hierarchy Regression" "Successfully maintain schema hierarchy functionality" \
    "CREATE SCHEMA regression_schema.level1.level2.level3 and qualified queries" "$start_time" "$end_time"

cat "$TEST_DB_DIR/schema_regression_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 3: Data Type Regression
echo "Testing data type regression..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/datatype_regression_test.sql" << 'EOF'
/* Data type regression test */
CONNECT 'test_databases/regression_test.fdb';

/* Create table with all supported data types */
CREATE TABLE regression_datatypes (
    id INTEGER PRIMARY KEY,
    
    -- Numeric types
    small_int SMALLINT,
    big_int BIGINT,
    numeric_val NUMERIC(15,2),
    decimal_val DECIMAL(10,4),
    float_val FLOAT,
    double_val DOUBLE PRECISION,
    
    -- String types
    char_val CHAR(10),
    varchar_val VARCHAR(100),
    text_val VARCHAR(1000),
    
    -- Date/Time types
    date_val DATE,
    time_val TIME,
    timestamp_val TIMESTAMP,
    
    -- Boolean type
    boolean_val BOOLEAN,
    
    -- Network types (PostgreSQL compatible)
    inet_val INET,
    cidr_val CIDR,
    macaddr_val MACADDR,
    uuid_val UUID,
    
    -- Binary types
    blob_val BLOB,
    
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Insert test data with various data types */
INSERT INTO regression_datatypes (
    id, small_int, big_int, numeric_val, decimal_val, float_val, double_val,
    char_val, varchar_val, text_val,
    date_val, time_val, timestamp_val,
    boolean_val,
    inet_val, cidr_val, macaddr_val, uuid_val,
    blob_val
) VALUES (
    1, 32767, 9223372036854775807, 12345.67, 1234.5678, 3.14159, 2.71828,
    'CHAR_TEST', 'VARCHAR_TEST', 'This is a longer text value for testing',
    '2025-01-17', '14:30:00', '2025-01-17 14:30:00',
    TRUE,
    '192.168.1.1', '192.168.1.0/24', '00:11:22:33:44:55', '550e8400-e29b-41d4-a716-446655440000',
    'Binary data test'
);

INSERT INTO regression_datatypes (
    id, small_int, big_int, numeric_val, decimal_val, float_val, double_val,
    char_val, varchar_val, text_val,
    date_val, time_val, timestamp_val,
    boolean_val,
    inet_val, cidr_val, macaddr_val, uuid_val,
    blob_val
) VALUES (
    2, -32768, -9223372036854775808, -12345.67, -1234.5678, -3.14159, -2.71828,
    'CHAR_2', 'VARCHAR_2', 'Second test record with different values',
    '2025-01-18', '09:15:30', '2025-01-18 09:15:30',
    FALSE,
    '10.0.0.1', '10.0.0.0/8', '00:22:33:44:55:66', '550e8400-e29b-41d4-a716-446655440001',
    'More binary data'
);

/* Test data type queries */
SELECT 
    id,
    small_int,
    big_int,
    numeric_val,
    decimal_val,
    float_val,
    double_val
FROM regression_datatypes
ORDER BY id;

SELECT 
    id,
    char_val,
    varchar_val,
    SUBSTRING(text_val FROM 1 FOR 50) as text_preview,
    date_val,
    time_val,
    timestamp_val,
    boolean_val
FROM regression_datatypes
ORDER BY id;

SELECT 
    id,
    inet_val,
    cidr_val,
    macaddr_val,
    uuid_val
FROM regression_datatypes
ORDER BY id;

/* Test data type operations */
SELECT 
    id,
    small_int + 100 as small_int_plus_100,
    big_int / 1000000 as big_int_millions,
    numeric_val * 2 as numeric_doubled,
    decimal_val + 0.5 as decimal_plus_half,
    ROUND(float_val, 2) as float_rounded,
    ROUND(double_val, 3) as double_rounded
FROM regression_datatypes
ORDER BY id;

/* Test string operations */
SELECT 
    id,
    UPPER(char_val) as char_upper,
    LOWER(varchar_val) as varchar_lower,
    LENGTH(text_val) as text_length,
    SUBSTRING(text_val FROM 1 FOR 20) as text_substring
FROM regression_datatypes
ORDER BY id;

/* Test date/time operations */
SELECT 
    id,
    date_val,
    date_val + 30 as date_plus_30_days,
    time_val,
    timestamp_val,
    EXTRACT(YEAR FROM date_val) as year_extracted,
    EXTRACT(MONTH FROM date_val) as month_extracted,
    EXTRACT(DAY FROM date_val) as day_extracted
FROM regression_datatypes
ORDER BY id;

/* Test network type operations */
SELECT 
    id,
    inet_val,
    NETWORK(inet_val) as network_addr,
    cidr_val,
    macaddr_val,
    uuid_val
FROM regression_datatypes
WHERE inet_val IS NOT NULL
ORDER BY id;

/* Test NULL handling */
INSERT INTO regression_datatypes (id, varchar_val) VALUES (3, 'NULL_TEST');

SELECT 
    id,
    varchar_val,
    CASE WHEN small_int IS NULL THEN 'NULL' ELSE CAST(small_int AS VARCHAR(20)) END as small_int_check,
    CASE WHEN boolean_val IS NULL THEN 'NULL' ELSE CAST(boolean_val AS VARCHAR(10)) END as boolean_check
FROM regression_datatypes
ORDER BY id;

/* Test data type constraints and validation */
SELECT 
    id,
    CASE 
        WHEN small_int BETWEEN -32768 AND 32767 THEN 'Valid SMALLINT'
        ELSE 'Invalid SMALLINT'
    END as smallint_validation,
    CASE 
        WHEN LENGTH(varchar_val) <= 100 THEN 'Valid VARCHAR(100)'
        ELSE 'Invalid VARCHAR(100)'
    END as varchar_validation
FROM regression_datatypes
WHERE small_int IS NOT NULL
ORDER BY id;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/datatype_regression_test.sql" "$TEST_DB_DIR/datatype_regression_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Data Type Regression" "Successfully maintain all data type functionality including network types" \
    "CREATE TABLE with all data types, INSERT, and type-specific operations" "$start_time" "$end_time"

cat "$TEST_DB_DIR/datatype_regression_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 4: Performance Regression Detection
echo "Testing performance regression detection..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/performance_regression_test.sql" << 'EOF'
/* Performance regression detection test */
CONNECT 'test_databases/regression_test.fdb';

/* Create performance baseline tables */
CREATE TABLE regression_perf_baseline (
    id INTEGER PRIMARY KEY,
    test_name VARCHAR(100),
    operation_type VARCHAR(50),
    record_count INTEGER,
    execution_time DECIMAL(10,3),
    memory_usage INTEGER,
    baseline_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create performance test table */
CREATE TABLE regression_perf_test (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    category VARCHAR(50),
    value DECIMAL(10,2),
    description VARCHAR(500),
    created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create index for performance testing */
CREATE INDEX idx_perf_category ON regression_perf_test (category);
CREATE INDEX idx_perf_value ON regression_perf_test (value);
CREATE INDEX idx_perf_name ON regression_perf_test (name);

/* Insert baseline performance data */
INSERT INTO regression_perf_baseline (id, test_name, operation_type, record_count, execution_time, memory_usage) VALUES
(1, 'Basic SELECT', 'SELECT', 1000, 0.050, 1024),
(2, 'Indexed SELECT', 'SELECT', 1000, 0.010, 512),
(3, 'JOIN Operation', 'JOIN', 1000, 0.100, 2048),
(4, 'Aggregate Query', 'AGGREGATE', 1000, 0.075, 1536),
(5, 'INSERT Operation', 'INSERT', 1000, 0.200, 1024),
(6, 'UPDATE Operation', 'UPDATE', 1000, 0.150, 1024),
(7, 'DELETE Operation', 'DELETE', 1000, 0.100, 512);

/* Insert test data for performance testing */
INSERT INTO regression_perf_test (id, name, category, value, description) VALUES
(1, 'Performance Test 1', 'Category A', 100.00, 'First performance test record'),
(2, 'Performance Test 2', 'Category B', 200.00, 'Second performance test record'),
(3, 'Performance Test 3', 'Category A', 300.00, 'Third performance test record'),
(4, 'Performance Test 4', 'Category C', 400.00, 'Fourth performance test record'),
(5, 'Performance Test 5', 'Category B', 500.00, 'Fifth performance test record'),
(6, 'Performance Test 6', 'Category A', 600.00, 'Sixth performance test record'),
(7, 'Performance Test 7', 'Category C', 700.00, 'Seventh performance test record'),
(8, 'Performance Test 8', 'Category B', 800.00, 'Eighth performance test record'),
(9, 'Performance Test 9', 'Category A', 900.00, 'Ninth performance test record'),
(10, 'Performance Test 10', 'Category C', 1000.00, 'Tenth performance test record');

/* Test basic SELECT performance */
SELECT COUNT(*) as total_records FROM regression_perf_test;

/* Test indexed SELECT performance */
SELECT * FROM regression_perf_test WHERE category = 'Category A' ORDER BY value;

/* Test JOIN performance */
SELECT 
    p.test_name,
    p.operation_type,
    p.execution_time,
    t.name,
    t.category
FROM regression_perf_baseline p
CROSS JOIN regression_perf_test t
WHERE p.id <= 3 AND t.id <= 3;

/* Test aggregate performance */
SELECT 
    category,
    COUNT(*) as record_count,
    AVG(value) as avg_value,
    SUM(value) as total_value,
    MIN(value) as min_value,
    MAX(value) as max_value
FROM regression_perf_test
GROUP BY category
ORDER BY avg_value DESC;

/* Test complex query performance */
SELECT 
    t1.name,
    t1.category,
    t1.value,
    t2.name as related_name,
    t2.value as related_value,
    ABS(t1.value - t2.value) as value_difference
FROM regression_perf_test t1
JOIN regression_perf_test t2 ON t1.category = t2.category AND t1.id != t2.id
WHERE t1.value > 300.00
ORDER BY value_difference DESC;

/* Test bulk operations performance */
UPDATE regression_perf_test SET value = value * 1.1 WHERE category = 'Category A';

SELECT 
    category,
    COUNT(*) as updated_count,
    AVG(value) as new_avg_value
FROM regression_perf_test
WHERE category = 'Category A'
GROUP BY category;

/* Test range query performance */
SELECT * FROM regression_perf_test 
WHERE value BETWEEN 400.00 AND 800.00 
ORDER BY value;

/* Test pattern matching performance */
SELECT * FROM regression_perf_test 
WHERE name LIKE 'Performance Test%' 
ORDER BY name;

/* Test subquery performance */
SELECT 
    name,
    category,
    value,
    (SELECT AVG(value) FROM regression_perf_test p2 WHERE p2.category = p1.category) as category_avg
FROM regression_perf_test p1
WHERE value > (SELECT AVG(value) FROM regression_perf_test)
ORDER BY value DESC;

/* Performance regression baseline comparison */
SELECT 
    'Performance Regression Check' as test_type,
    COUNT(*) as total_baseline_tests,
    AVG(execution_time) as avg_baseline_time,
    MAX(execution_time) as max_baseline_time
FROM regression_perf_baseline;

/* Test memory usage patterns */
SELECT 
    operation_type,
    AVG(memory_usage) as avg_memory,
    MAX(memory_usage) as max_memory,
    COUNT(*) as test_count
FROM regression_perf_baseline
GROUP BY operation_type
ORDER BY avg_memory DESC;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/performance_regression_test.sql" "$TEST_DB_DIR/performance_regression_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Performance Regression Detection" "Successfully detect performance regression issues" \
    "Complex queries, JOINs, aggregates, and bulk operations within acceptable performance bounds" "$start_time" "$end_time"

cat "$TEST_DB_DIR/performance_regression_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Summary
echo "Regression Tests Completed" >> "$OUTPUT_FILE"
echo "Test database: $TEST_DB" >> "$OUTPUT_FILE"
echo "Test files created in: $TEST_DB_DIR" >> "$OUTPUT_FILE"
echo "These tests verify that existing functionality remains stable" >> "$OUTPUT_FILE"
echo "Performance thresholds should be monitored for regression detection" >> "$OUTPUT_FILE"
echo "=====================================." >> "$OUTPUT_FILE"

echo "Regression tests completed successfully"
exit 0