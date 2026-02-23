#coding:utf-8

"""
ID:          new-database-13
TITLE:       New DB - RDB$GENERATORS
DESCRIPTION: Check for correct content of RDB$GENERATORS in empty database.
NOTES:
FBTEST:      functional.basic.db.13
NOTES:
[28.10.2015]
    1. Removed from comparison values of RDB$SECURITY_CLASS because its values often differ in 3.0 recent builds.
    2. Field rdb$description has been moved at the end of output (select) list.
    3. Added query to select FIELDS list of table because main check does not use asterisk
       and we have to know if DDL of table will have any changes in future.
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
    set blob all;
    set count on;
    set list on;

    -- Query for check whether fields list of table was changed:
    select rf.rdb$field_name
    from rdb$relation_fields rf
    where rf.rdb$relation_name = upper('rdb$generators')
    order by rf.rdb$field_name;

    -- Main test query:
    select
         rdb$generator_id
        ,rdb$generator_name
        ,rdb$system_flag
        ,rdb$initial_value
        ,rdb$generator_increment
        ,rdb$owner_name
        ,rdb$description as gen_descr_blob_id
    from rdb$generators g
    order by g.rdb$generator_id;
"""

act = isql_act('db', test_script, substitutions=[('GEN_DESCR_BLOB_ID.*', '')])

expected_stdout = """
    RDB$FIELD_NAME                  RDB$DESCRIPTION
    RDB$FIELD_NAME                  RDB$GENERATOR_ID
    RDB$FIELD_NAME                  RDB$GENERATOR_INCREMENT
    RDB$FIELD_NAME                  RDB$GENERATOR_NAME
    RDB$FIELD_NAME                  RDB$INITIAL_VALUE
    RDB$FIELD_NAME                  RDB$OWNER_NAME
    RDB$FIELD_NAME                  RDB$SECURITY_CLASS
    RDB$FIELD_NAME                  RDB$SYSTEM_FLAG

    Records affected: 8

    RDB$GENERATOR_ID                1
    RDB$GENERATOR_NAME              RDB$SECURITY_CLASS
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               <null>

    RDB$GENERATOR_ID                2
    RDB$GENERATOR_NAME              SQL$DEFAULT
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               <null>

    RDB$GENERATOR_ID                3
    RDB$GENERATOR_NAME              RDB$PROCEDURES
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               14:1e0
    Procedure ID

    RDB$GENERATOR_ID                4
    RDB$GENERATOR_NAME              RDB$EXCEPTIONS
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               14:1e1
    Exception ID

    RDB$GENERATOR_ID                5
    RDB$GENERATOR_NAME              RDB$CONSTRAINT_NAME
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               14:1e2
    Implicit constraint name

    RDB$GENERATOR_ID                6
    RDB$GENERATOR_NAME              RDB$FIELD_NAME
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               14:1e3
    Implicit domain name

    RDB$GENERATOR_ID                7
    RDB$GENERATOR_NAME              RDB$INDEX_NAME
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               14:1e4
    Implicit index name

    RDB$GENERATOR_ID                8
    RDB$GENERATOR_NAME              RDB$TRIGGER_NAME
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               14:1e5
    Implicit trigger name

    RDB$GENERATOR_ID                9
    RDB$GENERATOR_NAME              RDB$BACKUP_HISTORY
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               14:1e6
    Nbackup technology

    RDB$GENERATOR_ID                10
    RDB$GENERATOR_NAME              RDB$FUNCTIONS
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               14:1e7
    Function ID

    RDB$GENERATOR_ID                11
    RDB$GENERATOR_NAME              RDB$GENERATOR_NAME
    RDB$SYSTEM_FLAG                 1
    RDB$INITIAL_VALUE               0
    RDB$GENERATOR_INCREMENT         0
    RDB$OWNER_NAME                  SYSDBA
    GEN_DESCR_BLOB_ID               14:1e8
    Implicit generator name

    Records affected: 11
"""

@pytest.mark.version('>=3.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
