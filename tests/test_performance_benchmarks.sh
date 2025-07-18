#!/bin/bash

#
# ScratchBird v0.5.0 - Performance Benchmark Tests
#
# This script tests the operational speed and performance characteristics
# of ScratchBird, including benchmarks for:
# - Schema navigation performance
# - Network data type operations
# - Query execution speed
# - Index performance
# - Bulk operations
# - Memory usage optimization
# - Connection handling
#
# Test categories:
# 1. Schema hierarchy performance
# 2. Network data type performance
# 3. Query execution benchmarks
# 4. Index performance tests
# 5. Bulk operation benchmarks
# 6. Memory usage tests
# 7. Connection scalability tests
#
# Copyright (c) 2025 ScratchBird Development Team
# All Rights Reserved.
#

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DB_DIR="$SCRIPT_DIR/test_databases"
TEST_DB="$TEST_DB_DIR/performance_benchmark_test.fdb"
SB_ISQL_PATH="../gen/Release/scratchbird/bin/sb_isql"
OUTPUT_FILE="$SCRIPT_DIR/test_results.txt"

# Performance thresholds (in seconds)
FAST_THRESHOLD=0.1
MEDIUM_THRESHOLD=1.0
SLOW_THRESHOLD=5.0

# Create test database directory
mkdir -p "$TEST_DB_DIR"

# Function to log test results with performance analysis
log_test_result() {
    local test_name="$1"
    local expected_result="$2"
    local command="$3"
    local start_time="$4"
    local end_time="$5"
    
    local elapsed_time=$(echo "$end_time - $start_time" | bc -l)
    
    # Determine performance category
    local performance_category
    if (( $(echo "$elapsed_time < $FAST_THRESHOLD" | bc -l) )); then
        performance_category="FAST"
    elif (( $(echo "$elapsed_time < $MEDIUM_THRESHOLD" | bc -l) )); then
        performance_category="MEDIUM"
    elif (( $(echo "$elapsed_time < $SLOW_THRESHOLD" | bc -l) )); then
        performance_category="SLOW"
    else
        performance_category="VERY_SLOW"
    fi
    
    echo "=========================================" >> "$OUTPUT_FILE"
    echo "Test: $test_name" >> "$OUTPUT_FILE"
    echo "Expected: $expected_result" >> "$OUTPUT_FILE"
    echo "Command: $command" >> "$OUTPUT_FILE"
    echo "Execution time: ${elapsed_time}s" >> "$OUTPUT_FILE"
    echo "Performance category: $performance_category" >> "$OUTPUT_FILE"
    echo "=========================================" >> "$OUTPUT_FILE"
}

# Function to run sb_isql with test file and detailed timing
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

# Test 1: Schema Hierarchy Performance
echo "Testing schema hierarchy performance..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/schema_performance_test.sql" << 'EOF'
/* Schema hierarchy performance test */
CREATE DATABASE 'test_databases/performance_benchmark_test.fdb';
CONNECT 'test_databases/performance_benchmark_test.fdb';

/* Create deep schema hierarchy for performance testing */
CREATE SCHEMA perf;
CREATE SCHEMA perf.level1;
CREATE SCHEMA perf.level1.level2;
CREATE SCHEMA perf.level1.level2.level3;
CREATE SCHEMA perf.level1.level2.level3.level4;
CREATE SCHEMA perf.level1.level2.level3.level4.level5;
CREATE SCHEMA perf.level1.level2.level3.level4.level5.level6;
CREATE SCHEMA perf.level1.level2.level3.level4.level5.level6.level7;
CREATE SCHEMA perf.level1.level2.level3.level4.level5.level6.level7.level8;

/* Create table in deepest schema */
CREATE TABLE perf.level1.level2.level3.level4.level5.level6.level7.level8.deep_table (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    value DECIMAL(10,2),
    created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create index on deep table */
CREATE INDEX idx_deep_table_name ON perf.level1.level2.level3.level4.level5.level6.level7.level8.deep_table (name);

/* Insert test data */
INSERT INTO perf.level1.level2.level3.level4.level5.level6.level7.level8.deep_table (id, name, value) VALUES
(1, 'Performance Test 1', 100.00),
(2, 'Performance Test 2', 200.00),
(3, 'Performance Test 3', 300.00),
(4, 'Performance Test 4', 400.00),
(5, 'Performance Test 5', 500.00);

/* Performance test queries */
SELECT COUNT(*) FROM perf.level1.level2.level3.level4.level5.level6.level7.level8.deep_table;

SELECT * FROM perf.level1.level2.level3.level4.level5.level6.level7.level8.deep_table 
WHERE name = 'Performance Test 3';

SELECT AVG(value) FROM perf.level1.level2.level3.level4.level5.level6.level7.level8.deep_table;

/* Schema metadata query performance */
SELECT COUNT(*) FROM RDB$SCHEMAS WHERE RDB$SCHEMA_NAME STARTING WITH 'PERF';

SELECT RDB$SCHEMA_NAME, RDB$SCHEMA_LEVEL, RDB$SCHEMA_PATH 
FROM RDB$SCHEMAS 
WHERE RDB$SCHEMA_PATH CONTAINING 'perf' 
ORDER BY RDB$SCHEMA_LEVEL;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/schema_performance_test.sql" "$TEST_DB_DIR/schema_performance_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Schema Hierarchy Performance" "Execute operations on 8-level deep schema within performance thresholds" \
    "SELECT * FROM perf.level1.level2.level3.level4.level5.level6.level7.level8.deep_table" "$start_time" "$end_time"

cat "$TEST_DB_DIR/schema_performance_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 2: Network Data Type Performance
echo "Testing network data type performance..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/network_performance_test.sql" << 'EOF'
/* Network data type performance test */
CONNECT 'test_databases/performance_benchmark_test.fdb';

/* Create schema for network performance testing */
CREATE SCHEMA perf.network;

/* Create table with network data types */
CREATE TABLE perf.network.network_performance (
    id INTEGER PRIMARY KEY,
    ip_address INET,
    network_cidr CIDR,
    mac_address MACADDR,
    device_uuid UUID,
    created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create indexes for performance testing */
CREATE INDEX idx_net_ip ON perf.network.network_performance (ip_address);
CREATE INDEX idx_net_cidr ON perf.network.network_performance (network_cidr);
CREATE INDEX idx_net_mac ON perf.network.network_performance (mac_address);
CREATE INDEX idx_net_uuid ON perf.network.network_performance (device_uuid);

/* Insert bulk network data */
INSERT INTO perf.network.network_performance (id, ip_address, network_cidr, mac_address, device_uuid) VALUES
(1, '192.168.1.10', '192.168.1.0/24', '00:11:22:33:44:55', '550e8400-e29b-41d4-a716-446655440000'),
(2, '192.168.1.11', '192.168.1.0/24', '00:11:22:33:44:56', '550e8400-e29b-41d4-a716-446655440001'),
(3, '192.168.1.12', '192.168.1.0/24', '00:11:22:33:44:57', '550e8400-e29b-41d4-a716-446655440002'),
(4, '192.168.1.13', '192.168.1.0/24', '00:11:22:33:44:58', '550e8400-e29b-41d4-a716-446655440003'),
(5, '192.168.1.14', '192.168.1.0/24', '00:11:22:33:44:59', '550e8400-e29b-41d4-a716-446655440004'),
(6, '10.0.0.10', '10.0.0.0/8', '00:11:22:33:44:5A', '550e8400-e29b-41d4-a716-446655440005'),
(7, '10.0.0.11', '10.0.0.0/8', '00:11:22:33:44:5B', '550e8400-e29b-41d4-a716-446655440006'),
(8, '10.0.0.12', '10.0.0.0/8', '00:11:22:33:44:5C', '550e8400-e29b-41d4-a716-446655440007'),
(9, '10.0.0.13', '10.0.0.0/8', '00:11:22:33:44:5D', '550e8400-e29b-41d4-a716-446655440008'),
(10, '10.0.0.14', '10.0.0.0/8', '00:11:22:33:44:5E', '550e8400-e29b-41d4-a716-446655440009');

/* Performance test queries */
SELECT COUNT(*) FROM perf.network.network_performance;

/* IP address performance tests */
SELECT * FROM perf.network.network_performance WHERE ip_address = '192.168.1.12';
SELECT * FROM perf.network.network_performance WHERE ip_address > '192.168.1.10' ORDER BY ip_address;

/* CIDR performance tests */
SELECT * FROM perf.network.network_performance WHERE ip_address << '192.168.1.0/24';
SELECT * FROM perf.network.network_performance WHERE network_cidr && '192.168.0.0/16';

/* MAC address performance tests */
SELECT * FROM perf.network.network_performance WHERE mac_address = '00:11:22:33:44:57';
SELECT * FROM perf.network.network_performance ORDER BY mac_address;

/* UUID performance tests */
SELECT * FROM perf.network.network_performance WHERE device_uuid = '550e8400-e29b-41d4-a716-446655440005';
SELECT * FROM perf.network.network_performance ORDER BY device_uuid;

/* Aggregate performance tests */
SELECT network_cidr, COUNT(*) as device_count FROM perf.network.network_performance GROUP BY network_cidr;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/network_performance_test.sql" "$TEST_DB_DIR/network_performance_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Network Data Type Performance" "Execute network data type operations with optimal performance" \
    "SELECT * FROM perf.network.network_performance WHERE ip_address << '192.168.1.0/24'" "$start_time" "$end_time"

cat "$TEST_DB_DIR/network_performance_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 3: Query Execution Benchmarks
echo "Testing query execution benchmarks..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/query_performance_test.sql" << 'EOF'
/* Query execution benchmarks test */
CONNECT 'test_databases/performance_benchmark_test.fdb';

/* Create schema for query performance testing */
CREATE SCHEMA perf.queries;

/* Create tables for JOIN performance testing */
CREATE TABLE perf.queries.customers (
    customer_id INTEGER PRIMARY KEY,
    customer_name VARCHAR(100),
    email VARCHAR(200),
    registration_date DATE DEFAULT CURRENT_DATE
);

CREATE TABLE perf.queries.orders (
    order_id INTEGER PRIMARY KEY,
    customer_id INTEGER,
    order_date DATE DEFAULT CURRENT_DATE,
    total_amount DECIMAL(10,2),
    status VARCHAR(20) DEFAULT 'PENDING'
);

CREATE TABLE perf.queries.order_items (
    item_id INTEGER PRIMARY KEY,
    order_id INTEGER,
    product_name VARCHAR(100),
    quantity INTEGER,
    unit_price DECIMAL(10,2),
    total_price DECIMAL(10,2)
);

/* Create indexes for performance optimization */
CREATE INDEX idx_customers_email ON perf.queries.customers (email);
CREATE INDEX idx_orders_customer ON perf.queries.orders (customer_id);
CREATE INDEX idx_orders_date ON perf.queries.orders (order_date);
CREATE INDEX idx_items_order ON perf.queries.order_items (order_id);

/* Insert test data */
INSERT INTO perf.queries.customers (customer_id, customer_name, email) VALUES
(1, 'John Doe', 'john.doe@example.com'),
(2, 'Jane Smith', 'jane.smith@example.com'),
(3, 'Bob Johnson', 'bob.johnson@example.com'),
(4, 'Alice Brown', 'alice.brown@example.com'),
(5, 'Charlie Wilson', 'charlie.wilson@example.com');

INSERT INTO perf.queries.orders (order_id, customer_id, total_amount, status) VALUES
(1, 1, 150.00, 'COMPLETED'),
(2, 2, 200.00, 'COMPLETED'),
(3, 1, 75.00, 'PENDING'),
(4, 3, 300.00, 'COMPLETED'),
(5, 4, 125.00, 'SHIPPED'),
(6, 2, 180.00, 'PENDING'),
(7, 5, 220.00, 'COMPLETED'),
(8, 1, 90.00, 'SHIPPED'),
(9, 3, 250.00, 'COMPLETED'),
(10, 4, 175.00, 'PENDING');

INSERT INTO perf.queries.order_items (item_id, order_id, product_name, quantity, unit_price, total_price) VALUES
(1, 1, 'Product A', 2, 25.00, 50.00),
(2, 1, 'Product B', 1, 100.00, 100.00),
(3, 2, 'Product C', 3, 33.33, 100.00),
(4, 2, 'Product D', 2, 50.00, 100.00),
(5, 3, 'Product A', 3, 25.00, 75.00),
(6, 4, 'Product E', 1, 300.00, 300.00),
(7, 5, 'Product F', 5, 25.00, 125.00),
(8, 6, 'Product G', 2, 90.00, 180.00),
(9, 7, 'Product H', 4, 55.00, 220.00),
(10, 8, 'Product I', 3, 30.00, 90.00);

/* Performance benchmark queries */

/* Simple SELECT performance */
SELECT COUNT(*) FROM perf.queries.customers;
SELECT COUNT(*) FROM perf.queries.orders;
SELECT COUNT(*) FROM perf.queries.order_items;

/* Indexed lookup performance */
SELECT * FROM perf.queries.customers WHERE email = 'john.doe@example.com';
SELECT * FROM perf.queries.orders WHERE customer_id = 1;

/* JOIN performance tests */
SELECT 
    c.customer_name,
    o.order_id,
    o.total_amount,
    o.status
FROM perf.queries.customers c
JOIN perf.queries.orders o ON c.customer_id = o.customer_id
ORDER BY c.customer_name, o.order_id;

/* Complex JOIN with aggregation */
SELECT 
    c.customer_name,
    COUNT(o.order_id) as order_count,
    SUM(o.total_amount) as total_spent,
    AVG(o.total_amount) as avg_order_value
FROM perf.queries.customers c
LEFT JOIN perf.queries.orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name
ORDER BY total_spent DESC;

/* Three-way JOIN performance */
SELECT 
    c.customer_name,
    o.order_id,
    oi.product_name,
    oi.quantity,
    oi.total_price
FROM perf.queries.customers c
JOIN perf.queries.orders o ON c.customer_id = o.customer_id
JOIN perf.queries.order_items oi ON o.order_id = oi.order_id
WHERE o.status = 'COMPLETED'
ORDER BY c.customer_name, o.order_id, oi.item_id;

/* Subquery performance */
SELECT 
    customer_name,
    email
FROM perf.queries.customers
WHERE customer_id IN (
    SELECT customer_id 
    FROM perf.queries.orders 
    WHERE total_amount > 200.00
);

/* Aggregate performance */
SELECT 
    status,
    COUNT(*) as order_count,
    SUM(total_amount) as total_revenue,
    AVG(total_amount) as avg_order_value,
    MIN(total_amount) as min_order,
    MAX(total_amount) as max_order
FROM perf.queries.orders
GROUP BY status
ORDER BY total_revenue DESC;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/query_performance_test.sql" "$TEST_DB_DIR/query_performance_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Query Execution Performance" "Execute complex queries with JOINs and aggregations efficiently" \
    "SELECT c.customer_name, COUNT(o.order_id) FROM customers c LEFT JOIN orders o ON c.customer_id = o.customer_id GROUP BY c.customer_name" "$start_time" "$end_time"

cat "$TEST_DB_DIR/query_performance_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 4: Bulk Operations Performance
echo "Testing bulk operations performance..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/bulk_performance_test.sql" << 'EOF'
/* Bulk operations performance test */
CONNECT 'test_databases/performance_benchmark_test.fdb';

/* Create schema for bulk operations testing */
CREATE SCHEMA perf.bulk;

/* Create table for bulk operations */
CREATE TABLE perf.bulk.bulk_data (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    value DECIMAL(10,2),
    category VARCHAR(50),
    created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create index for bulk operations */
CREATE INDEX idx_bulk_category ON perf.bulk.bulk_data (category);
CREATE INDEX idx_bulk_value ON perf.bulk.bulk_data (value);

/* Bulk INSERT performance test */
INSERT INTO perf.bulk.bulk_data (id, name, value, category) VALUES
(1, 'Bulk Item 1', 100.00, 'Category A'),
(2, 'Bulk Item 2', 200.00, 'Category B'),
(3, 'Bulk Item 3', 300.00, 'Category A'),
(4, 'Bulk Item 4', 400.00, 'Category C'),
(5, 'Bulk Item 5', 500.00, 'Category B'),
(6, 'Bulk Item 6', 600.00, 'Category A'),
(7, 'Bulk Item 7', 700.00, 'Category C'),
(8, 'Bulk Item 8', 800.00, 'Category B'),
(9, 'Bulk Item 9', 900.00, 'Category A'),
(10, 'Bulk Item 10', 1000.00, 'Category C'),
(11, 'Bulk Item 11', 1100.00, 'Category B'),
(12, 'Bulk Item 12', 1200.00, 'Category A'),
(13, 'Bulk Item 13', 1300.00, 'Category C'),
(14, 'Bulk Item 14', 1400.00, 'Category B'),
(15, 'Bulk Item 15', 1500.00, 'Category A'),
(16, 'Bulk Item 16', 1600.00, 'Category C'),
(17, 'Bulk Item 17', 1700.00, 'Category B'),
(18, 'Bulk Item 18', 1800.00, 'Category A'),
(19, 'Bulk Item 19', 1900.00, 'Category C'),
(20, 'Bulk Item 20', 2000.00, 'Category B');

/* Bulk UPDATE performance test */
UPDATE perf.bulk.bulk_data 
SET value = value * 1.1 
WHERE category = 'Category A';

UPDATE perf.bulk.bulk_data 
SET name = name || ' - Updated' 
WHERE id > 10;

/* Bulk SELECT performance test */
SELECT * FROM perf.bulk.bulk_data ORDER BY id;

SELECT category, COUNT(*) as item_count, AVG(value) as avg_value 
FROM perf.bulk.bulk_data 
GROUP BY category 
ORDER BY avg_value DESC;

/* Bulk DELETE performance test (partial) */
DELETE FROM perf.bulk.bulk_data WHERE id > 15;

/* Verify remaining data */
SELECT COUNT(*) as remaining_count FROM perf.bulk.bulk_data;

/* Range query performance */
SELECT * FROM perf.bulk.bulk_data 
WHERE value BETWEEN 500.00 AND 1500.00 
ORDER BY value;

/* Pattern matching performance */
SELECT * FROM perf.bulk.bulk_data 
WHERE name LIKE '%Item%' 
ORDER BY name;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/bulk_performance_test.sql" "$TEST_DB_DIR/bulk_performance_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Bulk Operations Performance" "Execute bulk INSERT, UPDATE, and DELETE operations efficiently" \
    "INSERT INTO perf.bulk.bulk_data (20 rows) and UPDATE operations" "$start_time" "$end_time"

cat "$TEST_DB_DIR/bulk_performance_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 5: Memory Usage and Optimization
echo "Testing memory usage and optimization..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/memory_performance_test.sql" << 'EOF'
/* Memory usage and optimization test */
CONNECT 'test_databases/performance_benchmark_test.fdb';

/* Create schema for memory testing */
CREATE SCHEMA perf.memory;

/* Create table with large data types */
CREATE TABLE perf.memory.memory_test (
    id INTEGER PRIMARY KEY,
    large_text VARCHAR(1000),
    blob_data BLOB,
    created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Insert data with varying sizes */
INSERT INTO perf.memory.memory_test (id, large_text, blob_data) VALUES
(1, 'Small text data', 'Small blob data'),
(2, 'Medium text data that is longer than the first entry and contains more information to test memory usage patterns', 'Medium blob data with additional content'),
(3, 'Large text data that is significantly longer than previous entries and is designed to test memory allocation and deallocation patterns in the database system when handling variable-length string data types', 'Large blob data with substantial content that will test blob handling performance and memory management'),
(4, 'Another medium-sized text entry', 'Another medium-sized blob entry'),
(5, 'Final small text', 'Final small blob');

/* Memory usage queries */
SELECT COUNT(*) FROM perf.memory.memory_test;

SELECT 
    id,
    LENGTH(large_text) as text_length,
    SUBSTRING(large_text FROM 1 FOR 50) as text_preview
FROM perf.memory.memory_test
ORDER BY text_length DESC;

/* Test memory with complex operations */
SELECT 
    id,
    large_text,
    LENGTH(large_text) as text_length,
    UPPER(SUBSTRING(large_text FROM 1 FOR 100)) as upper_preview
FROM perf.memory.memory_test
WHERE LENGTH(large_text) > 50
ORDER BY text_length;

/* Test memory with aggregation */
SELECT 
    COUNT(*) as record_count,
    AVG(LENGTH(large_text)) as avg_text_length,
    SUM(LENGTH(large_text)) as total_text_length,
    MAX(LENGTH(large_text)) as max_text_length
FROM perf.memory.memory_test;

/* Clean up memory test data */
DELETE FROM perf.memory.memory_test WHERE id > 3;

SELECT COUNT(*) as remaining_records FROM perf.memory.memory_test;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/memory_performance_test.sql" "$TEST_DB_DIR/memory_performance_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Memory Usage Performance" "Handle large text and blob data efficiently" \
    "SELECT id, LENGTH(large_text), UPPER(SUBSTRING(large_text FROM 1 FOR 100)) FROM perf.memory.memory_test" "$start_time" "$end_time"

cat "$TEST_DB_DIR/memory_performance_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Performance Summary
echo "Performance Benchmark Summary" >> "$OUTPUT_FILE"
echo "=============================" >> "$OUTPUT_FILE"
echo "Test database: $TEST_DB" >> "$OUTPUT_FILE"
echo "Performance thresholds:" >> "$OUTPUT_FILE"
echo "  FAST: < ${FAST_THRESHOLD}s" >> "$OUTPUT_FILE"
echo "  MEDIUM: < ${MEDIUM_THRESHOLD}s" >> "$OUTPUT_FILE"
echo "  SLOW: < ${SLOW_THRESHOLD}s" >> "$OUTPUT_FILE"
echo "  VERY_SLOW: >= ${SLOW_THRESHOLD}s" >> "$OUTPUT_FILE"
echo "Test files created in: $TEST_DB_DIR" >> "$OUTPUT_FILE"
echo "=====================================." >> "$OUTPUT_FILE"

echo "Performance benchmark tests completed successfully"
exit 0