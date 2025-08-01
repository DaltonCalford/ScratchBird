#!/bin/bash

# 01_basic_database_operations.sh
# Comprehensive test of basic ScratchBird database operations using sb_isql
# Tests: Database creation, connection, basic DDL, data manipulation

set -e

# Source centralized test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config.sh"

# Test-specific configuration
TEST_NAME="01_basic_database_operations"
TEST_DB=$(generate_db_path "$TEST_NAME" "basic_operations_test")

# Remove existing test database
case "$SB_TEST_DB_LOCATION" in
    "local"|"temp")
        rm -f "$TEST_DB"
        ;;
    "remote")
        echo "Note: Remote database cleanup handled automatically"
        ;;
esac

echo "=== SCRATCHBIRD BASIC DATABASE OPERATIONS TEST ==="
echo "Test: $TEST_NAME"
echo "Date: $(date)"
echo "Database: $TEST_DB"
echo "ScratchBird Version: $(SCRATCHBIRD="$SB_INSTALL_DIR" "$SB_ISQL" -z 2>&1 | head -1)"
echo

# Log test execution
log_test_execution "$TEST_NAME" "START" "Beginning basic database operations test"

# Create comprehensive SQL test script
cat > "$SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql" << EOF
-- =================================================================
-- SCRATCHBIRD BASIC DATABASE OPERATIONS TEST
-- =================================================================

-- Test 1: Database Creation and Connection
-- ================================================
$(generate_create_db_sql "$TEST_DB")

-- Verify connection
SELECT 'DATABASE_CREATED_SUCCESSFULLY' AS STATUS FROM RDB$DATABASE;

-- Test 2: Basic Schema Operations
-- ===============================
CREATE SCHEMA test_schema;
SET SCHEMA 'test_schema';
SELECT CURRENT_SCHEMA FROM RDB$DATABASE;

-- Test 3: Table Creation with Various Data Types
-- ==============================================
CREATE TABLE customers (
    customer_id INTEGER NOT NULL PRIMARY KEY,
    customer_name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE,
    phone VARCHAR(20),
    registration_date DATE DEFAULT CURRENT_DATE,
    credit_limit DECIMAL(10,2) DEFAULT 1000.00,
    is_active BOOLEAN DEFAULT TRUE,
    notes BLOB SUB_TYPE TEXT,
    created_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Verify table structure
SELECT 
    r.RDB$FIELD_NAME,
    f.RDB$FIELD_TYPE,
    f.RDB$FIELD_LENGTH,
    r.RDB$NULL_FLAG
FROM RDB$RELATION_FIELDS r
JOIN RDB$FIELDS f ON r.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME
WHERE r.RDB$RELATION_NAME = 'CUSTOMERS'
ORDER BY r.RDB$FIELD_POSITION;

-- Test 4: Constraints and Indexes
-- ===============================
-- Add check constraint
ALTER TABLE customers 
    ADD CONSTRAINT chk_credit_limit 
    CHECK (credit_limit >= 0);

-- Create index on frequently queried column
CREATE INDEX idx_customer_email ON customers (email);
CREATE INDEX idx_customer_name ON customers (customer_name);

-- Test 5: Data Manipulation Operations
-- ===================================
-- Insert test data
INSERT INTO customers (customer_id, customer_name, email, phone, credit_limit, notes) 
VALUES (1, 'John Smith', 'john.smith@email.com', '555-1234', 5000.00, 'Premium customer');

INSERT INTO customers (customer_id, customer_name, email, phone, credit_limit) 
VALUES (2, 'Jane Doe', 'jane.doe@email.com', '555-5678', 2500.00);

INSERT INTO customers (customer_id, customer_name, email, credit_limit) 
VALUES (3, 'Bob Johnson', 'bob.johnson@email.com', 1500.00);

-- Verify inserts
SELECT 'INSERTED_RECORDS_COUNT' AS OPERATION, COUNT(*) AS RESULT FROM customers;

-- Test 6: Data Query Operations
-- ============================
-- Basic SELECT
SELECT customer_id, customer_name, email, credit_limit 
FROM customers 
ORDER BY customer_id;

-- WHERE clause filtering
SELECT customer_name, credit_limit 
FROM customers 
WHERE credit_limit > 2000 
ORDER BY credit_limit DESC;

-- Test 7: Update Operations
-- ========================
UPDATE customers 
SET credit_limit = credit_limit * 1.1 
WHERE customer_id = 1;

-- Verify update
SELECT customer_id, customer_name, credit_limit 
FROM customers 
WHERE customer_id = 1;

-- Test 8: Aggregate Functions
-- ==========================
SELECT 
    COUNT(*) AS total_customers,
    AVG(credit_limit) AS avg_credit_limit,
    MAX(credit_limit) AS max_credit_limit,
    MIN(credit_limit) AS min_credit_limit,
    SUM(credit_limit) AS total_credit_exposure
FROM customers;

-- Test 9: Date and Time Functions
-- ==============================
SELECT 
    CURRENT_DATE AS current_date,
    CURRENT_TIME AS current_time,
    CURRENT_TIMESTAMP AS current_timestamp,
    EXTRACT(YEAR FROM CURRENT_DATE) AS current_year,
    EXTRACT(MONTH FROM CURRENT_DATE) AS current_month
FROM RDB$DATABASE;

-- Test 10: String Functions
-- =========================
SELECT 
    customer_name,
    UPPER(customer_name) AS name_upper,
    LOWER(customer_name) AS name_lower,
    CHAR_LENGTH(customer_name) AS name_length,
    SUBSTRING(customer_name FROM 1 FOR 10) AS name_substr
FROM customers;

-- Test 11: NULL Handling
-- =====================
INSERT INTO customers (customer_id, customer_name, email) 
VALUES (4, 'Test Customer', 'test@email.com');

SELECT 
    customer_name,
    phone,
    COALESCE(phone, 'No phone provided') AS phone_display,
    CASE WHEN phone IS NULL THEN 'Missing' ELSE 'Available' END AS phone_status
FROM customers
WHERE customer_id = 4;

-- Test 12: Transaction Testing
-- ===========================
COMMIT;

-- Start explicit transaction
SET TRANSACTION;

INSERT INTO customers (customer_id, customer_name, email) 
VALUES (5, 'Transaction Test', 'transaction@email.com');

-- Verify within transaction
SELECT COUNT(*) AS records_in_transaction FROM customers;

-- Rollback transaction
ROLLBACK;

-- Verify rollback
SELECT COUNT(*) AS records_after_rollback FROM customers;

-- Test 13: View Creation and Usage
-- ================================
CREATE VIEW active_customers AS
SELECT 
    customer_id,
    customer_name,
    email,
    credit_limit,
    registration_date
FROM customers
WHERE is_active = TRUE;

-- Query view
SELECT * FROM active_customers ORDER BY customer_name;

-- Test 14: Sequence/Generator Operations
-- =====================================
CREATE SEQUENCE customer_id_seq
    START WITH 1000
    INCREMENT BY 1;

-- Test sequence
SELECT NEXT VALUE FOR customer_id_seq AS next_id FROM RDB$DATABASE;
SELECT NEXT VALUE FOR customer_id_seq AS next_id FROM RDB$DATABASE;
SELECT NEXT VALUE FOR customer_id_seq AS next_id FROM RDB$DATABASE;

-- Test 15: System Information Queries
-- ===================================
-- Database information
SELECT 
    MON$DATABASE_NAME,
    MON$PAGE_SIZE,
    MON$ODS_MAJOR,
    MON$ODS_MINOR,
    MON$OLDEST_TRANSACTION,
    MON$OLDEST_ACTIVE,
    MON$OLDEST_SNAPSHOT,
    MON$NEXT_TRANSACTION
FROM MON$DATABASE;

-- Connection information
SELECT 
    MON$ATTACHMENT_ID,
    MON$USER,
    MON$ROLE,
    MON$REMOTE_PROTOCOL,
    MON$REMOTE_ADDRESS,
    MON$CHARACTER_SET_ID
FROM MON$ATTACHMENTS
WHERE MON$ATTACHMENT_ID = CURRENT_CONNECTION;

-- Test 16: Cleanup and Verification
-- =================================
-- Drop objects in reverse order
DROP VIEW active_customers;
DROP SEQUENCE customer_id_seq;
DROP INDEX idx_customer_name;
DROP INDEX idx_customer_email;
DROP TABLE customers;
DROP SCHEMA test_schema;

-- Final verification
SELECT 'TEST_COMPLETED_SUCCESSFULLY' AS FINAL_STATUS FROM RDB$DATABASE;

-- Close connection
EXIT;
EOF

echo "Executing comprehensive basic database operations test..."

# Execute test with comprehensive output capture
if execute_sb_isql "$SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt"; then
    test_exit_code=0
    log_test_execution "$TEST_NAME" "SUCCESS" "Test completed successfully"
else
    test_exit_code=$?
    log_test_execution "$TEST_NAME" "ERROR" "Test failed with exit code $test_exit_code"
fi

# Create test execution log
cat > "$SB_TEST_RESULTS_DIR/${TEST_NAME}_results.log" << EOF
=================================================================
SCRATCHBIRD BASIC DATABASE OPERATIONS TEST RESULTS
=================================================================
Test Name: $TEST_NAME
Execution Date: $(date)
Test Database: $TEST_DB
ScratchBird Binary: $SB_ISQL
Database Location Mode: $SB_TEST_DB_LOCATION
Configuration: $(display_test_config 2>/dev/null | head -1 || echo "Centralized configuration active")

Test Components Executed:
- Database creation and connection
- Schema operations (CREATE/SET/DROP)
- Table creation with multiple data types
- Constraint and index operations
- Data manipulation (INSERT/UPDATE/SELECT)
- Transaction handling (COMMIT/ROLLBACK)
- Aggregate and built-in functions
- NULL handling and CASE expressions
- View creation and querying
- Sequence/generator operations
- System monitoring queries
- Complete cleanup operations

Exit Status: $test_exit_code
Output File: ${TEST_NAME}_output.txt
Input File: ${TEST_NAME}_input.sql

=================================================================
EOF

# Check for errors in output
if grep -q "Statement failed" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt"; then
    echo "❌ ERRORS DETECTED in basic database operations test!"
    echo "Check $SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt for details"
    echo
    echo "Error Summary:"
    grep -A 2 -B 2 "Statement failed" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt"
    log_test_execution "$TEST_NAME" "FAILED" "Errors detected in test output"
    exit_code=1
else
    echo "✅ Basic database operations test completed successfully!"
    echo
    echo "Key Results:"
    echo "- Database creation: $(grep -c "DATABASE_CREATED_SUCCESSFULLY" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt") success"
    echo "- Records processed: $(grep -o "records_after_rollback.*[0-9]" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt" | tail -1)"
    echo "- Final status: $(grep "TEST_COMPLETED_SUCCESSFULLY" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt" | wc -l) success"
    log_test_execution "$TEST_NAME" "PASSED" "All validations successful"
    exit_code=0
fi

echo
echo "Test files created:"
echo "- Input SQL: $SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql"
echo "- Output Log: $SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt" 
echo "- Results Summary: $SB_TEST_RESULTS_DIR/${TEST_NAME}_results.log"
echo

# Cleanup test database
cleanup_test_databases "$TEST_NAME"

echo "=== BASIC DATABASE OPERATIONS TEST COMPLETE ==="

exit ${exit_code:-0}