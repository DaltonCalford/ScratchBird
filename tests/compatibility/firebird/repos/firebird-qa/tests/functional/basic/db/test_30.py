#coding:utf-8

"""
ID:          new-database-30
TITLE:       New DB - RDB$TYPES content
DESCRIPTION: Check the correct content of RDB$TYPES in new database.
FBTEST:      functional.basic.db.30
NOTES:
[07.02.2022] pcisar
    Test fails on v4 (and likely on v5) because expected_stdout does not match real stdout.
[17.01.2023] pzotov
    DISABLED after discussion with dimitr, letters 17-sep-2022 11:23.
    Reasons:
        * There is no much sense to keep such tests because they fails extremely often during new major FB developing.
        * There is no chanse to get successful outcome for the whole test suite is some of system table became invalid,
          i.e. lot of other tests will be failed in such case.
    Single test for check DDL (type of columns, their order and total number) will be implemented for all RDB-tables.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select '' as "DDL of table rdb$types:" from rdb$database;
    show table rdb$types;
    select count(*) "Number of rows in rdb$types:" from rdb$types;
    select * from rdb$types;
"""

act = isql_act('db', test_script)

# version: 3.0

expected_stdout_1 = """
    DDL of table rdb$types:

    RDB$FIELD_NAME                  (RDB$FIELD_NAME) CHAR(31) CHARACTER SET UNICODE_FSS Nullable
    RDB$TYPE                        (RDB$GENERIC_TYPE) SMALLINT Nullable
    RDB$TYPE_NAME                   (RDB$TYPE_NAME) CHAR(31) CHARACTER SET UNICODE_FSS Nullable
    RDB$DESCRIPTION                 (RDB$DESCRIPTION) BLOB segment 80, subtype TEXT CHARACTER SET UNICODE_FSS Nullable
    RDB$SYSTEM_FLAG                 (RDB$SYSTEM_FLAG) SMALLINT Not Null

    Number of rows in rdb$types:    254

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        14
    RDB$TYPE_NAME                   TEXT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        7
    RDB$TYPE_NAME                   SHORT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        8
    RDB$TYPE_NAME                   LONG
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        9
    RDB$TYPE_NAME                   QUAD
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        10
    RDB$TYPE_NAME                   FLOAT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        27
    RDB$TYPE_NAME                   DOUBLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        35
    RDB$TYPE_NAME                   TIMESTAMP
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        37
    RDB$TYPE_NAME                   VARYING
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        261
    RDB$TYPE_NAME                   BLOB
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        40
    RDB$TYPE_NAME                   CSTRING
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        45
    RDB$TYPE_NAME                   BLOB_ID
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        12
    RDB$TYPE_NAME                   DATE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        13
    RDB$TYPE_NAME                   TIME
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        16
    RDB$TYPE_NAME                   INT64
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        23
    RDB$TYPE_NAME                   BOOLEAN
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   BINARY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   TEXT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   BLR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   ACL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   RANGES
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        5
    RDB$TYPE_NAME                   SUMMARY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        6
    RDB$TYPE_NAME                   FORMAT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        7
    RDB$TYPE_NAME                   TRANSACTION_DESCRIPTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        8
    RDB$TYPE_NAME                   EXTERNAL_FILE_DESCRIPTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        9
    RDB$TYPE_NAME                   DEBUG_INFORMATION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FUNCTION_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   VALUE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FUNCTION_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   BOOLEAN
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        0
    RDB$TYPE_NAME                   BY_VALUE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        1
    RDB$TYPE_NAME                   BY_REFERENCE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        2
    RDB$TYPE_NAME                   BY_VMS_DESCRIPTOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        3
    RDB$TYPE_NAME                   BY_ISC_DESCRIPTOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        4
    RDB$TYPE_NAME                   BY_SCALAR_ARRAY_DESCRIPTOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        5
    RDB$TYPE_NAME                   BY_REFERENCE_WITH_NULL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   PRE_STORE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   POST_STORE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   PRE_MODIFY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   POST_MODIFY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        5
    RDB$TYPE_NAME                   PRE_ERASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        6
    RDB$TYPE_NAME                   POST_ERASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        8192
    RDB$TYPE_NAME                   CONNECT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        8193
    RDB$TYPE_NAME                   DISCONNECT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        8194
    RDB$TYPE_NAME                   TRANSACTION_START
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        8195
    RDB$TYPE_NAME                   TRANSACTION_COMMIT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        8196
    RDB$TYPE_NAME                   TRANSACTION_ROLLBACK
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   RELATION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   VIEW
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   TRIGGER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   COMPUTED_FIELD
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   VALIDATION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        5
    RDB$TYPE_NAME                   PROCEDURE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        6
    RDB$TYPE_NAME                   EXPRESSION_INDEX
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        7
    RDB$TYPE_NAME                   EXCEPTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        8
    RDB$TYPE_NAME                   USER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        9
    RDB$TYPE_NAME                   FIELD
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        10
    RDB$TYPE_NAME                   INDEX
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        11
    RDB$TYPE_NAME                   CHARACTER_SET
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        12
    RDB$TYPE_NAME                   USER_GROUP
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        13
    RDB$TYPE_NAME                   ROLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        14
    RDB$TYPE_NAME                   GENERATOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        15
    RDB$TYPE_NAME                   UDF
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        16
    RDB$TYPE_NAME                   BLOB_FILTER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        17
    RDB$TYPE_NAME                   COLLATION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        18
    RDB$TYPE_NAME                   PACKAGE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        19
    RDB$TYPE_NAME                   PACKAGE BODY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRANSACTION_STATE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   LIMBO
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRANSACTION_STATE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   COMMITTED
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRANSACTION_STATE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   ROLLED_BACK
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        0
    RDB$TYPE_NAME                   USER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        1
    RDB$TYPE_NAME                   SYSTEM
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        2
    RDB$TYPE_NAME                   QLI
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        3
    RDB$TYPE_NAME                   CHECK_CONSTRAINT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        4
    RDB$TYPE_NAME                   REFERENTIAL_CONSTRAINT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        5
    RDB$TYPE_NAME                   VIEW_CHECK
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        6
    RDB$TYPE_NAME                   IDENTITY_GENERATOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   PERSISTENT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   VIEW
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   EXTERNAL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   VIRTUAL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   GLOBAL_TEMPORARY_PRESERVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        5
    RDB$TYPE_NAME                   GLOBAL_TEMPORARY_DELETE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PROCEDURE_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   LEGACY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PROCEDURE_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   SELECTABLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PROCEDURE_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   EXECUTABLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PARAMETER_MECHANISM
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NORMAL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PARAMETER_MECHANISM
    RDB$TYPE                        1
    RDB$TYPE_NAME                   TYPE OF
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STATE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   IDLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STATE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   ACTIVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STATE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   STALLED
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   ONLINE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   MULTI_USER_SHUTDOWN
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   SINGLE_USER_SHUTDOWN
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   FULL_SHUTDOWN
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$ISOLATION_MODE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   CONSISTENCY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$ISOLATION_MODE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   CONCURRENCY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$ISOLATION_MODE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   READ_COMMITTED_VERSION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$ISOLATION_MODE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   READ_COMMITTED_NO_VERSION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$BACKUP_STATE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NORMAL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$BACKUP_STATE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   STALLED
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$BACKUP_STATE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   MERGE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STAT_GROUP
    RDB$TYPE                        0
    RDB$TYPE_NAME                   DATABASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STAT_GROUP
    RDB$TYPE                        1
    RDB$TYPE_NAME                   ATTACHMENT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STAT_GROUP
    RDB$TYPE                        2
    RDB$TYPE_NAME                   TRANSACTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STAT_GROUP
    RDB$TYPE                        3
    RDB$TYPE_NAME                   STATEMENT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STAT_GROUP
    RDB$TYPE                        4
    RDB$TYPE_NAME                   CALL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$IDENTITY_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   ALWAYS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$IDENTITY_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   BY DEFAULT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PARAMETER_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   INPUT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PARAMETER_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   OUTPUT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_INACTIVE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   ACTIVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_INACTIVE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   INACTIVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$INDEX_INACTIVE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   ACTIVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$INDEX_INACTIVE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   INACTIVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$UNIQUE_FLAG
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NON_UNIQUE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$UNIQUE_FLAG
    RDB$TYPE                        1
    RDB$TYPE_NAME                   UNIQUE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$GRANT_OPTION
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NONE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$GRANT_OPTION
    RDB$TYPE                        1
    RDB$TYPE_NAME                   GRANT_OPTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$GRANT_OPTION
    RDB$TYPE                        2
    RDB$TYPE_NAME                   ADMIN_OPTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   HEADER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   PAGE_INVENTORY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   TRANSACTION_INVENTORY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   POINTER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        5
    RDB$TYPE_NAME                   DATA
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        6
    RDB$TYPE_NAME                   INDEX_ROOT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        7
    RDB$TYPE_NAME                   INDEX_BUCKET
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        8
    RDB$TYPE_NAME                   BLOB
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        9
    RDB$TYPE_NAME                   GENERATOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        10
    RDB$TYPE_NAME                   SCN_INVENTORY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PRIVATE_FLAG
    RDB$TYPE                        0
    RDB$TYPE_NAME                   PUBLIC
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PRIVATE_FLAG
    RDB$TYPE                        1
    RDB$TYPE_NAME                   PRIVATE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$LEGACY_FLAG
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NEW_STYLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$LEGACY_FLAG
    RDB$TYPE                        1
    RDB$TYPE_NAME                   LEGACY_STYLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$DETERMINISTIC_FLAG
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NON_DETERMINISTIC
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$DETERMINISTIC_FLAG
    RDB$TYPE                        1
    RDB$TYPE_NAME                   DETERMINISTIC
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MAP_TO_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   USER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MAP_TO_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   ROLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NONE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        1
    RDB$TYPE_NAME                   OCTETS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        2
    RDB$TYPE_NAME                   ASCII
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        3
    RDB$TYPE_NAME                   UNICODE_FSS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        4
    RDB$TYPE_NAME                   UTF8
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        5
    RDB$TYPE_NAME                   SJIS_0208
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        6
    RDB$TYPE_NAME                   EUCJ_0208
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        10
    RDB$TYPE_NAME                   DOS437
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        11
    RDB$TYPE_NAME                   DOS850
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        12
    RDB$TYPE_NAME                   DOS865
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        21
    RDB$TYPE_NAME                   ISO8859_1
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        22
    RDB$TYPE_NAME                   ISO8859_2
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        23
    RDB$TYPE_NAME                   ISO8859_3
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        34
    RDB$TYPE_NAME                   ISO8859_4
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        35
    RDB$TYPE_NAME                   ISO8859_5
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        36
    RDB$TYPE_NAME                   ISO8859_6
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        37
    RDB$TYPE_NAME                   ISO8859_7
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        38
    RDB$TYPE_NAME                   ISO8859_8
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        39
    RDB$TYPE_NAME                   ISO8859_9
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        40
    RDB$TYPE_NAME                   ISO8859_13
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        45
    RDB$TYPE_NAME                   DOS852
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        46
    RDB$TYPE_NAME                   DOS857
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        13
    RDB$TYPE_NAME                   DOS860
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        47
    RDB$TYPE_NAME                   DOS861
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        14
    RDB$TYPE_NAME                   DOS863
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        50
    RDB$TYPE_NAME                   CYRL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        9
    RDB$TYPE_NAME                   DOS737
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        15
    RDB$TYPE_NAME                   DOS775
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        16
    RDB$TYPE_NAME                   DOS858
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        17
    RDB$TYPE_NAME                   DOS862
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        18
    RDB$TYPE_NAME                   DOS864
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        48
    RDB$TYPE_NAME                   DOS866
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        49
    RDB$TYPE_NAME                   DOS869
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        51
    RDB$TYPE_NAME                   WIN1250
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        52
    RDB$TYPE_NAME                   WIN1251
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        53
    RDB$TYPE_NAME                   WIN1252
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        54
    RDB$TYPE_NAME                   WIN1253
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        55
    RDB$TYPE_NAME                   WIN1254
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        19
    RDB$TYPE_NAME                   NEXT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        58
    RDB$TYPE_NAME                   WIN1255
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        59
    RDB$TYPE_NAME                   WIN1256
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        60
    RDB$TYPE_NAME                   WIN1257
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        44
    RDB$TYPE_NAME                   KSC_5601
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        56
    RDB$TYPE_NAME                   BIG_5
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        57
    RDB$TYPE_NAME                   GB_2312
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        63
    RDB$TYPE_NAME                   KOI8R
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        64
    RDB$TYPE_NAME                   KOI8U
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        65
    RDB$TYPE_NAME                   WIN1258
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        66
    RDB$TYPE_NAME                   TIS620
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        67
    RDB$TYPE_NAME                   GBK
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        68
    RDB$TYPE_NAME                   CP943C
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        69
    RDB$TYPE_NAME                   GB18030
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        1
    RDB$TYPE_NAME                   BINARY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        2
    RDB$TYPE_NAME                   USASCII
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        2
    RDB$TYPE_NAME                   ASCII7
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        3
    RDB$TYPE_NAME                   UTF_FSS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        3
    RDB$TYPE_NAME                   SQL_TEXT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        4
    RDB$TYPE_NAME                   UTF-8
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        5
    RDB$TYPE_NAME                   SJIS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        6
    RDB$TYPE_NAME                   EUCJ
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        10
    RDB$TYPE_NAME                   DOS_437
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        11
    RDB$TYPE_NAME                   DOS_850
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        12
    RDB$TYPE_NAME                   DOS_865
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        21
    RDB$TYPE_NAME                   ISO88591
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        21
    RDB$TYPE_NAME                   LATIN1
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        21
    RDB$TYPE_NAME                   ANSI
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        22
    RDB$TYPE_NAME                   ISO88592
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        22
    RDB$TYPE_NAME                   LATIN2
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        22
    RDB$TYPE_NAME                   ISO-8859-2
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        23
    RDB$TYPE_NAME                   ISO88593
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        23
    RDB$TYPE_NAME                   LATIN3
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        23
    RDB$TYPE_NAME                   ISO-8859-3
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        34
    RDB$TYPE_NAME                   ISO88594
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        34
    RDB$TYPE_NAME                   LATIN4
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        34
    RDB$TYPE_NAME                   ISO-8859-4
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        35
    RDB$TYPE_NAME                   ISO88595
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        35
    RDB$TYPE_NAME                   ISO-8859-5
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        36
    RDB$TYPE_NAME                   ISO88596
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        36
    RDB$TYPE_NAME                   ISO-8859-6
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        37
    RDB$TYPE_NAME                   ISO88597
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        37
    RDB$TYPE_NAME                   ISO-8859-7
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        38
    RDB$TYPE_NAME                   ISO88598
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        38
    RDB$TYPE_NAME                   ISO-8859-8
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        39
    RDB$TYPE_NAME                   ISO88599
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        39
    RDB$TYPE_NAME                   LATIN5
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        39
    RDB$TYPE_NAME                   ISO-8859-9
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        40
    RDB$TYPE_NAME                   ISO885913
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        40
    RDB$TYPE_NAME                   LATIN7
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        40
    RDB$TYPE_NAME                   ISO-8859-13
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        45
    RDB$TYPE_NAME                   DOS_852
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        46
    RDB$TYPE_NAME                   DOS_857
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        13
    RDB$TYPE_NAME                   DOS_860
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        47
    RDB$TYPE_NAME                   DOS_861
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        14
    RDB$TYPE_NAME                   DOS_863
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        9
    RDB$TYPE_NAME                   DOS_737
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        15
    RDB$TYPE_NAME                   DOS_775
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        16
    RDB$TYPE_NAME                   DOS_858
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        17
    RDB$TYPE_NAME                   DOS_862
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        18
    RDB$TYPE_NAME                   DOS_864
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        48
    RDB$TYPE_NAME                   DOS_866
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        49
    RDB$TYPE_NAME                   DOS_869
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        51
    RDB$TYPE_NAME                   WIN_1250
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        52
    RDB$TYPE_NAME                   WIN_1251
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        53
    RDB$TYPE_NAME                   WIN_1252
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        54
    RDB$TYPE_NAME                   WIN_1253
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        55
    RDB$TYPE_NAME                   WIN_1254
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        58
    RDB$TYPE_NAME                   WIN_1255
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        59
    RDB$TYPE_NAME                   WIN_1256
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        60
    RDB$TYPE_NAME                   WIN_1257
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        65
    RDB$TYPE_NAME                   WIN_1258
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        44
    RDB$TYPE_NAME                   KSC5601
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        44
    RDB$TYPE_NAME                   DOS_949
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        44
    RDB$TYPE_NAME                   WIN_949
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        56
    RDB$TYPE_NAME                   BIG5
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        56
    RDB$TYPE_NAME                   DOS_950
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        56
    RDB$TYPE_NAME                   WIN_950
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        57
    RDB$TYPE_NAME                   GB2312
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        57
    RDB$TYPE_NAME                   DOS_936
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        57
    RDB$TYPE_NAME                   WIN_936
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
"""

@pytest.mark.version('>=3.0,<4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

#############
###  4.X  ###
#############

# version: 4.0

expected_stdout_2 = """
DDL of table rdb$types:
RDB$FIELD_NAME                  (RDB$FIELD_NAME) CHAR(63) CHARACTER SET UTF8 Nullable
RDB$TYPE                        (RDB$GENERIC_TYPE) SMALLINT Nullable
RDB$TYPE_NAME                   (RDB$TYPE_NAME) CHAR(63) CHARACTER SET UTF8 Nullable
RDB$DESCRIPTION                 (RDB$DESCRIPTION) BLOB segment 80, subtype TEXT CHARACTER SET UTF8 Nullable
RDB$SYSTEM_FLAG                 (RDB$SYSTEM_FLAG) SMALLINT Not Null
Number of rows in rdb$types:    293
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        14
RDB$TYPE_NAME                   TEXT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        7
RDB$TYPE_NAME                   SHORT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        8
RDB$TYPE_NAME                   LONG
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        9
RDB$TYPE_NAME                   QUAD
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        10
RDB$TYPE_NAME                   FLOAT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        27
RDB$TYPE_NAME                   DOUBLE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        35
RDB$TYPE_NAME                   TIMESTAMP
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        37
RDB$TYPE_NAME                   VARYING
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        261
RDB$TYPE_NAME                   BLOB
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        40
RDB$TYPE_NAME                   CSTRING
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        45
RDB$TYPE_NAME                   BLOB_ID
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        12
RDB$TYPE_NAME                   DATE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        13
RDB$TYPE_NAME                   TIME
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        16
RDB$TYPE_NAME                   INT64
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        23
RDB$TYPE_NAME                   BOOLEAN
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        24
RDB$TYPE_NAME                   DECFLOAT(16)
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        25
RDB$TYPE_NAME                   DECFLOAT(34)
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        29
RDB$TYPE_NAME                   TIMESTAMP WITH TIME ZONE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        28
RDB$TYPE_NAME                   TIME WITH TIME ZONE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_TYPE
RDB$TYPE                        26
RDB$TYPE_NAME                   INT128
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
RDB$TYPE                        0
RDB$TYPE_NAME                   BINARY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
RDB$TYPE                        1
RDB$TYPE_NAME                   TEXT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
RDB$TYPE                        2
RDB$TYPE_NAME                   BLR
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
RDB$TYPE                        3
RDB$TYPE_NAME                   ACL
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
RDB$TYPE                        4
RDB$TYPE_NAME                   RANGES
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
RDB$TYPE                        5
RDB$TYPE_NAME                   SUMMARY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
RDB$TYPE                        6
RDB$TYPE_NAME                   FORMAT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
RDB$TYPE                        7
RDB$TYPE_NAME                   TRANSACTION_DESCRIPTION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
RDB$TYPE                        8
RDB$TYPE_NAME                   EXTERNAL_FILE_DESCRIPTION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
RDB$TYPE                        9
RDB$TYPE_NAME                   DEBUG_INFORMATION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FUNCTION_TYPE
RDB$TYPE                        0
RDB$TYPE_NAME                   VALUE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$FUNCTION_TYPE
RDB$TYPE                        1
RDB$TYPE_NAME                   BOOLEAN
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$MECHANISM
RDB$TYPE                        0
RDB$TYPE_NAME                   BY_VALUE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$MECHANISM
RDB$TYPE                        1
RDB$TYPE_NAME                   BY_REFERENCE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$MECHANISM
RDB$TYPE                        2
RDB$TYPE_NAME                   BY_VMS_DESCRIPTOR
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$MECHANISM
RDB$TYPE                        3
RDB$TYPE_NAME                   BY_ISC_DESCRIPTOR
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$MECHANISM
RDB$TYPE                        4
RDB$TYPE_NAME                   BY_SCALAR_ARRAY_DESCRIPTOR
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$MECHANISM
RDB$TYPE                        5
RDB$TYPE_NAME                   BY_REFERENCE_WITH_NULL
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        1
RDB$TYPE_NAME                   PRE_STORE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        2
RDB$TYPE_NAME                   POST_STORE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        3
RDB$TYPE_NAME                   PRE_MODIFY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        4
RDB$TYPE_NAME                   POST_MODIFY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        5
RDB$TYPE_NAME                   PRE_ERASE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        6
RDB$TYPE_NAME                   POST_ERASE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        8192
RDB$TYPE_NAME                   CONNECT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        8193
RDB$TYPE_NAME                   DISCONNECT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        8194
RDB$TYPE_NAME                   TRANSACTION_START
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        8195
RDB$TYPE_NAME                   TRANSACTION_COMMIT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
RDB$TYPE                        8196
RDB$TYPE_NAME                   TRANSACTION_ROLLBACK
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        0
RDB$TYPE_NAME                   RELATION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        1
RDB$TYPE_NAME                   VIEW
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        2
RDB$TYPE_NAME                   TRIGGER
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        3
RDB$TYPE_NAME                   COMPUTED_FIELD
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        4
RDB$TYPE_NAME                   VALIDATION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        5
RDB$TYPE_NAME                   PROCEDURE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        6
RDB$TYPE_NAME                   EXPRESSION_INDEX
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        7
RDB$TYPE_NAME                   EXCEPTION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        8
RDB$TYPE_NAME                   USER
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        9
RDB$TYPE_NAME                   FIELD
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        10
RDB$TYPE_NAME                   INDEX
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        11
RDB$TYPE_NAME                   CHARACTER_SET
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        12
RDB$TYPE_NAME                   USER_GROUP
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        13
RDB$TYPE_NAME                   ROLE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        14
RDB$TYPE_NAME                   GENERATOR
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        15
RDB$TYPE_NAME                   UDF
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        16
RDB$TYPE_NAME                   BLOB_FILTER
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        17
RDB$TYPE_NAME                   COLLATION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        18
RDB$TYPE_NAME                   PACKAGE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$OBJECT_TYPE
RDB$TYPE                        19
RDB$TYPE_NAME                   PACKAGE BODY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRANSACTION_STATE
RDB$TYPE                        1
RDB$TYPE_NAME                   LIMBO
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRANSACTION_STATE
RDB$TYPE                        2
RDB$TYPE_NAME                   COMMITTED
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRANSACTION_STATE
RDB$TYPE                        3
RDB$TYPE_NAME                   ROLLED_BACK
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
RDB$TYPE                        0
RDB$TYPE_NAME                   USER
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
RDB$TYPE                        1
RDB$TYPE_NAME                   SYSTEM
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
RDB$TYPE                        2
RDB$TYPE_NAME                   QLI
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
RDB$TYPE                        3
RDB$TYPE_NAME                   CHECK_CONSTRAINT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
RDB$TYPE                        4
RDB$TYPE_NAME                   REFERENTIAL_CONSTRAINT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
RDB$TYPE                        5
RDB$TYPE_NAME                   VIEW_CHECK
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
RDB$TYPE                        6
RDB$TYPE_NAME                   IDENTITY_GENERATOR
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$RELATION_TYPE
RDB$TYPE                        0
RDB$TYPE_NAME                   PERSISTENT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$RELATION_TYPE
RDB$TYPE                        1
RDB$TYPE_NAME                   VIEW
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$RELATION_TYPE
RDB$TYPE                        2
RDB$TYPE_NAME                   EXTERNAL
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$RELATION_TYPE
RDB$TYPE                        3
RDB$TYPE_NAME                   VIRTUAL
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$RELATION_TYPE
RDB$TYPE                        4
RDB$TYPE_NAME                   GLOBAL_TEMPORARY_PRESERVE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$RELATION_TYPE
RDB$TYPE                        5
RDB$TYPE_NAME                   GLOBAL_TEMPORARY_DELETE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PROCEDURE_TYPE
RDB$TYPE                        0
RDB$TYPE_NAME                   LEGACY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PROCEDURE_TYPE
RDB$TYPE                        1
RDB$TYPE_NAME                   SELECTABLE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PROCEDURE_TYPE
RDB$TYPE                        2
RDB$TYPE_NAME                   EXECUTABLE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PARAMETER_MECHANISM
RDB$TYPE                        0
RDB$TYPE_NAME                   NORMAL
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PARAMETER_MECHANISM
RDB$TYPE                        1
RDB$TYPE_NAME                   TYPE OF
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$STATE
RDB$TYPE                        0
RDB$TYPE_NAME                   IDLE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$STATE
RDB$TYPE                        1
RDB$TYPE_NAME                   ACTIVE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$STATE
RDB$TYPE                        2
RDB$TYPE_NAME                   STALLED
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
RDB$TYPE                        0
RDB$TYPE_NAME                   ONLINE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
RDB$TYPE                        1
RDB$TYPE_NAME                   MULTI_USER_SHUTDOWN
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
RDB$TYPE                        2
RDB$TYPE_NAME                   SINGLE_USER_SHUTDOWN
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
RDB$TYPE                        3
RDB$TYPE_NAME                   FULL_SHUTDOWN
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$ISOLATION_MODE
RDB$TYPE                        0
RDB$TYPE_NAME                   CONSISTENCY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$ISOLATION_MODE
RDB$TYPE                        1
RDB$TYPE_NAME                   CONCURRENCY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$ISOLATION_MODE
RDB$TYPE                        2
RDB$TYPE_NAME                   READ_COMMITTED_VERSION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$ISOLATION_MODE
RDB$TYPE                        3
RDB$TYPE_NAME                   READ_COMMITTED_NO_VERSION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$ISOLATION_MODE
RDB$TYPE                        4
RDB$TYPE_NAME                   READ_COMMITTED_READ_CONSISTENCY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$BACKUP_STATE
RDB$TYPE                        0
RDB$TYPE_NAME                   NORMAL
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$BACKUP_STATE
RDB$TYPE                        1
RDB$TYPE_NAME                   STALLED
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$BACKUP_STATE
RDB$TYPE                        2
RDB$TYPE_NAME                   MERGE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$STAT_GROUP
RDB$TYPE                        0
RDB$TYPE_NAME                   DATABASE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$STAT_GROUP
RDB$TYPE                        1
RDB$TYPE_NAME                   ATTACHMENT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$STAT_GROUP
RDB$TYPE                        2
RDB$TYPE_NAME                   TRANSACTION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$STAT_GROUP
RDB$TYPE                        3
RDB$TYPE_NAME                   STATEMENT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$STAT_GROUP
RDB$TYPE                        4
RDB$TYPE_NAME                   CALL
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$IDENTITY_TYPE
RDB$TYPE                        0
RDB$TYPE_NAME                   ALWAYS
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$IDENTITY_TYPE
RDB$TYPE                        1
RDB$TYPE_NAME                   BY DEFAULT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PARAMETER_TYPE
RDB$TYPE                        0
RDB$TYPE_NAME                   INPUT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PARAMETER_TYPE
RDB$TYPE                        1
RDB$TYPE_NAME                   OUTPUT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_INACTIVE
RDB$TYPE                        0
RDB$TYPE_NAME                   ACTIVE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$TRIGGER_INACTIVE
RDB$TYPE                        1
RDB$TYPE_NAME                   INACTIVE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$INDEX_INACTIVE
RDB$TYPE                        0
RDB$TYPE_NAME                   ACTIVE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$INDEX_INACTIVE
RDB$TYPE                        1
RDB$TYPE_NAME                   INACTIVE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$UNIQUE_FLAG
RDB$TYPE                        0
RDB$TYPE_NAME                   NON_UNIQUE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$UNIQUE_FLAG
RDB$TYPE                        1
RDB$TYPE_NAME                   UNIQUE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$GRANT_OPTION
RDB$TYPE                        0
RDB$TYPE_NAME                   NONE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$GRANT_OPTION
RDB$TYPE                        1
RDB$TYPE_NAME                   GRANT_OPTION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$GRANT_OPTION
RDB$TYPE                        2
RDB$TYPE_NAME                   ADMIN_OPTION
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PAGE_TYPE
RDB$TYPE                        1
RDB$TYPE_NAME                   HEADER
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PAGE_TYPE
RDB$TYPE                        2
RDB$TYPE_NAME                   PAGE_INVENTORY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PAGE_TYPE
RDB$TYPE                        3
RDB$TYPE_NAME                   TRANSACTION_INVENTORY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PAGE_TYPE
RDB$TYPE                        4
RDB$TYPE_NAME                   POINTER
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PAGE_TYPE
RDB$TYPE                        5
RDB$TYPE_NAME                   DATA
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PAGE_TYPE
RDB$TYPE                        6
RDB$TYPE_NAME                   INDEX_ROOT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PAGE_TYPE
RDB$TYPE                        7
RDB$TYPE_NAME                   INDEX_BUCKET
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PAGE_TYPE
RDB$TYPE                        8
RDB$TYPE_NAME                   BLOB
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PAGE_TYPE
RDB$TYPE                        9
RDB$TYPE_NAME                   GENERATOR
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PAGE_TYPE
RDB$TYPE                        10
RDB$TYPE_NAME                   SCN_INVENTORY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PRIVATE_FLAG
RDB$TYPE                        0
RDB$TYPE_NAME                   PUBLIC
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$PRIVATE_FLAG
RDB$TYPE                        1
RDB$TYPE_NAME                   PRIVATE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$LEGACY_FLAG
RDB$TYPE                        0
RDB$TYPE_NAME                   NEW_STYLE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$LEGACY_FLAG
RDB$TYPE                        1
RDB$TYPE_NAME                   LEGACY_STYLE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$DETERMINISTIC_FLAG
RDB$TYPE                        0
RDB$TYPE_NAME                   NON_DETERMINISTIC
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$DETERMINISTIC_FLAG
RDB$TYPE                        1
RDB$TYPE_NAME                   DETERMINISTIC
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$MAP_TO_TYPE
RDB$TYPE                        0
RDB$TYPE_NAME                   USER
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$MAP_TO_TYPE
RDB$TYPE                        1
RDB$TYPE_NAME                   ROLE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        1
RDB$TYPE_NAME                   USER_MANAGEMENT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        2
RDB$TYPE_NAME                   READ_RAW_PAGES
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        3
RDB$TYPE_NAME                   CREATE_USER_TYPES
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        4
RDB$TYPE_NAME                   USE_NBACKUP_UTILITY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        5
RDB$TYPE_NAME                   CHANGE_SHUTDOWN_MODE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        6
RDB$TYPE_NAME                   TRACE_ANY_ATTACHMENT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        7
RDB$TYPE_NAME                   MONITOR_ANY_ATTACHMENT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        8
RDB$TYPE_NAME                   ACCESS_SHUTDOWN_DATABASE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        9
RDB$TYPE_NAME                   CREATE_DATABASE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        10
RDB$TYPE_NAME                   DROP_DATABASE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        11
RDB$TYPE_NAME                   USE_GBAK_UTILITY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        12
RDB$TYPE_NAME                   USE_GSTAT_UTILITY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        13
RDB$TYPE_NAME                   USE_GFIX_UTILITY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        14
RDB$TYPE_NAME                   IGNORE_DB_TRIGGERS
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        15
RDB$TYPE_NAME                   CHANGE_HEADER_SETTINGS
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        16
RDB$TYPE_NAME                   SELECT_ANY_OBJECT_IN_DATABASE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        17
RDB$TYPE_NAME                   ACCESS_ANY_OBJECT_IN_DATABASE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        18
RDB$TYPE_NAME                   MODIFY_ANY_OBJECT_IN_DATABASE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        19
RDB$TYPE_NAME                   CHANGE_MAPPING_RULES
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        20
RDB$TYPE_NAME                   USE_GRANTED_BY_CLAUSE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        21
RDB$TYPE_NAME                   GRANT_REVOKE_ON_ANY_OBJECT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        22
RDB$TYPE_NAME                   GRANT_REVOKE_ANY_DDL_RIGHT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        23
RDB$TYPE_NAME                   CREATE_PRIVILEGED_ROLES
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        24
RDB$TYPE_NAME                   GET_DBCRYPT_INFO
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        25
RDB$TYPE_NAME                   MODIFY_EXT_CONN_POOL
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
RDB$TYPE                        26
RDB$TYPE_NAME                   REPLICATE_INTO_DATABASE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$CRYPT_STATE
RDB$TYPE                        0
RDB$TYPE_NAME                   NOT ENCRYPTED
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$CRYPT_STATE
RDB$TYPE                        1
RDB$TYPE_NAME                   ENCRYPTED
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$CRYPT_STATE
RDB$TYPE                        2
RDB$TYPE_NAME                   DECRYPT IN PROGRESS
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$CRYPT_STATE
RDB$TYPE                        3
RDB$TYPE_NAME                   ENCRYPT IN PROGRESS
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$REPLICA_MODE
RDB$TYPE                        0
RDB$TYPE_NAME                   NONE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$REPLICA_MODE
RDB$TYPE                        1
RDB$TYPE_NAME                   READ-ONLY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  MON$REPLICA_MODE
RDB$TYPE                        2
RDB$TYPE_NAME                   READ-WRITE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        0
RDB$TYPE_NAME                   NONE
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        1
RDB$TYPE_NAME                   OCTETS
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        2
RDB$TYPE_NAME                   ASCII
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        3
RDB$TYPE_NAME                   UNICODE_FSS
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        4
RDB$TYPE_NAME                   UTF8
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        5
RDB$TYPE_NAME                   SJIS_0208
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        6
RDB$TYPE_NAME                   EUCJ_0208
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        10
RDB$TYPE_NAME                   DOS437
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        11
RDB$TYPE_NAME                   DOS850
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        12
RDB$TYPE_NAME                   DOS865
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        21
RDB$TYPE_NAME                   ISO8859_1
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        22
RDB$TYPE_NAME                   ISO8859_2
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        23
RDB$TYPE_NAME                   ISO8859_3
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        34
RDB$TYPE_NAME                   ISO8859_4
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        35
RDB$TYPE_NAME                   ISO8859_5
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        36
RDB$TYPE_NAME                   ISO8859_6
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        37
RDB$TYPE_NAME                   ISO8859_7
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        38
RDB$TYPE_NAME                   ISO8859_8
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        39
RDB$TYPE_NAME                   ISO8859_9
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        40
RDB$TYPE_NAME                   ISO8859_13
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        45
RDB$TYPE_NAME                   DOS852
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        46
RDB$TYPE_NAME                   DOS857
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        13
RDB$TYPE_NAME                   DOS860
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        47
RDB$TYPE_NAME                   DOS861
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        14
RDB$TYPE_NAME                   DOS863
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        50
RDB$TYPE_NAME                   CYRL
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        9
RDB$TYPE_NAME                   DOS737
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        15
RDB$TYPE_NAME                   DOS775
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        16
RDB$TYPE_NAME                   DOS858
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        17
RDB$TYPE_NAME                   DOS862
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        18
RDB$TYPE_NAME                   DOS864
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        48
RDB$TYPE_NAME                   DOS866
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        49
RDB$TYPE_NAME                   DOS869
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        51
RDB$TYPE_NAME                   WIN1250
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        52
RDB$TYPE_NAME                   WIN1251
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        53
RDB$TYPE_NAME                   WIN1252
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        54
RDB$TYPE_NAME                   WIN1253
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        55
RDB$TYPE_NAME                   WIN1254
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        19
RDB$TYPE_NAME                   NEXT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        58
RDB$TYPE_NAME                   WIN1255
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        59
RDB$TYPE_NAME                   WIN1256
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        60
RDB$TYPE_NAME                   WIN1257
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        44
RDB$TYPE_NAME                   KSC_5601
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        56
RDB$TYPE_NAME                   BIG_5
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        57
RDB$TYPE_NAME                   GB_2312
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        63
RDB$TYPE_NAME                   KOI8R
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        64
RDB$TYPE_NAME                   KOI8U
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        65
RDB$TYPE_NAME                   WIN1258
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        66
RDB$TYPE_NAME                   TIS620
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        67
RDB$TYPE_NAME                   GBK
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        68
RDB$TYPE_NAME                   CP943C
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        69
RDB$TYPE_NAME                   GB18030
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        1
RDB$TYPE_NAME                   BINARY
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        2
RDB$TYPE_NAME                   USASCII
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        2
RDB$TYPE_NAME                   ASCII7
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        3
RDB$TYPE_NAME                   UTF_FSS
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        3
RDB$TYPE_NAME                   SQL_TEXT
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        4
RDB$TYPE_NAME                   UTF-8
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        5
RDB$TYPE_NAME                   SJIS
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        6
RDB$TYPE_NAME                   EUCJ
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        10
RDB$TYPE_NAME                   DOS_437
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        11
RDB$TYPE_NAME                   DOS_850
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        12
RDB$TYPE_NAME                   DOS_865
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        21
RDB$TYPE_NAME                   ISO88591
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        21
RDB$TYPE_NAME                   LATIN1
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        21
RDB$TYPE_NAME                   ANSI
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        22
RDB$TYPE_NAME                   ISO88592
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        22
RDB$TYPE_NAME                   LATIN2
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        22
RDB$TYPE_NAME                   ISO-8859-2
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        23
RDB$TYPE_NAME                   ISO88593
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        23
RDB$TYPE_NAME                   LATIN3
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        23
RDB$TYPE_NAME                   ISO-8859-3
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        34
RDB$TYPE_NAME                   ISO88594
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        34
RDB$TYPE_NAME                   LATIN4
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        34
RDB$TYPE_NAME                   ISO-8859-4
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        35
RDB$TYPE_NAME                   ISO88595
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        35
RDB$TYPE_NAME                   ISO-8859-5
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        36
RDB$TYPE_NAME                   ISO88596
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        36
RDB$TYPE_NAME                   ISO-8859-6
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        37
RDB$TYPE_NAME                   ISO88597
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        37
RDB$TYPE_NAME                   ISO-8859-7
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        38
RDB$TYPE_NAME                   ISO88598
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        38
RDB$TYPE_NAME                   ISO-8859-8
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        39
RDB$TYPE_NAME                   ISO88599
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        39
RDB$TYPE_NAME                   LATIN5
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        39
RDB$TYPE_NAME                   ISO-8859-9
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        40
RDB$TYPE_NAME                   ISO885913
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        40
RDB$TYPE_NAME                   LATIN7
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        40
RDB$TYPE_NAME                   ISO-8859-13
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        45
RDB$TYPE_NAME                   DOS_852
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        46
RDB$TYPE_NAME                   DOS_857
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        13
RDB$TYPE_NAME                   DOS_860
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        47
RDB$TYPE_NAME                   DOS_861
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        14
RDB$TYPE_NAME                   DOS_863
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        9
RDB$TYPE_NAME                   DOS_737
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        15
RDB$TYPE_NAME                   DOS_775
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        16
RDB$TYPE_NAME                   DOS_858
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        17
RDB$TYPE_NAME                   DOS_862
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        18
RDB$TYPE_NAME                   DOS_864
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        48
RDB$TYPE_NAME                   DOS_866
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        49
RDB$TYPE_NAME                   DOS_869
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        51
RDB$TYPE_NAME                   WIN_1250
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        52
RDB$TYPE_NAME                   WIN_1251
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        53
RDB$TYPE_NAME                   WIN_1252
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        54
RDB$TYPE_NAME                   WIN_1253
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        55
RDB$TYPE_NAME                   WIN_1254
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        58
RDB$TYPE_NAME                   WIN_1255
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        59
RDB$TYPE_NAME                   WIN_1256
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        60
RDB$TYPE_NAME                   WIN_1257
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        65
RDB$TYPE_NAME                   WIN_1258
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        44
RDB$TYPE_NAME                   KSC5601
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        44
RDB$TYPE_NAME                   DOS_949
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        44
RDB$TYPE_NAME                   WIN_949
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        56
RDB$TYPE_NAME                   BIG5
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        56
RDB$TYPE_NAME                   DOS_950
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        56
RDB$TYPE_NAME                   WIN_950
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        57
RDB$TYPE_NAME                   GB2312
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        57
RDB$TYPE_NAME                   DOS_936
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
RDB$TYPE                        57
RDB$TYPE_NAME                   WIN_936
RDB$DESCRIPTION                 <null>
RDB$SYSTEM_FLAG                 1
"""

#@pytest.mark.version('>=4.0,<5.0')
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



    RDB$FIELD_NAME                  MON$BACKUP_STATE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   MERGE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$BACKUP_STATE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NORMAL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$BACKUP_STATE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   STALLED
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$CRYPT_STATE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   DECRYPT IN PROGRESS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$CRYPT_STATE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   ENCRYPT IN PROGRESS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$CRYPT_STATE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   ENCRYPTED
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$CRYPT_STATE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NOT ENCRYPTED
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$ISOLATION_MODE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   CONCURRENCY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$ISOLATION_MODE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   CONSISTENCY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$ISOLATION_MODE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   READ_COMMITTED_NO_VERSION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$ISOLATION_MODE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   READ_COMMITTED_READ_CONSISTENCY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$ISOLATION_MODE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   READ_COMMITTED_VERSION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$REPLICA_MODE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NONE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$REPLICA_MODE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   READ-ONLY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$REPLICA_MODE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   READ-WRITE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   FULL_SHUTDOWN
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   MULTI_USER_SHUTDOWN
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   ONLINE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$SHUTDOWN_MODE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   SINGLE_USER_SHUTDOWN
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STATE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   ACTIVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STATE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   IDLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STATE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   STALLED
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STAT_GROUP
    RDB$TYPE                        1
    RDB$TYPE_NAME                   ATTACHMENT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STAT_GROUP
    RDB$TYPE                        4
    RDB$TYPE_NAME                   CALL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STAT_GROUP
    RDB$TYPE                        0
    RDB$TYPE_NAME                   DATABASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STAT_GROUP
    RDB$TYPE                        3
    RDB$TYPE_NAME                   STATEMENT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  MON$STAT_GROUP
    RDB$TYPE                        2
    RDB$TYPE_NAME                   TRANSACTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        21
    RDB$TYPE_NAME                   ANSI
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        2
    RDB$TYPE_NAME                   ASCII
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        2
    RDB$TYPE_NAME                   ASCII7
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        56
    RDB$TYPE_NAME                   BIG5
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        56
    RDB$TYPE_NAME                   BIG_5
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        1
    RDB$TYPE_NAME                   BINARY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        68
    RDB$TYPE_NAME                   CP943C
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        50
    RDB$TYPE_NAME                   CYRL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        10
    RDB$TYPE_NAME                   DOS437
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        9
    RDB$TYPE_NAME                   DOS737
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        15
    RDB$TYPE_NAME                   DOS775
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        11
    RDB$TYPE_NAME                   DOS850
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        45
    RDB$TYPE_NAME                   DOS852
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        46
    RDB$TYPE_NAME                   DOS857
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        16
    RDB$TYPE_NAME                   DOS858
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        13
    RDB$TYPE_NAME                   DOS860
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        47
    RDB$TYPE_NAME                   DOS861
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        17
    RDB$TYPE_NAME                   DOS862
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        14
    RDB$TYPE_NAME                   DOS863
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        18
    RDB$TYPE_NAME                   DOS864
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        12
    RDB$TYPE_NAME                   DOS865
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        48
    RDB$TYPE_NAME                   DOS866
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        49
    RDB$TYPE_NAME                   DOS869
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        10
    RDB$TYPE_NAME                   DOS_437
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        9
    RDB$TYPE_NAME                   DOS_737
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        15
    RDB$TYPE_NAME                   DOS_775
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        11
    RDB$TYPE_NAME                   DOS_850
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        45
    RDB$TYPE_NAME                   DOS_852
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        46
    RDB$TYPE_NAME                   DOS_857
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        16
    RDB$TYPE_NAME                   DOS_858
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        13
    RDB$TYPE_NAME                   DOS_860
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        47
    RDB$TYPE_NAME                   DOS_861
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        17
    RDB$TYPE_NAME                   DOS_862
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        14
    RDB$TYPE_NAME                   DOS_863
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        18
    RDB$TYPE_NAME                   DOS_864
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        12
    RDB$TYPE_NAME                   DOS_865
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        48
    RDB$TYPE_NAME                   DOS_866
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        49
    RDB$TYPE_NAME                   DOS_869
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        57
    RDB$TYPE_NAME                   DOS_936
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        44
    RDB$TYPE_NAME                   DOS_949
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        56
    RDB$TYPE_NAME                   DOS_950
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        6
    RDB$TYPE_NAME                   EUCJ
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        6
    RDB$TYPE_NAME                   EUCJ_0208
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        69
    RDB$TYPE_NAME                   GB18030
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        57
    RDB$TYPE_NAME                   GB2312
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        67
    RDB$TYPE_NAME                   GBK
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        57
    RDB$TYPE_NAME                   GB_2312
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        40
    RDB$TYPE_NAME                   ISO-8859-13
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        22
    RDB$TYPE_NAME                   ISO-8859-2
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        23
    RDB$TYPE_NAME                   ISO-8859-3
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        34
    RDB$TYPE_NAME                   ISO-8859-4
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        35
    RDB$TYPE_NAME                   ISO-8859-5
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        36
    RDB$TYPE_NAME                   ISO-8859-6
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        37
    RDB$TYPE_NAME                   ISO-8859-7
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        38
    RDB$TYPE_NAME                   ISO-8859-8
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        39
    RDB$TYPE_NAME                   ISO-8859-9
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        21
    RDB$TYPE_NAME                   ISO88591
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        40
    RDB$TYPE_NAME                   ISO885913
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        22
    RDB$TYPE_NAME                   ISO88592
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        23
    RDB$TYPE_NAME                   ISO88593
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        34
    RDB$TYPE_NAME                   ISO88594
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        35
    RDB$TYPE_NAME                   ISO88595
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        36
    RDB$TYPE_NAME                   ISO88596
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        37
    RDB$TYPE_NAME                   ISO88597
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        38
    RDB$TYPE_NAME                   ISO88598
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        39
    RDB$TYPE_NAME                   ISO88599
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        21
    RDB$TYPE_NAME                   ISO8859_1
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        40
    RDB$TYPE_NAME                   ISO8859_13
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        22
    RDB$TYPE_NAME                   ISO8859_2
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        23
    RDB$TYPE_NAME                   ISO8859_3
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        34
    RDB$TYPE_NAME                   ISO8859_4
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        35
    RDB$TYPE_NAME                   ISO8859_5
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        36
    RDB$TYPE_NAME                   ISO8859_6
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        37
    RDB$TYPE_NAME                   ISO8859_7
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        38
    RDB$TYPE_NAME                   ISO8859_8
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        39
    RDB$TYPE_NAME                   ISO8859_9
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        63
    RDB$TYPE_NAME                   KOI8R
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        64
    RDB$TYPE_NAME                   KOI8U
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        44
    RDB$TYPE_NAME                   KSC5601
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        44
    RDB$TYPE_NAME                   KSC_5601
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        21
    RDB$TYPE_NAME                   LATIN1
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        22
    RDB$TYPE_NAME                   LATIN2
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        23
    RDB$TYPE_NAME                   LATIN3
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        34
    RDB$TYPE_NAME                   LATIN4
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        39
    RDB$TYPE_NAME                   LATIN5
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        40
    RDB$TYPE_NAME                   LATIN7
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        19
    RDB$TYPE_NAME                   NEXT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NONE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        1
    RDB$TYPE_NAME                   OCTETS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        5
    RDB$TYPE_NAME                   SJIS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        5
    RDB$TYPE_NAME                   SJIS_0208
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        3
    RDB$TYPE_NAME                   SQL_TEXT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        66
    RDB$TYPE_NAME                   TIS620
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        3
    RDB$TYPE_NAME                   UNICODE_FSS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        2
    RDB$TYPE_NAME                   USASCII
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        4
    RDB$TYPE_NAME                   UTF-8
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        4
    RDB$TYPE_NAME                   UTF8
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        3
    RDB$TYPE_NAME                   UTF_FSS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        51
    RDB$TYPE_NAME                   WIN1250
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        52
    RDB$TYPE_NAME                   WIN1251
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        53
    RDB$TYPE_NAME                   WIN1252
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        54
    RDB$TYPE_NAME                   WIN1253
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        55
    RDB$TYPE_NAME                   WIN1254
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        58
    RDB$TYPE_NAME                   WIN1255
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        59
    RDB$TYPE_NAME                   WIN1256
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        60
    RDB$TYPE_NAME                   WIN1257
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        65
    RDB$TYPE_NAME                   WIN1258
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        51
    RDB$TYPE_NAME                   WIN_1250
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        52
    RDB$TYPE_NAME                   WIN_1251
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        53
    RDB$TYPE_NAME                   WIN_1252
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        54
    RDB$TYPE_NAME                   WIN_1253
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        55
    RDB$TYPE_NAME                   WIN_1254
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        58
    RDB$TYPE_NAME                   WIN_1255
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        59
    RDB$TYPE_NAME                   WIN_1256
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        60
    RDB$TYPE_NAME                   WIN_1257
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        65
    RDB$TYPE_NAME                   WIN_1258
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        57
    RDB$TYPE_NAME                   WIN_936
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        44
    RDB$TYPE_NAME                   WIN_949
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$TYPE                        56
    RDB$TYPE_NAME                   WIN_950
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$DETERMINISTIC_FLAG
    RDB$TYPE                        1
    RDB$TYPE_NAME                   DETERMINISTIC
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$DETERMINISTIC_FLAG
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NON_DETERMINISTIC
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   ACL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   BINARY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   BLR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        9
    RDB$TYPE_NAME                   DEBUG_INFORMATION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        8
    RDB$TYPE_NAME                   EXTERNAL_FILE_DESCRIPTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        6
    RDB$TYPE_NAME                   FORMAT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   RANGES
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        5
    RDB$TYPE_NAME                   SUMMARY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   TEXT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_SUB_TYPE
    RDB$TYPE                        7
    RDB$TYPE_NAME                   TRANSACTION_DESCRIPTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        261
    RDB$TYPE_NAME                   BLOB
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        45
    RDB$TYPE_NAME                   BLOB_ID
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        23
    RDB$TYPE_NAME                   BOOLEAN
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        40
    RDB$TYPE_NAME                   CSTRING
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        12
    RDB$TYPE_NAME                   DATE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        24
    RDB$TYPE_NAME                   DECFLOAT(16)
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        25
    RDB$TYPE_NAME                   DECFLOAT(34)
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        27
    RDB$TYPE_NAME                   DOUBLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        10
    RDB$TYPE_NAME                   FLOAT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        26
    RDB$TYPE_NAME                   INT128
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        16
    RDB$TYPE_NAME                   INT64
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        8
    RDB$TYPE_NAME                   LONG
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        9
    RDB$TYPE_NAME                   QUAD
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        7
    RDB$TYPE_NAME                   SHORT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        14
    RDB$TYPE_NAME                   TEXT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        13
    RDB$TYPE_NAME                   TIME
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        28
    RDB$TYPE_NAME                   TIME WITH TIME ZONE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        35
    RDB$TYPE_NAME                   TIMESTAMP
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        29
    RDB$TYPE_NAME                   TIMESTAMP WITH TIME ZONE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        37
    RDB$TYPE_NAME                   VARYING
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FUNCTION_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   BOOLEAN
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$FUNCTION_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   VALUE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$GRANT_OPTION
    RDB$TYPE                        2
    RDB$TYPE_NAME                   ADMIN_OPTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$GRANT_OPTION
    RDB$TYPE                        1
    RDB$TYPE_NAME                   GRANT_OPTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$GRANT_OPTION
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NONE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$IDENTITY_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   ALWAYS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$IDENTITY_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   BY DEFAULT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$INDEX_INACTIVE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   ACTIVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$INDEX_INACTIVE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   INACTIVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$LEGACY_FLAG
    RDB$TYPE                        1
    RDB$TYPE_NAME                   LEGACY_STYLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$LEGACY_FLAG
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NEW_STYLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MAP_TO_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   ROLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MAP_TO_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   USER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        3
    RDB$TYPE_NAME                   BY_ISC_DESCRIPTOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        1
    RDB$TYPE_NAME                   BY_REFERENCE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        5
    RDB$TYPE_NAME                   BY_REFERENCE_WITH_NULL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        4
    RDB$TYPE_NAME                   BY_SCALAR_ARRAY_DESCRIPTOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        0
    RDB$TYPE_NAME                   BY_VALUE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$MECHANISM
    RDB$TYPE                        2
    RDB$TYPE_NAME                   BY_VMS_DESCRIPTOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        16
    RDB$TYPE_NAME                   BLOB_FILTER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        11
    RDB$TYPE_NAME                   CHARACTER_SET
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        17
    RDB$TYPE_NAME                   COLLATION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   COMPUTED_FIELD
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        7
    RDB$TYPE_NAME                   EXCEPTION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        6
    RDB$TYPE_NAME                   EXPRESSION_INDEX
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        9
    RDB$TYPE_NAME                   FIELD
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        14
    RDB$TYPE_NAME                   GENERATOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        10
    RDB$TYPE_NAME                   INDEX
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        18
    RDB$TYPE_NAME                   PACKAGE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        19
    RDB$TYPE_NAME                   PACKAGE BODY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        5
    RDB$TYPE_NAME                   PROCEDURE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   RELATION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        13
    RDB$TYPE_NAME                   ROLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   TRIGGER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        15
    RDB$TYPE_NAME                   UDF
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        8
    RDB$TYPE_NAME                   USER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        12
    RDB$TYPE_NAME                   USER_GROUP
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   VALIDATION
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$OBJECT_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   VIEW
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        8
    RDB$TYPE_NAME                   BLOB
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        5
    RDB$TYPE_NAME                   DATA
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        9
    RDB$TYPE_NAME                   GENERATOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   HEADER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        7
    RDB$TYPE_NAME                   INDEX_BUCKET
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        6
    RDB$TYPE_NAME                   INDEX_ROOT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   PAGE_INVENTORY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   POINTER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        10
    RDB$TYPE_NAME                   SCN_INVENTORY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PAGE_TYPE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   TRANSACTION_INVENTORY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PARAMETER_MECHANISM
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NORMAL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PARAMETER_MECHANISM
    RDB$TYPE                        1
    RDB$TYPE_NAME                   TYPE OF
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PARAMETER_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   INPUT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PARAMETER_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   OUTPUT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PRIVATE_FLAG
    RDB$TYPE                        1
    RDB$TYPE_NAME                   PRIVATE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PRIVATE_FLAG
    RDB$TYPE                        0
    RDB$TYPE_NAME                   PUBLIC
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PROCEDURE_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   EXECUTABLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PROCEDURE_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   LEGACY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$PROCEDURE_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   SELECTABLE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   EXTERNAL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        5
    RDB$TYPE_NAME                   GLOBAL_TEMPORARY_DELETE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   GLOBAL_TEMPORARY_PRESERVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   PERSISTENT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   VIEW
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$RELATION_TYPE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   VIRTUAL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        3
    RDB$TYPE_NAME                   CHECK_CONSTRAINT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        6
    RDB$TYPE_NAME                   IDENTITY_GENERATOR
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        2
    RDB$TYPE_NAME                   QLI
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        4
    RDB$TYPE_NAME                   REFERENTIAL_CONSTRAINT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        1
    RDB$TYPE_NAME                   SYSTEM
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        0
    RDB$TYPE_NAME                   USER
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG
    RDB$TYPE                        5
    RDB$TYPE_NAME                   VIEW_CHECK
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        17
    RDB$TYPE_NAME                   ACCESS_ANY_OBJECT_IN_DATABASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        8
    RDB$TYPE_NAME                   ACCESS_SHUTDOWN_DATABASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        15
    RDB$TYPE_NAME                   CHANGE_HEADER_SETTINGS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        19
    RDB$TYPE_NAME                   CHANGE_MAPPING_RULES
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        5
    RDB$TYPE_NAME                   CHANGE_SHUTDOWN_MODE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        9
    RDB$TYPE_NAME                   CREATE_DATABASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        23
    RDB$TYPE_NAME                   CREATE_PRIVILEGED_ROLES
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        3
    RDB$TYPE_NAME                   CREATE_USER_TYPES
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        10
    RDB$TYPE_NAME                   DROP_DATABASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        24
    RDB$TYPE_NAME                   GET_DBCRYPT_INFO
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        22
    RDB$TYPE_NAME                   GRANT_REVOKE_ANY_DDL_RIGHT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        21
    RDB$TYPE_NAME                   GRANT_REVOKE_ON_ANY_OBJECT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        14
    RDB$TYPE_NAME                   IGNORE_DB_TRIGGERS
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        18
    RDB$TYPE_NAME                   MODIFY_ANY_OBJECT_IN_DATABASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        25
    RDB$TYPE_NAME                   MODIFY_EXT_CONN_POOL
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        7
    RDB$TYPE_NAME                   MONITOR_ANY_ATTACHMENT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        2
    RDB$TYPE_NAME                   READ_RAW_PAGES
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        26
    RDB$TYPE_NAME                   REPLICATE_INTO_DATABASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        16
    RDB$TYPE_NAME                   SELECT_ANY_OBJECT_IN_DATABASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        6
    RDB$TYPE_NAME                   TRACE_ANY_ATTACHMENT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        1
    RDB$TYPE_NAME                   USER_MANAGEMENT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        11
    RDB$TYPE_NAME                   USE_GBAK_UTILITY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        13
    RDB$TYPE_NAME                   USE_GFIX_UTILITY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        20
    RDB$TYPE_NAME                   USE_GRANTED_BY_CLAUSE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        12
    RDB$TYPE_NAME                   USE_GSTAT_UTILITY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$SYSTEM_PRIVILEGES
    RDB$TYPE                        4
    RDB$TYPE_NAME                   USE_NBACKUP_UTILITY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRANSACTION_STATE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   COMMITTED
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRANSACTION_STATE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   LIMBO
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRANSACTION_STATE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   ROLLED_BACK
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_INACTIVE
    RDB$TYPE                        0
    RDB$TYPE_NAME                   ACTIVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_INACTIVE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   INACTIVE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        8192
    RDB$TYPE_NAME                   CONNECT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        8193
    RDB$TYPE_NAME                   DISCONNECT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        6
    RDB$TYPE_NAME                   POST_ERASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        4
    RDB$TYPE_NAME                   POST_MODIFY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        2
    RDB$TYPE_NAME                   POST_STORE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        5
    RDB$TYPE_NAME                   PRE_ERASE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        3
    RDB$TYPE_NAME                   PRE_MODIFY
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   PRE_STORE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        8195
    RDB$TYPE_NAME                   TRANSACTION_COMMIT
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        8196
    RDB$TYPE_NAME                   TRANSACTION_ROLLBACK
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$TRIGGER_TYPE
    RDB$TYPE                        8194
    RDB$TYPE_NAME                   TRANSACTION_START
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$UNIQUE_FLAG
    RDB$TYPE                        0
    RDB$TYPE_NAME                   NON_UNIQUE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1

    RDB$FIELD_NAME                  RDB$UNIQUE_FLAG
    RDB$TYPE                        1
    RDB$TYPE_NAME                   UNIQUE
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1


    Records affected: 293

    Are ordered columns unique ?    1


    Records affected: 1
"""

@pytest.mark.version('>=5.0')
@pytest.mark.skip("DISABLED: see notes")
def test_3(act: Action):
    act.expected_stdout = expected_stdout_3
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
