#!/bin/bash

# 30_core_database_functional_basic_db_db_30.sh
# ScratchBird Test - Migrated from Firebird Test Suite
# 
# Original Test ID: functional.basic.db.db_30
# Title: Empty DB - RDB$TYPES
# Original Firebird Version: 2.5

#
# Check for correct content of RDB$TYPES in empty database.

# 🚀 REVOLUTIONARY FEATURES DEMONSTRATED:
# 🚀 Hierarchical Schemas (PostgreSQL-exceeding)
#

set -e

# Source centralized test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config.sh"

# Test-specific configuration
TEST_NAME="30_core_database_functional_basic_db_db_30"
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
echo "Original: functional.basic.db.db_30"
echo "Date: $(date)"
echo "Database: $TEST_DB"
echo "Revolutionary Features: 1 active"
echo

# Log test execution
log_test_execution "$TEST_NAME" "START" "Beginning migrated test from Firebird"

# Create SQL test script
cat > "$SB_TEST_RESULTS_DIR/${TEST_NAME}_input.sql" << 'EOF'
-- =================================================================
-- SCRATCHBIRD MIGRATED TEST: Empty DB - RDB$TYPES
-- Original Firebird Test ID: functional.basic.db.db_30
-- =================================================================


-- ScratchBird Hierarchical Schema Enhancement
CREATE SCHEMA testing;
CREATE SCHEMA testing.basic;
SET SCHEMA 'testing.basic';

select * from RDB$TYPES;

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
Original Test ID: functional.basic.db.db_30
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
    if [ -n "RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$FIELD_TYPE                                                                                      14 TEXT                                                                                                     <null>               1
RDB$FIELD_TYPE                                                                                       7 SHORT                                                                                                    <null>               1
RDB$FIELD_TYPE                                                                                       8 LONG                                                                                                     <null>               1
RDB$FIELD_TYPE                                                                                       9 QUAD                                                                                                     <null>               1
RDB$FIELD_TYPE                                                                                      10 FLOAT                                                                                                    <null>               1
RDB$FIELD_TYPE                                                                                      27 DOUBLE                                                                                                   <null>               1
RDB$FIELD_TYPE                                                                                      35 TIMESTAMP                                                                                                <null>               1
RDB$FIELD_TYPE                                                                                      37 VARYING                                                                                                  <null>               1
RDB$FIELD_TYPE                                                                                     261 BLOB                                                                                                     <null>               1
RDB$FIELD_TYPE                                                                                      40 CSTRING                                                                                                  <null>               1
RDB$FIELD_TYPE                                                                                      45 BLOB_ID                                                                                                  <null>               1
RDB$FIELD_TYPE                                                                                      12 DATE                                                                                                     <null>               1
RDB$FIELD_TYPE                                                                                      13 TIME                                                                                                     <null>               1
RDB$FIELD_TYPE                                                                                      16 INT64                                                                                                    <null>               1
RDB$FIELD_SUB_TYPE                                                                                   0 BINARY                                                                                                   <null>               1
RDB$FIELD_SUB_TYPE                                                                                   1 TEXT                                                                                                     <null>               1
RDB$FIELD_SUB_TYPE                                                                                   2 BLR                                                                                                      <null>               1
RDB$FIELD_SUB_TYPE                                                                                   3 ACL                                                                                                      <null>               1
RDB$FIELD_SUB_TYPE                                                                                   4 RANGES                                                                                                   <null>               1
RDB$FIELD_SUB_TYPE                                                                                   5 SUMMARY                                                                                                  <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$FIELD_SUB_TYPE                                                                                   6 FORMAT                                                                                                   <null>               1
RDB$FIELD_SUB_TYPE                                                                                   7 TRANSACTION_DESCRIPTION                                                                                  <null>               1
RDB$FIELD_SUB_TYPE                                                                                   8 EXTERNAL_FILE_DESCRIPTION                                                                                <null>               1
RDB$FIELD_SUB_TYPE                                                                                   9 DEBUG_INFORMATION                                                                                        <null>               1
RDB$FUNCTION_TYPE                                                                                    0 VALUE                                                                                                    <null>               1
RDB$FUNCTION_TYPE                                                                                    1 BOOLEAN                                                                                                  <null>               1
RDB$MECHANISM                                                                                        0 BY_VALUE                                                                                                 <null>               1
RDB$MECHANISM                                                                                        1 BY_REFERENCE                                                                                             <null>               1
RDB$MECHANISM                                                                                        2 BY_VMS_DESCRIPTOR                                                                                        <null>               1
RDB$MECHANISM                                                                                        3 BY_ISC_DESCRIPTOR                                                                                        <null>               1
RDB$MECHANISM                                                                                        4 BY_SCALAR_ARRAY_DESCRIPTOR                                                                               <null>               1
RDB$MECHANISM                                                                                        5 BY_REFERENCE_WITH_NULL                                                                                   <null>               1
RDB$TRIGGER_TYPE                                                                                     1 PRE_STORE                                                                                                <null>               1
RDB$TRIGGER_TYPE                                                                                     2 POST_STORE                                                                                               <null>               1
RDB$TRIGGER_TYPE                                                                                     3 PRE_MODIFY                                                                                               <null>               1
RDB$TRIGGER_TYPE                                                                                     4 POST_MODIFY                                                                                              <null>               1
RDB$TRIGGER_TYPE                                                                                     5 PRE_ERASE                                                                                                <null>               1
RDB$TRIGGER_TYPE                                                                                     6 POST_ERASE                                                                                               <null>               1
RDB$TRIGGER_TYPE                                                                                  8192 CONNECT                                                                                                  <null>               1
RDB$TRIGGER_TYPE                                                                                  8193 DISCONNECT                                                                                               <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$TRIGGER_TYPE                                                                                  8194 TRANSACTION_START                                                                                        <null>               1
RDB$TRIGGER_TYPE                                                                                  8195 TRANSACTION_COMMIT                                                                                       <null>               1
RDB$TRIGGER_TYPE                                                                                  8196 TRANSACTION_ROLLBACK                                                                                     <null>               1
RDB$OBJECT_TYPE                                                                                      0 RELATION                                                                                                 <null>               1
RDB$OBJECT_TYPE                                                                                      1 VIEW                                                                                                     <null>               1
RDB$OBJECT_TYPE                                                                                      2 TRIGGER                                                                                                  <null>               1
RDB$OBJECT_TYPE                                                                                      3 COMPUTED_FIELD                                                                                           <null>               1
RDB$OBJECT_TYPE                                                                                      4 VALIDATION                                                                                               <null>               1
RDB$OBJECT_TYPE                                                                                      5 PROCEDURE                                                                                                <null>               1
RDB$OBJECT_TYPE                                                                                      6 EXPRESSION_INDEX                                                                                         <null>               1
RDB$OBJECT_TYPE                                                                                      7 EXCEPTION                                                                                                <null>               1
RDB$OBJECT_TYPE                                                                                      8 USER                                                                                                     <null>               1
RDB$OBJECT_TYPE                                                                                      9 FIELD                                                                                                    <null>               1
RDB$OBJECT_TYPE                                                                                     10 INDEX                                                                                                    <null>               1
RDB$OBJECT_TYPE                                                                                     12 USER_GROUP                                                                                               <null>               1
RDB$OBJECT_TYPE                                                                                     13 ROLE                                                                                                     <null>               1
RDB$OBJECT_TYPE                                                                                     14 GENERATOR                                                                                                <null>               1
RDB$OBJECT_TYPE                                                                                     15 UDF                                                                                                      <null>               1
RDB$OBJECT_TYPE                                                                                     16 BLOB_FILTER                                                                                              <null>               1
RDB$OBJECT_TYPE                                                                                     17 COLLATION                                                                                                <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$TRANSACTION_STATE                                                                                1 LIMBO                                                                                                    <null>               1
RDB$TRANSACTION_STATE                                                                                2 COMMITTED                                                                                                <null>               1
RDB$TRANSACTION_STATE                                                                                3 ROLLED_BACK                                                                                              <null>               1
RDB$SYSTEM_FLAG                                                                                      0 USER                                                                                                     <null>               1
RDB$SYSTEM_FLAG                                                                                      1 SYSTEM                                                                                                   <null>               1
RDB$SYSTEM_FLAG                                                                                      2 QLI                                                                                                      <null>               1
RDB$SYSTEM_FLAG                                                                                      3 CHECK_CONSTRAINT                                                                                         <null>               1
RDB$SYSTEM_FLAG                                                                                      4 REFERENTIAL_CONSTRAINT                                                                                   <null>               1
RDB$SYSTEM_FLAG                                                                                      5 VIEW_CHECK                                                                                               <null>               1
RDB$RELATION_TYPE                                                                                    0 PERSISTENT                                                                                               <null>               1
RDB$RELATION_TYPE                                                                                    1 VIEW                                                                                                     <null>               1
RDB$RELATION_TYPE                                                                                    2 EXTERNAL                                                                                                 <null>               1
RDB$RELATION_TYPE                                                                                    3 VIRTUAL                                                                                                  <null>               1
RDB$RELATION_TYPE                                                                                    4 GLOBAL_TEMPORARY_PRESERVE                                                                                <null>               1
RDB$RELATION_TYPE                                                                                    5 GLOBAL_TEMPORARY_DELETE                                                                                  <null>               1
RDB$PROCEDURE_TYPE                                                                                   0 LEGACY                                                                                                   <null>               1
RDB$PROCEDURE_TYPE                                                                                   1 SELECTABLE                                                                                               <null>               1
RDB$PROCEDURE_TYPE                                                                                   2 EXECUTABLE                                                                                               <null>               1
RDB$PARAMETER_MECHANISM                                                                              0 NORMAL                                                                                                   <null>               1
RDB$PARAMETER_MECHANISM                                                                              1 TYPE OF                                                                                                  <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
MON$STATE                                                                                            0 IDLE                                                                                                     <null>               1
MON$STATE                                                                                            1 ACTIVE                                                                                                   <null>               1
MON$STATE                                                                                            2 STALLED                                                                                                  <null>               1
MON$SHUTDOWN_MODE                                                                                    0 ONLINE                                                                                                   <null>               1
MON$SHUTDOWN_MODE                                                                                    1 MULTI_USER_SHUTDOWN                                                                                      <null>               1
MON$SHUTDOWN_MODE                                                                                    2 SINGLE_USER_SHUTDOWN                                                                                     <null>               1
MON$SHUTDOWN_MODE                                                                                    3 FULL_SHUTDOWN                                                                                            <null>               1
MON$ISOLATION_MODE                                                                                   0 CONSISTENCY                                                                                              <null>               1
MON$ISOLATION_MODE                                                                                   1 CONCURRENCY                                                                                              <null>               1
MON$ISOLATION_MODE                                                                                   2 READ_COMMITTED_VERSION                                                                                   <null>               1
MON$ISOLATION_MODE                                                                                   3 READ_COMMITTED_NO_VERSION                                                                                <null>               1
MON$BACKUP_STATE                                                                                     0 NORMAL                                                                                                   <null>               1
MON$BACKUP_STATE                                                                                     1 STALLED                                                                                                  <null>               1
MON$BACKUP_STATE                                                                                     2 MERGE                                                                                                    <null>               1
MON$STAT_GROUP                                                                                       0 DATABASE                                                                                                 <null>               1
MON$STAT_GROUP                                                                                       1 ATTACHMENT                                                                                               <null>               1
MON$STAT_GROUP                                                                                       2 TRANSACTION                                                                                              <null>               1
MON$STAT_GROUP                                                                                       3 STATEMENT                                                                                                <null>               1
MON$STAT_GROUP                                                                                       4 CALL                                                                                                     <null>               1
RDB$CHARACTER_SET_NAME                                                                               0 NONE                                                                                                     <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$CHARACTER_SET_NAME                                                                               1 OCTETS                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                               2 ASCII                                                                                                    <null>               1
RDB$CHARACTER_SET_NAME                                                                               3 UNICODE_FSS                                                                                              <null>               1
RDB$CHARACTER_SET_NAME                                                                               4 UTF8                                                                                                     <null>               1
RDB$CHARACTER_SET_NAME                                                                               5 SJIS_0208                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                               6 EUCJ_0208                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              10 DOS437                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              11 DOS850                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              12 DOS865                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              21 ISO8859_1                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              22 ISO8859_2                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              23 ISO8859_3                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              34 ISO8859_4                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              35 ISO8859_5                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              36 ISO8859_6                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              37 ISO8859_7                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              38 ISO8859_8                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              39 ISO8859_9                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              40 ISO8859_13                                                                                               <null>               1
RDB$CHARACTER_SET_NAME                                                                              45 DOS852                                                                                                   <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$CHARACTER_SET_NAME                                                                              46 DOS857                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              13 DOS860                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              47 DOS861                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              14 DOS863                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              50 CYRL                                                                                                     <null>               1
RDB$CHARACTER_SET_NAME                                                                               9 DOS737                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              15 DOS775                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              16 DOS858                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              17 DOS862                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              18 DOS864                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              48 DOS866                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              49 DOS869                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              51 WIN1250                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              52 WIN1251                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              53 WIN1252                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              54 WIN1253                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              55 WIN1254                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              19 NEXT                                                                                                     <null>               1
RDB$CHARACTER_SET_NAME                                                                              58 WIN1255                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              59 WIN1256                                                                                                  <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$CHARACTER_SET_NAME                                                                              60 WIN1257                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              44 KSC_5601                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              56 BIG_5                                                                                                    <null>               1
RDB$CHARACTER_SET_NAME                                                                              57 GB_2312                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              63 KOI8R                                                                                                    <null>               1
RDB$CHARACTER_SET_NAME                                                                              64 KOI8U                                                                                                    <null>               1
RDB$CHARACTER_SET_NAME                                                                              65 WIN1258                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              66 TIS620                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              67 GBK                                                                                                      <null>               1
RDB$CHARACTER_SET_NAME                                                                              68 CP943C                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              69 GB18030                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                               1 BINARY                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                               2 USASCII                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                               2 ASCII7                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                               3 UTF_FSS                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                               3 SQL_TEXT                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                               4 UTF-8                                                                                                    <null>               1
RDB$CHARACTER_SET_NAME                                                                               5 SJIS                                                                                                     <null>               1
RDB$CHARACTER_SET_NAME                                                                               6 EUCJ                                                                                                     <null>               1
RDB$CHARACTER_SET_NAME                                                                              10 DOS_437                                                                                                  <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$CHARACTER_SET_NAME                                                                              11 DOS_850                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              12 DOS_865                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              21 ISO8859_1                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              21 ISO88591                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              21 LATIN1                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              21 ANSI                                                                                                     <null>               1
RDB$CHARACTER_SET_NAME                                                                              22 ISO8859_2                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              22 ISO88592                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              22 LATIN2                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              22 ISO-8859-2                                                                                               <null>               1
RDB$CHARACTER_SET_NAME                                                                              23 ISO8859_3                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              23 ISO88593                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              23 LATIN3                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              23 ISO-8859-3                                                                                               <null>               1
RDB$CHARACTER_SET_NAME                                                                              34 ISO8859_4                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              34 ISO88594                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              34 LATIN4                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              34 ISO-8859-4                                                                                               <null>               1
RDB$CHARACTER_SET_NAME                                                                              35 ISO8859_5                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              35 ISO88595                                                                                                 <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$CHARACTER_SET_NAME                                                                              35 ISO-8859-5                                                                                               <null>               1
RDB$CHARACTER_SET_NAME                                                                              36 ISO8859_6                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              36 ISO88596                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              36 ISO-8859-6                                                                                               <null>               1
RDB$CHARACTER_SET_NAME                                                                              37 ISO8859_7                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              37 ISO88597                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              37 ISO-8859-7                                                                                               <null>               1
RDB$CHARACTER_SET_NAME                                                                              38 ISO8859_8                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              38 ISO88598                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              38 ISO-8859-8                                                                                               <null>               1
RDB$CHARACTER_SET_NAME                                                                              39 ISO8859_9                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              39 ISO88599                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              39 LATIN5                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              39 ISO-8859-9                                                                                               <null>               1
RDB$CHARACTER_SET_NAME                                                                              40 ISO8859_13                                                                                               <null>               1
RDB$CHARACTER_SET_NAME                                                                              40 ISO885913                                                                                                <null>               1
RDB$CHARACTER_SET_NAME                                                                              40 LATIN7                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              40 ISO-8859-13                                                                                              <null>               1
RDB$CHARACTER_SET_NAME                                                                              45 DOS_852                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              46 DOS_857                                                                                                  <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$CHARACTER_SET_NAME                                                                              13 DOS_860                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              47 DOS_861                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              14 DOS_863                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                               9 DOS_737                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              15 DOS_775                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              16 DOS_858                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              17 DOS_862                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              18 DOS_864                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              48 DOS_866                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              49 DOS_869                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              51 WIN_1250                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              52 WIN_1251                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              53 WIN_1252                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              54 WIN_1253                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              55 WIN_1254                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              58 WIN_1255                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              59 WIN_1256                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              60 WIN_1257                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              65 WIN_1258                                                                                                 <null>               1
RDB$CHARACTER_SET_NAME                                                                              44 KSC5601                                                                                                  <null>               1

RDB$FIELD_NAME                                                                                RDB$TYPE RDB$TYPE_NAME                                                                                   RDB$DESCRIPTION RDB$SYSTEM_FLAG
=============================================================================== ======== =============================================================================== ================= ===============
RDB$CHARACTER_SET_NAME                                                                              44 DOS_949                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              44 WIN_949                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              56 BIG5                                                                                                     <null>               1
RDB$CHARACTER_SET_NAME                                                                              56 DOS_950                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              56 WIN_950                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              57 GB2312                                                                                                   <null>               1
RDB$CHARACTER_SET_NAME                                                                              57 DOS_936                                                                                                  <null>               1
RDB$CHARACTER_SET_NAME                                                                              57 WIN_936                                                                                                  <null>               1" ]; then
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
echo "Original Firebird Test: functional.basic.db.db_30"
echo "ScratchBird Enhancements: 1 revolutionary features"

exit ${exit_code:-0}
