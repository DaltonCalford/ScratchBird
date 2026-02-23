#coding:utf-8

"""
ID:          new-database-23
TITLE:       New DB - RDB$RELATIONS content
DESCRIPTION: Check the correct content of RDB$RELATIONS in new database.
NOTES:
[28.10.2015]
    1. Removed from output BLOB IDs for fields rdb$security_class and rdb$default_class - they changes very often.
    2. Added blocks to 'substitution' section to:
    2.1  Suppress possible differences when check IDs of all BLOB fields.
    2.2. Ignore values of IDs in lines like "trigger_name: RDB$TRIGGER_**".
    3. Added query to select FIELDS list of table because main check does not use asterisk
    and we have to know if DDL of table will have any changes in future.
[17.01.2023] pzotov
    DISABLED after discussion with dimitr, letters 17-sep-2022 11:23.
    Reasons:
        * There is no much sense to keep such tests because they fails extremely often during new major FB developing.
        * There is no chanse to get successful outcome for the whole test suite is some of system table became invalid,
          i.e. lot of other tests will be failed in such case.
    Single test for check DDL (type of columns, their order and total number) will be implemented for all RDB-tables.
FBTEST:      functional.basic.db.23
"""

import pytest
from firebird.qa import *

substitutions = [
                     ('BLOB_ID_VIEW_BLR.*',  'BLOB_ID_VIEW_BLR')
                    ,('BLOB_ID_VIEW_SRC.*',  'BLOB_ID_VIEW_SRC')
                    ,('BLOB_ID_DESCR.*',     'BLOB_ID_DESCR')
                    ,('BLOB_ID_RUNTIME.*',   'BLOB_ID_RUNTIME')
                    ,('BLOB_ID_EXT_DESCR.*', 'BLOB_ID_EXT_DESCR')
                    ,('trigger_name:.*',     'trigger_name:')
                ]


db = db_factory()

test_script = """
    -- 28.10.2015.
    -- 1. Removed from output BLOB IDs for fields rdb$security_class and rdb$default_class - they changes very often.
    -- 2. Added blocks to 'substitution' section to:
    -- 2.1  Suppress possible differences when check IDs of all BLOB fields.
    -- 2.2. Ignore values of IDs in lines like "trigger_name: RDB$TRIGGER_**".
    -- 3. Added query to select FIELDS list of table because main check does not use asterisk
    -- and we have to know if DDL of table will have any changes in future.

    set list on;
    set blob all;
    set count on;

    -- Query for check whether fields list of table was changed:
    -- ::: NB:::
    -- RDB$RELATION_FIELDS has following constraint: UNIQUE (RDB$FIELD_NAME, RDB$RELATION_NAME);
    select trim(rf.rdb$field_name) as rdb$field_name
    from rdb$relation_fields rf
    where rf.rdb$relation_name = upper('rdb$relations')
    order by rf.rdb$field_name, rdb$relation_name;

    -- Main test query.
    -- NB: rdb$relation_name is unique column, see DDL:
    -- RDB$RELATIONS has following constraint: UNIQUE (RDB$RELATION_NAME);
    select
        rdb$relation_id
        ,trim(rdb$relation_name)  as rdb$relation_name
        ,rdb$system_flag
        ,rdb$dbkey_length
        ,rdb$format
        ,rdb$field_id
        ,rdb$flags
        ,rdb$relation_type
        ,trim(rdb$owner_name)     as rdb$owner_name
        ,rdb$external_file
        -- Removed from output BLOB IDs for fields rdb$security_class and rdb$default_class
        -- because they changes very often:
        -- ,trim(rdb$security_class) as rdb$security_class
        -- ,trim(rdb$default_class)  as rdb$default_class
        ,rdb$view_blr             as blob_id_view_blr
        ,rdb$view_source          as blob_id_view_src
        ,rdb$description          as blob_id_descr
        ,rdb$runtime              as blob_id_runtime
        ,rdb$external_description as blob_id_ext_descr
    from rdb$relations
    order by rdb$relation_name;
"""

act = isql_act('db', test_script, substitutions=substitutions)

# version: 3.0

expected_stdout_1 = """
RDB$FIELD_NAME                  RDB$DBKEY_LENGTH
RDB$FIELD_NAME                  RDB$DEFAULT_CLASS
RDB$FIELD_NAME                  RDB$DESCRIPTION
RDB$FIELD_NAME                  RDB$EXTERNAL_DESCRIPTION
RDB$FIELD_NAME                  RDB$EXTERNAL_FILE
RDB$FIELD_NAME                  RDB$FIELD_ID
RDB$FIELD_NAME                  RDB$FLAGS
RDB$FIELD_NAME                  RDB$FORMAT
RDB$FIELD_NAME                  RDB$OWNER_NAME
RDB$FIELD_NAME                  RDB$RELATION_ID
RDB$FIELD_NAME                  RDB$RELATION_NAME
RDB$FIELD_NAME                  RDB$RELATION_TYPE
RDB$FIELD_NAME                  RDB$RUNTIME
RDB$FIELD_NAME                  RDB$SECURITY_CLASS
RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
RDB$FIELD_NAME                  RDB$VIEW_BLR
RDB$FIELD_NAME                  RDB$VIEW_SOURCE
Records affected: 17
RDB$RELATION_ID                 34
RDB$RELATION_NAME               MON$ATTACHMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    20
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 37
RDB$RELATION_NAME               MON$CALL_STACK
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    10
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 40
RDB$RELATION_NAME               MON$CONTEXT_VARIABLES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 33
RDB$RELATION_NAME               MON$DATABASE
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    22
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 38
RDB$RELATION_NAME               MON$IO_STATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 41
RDB$RELATION_NAME               MON$MEMORY_USAGE
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 39
RDB$RELATION_NAME               MON$RECORD_STATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    16
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 36
RDB$RELATION_NAME               MON$STATEMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 49
RDB$RELATION_NAME               MON$TABLE_STATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 35
RDB$RELATION_NAME               MON$TRANSACTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    13
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 45
RDB$RELATION_NAME               RDB$AUTH_MAPPING
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    10
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 32
RDB$RELATION_NAME               RDB$BACKUP_HISTORY
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 28
RDB$RELATION_NAME               RDB$CHARACTER_SETS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    11
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 24
RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
Field id: 0
name: RDB$CONSTRAINT_NAME
Field id: 1
name: RDB$TRIGGER_NAME
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 29
RDB$RELATION_NAME               RDB$COLLATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    11
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 1
RDB$RELATION_NAME               RDB$DATABASE
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    5
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 47
RDB$RELATION_NAME               RDB$DB_CREATORS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 13
RDB$RELATION_NAME               RDB$DEPENDENCIES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 30
RDB$RELATION_NAME               RDB$EXCEPTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    7
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 2
RDB$RELATION_NAME               RDB$FIELDS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    30
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
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
Field id: 28
name: RDB$SECURITY_CLASS
Field id: 29
name: RDB$OWNER_NAME
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 21
RDB$RELATION_NAME               RDB$FIELD_DIMENSIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 10
RDB$RELATION_NAME               RDB$FILES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 16
RDB$RELATION_NAME               RDB$FILTERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    9
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 8
RDB$RELATION_NAME               RDB$FORMATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    3
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 14
RDB$RELATION_NAME               RDB$FUNCTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    20
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 15
RDB$RELATION_NAME               RDB$FUNCTION_ARGUMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    22
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 20
RDB$RELATION_NAME               RDB$GENERATORS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 3
RDB$RELATION_NAME               RDB$INDEX_SEGMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
Field id: 0
name: RDB$INDEX_NAME
Field id: 1
name: RDB$FIELD_NAME
Field id: 2
name: RDB$FIELD_POSITION
Field id: 3
name: RDB$STATISTICS
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 4
RDB$RELATION_NAME               RDB$INDICES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    13
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
Field id: 10
name: RDB$EXPRESSION_BLR
Field id: 11
name: RDB$EXPRESSION_SOURCE
Field id: 12
name: RDB$STATISTICS
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 25
RDB$RELATION_NAME               RDB$LOG_FILES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 42
RDB$RELATION_NAME               RDB$PACKAGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 0
RDB$RELATION_NAME               RDB$PAGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 26
RDB$RELATION_NAME               RDB$PROCEDURES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    18
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 27
RDB$RELATION_NAME               RDB$PROCEDURE_PARAMETERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    15
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 23
RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    5
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 6
RDB$RELATION_NAME               RDB$RELATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    17
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 22
RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
trigger_name:
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 5
RDB$RELATION_NAME               RDB$RELATION_FIELDS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    21
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
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
Field id: 19
name: RDB$GENERATOR_NAME
Field id: 20
name: RDB$IDENTITY_TYPE
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 31
RDB$RELATION_NAME               RDB$ROLES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    5
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 9
RDB$RELATION_NAME               RDB$SECURITY_CLASSES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    3
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 19
RDB$RELATION_NAME               RDB$TRANSACTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 12
RDB$RELATION_NAME               RDB$TRIGGERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    14
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
Field id: 9
name: RDB$FLAGS
Field id: 10
name: RDB$VALID_BLR
Field id: 11
name: RDB$DEBUG_INFO
Field id: 12
name: RDB$ENGINE_NAME
Field id: 13
name: RDB$ENTRYPOINT
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 17
RDB$RELATION_NAME               RDB$TRIGGER_MESSAGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    3
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 11
RDB$RELATION_NAME               RDB$TYPES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    5
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 18
RDB$RELATION_NAME               RDB$USER_PRIVILEGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
trigger_name:
trigger_name:
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 7
RDB$RELATION_NAME               RDB$VIEW_RELATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 48
RDB$RELATION_NAME               SEC$DB_CREATORS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 46
RDB$RELATION_NAME               SEC$GLOBAL_AUTH_MAPPING
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 43
RDB$RELATION_NAME               SEC$USERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 44
RDB$RELATION_NAME               SEC$USER_ATTRIBUTES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
Records affected: 50
"""

@pytest.mark.version('>=3.0,<4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 4.0

expected_stdout_2 = """
RDB$FIELD_NAME                  RDB$DBKEY_LENGTH
RDB$FIELD_NAME                  RDB$DEFAULT_CLASS
RDB$FIELD_NAME                  RDB$DESCRIPTION
RDB$FIELD_NAME                  RDB$EXTERNAL_DESCRIPTION
RDB$FIELD_NAME                  RDB$EXTERNAL_FILE
RDB$FIELD_NAME                  RDB$FIELD_ID
RDB$FIELD_NAME                  RDB$FLAGS
RDB$FIELD_NAME                  RDB$FORMAT
RDB$FIELD_NAME                  RDB$OWNER_NAME
RDB$FIELD_NAME                  RDB$RELATION_ID
RDB$FIELD_NAME                  RDB$RELATION_NAME
RDB$FIELD_NAME                  RDB$RELATION_TYPE
RDB$FIELD_NAME                  RDB$RUNTIME
RDB$FIELD_NAME                  RDB$SECURITY_CLASS
RDB$FIELD_NAME                  RDB$SQL_SECURITY
RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
RDB$FIELD_NAME                  RDB$VIEW_BLR
RDB$FIELD_NAME                  RDB$VIEW_SOURCE
Records affected: 18
RDB$RELATION_ID                 34
RDB$RELATION_NAME               MON$ATTACHMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    26
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 37
RDB$RELATION_NAME               MON$CALL_STACK
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    10
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 40
RDB$RELATION_NAME               MON$CONTEXT_VARIABLES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 33
RDB$RELATION_NAME               MON$DATABASE
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    28
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 38
RDB$RELATION_NAME               MON$IO_STATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 41
RDB$RELATION_NAME               MON$MEMORY_USAGE
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 39
RDB$RELATION_NAME               MON$RECORD_STATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    17
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 36
RDB$RELATION_NAME               MON$STATEMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    10
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 49
RDB$RELATION_NAME               MON$TABLE_STATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 35
RDB$RELATION_NAME               MON$TRANSACTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    13
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 45
RDB$RELATION_NAME               RDB$AUTH_MAPPING
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    10
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 32
RDB$RELATION_NAME               RDB$BACKUP_HISTORY
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 28
RDB$RELATION_NAME               RDB$CHARACTER_SETS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    11
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 24
RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
Field id: 0
name: RDB$CONSTRAINT_NAME
Field id: 1
name: RDB$TRIGGER_NAME
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 29
RDB$RELATION_NAME               RDB$COLLATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    11
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 53
RDB$RELATION_NAME               RDB$CONFIG
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 1
RDB$RELATION_NAME               RDB$DATABASE
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 47
RDB$RELATION_NAME               RDB$DB_CREATORS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 13
RDB$RELATION_NAME               RDB$DEPENDENCIES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 30
RDB$RELATION_NAME               RDB$EXCEPTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    7
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 2
RDB$RELATION_NAME               RDB$FIELDS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    30
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
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
Field id: 28
name: RDB$SECURITY_CLASS
Field id: 29
name: RDB$OWNER_NAME
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 21
RDB$RELATION_NAME               RDB$FIELD_DIMENSIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 10
RDB$RELATION_NAME               RDB$FILES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 16
RDB$RELATION_NAME               RDB$FILTERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    9
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 8
RDB$RELATION_NAME               RDB$FORMATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    3
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 14
RDB$RELATION_NAME               RDB$FUNCTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    21
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 15
RDB$RELATION_NAME               RDB$FUNCTION_ARGUMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    22
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 20
RDB$RELATION_NAME               RDB$GENERATORS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 3
RDB$RELATION_NAME               RDB$INDEX_SEGMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
Field id: 0
name: RDB$INDEX_NAME
Field id: 1
name: RDB$FIELD_NAME
Field id: 2
name: RDB$FIELD_POSITION
Field id: 3
name: RDB$STATISTICS
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 4
RDB$RELATION_NAME               RDB$INDICES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    13
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
Field id: 10
name: RDB$EXPRESSION_BLR
Field id: 11
name: RDB$EXPRESSION_SOURCE
Field id: 12
name: RDB$STATISTICS
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 25
RDB$RELATION_NAME               RDB$LOG_FILES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 42
RDB$RELATION_NAME               RDB$PACKAGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    9
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 0
RDB$RELATION_NAME               RDB$PAGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 26
RDB$RELATION_NAME               RDB$PROCEDURES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    19
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 27
RDB$RELATION_NAME               RDB$PROCEDURE_PARAMETERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    15
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 51
RDB$RELATION_NAME               RDB$PUBLICATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    5
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 52
RDB$RELATION_NAME               RDB$PUBLICATION_TABLES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 23
RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    5
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 6
RDB$RELATION_NAME               RDB$RELATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    18
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 22
RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
trigger_name:
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 5
RDB$RELATION_NAME               RDB$RELATION_FIELDS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    21
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
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
Field id: 19
name: RDB$GENERATOR_NAME
Field id: 20
name: RDB$IDENTITY_TYPE
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 31
RDB$RELATION_NAME               RDB$ROLES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 9
RDB$RELATION_NAME               RDB$SECURITY_CLASSES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    3
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 50
RDB$RELATION_NAME               RDB$TIME_ZONES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 19
RDB$RELATION_NAME               RDB$TRANSACTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 12
RDB$RELATION_NAME               RDB$TRIGGERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    15
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
Field id: 9
name: RDB$FLAGS
Field id: 10
name: RDB$VALID_BLR
Field id: 11
name: RDB$DEBUG_INFO
Field id: 12
name: RDB$ENGINE_NAME
Field id: 13
name: RDB$ENTRYPOINT
Field id: 14
name: RDB$SQL_SECURITY
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 17
RDB$RELATION_NAME               RDB$TRIGGER_MESSAGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    3
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 11
RDB$RELATION_NAME               RDB$TYPES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    5
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 18
RDB$RELATION_NAME               RDB$USER_PRIVILEGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
trigger_name:
trigger_name:
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 7
RDB$RELATION_NAME               RDB$VIEW_RELATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 48
RDB$RELATION_NAME               SEC$DB_CREATORS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 46
RDB$RELATION_NAME               SEC$GLOBAL_AUTH_MAPPING
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 43
RDB$RELATION_NAME               SEC$USERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 44
RDB$RELATION_NAME               SEC$USER_ATTRIBUTES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
Records affected: 54
"""

@pytest.mark.version('>=4.0,<5.0')
@pytest.mark.skip("DISABLED: see notes")
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 5.0

expected_stdout_3 = """
RDB$FIELD_NAME                  RDB$DBKEY_LENGTH
RDB$FIELD_NAME                  RDB$DEFAULT_CLASS
RDB$FIELD_NAME                  RDB$DESCRIPTION
RDB$FIELD_NAME                  RDB$EXTERNAL_DESCRIPTION
RDB$FIELD_NAME                  RDB$EXTERNAL_FILE
RDB$FIELD_NAME                  RDB$FIELD_ID
RDB$FIELD_NAME                  RDB$FLAGS
RDB$FIELD_NAME                  RDB$FORMAT
RDB$FIELD_NAME                  RDB$OWNER_NAME
RDB$FIELD_NAME                  RDB$RELATION_ID
RDB$FIELD_NAME                  RDB$RELATION_NAME
RDB$FIELD_NAME                  RDB$RELATION_TYPE
RDB$FIELD_NAME                  RDB$RUNTIME
RDB$FIELD_NAME                  RDB$SECURITY_CLASS
RDB$FIELD_NAME                  RDB$SQL_SECURITY
RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
RDB$FIELD_NAME                  RDB$VIEW_BLR
RDB$FIELD_NAME                  RDB$VIEW_SOURCE
Records affected: 18
RDB$RELATION_ID                 34
RDB$RELATION_NAME               MON$ATTACHMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    27
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 37
RDB$RELATION_NAME               MON$CALL_STACK
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    11
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 55
RDB$RELATION_NAME               MON$COMPILED_STATEMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    7
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 40
RDB$RELATION_NAME               MON$CONTEXT_VARIABLES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 33
RDB$RELATION_NAME               MON$DATABASE
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    28
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 38
RDB$RELATION_NAME               MON$IO_STATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 41
RDB$RELATION_NAME               MON$MEMORY_USAGE
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 39
RDB$RELATION_NAME               MON$RECORD_STATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    17
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 36
RDB$RELATION_NAME               MON$STATEMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    11
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 49
RDB$RELATION_NAME               MON$TABLE_STATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 35
RDB$RELATION_NAME               MON$TRANSACTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    13
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 45
RDB$RELATION_NAME               RDB$AUTH_MAPPING
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    10
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 32
RDB$RELATION_NAME               RDB$BACKUP_HISTORY
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 28
RDB$RELATION_NAME               RDB$CHARACTER_SETS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    11
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 24
RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
Field id: 0
name: RDB$CONSTRAINT_NAME
Field id: 1
name: RDB$TRIGGER_NAME
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 29
RDB$RELATION_NAME               RDB$COLLATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    11
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 53
RDB$RELATION_NAME               RDB$CONFIG
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 1
RDB$RELATION_NAME               RDB$DATABASE
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 47
RDB$RELATION_NAME               RDB$DB_CREATORS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 13
RDB$RELATION_NAME               RDB$DEPENDENCIES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 30
RDB$RELATION_NAME               RDB$EXCEPTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    7
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 2
RDB$RELATION_NAME               RDB$FIELDS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    30
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
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
Field id: 28
name: RDB$SECURITY_CLASS
Field id: 29
name: RDB$OWNER_NAME
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 21
RDB$RELATION_NAME               RDB$FIELD_DIMENSIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 10
RDB$RELATION_NAME               RDB$FILES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 16
RDB$RELATION_NAME               RDB$FILTERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    9
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 8
RDB$RELATION_NAME               RDB$FORMATS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    3
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 14
RDB$RELATION_NAME               RDB$FUNCTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    21
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 15
RDB$RELATION_NAME               RDB$FUNCTION_ARGUMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    22
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 20
RDB$RELATION_NAME               RDB$GENERATORS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 3
RDB$RELATION_NAME               RDB$INDEX_SEGMENTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
Field id: 0
name: RDB$INDEX_NAME
Field id: 1
name: RDB$FIELD_NAME
Field id: 2
name: RDB$FIELD_POSITION
Field id: 3
name: RDB$STATISTICS
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 4
RDB$RELATION_NAME               RDB$INDICES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    13
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
Field id: 10
name: RDB$EXPRESSION_BLR
Field id: 11
name: RDB$EXPRESSION_SOURCE
Field id: 12
name: RDB$STATISTICS
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 54
RDB$RELATION_NAME               RDB$KEYWORDS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 25
RDB$RELATION_NAME               RDB$LOG_FILES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 42
RDB$RELATION_NAME               RDB$PACKAGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    9
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 0
RDB$RELATION_NAME               RDB$PAGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 26
RDB$RELATION_NAME               RDB$PROCEDURES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    19
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 27
RDB$RELATION_NAME               RDB$PROCEDURE_PARAMETERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    15
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 51
RDB$RELATION_NAME               RDB$PUBLICATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    5
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 52
RDB$RELATION_NAME               RDB$PUBLICATION_TABLES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 23
RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    5
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 6
RDB$RELATION_NAME               RDB$RELATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    18
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 22
RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
trigger_name:
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 5
RDB$RELATION_NAME               RDB$RELATION_FIELDS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    21
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
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
Field id: 19
name: RDB$GENERATOR_NAME
Field id: 20
name: RDB$IDENTITY_TYPE
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 31
RDB$RELATION_NAME               RDB$ROLES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 9
RDB$RELATION_NAME               RDB$SECURITY_CLASSES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    3
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 50
RDB$RELATION_NAME               RDB$TIME_ZONES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 19
RDB$RELATION_NAME               RDB$TRANSACTIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 12
RDB$RELATION_NAME               RDB$TRIGGERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    15
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
field_not_null
Field id: 9
name: RDB$FLAGS
Field id: 10
name: RDB$VALID_BLR
Field id: 11
name: RDB$DEBUG_INFO
Field id: 12
name: RDB$ENGINE_NAME
Field id: 13
name: RDB$ENTRYPOINT
Field id: 14
name: RDB$SQL_SECURITY
trigger_name:
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 17
RDB$RELATION_NAME               RDB$TRIGGER_MESSAGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    3
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 11
RDB$RELATION_NAME               RDB$TYPES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    5
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 18
RDB$RELATION_NAME               RDB$USER_PRIVILEGES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
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
trigger_name:
trigger_name:
trigger_name:
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 7
RDB$RELATION_NAME               RDB$VIEW_RELATIONS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    6
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               0
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 48
RDB$RELATION_NAME               SEC$DB_CREATORS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    2
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 46
RDB$RELATION_NAME               SEC$GLOBAL_AUTH_MAPPING
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 43
RDB$RELATION_NAME               SEC$USERS
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    8
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
RDB$RELATION_ID                 44
RDB$RELATION_NAME               SEC$USER_ATTRIBUTES
RDB$SYSTEM_FLAG                 1
RDB$DBKEY_LENGTH                8
RDB$FORMAT                      0
RDB$FIELD_ID                    4
RDB$FLAGS                       <null>
RDB$RELATION_TYPE               3
RDB$OWNER_NAME                  SYSDBA
RDB$EXTERNAL_FILE               <null>
BLOB_ID_VIEW_BLR
BLOB_ID_VIEW_SRC
BLOB_ID_DESCR
BLOB_ID_RUNTIME
BLOB_ID_EXT_DESCR
Records affected: 56
"""

@pytest.mark.version('>=5.0')
@pytest.mark.skip("DISABLED: see notes")
def test_3(act: Action):
    act.expected_stdout = expected_stdout_3
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
