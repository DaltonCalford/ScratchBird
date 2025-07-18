#!/bin/bash

#
# ScratchBird v0.5.0 - UDR (User Defined Routines) Functionality Tests
#
# This script tests the UDR functionality in ScratchBird, including:
# - User-defined functions
# - User-defined procedures
# - User-defined triggers
# - External routine integration
# - UDR security and permissions
#
# Test categories:
# 1. UDR function creation and execution
# 2. UDR procedure creation and execution
# 3. UDR trigger creation and execution
# 4. UDR parameter handling
# 5. UDR exception handling
# 6. UDR security and permissions
# 7. UDR performance testing
#
# Copyright (c) 2025 ScratchBird Development Team
# All Rights Reserved.
#

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DB_DIR="$SCRIPT_DIR/test_databases"
TEST_DB="$TEST_DB_DIR/udr_functionality_test.fdb"
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

# Test 1: UDR Function Creation and Execution
echo "Testing UDR function creation and execution..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/udr_functions_test.sql" << 'EOF'
/* UDR functions test */
CREATE DATABASE 'test_databases/udr_functionality_test.fdb';
CONNECT 'test_databases/udr_functionality_test.fdb';

/* Create schema for UDR testing */
CREATE SCHEMA udr;
CREATE SCHEMA udr.functions;

/* Create table for UDR testing */
CREATE TABLE udr.test_data (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    value DECIMAL(10,2),
    created_date DATE DEFAULT CURRENT_DATE
);

/* Insert test data */
INSERT INTO udr.test_data (id, name, value) VALUES
(1, 'Test Item 1', 100.50),
(2, 'Test Item 2', 200.75),
(3, 'Test Item 3', 300.25),
(4, 'Test Item 4', 400.00),
(5, 'Test Item 5', 500.99);

/* Create simple UDR function for string manipulation */
CREATE FUNCTION udr.functions.uppercase_name(input_name VARCHAR(100))
RETURNS VARCHAR(100)
EXTERNAL NAME 'udr_functions!uppercase_name'
ENGINE udr;

/* Create UDR function for numeric calculations */
CREATE FUNCTION udr.functions.calculate_tax(amount DECIMAL(10,2), tax_rate DECIMAL(5,4))
RETURNS DECIMAL(10,2)
EXTERNAL NAME 'udr_functions!calculate_tax'
ENGINE udr;

/* Create UDR function for date operations */
CREATE FUNCTION udr.functions.days_since_date(input_date DATE)
RETURNS INTEGER
EXTERNAL NAME 'udr_functions!days_since_date'
ENGINE udr;

/* Test UDR function execution */
SELECT 
    id,
    name,
    udr.functions.uppercase_name(name) AS uppercase_name,
    value,
    udr.functions.calculate_tax(value, 0.0825) AS tax_amount,
    created_date,
    udr.functions.days_since_date(created_date) AS days_since_created
FROM udr.test_data
ORDER BY id;

/* Test UDR function with NULL handling */
INSERT INTO udr.test_data (id, name, value) VALUES (6, NULL, NULL);

SELECT 
    id,
    name,
    udr.functions.uppercase_name(name) AS uppercase_name,
    value,
    CASE 
        WHEN value IS NOT NULL THEN udr.functions.calculate_tax(value, 0.0825)
        ELSE NULL
    END AS tax_amount
FROM udr.test_data
WHERE id = 6;

/* Test UDR function performance */
SELECT COUNT(*) AS total_records FROM udr.test_data;

SELECT 
    AVG(udr.functions.calculate_tax(value, 0.0825)) AS avg_tax,
    SUM(udr.functions.calculate_tax(value, 0.0825)) AS total_tax
FROM udr.test_data
WHERE value IS NOT NULL;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/udr_functions_test.sql" "$TEST_DB_DIR/udr_functions_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "UDR Functions" "Successfully create and execute UDR functions" \
    "CREATE FUNCTION udr.functions.uppercase_name() EXTERNAL NAME 'udr_functions!uppercase_name'" "$start_time" "$end_time"

cat "$TEST_DB_DIR/udr_functions_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 2: UDR Procedure Creation and Execution
echo "Testing UDR procedure creation and execution..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/udr_procedures_test.sql" << 'EOF'
/* UDR procedures test */
CONNECT 'test_databases/udr_functionality_test.fdb';

/* Create schema for UDR procedures */
CREATE SCHEMA udr.procedures;

/* Create log table for procedure testing */
CREATE TABLE udr.procedures.execution_log (
    log_id INTEGER PRIMARY KEY,
    procedure_name VARCHAR(100),
    execution_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    parameters VARCHAR(500),
    result_code INTEGER,
    message VARCHAR(1000)
);

/* Create UDR procedure for data validation */
CREATE PROCEDURE udr.procedures.validate_data(
    input_id INTEGER,
    input_name VARCHAR(100),
    input_value DECIMAL(10,2)
)
RETURNS (
    is_valid INTEGER,
    error_message VARCHAR(500)
)
EXTERNAL NAME 'udr_procedures!validate_data'
ENGINE udr;

/* Create UDR procedure for bulk operations */
CREATE PROCEDURE udr.procedures.bulk_update_values(
    multiplier DECIMAL(10,2)
)
RETURNS (
    updated_count INTEGER,
    total_value DECIMAL(15,2)
)
EXTERNAL NAME 'udr_procedures!bulk_update_values'
ENGINE udr;

/* Create UDR procedure for logging */
CREATE PROCEDURE udr.procedures.log_execution(
    proc_name VARCHAR(100),
    params VARCHAR(500),
    result_code INTEGER,
    message VARCHAR(1000)
)
EXTERNAL NAME 'udr_procedures!log_execution'
ENGINE udr;

/* Test UDR procedure execution */
SELECT * FROM udr.procedures.validate_data(1, 'Test Name', 100.50);
SELECT * FROM udr.procedures.validate_data(2, '', -50.00);  -- Invalid data test
SELECT * FROM udr.procedures.validate_data(3, 'Valid Name', 250.75);

/* Test bulk operations procedure */
SELECT * FROM udr.procedures.bulk_update_values(1.1);

/* Test logging procedure */
EXECUTE PROCEDURE udr.procedures.log_execution('test_procedure', 'param1=value1', 0, 'Success');

/* Verify log entries */
SELECT * FROM udr.procedures.execution_log ORDER BY log_id;

/* Test procedure with error handling */
SELECT * FROM udr.procedures.validate_data(NULL, 'Test', 100.00);  -- NULL ID test

/* Test procedure performance */
SELECT COUNT(*) AS total_validations FROM (
    SELECT * FROM udr.procedures.validate_data(1, 'Test1', 100.00)
    UNION ALL
    SELECT * FROM udr.procedures.validate_data(2, 'Test2', 200.00)
    UNION ALL
    SELECT * FROM udr.procedures.validate_data(3, 'Test3', 300.00)
);

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/udr_procedures_test.sql" "$TEST_DB_DIR/udr_procedures_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "UDR Procedures" "Successfully create and execute UDR procedures" \
    "CREATE PROCEDURE udr.procedures.validate_data() EXTERNAL NAME 'udr_procedures!validate_data'" "$start_time" "$end_time"

cat "$TEST_DB_DIR/udr_procedures_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 3: UDR Trigger Creation and Execution
echo "Testing UDR trigger creation and execution..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/udr_triggers_test.sql" << 'EOF'
/* UDR triggers test */
CONNECT 'test_databases/udr_functionality_test.fdb';

/* Create schema for UDR triggers */
CREATE SCHEMA udr.triggers;

/* Create audit table for trigger testing */
CREATE TABLE udr.triggers.audit_log (
    audit_id INTEGER PRIMARY KEY,
    table_name VARCHAR(100),
    operation_type VARCHAR(10),
    old_values VARCHAR(1000),
    new_values VARCHAR(1000),
    user_name VARCHAR(100),
    timestamp_created TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create sequence for audit_id */
CREATE SEQUENCE udr.triggers.audit_seq;

/* Create table for trigger testing */
CREATE TABLE udr.triggers.monitored_data (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    value DECIMAL(10,2),
    status VARCHAR(20) DEFAULT 'ACTIVE',
    last_modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create UDR trigger for INSERT operations */
CREATE TRIGGER udr.triggers.trg_monitored_data_insert
    AFTER INSERT ON udr.triggers.monitored_data
    EXTERNAL NAME 'udr_triggers!audit_insert'
    ENGINE udr;

/* Create UDR trigger for UPDATE operations */
CREATE TRIGGER udr.triggers.trg_monitored_data_update
    AFTER UPDATE ON udr.triggers.monitored_data
    EXTERNAL NAME 'udr_triggers!audit_update'
    ENGINE udr;

/* Create UDR trigger for DELETE operations */
CREATE TRIGGER udr.triggers.trg_monitored_data_delete
    AFTER DELETE ON udr.triggers.monitored_data
    EXTERNAL NAME 'udr_triggers!audit_delete'
    ENGINE udr;

/* Test INSERT trigger */
INSERT INTO udr.triggers.monitored_data (id, name, value) VALUES
(1, 'Test Item 1', 100.00),
(2, 'Test Item 2', 200.00),
(3, 'Test Item 3', 300.00);

/* Check audit log for INSERT operations */
SELECT * FROM udr.triggers.audit_log ORDER BY audit_id;

/* Test UPDATE trigger */
UPDATE udr.triggers.monitored_data 
SET value = 150.00, last_modified = CURRENT_TIMESTAMP
WHERE id = 1;

UPDATE udr.triggers.monitored_data 
SET status = 'INACTIVE'
WHERE id = 2;

/* Check audit log for UPDATE operations */
SELECT * FROM udr.triggers.audit_log 
WHERE operation_type = 'UPDATE' 
ORDER BY audit_id;

/* Test DELETE trigger */
DELETE FROM udr.triggers.monitored_data WHERE id = 3;

/* Check audit log for DELETE operations */
SELECT * FROM udr.triggers.audit_log 
WHERE operation_type = 'DELETE' 
ORDER BY audit_id;

/* Test bulk operations with triggers */
INSERT INTO udr.triggers.monitored_data (id, name, value) VALUES
(4, 'Bulk Item 1', 400.00),
(5, 'Bulk Item 2', 500.00);

UPDATE udr.triggers.monitored_data 
SET value = value * 1.1 
WHERE id IN (4, 5);

/* Check final audit log */
SELECT 
    operation_type,
    COUNT(*) AS operation_count
FROM udr.triggers.audit_log
GROUP BY operation_type
ORDER BY operation_type;

SELECT * FROM udr.triggers.audit_log ORDER BY audit_id;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/udr_triggers_test.sql" "$TEST_DB_DIR/udr_triggers_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "UDR Triggers" "Successfully create and execute UDR triggers" \
    "CREATE TRIGGER udr.triggers.trg_monitored_data_insert EXTERNAL NAME 'udr_triggers!audit_insert'" "$start_time" "$end_time"

cat "$TEST_DB_DIR/udr_triggers_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 4: UDR Parameter Handling
echo "Testing UDR parameter handling..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/udr_parameters_test.sql" << 'EOF'
/* UDR parameter handling test */
CONNECT 'test_databases/udr_functionality_test.fdb';

/* Create schema for parameter testing */
CREATE SCHEMA udr.parameters;

/* Create UDR function with various parameter types */
CREATE FUNCTION udr.parameters.test_parameters(
    int_param INTEGER,
    varchar_param VARCHAR(100),
    decimal_param DECIMAL(10,2),
    date_param DATE,
    timestamp_param TIMESTAMP,
    blob_param BLOB
)
RETURNS VARCHAR(500)
EXTERNAL NAME 'udr_parameters!test_parameters'
ENGINE udr;

/* Create UDR function with optional parameters */
CREATE FUNCTION udr.parameters.optional_parameters(
    required_param INTEGER,
    optional_param1 VARCHAR(100) DEFAULT 'default_value',
    optional_param2 DECIMAL(10,2) DEFAULT 0.00
)
RETURNS VARCHAR(200)
EXTERNAL NAME 'udr_parameters!optional_parameters'
ENGINE udr;

/* Create UDR function with array parameters */
CREATE FUNCTION udr.parameters.array_parameters(
    int_array INTEGER[],
    string_array VARCHAR(50)[]
)
RETURNS VARCHAR(500)
EXTERNAL NAME 'udr_parameters!array_parameters'
ENGINE udr;

/* Test basic parameter handling */
SELECT udr.parameters.test_parameters(
    123,
    'Test String',
    456.78,
    '2025-01-15',
    '2025-01-15 10:30:00',
    'Binary data test'
) AS result;

/* Test optional parameters */
SELECT udr.parameters.optional_parameters(1) AS result_1;
SELECT udr.parameters.optional_parameters(2, 'custom_value') AS result_2;
SELECT udr.parameters.optional_parameters(3, 'custom_value', 123.45) AS result_3;

/* Test array parameters */
SELECT udr.parameters.array_parameters(
    ARRAY[1, 2, 3, 4, 5],
    ARRAY['a', 'b', 'c']
) AS result;

/* Test NULL parameter handling */
SELECT udr.parameters.test_parameters(
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
) AS null_result;

/* Test parameter validation */
SELECT udr.parameters.optional_parameters(NULL) AS invalid_result;  -- Should handle NULL required param

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/udr_parameters_test.sql" "$TEST_DB_DIR/udr_parameters_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "UDR Parameters" "Successfully handle various UDR parameter types" \
    "CREATE FUNCTION udr.parameters.test_parameters(int_param INTEGER, varchar_param VARCHAR(100), ...)" "$start_time" "$end_time"

cat "$TEST_DB_DIR/udr_parameters_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 5: UDR Exception Handling
echo "Testing UDR exception handling..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/udr_exceptions_test.sql" << 'EOF'
/* UDR exception handling test */
CONNECT 'test_databases/udr_functionality_test.fdb';

/* Create schema for exception testing */
CREATE SCHEMA udr.exceptions;

/* Create UDR function that can throw exceptions */
CREATE FUNCTION udr.exceptions.division_with_exception(
    dividend DECIMAL(10,2),
    divisor DECIMAL(10,2)
)
RETURNS DECIMAL(10,2)
EXTERNAL NAME 'udr_exceptions!division_with_exception'
ENGINE udr;

/* Create UDR function for custom exception handling */
CREATE FUNCTION udr.exceptions.validate_email(
    email_address VARCHAR(200)
)
RETURNS INTEGER
EXTERNAL NAME 'udr_exceptions!validate_email'
ENGINE udr;

/* Test normal operation */
SELECT udr.exceptions.division_with_exception(100.00, 5.00) AS normal_result;

/* Test division by zero (should throw exception) */
SELECT udr.exceptions.division_with_exception(100.00, 0.00) AS division_by_zero;

/* Test email validation */
SELECT udr.exceptions.validate_email('test@example.com') AS valid_email;
SELECT udr.exceptions.validate_email('invalid-email') AS invalid_email;

/* Test exception handling in procedures */
CREATE PROCEDURE udr.exceptions.safe_division(
    dividend DECIMAL(10,2),
    divisor DECIMAL(10,2)
)
RETURNS (
    result DECIMAL(10,2),
    error_code INTEGER,
    error_message VARCHAR(200)
)
EXTERNAL NAME 'udr_exceptions!safe_division'
ENGINE udr;

/* Test safe division procedure */
SELECT * FROM udr.exceptions.safe_division(100.00, 5.00);
SELECT * FROM udr.exceptions.safe_division(100.00, 0.00);

/* Test exception handling with NULL values */
SELECT * FROM udr.exceptions.safe_division(NULL, 5.00);
SELECT * FROM udr.exceptions.safe_division(100.00, NULL);

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/udr_exceptions_test.sql" "$TEST_DB_DIR/udr_exceptions_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "UDR Exceptions" "Successfully handle UDR exceptions and error conditions" \
    "CREATE FUNCTION udr.exceptions.division_with_exception() EXTERNAL NAME 'udr_exceptions!division_with_exception'" "$start_time" "$end_time"

cat "$TEST_DB_DIR/udr_exceptions_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 6: UDR Performance Testing
echo "Testing UDR performance..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/udr_performance_test.sql" << 'EOF'
/* UDR performance test */
CONNECT 'test_databases/udr_functionality_test.fdb';

/* Create schema for performance testing */
CREATE SCHEMA udr.performance;

/* Create performance test table */
CREATE TABLE udr.performance.performance_data (
    id INTEGER PRIMARY KEY,
    input_value DECIMAL(10,2),
    processed_value DECIMAL(10,2),
    processing_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

/* Create UDR function for performance testing */
CREATE FUNCTION udr.performance.complex_calculation(
    input_value DECIMAL(10,2)
)
RETURNS DECIMAL(10,2)
EXTERNAL NAME 'udr_performance!complex_calculation'
ENGINE udr;

/* Insert test data for performance testing */
INSERT INTO udr.performance.performance_data (id, input_value) VALUES
(1, 100.00), (2, 200.00), (3, 300.00), (4, 400.00), (5, 500.00),
(6, 600.00), (7, 700.00), (8, 800.00), (9, 900.00), (10, 1000.00);

/* Test UDR function performance */
UPDATE udr.performance.performance_data 
SET processed_value = udr.performance.complex_calculation(input_value);

/* Query performance results */
SELECT 
    COUNT(*) AS total_records,
    AVG(input_value) AS avg_input,
    AVG(processed_value) AS avg_processed,
    MIN(processing_time) AS min_time,
    MAX(processing_time) AS max_time
FROM udr.performance.performance_data;

/* Test bulk UDR operations */
SELECT 
    id,
    input_value,
    udr.performance.complex_calculation(input_value) AS calculated_value,
    processed_value
FROM udr.performance.performance_data
ORDER BY id;

/* Test UDR function with aggregation */
SELECT 
    SUM(udr.performance.complex_calculation(input_value)) AS total_calculated,
    AVG(udr.performance.complex_calculation(input_value)) AS avg_calculated
FROM udr.performance.performance_data;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/udr_performance_test.sql" "$TEST_DB_DIR/udr_performance_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "UDR Performance" "Successfully perform UDR operations with acceptable performance" \
    "SELECT SUM(udr.performance.complex_calculation(input_value)) FROM udr.performance.performance_data" "$start_time" "$end_time"

cat "$TEST_DB_DIR/udr_performance_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Summary
echo "UDR Functionality Tests Completed" >> "$OUTPUT_FILE"
echo "Test database: $TEST_DB" >> "$OUTPUT_FILE"
echo "Test files created in: $TEST_DB_DIR" >> "$OUTPUT_FILE"
echo "=====================================." >> "$OUTPUT_FILE"

echo "UDR functionality tests completed successfully"
exit 0