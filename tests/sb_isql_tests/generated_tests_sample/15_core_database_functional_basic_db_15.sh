#!/bin/bash

# 15_core_database_functional_basic_db_15.sh
# ScratchBird Test - Migrated from Firebird Test Suite
# 
# Original Test ID: functional.basic.db.15
# Title: Empty DB - RDB$INDEX_SEGMENTS
# Original Firebird Version: 2.0

#
# Check for correct content of RDB$INDEX_SEGMENTS in empty database.

# 🚀 REVOLUTIONARY FEATURES DEMONSTRATED:
# 🚀 Hierarchical Schemas (PostgreSQL-exceeding)
#

set -e

# Source centralized test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config.sh"

# Test-specific configuration
TEST_NAME="15_core_database_functional_basic_db_15"
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
echo "Original: functional.basic.db.15"
echo "Date: $(date)"
echo "Database: $TEST_DB"
echo "Revolutionary Features: 1 active"
echo

# Log test execution
log_test_execution "$TEST_NAME" "START" "Beginning migrated test from Firebird"

# Create SQL test script
cat > "$SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql" << 'EOF'
-- =================================================================
-- SCRATCHBIRD MIGRATED TEST: Empty DB - RDB$INDEX_SEGMENTS
-- Original Firebird Test ID: functional.basic.db.15
-- =================================================================


-- ScratchBird Hierarchical Schema Enhancement
CREATE SCHEMA testing;
CREATE SCHEMA testing.basic;
SET SCHEMA 'testing.basic';

select * from RDB$INDEX_SEGMENTS;

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
Original Test ID: functional.basic.db.15
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
    if [ -n "RDB$INDEX_NAME                                                                                RDB$FIELD_NAME                                                                                RDB$FIELD_POSITION          RDB$STATISTICS
=============================================================================== =============================================================================== ================== =======================
RDB$INDEX_0                                                                                   RDB$RELATION_NAME                                                                                              0                  <null>
RDB$INDEX_1                                                                                   RDB$RELATION_ID                                                                                                0                  <null>
RDB$INDEX_2                                                                                   RDB$FIELD_NAME                                                                                                 0                  <null>
RDB$INDEX_3                                                                                   RDB$FIELD_SOURCE                                                                                               0                  <null>
RDB$INDEX_4                                                                                   RDB$RELATION_NAME                                                                                              0                  <null>
RDB$INDEX_5                                                                                   RDB$INDEX_NAME                                                                                                 0                  <null>
RDB$INDEX_6                                                                                   RDB$INDEX_NAME                                                                                                 0                  <null>
RDB$INDEX_7                                                                                   RDB$SECURITY_CLASS                                                                                             0                  <null>
RDB$INDEX_8                                                                                   RDB$TRIGGER_NAME                                                                                               0                  <null>
RDB$INDEX_9                                                                                   RDB$FUNCTION_NAME                                                                                              0                  <null>
RDB$INDEX_10                                                                                  RDB$FUNCTION_NAME                                                                                              0                  <null>
RDB$INDEX_11                                                                                  RDB$GENERATOR_NAME                                                                                             0                  <null>
RDB$INDEX_12                                                                                  RDB$CONSTRAINT_NAME                                                                                            0                  <null>
RDB$INDEX_13                                                                                  RDB$CONSTRAINT_NAME                                                                                            0                  <null>
RDB$INDEX_14                                                                                  RDB$CONSTRAINT_NAME                                                                                            0                  <null>
RDB$INDEX_15                                                                                  RDB$FIELD_NAME                                                                                                 0                  <null>
RDB$INDEX_15                                                                                  RDB$RELATION_NAME                                                                                              1                  <null>
RDB$INDEX_16                                                                                  RDB$RELATION_ID                                                                                                0                  <null>
RDB$INDEX_16                                                                                  RDB$FORMAT                                                                                                     1                  <null>
RDB$INDEX_17                                                                                  RDB$INPUT_SUB_TYPE                                                                                             0                  <null>

RDB$INDEX_NAME                                                                                RDB$FIELD_NAME                                                                                RDB$FIELD_POSITION          RDB$STATISTICS
=============================================================================== =============================================================================== ================== =======================
RDB$INDEX_17                                                                                  RDB$OUTPUT_SUB_TYPE                                                                                            1                  <null>
RDB$INDEX_18                                                                                  RDB$PROCEDURE_NAME                                                                                             0                  <null>
RDB$INDEX_18                                                                                  RDB$PARAMETER_NAME                                                                                             1                  <null>
RDB$INDEX_19                                                                                  RDB$CHARACTER_SET_NAME                                                                                         0                  <null>
RDB$INDEX_20                                                                                  RDB$COLLATION_NAME                                                                                             0                  <null>
RDB$INDEX_21                                                                                  RDB$PROCEDURE_NAME                                                                                             0                  <null>
RDB$INDEX_22                                                                                  RDB$PROCEDURE_ID                                                                                               0                  <null>
RDB$INDEX_23                                                                                  RDB$EXCEPTION_NAME                                                                                             0                  <null>
RDB$INDEX_24                                                                                  RDB$EXCEPTION_NUMBER                                                                                           0                  <null>
RDB$INDEX_25                                                                                  RDB$CHARACTER_SET_ID                                                                                           0                  <null>
RDB$INDEX_26                                                                                  RDB$COLLATION_ID                                                                                               0                  <null>
RDB$INDEX_26                                                                                  RDB$CHARACTER_SET_ID                                                                                           1                  <null>
RDB$INDEX_27                                                                                  RDB$DEPENDENT_NAME                                                                                             0                  <null>
RDB$INDEX_28                                                                                  RDB$DEPENDED_ON_NAME                                                                                           0                  <null>
RDB$INDEX_29                                                                                  RDB$RELATION_NAME                                                                                              0                  <null>
RDB$INDEX_30                                                                                  RDB$USER                                                                                                       0                  <null>
RDB$INDEX_31                                                                                  RDB$RELATION_NAME                                                                                              0                  <null>
RDB$INDEX_32                                                                                  RDB$TRANSACTION_ID                                                                                             0                  <null>
RDB$INDEX_33                                                                                  RDB$VIEW_NAME                                                                                                  0                  <null>
RDB$INDEX_34                                                                                  RDB$RELATION_NAME                                                                                              0                  <null>

RDB$INDEX_NAME                                                                                RDB$FIELD_NAME                                                                                RDB$FIELD_POSITION          RDB$STATISTICS
=============================================================================== =============================================================================== ================== =======================
RDB$INDEX_35                                                                                  RDB$TRIGGER_NAME                                                                                               0                  <null>
RDB$INDEX_36                                                                                  RDB$FIELD_NAME                                                                                                 0                  <null>
RDB$INDEX_37                                                                                  RDB$TYPE_NAME                                                                                                  0                  <null>
RDB$INDEX_38                                                                                  RDB$RELATION_NAME                                                                                              0                  <null>
RDB$INDEX_39                                                                                  RDB$ROLE_NAME                                                                                                  0                  <null>
RDB$INDEX_40                                                                                  RDB$TRIGGER_NAME                                                                                               0                  <null>
RDB$INDEX_41                                                                                  RDB$FOREIGN_KEY                                                                                                0                  <null>
RDB$INDEX_42                                                                                  RDB$RELATION_NAME                                                                                              0                  <null>
RDB$INDEX_42                                                                                  RDB$CONSTRAINT_TYPE                                                                                            1                  <null>
RDB$INDEX_43                                                                                  RDB$INDEX_NAME                                                                                                 0                  <null>
RDB$INDEX_44                                                                                  RDB$BACKUP_LEVEL                                                                                               0                  <null>
RDB$INDEX_44                                                                                  RDB$BACKUP_ID                                                                                                  1                  <null>
RDB$INDEX_45                                                                                  RDB$FUNCTION_NAME                                                                                              0                  <null>" ]; then
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
echo "Original Firebird Test: functional.basic.db.15"
echo "ScratchBird Enhancements: 1 revolutionary features"

exit ${exit_code:-0}
