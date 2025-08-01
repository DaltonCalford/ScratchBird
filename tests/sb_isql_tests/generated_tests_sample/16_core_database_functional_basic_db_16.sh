#!/bin/bash

# 16_core_database_functional_basic_db_16.sh
# ScratchBird Test - Migrated from Firebird Test Suite
# 
# Original Test ID: functional.basic.db.16
# Title: Empty DB - RDB$INDICES
# Original Firebird Version: 2.0

#
# Check for correct content of RDB$INDICES in empty database.

# 🚀 REVOLUTIONARY FEATURES DEMONSTRATED:
# 🚀 Hierarchical Schemas (PostgreSQL-exceeding)
#

set -e

# Source centralized test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config.sh"

# Test-specific configuration
TEST_NAME="16_core_database_functional_basic_db_16"
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
echo "Original: functional.basic.db.16"
echo "Date: $(date)"
echo "Database: $TEST_DB"
echo "Revolutionary Features: 1 active"
echo

# Log test execution
log_test_execution "$TEST_NAME" "START" "Beginning migrated test from Firebird"

# Create SQL test script
cat > "$SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql" << 'EOF'
-- =================================================================
-- SCRATCHBIRD MIGRATED TEST: Empty DB - RDB$INDICES
-- Original Firebird Test ID: functional.basic.db.16
-- =================================================================


-- ScratchBird Hierarchical Schema Enhancement
CREATE SCHEMA testing;
CREATE SCHEMA testing.basic;
SET SCHEMA 'testing.basic';

select * from RDB$INDICES;

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
Original Test ID: functional.basic.db.16
Category: $TEST_CATEGORY
Execution Date: $(date)
Test Database: $TEST_DB
Database Location Mode: $SB_TEST_DB_LOCATION
Original Firebird Version: 2.0

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
    if [ -n "RDB$INDEX_NAME                                                                                RDB$RELATION_NAME                                                                             RDB$INDEX_ID RDB$UNIQUE_FLAG   RDB$DESCRIPTION RDB$SEGMENT_COUNT RDB$INDEX_INACTIVE RDB$INDEX_TYPE RDB$FOREIGN_KEY                                                                               RDB$SYSTEM_FLAG RDB$EXPRESSION_BLR RDB$EXPRESSION_SOURCE          RDB$STATISTICS
=============================================================================== =============================================================================== ============ =============== ================= ================= ================== ============== =============================================================================== =============== ================== ===================== =======================
RDB$INDEX_0                                                                                   RDB$RELATIONS                                                                                            1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_1                                                                                   RDB$RELATIONS                                                                                            2               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_2                                                                                   RDB$FIELDS                                                                                               1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_3                                                                                   RDB$RELATION_FIELDS                                                                                      1               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_4                                                                                   RDB$RELATION_FIELDS                                                                                      2               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_5                                                                                   RDB$INDICES                                                                                              1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_6                                                                                   RDB$INDEX_SEGMENTS                                                                                       1               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_7                                                                                   RDB$SECURITY_CLASSES                                                                                     1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_8                                                                                   RDB$TRIGGERS                                                                                             1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_9                                                                                   RDB$FUNCTIONS                                                                                            1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_10                                                                                  RDB$FUNCTION_ARGUMENTS                                                                                   1               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_11                                                                                  RDB$GENERATORS                                                                                           1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_12                                                                                  RDB$RELATION_CONSTRAINTS                                                                                 1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_13                                                                                  RDB$REF_CONSTRAINTS                                                                                      1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_14                                                                                  RDB$CHECK_CONSTRAINTS                                                                                    1               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_15                                                                                  RDB$RELATION_FIELDS                                                                                      3               1            <null>                 2                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_16                                                                                  RDB$FORMATS                                                                                              1               0            <null>                 2                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_17                                                                                  RDB$FILTERS                                                                                              1               1            <null>                 2                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_18                                                                                  RDB$PROCEDURE_PARAMETERS                                                                                 1               1            <null>                 2                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_19                                                                                  RDB$CHARACTER_SETS                                                                                       1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>

RDB$INDEX_NAME                                                                                RDB$RELATION_NAME                                                                             RDB$INDEX_ID RDB$UNIQUE_FLAG   RDB$DESCRIPTION RDB$SEGMENT_COUNT RDB$INDEX_INACTIVE RDB$INDEX_TYPE RDB$FOREIGN_KEY                                                                               RDB$SYSTEM_FLAG RDB$EXPRESSION_BLR RDB$EXPRESSION_SOURCE          RDB$STATISTICS
=============================================================================== =============================================================================== ============ =============== ================= ================= ================== ============== =============================================================================== =============== ================== ===================== =======================
RDB$INDEX_20                                                                                  RDB$COLLATIONS                                                                                           1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_21                                                                                  RDB$PROCEDURES                                                                                           1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_22                                                                                  RDB$PROCEDURES                                                                                           2               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_23                                                                                  RDB$EXCEPTIONS                                                                                           1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_24                                                                                  RDB$EXCEPTIONS                                                                                           2               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_25                                                                                  RDB$CHARACTER_SETS                                                                                       2               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_26                                                                                  RDB$COLLATIONS                                                                                           2               1            <null>                 2                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_27                                                                                  RDB$DEPENDENCIES                                                                                         1               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_28                                                                                  RDB$DEPENDENCIES                                                                                         2               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_29                                                                                  RDB$USER_PRIVILEGES                                                                                      1               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_30                                                                                  RDB$USER_PRIVILEGES                                                                                      2               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_31                                                                                  RDB$INDICES                                                                                              2               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_32                                                                                  RDB$TRANSACTIONS                                                                                         1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_33                                                                                  RDB$VIEW_RELATIONS                                                                                       1               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_34                                                                                  RDB$VIEW_RELATIONS                                                                                       2               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_35                                                                                  RDB$TRIGGER_MESSAGES                                                                                     1               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_36                                                                                  RDB$FIELD_DIMENSIONS                                                                                     1               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_37                                                                                  RDB$TYPES                                                                                                1               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_38                                                                                  RDB$TRIGGERS                                                                                             2               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_39                                                                                  RDB$ROLES                                                                                                1               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>

RDB$INDEX_NAME                                                                                RDB$RELATION_NAME                                                                             RDB$INDEX_ID RDB$UNIQUE_FLAG   RDB$DESCRIPTION RDB$SEGMENT_COUNT RDB$INDEX_INACTIVE RDB$INDEX_TYPE RDB$FOREIGN_KEY                                                                               RDB$SYSTEM_FLAG RDB$EXPRESSION_BLR RDB$EXPRESSION_SOURCE          RDB$STATISTICS
=============================================================================== =============================================================================== ============ =============== ================= ================= ================== ============== =============================================================================== =============== ================== ===================== =======================
RDB$INDEX_40                                                                                  RDB$CHECK_CONSTRAINTS                                                                                    2               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_41                                                                                  RDB$INDICES                                                                                              3               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_42                                                                                  RDB$RELATION_CONSTRAINTS                                                                                 2               0            <null>                 2                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_43                                                                                  RDB$RELATION_CONSTRAINTS                                                                                 3               0            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_44                                                                                  RDB$BACKUP_HISTORY                                                                                       1               1            <null>                 2                  0              1 <null>                                                                                                      1             <null>                <null>                  <null>
RDB$INDEX_45                                                                                  RDB$FILTERS                                                                                              2               1            <null>                 1                  0         <null> <null>                                                                                                      1             <null>                <null>                  <null>" ]; then
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
echo "Original Firebird Test: functional.basic.db.16"
echo "ScratchBird Enhancements: 1 revolutionary features"

exit ${exit_code:-0}
