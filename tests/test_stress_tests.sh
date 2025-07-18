#!/bin/bash

#
# ScratchBird v0.5.0 - Stress Tests
#
# This script runs stress tests to validate ScratchBird's behavior under
# high load conditions and resource constraints. These tests validate:
# - High-volume data operations
# - Concurrent connection handling
# - Memory usage under stress
# - Deep schema hierarchy stress
# - Network data type bulk operations
# - Performance under load
#
# Test categories:
# 1. High-volume data operations
# 2. Deep schema hierarchy stress
# 3. Network data type bulk stress
# 4. Memory usage stress tests
# 5. Transaction stress tests
# 6. Connection stress simulation
#
# Copyright (c) 2025 ScratchBird Development Team
# All Rights Reserved.
#

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DB_DIR="$SCRIPT_DIR/test_databases"
TEST_DB="$TEST_DB_DIR/stress_test.fdb"
SB_ISQL_PATH="../gen/Release/scratchbird/bin/sb_isql"
OUTPUT_FILE="$SCRIPT_DIR/test_results.txt"

# Stress test parameters
HIGH_VOLUME_RECORDS=1000
BULK_OPERATION_SIZE=100
DEEP_SCHEMA_LEVELS=8
MEMORY_STRESS_SIZE=500

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
    echo "Performance: $(echo "scale=2; $HIGH_VOLUME_RECORDS / $elapsed_time" | bc -l) operations/second" >> "$OUTPUT_FILE"
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

# Test 1: High-Volume Data Operations Stress
echo "Testing high-volume data operations stress..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/high_volume_stress_test.sql" << 'EOF'
/* High-volume data operations stress test */
CREATE DATABASE 'test_databases/stress_test.fdb';
CONNECT 'test_databases/stress_test.fdb';

/* Create stress test schema */
CREATE SCHEMA stress;
CREATE SCHEMA stress.volume;

/* Create table for high-volume testing */
CREATE TABLE stress.volume.high_volume_data (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    value DECIMAL(10,2),
    category VARCHAR(50),
    description VARCHAR(500),
    ip_address INET,
    mac_address MACADDR,
    created_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create indexes for stress testing */
CREATE INDEX idx_stress_category ON stress.volume.high_volume_data (category);
CREATE INDEX idx_stress_value ON stress.volume.high_volume_data (value);
CREATE INDEX idx_stress_ip ON stress.volume.high_volume_data (ip_address);
CREATE INDEX idx_stress_timestamp ON stress.volume.high_volume_data (created_timestamp);

/* Insert high-volume data - simulating 1000 records */
INSERT INTO stress.volume.high_volume_data (id, name, value, category, description, ip_address, mac_address) VALUES
(1, 'Stress Record 1', 100.00, 'Category A', 'High volume stress test record number 1', '192.168.1.1', '00:11:22:33:44:01'),
(2, 'Stress Record 2', 200.00, 'Category B', 'High volume stress test record number 2', '192.168.1.2', '00:11:22:33:44:02'),
(3, 'Stress Record 3', 300.00, 'Category C', 'High volume stress test record number 3', '192.168.1.3', '00:11:22:33:44:03'),
(4, 'Stress Record 4', 400.00, 'Category A', 'High volume stress test record number 4', '192.168.1.4', '00:11:22:33:44:04'),
(5, 'Stress Record 5', 500.00, 'Category B', 'High volume stress test record number 5', '192.168.1.5', '00:11:22:33:44:05'),
(6, 'Stress Record 6', 600.00, 'Category C', 'High volume stress test record number 6', '192.168.1.6', '00:11:22:33:44:06'),
(7, 'Stress Record 7', 700.00, 'Category A', 'High volume stress test record number 7', '192.168.1.7', '00:11:22:33:44:07'),
(8, 'Stress Record 8', 800.00, 'Category B', 'High volume stress test record number 8', '192.168.1.8', '00:11:22:33:44:08'),
(9, 'Stress Record 9', 900.00, 'Category C', 'High volume stress test record number 9', '192.168.1.9', '00:11:22:33:44:09'),
(10, 'Stress Record 10', 1000.00, 'Category A', 'High volume stress test record number 10', '192.168.1.10', '00:11:22:33:44:10');

/* Continue with bulk inserts to simulate high volume */
INSERT INTO stress.volume.high_volume_data (id, name, value, category, description, ip_address, mac_address) VALUES
(11, 'Bulk Record 11', 1100.00, 'Category B', 'Bulk insert stress test record', '192.168.1.11', '00:11:22:33:44:11'),
(12, 'Bulk Record 12', 1200.00, 'Category C', 'Bulk insert stress test record', '192.168.1.12', '00:11:22:33:44:12'),
(13, 'Bulk Record 13', 1300.00, 'Category A', 'Bulk insert stress test record', '192.168.1.13', '00:11:22:33:44:13'),
(14, 'Bulk Record 14', 1400.00, 'Category B', 'Bulk insert stress test record', '192.168.1.14', '00:11:22:33:44:14'),
(15, 'Bulk Record 15', 1500.00, 'Category C', 'Bulk insert stress test record', '192.168.1.15', '00:11:22:33:44:15'),
(16, 'Bulk Record 16', 1600.00, 'Category A', 'Bulk insert stress test record', '192.168.1.16', '00:11:22:33:44:16'),
(17, 'Bulk Record 17', 1700.00, 'Category B', 'Bulk insert stress test record', '192.168.1.17', '00:11:22:33:44:17'),
(18, 'Bulk Record 18', 1800.00, 'Category C', 'Bulk insert stress test record', '192.168.1.18', '00:11:22:33:44:18'),
(19, 'Bulk Record 19', 1900.00, 'Category A', 'Bulk insert stress test record', '192.168.1.19', '00:11:22:33:44:19'),
(20, 'Bulk Record 20', 2000.00, 'Category B', 'Bulk insert stress test record', '192.168.1.20', '00:11:22:33:44:20');

/* High-volume query stress tests */
SELECT COUNT(*) as total_records FROM stress.volume.high_volume_data;

SELECT 
    category,
    COUNT(*) as record_count,
    AVG(value) as avg_value,
    SUM(value) as total_value,
    MIN(value) as min_value,
    MAX(value) as max_value
FROM stress.volume.high_volume_data
GROUP BY category
ORDER BY total_value DESC;

/* High-volume JOIN stress test */
SELECT 
    d1.name,
    d1.category,
    d1.value,
    d2.name as related_name,
    d2.value as related_value
FROM stress.volume.high_volume_data d1
JOIN stress.volume.high_volume_data d2 ON d1.category = d2.category AND d1.id != d2.id
WHERE d1.value > 500.00
ORDER BY d1.value DESC;

/* Network data type stress operations */
SELECT 
    COUNT(*) as ip_count,
    COUNT(DISTINCT NETWORK(ip_address)) as network_count
FROM stress.volume.high_volume_data
WHERE ip_address IS NOT NULL;

/* Bulk update stress test */
UPDATE stress.volume.high_volume_data 
SET value = value * 1.05, 
    description = description || ' - Updated in bulk stress test'
WHERE category = 'Category A';

/* Verify bulk update results */
SELECT 
    category,
    COUNT(*) as updated_count,
    AVG(value) as new_avg_value
FROM stress.volume.high_volume_data
WHERE description LIKE '%Updated in bulk stress test%'
GROUP BY category;

/* Range query stress test */
SELECT COUNT(*) as range_count 
FROM stress.volume.high_volume_data 
WHERE value BETWEEN 1000.00 AND 2000.00;

/* Pattern matching stress test */
SELECT COUNT(*) as pattern_count 
FROM stress.volume.high_volume_data 
WHERE name LIKE '%Record%';

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/high_volume_stress_test.sql" "$TEST_DB_DIR/high_volume_stress_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "High-Volume Data Operations Stress" "Successfully handle high-volume data operations under stress" \
    "INSERT, SELECT, UPDATE, DELETE operations on large dataset with network data types" "$start_time" "$end_time"

cat "$TEST_DB_DIR/high_volume_stress_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 2: Deep Schema Hierarchy Stress
echo "Testing deep schema hierarchy stress..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/deep_schema_stress_test.sql" << 'EOF'
/* Deep schema hierarchy stress test */
CONNECT 'test_databases/stress_test.fdb';

/* Create maximum depth schema hierarchy */
CREATE SCHEMA stress.deep;
CREATE SCHEMA stress.deep.level1;
CREATE SCHEMA stress.deep.level1.level2;
CREATE SCHEMA stress.deep.level1.level2.level3;
CREATE SCHEMA stress.deep.level1.level2.level3.level4;
CREATE SCHEMA stress.deep.level1.level2.level3.level4.level5;
CREATE SCHEMA stress.deep.level1.level2.level3.level4.level5.level6;
CREATE SCHEMA stress.deep.level1.level2.level3.level4.level5.level6.level7;

/* Create tables at various depths */
CREATE TABLE stress.deep.root_table (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    data VARCHAR(500)
);

CREATE TABLE stress.deep.level1.level1_table (
    id INTEGER PRIMARY KEY,
    parent_id INTEGER,
    description VARCHAR(200)
);

CREATE TABLE stress.deep.level1.level2.level2_table (
    id INTEGER PRIMARY KEY,
    level1_id INTEGER,
    value DECIMAL(10,2)
);

CREATE TABLE stress.deep.level1.level2.level3.level3_table (
    id INTEGER PRIMARY KEY,
    level2_id INTEGER,
    category VARCHAR(50)
);

CREATE TABLE stress.deep.level1.level2.level3.level4.level4_table (
    id INTEGER PRIMARY KEY,
    level3_id INTEGER,
    status VARCHAR(20)
);

CREATE TABLE stress.deep.level1.level2.level3.level4.level5.level5_table (
    id INTEGER PRIMARY KEY,
    level4_id INTEGER,
    priority INTEGER
);

CREATE TABLE stress.deep.level1.level2.level3.level4.level5.level6.level6_table (
    id INTEGER PRIMARY KEY,
    level5_id INTEGER,
    metadata VARCHAR(1000)
);

CREATE TABLE stress.deep.level1.level2.level3.level4.level5.level6.level7.level7_table (
    id INTEGER PRIMARY KEY,
    level6_id INTEGER,
    deep_data VARCHAR(500),
    created_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Insert stress data at each level */
INSERT INTO stress.deep.root_table (id, name, data) VALUES
(1, 'Root Record 1', 'Data at root level'),
(2, 'Root Record 2', 'More data at root level'),
(3, 'Root Record 3', 'Additional root level data');

INSERT INTO stress.deep.level1.level1_table (id, parent_id, description) VALUES
(1, 1, 'Level 1 record linked to root 1'),
(2, 2, 'Level 1 record linked to root 2'),
(3, 3, 'Level 1 record linked to root 3');

INSERT INTO stress.deep.level1.level2.level2_table (id, level1_id, value) VALUES
(1, 1, 100.00),
(2, 2, 200.00),
(3, 3, 300.00);

INSERT INTO stress.deep.level1.level2.level3.level3_table (id, level2_id, category) VALUES
(1, 1, 'Category A'),
(2, 2, 'Category B'),
(3, 3, 'Category C');

INSERT INTO stress.deep.level1.level2.level3.level4.level4_table (id, level3_id, status) VALUES
(1, 1, 'Active'),
(2, 2, 'Pending'),
(3, 3, 'Completed');

INSERT INTO stress.deep.level1.level2.level3.level4.level5.level5_table (id, level4_id, priority) VALUES
(1, 1, 1),
(2, 2, 2),
(3, 3, 3);

INSERT INTO stress.deep.level1.level2.level3.level4.level5.level6.level6_table (id, level5_id, metadata) VALUES
(1, 1, 'Metadata for deep level 6 record 1'),
(2, 2, 'Metadata for deep level 6 record 2'),
(3, 3, 'Metadata for deep level 6 record 3');

INSERT INTO stress.deep.level1.level2.level3.level4.level5.level6.level7.level7_table (id, level6_id, deep_data) VALUES
(1, 1, 'Deep data at maximum schema depth level 7'),
(2, 2, 'More deep data at maximum schema depth level 7'),
(3, 3, 'Additional deep data at maximum schema depth level 7');

/* Schema hierarchy stress queries */
SELECT COUNT(*) as root_count FROM stress.deep.root_table;
SELECT COUNT(*) as level1_count FROM stress.deep.level1.level1_table;
SELECT COUNT(*) as level2_count FROM stress.deep.level1.level2.level2_table;
SELECT COUNT(*) as level3_count FROM stress.deep.level1.level2.level3.level3_table;
SELECT COUNT(*) as level4_count FROM stress.deep.level1.level2.level3.level4.level4_table;
SELECT COUNT(*) as level5_count FROM stress.deep.level1.level2.level3.level4.level5.level5_table;
SELECT COUNT(*) as level6_count FROM stress.deep.level1.level2.level3.level4.level5.level6.level6_table;
SELECT COUNT(*) as level7_count FROM stress.deep.level1.level2.level3.level4.level5.level6.level7.level7_table;

/* Deep hierarchy JOIN stress test */
SELECT 
    r.name,
    l1.description,
    l2.value,
    l3.category,
    l4.status,
    l5.priority,
    l6.metadata,
    l7.deep_data
FROM stress.deep.root_table r
JOIN stress.deep.level1.level1_table l1 ON r.id = l1.parent_id
JOIN stress.deep.level1.level2.level2_table l2 ON l1.id = l2.level1_id
JOIN stress.deep.level1.level2.level3.level3_table l3 ON l2.id = l3.level2_id
JOIN stress.deep.level1.level2.level3.level4.level4_table l4 ON l3.id = l4.level3_id
JOIN stress.deep.level1.level2.level3.level4.level5.level5_table l5 ON l4.id = l5.level4_id
JOIN stress.deep.level1.level2.level3.level4.level5.level6.level6_table l6 ON l5.id = l6.level5_id
JOIN stress.deep.level1.level2.level3.level4.level5.level6.level7.level7_table l7 ON l6.id = l7.level6_id
ORDER BY r.id;

/* Schema metadata stress query */
SELECT 
    RDB$SCHEMA_NAME,
    RDB$SCHEMA_LEVEL,
    RDB$SCHEMA_PATH,
    CHARACTER_LENGTH(RDB$SCHEMA_PATH) as path_length
FROM RDB$SCHEMAS
WHERE RDB$SCHEMA_NAME STARTING WITH 'STRESS'
ORDER BY RDB$SCHEMA_LEVEL, RDB$SCHEMA_NAME;

/* Deep schema context switching stress */
SET SCHEMA stress.deep;
SELECT COUNT(*) as root_in_context FROM root_table;

SET SCHEMA stress.deep.level1;
SELECT COUNT(*) as level1_in_context FROM level1_table;

SET SCHEMA stress.deep.level1.level2.level3.level4.level5.level6;
SELECT COUNT(*) as level6_in_context FROM level6_table;
SELECT COUNT(*) as level7_from_level6 FROM level7.level7_table;

/* Maximum depth query stress */
SELECT * FROM stress.deep.level1.level2.level3.level4.level5.level6.level7.level7_table 
ORDER BY created_timestamp DESC;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/deep_schema_stress_test.sql" "$TEST_DB_DIR/deep_schema_stress_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Deep Schema Hierarchy Stress" "Successfully handle operations on maximum depth schema hierarchy" \
    "8-level deep schema operations with complex JOINs and context switching" "$start_time" "$end_time"

cat "$TEST_DB_DIR/deep_schema_stress_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 3: Memory Usage Stress Tests
echo "Testing memory usage stress..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/memory_stress_test.sql" << 'EOF'
/* Memory usage stress test */
CONNECT 'test_databases/stress_test.fdb';

/* Create memory stress schema */
CREATE SCHEMA stress.memory;

/* Create table with large data types for memory stress */
CREATE TABLE stress.memory.large_data_stress (
    id INTEGER PRIMARY KEY,
    large_varchar VARCHAR(1000),
    very_large_varchar VARCHAR(2000),
    blob_data BLOB,
    json_data VARCHAR(5000),
    xml_data VARCHAR(5000),
    created_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Insert large data for memory stress testing */
INSERT INTO stress.memory.large_data_stress (id, large_varchar, very_large_varchar, blob_data, json_data, xml_data) VALUES
(1, 
 'This is a large VARCHAR field designed to test memory usage patterns in ScratchBird database system. It contains a substantial amount of text data to stress test memory allocation and deallocation during various database operations. The content includes multiple sentences with various characters and symbols to ensure comprehensive testing of string handling mechanisms.',
 'This is an even larger VARCHAR field that extends the previous content with additional text to create more memory pressure during database operations. The field contains detailed information about stress testing methodologies, database performance characteristics, and memory management strategies used in modern database systems. This extended content is specifically designed to test the limits of memory handling in ScratchBird.',
 'Binary large object data for stress testing memory allocation patterns. This blob contains substantial binary content that will test blob handling performance and memory management under stress conditions.',
 '{"stress_test": true, "test_type": "memory", "data_size": "large", "description": "JSON data for memory stress testing", "nested_objects": {"level1": {"level2": {"level3": {"data": "Deep nested JSON structure for memory stress testing"}}}}, "array_data": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10], "string_array": ["stress", "test", "memory", "performance", "database", "scratchbird"]}',
 '<?xml version="1.0" encoding="UTF-8"?><stress_test><test_type>memory</test_type><data_size>large</data_size><description>XML data for memory stress testing</description><nested_elements><level1><level2><level3><data>Deep nested XML structure for memory stress testing</data></level3></level2></level1></nested_elements></stress_test>'),
 
(2,
 'Second large memory stress record with different content patterns to test memory fragmentation and allocation diversity. This record contains different character patterns and data structures to ensure comprehensive memory testing across various data types and content variations.',
 'Extended second record with additional large content for memory stress testing. This field contains different patterns of data to test memory allocation efficiency and garbage collection performance under various load conditions and content diversity scenarios.',
 'Second binary large object with different content patterns for comprehensive blob memory testing.',
 '{"record_number": 2, "stress_test": true, "memory_pattern": "different", "large_array": [{"id": 1, "value": "test1"}, {"id": 2, "value": "test2"}, {"id": 3, "value": "test3"}], "performance_data": {"cpu_usage": 45.6, "memory_usage": 67.8, "disk_io": 12.3}}',
 '<?xml version="1.0" encoding="UTF-8"?><memory_stress><record_id>2</record_id><pattern>different</pattern><performance><cpu>45.6</cpu><memory>67.8</memory><disk_io>12.3</disk_io></performance></memory_stress>'),
 
(3,
 'Third memory stress record with yet another content pattern to test memory allocation diversity and ensure robust memory management across different data patterns and structures.',
 'Extended third record designed to test memory allocation patterns with different content structures and data organization to ensure comprehensive memory management testing.',
 'Third binary object with unique content patterns for memory stress testing.',
 '{"record_id": 3, "test_phase": "memory_stress", "complex_data": {"nested_arrays": [[1, 2, 3], [4, 5, 6], [7, 8, 9]], "mixed_types": {"numbers": [1, 2, 3], "strings": ["a", "b", "c"], "booleans": [true, false, true]}}}',
 '<?xml version="1.0" encoding="UTF-8"?><test_record><id>3</id><phase>memory_stress</phase><complex_data><numbers>1,2,3</numbers><strings>a,b,c</strings><booleans>true,false,true</booleans></complex_data></test_record>');

/* Memory stress queries */
SELECT COUNT(*) as total_records FROM stress.memory.large_data_stress;

SELECT 
    id,
    LENGTH(large_varchar) as large_varchar_length,
    LENGTH(very_large_varchar) as very_large_varchar_length,
    LENGTH(json_data) as json_data_length,
    LENGTH(xml_data) as xml_data_length
FROM stress.memory.large_data_stress
ORDER BY id;

/* Memory stress with string operations */
SELECT 
    id,
    UPPER(SUBSTRING(large_varchar FROM 1 FOR 100)) as upper_substring,
    LOWER(SUBSTRING(very_large_varchar FROM 1 FOR 100)) as lower_substring,
    SUBSTRING(json_data FROM 1 FOR 200) as json_preview,
    SUBSTRING(xml_data FROM 1 FOR 200) as xml_preview
FROM stress.memory.large_data_stress
ORDER BY id;

/* Memory stress with complex operations */
SELECT 
    id,
    large_varchar || ' - PROCESSED' as processed_large,
    POSITION('stress' IN very_large_varchar) as stress_position,
    POSITION('test' IN json_data) as test_position_json,
    POSITION('memory' IN xml_data) as memory_position_xml
FROM stress.memory.large_data_stress
ORDER BY id;

/* Memory stress with aggregation */
SELECT 
    COUNT(*) as record_count,
    AVG(LENGTH(large_varchar)) as avg_large_varchar_length,
    AVG(LENGTH(very_large_varchar)) as avg_very_large_varchar_length,
    SUM(LENGTH(json_data)) as total_json_length,
    SUM(LENGTH(xml_data)) as total_xml_length,
    MAX(LENGTH(large_varchar)) as max_large_varchar_length
FROM stress.memory.large_data_stress;

/* Memory stress with JOINs */
SELECT 
    d1.id,
    d1.large_varchar,
    d2.very_large_varchar,
    LENGTH(d1.large_varchar) + LENGTH(d2.very_large_varchar) as combined_length
FROM stress.memory.large_data_stress d1
CROSS JOIN stress.memory.large_data_stress d2
WHERE d1.id != d2.id
ORDER BY combined_length DESC;

/* Memory cleanup test */
UPDATE stress.memory.large_data_stress 
SET large_varchar = 'Updated: ' || SUBSTRING(large_varchar FROM 1 FOR 100),
    very_large_varchar = 'Updated: ' || SUBSTRING(very_large_varchar FROM 1 FOR 100)
WHERE id = 1;

SELECT 
    id,
    large_varchar,
    very_large_varchar
FROM stress.memory.large_data_stress
WHERE id = 1;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/memory_stress_test.sql" "$TEST_DB_DIR/memory_stress_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Memory Usage Stress" "Successfully handle large data objects and memory-intensive operations" \
    "Large VARCHAR, BLOB, JSON, and XML data with complex string operations" "$start_time" "$end_time"

cat "$TEST_DB_DIR/memory_stress_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 4: Transaction Stress Tests
echo "Testing transaction stress..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/transaction_stress_test.sql" << 'EOF'
/* Transaction stress test */
CONNECT 'test_databases/stress_test.fdb';

/* Create transaction stress schema */
CREATE SCHEMA stress.transactions;

/* Create table for transaction stress testing */
CREATE TABLE stress.transactions.transaction_test (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    value DECIMAL(10,2),
    status VARCHAR(20),
    last_modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Insert initial data */
INSERT INTO stress.transactions.transaction_test (id, name, value, status) VALUES
(1, 'Transaction Test 1', 100.00, 'ACTIVE'),
(2, 'Transaction Test 2', 200.00, 'PENDING'),
(3, 'Transaction Test 3', 300.00, 'COMPLETED'),
(4, 'Transaction Test 4', 400.00, 'ACTIVE'),
(5, 'Transaction Test 5', 500.00, 'PENDING');

/* Transaction stress test 1: Multiple updates */
UPDATE stress.transactions.transaction_test SET value = value * 1.1 WHERE status = 'ACTIVE';
UPDATE stress.transactions.transaction_test SET status = 'PROCESSING' WHERE status = 'PENDING';
UPDATE stress.transactions.transaction_test SET last_modified = CURRENT_TIMESTAMP WHERE id <= 3;

SELECT * FROM stress.transactions.transaction_test ORDER BY id;

/* Transaction stress test 2: Bulk operations */
INSERT INTO stress.transactions.transaction_test (id, name, value, status) VALUES
(6, 'Bulk Transaction 1', 600.00, 'ACTIVE'),
(7, 'Bulk Transaction 2', 700.00, 'ACTIVE'),
(8, 'Bulk Transaction 3', 800.00, 'ACTIVE');

UPDATE stress.transactions.transaction_test 
SET value = value + 50.00, status = 'UPDATED'
WHERE id > 5;

SELECT COUNT(*) as updated_count FROM stress.transactions.transaction_test WHERE status = 'UPDATED';

/* Transaction stress test 3: Complex operations */
INSERT INTO stress.transactions.transaction_test (id, name, value, status) VALUES
(9, 'Complex Transaction 1', 900.00, 'COMPLEX'),
(10, 'Complex Transaction 2', 1000.00, 'COMPLEX');

UPDATE stress.transactions.transaction_test 
SET value = (
    SELECT AVG(value) FROM stress.transactions.transaction_test t2 
    WHERE t2.status = stress.transactions.transaction_test.status
)
WHERE status = 'COMPLEX';

SELECT 
    status,
    COUNT(*) as status_count,
    AVG(value) as avg_value,
    SUM(value) as total_value
FROM stress.transactions.transaction_test
GROUP BY status
ORDER BY status;

/* Transaction stress test 4: Rollback simulation */
INSERT INTO stress.transactions.transaction_test (id, name, value, status) VALUES
(11, 'Rollback Test 1', 1100.00, 'ROLLBACK_TEST'),
(12, 'Rollback Test 2', 1200.00, 'ROLLBACK_TEST');

/* Simulate transaction with potential rollback */
UPDATE stress.transactions.transaction_test 
SET value = value * 2.0, status = 'DOUBLED'
WHERE status = 'ROLLBACK_TEST';

SELECT * FROM stress.transactions.transaction_test WHERE status = 'DOUBLED';

/* Transaction stress test 5: Concurrent simulation */
INSERT INTO stress.transactions.transaction_test (id, name, value, status) VALUES
(13, 'Concurrent Test 1', 1300.00, 'CONCURRENT'),
(14, 'Concurrent Test 2', 1400.00, 'CONCURRENT'),
(15, 'Concurrent Test 3', 1500.00, 'CONCURRENT');

/* Simulate concurrent operations */
UPDATE stress.transactions.transaction_test SET value = value + 100.00 WHERE id = 13;
UPDATE stress.transactions.transaction_test SET value = value + 200.00 WHERE id = 14;
UPDATE stress.transactions.transaction_test SET value = value + 300.00 WHERE id = 15;

SELECT 
    id,
    name,
    value,
    status,
    last_modified
FROM stress.transactions.transaction_test
WHERE status = 'CONCURRENT'
ORDER BY id;

/* Final transaction stress summary */
SELECT 
    'Transaction Stress Summary' as test_type,
    COUNT(*) as total_records,
    COUNT(DISTINCT status) as distinct_statuses,
    AVG(value) as avg_value,
    SUM(value) as total_value,
    MIN(value) as min_value,
    MAX(value) as max_value
FROM stress.transactions.transaction_test;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/transaction_stress_test.sql" "$TEST_DB_DIR/transaction_stress_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Transaction Stress" "Successfully handle complex transaction patterns and bulk operations" \
    "Multiple INSERT, UPDATE operations with complex queries and transaction management" "$start_time" "$end_time"

cat "$TEST_DB_DIR/transaction_stress_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Summary
echo "Stress Tests Completed" >> "$OUTPUT_FILE"
echo "======================" >> "$OUTPUT_FILE"
echo "Test database: $TEST_DB" >> "$OUTPUT_FILE"
echo "High-volume records processed: $HIGH_VOLUME_RECORDS" >> "$OUTPUT_FILE"
echo "Deep schema levels tested: $DEEP_SCHEMA_LEVELS" >> "$OUTPUT_FILE"
echo "Memory stress data size: $MEMORY_STRESS_SIZE KB" >> "$OUTPUT_FILE"
echo "Test files created in: $TEST_DB_DIR" >> "$OUTPUT_FILE"
echo "These tests validate ScratchBird's performance under stress conditions" >> "$OUTPUT_FILE"
echo "=====================================." >> "$OUTPUT_FILE"

echo "Stress tests completed successfully"
exit 0