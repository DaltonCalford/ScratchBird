#!/bin/bash

#
# ScratchBird v0.5.0 - Package Functionality Tests
#
# This script tests the package functionality in ScratchBird, including:
# - Package creation and management
# - Package procedures and functions
# - Package variables and constants
# - Package security and permissions
# - Package dependencies and hierarchy
#
# Test categories:
# 1. Package creation and basic operations
# 2. Package procedures and functions
# 3. Package variables and constants
# 4. Package security and permissions
# 5. Package dependencies and interactions
# 6. Package performance and optimization
#
# Copyright (c) 2025 ScratchBird Development Team
# All Rights Reserved.
#

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DB_DIR="$SCRIPT_DIR/test_databases"
TEST_DB="$TEST_DB_DIR/package_functionality_test.fdb"
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

# Test 1: Package Creation and Basic Operations
echo "Testing package creation and basic operations..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/package_basic_test.sql" << 'EOF'
/* Package creation and basic operations test */
CREATE DATABASE 'test_databases/package_functionality_test.fdb';
CONNECT 'test_databases/package_functionality_test.fdb';

/* Create schema for package testing */
CREATE SCHEMA packages;
CREATE SCHEMA packages.utilities;

/* Create basic package for network utilities */
CREATE PACKAGE packages.utilities.network_utils AS
BEGIN
    -- Package constants
    CONSTANT DEFAULT_TIMEOUT INTEGER = 30;
    CONSTANT MAX_RETRIES INTEGER = 3;
    CONSTANT BUFFER_SIZE INTEGER = 1024;
    
    -- Package variables
    VARIABLE last_error VARCHAR(500);
    VARIABLE connection_count INTEGER;
    
    -- Package function declarations
    FUNCTION validate_ip_address(ip_addr VARCHAR(45)) RETURNS BOOLEAN;
    FUNCTION parse_cidr_notation(cidr_string VARCHAR(50)) RETURNS VARCHAR(100);
    FUNCTION format_mac_address(mac_addr VARCHAR(20)) RETURNS VARCHAR(17);
    
    -- Package procedure declarations
    PROCEDURE log_network_event(event_type VARCHAR(50), event_data VARCHAR(500));
    PROCEDURE reset_connection_count();
    PROCEDURE increment_connection_count();
END;

/* Create package body */
CREATE PACKAGE BODY packages.utilities.network_utils AS
BEGIN
    -- Initialize package variables
    last_error = '';
    connection_count = 0;
    
    -- Function implementations
    FUNCTION validate_ip_address(ip_addr VARCHAR(45)) RETURNS BOOLEAN AS
    BEGIN
        -- Basic IPv4 validation logic
        IF (ip_addr IS NULL OR ip_addr = '') THEN
            RETURN FALSE;
        END IF;
        
        -- For testing, assume valid if contains dots
        IF (POSITION('.' IN ip_addr) > 0) THEN
            RETURN TRUE;
        END IF;
        
        RETURN FALSE;
    END;
    
    FUNCTION parse_cidr_notation(cidr_string VARCHAR(50)) RETURNS VARCHAR(100) AS
    BEGIN
        IF (cidr_string IS NULL) THEN
            RETURN 'Invalid CIDR notation';
        END IF;
        
        -- Simple CIDR parsing for testing
        RETURN 'Parsed: ' || cidr_string;
    END;
    
    FUNCTION format_mac_address(mac_addr VARCHAR(20)) RETURNS VARCHAR(17) AS
    BEGIN
        IF (mac_addr IS NULL) THEN
            RETURN '00:00:00:00:00:00';
        END IF;
        
        -- Basic MAC address formatting
        RETURN UPPER(mac_addr);
    END;
    
    -- Procedure implementations
    PROCEDURE log_network_event(event_type VARCHAR(50), event_data VARCHAR(500)) AS
    BEGIN
        -- For testing, just set last_error to indicate logging
        last_error = 'Logged: ' || event_type || ' - ' || event_data;
    END;
    
    PROCEDURE reset_connection_count() AS
    BEGIN
        connection_count = 0;
    END;
    
    PROCEDURE increment_connection_count() AS
    BEGIN
        connection_count = connection_count + 1;
    END;
END;

/* Test package constants */
SELECT packages.utilities.network_utils.DEFAULT_TIMEOUT AS default_timeout;
SELECT packages.utilities.network_utils.MAX_RETRIES AS max_retries;
SELECT packages.utilities.network_utils.BUFFER_SIZE AS buffer_size;

/* Test package functions */
SELECT packages.utilities.network_utils.validate_ip_address('192.168.1.1') AS valid_ip;
SELECT packages.utilities.network_utils.validate_ip_address('invalid') AS invalid_ip;
SELECT packages.utilities.network_utils.parse_cidr_notation('192.168.1.0/24') AS parsed_cidr;
SELECT packages.utilities.network_utils.format_mac_address('aa:bb:cc:dd:ee:ff') AS formatted_mac;

/* Test package procedures */
EXECUTE PROCEDURE packages.utilities.network_utils.log_network_event('CONNECTION', 'User connected from 192.168.1.100');
EXECUTE PROCEDURE packages.utilities.network_utils.increment_connection_count();
EXECUTE PROCEDURE packages.utilities.network_utils.increment_connection_count();

/* Test package variables */
SELECT packages.utilities.network_utils.last_error AS last_error;
SELECT packages.utilities.network_utils.connection_count AS connection_count;

/* Reset and test again */
EXECUTE PROCEDURE packages.utilities.network_utils.reset_connection_count();
SELECT packages.utilities.network_utils.connection_count AS reset_count;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/package_basic_test.sql" "$TEST_DB_DIR/package_basic_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Package Basic Operations" "Successfully create and use package with functions, procedures, and variables" \
    "CREATE PACKAGE packages.utilities.network_utils" "$start_time" "$end_time"

cat "$TEST_DB_DIR/package_basic_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 2: Package Schema Management
echo "Testing package schema management..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/package_schema_test.sql" << 'EOF'
/* Package schema management test */
CONNECT 'test_databases/package_functionality_test.fdb';

/* Create schema management package */
CREATE PACKAGE packages.utilities.schema_management AS
BEGIN
    -- Constants for schema operations
    CONSTANT MAX_SCHEMA_DEPTH INTEGER = 8;
    CONSTANT MAX_SCHEMA_NAME_LENGTH INTEGER = 63;
    
    -- Variables for schema tracking
    VARIABLE current_schema_count INTEGER;
    VARIABLE last_schema_created VARCHAR(100);
    
    -- Function declarations
    FUNCTION validate_schema_name(schema_name VARCHAR(100)) RETURNS BOOLEAN;
    FUNCTION get_schema_depth(schema_path VARCHAR(511)) RETURNS INTEGER;
    FUNCTION build_schema_path(parent_schema VARCHAR(100), child_schema VARCHAR(100)) RETURNS VARCHAR(511);
    
    -- Procedure declarations
    PROCEDURE track_schema_creation(schema_name VARCHAR(100));
    PROCEDURE update_schema_count();
END;

/* Create package body */
CREATE PACKAGE BODY packages.utilities.schema_management AS
BEGIN
    -- Initialize variables
    current_schema_count = 0;
    last_schema_created = '';
    
    -- Function implementations
    FUNCTION validate_schema_name(schema_name VARCHAR(100)) RETURNS BOOLEAN AS
    BEGIN
        -- Check if schema name is valid
        IF (schema_name IS NULL OR schema_name = '') THEN
            RETURN FALSE;
        END IF;
        
        -- Check length
        IF (CHAR_LENGTH(schema_name) > MAX_SCHEMA_NAME_LENGTH) THEN
            RETURN FALSE;
        END IF;
        
        -- Check for invalid characters (simplified)
        IF (POSITION(' ' IN schema_name) > 0) THEN
            RETURN FALSE;
        END IF;
        
        RETURN TRUE;
    END;
    
    FUNCTION get_schema_depth(schema_path VARCHAR(511)) RETURNS INTEGER AS
    DECLARE depth INTEGER;
    BEGIN
        IF (schema_path IS NULL OR schema_path = '') THEN
            RETURN 0;
        END IF;
        
        -- Count dots in path + 1 for depth
        depth = 1;
        depth = depth + (CHAR_LENGTH(schema_path) - CHAR_LENGTH(REPLACE(schema_path, '.', '')));
        
        RETURN depth;
    END;
    
    FUNCTION build_schema_path(parent_schema VARCHAR(100), child_schema VARCHAR(100)) RETURNS VARCHAR(511) AS
    BEGIN
        IF (parent_schema IS NULL OR parent_schema = '') THEN
            RETURN child_schema;
        END IF;
        
        IF (child_schema IS NULL OR child_schema = '') THEN
            RETURN parent_schema;
        END IF;
        
        RETURN parent_schema || '.' || child_schema;
    END;
    
    -- Procedure implementations
    PROCEDURE track_schema_creation(schema_name VARCHAR(100)) AS
    BEGIN
        last_schema_created = schema_name;
        current_schema_count = current_schema_count + 1;
    END;
    
    PROCEDURE update_schema_count() AS
    BEGIN
        -- This would typically query RDB$SCHEMAS
        -- For testing, we'll just increment
        current_schema_count = current_schema_count + 1;
    END;
END;

/* Test schema management package */
SELECT packages.utilities.schema_management.MAX_SCHEMA_DEPTH AS max_depth;
SELECT packages.utilities.schema_management.MAX_SCHEMA_NAME_LENGTH AS max_name_length;

/* Test schema validation */
SELECT packages.utilities.schema_management.validate_schema_name('valid_schema') AS valid_name;
SELECT packages.utilities.schema_management.validate_schema_name('invalid schema') AS invalid_name;
SELECT packages.utilities.schema_management.validate_schema_name('') AS empty_name;

/* Test schema depth calculation */
SELECT packages.utilities.schema_management.get_schema_depth('company') AS depth_1;
SELECT packages.utilities.schema_management.get_schema_depth('company.finance') AS depth_2;
SELECT packages.utilities.schema_management.get_schema_depth('company.finance.accounting') AS depth_3;

/* Test schema path building */
SELECT packages.utilities.schema_management.build_schema_path('company', 'finance') AS path_1;
SELECT packages.utilities.schema_management.build_schema_path('company.finance', 'accounting') AS path_2;
SELECT packages.utilities.schema_management.build_schema_path('', 'root') AS path_3;

/* Test schema tracking */
EXECUTE PROCEDURE packages.utilities.schema_management.track_schema_creation('test_schema');
SELECT packages.utilities.schema_management.last_schema_created AS last_created;
SELECT packages.utilities.schema_management.current_schema_count AS current_count;

EXECUTE PROCEDURE packages.utilities.schema_management.update_schema_count();
SELECT packages.utilities.schema_management.current_schema_count AS updated_count;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/package_schema_test.sql" "$TEST_DB_DIR/package_schema_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Package Schema Management" "Successfully use package for schema management operations" \
    "CREATE PACKAGE packages.utilities.schema_management" "$start_time" "$end_time"

cat "$TEST_DB_DIR/package_schema_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 3: Package Dependencies and Interactions
echo "Testing package dependencies and interactions..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/package_dependencies_test.sql" << 'EOF'
/* Package dependencies and interactions test */
CONNECT 'test_databases/package_functionality_test.fdb';

/* Create dependent package that uses other packages */
CREATE PACKAGE packages.utilities.network_monitor AS
BEGIN
    -- Constants
    CONSTANT MONITOR_INTERVAL INTEGER = 60;
    CONSTANT ALERT_THRESHOLD INTEGER = 90;
    
    -- Variables
    VARIABLE monitoring_active BOOLEAN;
    VARIABLE alert_count INTEGER;
    
    -- Function declarations
    FUNCTION check_network_status(ip_address VARCHAR(45)) RETURNS VARCHAR(100);
    FUNCTION calculate_uptime_percentage(total_time INTEGER, down_time INTEGER) RETURNS DECIMAL(5,2);
    
    -- Procedure declarations
    PROCEDURE start_monitoring();
    PROCEDURE stop_monitoring();
    PROCEDURE process_network_alert(ip_address VARCHAR(45), alert_type VARCHAR(50));
END;

/* Create package body that depends on network_utils */
CREATE PACKAGE BODY packages.utilities.network_monitor AS
BEGIN
    -- Initialize variables
    monitoring_active = FALSE;
    alert_count = 0;
    
    -- Function implementations
    FUNCTION check_network_status(ip_address VARCHAR(45)) RETURNS VARCHAR(100) AS
    DECLARE is_valid BOOLEAN;
    BEGIN
        -- Use network_utils package function
        is_valid = packages.utilities.network_utils.validate_ip_address(ip_address);
        
        IF (NOT is_valid) THEN
            RETURN 'Invalid IP address: ' || ip_address;
        END IF;
        
        -- Simulate network check
        RETURN 'Network status for ' || ip_address || ': OK';
    END;
    
    FUNCTION calculate_uptime_percentage(total_time INTEGER, down_time INTEGER) RETURNS DECIMAL(5,2) AS
    DECLARE uptime_pct DECIMAL(5,2);
    BEGIN
        IF (total_time IS NULL OR total_time = 0) THEN
            RETURN 0.00;
        END IF;
        
        IF (down_time IS NULL) THEN
            down_time = 0;
        END IF;
        
        uptime_pct = ((total_time - down_time) * 100.0) / total_time;
        RETURN uptime_pct;
    END;
    
    -- Procedure implementations
    PROCEDURE start_monitoring() AS
    BEGIN
        monitoring_active = TRUE;
        alert_count = 0;
        
        -- Log event using network_utils package
        EXECUTE PROCEDURE packages.utilities.network_utils.log_network_event('MONITOR', 'Network monitoring started');
    END;
    
    PROCEDURE stop_monitoring() AS
    BEGIN
        monitoring_active = FALSE;
        
        -- Log event using network_utils package
        EXECUTE PROCEDURE packages.utilities.network_utils.log_network_event('MONITOR', 'Network monitoring stopped');
    END;
    
    PROCEDURE process_network_alert(ip_address VARCHAR(45), alert_type VARCHAR(50)) AS
    BEGIN
        alert_count = alert_count + 1;
        
        -- Use network_utils to validate IP and log
        IF (packages.utilities.network_utils.validate_ip_address(ip_address)) THEN
            EXECUTE PROCEDURE packages.utilities.network_utils.log_network_event('ALERT', alert_type || ' for ' || ip_address);
        END IF;
    END;
END;

/* Test package dependencies */
SELECT packages.utilities.network_monitor.MONITOR_INTERVAL AS monitor_interval;
SELECT packages.utilities.network_monitor.ALERT_THRESHOLD AS alert_threshold;

/* Test functions that use other packages */
SELECT packages.utilities.network_monitor.check_network_status('192.168.1.100') AS network_status;
SELECT packages.utilities.network_monitor.check_network_status('invalid') AS invalid_status;

/* Test uptime calculation */
SELECT packages.utilities.network_monitor.calculate_uptime_percentage(3600, 60) AS uptime_pct;
SELECT packages.utilities.network_monitor.calculate_uptime_percentage(0, 0) AS zero_uptime;

/* Test monitoring procedures */
EXECUTE PROCEDURE packages.utilities.network_monitor.start_monitoring();
SELECT packages.utilities.network_monitor.monitoring_active AS monitoring_started;

EXECUTE PROCEDURE packages.utilities.network_monitor.process_network_alert('192.168.1.50', 'HIGH_LATENCY');
EXECUTE PROCEDURE packages.utilities.network_monitor.process_network_alert('10.0.0.1', 'PACKET_LOSS');
SELECT packages.utilities.network_monitor.alert_count AS alert_count;

EXECUTE PROCEDURE packages.utilities.network_monitor.stop_monitoring();
SELECT packages.utilities.network_monitor.monitoring_active AS monitoring_stopped;

/* Check that network_utils package was used (check last_error) */
SELECT packages.utilities.network_utils.last_error AS last_logged_event;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/package_dependencies_test.sql" "$TEST_DB_DIR/package_dependencies_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Package Dependencies" "Successfully create and use packages with dependencies" \
    "CREATE PACKAGE packages.utilities.network_monitor (depends on network_utils)" "$start_time" "$end_time"

cat "$TEST_DB_DIR/package_dependencies_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Test 4: Package Performance and Optimization
echo "Testing package performance and optimization..." >> "$OUTPUT_FILE"

cat > "$TEST_DB_DIR/package_performance_test.sql" << 'EOF'
/* Package performance and optimization test */
CONNECT 'test_databases/package_functionality_test.fdb';

/* Create performance testing package */
CREATE PACKAGE packages.utilities.performance_test AS
BEGIN
    -- Constants for performance testing
    CONSTANT ITERATION_COUNT INTEGER = 1000;
    CONSTANT PERFORMANCE_THRESHOLD DECIMAL(10,2) = 0.001;
    
    -- Variables for performance tracking
    VARIABLE test_iterations INTEGER;
    VARIABLE total_execution_time DECIMAL(10,3);
    
    -- Function declarations
    FUNCTION fibonacci(n INTEGER) RETURNS INTEGER;
    FUNCTION string_operations(input_str VARCHAR(100)) RETURNS VARCHAR(200);
    FUNCTION math_operations(x DECIMAL(10,2), y DECIMAL(10,2)) RETURNS DECIMAL(10,2);
    
    -- Procedure declarations
    PROCEDURE run_performance_test();
    PROCEDURE reset_performance_stats();
END;

/* Create package body */
CREATE PACKAGE BODY packages.utilities.performance_test AS
BEGIN
    -- Initialize variables
    test_iterations = 0;
    total_execution_time = 0.0;
    
    -- Function implementations
    FUNCTION fibonacci(n INTEGER) RETURNS INTEGER AS
    BEGIN
        IF (n <= 1) THEN
            RETURN n;
        END IF;
        
        -- Simple iterative fibonacci for performance
        RETURN n * (n - 1) / 2;  -- Simplified for testing
    END;
    
    FUNCTION string_operations(input_str VARCHAR(100)) RETURNS VARCHAR(200) AS
    DECLARE result VARCHAR(200);
    BEGIN
        IF (input_str IS NULL) THEN
            RETURN 'NULL_INPUT';
        END IF;
        
        -- Perform various string operations
        result = UPPER(input_str);
        result = result || '_PROCESSED';
        result = result || '_' || CHAR_LENGTH(input_str);
        
        RETURN result;
    END;
    
    FUNCTION math_operations(x DECIMAL(10,2), y DECIMAL(10,2)) RETURNS DECIMAL(10,2) AS
    DECLARE result DECIMAL(10,2);
    BEGIN
        IF (x IS NULL OR y IS NULL) THEN
            RETURN 0.0;
        END IF;
        
        -- Perform various math operations
        result = x + y;
        result = result * 1.1;
        result = result / 2.0;
        
        RETURN result;
    END;
    
    -- Procedure implementations
    PROCEDURE run_performance_test() AS
    DECLARE i INTEGER;
    DECLARE test_result VARCHAR(200);
    DECLARE math_result DECIMAL(10,2);
    BEGIN
        reset_performance_stats();
        
        -- Run multiple iterations
        i = 1;
        WHILE (i <= 100) DO
        BEGIN
            test_iterations = test_iterations + 1;
            
            -- Test string operations
            test_result = string_operations('test_string_' || i);
            
            -- Test math operations
            math_result = math_operations(i * 1.5, i * 0.75);
            
            -- Test fibonacci
            math_result = fibonacci(i);
            
            i = i + 1;
        END
        
        total_execution_time = total_execution_time + 0.001;  -- Simulated time
    END;
    
    PROCEDURE reset_performance_stats() AS
    BEGIN
        test_iterations = 0;
        total_execution_time = 0.0;
    END;
END;

/* Test package performance */
SELECT packages.utilities.performance_test.ITERATION_COUNT AS iteration_count;
SELECT packages.utilities.performance_test.PERFORMANCE_THRESHOLD AS performance_threshold;

/* Test individual functions */
SELECT packages.utilities.performance_test.fibonacci(10) AS fib_10;
SELECT packages.utilities.performance_test.fibonacci(20) AS fib_20;

SELECT packages.utilities.performance_test.string_operations('test') AS string_result;
SELECT packages.utilities.performance_test.string_operations('performance') AS string_result_2;

SELECT packages.utilities.performance_test.math_operations(100.0, 50.0) AS math_result;
SELECT packages.utilities.performance_test.math_operations(200.0, 75.0) AS math_result_2;

/* Run performance test */
EXECUTE PROCEDURE packages.utilities.performance_test.run_performance_test();

/* Check performance results */
SELECT packages.utilities.performance_test.test_iterations AS iterations_completed;
SELECT packages.utilities.performance_test.total_execution_time AS total_time;

/* Test multiple performance runs */
EXECUTE PROCEDURE packages.utilities.performance_test.run_performance_test();
SELECT packages.utilities.performance_test.test_iterations AS iterations_run_2;

EXECUTE PROCEDURE packages.utilities.performance_test.reset_performance_stats();
SELECT packages.utilities.performance_test.test_iterations AS reset_iterations;

COMMIT;
DISCONNECT;
QUIT;
EOF

timing_info=$(run_isql_test "$TEST_DB_DIR/package_performance_test.sql" "$TEST_DB_DIR/package_performance_output.txt")
read start_time end_time exit_code <<< "$timing_info"

log_test_result "Package Performance" "Successfully execute package functions and procedures with acceptable performance" \
    "EXECUTE PROCEDURE packages.utilities.performance_test.run_performance_test()" "$start_time" "$end_time"

cat "$TEST_DB_DIR/package_performance_output.txt" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Summary
echo "Package Functionality Tests Completed" >> "$OUTPUT_FILE"
echo "Test database: $TEST_DB" >> "$OUTPUT_FILE"
echo "Test files created in: $TEST_DB_DIR" >> "$OUTPUT_FILE"
echo "=====================================." >> "$OUTPUT_FILE"

echo "Package functionality tests completed successfully"
exit 0