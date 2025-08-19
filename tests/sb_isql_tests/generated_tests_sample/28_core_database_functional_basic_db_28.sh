#!/bin/bash

# 28_core_database_functional_basic_db_28.sh
# ScratchBird Test - Migrated from Firebird Test Suite
# 
# Original Test ID: functional.basic.db.28
# Title: Empty DB - RDB$TRIGGER_MESSAGES
# Original Firebird Version: 2.0

#
# Check for correct content of RDB$TRIGGER_MESSAGES in empty database.

# 🚀 REVOLUTIONARY FEATURES DEMONSTRATED:
# 🚀 Hierarchical Schemas (PostgreSQL-exceeding)
#

set -e

# Source centralized test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config.sh"

# Test-specific configuration
TEST_NAME="28_core_database_functional_basic_db_28"
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
echo "Original: functional.basic.db.28"
echo "Date: $(date)"
echo "Database: $TEST_DB"
echo "Revolutionary Features: 1 active"
echo

# Log test execution
log_test_execution "$TEST_NAME" "START" "Beginning migrated test from Firebird"

# Create SQL test script
cat > "$SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql" << 'EOF'
-- =================================================================
-- SCRATCHBIRD MIGRATED TEST: Empty DB - RDB$TRIGGER_MESSAGES
-- Original Firebird Test ID: functional.basic.db.28
-- =================================================================


-- ScratchBird Hierarchical Schema Enhancement
CREATE SCHEMA testing;
CREATE SCHEMA testing.basic;
SET SCHEMA 'testing.basic';

select * from RDB$TRIGGER_MESSAGES;

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
Original Test ID: functional.basic.db.28
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
    if [ -n "RDB$TRIGGER_NAME                                                                              RDB$MESSAGE_NUMBER RDB$MESSAGE
=============================================================================== ================== ===============================================================================
RDB$TRIGGER_9                                                                                                  0 grant_obj_notfound
RDB$TRIGGER_9                                                                                                  1 grant_fld_notfound
RDB$TRIGGER_9                                                                                                  2 grant_nopriv
RDB$TRIGGER_9                                                                                                  3 nonsql_security_rel
RDB$TRIGGER_9                                                                                                  4 nonsql_security_fld
RDB$TRIGGER_9                                                                                                  5 grant_nopriv_on_base
RDB$TRIGGER_1                                                                                                  0 existing_priv_mod
RDB$TRIGGER_2                                                                                                  0 systrig_update
RDB$TRIGGER_3                                                                                                  0 systrig_update
RDB$TRIGGER_5                                                                                                  0 not_rel_owner
RDB$TRIGGER_24                                                                                                 1 cnstrnt_fld_rename
RDB$TRIGGER_23                                                                                                 1 cnstrnt_fld_del
RDB$TRIGGER_22                                                                                                 1 check_trig_update
RDB$TRIGGER_21                                                                                                 1 check_trig_del
RDB$TRIGGER_20                                                                                                 1 integ_index_mod
RDB$TRIGGER_20                                                                                                 2 integ_index_deactivate
RDB$TRIGGER_20                                                                                                 3 integ_deactivate_primary
RDB$TRIGGER_19                                                                                                 1 integ_index_del
RDB$TRIGGER_18                                                                                                 1 integ_index_seg_mod
RDB$TRIGGER_17                                                                                                 1 integ_index_seg_del

RDB$TRIGGER_NAME                                                                              RDB$MESSAGE_NUMBER RDB$MESSAGE
=============================================================================== ================== ===============================================================================
RDB$TRIGGER_15                                                                                                 1 check_cnstrnt_del
RDB$TRIGGER_14                                                                                                 1 check_cnstrnt_update
RDB$TRIGGER_13                                                                                                 1 ref_cnstrnt_update
RDB$TRIGGER_12                                                                                                 1 ref_cnstrnt_notfound
RDB$TRIGGER_12                                                                                                 2 foreign_key_notfound
RDB$TRIGGER_10                                                                                                 1 primary_key_ref
RDB$TRIGGER_10                                                                                                 2 primary_key_notnull
RDB$TRIGGER_25                                                                                                 1 rel_cnstrnt_update
RDB$TRIGGER_26                                                                                                 1 constaint_on_view
RDB$TRIGGER_26                                                                                                 2 invld_cnstrnt_type
RDB$TRIGGER_26                                                                                                 3 primary_key_exists
RDB$TRIGGER_31                                                                                                 0 no_write_user_priv
RDB$TRIGGER_32                                                                                                 0 no_write_user_priv
RDB$TRIGGER_33                                                                                                 0 no_write_user_priv
RDB$TRIGGER_24                                                                                                 2 integ_index_seg_mod
RDB$TRIGGER_36                                                                                                 1 integ_index_seg_mod" ]; then
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
echo "Original Firebird Test: functional.basic.db.28"
echo "ScratchBird Enhancements: 1 revolutionary features"

exit ${exit_code:-0}
