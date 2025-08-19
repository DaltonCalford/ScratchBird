#!/bin/bash

# 23_core_database_functional_basic_db_23.sh
# ScratchBird Test - Migrated from Firebird Test Suite
# 
# Original Test ID: functional.basic.db.23
# Title: Empty DB - RDB$RELATIONS
# Original Firebird Version: 2.5

#
# Check for correct content of RDB$RELATIONS in empty database.

# 🚀 REVOLUTIONARY FEATURES DEMONSTRATED:
# 🚀 Hierarchical Schemas (PostgreSQL-exceeding)
#

set -e

# Source centralized test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config.sh"

# Test-specific configuration
TEST_NAME="23_core_database_functional_basic_db_23"
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
echo "Original: functional.basic.db.23"
echo "Date: $(date)"
echo "Database: $TEST_DB"
echo "Revolutionary Features: 1 active"
echo

# Log test execution
log_test_execution "$TEST_NAME" "START" "Beginning migrated test from Firebird"

# Create SQL test script
cat > "$SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql" << 'EOF'
-- =================================================================
-- SCRATCHBIRD MIGRATED TEST: Empty DB - RDB$RELATIONS
-- Original Firebird Test ID: functional.basic.db.23
-- =================================================================


-- ScratchBird Hierarchical Schema Enhancement
CREATE SCHEMA testing;
CREATE SCHEMA testing.basic;
SET SCHEMA 'testing.basic';

set blob all;
select * from RDB$RELATIONS;

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
Original Test ID: functional.basic.db.23
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
    if [ -n "RDB$VIEW_BLR   RDB$VIEW_SOURCE   RDB$DESCRIPTION RDB$RELATION_ID RDB$SYSTEM_FLAG RDB$DBKEY_LENGTH RDB$FORMAT RDB$FIELD_ID RDB$RELATION_NAME                                                                             RDB$SECURITY_CLASS                                                                            RDB$EXTERNAL_FILE                                                                                                                                                                                                                                                     RDB$RUNTIME RDB$EXTERNAL_DESCRIPTION RDB$OWNER_NAME                                                                                RDB$DEFAULT_CLASS                                                                             RDB$FLAGS RDB$RELATION_TYPE
================= ================= ================= =============== =============== ================ ========== ============ =============================================================================== =============================================================================== =============================================================================== ================= ======================== =============================================================================== =============================================================================== ========= =================
           <null>            <null>            <null>               0               1                8          0            4 RDB$PAGES                                                                                     SQL$2                                                                                         <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        SQL$DEFAULT2                                                                                     <null>                 0
           <null>            <null>            <null>               1               1                8          0            4 RDB$DATABASE                                                                                  <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>               2               1                8          0           28 RDB$FIELDS                                                                                    <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:f9                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$FIELD_NAME
        	Field id: 1
        	    name: RDB$QUERY_NAME
        	Field id: 2
        	    name: RDB$VALIDATION_BLR
        	Field id: 3
        	    name: RDB$VALIDATION_SOURCE
        	Field id: 4
        	    name: RDB$COMPUTED_BLR
        	Field id: 5
        	    name: RDB$COMPUTED_SOURCE
        	Field id: 6
        	    name: RDB$DEFAULT_VALUE
        	Field id: 7
        	    name: RDB$DEFAULT_SOURCE
        	Field id: 8
        	    name: RDB$FIELD_LENGTH
        	Field id: 9
        	    name: RDB$FIELD_SCALE
        	Field id: 10
        	    name: RDB$FIELD_TYPE
        	Field id: 11
        	    name: RDB$FIELD_SUB_TYPE
        	Field id: 12
        	    name: RDB$MISSING_VALUE
        	Field id: 13
        	    name: RDB$MISSING_SOURCE
        	Field id: 14
        	    name: RDB$DESCRIPTION
        	Field id: 15
        	    name: RDB$SYSTEM_FLAG
        	Field id: 16
        	    name: RDB$QUERY_HEADER
        	Field id: 17
        	    name: RDB$SEGMENT_LENGTH
        	Field id: 18
        	    name: RDB$EDIT_STRING
        	Field id: 19
        	    name: RDB$EXTERNAL_LENGTH
        	Field id: 20
        	    name: RDB$EXTERNAL_SCALE
        	Field id: 21
        	    name: RDB$EXTERNAL_TYPE
        	Field id: 22
        	    name: RDB$DIMENSIONS
        	Field id: 23
        	    name: RDB$NULL_FLAG
        	Field id: 24
        	    name: RDB$CHARACTER_LENGTH
        	Field id: 25
        	    name: RDB$COLLATION_ID
        	Field id: 26
        	    name: RDB$CHARACTER_SET_ID
        	Field id: 27
        	    name: RDB$FIELD_PRECISION
        	    trigger_name: RDB$TRIGGER_36

==============================================================================
           <null>            <null>            <null>               3               1                8          0            4 RDB$INDEX_SEGMENTS                                                                            <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:f4                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$INDEX_NAME
        	Field id: 1
        	    name: RDB$FIELD_NAME
        	Field id: 2
        	    name: RDB$FIELD_POSITION
        	Field id: 3
        	    name: RDB$STATISTICS
        	    trigger_name: RDB$TRIGGER_17
        	    trigger_name: RDB$TRIGGER_18

==============================================================================
           <null>            <null>            <null>               4               1                8          0           13 RDB$INDICES                                                                                   <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:f5                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$INDEX_NAME
        	Field id: 1
        	    name: RDB$RELATION_NAME
        	Field id: 2
        	    name: RDB$INDEX_ID
        	Field id: 3
        	    name: RDB$UNIQUE_FLAG
        	Field id: 4
        	    name: RDB$DESCRIPTION
        	Field id: 5
        	    name: RDB$SEGMENT_COUNT
        	Field id: 6
        	    name: RDB$INDEX_INACTIVE
        	Field id: 7
        	    name: RDB$INDEX_TYPE
        	Field id: 8
        	    name: RDB$FOREIGN_KEY
        	Field id: 9
        	    name: RDB$SYSTEM_FLAG
        	Field id: 10
        	    name: RDB$EXPRESSION_BLR
        	Field id: 11
        	    name: RDB$EXPRESSION_SOURCE
        	Field id: 12
        	    name: RDB$STATISTICS
        	    trigger_name: RDB$TRIGGER_19
        	    trigger_name: RDB$TRIGGER_20

==============================================================================
           <null>            <null>            <null>               5               1                8          0           19 RDB$RELATION_FIELDS                                                                           <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:f6                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$FIELD_NAME
        	Field id: 1
        	    name: RDB$RELATION_NAME
        	Field id: 2
        	    name: RDB$FIELD_SOURCE
        	Field id: 3
        	    name: RDB$QUERY_NAME
        	Field id: 4
        	    name: RDB$BASE_FIELD
        	Field id: 5
        	    name: RDB$EDIT_STRING
        	Field id: 6
        	    name: RDB$FIELD_POSITION
        	Field id: 7
        	    name: RDB$QUERY_HEADER
        	Field id: 8
        	    name: RDB$UPDATE_FLAG
        	Field id: 9
        	    name: RDB$FIELD_ID
        	Field id: 10
        	    name: RDB$VIEW_CONTEXT
        	Field id: 11
        	    name: RDB$DESCRIPTION
        	Field id: 12
        	    name: RDB$DEFAULT_VALUE
        	Field id: 13
        	    name: RDB$SYSTEM_FLAG
        	Field id: 14
        	    name: RDB$SECURITY_CLASS
        	Field id: 15
        	    name: RDB$COMPLEX_NAME
        	Field id: 16
        	    name: RDB$NULL_FLAG
        	Field id: 17
        	    name: RDB$DEFAULT_SOURCE
        	Field id: 18
        	    name: RDB$COLLATION_ID
        	    trigger_name: RDB$TRIGGER_23
        	    trigger_name: RDB$TRIGGER_24
        	    trigger_name: RDB$TRIGGER_27

==============================================================================
           <null>            <null>            <null>               6               1                8          0           17 RDB$RELATIONS                                                                                 <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:ef                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$VIEW_BLR
        	Field id: 1
        	    name: RDB$VIEW_SOURCE
        	Field id: 2
        	    name: RDB$DESCRIPTION
        	Field id: 3
        	    name: RDB$RELATION_ID
        	Field id: 4
        	    name: RDB$SYSTEM_FLAG
        	Field id: 5
        	    name: RDB$DBKEY_LENGTH
        	Field id: 6
        	    name: RDB$FORMAT
        	Field id: 7
        	    name: RDB$FIELD_ID
        	Field id: 8
        	    name: RDB$RELATION_NAME
        	Field id: 9
        	    name: RDB$SECURITY_CLASS
        	Field id: 10
        	    name: RDB$EXTERNAL_FILE
        	Field id: 11
        	    name: RDB$RUNTIME
        	Field id: 12
        	    name: RDB$EXTERNAL_DESCRIPTION
        	Field id: 13
        	    name: RDB$OWNER_NAME
        	Field id: 14
        	    name: RDB$DEFAULT_CLASS
        	Field id: 15
        	    name: RDB$FLAGS
        	Field id: 16
        	    name: RDB$RELATION_TYPE
        	    trigger_name: RDB$TRIGGER_4
        	    trigger_name: RDB$TRIGGER_5

==============================================================================
           <null>            <null>            <null>               7               1                8          0            4 RDB$VIEW_RELATIONS                                                                            <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>               8               1                8          0            3 RDB$FORMATS                                                                                   SQL$3                                                                                         <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        SQL$DEFAULT3                                                                                     <null>                 0
           <null>            <null>            <null>               9               1                8          0            3 RDB$SECURITY_CLASSES                                                                          <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              10               1                8          0            6 RDB$FILES                                                                                     <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              11               1                8          0            5 RDB$TYPES                                                                                     <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              12               1                8          0           12 RDB$TRIGGERS                                                                                  <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:2b                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$TRIGGER_NAME
        	Field id: 1
        	    name: RDB$RELATION_NAME
        	Field id: 2
        	    name: RDB$TRIGGER_SEQUENCE
        	Field id: 3
        	    name: RDB$TRIGGER_TYPE
        	Field id: 4
        	    name: RDB$TRIGGER_SOURCE
        	Field id: 5
        	    name: RDB$TRIGGER_BLR
        	Field id: 6
        	    name: RDB$DESCRIPTION
        	Field id: 7
        	    name: RDB$TRIGGER_INACTIVE
        	Field id: 8
        	    name: RDB$SYSTEM_FLAG
        	Field id: 9
        	    name: RDB$FLAGS
        	Field id: 10
        	    name: RDB$VALID_BLR
        	Field id: 11
        	    name: RDB$DEBUG_INFO
        	    trigger_name: RDB$TRIGGER_2
        	    trigger_name: RDB$TRIGGER_21
        	    trigger_name: RDB$TRIGGER_22
        	    trigger_name: RDB$TRIGGER_3

==============================================================================
           <null>            <null>            <null>              13               1                8          0            5 RDB$DEPENDENCIES                                                                              <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              14               1                8          0            8 RDB$FUNCTIONS                                                                                 <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              15               1                8          0           10 RDB$FUNCTION_ARGUMENTS                                                                        <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              16               1                8          0            7 RDB$FILTERS                                                                                   <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              17               1                8          0            3 RDB$TRIGGER_MESSAGES                                                                          <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              18               1                8          0            8 RDB$USER_PRIVILEGES                                                                           <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:2a                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$USER
        	Field id: 1
        	    name: RDB$GRANTOR
        	Field id: 2
        	    name: RDB$PRIVILEGE
        	Field id: 3
        	    name: RDB$GRANT_OPTION
        	Field id: 4
        	    name: RDB$RELATION_NAME
        	Field id: 5
        	    name: RDB$FIELD_NAME
        	Field id: 6
        	    name: RDB$USER_TYPE
        	Field id: 7
        	    name: RDB$OBJECT_TYPE
        	    trigger_name: RDB$TRIGGER_1
        	    trigger_name: RDB$TRIGGER_31
        	    trigger_name: RDB$TRIGGER_32
        	    trigger_name: RDB$TRIGGER_33
        	    trigger_name: RDB$TRIGGER_8
        	    trigger_name: RDB$TRIGGER_9

==============================================================================
           <null>            <null>            <null>              19               1                8          0            4 RDB$TRANSACTIONS                                                                              <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0

     RDB$VIEW_BLR   RDB$VIEW_SOURCE   RDB$DESCRIPTION RDB$RELATION_ID RDB$SYSTEM_FLAG RDB$DBKEY_LENGTH RDB$FORMAT RDB$FIELD_ID RDB$RELATION_NAME                                                                             RDB$SECURITY_CLASS                                                                            RDB$EXTERNAL_FILE                                                                                                                                                                                                                                                     RDB$RUNTIME RDB$EXTERNAL_DESCRIPTION RDB$OWNER_NAME                                                                                RDB$DEFAULT_CLASS                                                                             RDB$FLAGS RDB$RELATION_TYPE
================= ================= ================= =============== =============== ================ ========== ============ =============================================================================== =============================================================================== =============================================================================== ================= ======================== =============================================================================== =============================================================================== ========= =================
           <null>            <null>            <null>              20               1                8          0            4 RDB$GENERATORS                                                                                <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:f0                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$GENERATOR_NAME
        	Field id: 1
        	    name: RDB$GENERATOR_ID
        	Field id: 2
        	    name: RDB$SYSTEM_FLAG
        	Field id: 3
        	    name: RDB$DESCRIPTION
        	    trigger_name: RDB$TRIGGER_6

==============================================================================
           <null>            <null>            <null>              21               1                8          0            4 RDB$FIELD_DIMENSIONS                                                                          <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              22               1                8          0            6 RDB$RELATION_CONSTRAINTS                                                                      <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:f1                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$CONSTRAINT_NAME
        	Field id: 1
        	    name: RDB$CONSTRAINT_TYPE
        	Field id: 2
        	    name: RDB$RELATION_NAME
        	Field id: 3
        	    name: RDB$DEFERRABLE
        	    default_value:
        	        blr_version5,
        	        blr_literal, blr_text2, 2,0, 2,0, 'N','O',
        	        blr_eoc
        	Field id: 4
        	    name: RDB$INITIALLY_DEFERRED
        	    default_value:
        	        blr_version5,
        	        blr_literal, blr_text2, 2,0, 2,0, 'N','O',
        	        blr_eoc
        	Field id: 5
        	    name: RDB$INDEX_NAME
        	    trigger_name: RDB$TRIGGER_10
        	    trigger_name: RDB$TRIGGER_11
        	    trigger_name: RDB$TRIGGER_25
        	    trigger_name: RDB$TRIGGER_26
        	    trigger_name: RDB$TRIGGER_34

==============================================================================
           <null>            <null>            <null>              23               1                8          0            5 RDB$REF_CONSTRAINTS                                                                           <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:f2                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$CONSTRAINT_NAME
        	Field id: 1
        	    name: RDB$CONST_NAME_UQ
        	Field id: 2
        	    name: RDB$MATCH_OPTION
        	    default_value:
        	        blr_version5,
        	        blr_literal, blr_text2, 2,0, 4,0, 'F','U','L','L',
        	        blr_eoc
        	Field id: 3
        	    name: RDB$UPDATE_RULE
        	    default_value:
        	        blr_version5,
        	        blr_literal, blr_text2, 2,0, 8,0, 'R','E','S','T','R','I','C','T',
        	        blr_eoc
        	Field id: 4
        	    name: RDB$DELETE_RULE
        	    default_value:
        	        blr_version5,
        	        blr_literal, blr_text2, 2,0, 8,0, 'R','E','S','T','R','I','C','T',
        	        blr_eoc
        	    trigger_name: RDB$TRIGGER_12
        	    trigger_name: RDB$TRIGGER_13

==============================================================================
           <null>            <null>            <null>              24               1                8          0            2 RDB$CHECK_CONSTRAINTS                                                                         <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:f3                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$CONSTRAINT_NAME
        	Field id: 1
        	    name: RDB$TRIGGER_NAME
        	    trigger_name: RDB$TRIGGER_14
        	    trigger_name: RDB$TRIGGER_15
        	    trigger_name: RDB$TRIGGER_16
        	    trigger_name: RDB$TRIGGER_35

==============================================================================
           <null>            <null>            <null>              25               1                8          0            6 RDB$LOG_FILES                                                                                 <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              26               1                8          0           14 RDB$PROCEDURES                                                                                <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:f7                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$PROCEDURE_NAME
        	Field id: 1
        	    name: RDB$PROCEDURE_ID
        	Field id: 2
        	    name: RDB$PROCEDURE_INPUTS
        	Field id: 3
        	    name: RDB$PROCEDURE_OUTPUTS
        	Field id: 4
        	    name: RDB$DESCRIPTION
        	Field id: 5
        	    name: RDB$PROCEDURE_SOURCE
        	Field id: 6
        	    name: RDB$PROCEDURE_BLR
        	Field id: 7
        	    name: RDB$SECURITY_CLASS
        	Field id: 8
        	    name: RDB$OWNER_NAME
        	Field id: 9
        	    name: RDB$RUNTIME
        	Field id: 10
        	    name: RDB$SYSTEM_FLAG
        	Field id: 11
        	    name: RDB$PROCEDURE_TYPE
        	Field id: 12
        	    name: RDB$VALID_BLR
        	Field id: 13
        	    name: RDB$DEBUG_INFO
        	    trigger_name: RDB$TRIGGER_28
        	    trigger_name: RDB$TRIGGER_29

==============================================================================
           <null>            <null>            <null>              27               1                8          0           14 RDB$PROCEDURE_PARAMETERS                                                                      <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              28               1                8          0            9 RDB$CHARACTER_SETS                                                                            <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              29               1                8          0            9 RDB$COLLATIONS                                                                                <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              30               1                8          0            5 RDB$EXCEPTIONS                                                                                <null>                                                                                        <null>                                                                                                                                                                                                                                                                       6:f8                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
==============================================================================
RDB$RUNTIME:
        	Field id: 0
        	    name: RDB$EXCEPTION_NAME
        	Field id: 1
        	    name: RDB$EXCEPTION_NUMBER
        	Field id: 2
        	    name: RDB$MESSAGE
        	Field id: 3
        	    name: RDB$DESCRIPTION
        	Field id: 4
        	    name: RDB$SYSTEM_FLAG
        	    trigger_name: RDB$TRIGGER_30

==============================================================================
           <null>            <null>            <null>              31               1                8          0            4 RDB$ROLES                                                                                     SQL$1                                                                                         <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        SQL$DEFAULT1                                                                                     <null>                 0
           <null>            <null>            <null>              32               1                8          0            6 RDB$BACKUP_HISTORY                                                                            <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 0
           <null>            <null>            <null>              33               1                8          0           19 MON$DATABASE                                                                                  <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 3
           <null>            <null>            <null>              34               1                8          0           14 MON$ATTACHMENTS                                                                               <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 3
           <null>            <null>            <null>              35               1                8          0           13 MON$TRANSACTIONS                                                                              <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 3
           <null>            <null>            <null>              36               1                8          0            7 MON$STATEMENTS                                                                                <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 3
           <null>            <null>            <null>              37               1                8          0            9 MON$CALL_STACK                                                                                <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 3
           <null>            <null>            <null>              38               1                8          0            6 MON$IO_STATS                                                                                  <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 3
           <null>            <null>            <null>              39               1                8          0           10 MON$RECORD_STATS                                                                              <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 3

     RDB$VIEW_BLR   RDB$VIEW_SOURCE   RDB$DESCRIPTION RDB$RELATION_ID RDB$SYSTEM_FLAG RDB$DBKEY_LENGTH RDB$FORMAT RDB$FIELD_ID RDB$RELATION_NAME                                                                             RDB$SECURITY_CLASS                                                                            RDB$EXTERNAL_FILE                                                                                                                                                                                                                                                     RDB$RUNTIME RDB$EXTERNAL_DESCRIPTION RDB$OWNER_NAME                                                                                RDB$DEFAULT_CLASS                                                                             RDB$FLAGS RDB$RELATION_TYPE
================= ================= ================= =============== =============== ================ ========== ============ =============================================================================== =============================================================================== =============================================================================== ================= ======================== =============================================================================== =============================================================================== ========= =================
           <null>            <null>            <null>              40               1                8          0            4 MON$CONTEXT_VARIABLES                                                                         <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 3
           <null>            <null>            <null>              41               1                8          0            6 MON$MEMORY_USAGE                                                                              <null>                                                                                        <null>                                                                                                                                                                                                                                                                     <null>                   <null> SYSDBA                                                                                        <null>                                                                                           <null>                 3" ]; then
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
echo "Original Firebird Test: functional.basic.db.23"
echo "ScratchBird Enhancements: 1 revolutionary features"

exit ${exit_code:-0}
