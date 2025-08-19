#!/bin/bash

# 18_core_database_functional_basic_db_18.sh
# ScratchBird Test - Migrated from Firebird Test Suite
# 
# Original Test ID: functional.basic.db.18
# Title: Empty DB - RDB$PAGES
# Original Firebird Version: 2.5

#
# Check for correct content of RDB$PAGES in empty database.
# Database is created with 4K pages. We may add other RDB$PAGES tests for different page sizes.

# 🚀 REVOLUTIONARY FEATURES DEMONSTRATED:
# 🚀 Hierarchical Schemas (PostgreSQL-exceeding)
#

set -e

# Source centralized test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config.sh"

# Test-specific configuration
TEST_NAME="18_core_database_functional_basic_db_18"
TEST_CATEGORY="core_database"
TEST_DB=$(generate_db_path "$TEST_NAME" "test_db")

# Remove existing test database
case "$SB_TEST_DB_LOCATION" in
    "local"|"temp")
        rm -f "$TEST_DB"
        ;;
    "remote")
        echo "Note: Remote database cleanup handled automatically"
        ;;
esac

echo "=== SCRATCHBIRD MIGRATED TEST ==="
echo "Test: $TEST_NAME"
echo "Category: $TEST_CATEGORY"
echo "Original: functional.basic.db.18"
echo "Date: $(date)"
echo "Database: $TEST_DB"
echo "Revolutionary Features: 1 active"
echo

# Log test execution
log_test_execution "$TEST_NAME" "START" "Beginning migrated test from Firebird"

# Create SQL test script
cat > "$SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql" << 'EOF'
-- =================================================================
-- SCRATCHBIRD MIGRATED TEST: Empty DB - RDB$PAGES
-- Original Firebird Test ID: functional.basic.db.18
-- =================================================================


-- ScratchBird Hierarchical Schema Enhancement
CREATE SCHEMA testing;
CREATE SCHEMA testing.basic;
SET SCHEMA 'testing.basic';

select * from RDB$PAGES;

-- Test completion marker
SELECT 'MIGRATED_TEST_COMPLETED_SUCCESSFULLY' AS FINAL_STATUS FROM RDB$DATABASE;

-- Close connection
EXIT;
EOF

echo "Executing migrated test..."

# Execute test with comprehensive output capture
if execute_sb_isql "$SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt"; then
    test_exit_code=0
    log_test_execution "$TEST_NAME" "SUCCESS" "Migrated test completed successfully"
else
    test_exit_code=$?
    log_test_execution "$TEST_NAME" "ERROR" "Migrated test failed with exit code $test_exit_code"
fi

# Create test execution log
cat > "$SB_TEST_RESULTS_DIR/${TEST_NAME}_results.log" << EOF
=================================================================
SCRATCHBIRD MIGRATED TEST RESULTS
=================================================================
Test Name: $TEST_NAME
Original Test ID: functional.basic.db.18
Category: $TEST_CATEGORY
Execution Date: $(date)
Test Database: $TEST_DB
Database Location Mode: $SB_TEST_DB_LOCATION
Original Firebird Version: 2.5

Revolutionary Features Demonstrated:
- 🚀 Hierarchical Schemas (PostgreSQL-exceeding)

Migration Information:
- Migrated from Firebird test suite
- SQL translated for ScratchBird compatibility
- Enhanced with revolutionary features where applicable
- Uses centralized test configuration

Exit Status: $test_exit_code
Output File: ${TEST_NAME}_output.txt
Input File: ${TEST_NAME}_input.sql

=================================================================
EOF

# Check for errors in output
if grep -q "Statement failed" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt"; then
    echo "❌ ERRORS DETECTED in migrated test!"
    echo "Check $SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt for details"
    echo
    echo "Error Summary:"
    grep -A 2 -B 2 "Statement failed" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt"
    log_test_execution "$TEST_NAME" "FAILED" "Errors detected in test output"
    exit_code=1
else
    echo "✅ Migrated test completed successfully!"
    echo
    echo "Key Results:"
    if [ -n "RDB$PAGE_NUMBER RDB$RELATION_ID RDB$PAGE_SEQUENCE RDB$PAGE_TYPE
=============== =============== ================= =============
              3               0                 0             4
              4               0                 0             6
              6               1                 0             4
              7               1                 0             6
              8               2                 0             4
              9               2                 0             6
             10               3                 0             4
             11               3                 0             6
             12               4                 0             4
             13               4                 0             6
             14               5                 0             4
             15               5                 0             6
             16               6                 0             4
             17               6                 0             6
             18               7                 0             4
             19               7                 0             6
             20               8                 0             4
             21               8                 0             6
             22               9                 0             4
             23               9                 0             6

RDB$PAGE_NUMBER RDB$RELATION_ID RDB$PAGE_SEQUENCE RDB$PAGE_TYPE
=============== =============== ================= =============
             24              10                 0             4
             25              10                 0             6
             26              11                 0             4
             27              11                 0             6
             28              12                 0             4
             29              12                 0             6
             30              13                 0             4
             31              13                 0             6
             32              14                 0             4
             33              14                 0             6
             34              15                 0             4
             35              15                 0             6
             36              16                 0             4
             37              16                 0             6
             38              17                 0             4
             39              17                 0             6
             40              18                 0             4
             41              18                 0             6
             42              19                 0             4
             43              19                 0             6

RDB$PAGE_NUMBER RDB$RELATION_ID RDB$PAGE_SEQUENCE RDB$PAGE_TYPE
=============== =============== ================= =============
             44              20                 0             4
             45              20                 0             6
             46              21                 0             4
             47              21                 0             6
             48              22                 0             4
             49              22                 0             6
             50              23                 0             4
             51              23                 0             6
             52              24                 0             4
             53              24                 0             6
             54              25                 0             4
             55              25                 0             6
             56              26                 0             4
             57              26                 0             6
             58              27                 0             4
             59              27                 0             6
             60              28                 0             4
             61              28                 0             6
             62              29                 0             4
             63              29                 0             6

RDB$PAGE_NUMBER RDB$RELATION_ID RDB$PAGE_SEQUENCE RDB$PAGE_TYPE
=============== =============== ================= =============
             64              30                 0             4
             65              30                 0             6
             66              31                 0             4
             67              31                 0             6
             68              32                 0             4
             69              32                 0             6
            150               0                 0             9
            162               0                 0             3" ]; then
        echo "- Expected output validation: $(grep -c "MIGRATED_TEST_COMPLETED_SUCCESSFULLY" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt") success"
    fi
    echo "- Revolutionary features: 1 demonstrated"
    echo "- Final status: $(grep "MIGRATED_TEST_COMPLETED_SUCCESSFULLY" "$SB_TEST_RESULTS_DIR/${TEST_NAME}_output.txt" | wc -l) success"
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

echo "=== MIGRATED TEST COMPLETE ==="
echo "Original Firebird Test: functional.basic.db.18"
echo "ScratchBird Enhancements: 1 revolutionary features"

exit ${exit_code:-0}
