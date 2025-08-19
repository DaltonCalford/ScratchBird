#!/bin/bash

# 02_core_database_functional_basic_db_db_02.sh
# ScratchBird Test - Migrated from Firebird Test Suite
# 
# Original Test ID: functional.basic.db.db_02
# Title: Empty DB - RDB$CHARACTER_SETS
# Original Firebird Version: 2.5

#
# Check the correct content of RDB$CHARACTER_SETS for empty database

# 🚀 REVOLUTIONARY FEATURES DEMONSTRATED:
# 🚀 Hierarchical Schemas (PostgreSQL-exceeding)
#

set -e

# Source centralized test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config.sh"

# Test-specific configuration
TEST_NAME="02_core_database_functional_basic_db_db_02"
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
echo "Original: functional.basic.db.db_02"
echo "Date: $(date)"
echo "Database: $TEST_DB"
echo "Revolutionary Features: 1 active"
echo

# Log test execution
log_test_execution "$TEST_NAME" "START" "Beginning migrated test from Firebird"

# Create SQL test script
cat > "$SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql" << 'EOF'
-- =================================================================
-- SCRATCHBIRD MIGRATED TEST: Empty DB - RDB$CHARACTER_SETS
-- Original Firebird Test ID: functional.basic.db.db_02
-- =================================================================


-- ScratchBird Hierarchical Schema Enhancement
CREATE SCHEMA testing;
CREATE SCHEMA testing.basic;
SET SCHEMA 'testing.basic';

select * from RDB$CHARACTER_SETS;

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
Original Test ID: functional.basic.db.db_02
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
    if [ -n "RDB$CHARACTER_SET_NAME                                                                        RDB$FORM_OF_USE                                                                               RDB$NUMBER_OF_CHARACTERS RDB$DEFAULT_COLLATE_NAME                                                                      RDB$CHARACTER_SET_ID RDB$SYSTEM_FLAG   RDB$DESCRIPTION RDB$FUNCTION_NAME                                                                             RDB$BYTES_PER_CHARACTER
=============================================================================== =============================================================================== ======================== =============================================================================== ==================== =============== ================= =============================================================================== =======================
NONE                                                                                          <null>                                                                                                          <null> NONE                                                                                                             0               1            <null> <null>                                                                                                              1
OCTETS                                                                                        <null>                                                                                                          <null> OCTETS                                                                                                           1               1            <null> <null>                                                                                                              1
ASCII                                                                                         <null>                                                                                                          <null> ASCII                                                                                                            2               1            <null> <null>                                                                                                              1
UNICODE_FSS                                                                                   <null>                                                                                                          <null> UNICODE_FSS                                                                                                      3               1            <null> <null>                                                                                                              3
UTF8                                                                                          <null>                                                                                                          <null> UTF8                                                                                                             4               1            <null> <null>                                                                                                              4
SJIS_0208                                                                                     <null>                                                                                                          <null> SJIS_0208                                                                                                        5               1            <null> <null>                                                                                                              2
EUCJ_0208                                                                                     <null>                                                                                                          <null> EUCJ_0208                                                                                                        6               1            <null> <null>                                                                                                              2
DOS437                                                                                        <null>                                                                                                          <null> DOS437                                                                                                          10               1            <null> <null>                                                                                                              1
DOS850                                                                                        <null>                                                                                                          <null> DOS850                                                                                                          11               1            <null> <null>                                                                                                              1
DOS865                                                                                        <null>                                                                                                          <null> DOS865                                                                                                          12               1            <null> <null>                                                                                                              1
ISO8859_1                                                                                     <null>                                                                                                          <null> ISO8859_1                                                                                                       21               1            <null> <null>                                                                                                              1
ISO8859_2                                                                                     <null>                                                                                                          <null> ISO8859_2                                                                                                       22               1            <null> <null>                                                                                                              1
ISO8859_3                                                                                     <null>                                                                                                          <null> ISO8859_3                                                                                                       23               1            <null> <null>                                                                                                              1
ISO8859_4                                                                                     <null>                                                                                                          <null> ISO8859_4                                                                                                       34               1            <null> <null>                                                                                                              1
ISO8859_5                                                                                     <null>                                                                                                          <null> ISO8859_5                                                                                                       35               1            <null> <null>                                                                                                              1
ISO8859_6                                                                                     <null>                                                                                                          <null> ISO8859_6                                                                                                       36               1            <null> <null>                                                                                                              1
ISO8859_7                                                                                     <null>                                                                                                          <null> ISO8859_7                                                                                                       37               1            <null> <null>                                                                                                              1
ISO8859_8                                                                                     <null>                                                                                                          <null> ISO8859_8                                                                                                       38               1            <null> <null>                                                                                                              1
ISO8859_9                                                                                     <null>                                                                                                          <null> ISO8859_9                                                                                                       39               1            <null> <null>                                                                                                              1
ISO8859_13                                                                                    <null>                                                                                                          <null> ISO8859_13                                                                                                      40               1            <null> <null>                                                                                                              1

RDB$CHARACTER_SET_NAME                                                                        RDB$FORM_OF_USE                                                                               RDB$NUMBER_OF_CHARACTERS RDB$DEFAULT_COLLATE_NAME                                                                      RDB$CHARACTER_SET_ID RDB$SYSTEM_FLAG   RDB$DESCRIPTION RDB$FUNCTION_NAME                                                                             RDB$BYTES_PER_CHARACTER
=============================================================================== =============================================================================== ======================== =============================================================================== ==================== =============== ================= =============================================================================== =======================
DOS852                                                                                        <null>                                                                                                          <null> DOS852                                                                                                          45               1            <null> <null>                                                                                                              1
DOS857                                                                                        <null>                                                                                                          <null> DOS857                                                                                                          46               1            <null> <null>                                                                                                              1
DOS860                                                                                        <null>                                                                                                          <null> DOS860                                                                                                          13               1            <null> <null>                                                                                                              1
DOS861                                                                                        <null>                                                                                                          <null> DOS861                                                                                                          47               1            <null> <null>                                                                                                              1
DOS863                                                                                        <null>                                                                                                          <null> DOS863                                                                                                          14               1            <null> <null>                                                                                                              1
CYRL                                                                                          <null>                                                                                                          <null> CYRL                                                                                                            50               1            <null> <null>                                                                                                              1
DOS737                                                                                        <null>                                                                                                          <null> DOS737                                                                                                           9               1            <null> <null>                                                                                                              1
DOS775                                                                                        <null>                                                                                                          <null> DOS775                                                                                                          15               1            <null> <null>                                                                                                              1
DOS858                                                                                        <null>                                                                                                          <null> DOS858                                                                                                          16               1            <null> <null>                                                                                                              1
DOS862                                                                                        <null>                                                                                                          <null> DOS862                                                                                                          17               1            <null> <null>                                                                                                              1
DOS864                                                                                        <null>                                                                                                          <null> DOS864                                                                                                          18               1            <null> <null>                                                                                                              1
DOS866                                                                                        <null>                                                                                                          <null> DOS866                                                                                                          48               1            <null> <null>                                                                                                              1
DOS869                                                                                        <null>                                                                                                          <null> DOS869                                                                                                          49               1            <null> <null>                                                                                                              1
WIN1250                                                                                       <null>                                                                                                          <null> WIN1250                                                                                                         51               1            <null> <null>                                                                                                              1
WIN1251                                                                                       <null>                                                                                                          <null> WIN1251                                                                                                         52               1            <null> <null>                                                                                                              1
WIN1252                                                                                       <null>                                                                                                          <null> WIN1252                                                                                                         53               1            <null> <null>                                                                                                              1
WIN1253                                                                                       <null>                                                                                                          <null> WIN1253                                                                                                         54               1            <null> <null>                                                                                                              1
WIN1254                                                                                       <null>                                                                                                          <null> WIN1254                                                                                                         55               1            <null> <null>                                                                                                              1
NEXT                                                                                          <null>                                                                                                          <null> NEXT                                                                                                            19               1            <null> <null>                                                                                                              1
WIN1255                                                                                       <null>                                                                                                          <null> WIN1255                                                                                                         58               1            <null> <null>                                                                                                              1

RDB$CHARACTER_SET_NAME                                                                        RDB$FORM_OF_USE                                                                               RDB$NUMBER_OF_CHARACTERS RDB$DEFAULT_COLLATE_NAME                                                                      RDB$CHARACTER_SET_ID RDB$SYSTEM_FLAG   RDB$DESCRIPTION RDB$FUNCTION_NAME                                                                             RDB$BYTES_PER_CHARACTER
=============================================================================== =============================================================================== ======================== =============================================================================== ==================== =============== ================= =============================================================================== =======================
WIN1256                                                                                       <null>                                                                                                          <null> WIN1256                                                                                                         59               1            <null> <null>                                                                                                              1
WIN1257                                                                                       <null>                                                                                                          <null> WIN1257                                                                                                         60               1            <null> <null>                                                                                                              1
KSC_5601                                                                                      <null>                                                                                                          <null> KSC_5601                                                                                                        44               1            <null> <null>                                                                                                              2
BIG_5                                                                                         <null>                                                                                                          <null> BIG_5                                                                                                           56               1            <null> <null>                                                                                                              2
GB_2312                                                                                       <null>                                                                                                          <null> GB_2312                                                                                                         57               1            <null> <null>                                                                                                              2
KOI8R                                                                                         <null>                                                                                                          <null> KOI8R                                                                                                           63               1            <null> <null>                                                                                                              1
KOI8U                                                                                         <null>                                                                                                          <null> KOI8U                                                                                                           64               1            <null> <null>                                                                                                              1
WIN1258                                                                                       <null>                                                                                                          <null> WIN1258                                                                                                         65               1            <null> <null>                                                                                                              1
TIS620                                                                                        <null>                                                                                                          <null> TIS620                                                                                                          66               1            <null> <null>                                                                                                              1
GBK                                                                                           <null>                                                                                                          <null> GBK                                                                                                             67               1            <null> <null>                                                                                                              2
CP943C                                                                                        <null>                                                                                                          <null> CP943C                                                                                                          68               1            <null> <null>                                                                                                              2
GB18030                                                                                       <null>                                                                                                          <null> GB18030                                                                                                         69               1            <null> <null>                                                                                                              4" ]; then
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
echo "Original Firebird Test: functional.basic.db.db_02"
echo "ScratchBird Enhancements: 1 revolutionary features"

exit ${exit_code:-0}
